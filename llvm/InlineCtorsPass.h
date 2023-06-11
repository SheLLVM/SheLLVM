#ifndef INLINE_CTORS_PASS_H
#define INLINE_CTORS_PASS_H

#include "llvm/IR/PassManager.h"

using namespace llvm;

struct InlineCtorsPass : PassInfoMixin<InlineCtorsPass> {
  typedef std::pair<Function *, uint64_t> FunctionPair;

  void getFunctions(GlobalVariable *Tors, SmallVectorImpl<FunctionPair> &Vec);

  llvm::PreservedAnalyses run(Function &F, FunctionAnalysisManager &FM);
};

#endif