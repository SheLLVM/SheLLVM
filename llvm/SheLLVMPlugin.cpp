#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include "FlattenPass.h"
#include "GlobalToStackPass.h"
#include "InlineCtorsPass.h"
#include "MergeCallsPass.h"
#include "PostcheckPass.h"
#include "PrecheckPass.h"
#include "PreparePass.h"

using namespace llvm;

llvm::PassPluginLibraryInfo getSheLLVMPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "SheLLVM", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            // Register module passes
            PB.registerPipelineParsingCallback(
                [](StringRef Name, llvm::ModulePassManager &PM,
                   ArrayRef<llvm::PassBuilder::PipelineElement>) {
                  if (Name == "shellvm-prepare") {
                    PM.addPass(PreparePass());
                    return true;
                  } else if (Name == "shellvm-postcheck") {
                    PM.addPass(Postcheck());
                    return true;
                  } else if (Name == "shellvm-flatten") {
                    PM.addPass(Flatten());
                    return true;
                  } else if (Name == "shellvm-global2stack") {
                    PM.addPass(GlobalToStack());
                    return true;
                  }
                  return false;
                });

            // Register function passes
            PB.registerPipelineParsingCallback(
                [](StringRef Name, llvm::FunctionPassManager &PM,
                   ArrayRef<llvm::PassBuilder::PipelineElement>) {
                  if (Name == "shellvm-precheck") {
                    PM.addPass(Precheck());
                    return true;
                  } else if (Name == "mergecalls") {
                    PM.addPass(MergeCalls());
                    return true;
                  } else if (Name == "shellvm-inlinectors") {
                    PM.addPass(InlineCtorsPass());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getSheLLVMPluginInfo();
}
