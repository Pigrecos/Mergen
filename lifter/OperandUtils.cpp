#include "OperandUtils.h"
#include "GEPTracker.h"
#include "includes.h"
#include "lifterClass.h"
#include <llvm/Analysis/DomConditionCache.h>
#include <llvm/Analysis/InstructionSimplify.h>
#include <llvm/Analysis/SimplifyQuery.h>
#include <llvm/Analysis/ValueLattice.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>

#ifndef TESTFOLDER
#define TESTFOLDER
#define TESTFOLDER3
#define TESTFOLDER4
#define TESTFOLDER5
#define TESTFOLDER6
#define TESTFOLDER7
#define TESTFOLDER8
#define TESTFOLDERshl
#define TESTFOLDERshr
#endif

using namespace PatternMatch;

static void findAffectedValues(Value* Cond, SmallVectorImpl<Value*>& Affected) {
  auto AddAffected = [&Affected](Value* V) {
    if (isa<Argument>(V) || isa<GlobalValue>(V)) {
      Affected.push_back(V);
    } else if (auto* I = dyn_cast<Instruction>(V)) {
      Affected.push_back(I);

      // Peek through unary operators to find the source of the condition.

      Value* Op;
      if (match(I, m_PtrToInt(m_Value(Op)))) {
        if (isa<Instruction>(Op) || isa<Argument>(Op))
          Affected.push_back(Op);
      }
    }
  };

  ICmpInst::Predicate Pred;
  Value* A;
  if (match(Cond, m_ICmp(Pred, m_Value(A), m_Constant()))) {
    AddAffected(A);

    if (ICmpInst::isEquality(Pred)) {
      Value* X;
      // (X & C) or (X | C) or (X ^ C).
      // (X << C) or (X >>_s C) or (X >>_u C).
      if (match(A, m_BitwiseLogic(m_Value(X), m_ConstantInt())) ||
          match(A, m_Shift(m_Value(X), m_ConstantInt())))
        AddAffected(X);
    } else {
      Value* X;
      // Handle (A + C1) u< C2, which is the canonical form of A > C3 && A < C4.
      if (match(A, m_Add(m_Value(X), m_ConstantInt())))
        AddAffected(X);
    }
  }
}

namespace GetSimplifyQuery {

  vector<BranchInst*> BIlist;
  void RegisterBranch(BranchInst* BI) {
    //
    BIlist.push_back(BI);
  }

  SimplifyQuery createSimplifyQuery(Function* fnc, Instruction* Inst) {
    AssumptionCache AC(*fnc);
    GEPStoreTracker::updateDomTree(*fnc);
    auto DT = GEPStoreTracker::getDomTree();
    auto DL = fnc->getParent()->getDataLayout();
    static TargetLibraryInfoImpl TLIImpl(
        Triple(fnc->getParent()->getTargetTriple()));
    static TargetLibraryInfo TLI(TLIImpl);

    DomConditionCache* DC = new DomConditionCache();

    for (auto BI : BIlist) {

      DC->registerBranch(BI);
      SmallVector<Value*, 16> Affected;
      findAffectedValues(BI->getCondition(), Affected);
      for (auto affectedvalues : Affected) {
        printvalue(affectedvalues);
      }
    }

    SimplifyQuery SQ(DL, &TLI, DT, nullptr, Inst, true, true, DC);

    return SQ;
  }

} // namespace GetSimplifyQuery

// returns if a comes before b
bool comesBefore(Instruction* a, Instruction* b, DominatorTree& DT) {

  bool sameBlock =
      a->getParent() == b->getParent(); // if same block, use ->comesBefore,

  if (sameBlock) {
    return a->comesBefore(b); // if a comes before b, return true
  }
  // if "a"'s block dominates "b"'s block, "a" comes first.
  bool dominate = DT.properlyDominates(a->getParent(), b->getParent());
  return dominate;
}

using namespace llvm::PatternMatch;

Value* doPatternMatching(Instruction::BinaryOps I, Value* op0, Value* op1) {
  Value* X = nullptr;
  switch (I) {
  case Instruction::And: {
    // X & ~X
    // how the hell we remove this zext and truncs it looks horrible

    if ((match(op0, m_Not(m_Value(X))) && X == op1) ||
        (match(op1, m_Not(m_Value(X))) && X == op0) ||
        (match(op0, m_ZExt(m_Not(m_Value(X)))) &&
         match(op1, m_ZExt(m_Specific(X)))) ||
        (match(op1, m_ZExt(m_Not(m_Value(X)))) &&
         match(op0, m_ZExt(m_Specific(X)))) ||
        (match(op0, m_Trunc(m_Not(m_Value(X)))) &&
         match(op1, m_Trunc(m_Specific(X)))) ||
        (match(op1, m_Trunc(m_Not(m_Value(X)))) &&
         match(op0, m_Trunc(m_Specific(X))))) {
      auto possibleSimplify = ConstantInt::get(op1->getType(), 0);
      return possibleSimplify;
    }
    // ~X & ~X

    if (match(op0, m_Not(m_Value(X))) && X == op1)
      return op0;

    break;
  }
  case Instruction::Xor: {
    // X ^ ~X
    if ((match(op0, m_Not(m_Value(X))) && X == op1) ||
        (match(op1, m_Not(m_Value(X))) && X == op0) ||
        (match(op0, m_ZExt(m_Not(m_Value(X)))) &&
         match(op1, m_ZExt(m_Specific(X)))) ||
        (match(op1, m_ZExt(m_Not(m_Value(X)))) &&
         match(op0, m_ZExt(m_Specific(X)))) ||
        (match(op0, m_Trunc(m_Not(m_Value(X)))) &&
         match(op1, m_Trunc(m_Specific(X)))) ||
        (match(op1, m_Trunc(m_Not(m_Value(X)))) &&
         match(op0, m_Trunc(m_Specific(X))))) {
      auto possibleSimplify = ConstantInt::get(op1->getType(), -1);
      printvalue(possibleSimplify);
      return possibleSimplify;
    }
    if (match(op0, m_Specific(op1)) ||
        (match(op0, m_Trunc(m_Value(X))) &&
         match(op1, m_Trunc(m_Specific(X)))) ||
        (match(op0, m_ZExt(m_Value(X))) && match(op1, m_ZExt(m_Specific(X))))) {
      auto possibleSimplify = ConstantInt::get(op1->getType(), 0);
      printvalue(possibleSimplify);
      return possibleSimplify;
    }
    break;
  }
  default: {
    return nullptr;
  }
  }

  return nullptr;
}

static void computeKnownBitsFromCmp(Value* V, CmpInst::Predicate Pred, Value* LHS, Value* RHS, KnownBits& Known, const SimplifyQuery& Q) {
  // Handle comparison of pointer to null explicitly, as it will not be
  // covered by the m_APInt() logic below.
  
  if (LHS == V && match(RHS, m_Zero())) {
    int iszero = Pred;
    
    switch (Pred) {
    case ICmpInst::ICMP_EQ:
      Known.setAllZero();
      break;
    case ICmpInst::ICMP_SGE:
    case ICmpInst::ICMP_SGT:
      Known.makeNonNegative();
      break;
    case ICmpInst::ICMP_SLT:
      Known.makeNegative();
      break;
    default:
      break;
    }
  }

  unsigned BitWidth = Known.getBitWidth();
  auto m_V =
      m_CombineOr(m_Specific(V), m_PtrToIntSameSize(Q.DL, m_Specific(V)));

  const APInt *Mask, *C;
  uint64_t ShAmt;
  switch (Pred) {
  case ICmpInst::ICMP_EQ:
    // assume(V = C)
    if (match(LHS, m_V) && match(RHS, m_APInt(C))) {
      Known = Known.unionWith(KnownBits::makeConstant(*C));
      // assume(V & Mask = C)
    } else if (match(LHS, m_And(m_V, m_APInt(Mask))) &&
               match(RHS, m_APInt(C))) {
      // For one bits in Mask, we can propagate bits from C to V.
      Known.Zero |= ~*C & *Mask;
      Known.One |= *C & *Mask;
      // assume(V | Mask = C)
    } else if (match(LHS, m_Or(m_V, m_APInt(Mask))) && match(RHS, m_APInt(C))) {
      // For zero bits in Mask, we can propagate bits from C to V.
      Known.Zero |= ~*C & ~*Mask;
      Known.One |= *C & ~*Mask;
      // assume(V ^ Mask = C)
    } else if (match(LHS, m_Xor(m_V, m_APInt(Mask))) &&
               match(RHS, m_APInt(C))) {
      // Equivalent to assume(V == Mask ^ C)
      Known = Known.unionWith(KnownBits::makeConstant(*C ^ *Mask));
      // assume(V << ShAmt = C)
    } else if (match(LHS, m_Shl(m_V, m_ConstantInt(ShAmt))) &&
               match(RHS, m_APInt(C)) && ShAmt < BitWidth) {
      // For those bits in C that are known, we can propagate them to known
      // bits in V shifted to the right by ShAmt.
      KnownBits RHSKnown = KnownBits::makeConstant(*C);
      RHSKnown.Zero.lshrInPlace(ShAmt);
      RHSKnown.One.lshrInPlace(ShAmt);
      Known = Known.unionWith(RHSKnown);
      // assume(V >> ShAmt = C)
    } else if (match(LHS, m_Shr(m_V, m_ConstantInt(ShAmt))) &&
               match(RHS, m_APInt(C)) && ShAmt < BitWidth) {
      KnownBits RHSKnown = KnownBits::makeConstant(*C);
      // For those bits in RHS that are known, we can propagate them to known
      // bits in V shifted to the right by C.
      Known.Zero |= RHSKnown.Zero << ShAmt;
      Known.One |= RHSKnown.One << ShAmt;
    }
    break;
  case ICmpInst::ICMP_NE: {
    // assume (V & B != 0) where B is a power of 2
    const APInt* BPow2;
    if (match(LHS, m_And(m_V, m_Power2(BPow2))) && match(RHS, m_Zero()))
      Known.One |= *BPow2;
    break;
  }
  default:
    const APInt* Offset = nullptr;
    if (match(LHS, m_CombineOr(m_V, m_Add(m_V, m_APInt(Offset)))) &&
        match(RHS, m_APInt(C))) {
      ConstantRange LHSRange = ConstantRange::makeAllowedICmpRegion(Pred, *C);
      if (Offset)
        LHSRange = LHSRange.sub(*Offset);
      Known = Known.unionWith(LHSRange.toKnownBits());
    }
    break;
  }
}

void getKnownBitsFromContext(Value* V, KnownBits& Known, const SimplifyQuery& Q) {
  if (!Q.CxtI)
    return;
  
  if (Q.DC && Q.DT) {
    
    // Handle dominating conditions.
    for (BranchInst* BI : Q.DC->conditionsFor(V)) {
      auto* Cmp = dyn_cast<ICmpInst>(BI->getCondition());
      printvalue(BI->getCondition());

      if (!Cmp)
        continue;

      BasicBlockEdge Edge0(BI->getParent(), BI->getSuccessor(0));
      if (Q.DT->dominates(Edge0, Q.CxtI->getParent()))
        computeKnownBitsFromCmp(V, Cmp->getPredicate(), Cmp->getOperand(0),
                                Cmp->getOperand(1), Known, Q);

      BasicBlockEdge Edge1(BI->getParent(), BI->getSuccessor(1));
      if (Q.DT->dominates(Edge1, Q.CxtI->getParent()))
        computeKnownBitsFromCmp(V, Cmp->getInversePredicate(),
                                Cmp->getOperand(0), Cmp->getOperand(1), Known,
                                Q);
    }
    if (Known.hasConflict())
      Known.resetAll();
  }
 
}

// how to get all possible values
// 1- find the value with least amount of known bits (excluding constants)
// 2- then calculate

KnownBits analyzeValueKnownBits(Value* value, Instruction* ctxI) {

  KnownBits knownBits(64);
  knownBits.resetAll();
  
  if (value->getType() == Type::getInt128Ty(value->getContext()))
    return knownBits;

  if (auto CIv = dyn_cast<ConstantInt>(value)) {
    return KnownBits::makeConstant(APInt(64, CIv->getZExtValue(), false));
  }

  auto SQ = GetSimplifyQuery::createSimplifyQuery(
      ctxI->getParent()->getParent(), ctxI);

  computeKnownBits(value, knownBits, 0, SQ);

  // BLAME
  if (knownBits.getBitWidth() < 64)
    (&knownBits)->zext(64);

  return knownBits;
}

Value* simplifyValue(Value* v, const DataLayout& DL) {

  if (!isa<Instruction>(v))
    return v;

  Instruction* inst = cast<Instruction>(v);

  /*
  shl al, cl
  where cl is bigger than 8, it just clears the al
  */

  SimplifyQuery SQ(DL, inst);
  if (auto vconstant = ConstantFoldInstruction(inst, DL)) {
    if (isa<PoisonValue>(vconstant)) // if poison it should be 0 for shifts,
                                     // can other operations generate poison
                                     // without a poison value anyways?
      return ConstantInt::get(v->getType(), 0);
    return vconstant;
  }

  if (auto vsimplified = simplifyInstruction(inst, SQ)) {

    if (isa<PoisonValue>(vsimplified)) // if poison it should be 0 for shifts,
                                       // can other operations generate poison
                                       // without a poison value anyways?
      return ConstantInt::get(v->getType(), 0);

    return vsimplified;
  }

  return v;
}

Value* simplifyLoadValue(Value* v) {

  Instruction* inst = cast<Instruction>(v);
  Function& F = *inst->getFunction();

  llvm::IRBuilder<> builder(&*F.getEntryBlock().getFirstInsertionPt());
  auto LInst = cast<LoadInst>(v);
  auto GEPVal = LInst->getPointerOperand();

  if (!isa<GetElementPtrInst>(GEPVal))
    return nullptr;

  auto GEPInst = cast<GetElementPtrInst>(GEPVal);

  Value* pv = GEPInst->getPointerOperand();
  Value* idxv = GEPInst->getOperand(1);
  uint32_t byteCount = v->getType()->getIntegerBitWidth() / 8;

  printvalue(v) printvalue(pv) printvalue(idxv) printvalue2(byteCount);

  auto retVal = GEPStoreTracker::solveLoad(cast<LoadInst>(v));

  printvalue(v);
  return retVal;
}

struct InstructionKey {
  unsigned opcode;
  Value* operand1;
  union {
    Value* operand2;
    Type* destType;
  };

  InstructionKey(unsigned opcode, Value* operand1, Value* operand2)
      : opcode(opcode), operand1(operand1), operand2(operand2) {};

  InstructionKey(unsigned opcode, Value* operand1, Type* destType)
      : opcode(opcode), operand1(operand1), destType(destType) {};

  bool operator==(const InstructionKey& other) const {
    return opcode == other.opcode && operand1 == other.operand1 &&
           operand2 == other.operand2 && destType == other.destType;
  }
};

struct InstructionKeyHash {
  uint64_t operator()(const InstructionKey& key) const {
    uint64_t h1 = std::hash<unsigned>()(key.opcode);
    uint64_t h2 = reinterpret_cast<uint64_t>(key.operand1);
    uint64_t h3 = reinterpret_cast<uint64_t>(key.operand2);
    return h1 ^ (h2 << 1) ^ (h3 << 2);
  }
};

class InstructionCache {
public:
  Value* getOrCreate(IRBuilder<>& builder, const InstructionKey& key,
                     const Twine& Name) {
    auto it = cache.find(key);
    if (it != cache.end()) {
      return it->second;
    }

    Value* newInstruction = nullptr;
    if (key.operand2) {
      // Binary instruction
      newInstruction =
          builder.CreateBinOp(static_cast<Instruction::BinaryOps>(key.opcode),
                              key.operand1, key.operand2, Name);
    } else if (key.destType) {
      // Cast instruction
      switch (key.opcode) {
      case Instruction::Trunc:
        newInstruction = builder.CreateTrunc(key.operand1, key.destType, Name);
        break;
      case Instruction::ZExt:
        newInstruction = builder.CreateZExt(key.operand1, key.destType, Name);
        break;
      case Instruction::SExt:
        newInstruction = builder.CreateSExt(key.operand1, key.destType, Name);
        break;
      // Add other cast operations as needed
      default:
        llvm_unreachable("Unsupported cast opcode");
      }
    } else {
      // Unary instruction
      switch (key.opcode) {
      case Instruction::Trunc:
        newInstruction =
            builder.CreateTrunc(key.operand1, key.operand1->getType(), Name);
        break;
      case Instruction::ZExt:
        newInstruction =
            builder.CreateZExt(key.operand1, key.operand1->getType(), Name);
        break;
      case Instruction::SExt:
        newInstruction =
            builder.CreateSExt(key.operand1, key.operand1->getType(), Name);
        break;
      // Add other unary operations as needed
      default:
        llvm_unreachable("Unsupported unary opcode");
      }
    }

    cache[key] = newInstruction;
    return newInstruction;
  }

private:
  std::unordered_map<InstructionKey, Value*, InstructionKeyHash> cache;
};

Value* createInstruction(IRBuilder<>& builder, unsigned opcode, Value* operand1,
                         Value* operand2, Type* destType, const Twine& Name) {
  static InstructionCache cache;
  DataLayout DL(builder.GetInsertBlock()->getParent()->getParent());

  InstructionKey key = {opcode, operand1,
                        (Value*)((uint64_t)operand2 | (uint64_t)destType)};

  Value* newValue = cache.getOrCreate(builder, key, Name);

  return simplifyValue(newValue, DL);
}

Value* createSelectFolder(IRBuilder<>& builder, Value* C, Value* True,
                          Value* False, const Twine& Name) {
#ifdef TESTFOLDER
  if (auto* CConst = dyn_cast<Constant>(C)) {

    if (auto* CBool = dyn_cast<ConstantInt>(CConst)) {
      if (CBool->isOne()) {
        return True;
      } else if (CBool->isZero()) {
        return False;
      }
    }
  }
#endif
  DataLayout DL(builder.GetInsertBlock()->getParent()->getParent());
  return simplifyValue(builder.CreateSelect(C, True, False, Name), DL);
}

Value* createAddFolder(IRBuilder<>& builder, Value* LHS, Value* RHS,  const Twine& Name) {
#ifdef TESTFOLDER3

  if (ConstantInt* LHSConst = dyn_cast<ConstantInt>(LHS)) {
    if (LHSConst->isZero())
      return RHS;
  }
  if (ConstantInt* RHSConst = dyn_cast<ConstantInt>(RHS)) {
    if (RHSConst->isZero())
      return LHS;
  }
#endif

  auto addret = createInstruction(builder, Instruction::Add, LHS, RHS, nullptr, Name);

  auto SQ = GetSimplifyQuery::createSimplifyQuery(builder.GetInsertBlock()->getParent(), dyn_cast<Instruction>(addret));

  KnownBits LHSKB(64);
  getKnownBitsFromContext(LHS, LHSKB, SQ);

  KnownBits RHSKB(64);
  getKnownBitsFromContext(LHS, RHSKB, SQ);

  auto tryCompute = KnownBits::computeForAddSub(1, 0, LHSKB, RHSKB);
  if (tryCompute.isConstant() && !tryCompute.hasConflict())
    return builder.getIntN(LHS->getType()->getIntegerBitWidth(), tryCompute.getConstant().getZExtValue());
  return addret;
}

Value* createSubFolder(IRBuilder<>& builder, Value* LHS, Value* RHS, const Twine& Name) {
#ifdef TESTFOLDER4
  if (ConstantInt* RHSConst = dyn_cast<ConstantInt>(RHS)) {
    if (RHSConst->isZero())
      return LHS;
  }
#endif
  DataLayout DL(builder.GetInsertBlock()->getParent()->getParent());
    
  // sub x, y => add x, -y
  auto subret = createInstruction(builder, Instruction::Add, LHS,  builder.CreateNeg(RHS), nullptr, Name);
  auto SQ = GetSimplifyQuery::createSimplifyQuery(builder.GetInsertBlock()->getParent(), dyn_cast<Instruction>(subret));

  KnownBits LHSKB(64);
  getKnownBitsFromContext(LHS, LHSKB, SQ);

  KnownBits RHSKB(64);
  getKnownBitsFromContext(LHS, RHSKB, SQ);

  auto tryCompute = KnownBits::computeForAddSub(0, 0, LHSKB, RHSKB);
  if (tryCompute.isConstant() && !tryCompute.hasConflict())
    return builder.getIntN(LHS->getType()->getIntegerBitWidth(), tryCompute.getConstant().getZExtValue());

  return subret;
}

Value* foldLShrKnownBits(LLVMContext& context, KnownBits LHS, KnownBits RHS) {

  if (RHS.hasConflict() || LHS.hasConflict() || !RHS.isConstant() ||
      RHS.getBitWidth() > 64 || LHS.isUnknown() || LHS.getBitWidth() <= 1)
    return nullptr;

  APInt shiftAmount = RHS.getConstant();
  unsigned shiftSize = shiftAmount.getZExtValue();

  if (shiftSize >= LHS.getBitWidth())
    return ConstantInt::get(Type::getIntNTy(context, LHS.getBitWidth()), 0);
  ;

  KnownBits result(LHS.getBitWidth());
  result.One = LHS.One.lshr(shiftSize);
  result.Zero = LHS.Zero.lshr(shiftSize) |
                APInt::getHighBitsSet(LHS.getBitWidth(), shiftSize);

  if (!(result.Zero | result.One).isAllOnes()) {
    return nullptr;
  }

  return ConstantInt::get(Type::getIntNTy(context, LHS.getBitWidth()),
                          result.getConstant());
}

Value* foldShlKnownBits(LLVMContext& context, KnownBits LHS, KnownBits RHS) {
  if (RHS.hasConflict() || LHS.hasConflict() || !RHS.isConstant() ||
      RHS.getBitWidth() > 64 || LHS.isUnknown() || LHS.getBitWidth() <= 1)
    return nullptr;

  APInt shiftAmount = RHS.getConstant();
  unsigned shiftSize = shiftAmount.getZExtValue();

  if (shiftSize >= LHS.getBitWidth())
    return ConstantInt::get(Type::getIntNTy(context, LHS.getBitWidth()), 0);

  KnownBits result(LHS.getBitWidth());
  result.One = LHS.One.shl(shiftSize);
  result.Zero = LHS.Zero.shl(shiftSize) |
                APInt::getLowBitsSet(LHS.getBitWidth(), shiftSize);

  if (result.hasConflict() || !result.isConstant()) {
    return nullptr;
  }

  return ConstantInt::get(Type::getIntNTy(context, LHS.getBitWidth()),
                          result.getConstant());
}

Value* createShlFolder(IRBuilder<>& builder, Value* LHS, Value* RHS, const Twine& Name) {

  if (ConstantInt* RHSConst = dyn_cast<ConstantInt>(RHS)) {
    if (ConstantInt* LHSConst = dyn_cast<ConstantInt>(LHS))
      return ConstantInt::get(RHS->getType(), LHSConst->getZExtValue() << RHSConst->getZExtValue());
    if (RHSConst->isZero())
      return LHS;
  }

  auto result = createInstruction(builder, Instruction::Shl, LHS, RHS, nullptr, Name);

  if (auto ctxI = dyn_cast<Instruction>(result)) {
    KnownBits KnownLHS = analyzeValueKnownBits(LHS, ctxI);
    KnownBits KnownRHS = analyzeValueKnownBits(RHS, ctxI);

    if (Value* knownBitsShl =  foldShlKnownBits(builder.getContext(), KnownLHS, KnownRHS)) {
      return knownBitsShl;
    }
  }
  return result;
}

Value* createLShrFolder(IRBuilder<>& builder, Value* LHS, Value* RHS, const Twine& Name) {

#ifdef TESTFOLDERshr

  if (ConstantInt* RHSConst = dyn_cast<ConstantInt>(RHS)) {
    if (ConstantInt* LHSConst = dyn_cast<ConstantInt>(LHS))
      return ConstantInt::get(RHS->getType(), LHSConst->getZExtValue() >> RHSConst->getZExtValue());
    if (RHSConst->isZero())
      return LHS;
  }

  auto result = createInstruction(builder, Instruction::LShr, LHS, RHS, nullptr, Name);
  if (auto ctxI = dyn_cast<Instruction>(result)) {
    KnownBits KnownLHS = analyzeValueKnownBits(LHS, ctxI);
    KnownBits KnownRHS = analyzeValueKnownBits(RHS, ctxI);
    printvalue2(KnownLHS);
    printvalue2(KnownRHS);
    if (Value* knownBitsLshr = foldLShrKnownBits(builder.getContext(), KnownLHS, KnownRHS)) {
      // printvalue(knownBitsLshr)
      return knownBitsLshr;
    }

    KnownBits KnownInst = analyzeValueKnownBits(result, ctxI);
    printvalue2(KnownInst);
  }

#endif

  return result;
}

Value* createShlFolder(IRBuilder<>& builder, Value* LHS, uint64_t RHS, const Twine& Name) {
  return createShlFolder(builder, LHS, ConstantInt::get(LHS->getType(), RHS), Name);
}

Value* createShlFolder(IRBuilder<>& builder, Value* LHS, APInt RHS, const Twine& Name) {
  return createShlFolder(builder, LHS, ConstantInt::get(LHS->getType(), RHS), Name);
}

Value* createLShrFolder(IRBuilder<>& builder, Value* LHS, uint64_t RHS, const Twine& Name) {
  return createLShrFolder(builder, LHS, ConstantInt::get(LHS->getType(), RHS), Name);
}
Value* createLShrFolder(IRBuilder<>& builder, Value* LHS, APInt RHS, const Twine& Name) {
  return createLShrFolder(builder, LHS, ConstantInt::get(LHS->getType(), RHS), Name);
}

Value* foldOrKnownBits(LLVMContext& context, KnownBits LHS, KnownBits RHS) {

  if (RHS.hasConflict() || LHS.hasConflict() || LHS.isUnknown() ||
      RHS.isUnknown() || LHS.getBitWidth() != RHS.getBitWidth() ||
      !RHS.isConstant() || LHS.getBitWidth() <= 1 || RHS.getBitWidth() < 1 ||
      RHS.getBitWidth() > 64 || LHS.getBitWidth() > 64) {
    return nullptr;
  }

  KnownBits combined;
  combined.One = LHS.One | RHS.One;
  combined.Zero = LHS.Zero & RHS.Zero;

  if (!combined.isConstant() || combined.hasConflict()) {
    return nullptr;
  }

  APInt resultValue = combined.One;
  return ConstantInt::get(Type::getIntNTy(context, combined.getBitWidth()),
                          resultValue);
}

Value* createOrFolder(IRBuilder<>& builder, Value* LHS, Value* RHS, const Twine& Name) {
#ifdef TESTFOLDER5

  if (ConstantInt* LHSConst = dyn_cast<ConstantInt>(LHS)) {
    if (ConstantInt* RHSConst = dyn_cast<ConstantInt>(RHS))
      return ConstantInt::get(RHS->getType(), RHSConst->getZExtValue() | LHSConst->getZExtValue());
    if (LHSConst->isZero())
      return RHS;
  }
  if (ConstantInt* RHSConst = dyn_cast<ConstantInt>(RHS)) {
    if (RHSConst->isZero())
      return LHS;
  }

  auto result = createInstruction(builder, Instruction::Or, LHS, RHS, nullptr, Name);
  if (auto ctxI = dyn_cast<Instruction>(result)) {
    KnownBits KnownLHS = analyzeValueKnownBits(LHS, ctxI);
    KnownBits KnownRHS = analyzeValueKnownBits(RHS, ctxI);
    printvalue2(KnownLHS) printvalue2(KnownRHS);
    if (Value* knownBitsAnd = foldOrKnownBits(builder.getContext(), KnownLHS, KnownRHS)) {
      return knownBitsAnd;
    }
    if (Value* knownBitsAnd = foldOrKnownBits(builder.getContext(), KnownRHS, KnownLHS)) {
      return knownBitsAnd;
    }
  }
#endif

  return result;
}

Value* foldXorKnownBits(LLVMContext& context, KnownBits LHS, KnownBits RHS) {

  if (RHS.hasConflict() || LHS.hasConflict() || LHS.isUnknown() ||
      RHS.isUnknown() || !RHS.isConstant() ||
      LHS.getBitWidth() != RHS.getBitWidth() || RHS.getBitWidth() <= 1 ||
      LHS.getBitWidth() <= 1 || RHS.getBitWidth() > 64 ||
      LHS.getBitWidth() > 64) {
    return nullptr;
  }

  if (!((LHS.Zero | LHS.One) & RHS.One).eq(RHS.One)) {
    return nullptr;
  }
  APInt resultValue = LHS.One ^ RHS.One;

  return ConstantInt::get(Type::getIntNTy(context, LHS.getBitWidth()),
                          resultValue);
}

Value* createXorFolder(IRBuilder<>& builder, Value* LHS, Value* RHS,
                       const Twine& Name) {
#ifdef TESTFOLDER6

  if (LHS == RHS) {
    return ConstantInt::get(LHS->getType(), 0);
  }

  if (ConstantInt* LHSConst = dyn_cast<ConstantInt>(LHS)) {
    if (ConstantInt* RHSConst = dyn_cast<ConstantInt>(RHS))
      return ConstantInt::get(RHS->getType(), RHSConst->getZExtValue() ^
                                                  LHSConst->getZExtValue());
    if (LHSConst->isZero())
      return RHS;
  }
  if (ConstantInt* RHSConst = dyn_cast<ConstantInt>(RHS)) {
    if (RHSConst->isZero())
      return LHS;
  }

#endif

  auto result = createInstruction(builder, Instruction::Xor, LHS, RHS, nullptr, Name);
  if (auto ctxI = dyn_cast<Instruction>(result)) {
    KnownBits KnownLHS = analyzeValueKnownBits(LHS, ctxI);
    KnownBits KnownRHS = analyzeValueKnownBits(RHS, ctxI);

    if (auto V = foldXorKnownBits(builder.getContext(), KnownLHS, KnownRHS))
      return V;
  }
  if (auto simplifiedByPM = doPatternMatching(Instruction::Xor, LHS, RHS))
    return simplifiedByPM;

  return result;
}

std::optional<bool> foldKnownBits(CmpInst::Predicate P, KnownBits LHS, KnownBits RHS) {

  switch (P) {
  case CmpInst::ICMP_EQ:
    return KnownBits::eq(LHS, RHS);
  case CmpInst::ICMP_NE:
    return KnownBits::ne(LHS, RHS);
  case CmpInst::ICMP_UGT:
    return KnownBits::ugt(LHS, RHS);
  case CmpInst::ICMP_UGE:
    return KnownBits::uge(LHS, RHS);
  case CmpInst::ICMP_ULT:
    return KnownBits::ult(LHS, RHS);
  case CmpInst::ICMP_ULE:
    return KnownBits::ule(LHS, RHS);
  case CmpInst::ICMP_SGT:
    return KnownBits::sgt(LHS, RHS);
  case CmpInst::ICMP_SGE:
    return KnownBits::sge(LHS, RHS);
  case CmpInst::ICMP_SLT:
    return KnownBits::slt(LHS, RHS);
  case CmpInst::ICMP_SLE:
    return KnownBits::sle(LHS, RHS);
  default:
    return nullopt;
  }

  return nullopt;
}

Value* ICMPPatternMatcher(IRBuilder<>& builder, CmpInst::Predicate P, Value* LHS, Value* RHS, const Twine& Name) {
  switch (P) {
  case CmpInst::ICMP_UGT: {
    // Check if LHS is the result of a call to @llvm.ctpop.i64
    if (match(RHS, m_SpecificInt(64))) {
      // Check if LHS is `and i64 %neg, 255`
      Value* Neg = nullptr;
      if (match(LHS, m_And(m_Value(Neg), m_SpecificInt(255)))) {
        // Check if `neg` is `sub nsw i64 0, %125`
        Value* CtpopResult = nullptr;
        if (match(Neg, m_Sub(m_Zero(), m_Value(CtpopResult)))) {
          // Check if `%125` is a call to `llvm.ctpop.i64`
          if (auto* CI = dyn_cast<CallInst>(CtpopResult)) {
            if (CI->getCalledFunction() &&
                CI->getCalledFunction()->getIntrinsicID() == Intrinsic::ctpop) {
              Value* R8 = CI->getArgOperand(0);
              // Replace with: %isIndexInBound = icmp ne i64 %r8, 0
              auto* isIndexInBound =
                  builder.CreateICmpNE(R8, builder.getInt64(0), Name);
              return isIndexInBound;
            }
          }
        }
      }
    }
    break;
  }
  default: {
    break;
  }
  }
  // c = add a, b
  // cmp x, c, 0
  // =>
  // cmp x, a, -b

  // lhs is a bin op
  if (auto* AddInst = dyn_cast<BinaryOperator>(LHS)) {
    auto binOpcode = AddInst->getOpcode();
    if (binOpcode == Instruction::Add || binOpcode == Instruction::Sub) {
      Value* A = AddInst->getOperand(0);
      Value* B = AddInst->getOperand(1);
      if (binOpcode == Instruction::Add)
        B = builder.CreateNeg(B);
      Value* NewB = builder.CreateAdd(RHS, B);
      return builder.CreateICmp(P, A, NewB, Name);
    }
  }
  return nullptr;
}

Value* createICMPFolder(IRBuilder<>& builder, CmpInst::Predicate P, Value* LHS, Value* RHS, const Twine& Name) {

  // Assicurati che LHS e RHS abbiano lo stesso tipo
  Type* LHSTy = LHS->getType();
  Type* RHSTy = RHS->getType();
  if (LHSTy != RHSTy) {
    // Scegli il tipo pi� grande
    Type* DestTy = LHSTy->getIntegerBitWidth() > RHSTy->getIntegerBitWidth()
                       ? LHSTy
                       : RHSTy;
    LHS = createZExtOrTruncFolder(builder, LHS, DestTy, "lhs_ext");
    RHS = createZExtOrTruncFolder(builder, RHS, DestTy, "rhs_ext");
  }
  auto result = builder.CreateICmp(P, LHS, RHS, Name);

  if (auto ctxI = dyn_cast<Instruction>(result)) {
         
    KnownBits KnownLHS = analyzeValueKnownBits(LHS, ctxI);
    KnownBits KnownRHS = analyzeValueKnownBits(RHS, ctxI);

    if (std::optional<bool> v = foldKnownBits(P, KnownLHS, KnownRHS)) {
      return ConstantInt::get(Type::getInt1Ty(builder.getContext()), v.value());
    }
    printvalue2(KnownLHS) printvalue2(KnownRHS);
  }
  
  if (auto patternCheck = ICMPPatternMatcher(builder, P, LHS, RHS, Name)) {
    printvalue(patternCheck);
    return patternCheck;
  }

  return result;
}

Value* foldAndKnownBits(LLVMContext& context, KnownBits LHS, KnownBits RHS) {
  if (RHS.hasConflict() || LHS.hasConflict() || LHS.isUnknown() ||
      RHS.isUnknown() || !RHS.isConstant() ||
      LHS.getBitWidth() != RHS.getBitWidth() || RHS.getBitWidth() <= 1 ||
      LHS.getBitWidth() <= 1 || RHS.getBitWidth() > 64 ||
      LHS.getBitWidth() > 64) {
    return nullptr;
  }

  if (!((LHS.Zero | LHS.One) & RHS.One).eq(RHS.One)) {
    return nullptr;
  }
  APInt resultValue = LHS.One & RHS.One;

  return ConstantInt::get(Type::getIntNTy(context, LHS.getBitWidth()),
                          resultValue);
}

Value* createAndFolder(IRBuilder<>& builder, Value* LHS, Value* RHS, const Twine& Name) {
#ifdef TESTFOLDER
  if (ConstantInt* LHSConst = dyn_cast<ConstantInt>(LHS)) {
    if (ConstantInt* RHSConst = dyn_cast<ConstantInt>(RHS))
      return ConstantInt::get(RHS->getType(), RHSConst->getZExtValue() & LHSConst->getZExtValue());
    if (LHSConst->isZero())
      return ConstantInt::get(RHS->getType(), 0);
  }
  if (ConstantInt* RHSConst = dyn_cast<ConstantInt>(RHS)) {
    if (RHSConst->isZero())
      return ConstantInt::get(LHS->getType(), 0);
  }
  if (ConstantInt* LHSConst = dyn_cast<ConstantInt>(LHS)) {
    if (LHSConst->isMinusOne())
      return RHS;
  }
  if (ConstantInt* RHSConst = dyn_cast<ConstantInt>(RHS)) {
    if (RHSConst->isMinusOne())
      return LHS;
  }

  // Assicurati che LHS e RHS abbiano lo stesso tipo
  Type* LHSTy = LHS->getType();
  Type* RHSTy = RHS->getType();
  if (LHSTy != RHSTy) {
    // Scegli il tipo pi� grande
    Type* DestTy = LHSTy->getIntegerBitWidth() > RHSTy->getIntegerBitWidth()
                       ? LHSTy
                       : RHSTy;
    LHS = createZExtOrTruncFolder(builder, LHS, DestTy, "lhs_ext");
    RHS = createZExtOrTruncFolder(builder, RHS, DestTy, "rhs_ext");
  }

  auto result =  createInstruction(builder, Instruction::And, LHS, RHS, nullptr, Name);
  if (auto ctxI = dyn_cast<Instruction>(result)) {
    DataLayout DL(builder.GetInsertBlock()->getParent()->getParent());
    KnownBits KnownLHS = analyzeValueKnownBits(LHS, ctxI);
    KnownBits KnownRHS = analyzeValueKnownBits(RHS, ctxI);

    if (Value* knownBitsAnd = foldAndKnownBits(builder.getContext(), KnownLHS, KnownRHS)) {
      printvalue(knownBitsAnd);
      return knownBitsAnd;
    }
    if (Value* knownBitsAnd = foldAndKnownBits(builder.getContext(), KnownRHS, KnownLHS)) {
      printvalue(knownBitsAnd);
      return knownBitsAnd;
    }
  }

#endif
  if (auto sillyResult = doPatternMatching(Instruction::And, LHS, RHS)) {
    printvalue(sillyResult);
    return sillyResult;
  }
  return result;
}

// - probably not needed anymore
Value* createTruncFolder(IRBuilder<>& builder, Value* V, Type* DestTy, const Twine& Name) {
  Value* result = builder.CreateTrunc(V, DestTy, Name);

  DataLayout DL(builder.GetInsertBlock()->getParent()->getParent());
  if (auto ctxI = dyn_cast<Instruction>(result)) {

    KnownBits KnownTruncResult = analyzeValueKnownBits(result, ctxI);
    printvalue2(KnownTruncResult);
    if (!KnownTruncResult.hasConflict() && KnownTruncResult.getBitWidth() > 1 &&
        KnownTruncResult.isConstant())
      return ConstantInt::get(DestTy, KnownTruncResult.getConstant());
  }
  // TODO: CREATE A MAP FOR AVAILABLE TRUNCs/ZEXTs/SEXTs
  // WHY?
  // IF %y = trunc %x exists
  // we dont want to create %y2 = trunc %x
  // just use %y
  // so xor %y, %y2 => %y, %y => 0

  return simplifyValue(result, DL);
}

Value* createZExtFolder(IRBuilder<>& builder, Value* V, Type* DestTy, const Twine& Name) {
  auto result = builder.CreateZExt(V, DestTy, Name);
  DataLayout DL(builder.GetInsertBlock()->getParent()->getParent());
#ifdef TESTFOLDER8
  if (auto ctxI = dyn_cast<Instruction>(result)) {
    KnownBits KnownRHS = analyzeValueKnownBits(result, ctxI);
    if (!KnownRHS.hasConflict() && KnownRHS.getBitWidth() > 1 &&
        KnownRHS.isConstant())
      return ConstantInt::get(DestTy, KnownRHS.getConstant());
  }
#endif
  return simplifyValue(result, DL);
}

Value* createZExtOrTruncFolder(IRBuilder<>& builder, Value* V, Type* DestTy, const Twine& Name) {
  Type* VTy = V->getType();
  if (VTy->getScalarSizeInBits() < DestTy->getScalarSizeInBits()) {
    auto newValue = createZExtFolder(builder, V, DestTy, Name);
    printvalue(newValue);
    return newValue;
  }
  if (VTy->getScalarSizeInBits() > DestTy->getScalarSizeInBits()) {
    auto newValue = createTruncFolder(builder, V, DestTy, Name);
    printvalue(newValue);
    return newValue;
  }
  return V;
}

Value* createSExtFolder(IRBuilder<>& builder, Value* V, Type* DestTy, const Twine& Name) {
#ifdef TESTFOLDER9

  if (V->getType() == DestTy) {
    return V;
  }

  if (auto* TruncInsts = dyn_cast<TruncInst>(V)) {
    Value* OriginalValue = TruncInsts->getOperand(0);
    Type* OriginalType = OriginalValue->getType();

    if (OriginalType == DestTy) {
      return OriginalValue;
    }
  }

  if (auto* ConstInt = dyn_cast<ConstantInt>(V)) {
    return ConstantInt::get(
        DestTy, ConstInt->getValue().sextOrTrunc(DestTy->getIntegerBitWidth()));
  }

  if (auto* SExtInsts = dyn_cast<SExtInst>(V)) {
    return builder.CreateSExt(SExtInsts->getOperand(0), DestTy, Name);
  }
#endif

  return builder.CreateSExt(V, DestTy, Name);
}

Value* createSExtOrTruncFolder(IRBuilder<>& builder, Value* V, Type* DestTy, const Twine& Name) {
  Type* VTy = V->getType();
  if (VTy->getScalarSizeInBits() < DestTy->getScalarSizeInBits())
    return createSExtFolder(builder, V, DestTy, Name);
  if (VTy->getScalarSizeInBits() > DestTy->getScalarSizeInBits())
    return createTruncFolder(builder, V, DestTy, Name);
  return V;
}

/*
%extendedValue13 = zext i8 %trunc11 to i64
%maskedreg14 = and i64 %newreg9, -256
*/

void lifterClass::Init_Flags() {
  LLVMContext& context = builder.getContext();
  auto zero = ConstantInt::getSigned(Type::getInt1Ty(context), 0);
  auto one = ConstantInt::getSigned(Type::getInt1Ty(context), 1);

  FlagList[FLAG_CF] = zero;
  FlagList[FLAG_PF] = zero;
  FlagList[FLAG_AF] = zero;
  FlagList[FLAG_ZF] = zero;
  FlagList[FLAG_SF] = zero;
  FlagList[FLAG_TF] = zero;
  FlagList[FLAG_IF] = zero;
  FlagList[FLAG_DF] = zero;
  FlagList[FLAG_OF] = zero;

  FlagList[FLAG_RESERVED1] = one;
}

// ???
Value* lifterClass::setFlag(Flag flag, Value* newValue) {
  LLVMContext& context = builder.getContext();
  newValue = createTruncFolder(builder, newValue, Type::getInt1Ty(context));
  printvalue2((int32_t)flag) printvalue(newValue);
  if (flag == FLAG_RESERVED1 || flag == FLAG_RESERVED5 || flag == FLAG_IF ||
      flag == FLAG_DF)
    return nullptr;

  return FlagList[flag] = newValue;
}
Value* lifterClass::getFlag(Flag flag) {
  if (FlagList[flag])
    return FlagList[flag];

  LLVMContext& context = builder.getContext();
  return ConstantInt::getSigned(Type::getInt1Ty(context), 0);
}

// for love of god this is so ugly
RegisterMap lifterClass::getRegisters() { return Registers; }
void lifterClass::setRegisters(RegisterMap newRegisters) {
  Registers = newRegisters;
}

Value* memoryAlloc;
Value* TEB;
void initMemoryAlloc(Value* allocArg) { memoryAlloc = allocArg; }
Value* getMemory() { return memoryAlloc; }

// todo?
ReverseRegisterMap lifterClass::flipRegisterMap() {
  ReverseRegisterMap RevMap;
  for (const auto& pair : Registers) {
    RevMap[pair.second] = pair.first;
  }
  /*for (const auto& pair : FlagList) {
          RevMap[pair.second] = pair.first;
  }*/
  return RevMap;
}

RegisterMap lifterClass::InitRegisters(Function* function, ZyanU64 rip) {

  // rsp
  // rsp_unaligned = %rsp % 16
  // rsp_aligned_to16 = rsp - rsp_unaligned
  int zydisRegister = ZYDIS_REGISTER_RAX;
  int xmmRegister = ZYDIS_REGISTER_XMM0;

  auto argEnd = function->arg_end();
  for (auto argIt = function->arg_begin(); argIt != argEnd; ++argIt) {

    Argument* arg = &*argIt;
    if (zydisRegister <= ZYDIS_REGISTER_R15) {
      arg->setName(ZydisRegisterGetString((ZydisRegister)zydisRegister));
      Registers[(ZydisRegister)zydisRegister] = arg;
      zydisRegister++;
    } else if (xmmRegister <= ZYDIS_REGISTER_XMM15) {
      arg->setName(ZydisRegisterGetString((ZydisRegister)xmmRegister));
      Registers[(ZydisRegister)xmmRegister] = arg;
      xmmRegister++;
    } else if (std::next(argIt) == argEnd) {
      arg->setName("memory");
      memoryAlloc = arg;
    } else if (std::next(argIt, 2) == argEnd) {
      arg->setName("TEB");
      TEB = arg;
    }
  }

  Init_Flags();

  LLVMContext& context = builder.getContext();

  auto zero = ConstantInt::getSigned(Type::getInt64Ty(context), 0);

  auto value =
      cast<Value>(ConstantInt::getSigned(Type::getInt64Ty(context), rip));

  auto new_rip = createAddFolder(builder, zero, value);

  Registers[ZYDIS_REGISTER_RIP] = new_rip;

  auto stackvalue = cast<Value>(
      ConstantInt::getSigned(Type::getInt64Ty(context), STACKP_VALUE));
  auto new_stack_pointer = createAddFolder(builder, stackvalue, zero);

  Registers[ZYDIS_REGISTER_RSP] = new_stack_pointer;

  return Registers;
}

Value* lifterClass::GetValueFromHighByteRegister(int reg) {

  Value* fullRegisterValue = Registers[ZydisRegisterGetLargestEnclosing(
      ZYDIS_MACHINE_MODE_LONG_64, (ZydisRegister)reg)];

  Value* shiftedValue =
      createLShrFolder(builder, fullRegisterValue, 8, "highreg");

  Value* FF = ConstantInt::get(shiftedValue->getType(), 0xff);
  Value* highByteValue = createAndFolder(builder, shiftedValue, FF, "highByte");

  return highByteValue;
}

void lifterClass::SetRFLAGSValue(Value* value) {
  LLVMContext& context = builder.getContext();
  for (int flag = FLAG_CF; flag < FLAGS_END; flag++) {
    int shiftAmount = flag;
    Value* shiftedFlagValue = createLShrFolder(
        builder, value, ConstantInt::get(value->getType(), shiftAmount),
        "setflag");
    auto flagValue = createTruncFolder(builder, shiftedFlagValue,
                                       Type::getInt1Ty(context), "flagtrunc");

    setFlag((Flag)flag, flagValue);
  }
  return;
}

Value* lifterClass::GetRFLAGSValue() {
  LLVMContext& context = builder.getContext();
  Value* rflags = ConstantInt::get(Type::getInt64Ty(context), 0);
  for (int flag = FLAG_CF; flag < FLAGS_END; flag++) {
    Value* flagValue = getFlag((Flag)flag);
    int shiftAmount = flag;
    Value* shiftedFlagValue = createShlFolder(
        builder,
        createZExtFolder(builder, flagValue, Type::getInt64Ty(context),
                         "createrflag1"),
        ConstantInt::get(Type::getInt64Ty(context), shiftAmount),
        "createrflag2");
    rflags = createOrFolder(builder, rflags, shiftedFlagValue, "creatingrflag");
  }
  return rflags;
}

Value* lifterClass::GetRegisterValue(int key) {

  if (key == ZYDIS_REGISTER_AH || key == ZYDIS_REGISTER_CH ||
      key == ZYDIS_REGISTER_DH || key == ZYDIS_REGISTER_BH) {
    return GetValueFromHighByteRegister(key);
  }

  int newKey = (key != ZYDIS_REGISTER_RFLAGS) && (key != ZYDIS_REGISTER_RIP)
                   ? ZydisRegisterGetLargestEnclosing(
                         ZYDIS_MACHINE_MODE_LONG_64, (ZydisRegister)key)
                   : key;

  if (key == ZYDIS_REGISTER_RFLAGS || key == ZYDIS_REGISTER_EFLAGS) {
    return GetRFLAGSValue();
  }

  if (key >= ZYDIS_REGISTER_XMM0 && key <= ZYDIS_REGISTER_XMM15) {
    return Registers[key];
  }

  /*
  if (Registers.find(newKey) == Registers.end()) {
          throw std::runtime_error("register not found"); exit(-1);
  }
  */

  return Registers[newKey];
}

Value* lifterClass::SetValueToHighByteRegister(int reg, Value* value) {
  LLVMContext& context = builder.getContext();
  int shiftValue = 8;

  int fullRegKey = ZydisRegisterGetLargestEnclosing(ZYDIS_MACHINE_MODE_LONG_64,
                                                    (ZydisRegister)reg);
  Value* fullRegisterValue = Registers[fullRegKey];

  Value* eightBitValue = createAndFolder(
      builder, value, ConstantInt::get(value->getType(), 0xFF), "eight-bit");
  Value* shiftedValue =
      createShlFolder(builder, eightBitValue,
                      ConstantInt::get(value->getType(), shiftValue), "shl");

  Value* mask =
      ConstantInt::get(Type::getInt64Ty(context), ~(0xFF << shiftValue));
  Value* clearedRegister =
      createAndFolder(builder, fullRegisterValue, mask, "clear-reg");

  shiftedValue =
      createZExtFolder(builder, shiftedValue, fullRegisterValue->getType());

  Value* newRegisterValue =
      createOrFolder(builder, clearedRegister, shiftedValue, "high_byte");

  return newRegisterValue;
}

Value* lifterClass::SetValueToSubRegister_8b(int reg, Value* value) {
  LLVMContext& context = builder.getContext();
  int fullRegKey = ZydisRegisterGetLargestEnclosing(
      ZYDIS_MACHINE_MODE_LONG_64, static_cast<ZydisRegister>(reg));
  Value* fullRegisterValue = Registers[fullRegKey];
  fullRegisterValue = createZExtOrTruncFolder(builder, fullRegisterValue,
                                              Type::getInt64Ty(context));

  uint64_t mask = 0xFFFFFFFFFFFFFFFFULL;
  if (reg == ZYDIS_REGISTER_AH || reg == ZYDIS_REGISTER_CH ||
      reg == ZYDIS_REGISTER_DH || reg == ZYDIS_REGISTER_BH) {
    mask = 0xFFFFFFFFFFFF00FFULL;
  } else {
    mask = 0xFFFFFFFFFFFFFF00ULL;
  }

  Value* maskValue = ConstantInt::get(Type::getInt64Ty(context), mask);
  Value* extendedValue = createZExtFolder(
      builder, value, Type::getInt64Ty(context), "extendedValue");

  Value* maskedFullReg =
      createAndFolder(builder, fullRegisterValue, maskValue, "maskedreg");

  if (reg == ZYDIS_REGISTER_AH || reg == ZYDIS_REGISTER_CH ||
      reg == ZYDIS_REGISTER_DH || reg == ZYDIS_REGISTER_BH) {
    extendedValue = createShlFolder(builder, extendedValue, 8, "shiftedValue");
  }

  Value* updatedReg =
      createOrFolder(builder, maskedFullReg, extendedValue, "newreg");

  printvalue(fullRegisterValue) printvalue(maskValue) printvalue(maskedFullReg)
      printvalue(extendedValue) printvalue(updatedReg);

  Registers[fullRegKey] = updatedReg;

  return updatedReg;
}

Value* lifterClass::SetValueToSubRegister_16b(int reg, Value* value) {

  int fullRegKey = ZydisRegisterGetLargestEnclosing(ZYDIS_MACHINE_MODE_LONG_64,
                                                    (ZydisRegister)reg);
  Value* fullRegisterValue = Registers[fullRegKey];

  Value* last4cleared =
      ConstantInt::get(fullRegisterValue->getType(), 0xFFFFFFFFFFFF0000);
  Value* maskedFullReg =
      createAndFolder(builder, fullRegisterValue, last4cleared, "maskedreg");
  value = createZExtFolder(builder, value, fullRegisterValue->getType());

  Value* updatedReg = createOrFolder(builder, maskedFullReg, value, "newreg");
  printvalue(updatedReg);
  return updatedReg;
}

void lifterClass::SetRegisterValue(int key, Value* value) {
  if ((key == ZYDIS_REGISTER_AH || key == ZYDIS_REGISTER_CH ||
       key == ZYDIS_REGISTER_DH || key == ZYDIS_REGISTER_BH)) {

    value = SetValueToSubRegister_8b(key, value);
  }

  if (((key >= ZYDIS_REGISTER_R8B) && (key <= ZYDIS_REGISTER_R15B)) ||
      ((key >= ZYDIS_REGISTER_AL) && (key <= ZYDIS_REGISTER_BL)) ||
      ((key >= ZYDIS_REGISTER_SPL) && (key <= ZYDIS_REGISTER_DIL))) {

    value = SetValueToSubRegister_8b(key, value);
  }

  if (((key >= ZYDIS_REGISTER_AX) && (key <= ZYDIS_REGISTER_R15W))) {
    value = SetValueToSubRegister_16b(key, value);
  }

  if (key == ZYDIS_REGISTER_RFLAGS) {
    SetRFLAGSValue(value);
    return;
  }

  if (key >= ZYDIS_REGISTER_XMM0 && key <= ZYDIS_REGISTER_XMM15) {
    Registers[key] = value;
    return;
  }

  int newKey = (key != ZYDIS_REGISTER_RFLAGS) && (key != ZYDIS_REGISTER_RIP)
                   ? ZydisRegisterGetLargestEnclosing(
                         ZYDIS_MACHINE_MODE_LONG_64, (ZydisRegister)key)
                   : key;

  Registers[newKey] = value;
}

Value* lifterClass::GetEffectiveAddress(ZydisDecodedOperand& op,
                                        int possiblesize) {
  LLVMContext& context = builder.getContext();

  Value* effectiveAddress = nullptr;

  Value* baseValue = nullptr;
  if (op.mem.base != ZYDIS_REGISTER_NONE) {
    baseValue = GetRegisterValue(op.mem.base);
    baseValue = createZExtFolder(builder, baseValue, Type::getInt64Ty(context));
    printvalue(baseValue);
  }

  Value* indexValue = nullptr;
  if (op.mem.index != ZYDIS_REGISTER_NONE) {
    indexValue = GetRegisterValue(op.mem.index);

    indexValue =
        createZExtFolder(builder, indexValue, Type::getInt64Ty(context));
    printvalue(indexValue);
    if (op.mem.scale > 1) {
      Value* scaleValue =
          ConstantInt::get(Type::getInt64Ty(context), op.mem.scale);
      indexValue = builder.CreateMul(indexValue, scaleValue, "mul_ea");
    }
  }

  if (baseValue && indexValue) {
    effectiveAddress = createAddFolder(builder, baseValue, indexValue,
                                       "bvalue_indexvalue_set");
  } else if (baseValue) {
    effectiveAddress = baseValue;
  } else if (indexValue) {
    effectiveAddress = indexValue;
  } else {
    effectiveAddress = ConstantInt::get(Type::getInt64Ty(context), 0);
  }

  if (op.mem.disp.value) {
    Value* dispValue =
        ConstantInt::get(Type::getInt64Ty(context), op.mem.disp.value);
    effectiveAddress =
        createAddFolder(builder, effectiveAddress, dispValue, "disp_set");
  }
  printvalue(effectiveAddress);
  return createZExtOrTruncFolder(builder, effectiveAddress,
                                 Type::getIntNTy(context, possiblesize));
}

Value* ConvertIntToPTR(IRBuilder<>& builder, Value* effectiveAddress) {

  LLVMContext& context = builder.getContext();
  std::vector<Value*> indices;
  indices.push_back(effectiveAddress);

  auto memoryOperand = memoryAlloc;
  //
  // if (segment == ZYDIS_REGISTER_GS)
  //     memoryOperand = TEB;

  Value* pointer =
      builder.CreateGEP(Type::getInt8Ty(context), memoryOperand, indices);
  return pointer;
}

Value* lifterClass::GetOperandValue(ZydisDecodedOperand& op, int possiblesize, string address) {
  LLVMContext& context = builder.getContext();
  auto type = Type::getIntNTy(context, possiblesize);

  switch (op.type) {
  case ZYDIS_OPERAND_TYPE_REGISTER: {
    Value* value = GetRegisterValue(op.reg.value);
    auto vtype = value->getType();
    if (isa<IntegerType>(vtype)) {
      auto typeBitWidth = dyn_cast<IntegerType>(vtype)->getBitWidth();
      if (typeBitWidth < 128)
        value = createZExtOrTruncFolder(builder, value, type, "trunc");
    }
    return value;
  }
  case ZYDIS_OPERAND_TYPE_IMMEDIATE: {
    ConstantInt* val;
    if (op.imm.is_signed) {
      val = ConstantInt::getSigned(type, op.imm.value.s);
    } else {
      val = ConstantInt::get(context, APInt(possiblesize, op.imm.value.u)); // ?
    }
    return val;
  }
  case ZYDIS_OPERAND_TYPE_MEMORY: {

    Value* effectiveAddress = nullptr;

    Value* baseValue = nullptr;
    if (op.mem.base != ZYDIS_REGISTER_NONE) {
      baseValue = GetRegisterValue(op.mem.base);
      baseValue = createZExtFolder(builder, baseValue, Type::getInt64Ty(context));
      printvalue(baseValue);
    }

    Value* indexValue = nullptr;
    if (op.mem.index != ZYDIS_REGISTER_NONE) {
      indexValue = GetRegisterValue(op.mem.index);
      indexValue = createZExtFolder(builder, indexValue, Type::getInt64Ty(context));
      printvalue(indexValue);
      if (op.mem.scale > 1) {
        Value* scaleValue = ConstantInt::get(Type::getInt64Ty(context), op.mem.scale);
        indexValue = builder.CreateMul(indexValue, scaleValue);
      }
    }

    if (baseValue && indexValue) {
      effectiveAddress = createAddFolder(builder, baseValue, indexValue, "bvalue_indexvalue");
    } else if (baseValue) {
      effectiveAddress = baseValue;
    } else if (indexValue) {
      effectiveAddress = indexValue;
    } else {
      effectiveAddress = ConstantInt::get(Type::getInt64Ty(context), 0);
    }

    if (op.mem.disp.has_displacement) {
      Value* dispValue = ConstantInt::get(Type::getInt64Ty(context), (int)(op.mem.disp.value));
      effectiveAddress = createAddFolder(builder, effectiveAddress, dispValue, "memory_addr");
    }
    printvalue(effectiveAddress);

    Type* loadType = Type::getIntNTy(context, possiblesize);

    std::vector<Value*> indices;
    indices.push_back(effectiveAddress);
    Value* memoryOperand = memoryAlloc;
    if (op.mem.segment == ZYDIS_REGISTER_GS)
      memoryOperand = TEB;

    Value* pointer = builder.CreateGEP(Type::getInt8Ty(context), memoryOperand, indices, "GEPLoadxd-" + address + "-");

    auto retval = builder.CreateLoad(loadType, pointer, "Loadxd-" + address + "-");

    GEPStoreTracker::loadMemoryOp(retval);

    if (isa<ConstantInt>(effectiveAddress)) {
      ConstantInt* effectiveAddressInt = dyn_cast<ConstantInt>(effectiveAddress);

      if (!effectiveAddressInt)
        return nullptr;

      uint64_t addr = effectiveAddressInt->getZExtValue();
      unsigned byteSize = loadType->getIntegerBitWidth() / 8;

      // Qui usiamo la nostra nuova funzionalit� di concretizzazione
      if (memoryConcretization.hasConcretization(addr)) {
        uint64_t concreteValue = memoryConcretization.getConcretization(addr);
        APInt value(byteSize * 8, concreteValue);
        Constant* newVal = ConstantInt::get(loadType, value);
        printvalue(newVal);
        return newVal;
      }
    }

    Value* solvedLoad = GEPStoreTracker::solveLoad(retval);
    if (solvedLoad) {
      return solvedLoad;
    }

    pointer = simplifyValue(pointer, builder.GetInsertBlock()->getParent()->getParent()->getDataLayout());

    printvalue(retval);

    return retval;
  }
  default: {
    throw std::runtime_error("operand type not implemented");
    exit(-1);
  }
  }
}

Value* lifterClass::SetOperandValue(ZydisDecodedOperand& op, Value* value,
                                    string address) {
  LLVMContext& context = builder.getContext();
  value = simplifyValue(
      value,
      builder.GetInsertBlock()->getParent()->getParent()->getDataLayout());

  switch (op.type) {
  case ZYDIS_OPERAND_TYPE_REGISTER: {
    SetRegisterValue(op.reg.value, value);
    return value;
    break;
  }
  case ZYDIS_OPERAND_TYPE_MEMORY: {

    Value* effectiveAddress = nullptr;

    Value* baseValue = nullptr;
    if (op.mem.base != ZYDIS_REGISTER_NONE) {
      baseValue = GetRegisterValue(op.mem.base);
      baseValue =
          createZExtFolder(builder, baseValue, Type::getInt64Ty(context));
      printvalue(baseValue);
    }

    Value* indexValue = nullptr;
    if (op.mem.index != ZYDIS_REGISTER_NONE) {
      indexValue = GetRegisterValue(op.mem.index);
      indexValue =
          createZExtFolder(builder, indexValue, Type::getInt64Ty(context));
      printvalue(indexValue);
      if (op.mem.scale > 1) {
        Value* scaleValue =
            ConstantInt::get(Type::getInt64Ty(context), op.mem.scale);
        indexValue = builder.CreateMul(indexValue, scaleValue, "mul_ea");
      }
    }

    if (baseValue && indexValue) {
      effectiveAddress = createAddFolder(builder, baseValue, indexValue,
                                         "bvalue_indexvalue_set");
    } else if (baseValue) {
      effectiveAddress = baseValue;
    } else if (indexValue) {
      effectiveAddress = indexValue;
    } else {
      effectiveAddress = ConstantInt::get(Type::getInt64Ty(context), 0);
    }

    if (op.mem.disp.value) {
      Value* dispValue =
          ConstantInt::get(Type::getInt64Ty(context), op.mem.disp.value);
      effectiveAddress =
          createAddFolder(builder, effectiveAddress, dispValue, "disp_set");
    }

    std::vector<Value*> indices;
    indices.push_back(effectiveAddress);

    auto memoryOperand = memoryAlloc;
    if (op.mem.segment == ZYDIS_REGISTER_GS)
      memoryOperand = TEB;

    Value* pointer = builder.CreateGEP(Type::getInt8Ty(context), memoryOperand,
                                       indices, "GEPSTORE-" + address + "-");

    pointer = simplifyValue(
        pointer,
        builder.GetInsertBlock()->getParent()->getParent()->getDataLayout());

    Value* store = builder.CreateStore(value, pointer);

    printvalue(effectiveAddress) printvalue(pointer);
    // if effectiveAddress is not pointing at stack, its probably self
    // modifying code if effectiveAddress and value is consant we can
    // say its a self modifying code and modify the binary

    GEPStoreTracker::insertMemoryOp(cast<StoreInst>(store));

    return store;
  } break;

  default: {
    throw std::runtime_error("operand type not implemented");
    exit(-1);
    return nullptr;
  }
  }
}

Value* getMemoryFromValue(IRBuilder<>& builder, Value* value) {
  LLVMContext& context = builder.getContext();

  std::vector<Value*> indices;
  indices.push_back(value);

  Value* pointer = builder.CreateGEP(Type::getInt8Ty(context), memoryAlloc,
                                     indices, "GEPSTOREVALUE");

  return pointer;
}

vector<Value*> lifterClass::GetRFLAGS() {
  vector<Value*> rflags;
  for (int flag = FLAG_CF; flag < FLAGS_END; flag++) {
    rflags.push_back(getFlag((Flag)flag));
  }
  return rflags;
}

void lifterClass::pushFlags(vector<Value*> value, string address) {
  LLVMContext& context = builder.getContext();

  auto rsp = GetRegisterValue(ZYDIS_REGISTER_RSP);

  for (size_t i = 0; i < value.size(); i += 8) {
    Value* byteVal = ConstantInt::get(Type::getInt8Ty(context), 0);
    for (size_t j = 0; j < 8 && (i + j) < value.size(); ++j) {
      Value* flag = value[i + j];
      Value* extendedFlag = createZExtFolder(
          builder, flag, Type::getInt8Ty(context), "pushflag1");
      Value* shiftedFlag =
          createShlFolder(builder, extendedFlag, j, "pushflag2");
      byteVal =
          createOrFolder(builder, byteVal, shiftedFlag, "pushflagbyteval");
    }

    std::vector<Value*> indices;
    indices.push_back(rsp);
    Value* pointer = builder.CreateGEP(Type::getInt8Ty(context), memoryAlloc,
                                       indices, "GEPSTORE-" + address + "-");

    auto store = builder.CreateStore(byteVal, pointer, "storebyte");

    printvalue(rsp) printvalue(pointer) printvalue(byteVal) printvalue(store);

    GEPStoreTracker::insertMemoryOp(cast<StoreInst>(store));
    rsp = createAddFolder(builder, rsp, ConstantInt::get(rsp->getType(), 1));
  }
}

// return [rsp], rsp+=8
Value* lifterClass::popStack() {
  LLVMContext& context = builder.getContext();
  auto rsp = GetRegisterValue(ZYDIS_REGISTER_RSP);
  // should we get a address calculator function, do we need that?

  std::vector<Value*> indices;
  indices.push_back(rsp);

  Value* pointer = builder.CreateGEP(Type::getInt8Ty(context), memoryAlloc,
                                     indices, "GEPLoadPOPStack--");

  auto loadType = Type::getInt64Ty(context);
  auto returnValue = builder.CreateLoad(loadType, pointer, "PopStack-");

  auto CI = ConstantInt::get(rsp->getType(), 8);
  SetRegisterValue(ZYDIS_REGISTER_RSP, createAddFolder(builder, rsp, CI));

  Value* solvedLoad = GEPStoreTracker::solveLoad(returnValue);
  if (solvedLoad) {
    return solvedLoad;
  }

  return returnValue;
}