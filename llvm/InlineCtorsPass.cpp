#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>

using namespace llvm;

namespace {
struct InlineCtorsPass : public FunctionPass {
  static char ID;

  typedef std::pair<Function*, uint64_t> FunctionPair;

  InlineCtorsPass() : FunctionPass(ID) {}

  void getFunctions(GlobalVariable *Tors, SmallVectorImpl<FunctionPair> &Vec) {
    if (!Tors) {
      return;
    }
    ConstantArray *A = cast<ConstantArray>(Tors->getOperand(0));

    for (int I = 0, E = A->getNumOperands(); I < E; ++I) {
      ConstantStruct *S = cast<ConstantStruct>(A->getOperand(I));
      if (Function *Fn = dyn_cast<Function>(S->getOperand(1))) {
        Vec.push_back(std::make_pair(Fn, cast<ConstantInt>(S->getOperand(0))->getZExtValue()));
      }
    }
  }

  bool runOnFunction(Function &F) override {
    if (!F.hasFnAttribute("shellvm-main")) {
      return false;
    }

    GlobalVariable *GlobalCtors = F.getParent()->getNamedGlobal("llvm.global_ctors");
    GlobalVariable *GlobalDtors = F.getParent()->getNamedGlobal("llvm.global_dtors");
    if (!GlobalCtors && !GlobalDtors) {
      return false;
    }
    
    SmallVector<FunctionPair, 4> Ctors;
    getFunctions(GlobalCtors, Ctors);
    SmallVector<FunctionPair, 4> Dtors;
    getFunctions(GlobalDtors, Dtors);

    // Sort constructors in ascending order of priority
    std::sort(Ctors.begin(), Ctors.end(), [] (FunctionPair &A, FunctionPair &B) -> bool {
      return A.second > B.second;
    });

    // Sort destructors in descending order of priority
    std::sort(Ctors.begin(), Ctors.end(), [] (FunctionPair &A, FunctionPair &B) -> bool {
      return A.second < B.second;
    });

    {
      BasicBlock *BBEntry = &F.getEntryBlock();
      BasicBlock::iterator I = BBEntry->begin();
      // Place calls to constructors after alloca
      while (isa<AllocaInst>(I))
        ++I;

      for (auto & [ Fn, Priority ] : Ctors) {
        CallInst::Create(Fn, ArrayRef<Value*>(), "", &*I);
      }
    }

    
    // Search for ret instructions
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      if (!isa<ReturnInst>(*I)) {
        continue;
      }

      for (auto & [ Fn, Priority ] : Dtors) {
        CallInst::Create(Fn, ArrayRef<Value*>(), "", &*I);
      }
    }


    // Delete llvm.global_ctors and llvm.global_dtors to prevent duplicate calls
    if (GlobalCtors)
      GlobalCtors->removeFromParent();
    if (GlobalDtors)
      GlobalDtors->removeFromParent();


    return true;
  }
}; // end of struct InlineCtorsPass
} // end of anonymous namespace

char InlineCtorsPass::ID = 0;

static RegisterPass<InlineCtorsPass> X("shellvm-inlinectors", "Moves global constructors and destructors to the shellvm-main function",
                                   false /* Only looks at CFG */,
                                   false /* Analysis Pass */);