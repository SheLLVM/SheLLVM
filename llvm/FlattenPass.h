#ifndef FLATTEN_PASS_H
#define FLATTEN_PASS_H

#include "llvm/IR/PassManager.h"

using namespace llvm;

struct Flatten : PassInfoMixin<Flatten> {
  FunctionPass *reg2mem = nullptr;
  std::vector<Function *> functionsFromMain;

  /// Inlines function F into Caller, using MergeCalls to avoid duplication.
  bool inlineFunction(Function *F, Function *Caller);

  /// See if Caller calls Callee
  bool doesNodeCallOther(CallGraphNode *Caller, CallGraphNode *Callee);

  /// Find the CallGraphNode that solely calls CGN, or nullptr if it's not
  /// called by exactly one other function.
  CallGraphNode *getSingleCaller(CallGraph &CG, CallGraphNode *CGN);

  llvm::PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
}; // end of struct Flatten

#endif