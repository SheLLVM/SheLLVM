#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/GlobalOpt.h"

using namespace std;
using namespace llvm;

namespace {
static Function *getUsingFunction(Value &V) {
  Function *F = nullptr;

  SmallVector<User *, 4> Worklist;
  SmallSet<User *, 4> Visited;

  for (auto *U : V.users())
    Worklist.push_back(U);
  while (!Worklist.empty()) {
    auto *U = Worklist.pop_back_val();

    if (Visited.count(U))
      continue;
    else
      Visited.insert(U);

    if (isa<ConstantExpr>(U) || isa<ConstantAggregate>(U) ||
        isa<GlobalVariable>(U)) {
      if (isa<GlobalVariable>(U) &&
          !cast<GlobalVariable>(U)->isDiscardableIfUnused())
        return nullptr;
      for (auto *UU : U->users()) {
        Worklist.push_back(UU);
      }
      continue;
    }

    auto *I = dyn_cast<Instruction>(U);
    if (!I)
      return nullptr;
    if (!F)
      F = I->getParent()->getParent();
    if (I->getParent()->getParent() != F)
      return nullptr;
  }

  return F;
}

// Copied from GlobalOpt.cpp
static void makeAllConstantUsesInstructions(Constant *C) {
  SmallVector<ConstantExpr *, 4> Users;
  for (auto *U : C->users()) {
    if (auto *CE = dyn_cast<ConstantExpr>(U))
      Users.push_back(CE);
    else
      // We should never get here; allNonInstructionUsersCanBeMadeInstructions
      // should not have returned true for C.
      assert(
          isa<Instruction>(U) &&
          "Can't transform non-constantexpr non-instruction to instruction!");
  }

  SmallVector<Instruction *, 4> CEUsers;
  for (auto *U : Users) {
    // DFS DAG traversal of U to eliminate ConstantExprs recursively
    ConstantExpr *CE = nullptr;

    do {
      CE = U; // Start by trying to destroy the root

      CEUsers.clear();
      auto it = CE->user_begin();
      while (it != CE->user_end()) {
        if (isa<ConstantExpr>(*it)) {
          // Recursive ConstantExpr found; switch to it
          CEUsers.clear();
          CE = cast<ConstantExpr>(*it);
          it = CE->user_begin();
        } else {
          // Function; add to UUsers
          CEUsers.push_back(cast<Instruction>(*it));
          it++;
        }
      }

      // All users of CE are instructions; replace CE with an instruction for
      // each
      for (auto *CEU : CEUsers) {
        Instruction *NewU = CE->getAsInstruction();
        NewU->insertBefore(CEU);
        CEU->replaceUsesOfWith(CE, NewU);
      }

      // We've replaced all the uses, so destroy the constant. (destroyConstant
      // will update value handles and metadata.)
      CE->destroyConstant();
    } while (CE != U); // Continue until U is destroyed
  }
}

struct GlobalToStack : public ModulePass {
  static char ID;
  GlobalToStack() : ModulePass(ID) {}

  bool shouldInline(GlobalVariable &G) {
    if (!G.isDiscardableIfUnused())
      return false; // Goal is to discard these; ignore if that's not possible
    if (!getUsingFunction(G))
      return false; // This isn't safe. We can only be on one function's stack.

    return true;
  }

  void disaggregateVars(Instruction *After, Value *Ptr,
                        SmallVectorImpl<Value *> &Idx, ConstantAggregate &C,
                        SmallSetVector<GlobalVariable *, 4> &Vars) {
    SmallSetVector<Value *, 4> ToUndefine;

    Constant *C2;
    for (unsigned i = 0; (C2 = C.getAggregateElement(i)); i++) {
      Idx.push_back(ConstantInt::get(
          Type::getInt32Ty(After->getParent()->getContext()), i));

      if (isa<ConstantAggregate>(C2)) {
        disaggregateVars(After, Ptr, Idx, cast<ConstantAggregate>(*C2), Vars);

      } else if (isa<ConstantExpr>(C2) ||
                 (isa<GlobalVariable>(C2) &&
                  Vars.count(cast<GlobalVariable>(C2)))) {
        GetElementPtrInst *GEP = GetElementPtrInst::CreateInBounds(Ptr, Idx);
        GEP->insertAfter(After);

        ToUndefine.insert(C2);

        (new StoreInst(C2, GEP))->insertAfter(GEP);
      }

      Idx.pop_back();
    }

    for (auto *V : ToUndefine)
      C.handleOperandChange(V, UndefValue::get(V->getType()));
  }

  void extractValuesFromStore(StoreInst *inst,
                              SmallSetVector<GlobalVariable *, 4> &Vars) {
    Value *V = inst->getValueOperand();
    if (!isa<ConstantAggregate>(V))
      return;

    SmallVector<Value *, 4> Idx;
    Idx.push_back(
        ConstantInt::get(Type::getInt32Ty(inst->getParent()->getContext()), 0));

    disaggregateVars(inst, inst->getPointerOperand(), Idx,
                     cast<ConstantAggregate>(*V), Vars);
  }

  void inlineGlobals(Function *F, SmallSetVector<GlobalVariable *, 4> &Vars) {
    BasicBlock &BB = F->getEntryBlock();
    Instruction *insertionPoint = &*BB.getFirstInsertionPt();

    // Step one: Bring all vars into F
    SmallMapVector<GlobalVariable *, Instruction *, 4> Replacements;
    StoreInst *firstStore = nullptr;
    for (auto *G : Vars) {
      Instruction *inst =
          new AllocaInst(G->getValueType(),
#if LLVM_VERSION_MAJOR >= 5
                         G->getType()->getAddressSpace(),
#endif
                         nullptr, MaybeAlign{G->getAlignment()}, "",
                         firstStore ? firstStore : insertionPoint);

      inst->takeName(G);

      Replacements[G] = inst;

      if (G->hasInitializer()) {
        Constant *initializer = G->getInitializer();
        StoreInst *store = new StoreInst(initializer, inst, insertionPoint);
        G->setInitializer(nullptr);

        extractValuesFromStore(store, Vars);

        if (!firstStore)
          firstStore = store;
      }
    }

    // Step two: Replace all uses
    for (auto &KV : Replacements) {
      // Some users of G might be ConstantExprs. These can't refer
      // to Instructions, so we need to turn them into explicit Instructions.
      makeAllConstantUsesInstructions(KV.first);

      KV.first->replaceAllUsesWith(KV.second);
      KV.first->eraseFromParent();
    }
  }

  bool runOnModule(Module &M) override {
    bool inlined = false;
    SmallMapVector<Function *, SmallSetVector<GlobalVariable *, 4>, 4> Usage;

    for (GlobalVariable &G : M.globals())
      if (shouldInline(G))
        Usage[getUsingFunction(G)].insert(&G);

    for (auto &KV : Usage)
      inlineGlobals(KV.first, KV.second);

    return !Usage.empty();
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
  }
}; // end of struct GlobalToStack
} // end of anonymous namespace

char GlobalToStack::ID = 0;
static RegisterPass<GlobalToStack> X("shellvm-global2stack",
                                     "Global-to-Stack Pass",
                                     false /* Only looks at CFG */,
                                     false /* Analysis Pass */);
