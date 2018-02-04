#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

// This pass makes sure exactly one function is marked as the main
// function (via the __attribute__((annotate("shellvm-main"))) annotation).
// It removes this annotation, replaces it with an LLVM attribute, and marks
// all other functions in the module as private.
struct PreparePass : public ModulePass {
  static char ID;
  PreparePass() : ModulePass(ID) {}

  bool runOnModule(Module &M) override {
    auto GlobalMains = M.getNamedGlobal("llvm.global.annotations");
    if (!GlobalMains) {
      report_fatal_error("No functions have been annotated with shellvm-main");
    }

    bool FoundAnnotation = false;
    ConstantArray *A = cast<ConstantArray>(GlobalMains->getOperand(0));
    for (int I = 0, E = A->getNumOperands(); I < E; ++I) {
      auto D = cast<ConstantStruct>(A->getOperand(I));

      if (auto Fn = dyn_cast<Function>(D->getOperand(0)->getOperand(0))) {
        auto Annotation =
            cast<ConstantDataArray>(
                cast<GlobalVariable>(D->getOperand(1)->getOperand(0))
                    ->getOperand(0))
                ->getAsCString();

        if (Annotation == "shellvm-main") {
          if (FoundAnnotation) {
            report_fatal_error("More than one function has been annotated "
                      "with shellvm-main");
          }
          Fn->addFnAttr(Annotation);
          FoundAnnotation = true;
          // Continue searching for duplicate annotations
        }
      }
    }

    if (!FoundAnnotation) {
      report_fatal_error("No functions have been annotated with shellvm-main");
    }

    for (Function &F : M.getFunctionList()) {
      if (!F.hasFnAttribute("shellvm-main") && !F.isDeclaration()) {
        // Mark all other functions as private,
        // excluding functions defined in another module
        F.setLinkage(GlobalValue::InternalLinkage);
      }
    }

    return true;
  }

}; // end of struct PreparePass
} // end of anonymous namespace

char PreparePass::ID = 0;

static RegisterPass<PreparePass> X("shellvm-prepare", "SheLLVM Prepare Pass",
                                   false /* Only looks at CFG */,
                                   false /* Analysis Pass */);