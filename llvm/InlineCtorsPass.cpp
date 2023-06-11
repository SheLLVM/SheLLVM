#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>

#include "InlineCtorsPass.h"

using namespace llvm;

void InlineCtorsPass::getFunctions(GlobalVariable *Tors,
                                   SmallVectorImpl<FunctionPair> &Vec) {
  if (!Tors) {
    return;
  }
  ConstantArray *A = cast<ConstantArray>(Tors->getOperand(0));

  for (int I = 0, E = A->getNumOperands(); I < E; ++I) {
    ConstantStruct *S = cast<ConstantStruct>(A->getOperand(I));
    if (Function *Fn = dyn_cast<Function>(S->getOperand(1))) {
      Vec.push_back(std::make_pair(
          Fn, cast<ConstantInt>(S->getOperand(0))->getZExtValue()));
    }
  }
}

llvm::PreservedAnalyses InlineCtorsPass::run(Function &F,
                                             FunctionAnalysisManager &FM) {
  if (!F.hasFnAttribute("shellvm-main")) {

    return llvm::PreservedAnalyses::all();
  }

  GlobalVariable *GlobalCtors =
      F.getParent()->getNamedGlobal("llvm.global_ctors");
  GlobalVariable *GlobalDtors =
      F.getParent()->getNamedGlobal("llvm.global_dtors");
  if (!GlobalCtors && !GlobalDtors) {
    return llvm::PreservedAnalyses::all();
  }

  SmallVector<FunctionPair, 4> Ctors;
  getFunctions(GlobalCtors, Ctors);
  SmallVector<FunctionPair, 4> Dtors;
  getFunctions(GlobalDtors, Dtors);

  // Sort constructors in ascending order of priority
  std::sort(Ctors.begin(), Ctors.end(),
            [](const FunctionPair &A, const FunctionPair &B) -> bool {
              return A.second > B.second;
            });

  // Sort destructors in descending order of priority
  std::sort(Ctors.begin(), Ctors.end(),
            [](const FunctionPair &A, const FunctionPair &B) -> bool {
              return A.second < B.second;
            });

  {
    BasicBlock *BBEntry = &F.getEntryBlock();
    BasicBlock::iterator I = BBEntry->begin();
    // Place calls to constructors after alloca
    while (isa<AllocaInst>(I))
      ++I;

    for (auto &KV : Ctors) {
      CallInst::Create(KV.first, ArrayRef<Value *>(), "", &*I);
    }
  }

  // Search for ret instructions
  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
    if (!isa<ReturnInst>(*I)) {
      continue;
    }

    for (auto &KV : Dtors) {
      CallInst::Create(KV.first, ArrayRef<Value *>(), "", &*I);
    }
  }

  // Delete llvm.global_ctors and llvm.global_dtors to prevent duplicate calls
  if (GlobalCtors)
    GlobalCtors->removeFromParent();
  if (GlobalDtors)
    GlobalDtors->removeFromParent();

  return llvm::PreservedAnalyses::none();
}
