#ifndef PREPARE_PASS_H
#define PREPARE_PASS_H

#include "llvm/IR/PassManager.h"

using namespace llvm;

struct PreparePass : PassInfoMixin<PreparePass> {
  bool mustPreserve(const GlobalValue &value);
  llvm::PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

#endif