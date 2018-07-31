#ifndef MERGE_CALLS_PASS_H
#define MERGE_CALLS_PASS_H

#include "llvm/Pass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Constants.h"
#include "llvm/Transforms/Utils/Local.h"
#include <map>
#include <vector>

using namespace llvm;

namespace {
struct MergeCalls : public FunctionPass {
  static char ID;
  MergeCalls() : FunctionPass(ID) {}

  bool valueEscapes(const Instruction *Inst) const {
    // Taken verbatim off LLVM's reg2mem pass.
    const BasicBlock *BB = Inst->getParent();
    for (const User *U : Inst->users()) {
      const Instruction *UI = cast<Instruction>(U);
      if (UI->getParent() != BB || isa<PHINode>(UI))
        return true;
     }
     return false;
  }

  BasicBlock* getUnreachableBlock(Function& F) {
      for (BasicBlock &BB : F) {
          if(BB.getInstList().size() == 1) {
              // Single-instruction block, let's see if it only consists of a unreachable instruction.
              const Instruction* instr = BB.getFirstNonPHI();
 
              if(instr == nullptr) {
                  // Guess we only have a single PHI instruction in this block (?)
                  continue;
              }

              if(isa<UnreachableInst>(instr)) {
                  // We only have a single instruction, and it's an "unreachable". Return the basic block it's in.
                  return &BB;
              }
          }
      }

      // No 'unreachable' basic block. Let's build our own.
      BasicBlock* unreachableBlock = BasicBlock::Create(F.getContext(), "", &F, nullptr);
      new UnreachableInst(F.getContext(), unreachableBlock);

      return unreachableBlock;
  }

  bool runOnFunction(Function &F) override {
    std::map<Function*, std::vector<CallInst*>> funcToInvokers;
    bool functionModified = false;

    for(BasicBlock &BB : F) {
        for(Instruction &I : BB) {
           if(isa<CallInst>(I)) {
                CallInst& C = cast<CallInst>(I);
                if(C.isInlineAsm()) {
                    // This is inline assembly; this can be deduplicated by a
                    // different pass if necessary. It doesn't call anything.
                    continue;
                }
                if(C.getCalledFunction() == nullptr) {
                    // Indirect invocation (call-by-ptr). Skip for now.
                    continue;
                }
                if(C.getCalledFunction()->isIntrinsic()) {
                    // LLVM intrinsic - don't tamper with this!
                    continue;
                }

                funcToInvokers[C.getCalledFunction()].push_back(&C);
            }
        }
    }

    for(auto &KV: funcToInvokers) {
        Function *target = KV.first;
        std::vector<CallInst*> &callers = KV.second;
        if(callers.size() > 1) {
            if(!functionModified)
                functionModified = true;
            std::vector<Value*> callArgs;
            std::map<BasicBlock*, BasicBlock*> callerToRet;
            std::map<Instruction*, BasicBlock*> callerToOrigParent;
            BasicBlock* callBlock = BasicBlock::Create(F.getContext(), "", &F, nullptr);

            // alloca insertion point tracing logic taken verbatim off LLVM's reg2mem pass.
            BasicBlock* BBEntry = &F.getEntryBlock();
            BasicBlock::iterator I = BBEntry->begin();
            while (isa<AllocaInst>(I)) ++I;

            CastInst *AllocaInsertionPoint = new BitCastInst(Constant::getNullValue(Type::getInt32Ty(F.getContext())), Type::getInt32Ty(F.getContext()), "mergecalls alloca point", &*I);

            for(CallInst* caller : callers) {
                std::vector<Instruction*> toDemote;
                BasicBlock* parentBlock = caller->getParent();
                BasicBlock* returnBlock = parentBlock->splitBasicBlock(caller->getNextNode(), "");
                callerToOrigParent[caller] = parentBlock;
                callerToRet[parentBlock] = returnBlock;

                // We actually need the vector for this:
                // The iterator gets invalidated during demotion.
                for(Instruction &I : *parentBlock) {
                    if (!(isa<AllocaInst>(I) && I.getParent() == BBEntry) && valueEscapes(&I))
                        toDemote.push_back(&I);
                }

                for(Instruction* demotedInstr : toDemote) {
                    DemoteRegToStack(*demotedInstr, false, AllocaInsertionPoint);
                }

                // Move the call instruction to the beginning of the return block (before the first non-PHI instruction).
                caller->moveBefore(returnBlock->getFirstNonPHI());

                // Demote the call instruction as well if it has any users.
                for(User* U : caller->users()) {
                    DemoteRegToStack(cast<Instruction>(*caller), false, AllocaInsertionPoint); 
                    break;
                }

                // Generate a branch to our call block and get rid of the branch generated by splitBasicBlock.
                BranchInst* ourBranch = BranchInst::Create(callBlock, parentBlock);
                ourBranch->getPrevNode()->eraseFromParent();
            }

            if(target->arg_size() > 0) {
                int argCtr = 0;
                for(Argument &A : target->args()) {
                    // We have to create a PHI node for each incoming basic block/value pair.
                    PHINode* argNode = PHINode::Create(A.getType(), callers.size(), "", callBlock);
                    for(CallInst* caller : callers) {
                        argNode->addIncoming(caller->getArgOperand(argCtr), callerToOrigParent[caller]);
                    }

                    callArgs.push_back(cast<Value>(argNode));
                    ++argCtr;
                }
            }

            CallInst* callInstr = CallInst::Create(cast<Value>(target), ArrayRef<Value*>(callArgs), "", callBlock);

            for(CallInst* caller : callers) {
                // Get rid of the original call, replace all references to it with the call in our call block.
                caller->replaceAllUsesWith(callInstr);
                caller->eraseFromParent();
            }

            // Emit PHI/switch instructions for branching back to the return blocks:
            PHINode* whereFromNode = PHINode::Create(Type::getInt32Ty(F.getContext()), callers.size(), "", callInstr);
            // Our default is gonna be a basic block that only contains an 'unreachable' instruction.
            SwitchInst* switchBackInstr = SwitchInst::Create(whereFromNode, getUnreachableBlock(F), callerToRet.size(), callBlock);
            int switchCtr = 0;

            for(auto &KV : callerToRet) {
                llvm::ConstantInt* branchIdx = llvm::ConstantInt::get(F.getContext(), llvm::APInt(32, switchCtr, true));
                whereFromNode->addIncoming(branchIdx, KV.first);
                switchBackInstr->addCase(branchIdx, KV.second);
                ++switchCtr;
            }
        }
    }

    return functionModified;
  }
}; // end of struct MergeCalls
}  // end of anonymous namespace

char MergeCalls::ID = 0;
#endif

