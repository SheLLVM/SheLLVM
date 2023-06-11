#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include "PrecheckPass.h"

using namespace std;
using namespace llvm;

llvm::PreservedAnalyses Precheck::run(Function &F,
                                      FunctionAnalysisManager &FM) {
  if (!F.hasFnAttribute("shellvm-main")) {
    if (F.getUnnamedAddr() != GlobalValue::UnnamedAddr::None) {
      report_fatal_error("Function " + F.getName() +
                         " is not marked as unnamed_addr!");
    }
  } else {
    if (F.getUnnamedAddr() != GlobalValue::UnnamedAddr::Local) {
      report_fatal_error("SheLLVM main function " + F.getName() +
                         " is not marked as local_unnamed_addr!");
    }
  }

  return PreservedAnalyses::all();
}
