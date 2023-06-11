#ifndef PRECHECK_PASS_H
#define PRECHECK_PASS_H

#include "llvm/IR/PassManager.h"

using namespace llvm;

struct Precheck : PassInfoMixin<Precheck> {
  llvm::PreservedAnalyses run(Function &F, FunctionAnalysisManager &FM);
};

#endif