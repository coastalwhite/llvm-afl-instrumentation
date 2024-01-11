#include "llvm/Pass.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <random>

using namespace llvm;

#define DEBUG_TYPE "afl-coverage"

STATISTIC(BasicBlocksInstrumented, "Basic blocks instrumented");

namespace {
class AFLCoverage : public ModulePass {
  public:
    static char ID;

    AFLCoverage() : ModulePass(ID) {}

    bool runOnModule(Module &M) override;

    void getAnalysisUsage(AnalysisUsage &AU) const override {
        // AU.addRequired<DataLayoutPass>();
    }
};
} // namespace

bool AFLCoverage::runOnModule(Module &M) {
    LLVM_DEBUG(dbgs() << "[C] AFL coverage instrumentation\n");

    bool count = false;

    LLVMContext &C = M.getContext();

    IntegerType *Int8Ty = IntegerType::getInt8Ty(C);
    IntegerType *Int16Ty = IntegerType::getInt16Ty(C);
    IntegerType *Int64Ty = IntegerType::getInt64Ty(C);

    const unsigned int mapsize = 1 << 16;

    // Get uniform distrubution of 16 bit numbers
    std::random_device rand_dev;
    std::mt19937 generator(rand_dev());
    std::uniform_int_distribution<int> distribution(0, mapsize - 1);

    // Globals for map and prev_loc
    GlobalVariable *AFLMap;
    AFLMap = M.getGlobalVariable("__afl_map_ptr");

    if (AFLMap == nullptr) {
        AFLMap = new GlobalVariable(M, ArrayType::get(Int8Ty, mapsize), false,
                                    GlobalValue::ExternalLinkage, 0,
                                    "__afl_map_ptr");
    }

    GlobalVariable *AFLPrevLoc = new GlobalVariable(
        M, Int16Ty, false, GlobalValue::ExternalLinkage, 0, "__prev_loc");
    AFLPrevLoc->setInitializer(ConstantInt::getNullValue(Int16Ty));

    GlobalVariable *BBCounter;
    if (count)
        BBCounter = new GlobalVariable(
            M, Int64Ty, false, GlobalValue::ExternalLinkage, 0, "__bb_counter");

    // Instrument all blocks
    for (auto &F : M)
        for (auto &BB : F) {
            BasicBlock::iterator IP = BB.getFirstInsertionPt();
            IRBuilder<> IRB(&(*IP));

            // Update the map
            unsigned int random = distribution(generator);

            LLVM_DEBUG(dbgs() << "[C] Using random: " << random << "\n");

            ConstantInt *CurLoc = ConstantInt::get(Int64Ty, random);

            LoadInst *PrevLoc = IRB.CreateLoad(Int16Ty, AFLPrevLoc, false);
            PrevLoc->setMetadata(M.getMDKindID("nosanitize"),
                                 MDNode::get(C, std::nullopt));

            Value *PrevLocCasted = IRB.CreateZExt(PrevLoc, IRB.getInt64Ty());

            std::vector<Value *> MapIdx{ConstantInt::get(Int64Ty, 0),
                                        IRB.CreateXor(PrevLocCasted, CurLoc)};

            Value *MapPtr = IRB.CreateInBoundsGEP(
                ArrayType::get(Int8Ty, mapsize), AFLMap, MapIdx);
            LoadInst *Counter = IRB.CreateLoad(Int8Ty, MapPtr, false);
            Counter->setMetadata(M.getMDKindID("nosanitize"),
                                 MDNode::get(C, std::nullopt));

            Value *Incr = IRB.CreateAdd(Counter, ConstantInt::get(Int8Ty, 1));
            IRB.CreateStore(Incr, MapPtr)
                ->setMetadata(M.getMDKindID("nosanitize"),
                              MDNode::get(C, std::nullopt));

            // Update prev_loc
            StoreInst *Store = IRB.CreateStore(
                ConstantInt::get(Int16Ty, random >> 1), AFLPrevLoc);
            Store->setMetadata(M.getMDKindID("nosanitize"),
                               MDNode::get(C, std::nullopt));

            if (count) {
                LoadInst *Counter = IRB.CreateLoad(Int8Ty, BBCounter, false);
                Counter->setMetadata(M.getMDKindID("nosanitize"),
                                     MDNode::get(C, std::nullopt));

                Value *Incr =
                    IRB.CreateAdd(Counter, ConstantInt::get(Int64Ty, 1));

                StoreInst *Store = IRB.CreateStore(Incr, BBCounter);
                Store->setMetadata(M.getMDKindID("nosanitize"),
                                   MDNode::get(C, std::nullopt));
            }

            BasicBlocksInstrumented++;
        }

    return true;
}

char AFLCoverage::ID = 0;
static RegisterPass<AFLCoverage> X("afl-coverage", "AFL style coverage pass",
                                   false, false);