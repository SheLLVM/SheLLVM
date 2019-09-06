#include "MergeCallsPass.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/Local.h"

using namespace llvm;

namespace {
struct Flatten : public ModulePass {
  static char ID;
  FunctionPass *reg2mem = nullptr;
  std::vector<Function *> functionsFromMain;
  Flatten() : ModulePass(ID) {}

  /// Inlines function F into Caller, using MergeCalls to avoid duplication.
  bool inlineFunction(Function *F, Function *Caller) {
    if (F->hasFnAttribute(Attribute::NoInline)) {
      // Function not eligible for inlining. Bail out.
      return false;
    }

    if (F->isDeclaration()) {
      // Function is declared, and not defined. Bail out.
      return false;
    }

    // First, run the MergeCalls pass on it:
    CallInst *CallSite = MergeCalls::mergeCallSites(Caller, F);
    assert(CallSite && "MergeCalls did not return a unified callsite");

    if (reg2mem == nullptr)
      reg2mem = llvm::createDemoteRegisterToMemoryPass();
    reg2mem->runOnFunction(*Caller);

    // Coerce LLVM to inline the function.
    InlineFunctionInfo IFI;
    if (!InlineFunction(CallSite, IFI))
      return false;

    // If F is now completely dead, we can erase it from the module.
    if (F->isDefTriviallyDead()) {
      F->eraseFromParent();
    }

    return true;
  }

  /// See if Caller calls Callee
  bool doesNodeCallOther(CallGraphNode *Caller, CallGraphNode *Callee) {
    for (unsigned i = 0; i < Caller->size(); ++i) {
      if ((*Caller)[i] == Callee) {
        return true;
      }
    }

    return false;
  }

  /// Find the CallGraphNode that solely calls CGN, or nullptr if it's not
  /// called by exactly one other function.
  CallGraphNode *getSingleCaller(CallGraph &CG, CallGraphNode *CGN) {
    CallGraphNode *Caller = nullptr;
    for (auto &CGI : CG) {
      std::unique_ptr<CallGraphNode> &CGN2 = CGI.second;

      if (!doesNodeCallOther(CGN2.get(), CGN)) {
        continue; // Function represented by CGN2 does not call CGN
      }

      if (CGN2.get() == CG.getExternalCallingNode()) {
        // CGN2 - a caller of our function - is actually the external calling
        // node, meaning our function is called externally. It doesn't have a
        // single, definitive caller.
        return nullptr;
      }

      if (Caller != nullptr) {
        // We're already called by something else, meaning we have more
        // than one external caller.
        return nullptr;
      }

      // Record our caller, but keep looping to make sure it's the only one.
      Caller = CGN2.get();
      assert(Caller->getFunction() && "Caller doesn't represent a function!");
    }

    return Caller;
  }

  bool runOnModule(Module &M) override {
    bool ModifiedAny = false;
    bool ModifiedOne = false;
    SmallSetVector<Function *, 4> ModifiedFunctions;

    // We keep running, inlining functions into other functions, until there's
    // no work left to do.
    do {
      ModifiedOne = false;

      // Build the call graph of the entire module.
      CallGraph CG(M);

      for (Function &F : M.functions()) {
        CallGraphNode *CGN = CG[&F];
        CallGraphNode *Caller = getSingleCaller(CG, CGN);

        if (Caller == nullptr) {
          continue; // We need a function with a single caller function
        }

        if (inlineFunction(&F, Caller->getFunction())) {
          // Keep track of functions that need reg2mem
          ModifiedFunctions.insert(Caller->getFunction());
          ModifiedFunctions.remove(&F);

          // Mark the module as modified and start over
          ModifiedOne = true;
          ModifiedAny = true;
          break;
        }
      }

    } while (ModifiedOne);

    return ModifiedAny;
  }
}; // end of struct Flatten
} // end of anonymous namespace

char Flatten::ID = 0;
static RegisterPass<Flatten> X("shellvm-flatten", "Flatten Functions Pass",
                               false /* Only looks at CFG */,
                               false /* Analysis Pass */);
