#include "llvm/Pass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Constants.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "MergeCallsPass.h"

using namespace llvm;

namespace {
struct Flatten : public FunctionPass {
  static char ID;
  Flatten() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
      if(F.hasFnAttribute(Attribute::NoInline)) {
          // Function not eligible for inlining. Bail out.
          return false;
      }

      if(F.isDeclaration()) {
          // Function is declared, and not defined. Bail out.
          return false;
      }

      std::vector<CallInst*> callers;

      for(User* U : F.users()) {
          if (!isa<CallInst>(U)) {
              // User is not a call instruction, proceed to the next one.
              continue;
          }

          CallInst* caller = cast<CallInst>(U);
          Function* parentFunction = caller->getFunction();

          if(!parentFunction->hasFnAttribute("shellvm-main")) {
              // Caller is not our main function, do not inline.
              continue;
          }

          callers.push_back(caller);
      }

      if(callers.size() <= 0) {
          // We have no users. We are not eligible for inlining. Bail.
          return false;
      }

      // First, run the MergeCalls pass on it:
      MergeCalls* mergeCalls = new MergeCalls();
      mergeCalls->runOnFunction(F);

      // Coerce LLVM to inline the function.
      InlineFunctionInfo IFI;
      for(CallInst* caller : callers) {
          InlineFunction(caller, IFI, nullptr, true);
      }

      return true;
  }
}; // end of struct Flatten
}  // end of anonymous namespace

char Flatten::ID = 0;
static RegisterPass<Flatten> X("shellvm-flatten", "Flatten Functions Pass",
                                  false /* Only looks at CFG */,
                                  false /* Analysis Pass */);
