#include "CustomPasses.hpp"
#include "OperandUtils.h"
#include "lifterClass.h"
#include "utils.h"
#include <iostream>
#include <llvm/ADT/DenseMap.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/Casting.h>

// simplify Users with BFS
// because =>
// x = add a, b
// if we go simplify a then simplify x, then simplify b, we might miss
// simplifying x if we go simplify a, then simplify b, then simplify x we will
// not miss
//
// also refactor this

PATH_info getConstraintVal(llvm::Function* function, Value* constraint,
                           uint64_t& dest) {
  PATH_info result = PATH_unsolved;
  printvalue(constraint);
  auto simplified_constraint = simplifyValue(
      constraint,
      function->getParent()->getDataLayout()); // this is such a hack
  printvalue(simplified_constraint);

  if (llvm::ConstantInt* constInt =
          dyn_cast<llvm::ConstantInt>(simplified_constraint)) {
    printvalue(constInt) dest = constInt->getZExtValue();
    result = PATH_solved;
    return result;
  }

  return result;
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

  llvm::ModulePassManager modulePassManager =
      passBuilder.buildPerModuleDefaultPipeline(OptimizationLevel::O0);

  llvm::Module* module = clonedFuncx->getParent();

  bool changed = 0;
  do {
    changed = false;

    const size_t beforeSize = module->getInstructionCount();

    modulePassManager = passBuilder.buildPerModuleDefaultPipeline(OptimizationLevel::O1);

    modulePassManager.addPass(GEPLoadPass());
    modulePassManager.addPass(ReplaceTruncWithLoadPass());
    modulePassManager.addPass(PromotePseudoStackPass());

    modulePassManager.run(*module, moduleAnalysisManager);

    const size_t afterSize = module->getInstructionCount();

    changed = beforeSize != afterSize;

  } while (changed);

  modulePassManager =  passBuilder.buildPerModuleDefaultPipeline(OptimizationLevel::O2);

  modulePassManager.addPass(ResizeAllocatedStackPass());
  modulePassManager.addPass(PromotePseudoMemory());

  modulePassManager.run(*module, moduleAnalysisManager);
}

ICmpInst* findConditionInChain(Value* V, unsigned maxDepth = 5) {
  if (maxDepth == 0)
    return nullptr;
  if (auto icmp = dyn_cast<ICmpInst>(V))
    return icmp;
  if (auto opInst = dyn_cast<Instruction>(V)) {
    // Se l'istruzione � un cast o un'operazione aritmetica elementare,
    // prova a cercare in tutti i suoi operandi.
    for (unsigned i = 0, e = opInst->getNumOperands(); i < e; i++) {
      if (ICmpInst* found = findConditionInChain(opInst->getOperand(i), maxDepth - 1))
        return found;
    }
  }
  return nullptr;
}

// Funzione helper per "decodificare" la catena aritmetica Supponiamo che
// 'gepIndex' sia il valore usato come indice nel GEP
bool decodeIndexChain(Value* gepIndex, APInt& baseConstant, APInt& multiplier) {
  // Se gepIndex � prodotto da un'istruzione di add
  if (auto addInst = dyn_cast<BinaryOperator>(gepIndex)) {
    if (addInst->getOpcode() == Instruction::Add) {
      // Presumiamo che l'add abbia due operandi: uno � il prodotto della
      // condizione e l'altro � una costante
      Value* mulPart = addInst->getOperand(0);
      Value* basePart = addInst->getOperand(1);
      if (auto baseConst = dyn_cast<ConstantInt>(basePart)) {
        baseConstant = baseConst->getValue();
        // Ora, verifica che mulPart sia una moltiplicazione
        if (auto mulInst = dyn_cast<BinaryOperator>(mulPart)) {
          if (mulInst->getOpcode() == Instruction::Mul) {
            // Supponiamo che uno degli operandi sia la condizione estesa (che,
            // se vera, vale 1) e l'altro sia il moltiplicatore
            Value* op1 = mulInst->getOperand(0);
            Value* op2 = mulInst->getOperand(1);
            // Verifica se uno di questi � una costante
            if (auto multConst = dyn_cast<ConstantInt>(op2)) {
              multiplier = multConst->getValue();
              return true;
            }
            if (auto multConst = dyn_cast<ConstantInt>(op1)) {
              multiplier = multConst->getValue();
              return true;
            }
          }
        }
      }
    }
  }
  return false;
}

PATH_info lifterClass::solvePath(Function* function, uint64_t& dest, Value* simplifyValue) {

  PATH_info result = PATH_unsolved;
  if (llvm::ConstantInt* constInt =  dyn_cast<llvm::ConstantInt>(simplifyValue)) {
    dest = constInt->getZExtValue();
    result = PATH_solved;
    run = 0;
    auto bb_solved = BasicBlock::Create(function->getContext(), "bb_constraint", builder.GetInsertBlock()->getParent());

    builder.CreateBr(bb_solved);
    blockInfo = BBInfo(dest, bb_solved);
    return result;
  }

  if (PATH_info solved = getConstraintVal(function, simplifyValue, dest)) {
    if (solved == PATH_solved) {
      run = 0;
      std::cout << "Solved the constraint and moving to next path\n" << std::flush;
      auto bb_solved = BasicBlock::Create(function->getContext(), "bb_constraint", builder.GetInsertBlock()->getParent());

      builder.CreateBr(bb_solved);
      blockInfo = BBInfo(dest, bb_solved);
      return solved;
    }
  }

  // --- NUOVA PARTE: Gestione del pattern mascherato --- Se V è un LoadInst proveniente da un GEP che ha almeno un use ICmp, allora tentiamo il path splitting.
  if (auto loadInst = dyn_cast<LoadInst>(simplifyValue)) {
    if (auto gepInst = dyn_cast<GetElementPtrInst>(loadInst->getPointerOperand())) {
      // Supponiamo che l'indice del GEP sia l'ultimo operando
      Value* gepIndex     = gepInst->getOperand(gepInst->getNumOperands() - 1);
      ICmpInst* foundICmp = findConditionInChain(gepIndex);
      APInt baseConst(64, 0); // inizializza un APInt a 64 bit
      APInt mult(64, 0);
      if (decodeIndexChain(gepIndex, baseConst, mult)) {
        // Ora, calcola i due possibili offset. Convertiamo in valori:
        uint64_t falseOffset = baseConst.getZExtValue(); // condizione = 0
        uint64_t trueOffset  = baseConst.getZExtValue() + mult.getZExtValue(); // condizione = 1

        // A questo punto, possiamo "simulare" il comportamento del GEP: Ad esempio, se il GEP originale usava il valore base 'gepBase', creiamo due versioni:
        Value* gepBase = gepInst->getPointerOperand();

        APInt apTrue(64, 0);
        APInt apFalse(64, 0);
        // Ora estrai i valori concreti se possibili
        uint64_t trueDest = 0, falseDest = 0;
        BinaryOperations::readMemory(trueOffset, 8, apTrue);
        BinaryOperations::readMemory(falseOffset, 8, apFalse);

        // Crea i due basic block per i due percorsi
        BasicBlock* bb_true  = BasicBlock::Create(builder.getContext(), "bb_true", function);
        BasicBlock* bb_false = BasicBlock::Create(builder.getContext(), "bb_false", function);
        // Crea un branch condizionale basato sulla condizione (foundICmp)
        auto branchInst = builder.CreateCondBr(foundICmp, bb_true, bb_false);

        RegisterBranch(branchInst);

        // Imposta i due percorsi: Nel percorso corrente, assumiamo che la
        // condizione sia vera (ad esempio) o in base all'ordine desiderato
        blockInfo = BBInfo(apTrue.getZExtValue(), bb_false);

        // Crea un nuovo lifter per l'altro percorso
        lifterClass* newLifter = new lifterClass(*this);
        newLifter->blockInfo = BBInfo(apFalse.getZExtValue(), bb_true);

        // Aggiorna le assunzioni:
        assumptions[cast<Instruction>(foundICmp)]            = 1; // nel lifter corrente, condizione vera
        newLifter->assumptions[cast<Instruction>(foundICmp)] = 0; // nel nuovo lifter, condizione falsa

        lifters.push_back(newLifter);
        run = 0;
        return result;
      }
    }
  }
  // --- Fine NUOVA PARTE ---

  // unsolved
  printvalue(simplifyValue);
  run = 0;
  auto pvset = computePossibleValues(simplifyValue);
  vector<APInt> pv(pvset.begin(), pvset.end());
  if (pv.size() == 1) {
    printvalue2(pv[0]);
    auto bb_solved = BasicBlock::Create(function->getContext(), "bb_false", builder.GetInsertBlock()->getParent());

    builder.CreateBr(bb_solved);
    blockInfo = BBInfo(pv[0].getZExtValue(), bb_solved);
  }
  if (pv.size() == 2) {
    auto bb_false = BasicBlock::Create(function->getContext(), "bb_false", builder.GetInsertBlock()->getParent());
    auto bb_true = BasicBlock::Create(function->getContext(), "bb_true", builder.GetInsertBlock()->getParent());

    auto firstcase = pv[0];
    auto secondcase = pv[1];

    static auto try_simplify = [&](APInt c1, Value* simplifyv) -> optional<Value*> {
      if (auto si = dyn_cast<SelectInst>(simplifyv)) {
        auto firstcase_v = builder.getIntN(simplifyv->getType()->getIntegerBitWidth(), c1.getZExtValue());
        if (si->getTrueValue() == firstcase_v)
          return si->getCondition();
      }
      return nullopt;
    };
    Value* condition = nullptr;

    // condition value is a kind of hack
    // 1- if its a select, we can extract the condition
    // 1a- if firstcase is in the select, extract the condition
    // 1b- if secondcase is in the select, extract the condition and reverse
    // values
    // 2- create a hacky compare for condition == potentialvalue

    if (auto can_simplify = try_simplify(firstcase, simplifyValue))
      condition = can_simplify.value();
    else if (auto can_simplify2 = try_simplify(secondcase, simplifyValue)) {
      swap(firstcase, secondcase);
      condition = can_simplify2.value();
    } else
      condition = createICMPFolder(CmpInst::ICMP_EQ, simplifyValue, builder.getIntN(simplifyValue->getType()->getIntegerBitWidth(), firstcase.getZExtValue()));
    printvalue(condition);
    auto BR = builder.CreateCondBr(condition, bb_true, bb_false);

    RegisterBranch(BR);

    printvalue2(firstcase);
    printvalue2(secondcase);
    blockInfo = BBInfo(secondcase.getZExtValue(), bb_false);
    // for [this], we can assume condition is true
    // we can simplify any value tied to is dependent on condition,
    // and try to simplify any value calculates condition

    lifterClass* newlifter = new lifterClass(*this);

    // for [newlifter], we can assume condition is false
    newlifter->blockInfo = BBInfo(firstcase.getZExtValue(), bb_true);
    printvalue(condition);
    newlifter->assumptions[cast<Instruction>(condition)] = 1;

    assumptions[cast<Instruction>(condition)] = 0;

    lifters.push_back(newlifter);

    debugging::doIfDebug([&]() {
      std::string Filename = "output_newpath.ll";
      std::error_code EC;
      raw_fd_ostream OS(Filename, EC);
      function->getParent()->print(OS, nullptr);
    });
    std::cout << "created a new path\n" << std::flush;
  }
  if (pv.size() > 2) {
    UNREACHABLE("cant reach more than 2 paths!");
  }

  return result;
}