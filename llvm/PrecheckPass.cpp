#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

using namespace std;
using namespace llvm;

namespace {
struct Precheck : public FunctionPass {
  static char ID;
  Precheck() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
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

    return false;
  }
}; // end of struct Precheck
} // end of anonymous namespace

char Precheck::ID = 0;
static RegisterPass<Precheck> X("shellvm-precheck", "Precheck Pass",
                                false /* Only looks at CFG */,
                                false /* Analysis Pass */);
