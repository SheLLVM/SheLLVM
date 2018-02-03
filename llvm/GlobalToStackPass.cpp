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
  SmallVector<User*,4> Worklist;
  for (auto *U : V.users())
    Worklist.push_back(U);
  while (!Worklist.empty()) {
    auto *U = Worklist.pop_back_val();
    if (isa<ConstantExpr>(U)) {
      for (auto *UU : U->users())
        Worklist.push_back(UU);
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
  SmallVector<ConstantExpr*,4> Users;
  for (auto *U : C->users()) {
    if (isa<ConstantExpr>(U))
      Users.push_back(cast<ConstantExpr>(U));
    else
      // We should never get here; allNonInstructionUsersCanBeMadeInstructions
      // should not have returned true for C.
      assert(
          isa<Instruction>(U) &&
          "Can't transform non-constantexpr non-instruction to instruction!");
  }

  SmallVector<Value*,4> UUsers;
  for (auto *U : Users) {
    UUsers.clear();
    for (auto *UU : U->users())
      UUsers.push_back(UU);
    for (auto *UU : UUsers) {
      Instruction *UI = cast<Instruction>(UU);
      Instruction *NewU = U->getAsInstruction();
      NewU->insertBefore(UI);
      UI->replaceUsesOfWith(U, NewU);
    }
    // We've replaced all the uses, so destroy the constant. (destroyConstant
    // will update value handles and metadata.)
    U->destroyConstant();
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

  bool inlineGlobal(GlobalVariable &G) {
    Function *F = getUsingFunction(G);
    assert(F != nullptr);

    BasicBlock &BB = F->getEntryBlock();
    Instruction *insertionPoint = &*BB.getFirstInsertionPt();
    Instruction *inst = new AllocaInst(
      G.getValueType(),
      nullptr,
      G.getAlignment(),
      "",
      insertionPoint);
    inst->takeName(&G);

    // Some users of G might be ConstantExprs. These can't refer
    // to Instructions, so we need to turn them into explicit Instructions.
    makeAllConstantUsesInstructions(&G);

    G.replaceAllUsesWith(inst);
    if (G.hasInitializer())
      new StoreInst(G.getInitializer(), inst, insertionPoint);

    return true;
  }

  bool runOnModule(Module &M) override {
    bool inlined = false;

    for (GlobalVariable &G : M.globals())
      if (shouldInline(G))
        inlined |= inlineGlobal(G);

    return inlined;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
  }
}; // end of struct GlobalToStack
}  // end of anonymous namespace

char GlobalToStack::ID = 0;
static RegisterPass<GlobalToStack> X("global2stack", "Global-to-Stack Pass",
                                     false /* Only looks at CFG */,
                                     false /* Analysis Pass */);
