#ifndef POSTCHECK_PASS_H
#define POSTCHECK_PASS_H

#include "llvm/IR/PassManager.h"

using namespace llvm;

struct Postcheck : PassInfoMixin<Postcheck> {
  bool dfsTraverse(CallGraphNode *node);

  llvm::PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
}; // end of struct Postcheck

#endif