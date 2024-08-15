#include "CustomPasses.hpp"
#include "OperandUtils.h"
#include "includes.h"
#include "lifterClass.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/Casting.h>

void* file_base_g;
ZyanU8* data_g;

struct InstructionDependencyOrder {
  bool operator()(Instruction* const& a, Instruction* const& b) const {

    Function* F = a->getFunction();
    GEPStoreTracker::updateDomTree(*F);
    DominatorTree* DT = GEPStoreTracker::getDomTree();

    return (comesBefore(b, a, *DT));
  }
};

void lifterClass::replaceAllUsesWithandReplaceRMap(Value* v, Value* nv,
                                                   ReverseRegisterMap rVMap) {

  // if two values are same, we go in a infinite loop
  if (v == nv)
    return;

  auto registerV = rVMap[v];

  if (registerV) {
    if (isa<Instruction>(v)) {
      // auto registerI = cast<Instruction>(v);
      SetRegisterValue(registerV, v);
    }
  }

  v->replaceAllUsesWith(nv);

  // redundant?
  v = nv;

  // dont ask me this i dont know

  // users start from latest user
  std::vector<User*> users;
  for (auto& use : v->uses()) {
    users.push_back(use.getUser());
  }

  // iterate over the users in reverse order
  for (auto it = users.rbegin(); it != users.rend(); ++it) {
    User* user = *it;
    if (auto GEPuser = dyn_cast<GetElementPtrInst>(user)) {
      for (auto StoreUser : GEPuser->users()) {
        if (auto SI = dyn_cast<StoreInst>(StoreUser)) {
          GEPStoreTracker::updateMemoryOp(SI);
        }
      }
    }
  }
}

// simplify Users with BFS
// because =>
// x = add a, b
// if we go simplify a then simplify x, then simplify b, we might miss
// simplifying x if we go simplify a, then simplify b, then simplify x we will
// not miss
//
// also refactor this

PATH_info getConstraintVal(llvm::Function* function, Value* constraint, uint64_t& dest) {
  PATH_info result = PATH_unsolved;
  printvalue(constraint);
  auto simplified_constraint = simplifyValue(constraint, function->getParent()->getDataLayout()); // this is such a hack
  printvalue(simplified_constraint);

  if (llvm::ConstantInt* constInt =  dyn_cast<llvm::ConstantInt>(simplified_constraint)) {
    printvalue(constInt) dest = constInt->getZExtValue();
    result = PATH_solved;
    return result;
  }

  return result;
}

void minimal_optpass(Function* clonedFuncx) {

  llvm::PassBuilder passBuilder;

  llvm::LoopAnalysisManager loopAnalysisManager;
  llvm::FunctionAnalysisManager functionAnalysisManager;
  llvm::CGSCCAnalysisManager cGSCCAnalysisManager;
  llvm::ModuleAnalysisManager moduleAnalysisManager;

  passBuilder.registerModuleAnalyses(moduleAnalysisManager);
  passBuilder.registerCGSCCAnalyses(cGSCCAnalysisManager);
  passBuilder.registerFunctionAnalyses(functionAnalysisManager);
  passBuilder.registerLoopAnalyses(loopAnalysisManager);
  passBuilder.crossRegisterProxies(loopAnalysisManager, functionAnalysisManager, cGSCCAnalysisManager, moduleAnalysisManager);

  llvm::ModulePassManager modulePassManager;

  llvm::Module* module = clonedFuncx->getParent();

  llvm::FunctionPassManager fpm;

  fpm.addPass(llvm::InstCombinePass());
  fpm.addPass(llvm::EarlyCSEPass(true));
  fpm.addPass(llvm::InstCombinePass());

  modulePassManager.addPass(llvm::createModuleToFunctionPassAdaptor(std::move(fpm)));

  modulePassManager.run(*module, moduleAnalysisManager);
}

void final_optpass(Function* clonedFuncx) {
  llvm::PassBuilder passBuilder;

  llvm::LoopAnalysisManager loopAnalysisManager;
  llvm::FunctionAnalysisManager functionAnalysisManager;
  llvm::CGSCCAnalysisManager cGSCCAnalysisManager;
  llvm::ModuleAnalysisManager moduleAnalysisManager;

  passBuilder.registerModuleAnalyses(moduleAnalysisManager);
  passBuilder.registerCGSCCAnalyses(cGSCCAnalysisManager);
  passBuilder.registerFunctionAnalyses(functionAnalysisManager);
  passBuilder.registerLoopAnalyses(loopAnalysisManager);
  passBuilder.crossRegisterProxies(loopAnalysisManager, functionAnalysisManager, cGSCCAnalysisManager, moduleAnalysisManager);

  llvm::ModulePassManager modulePassManager = passBuilder.buildPerModuleDefaultPipeline(OptimizationLevel::O0);

  llvm::Module* module = clonedFuncx->getParent();

  bool changed;
  do {
    changed = false;

    size_t beforeSize = module->getInstructionCount();

    modulePassManager = passBuilder.buildPerModuleDefaultPipeline(OptimizationLevel::O3);

    modulePassManager.addPass(GEPLoadPass());
    modulePassManager.addPass(ReplaceTruncWithLoadPass());
    modulePassManager.addPass(PromotePseudoStackPass());

    modulePassManager.run(*module, moduleAnalysisManager);

    size_t afterSize = module->getInstructionCount();

    if (beforeSize != afterSize) {
      changed = true;
    }

  } while (changed);

  modulePassManager = passBuilder.buildPerModuleDefaultPipeline(OptimizationLevel::O3);

  modulePassManager.addPass(ResizeAllocatedStackPass());
  modulePassManager.addPass(PromotePseudoMemory());

  modulePassManager.run(*module, moduleAnalysisManager);
}

llvm::ValueToValueMapTy* flipVMap(const ValueToValueMapTy& VMap) {

  ValueToValueMapTy* RevMap = new llvm::ValueToValueMapTy;
  for (const auto& pair : VMap) {
    (*RevMap)[pair.second] = const_cast<Value*>(pair.first);
  }
  return RevMap;
}

PATH_info lifterClass::solvePath(Function* function, uint64_t& dest, Value* simplifyValue) {

  PATH_info result = PATH_unsolved;
  if (llvm::ConstantInt* constInt = dyn_cast<llvm::ConstantInt>(simplifyValue)) {
    dest = constInt->getZExtValue();
    result = PATH_solved;
    return result;
  }


  llvm::ValueToValueMapTy VMap;
  llvm::Function* clonedFunctmp = llvm::CloneFunction(function, VMap);

  std::unique_ptr<Module> destinationModule = std::make_unique<Module>("destination_module", function->getContext());
  clonedFunctmp->removeFromParent();

  destinationModule->getFunctionList().push_back(clonedFunctmp);

  Function* clonedFunc = destinationModule->getFunction(clonedFunctmp->getName());

  minimal_optpass(clonedFunc);
  debugging::doIfDebug([&]() {
    std::string Filename = "clonedFunc_JMP.ll";
    std::error_code EC;
    raw_fd_ostream OS(Filename, EC);
    clonedFunc->print(OS);
  });

  if (PATH_info solved = getConstraintVal(clonedFunc, simplifyValue, dest)) {
    if (solved == PATH_solved) {
      return solved;
    }
  }

  for (auto& BB : *clonedFunc) {
    if (auto* RI = dyn_cast<ReturnInst>(BB.getTerminator())) {
      if (auto* CI = dyn_cast<ConstantInt>(RI->getReturnValue())) {
        dest = CI->getZExtValue();
        return PATH_solved;
      }
    }
  }



  if (PATH_info solved = getConstraintVal(function, simplifyValue, dest)) {
    if (solved == PATH_solved) {
      outs() << "Solved the constraint and moving to next path\n";
      outs().flush();
      return solved;
    }
  }

  return result;
}