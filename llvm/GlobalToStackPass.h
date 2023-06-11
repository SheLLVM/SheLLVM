#ifndef GLOBAL_TO_STACK_PASS_H
#define GLOBAL_TO_STACK_PASS_H

#include "llvm/IR/PassManager.h"

using namespace llvm;

struct GlobalToStack : PassInfoMixin<GlobalToStack> {

  bool shouldInline(GlobalVariable &G);

  void disaggregateVars(Instruction *After, Value *Ptr,
                        SmallVectorImpl<Value *> &Idx, ConstantAggregate &C,
                        SmallSetVector<GlobalVariable *, 4> &Vars);

  void extractValuesFromStore(StoreInst *inst,
                              SmallSetVector<GlobalVariable *, 4> &Vars);

  void inlineGlobals(Function *F, SmallSetVector<GlobalVariable *, 4> &Vars);

  llvm::PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
}; // end of struct GlobalToStack

#endif