#include "MergeCallsPass.h"
#include "llvm/Pass.h"

using namespace llvm;

static RegisterPass<MergeCalls> X("mergecalls", "Merge Calls Pass",
                                  false /* Only looks at CFG */,
                                  false /* Analysis Pass */);
