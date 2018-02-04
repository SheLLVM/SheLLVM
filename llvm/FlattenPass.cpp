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
#include "llvm/Analysis/CallGraph.h"
#include "MergeCallsPass.h"

using namespace llvm;

namespace {
struct Flatten : public FunctionPass {
  static char ID;
  std::vector<Function*> functionsFromMain;
  Flatten() : FunctionPass(ID) {}

  bool inlineFunction(Function &F) {
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

          if(std::find(functionsFromMain.begin(), functionsFromMain.end(), parentFunction) != functionsFromMain.end()) {
              // Called indirectly by main. This is not (currently) eligible.
              return false;
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
      delete mergeCalls;

      // Coerce LLVM to inline the function.
      InlineFunctionInfo IFI;
      for(CallInst* caller : callers) {
          InlineFunction(caller, IFI, nullptr, true);
      }

      // Check if we still have any users left. If not, we can proceed with erasing the function from the module.
      bool hasUsers = false;
      for(User* U : F.users()) {
          hasUsers = true;
          break;
      }

      if(!hasUsers) {
          F.eraseFromParent();
      }

      return true;
  }

  void dfsTraverse(CallGraphNode* node) {
      // Depth-first search of the call graph.
      // Used to build the list of functions invoked (directly or indirectly) by the main routine.
      for(auto &KV : *node) {
          CallGraphNode* child = KV.second;

          if(child == nullptr) {
              return;
          }

          if(std::find(functionsFromMain.begin(), functionsFromMain.end(), child->getFunction()) == functionsFromMain.end()) {
              functionsFromMain.push_back(child->getFunction());
          }

          dfsTraverse(child);
      }
  }

  bool runOnFunction(Function &F) override {
      if(!F.hasFnAttribute("shellvm-main")) {
          // We only want to run on the main function.
          return false;
      }

      // Build the call graph of the entire module.
      CallGraph* cg = new CallGraph(*F.getParent());

      // Start traversing the call graph for the main function.
      CallGraphNode* cgn = cg->operator[](&F);
      dfsTraverse(cgn);

      do {
          // Kinda messy, but necessary in this case. We need to signal deletion of any inlined functions from functionsFromMain.
          // This requires us to alter functionsFromMain from within the context of the loop.
          for(std::vector<Function*>::iterator it = functionsFromMain.begin(); it != functionsFromMain.end(); ) {
              if(inlineFunction(*(*it))) {
                  it = functionsFromMain.erase(it);
              } else {
                  ++it;
              }
          }
      } while(functionsFromMain.size() > 0);

      return true;
  }
}; // end of struct Flatten
}  // end of anonymous namespace

char Flatten::ID = 0;
static RegisterPass<Flatten> X("shellvm-flatten", "Flatten Functions Pass",
                                  false /* Only looks at CFG */,
                                  false /* Analysis Pass */);
