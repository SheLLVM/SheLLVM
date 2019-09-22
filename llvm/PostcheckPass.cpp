#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

using namespace std;
using namespace llvm;

namespace {
struct Postcheck : public ModulePass {
  static char ID;
  Postcheck() : ModulePass(ID) {}

  bool dfsTraverse(CallGraphNode *node) {
    for (auto &KV : *node) {
      CallGraphNode *child = KV.second;
      if (child == nullptr) {
        return true;
      }

      if (child->getFunction() && !child->getFunction()->isIntrinsic()) {
        return false;
      }

      if (!dfsTraverse(child)) {
        return false;
      }
    }

    return true;
  }

  bool runOnModule(Module &M) override {
    // Check 1: Make sure we only have a single function, with the attribute
    // shellvm-main.
    Function *mainFunc = nullptr;

    for (Function &f : M.functions()) {
      if (f.isIntrinsic()) {
        continue;
      }

      if (mainFunc) {
        report_fatal_error("More than one function in module!");
        mainFunc = nullptr;
        continue;
      }

      mainFunc = &f;
    }

    if (!mainFunc) {
      report_fatal_error("No functions found in module!");
    }

    if (mainFunc && !mainFunc->hasFnAttribute("shellvm-main")) {
      report_fatal_error("SheLLVM main function " + mainFunc->getName() +
                         " has no shellvm-main attribute!");
    }

    // Check 2: Make sure there are no globals.
    for (GlobalVariable &global : M.getGlobalList()) {
      if (!global.getSection().equals("llvm.metadata")) {
        report_fatal_error("Module has global variables!");
      }
    }

    // Check 3: No switch instructions allowed in function body.
    if (mainFunc) {
      for (BasicBlock &BB : *mainFunc) {
        for (Instruction &I : BB) {
          if (isa<SwitchInst>(I)) {
            report_fatal_error(
                "Switch instruction found within main function.");
          }
        }
      }

      // Check 4: No external calls (that aren't LLVM intrinsics).
      CallGraph *cg = new CallGraph(M);
      CallGraphNode *cgn = cg->operator[](mainFunc);
      if (!dfsTraverse(cgn)) {
        report_fatal_error(
            "Non-intrinsic external call found within main function!");
      }
    }

    return false;
  }
}; // end of struct Postcheck
} // end of anonymous namespace

char Postcheck::ID = 0;
static RegisterPass<Postcheck> X("shellvm-postcheck", "Postcheck Pass",
                                 false /* Only looks at CFG */,
                                 false /* Analysis Pass */);
