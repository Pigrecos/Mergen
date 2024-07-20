﻿#include "FunctionSignatures.h"
#include "GEPTracker.h"
#include "OperandUtils.h"
#include "PathSolver.h"
#include "includes.h"
#include "utils.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/Casting.h>

FunctionType* parseArgsType(funcsignatures::functioninfo* funcInfo,
                            LLVMContext& context) {
  if (!funcInfo) {
    FunctionType* externFuncType = FunctionType::get(
        Type::getInt64Ty(context),
        {llvm::Type::getInt64Ty(context), llvm::Type::getInt64Ty(context),
         llvm::Type::getInt64Ty(context), llvm::Type::getInt64Ty(context),
         llvm::Type::getInt64Ty(context), llvm::Type::getInt64Ty(context),
         llvm::Type::getInt64Ty(context), llvm::Type::getInt64Ty(context),
         llvm::Type::getInt64Ty(context), llvm::Type::getInt64Ty(context),
         llvm::Type::getInt64Ty(context), llvm::Type::getInt64Ty(context),
         llvm::Type::getInt64Ty(context), llvm::Type::getInt64Ty(context),
         llvm::Type::getInt64Ty(context), llvm::Type::getInt64Ty(context)},
        false);

    return externFuncType;
  }
  std::vector<llvm::Type*> argTypes;
  for (const auto& arg : funcInfo->args) {
    llvm::Type* type = nullptr;
    type = llvm::Type::getIntNTy(context, 8 << (arg.argtype.size - 1));

    if (arg.argtype.isPtr) {
      type = type->getPointerTo();
    }
    argTypes.push_back(type);
  }

  return llvm::FunctionType::get(llvm::Type::getInt64Ty(context), argTypes,
                                 false);
}

vector<Value*> parseArgs(funcsignatures::functioninfo* funcInfo, IRBuilder<>& builder) {
  auto& context = builder.getContext();

  auto RspRegister = GetRegisterValue(builder, ZYDIS_REGISTER_RSP);
  if (!funcInfo)
    return {
        // General purpose registers
        createZExtFolder(builder, GetRegisterValue(builder, ZYDIS_REGISTER_RAX), Type::getInt64Ty(context)),
        createZExtFolder(builder, GetRegisterValue(builder, ZYDIS_REGISTER_RCX), Type::getInt64Ty(context)),
        createZExtFolder(builder, GetRegisterValue(builder, ZYDIS_REGISTER_RDX), Type::getInt64Ty(context)),
        createZExtFolder(builder, GetRegisterValue(builder, ZYDIS_REGISTER_RBX), Type::getInt64Ty(context)),
        RspRegister,
        createZExtFolder(builder, GetRegisterValue(builder, ZYDIS_REGISTER_RBP), Type::getInt64Ty(context)),
        createZExtFolder(builder, GetRegisterValue(builder, ZYDIS_REGISTER_RSI), Type::getInt64Ty(context)),
        createZExtFolder(builder, GetRegisterValue(builder, ZYDIS_REGISTER_RDI), Type::getInt64Ty(context)),
        createZExtFolder(builder, GetRegisterValue(builder, ZYDIS_REGISTER_R8),  Type::getInt64Ty(context)),
        createZExtFolder(builder, GetRegisterValue(builder, ZYDIS_REGISTER_R9),  Type::getInt64Ty(context)),
        createZExtFolder(builder, GetRegisterValue(builder, ZYDIS_REGISTER_R10), Type::getInt64Ty(context)),
        createZExtFolder(builder, GetRegisterValue(builder, ZYDIS_REGISTER_R11), Type::getInt64Ty(context)),
        createZExtFolder(builder, GetRegisterValue(builder, ZYDIS_REGISTER_R12), Type::getInt64Ty(context)),
        createZExtFolder(builder, GetRegisterValue(builder, ZYDIS_REGISTER_R13), Type::getInt64Ty(context)),
        createZExtFolder(builder, GetRegisterValue(builder, ZYDIS_REGISTER_R14), Type::getInt64Ty(context)),
        createZExtFolder(builder, GetRegisterValue(builder, ZYDIS_REGISTER_R15), Type::getInt64Ty(context)),
        // XMM registers
        GetRegisterValue(builder, ZYDIS_REGISTER_XMM0),
        GetRegisterValue(builder, ZYDIS_REGISTER_XMM1),
        GetRegisterValue(builder, ZYDIS_REGISTER_XMM2),
        GetRegisterValue(builder, ZYDIS_REGISTER_XMM3),
        GetRegisterValue(builder, ZYDIS_REGISTER_XMM4),
        GetRegisterValue(builder, ZYDIS_REGISTER_XMM5),
        GetRegisterValue(builder, ZYDIS_REGISTER_XMM6),
        GetRegisterValue(builder, ZYDIS_REGISTER_XMM7),
        GetRegisterValue(builder, ZYDIS_REGISTER_XMM8),
        GetRegisterValue(builder, ZYDIS_REGISTER_XMM9),
        GetRegisterValue(builder, ZYDIS_REGISTER_XMM10),
        GetRegisterValue(builder, ZYDIS_REGISTER_XMM11),
        GetRegisterValue(builder, ZYDIS_REGISTER_XMM12),
        GetRegisterValue(builder, ZYDIS_REGISTER_XMM13),
        GetRegisterValue(builder, ZYDIS_REGISTER_XMM14),
        GetRegisterValue(builder, ZYDIS_REGISTER_XMM15),
        // Memory
        getMemory()};

  std::vector<Value*> args;
  for (const auto& arg : funcInfo->args) {
    Value* argValue = GetRegisterValue(builder, arg.reg);

    // Handle XMM registers differently
    if (ZYDIS_REGISTER_IS_XMM(arg.reg)) {
      // XMM registers are already 128-bit, so we don't need to extend or
      // truncate
      args.push_back(argValue);
    } else {
      argValue = createZExtOrTruncFolder(builder, argValue, Type::getIntNTy(context, 8 << (arg.argtype.size - 1)));
      if (arg.argtype.isPtr)
        argValue = ConvertIntToPTR(builder, argValue);
      args.push_back(argValue);
    }
  }
  return args;
}

// probably move this stuff somewhere else
void callFunctionIR(string functionName, IRBuilder<>& builder,
                    funcsignatures::functioninfo* funcInfo) {
  auto& context = builder.getContext();

  /*
  if (functionName == "GetTickCount64") {
      SetRegisterValue(
          builder, ZYDIS_REGISTER_RAX,
          ConstantInt::get(builder.getInt64Ty(), 1)); // rax = externalfunc()
      return;
  }
  */

  // TODO: lololololololol wtf
  if (!funcInfo) {
    // try to get funcinfo from name
    funcInfo = funcsignatures::getFunctionInfo(functionName);
  }
  FunctionType* externFuncType = parseArgsType(funcInfo, context);
  auto M = builder.GetInsertBlock()->getParent()->getParent();

  // what about ordinals???????
  Function* externFunc = cast<Function>(
      M->getOrInsertFunction(functionName, externFuncType).getCallee());
  // fix calling
  vector<Value*> args = parseArgs(funcInfo, builder);
  auto callresult = builder.CreateCall(externFunc, args);
  SetRegisterValue(builder, ZYDIS_REGISTER_RAX,
                   callresult); // rax = externalfunc()
  // check if the function is exit or something similar to that
}

Value* computeOverflowFlagAdc(IRBuilder<>& builder, Value* Lvalue,
                              Value* Rvalue, Value* cf, Value* add) {
  auto cfc = createZExtOrTruncFolder(builder, cf, add->getType(), "ofadc1");
  auto ofAdd = createAddFolder(builder, add, cfc, "ofadc2");
  auto xor0 = createXorFolder(builder, Lvalue, ofAdd, "ofadc3");
  auto xor1 = createXorFolder(builder, Rvalue, ofAdd, "ofadc4");
  auto ofAnd = createAndFolder(builder, xor0, xor1, "ofadc5");
  return createICMPFolder(builder, CmpInst::ICMP_SLT, ofAnd,
                          ConstantInt::get(ofAnd->getType(), 0), "ofadc6");
}

Value* computeOverflowFlagAdd(IRBuilder<>& builder, Value* Lvalue,
                              Value* Rvalue, Value* add) {
  auto xor0 = createXorFolder(builder, Lvalue, add, "ofadd");
  auto xor1 = createXorFolder(builder, Rvalue, add, "ofadd1");
  auto ofAnd = createAndFolder(builder, xor0, xor1, "ofadd2");
  return createICMPFolder(builder, CmpInst::ICMP_SLT, ofAnd,
                          ConstantInt::get(ofAnd->getType(), 0), "ofadd3");
}

Value* computeOverflowFlagSub(IRBuilder<>& builder, Value* Lvalue,
                              Value* Rvalue, Value* sub) {
  auto xor0 = createXorFolder(builder, Lvalue, Rvalue, "ofsub");
  auto xor1 = createXorFolder(builder, Lvalue, sub, "ofsub1");
  auto ofAnd = createAndFolder(builder, xor0, xor1, "ofsub2");
  return createICMPFolder(builder, CmpInst::ICMP_SLT, ofAnd,
                          ConstantInt::get(ofAnd->getType(), 0), "ofsub3");
}

Value* computeOverflowFlagSbb(IRBuilder<>& builder, Value* Lvalue,
                              Value* Rvalue, Value* cf, Value* sub) {
  auto cfc = createZExtOrTruncFolder(builder, cf, sub->getType(), "ofsbb");
  auto ofSub = createSubFolder(builder, sub, cfc, "ofsbb1");
  auto xor0 = createXorFolder(builder, Lvalue, Rvalue, "ofsbb2");
  auto xor1 = createXorFolder(builder, Lvalue, ofSub, "ofsbb3");
  auto ofAnd = createAndFolder(builder, xor0, xor1, "ofsbb4");
  return createICMPFolder(builder, CmpInst::ICMP_SLT, ofAnd,
                          ConstantInt::get(ofAnd->getType(), 0), "ofsbb5");
}

Value* computeAuxFlagSbb(IRBuilder<>& builder, Value* Lvalue, Value* Rvalue,
                         Value* cf) {
  auto ci15 = ConstantInt::get(Lvalue->getType(), 15);
  auto and0 = createAndFolder(builder, Lvalue, ci15, "auxsbb1");
  auto and1 = createAndFolder(builder, Rvalue, ci15, "auxsbb2");
  auto sub = createSubFolder(builder, and0, and1, "auxsbb3");

  auto cfc = createZExtOrTruncFolder(builder, cf, sub->getType(), "auxsbb4");
  auto add = createAddFolder(builder, sub, cfc, "auxsbb5");
  return createICMPFolder(builder, CmpInst::ICMP_UGT, add, ci15, "auxsbb6");
}

/*
https://graphics.stanford.edu/~seander/bithacks.html#ParityWith64Bits

Compute parity of a byte using 64-bit multiply and modulus division
unsigned char b;  // byte value to compute the parity of
bool parity =
  (((b * 0x0101010101010101ULL) & 0x8040201008040201ULL) % 0x1FF) & 1;
The method above takes around 4 operations, but only works on bytes.
*/
Value* computeParityFlag(IRBuilder<>& builder, Value* value) {
  LLVMContext& context = value->getContext();

  Value* lsb = builder.CreateZExt(
      createAndFolder(builder, value, ConstantInt::get(value->getType(), 0xFF),
                      "lsb"),
      Type::getInt64Ty(context));

  // s or u rem?
  Value* parity = createAndFolder(
      builder,
      builder.CreateURem(
          createAndFolder(
              builder,
              builder.CreateMul(
                  lsb, ConstantInt::get(lsb->getType(), 0x0101010101010101),
                  "pf1"),
              ConstantInt::get(lsb->getType(), 0x8040201008040201ULL), "pf2"),
          ConstantInt::get(lsb->getType(), 0x1FF), "pf3"),
      ConstantInt::get(lsb->getType(), 1), "pf4");
  // parity
  parity =
      builder.CreateICmpEQ(ConstantInt::get(lsb->getType(), 0), parity, "pf5");
  return parity; // Returns 1 if even parity, 0 if odd
}

Value* computeZeroFlag(IRBuilder<>& builder, Value* value) { // x == 0 = zf
  return createICMPFolder(builder, CmpInst::ICMP_EQ, value,
                          ConstantInt::get(value->getType(), 0), "zeroflag");
}

Value* computeSignFlag(IRBuilder<>& builder, Value* value) { // x < 0 = sf
  return createICMPFolder(builder, CmpInst::ICMP_SLT, value,
                          ConstantInt::get(value->getType(), 0), "signflag");
}

// this function is used for jumps that are related to user, ex: vms using
// different handlers, jmptables, etc.

void branchHelper(IRBuilder<>& builder, ZydisDisassembledInstruction& instruction, shared_ptr<vector<BBInfo>> blockAddresses, Value* condition, string instname, int numbered) {
  LLVMContext& context = builder.getContext();
  // TODO:
  // save the current state of memory, registers etc.,
  // after execution is finished, return to latest state and continue
  // execution from the other branch

  auto block = builder.GetInsertBlock();
  block->setName(instname + to_string(numbered));
  auto function = block->getParent();

  auto dest = instruction.operands[0];
  Value* true_jump = ConstantInt::get(function->getReturnType(),  dest.imm.value.s + instruction.runtime_address + instruction.info.length);
  Value* false_jump = ConstantInt::get(function->getReturnType(), instruction.runtime_address + instruction.info.length);
  Value* next_jump =  createSelectFolder(builder, condition, true_jump, false_jump);
  auto lastinst = builder.CreateRet(next_jump);

  uintptr_t destination = 0;

  PATH_info pathInfo;
  // Controlla se l'indirizzo corrente è nella tabella dei
  // predicati opachi
  auto it = std::find_if(opaquePredicates.begin(), opaquePredicates.end(),
                         [&instruction](const OpaquePredicateEntry& entry) {
                           return entry.address == instruction.runtime_address;
                         });

  if (it != opaquePredicates.end()) {
    // Se troviamo un predicato opaco, usiamo la nuova destinazione
    pathInfo = PATH_solved;
    destination = it->jmpAddress;
    printvalueforce2(instruction.runtime_address);
    outs() << "Opaque Predicate replaced!\n";
    outs().flush();
  } else {
    // Altrimenti, proseguiamo normalmente
    pathInfo = solvePath(function, destination, next_jump);
  }

  ValueToValueMapTy VMap_test;

  block->setName("previousjmp_block-" + to_string(destination) + "-");
  // cout << "pathInfo:" << pathInfo << " dest: " << destination  <<
  // "\n";
  if (pathInfo == PATH_solved) {

    lastinst->eraseFromParent();
    string block_name = "jmp-" + to_string(destination) + "-";
    auto bb = BasicBlock::Create(context, block_name.c_str(),
                                 builder.GetInsertBlock()->getParent());

    builder.CreateBr(bb);

    blockAddresses->push_back(make_tuple(destination, bb, getRegisters()));
  }
}

namespace mov {

  void lift_movsb(IRBuilder<>& builder,
                  ZydisDisassembledInstruction& instruction) {

    // DEST := SRC;
    // [esi] = [edi]
    // sign = DF (-1/+1)
    // incdecv = size*sign (sb means size is 1)
    // esi += incdecv
    // edi += incdecv
    //

    // Value* SRCptrvalue =
    // GetOperandValue(builder,instruction.operands[0],instruction.operands[0].size);

    Value* DSTptrvalue = GetOperandValue(builder, instruction.operands[1],
                                         instruction.operands[1].size);

    SetOperandValue(builder, instruction.operands[0], DSTptrvalue);

    bool isREP = (instruction.info.attributes & ZYDIS_ATTRIB_HAS_REP) != 0;

    Value* DF = getFlag(builder, FLAG_DF);
    auto one = ConstantInt::get(DF->getType(), 1);
    // sign = (x*(x+1)) - 1
    // v = sign * bytesize ; bytesize is 1

    Value* Direction = builder.CreateSub(
        builder.CreateMul(DF, builder.CreateAdd(DF, one)), one);

    auto SRCop = instruction.operands[2 + isREP];
    auto DSTop = instruction.operands[3 + isREP];

    Value* SRCvalue = GetOperandValue(builder, SRCop, SRCop.size);
    Value* DSTvalue = GetOperandValue(builder, DSTop, DSTop.size);

    if (isREP) {
      // if REP, instruction.operands[1] will be e/rax
      // in that case, repeat and decrement e/rax until its 0

      // we can create a loop but I dont know how that would effect our
      // optimizations
      Value* count = GetOperandValue(builder, instruction.operands[2],
                                     instruction.operands[2].size);
      if (auto countci = dyn_cast<ConstantInt>(count)) {
        Value* UpdateSRCvalue = SRCvalue;
        Value* UpdateDSTvalue = DSTvalue;
        uint64_t looptime = countci->getZExtValue();
        printvalue2(looptime);

        for (int i = looptime; i > 0; i--) {
          // TODO: fix this loop

          // Value* SRCptrvalue = GetOperandValue(builder,
          // instruction.operands[0],
          // instruction.operands[0].size);
          Value* DSTptrvalue = GetOperandValue(builder, instruction.operands[1],
                                               instruction.operands[1].size);

          SetOperandValue(builder, instruction.operands[0], DSTptrvalue);

          UpdateSRCvalue = builder.CreateAdd(UpdateSRCvalue, Direction);
          UpdateDSTvalue = builder.CreateAdd(UpdateDSTvalue, Direction);
          printvalue(UpdateDSTvalue) printvalue(UpdateSRCvalue);

          SetOperandValue(builder, SRCop, UpdateSRCvalue);
          SetOperandValue(builder, DSTop, UpdateDSTvalue);
          // bad cheat
          if (i > 1)
            debugging::increaseInstCounter();
        }

        SetOperandValue(builder, instruction.operands[2],
                        ConstantInt::get(count->getType(), 0));

        return;
      } else {
        throw "fix rep";
      }
    }

    Value* UpdateSRCvalue = builder.CreateAdd(SRCvalue, Direction);
    Value* UpdateDSTvalue = builder.CreateAdd(DSTvalue, Direction);

    SetOperandValue(builder, SRCop, UpdateSRCvalue);
    SetOperandValue(builder, DSTop, UpdateDSTvalue);
  }
  void lift_movaps(IRBuilder<>& builder,
                   ZydisDisassembledInstruction& instruction) {
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    auto Rvalue = GetOperandValue(builder, src, src.size,
                                  to_string(instruction.runtime_address));
    SetOperandValue(builder, dest, Rvalue,
                    to_string(instruction.runtime_address));
  }
  void lift_mov(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    auto Rvalue = GetOperandValue(builder, src, src.size,
                                  to_string(instruction.runtime_address));

    switch (instruction.info.mnemonic) {
    case ZYDIS_MNEMONIC_MOVSX: {
      Rvalue = createSExtFolder(
          builder, Rvalue, Type::getIntNTy(context, dest.size),
          "movsx-" + to_string(instruction.runtime_address) + "-");
      break;
    }
    case ZYDIS_MNEMONIC_MOVZX: {
      Rvalue = createZExtFolder(
          builder, Rvalue, Type::getIntNTy(context, dest.size),
          "movzx-" + to_string(instruction.runtime_address) + "-");
      break;
    }
    case ZYDIS_MNEMONIC_MOVSXD: {
      Rvalue = createSExtFolder(
          builder, Rvalue, Type::getIntNTy(context, dest.size),
          "movsxd-" + to_string(instruction.runtime_address) + "-");
      break;
    }
    default: {
      break;
    }
    }
    printvalue(Rvalue);
    if (src.type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
      Rvalue = GetOperandValue(builder, src, dest.size);
    }

    printvalue(Rvalue);

    SetOperandValue(builder, dest, Rvalue,
                    to_string(instruction.runtime_address));
  }

}; // namespace mov

namespace cmov {

  void lift_cmovbz(IRBuilder<>& builder,
                   ZydisDisassembledInstruction& instruction) {

    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    Value* zf = getFlag(builder, FLAG_ZF);
    Value* cf = getFlag(builder, FLAG_CF);

    Value* condition = createOrFolder(builder, zf, cf, "cmovbz-or");

    Value* Rvalue = GetOperandValue(builder, src, dest.size);
    Value* Lvalue = GetOperandValue(builder, dest, dest.size);

    Value* result = createSelectFolder(builder, condition, Rvalue, Lvalue);

    SetOperandValue(builder, dest, result);
  }

  void lift_cmovnbz(IRBuilder<>& builder,
                    ZydisDisassembledInstruction& instruction) {
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    Value* Lvalue = GetOperandValue(builder, dest, dest.size);
    Value* Rvalue = GetOperandValue(builder, src, src.size);

    Value* cf = getFlag(builder, FLAG_CF);
    Value* zf = getFlag(builder, FLAG_ZF);

    Value* nbeCondition = createAndFolder(
        builder, builder.CreateNot(cf), builder.CreateNot(zf), "nbeCondition");

    Value* resultValue =
        createSelectFolder(builder, nbeCondition, Rvalue, Lvalue, "cmovnbe");

    SetOperandValue(builder, dest, resultValue,
                    to_string(instruction.runtime_address));
  }

  void lift_cmovz(IRBuilder<>& builder,
                  ZydisDisassembledInstruction& instruction) {
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    Value* Lvalue = GetOperandValue(builder, dest, dest.size);
    Value* Rvalue = GetOperandValue(builder, src, src.size);

    Value* zf = getFlag(builder, FLAG_ZF);

    Value* resultValue =
        createSelectFolder(builder, zf, Rvalue, Lvalue, "cmovz");

    SetOperandValue(builder, dest, resultValue,
                    to_string(instruction.runtime_address));
  }

  void lift_cmovnz(IRBuilder<>& builder,
                   ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    Value* zf = getFlag(builder, FLAG_ZF);
    zf = createICMPFolder(builder, CmpInst::ICMP_EQ, zf,
                          ConstantInt::get(Type::getInt1Ty(context), 0));

    Value* Rvalue = GetOperandValue(builder, src, dest.size);
    Value* Lvalue = GetOperandValue(builder, dest, dest.size);

    Value* result = createSelectFolder(builder, zf, Rvalue, Lvalue);

    SetOperandValue(builder, dest, result);
  }
  void lift_cmovl(IRBuilder<>& builder,
                  ZydisDisassembledInstruction& instruction) {

    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    Value* sf = getFlag(builder, FLAG_SF);
    Value* of = getFlag(builder, FLAG_OF);

    Value* condition = createICMPFolder(builder, CmpInst::ICMP_NE, sf, of);

    Value* Rvalue = GetOperandValue(builder, src, dest.size);
    Value* Lvalue = GetOperandValue(builder, dest, dest.size);

    Value* result = createSelectFolder(builder, condition, Rvalue, Lvalue);

    SetOperandValue(builder, dest, result);
  }

  void lift_cmovb(IRBuilder<>& builder,
                  ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    Value* cf = getFlag(builder, FLAG_CF);

    Value* condition =
        createICMPFolder(builder, CmpInst::ICMP_EQ, cf,
                         ConstantInt::get(Type::getInt1Ty(context), 1));

    Value* Rvalue = GetOperandValue(builder, src, dest.size);
    Value* Lvalue = GetOperandValue(builder, dest, dest.size);

    Value* result = createSelectFolder(builder, condition, Rvalue, Lvalue);

    SetOperandValue(builder, dest, result);
  }

  void lift_cmovnb(IRBuilder<>& builder,
                   ZydisDisassembledInstruction& instruction) {
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    Value* Lvalue = GetOperandValue(builder, dest, dest.size);
    Value* Rvalue = GetOperandValue(builder, src, src.size);

    Value* cf = getFlag(builder, FLAG_CF);

    Value* resultValue = createSelectFolder(builder, builder.CreateNot(cf),
                                            Rvalue, Lvalue, "cmovnb");

    SetOperandValue(builder, dest, resultValue,
                    to_string(instruction.runtime_address));
  }

  void lift_cmovns(IRBuilder<>& builder,
                   ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    Value* sf = getFlag(builder, FLAG_SF);

    Value* condition =
        createICMPFolder(builder, CmpInst::ICMP_EQ, sf,
                         ConstantInt::get(Type::getInt1Ty(context), 0));

    Value* Rvalue = GetOperandValue(builder, src, dest.size);
    Value* Lvalue = GetOperandValue(builder, dest, dest.size);

    Value* result = createSelectFolder(builder, condition, Rvalue, Lvalue);

    SetOperandValue(builder, dest, result);
  }
  // cmovnl = cmovge
  void lift_cmovnl(IRBuilder<>& builder,
                   ZydisDisassembledInstruction& instruction) {
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    Value* sf = getFlag(builder, FLAG_SF);
    Value* of = getFlag(builder, FLAG_OF);
    Value* condition = createICMPFolder(builder, CmpInst::ICMP_EQ, sf, of);

    Value* Rvalue = GetOperandValue(builder, src, dest.size);
    Value* Lvalue = GetOperandValue(builder, dest, dest.size);
    printvalue(sf);
    printvalue(of);
    printvalue(condition);
    printvalue(Lvalue);
    printvalue(Rvalue);

    Value* result = createSelectFolder(builder, condition, Rvalue, Lvalue);

    SetOperandValue(builder, dest, result);
  }
  void lift_cmovs(IRBuilder<>& builder,
                  ZydisDisassembledInstruction& instruction) {
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    Value* sf = getFlag(builder, FLAG_SF);

    Value* Rvalue = GetOperandValue(builder, src, dest.size);
    Value* Lvalue = GetOperandValue(builder, dest, dest.size);

    Value* result = createSelectFolder(builder, sf, Rvalue, Lvalue);

    SetOperandValue(builder, dest, result);
  }

  void lift_cmovnle(IRBuilder<>& builder,
                    ZydisDisassembledInstruction& instruction) {

    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    Value* zf = getFlag(builder, FLAG_ZF);
    Value* sf = getFlag(builder, FLAG_SF);
    Value* of = getFlag(builder, FLAG_OF);

    Value* condition = createAndFolder(
        builder, builder.CreateNot(zf, "notZF"),
        createICMPFolder(builder, CmpInst::ICMP_EQ, sf, of, "sf_eq_of"),
        "cmovnle_cond");

    Value* Rvalue = GetOperandValue(builder, src, dest.size);
    Value* Lvalue = GetOperandValue(builder, dest, dest.size);

    Value* result = createSelectFolder(builder, condition, Rvalue, Lvalue);

    SetOperandValue(builder, dest, result);
  }

  void lift_cmovle(IRBuilder<>& builder,
                   ZydisDisassembledInstruction& instruction) {
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    Value* zf = getFlag(builder, FLAG_ZF);
    Value* sf = getFlag(builder, FLAG_SF);
    Value* of = getFlag(builder, FLAG_OF);

    Value* sf_neq_of = createICMPFolder(builder, CmpInst::ICMP_NE, sf, of);
    Value* condition = createOrFolder(builder, zf, sf_neq_of, "cmovle-or");

    Value* Rvalue = GetOperandValue(builder, src, dest.size);
    Value* Lvalue = GetOperandValue(builder, dest, dest.size);

    Value* result = createSelectFolder(builder, condition, Rvalue, Lvalue);

    SetOperandValue(builder, dest, result);
  }

  void lift_cmovo(IRBuilder<>& builder,
                  ZydisDisassembledInstruction& instruction) {
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    Value* of = getFlag(builder, FLAG_OF);

    Value* Rvalue = GetOperandValue(builder, src, dest.size);
    Value* Lvalue = GetOperandValue(builder, dest, dest.size);

    Value* result = createSelectFolder(builder, of, Rvalue, Lvalue);
    printvalue(Lvalue);
    printvalue(Rvalue);
    printvalue(of);
    printvalue(result);
    SetOperandValue(builder, dest, result);
  }
  void lift_cmovno(IRBuilder<>& builder,
                   ZydisDisassembledInstruction& instruction) {
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    Value* of = getFlag(builder, FLAG_OF);

    printvalue(of) of = builder.CreateNot(of, "negateOF");

    Value* Rvalue = GetOperandValue(builder, src, dest.size);
    Value* Lvalue = GetOperandValue(builder, dest, dest.size);

    Value* result = createSelectFolder(builder, of, Rvalue, Lvalue);

    printvalue(Lvalue) printvalue(Rvalue) printvalue(result)
        SetOperandValue(builder, dest, result);
  }

  void lift_cmovp(IRBuilder<>& builder,
                  ZydisDisassembledInstruction& instruction) {
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    Value* pf = getFlag(builder, FLAG_PF);

    Value* Rvalue = GetOperandValue(builder, src, dest.size);
    Value* Lvalue = GetOperandValue(builder, dest, dest.size);
    printvalue(pf) printvalue(Lvalue) printvalue(Rvalue)

        Value* result = createSelectFolder(builder, pf, Rvalue, Lvalue);

    SetOperandValue(builder, dest, result);
  }

  void lift_cmovnp(IRBuilder<>& builder,
                   ZydisDisassembledInstruction& instruction) {
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    Value* pf = getFlag(builder, FLAG_PF);

    pf = builder.CreateNot(pf, "negatePF");

    Value* Rvalue = GetOperandValue(builder, src, dest.size);
    Value* Lvalue = GetOperandValue(builder, dest, dest.size);

    Value* result = createSelectFolder(builder, pf, Rvalue, Lvalue);

    SetOperandValue(builder, dest, result);
  }
} // namespace cmov

namespace branches {

  // for now assume every call is fake
  void lift_call(IRBuilder<>& builder,
                 ZydisDisassembledInstruction& instruction,
                 shared_ptr<vector<BBInfo>> blockAddresses) {
    LLVMContext& context = builder.getContext();
    // 0 = function
    // 1 = rip
    // 2 = register rsp
    // 3 = [rsp]
    auto src = instruction.operands[0];        // value that we are pushing
    auto rsp = instruction.operands[2];        // value that we are pushing
    auto rsp_memory = instruction.operands[3]; // value that we are pushing

    auto RspValue = GetOperandValue(builder, rsp, rsp.size);

    auto val = ConstantInt::getSigned(Type::getInt64Ty(context),
                                      8); // assuming its x64
    auto result = createSubFolder(builder, RspValue, val, "pushing_newrsp");

    SetOperandValue(builder, rsp, result,
                    to_string(instruction.runtime_address));
    ; // sub rsp 8 first,

    auto push_into_rsp = GetRegisterValue(builder, ZYDIS_REGISTER_RIP);

    SetOperandValue(builder, rsp_memory, push_into_rsp,
                    to_string(instruction.runtime_address));
    ; // sub rsp 8 first,

    string block_name = "jmp-call";

    uint64_t jump_address =
        instruction.runtime_address + instruction.info.length;
    switch (src.type) {
    case ZYDIS_OPERAND_TYPE_IMMEDIATE: {
      jump_address += src.imm.value.s;
      break;
    }
    case ZYDIS_OPERAND_TYPE_MEMORY:
    case ZYDIS_OPERAND_TYPE_REGISTER: {
      auto registerValue = GetOperandValue(builder, src, 64);
      if (!isa<ConstantInt>(registerValue)) {

        callFunctionIR(registerValue->getName().str() + "fnc_ptr", builder,
                       nullptr);

        SetOperandValue(builder, rsp, RspValue,
                        to_string(instruction.runtime_address));
        break;
        // registerValue =
        //    ConstantInt::get(Type::getInt32Ty(context), 0x1337);

        // throw("trying to call an unknown value");
      }
      auto registerCValue = cast<ConstantInt>(registerValue);
      jump_address = registerCValue->getZExtValue();
      break;
    }
    default:
      break;
    }

    auto bb = BasicBlock::Create(context, block_name.c_str(),
                                 builder.GetInsertBlock()->getParent());
    // if its trying to jump somewhere else than our binary, call it and
    // continue from [rsp]
    APInt temp;

    builder.CreateBr(bb);

    printvalue2(jump_address);

    blockAddresses->push_back(make_tuple(jump_address, bb, getRegisters()));
  }

  int ret_count = 0;
  void lift_ret(IRBuilder<>& builder, ZydisDisassembledInstruction& instruction, shared_ptr<vector<BBInfo>> blockAddresses, bool& run) {
    LLVMContext& context = builder.getContext();
    // [0] = rip
    // [1] = rsp
    // [2] = [rsp]

    // if its ret 0x10
    // then its
    // [0] = 0x10
    // [1] = rip
    // [2] = rsp
    // [3] = [rsp]

    auto rspaddr = instruction.operands[2];

    auto rsp = ZYDIS_REGISTER_RSP;
    auto rspvalue = GetRegisterValue(builder, rsp);
    if (instruction.operands[0].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
      rspaddr = instruction.operands[3];
    }

    auto realval = GetOperandValue(builder, rspaddr, rspaddr.size);

    auto block = builder.GetInsertBlock();
    block->setName("ret_check" + to_string(ret_count));
    auto function = block->getParent();
    auto lastinst = builder.CreateRet(realval);

    printvalue(rspvalue) debugging::doIfDebug([&]() {
      std::string Filename = "output_rets.ll";
      std::error_code EC;
      raw_fd_ostream OS(Filename, EC);
      function->getParent()->print(OS, nullptr);
    });

    uint64_t destination = 0;

    ROP_info result = ROP_return;

    if (llvm::ConstantInt* constInt =
            llvm::dyn_cast<llvm::ConstantInt>(rspvalue)) {
      int64_t rspval = constInt->getSExtValue();
      printvalue2(rspval);
      result = rspval == STACKP_VALUE ? REAL_return : ROP_return;
    }
    printvalue2(result);
    if (result == REAL_return) {
      lastinst->eraseFromParent();
      block->setName("real_ret");
      auto rax = GetRegisterValue(builder, ZYDIS_REGISTER_RAX);
      builder.CreateRet(
          createZExtFolder(builder, rax, Type::getInt64Ty(rax->getContext())));
      Function* originalFunc_finalnopt = builder.GetInsertBlock()->getParent();

      std::string Filename_finalnopt = "output_finalnoopt.ll";
      std::error_code EC_finalnopt;
      raw_fd_ostream OS_finalnopt(Filename_finalnopt, EC_finalnopt);

      originalFunc_finalnopt->print(OS_finalnopt);

      // function->print(outs());

      final_optpass(originalFunc_finalnopt);
      debugging::doIfDebug([&]() {
        std::string Filename = "output_finalopt.ll";
        std::error_code EC;
        raw_fd_ostream OS(Filename, EC);
        originalFunc_finalnopt->print(OS);
      });
      run = 0;
      return;
    }

    auto it = std::find_if(returnTable.begin(), returnTable.end(),
                           [&instruction](const RetItem& item) {
                             return item.Address == instruction.runtime_address;
                           });

    PATH_info pathInfo;
    if (it != returnTable.end()) {
      // Abbiamo trovato una corrispondenza
      destination = it->RetAddress;
      pathInfo = PATH_solved;
      printvalue2(destination);
      // printvalue2("Found in returnTable");
      returnTable.erase(it);
    } else {
      // Se non troviamo una corrispondenza, procediamo con solvePath
      pathInfo = solvePath(function, destination, realval);
    }

    //PATH_info pathInfo = solvePath(function, destination, realval);

    block->setName("previousret_block");

    if (pathInfo == PATH_solved) {

      lastinst->eraseFromParent();
      block->setName("fake_ret");

      string block_name = "jmp_ret-" + to_string(destination) + "-";
      auto bb = BasicBlock::Create(context, block_name.c_str(),
                                   builder.GetInsertBlock()->getParent());

      auto val = ConstantInt::getSigned(Type::getInt64Ty(context),
                                        8); // assuming its x64
      auto result = createAddFolder(
          builder, rspvalue, val,
          "ret-new-rsp-" + to_string(instruction.runtime_address) + "-");

      if (instruction.operands[0].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        rspaddr = instruction.operands[3];
        result = createAddFolder(
            builder, result,
            ConstantInt::get(result->getType(),
                             instruction.operands[0].imm.value.u));
      }

      SetRegisterValue(builder, rsp, result); // then add rsp 8

      builder.CreateBr(bb);

      blockAddresses->push_back(make_tuple(destination, bb, getRegisters()));
      run = 0;
    }
  }

  int jmpcount = 0;
  void lift_jmp(IRBuilder<>& builder, ZydisDisassembledInstruction& instruction,
                shared_ptr<vector<BBInfo>> blockAddresses, bool& run) {
    LLVMContext& context = builder.getContext();
    auto dest = instruction.operands[0];

    auto Value = GetOperandValue(builder, dest, 64);
    auto ripval = GetRegisterValue(builder, ZYDIS_REGISTER_RIP);
    auto newRip = createAddFolder(
        builder, Value, ripval,
        "jump-xd-" + to_string(instruction.runtime_address) + "-");

    jmpcount++;
    if (dest.type == ZYDIS_OPERAND_TYPE_REGISTER ||
        dest.type == ZYDIS_OPERAND_TYPE_MEMORY) {
      auto rspvalue = GetOperandValue(builder, dest, 64);
      auto trunc = createZExtOrTruncFolder(
          builder, rspvalue, Type::getInt64Ty(context), "jmp-register");

      auto block = builder.GetInsertBlock();
      block->setName("jmp_check" + to_string(ret_count));
      auto function = block->getParent();

      auto lastinst = builder.CreateRet(trunc);
      debugging::doIfDebug([&]() {
        std::string Filename = "output_beforeJMP.ll";
        std::error_code EC;
        raw_fd_ostream OS(Filename, EC);
        function->print(OS);
      });

      uint64_t destination = 0;
      PATH_info pathInfo = solvePath(function, destination, trunc);

      ValueToValueMapTy VMap_test;

      block->setName("previousjmp_block-" + to_string(destination) + "-");
      // cout << "pathInfo:" << pathInfo << " dest: " << destination  <<
      // "\n";
      if (pathInfo == PATH_solved) {

        lastinst->eraseFromParent();
        string block_name = "jmp-" + to_string(destination) + "-";
        auto bb = BasicBlock::Create(context, block_name.c_str(),
                                     builder.GetInsertBlock()->getParent());

        builder.CreateBr(bb);

        blockAddresses->push_back(make_tuple(destination, bb, getRegisters()));
        run = 0;
      }
      run = 0;

      // if ROP is not JOP_jmp, then its bugged
      return;
    }

    SetRegisterValue(builder, ZYDIS_REGISTER_RIP, newRip);

    uint64_t test = dest.imm.value.s + instruction.runtime_address +
                    instruction.info.length;

    string block_name = "jmp-" + to_string(test) + "-";
    auto bb = BasicBlock::Create(context, block_name.c_str(),
                                 builder.GetInsertBlock()->getParent());

    builder.CreateBr(bb);

    blockAddresses->push_back(make_tuple(test, bb, getRegisters()));
    run = 0;
  }

  int branchnumber = 0;
  // jnz and jne
  void lift_jnz(IRBuilder<>& builder, ZydisDisassembledInstruction& instruction,
                shared_ptr<vector<BBInfo>> blockAddresses) {

    LLVMContext& context = builder.getContext();

    auto zf = getFlag(builder, FLAG_ZF);

    // auto dest = instruction.operands[0];

    // auto Value = GetOperandValue(builder, dest, 64);
    // auto ripval = GetRegisterValue(builder, ZYDIS_REGISTER_RIP);

    // auto newRip = createAddFolder(builder, Value, ripval, "jnz");

    printvalue(zf);

    zf = createICMPFolder(builder, CmpInst::ICMP_EQ, zf,
                          ConstantInt::get(Type::getInt1Ty(context), 0));

    branchHelper(builder, instruction, blockAddresses, zf, "jnz", branchnumber);

    branchnumber++;
  }

  void lift_js(IRBuilder<>& builder, ZydisDisassembledInstruction& instruction,
               shared_ptr<vector<BBInfo>> blockAddresses) {

    auto sf = getFlag(builder, FLAG_SF);

    // auto dest = instruction.operands[0];

    // auto Value = GetOperandValue(builder, dest, 64);
    // auto ripval = GetRegisterValue(builder, ZYDIS_REGISTER_RIP);

    // auto newRip = createAddFolder(builder, Value, ripval, "js");

    branchHelper(builder, instruction, blockAddresses, sf, "js", branchnumber);

    branchnumber++;
  }
  void lift_jns(IRBuilder<>& builder, ZydisDisassembledInstruction& instruction,
                shared_ptr<vector<BBInfo>> blockAddresses) {

    auto sf = getFlag(builder, FLAG_SF);

    // auto dest = instruction.operands[0];

    // auto Value = GetOperandValue(builder, dest, 64);
    // auto ripval = GetRegisterValue(builder, ZYDIS_REGISTER_RIP);

    // auto newRip = createAddFolder(builder, Value, ripval, "jns");

    sf = builder.CreateNot(sf);

    branchHelper(builder, instruction, blockAddresses, sf, "jns", branchnumber);

    branchnumber++;
  }

  void lift_jz(IRBuilder<>& builder, ZydisDisassembledInstruction& instruction,
               shared_ptr<vector<BBInfo>> blockAddresses) {

    // if 0, then jmp, if not then not jump

    auto zf = getFlag(builder, FLAG_ZF);

    // auto dest = instruction.operands[0];

    // auto Value = GetOperandValue(builder, dest, 64);
    // auto ripval = GetRegisterValue(builder, ZYDIS_REGISTER_RIP);

    // auto newRip = createAddFolder(builder, Value, ripval, "jnz");

    branchHelper(builder, instruction, blockAddresses, zf, "jz", branchnumber);

    branchnumber++;
  }

  void lift_jle(IRBuilder<>& builder, ZydisDisassembledInstruction& instruction,
                shared_ptr<vector<BBInfo>> blockAddresses) {
    // If SF != OF or ZF = 1, then jump. Otherwise, do not jump.

    auto sf = getFlag(builder, FLAG_SF);
    auto of = getFlag(builder, FLAG_OF);
    auto zf = getFlag(builder, FLAG_ZF);

    // auto dest = instruction.operands[0];
    // auto Value = GetOperandValue(builder, dest, 64);
    // auto ripval = GetRegisterValue(builder, ZYDIS_REGISTER_RIP);
    // auto newRip = createAddFolder(builder, Value, ripval, "jle");

    // Check if SF != OF or ZF is set
    auto sf_neq_of = createXorFolder(builder, sf, of, "jle_SF_NEQ_OF");
    auto condition = createOrFolder(builder, sf_neq_of, zf, "jle_Condition");

    branchHelper(builder, instruction, blockAddresses, condition, "jle",
                 branchnumber);

    branchnumber++;
  }

  void lift_jl(IRBuilder<>& builder, ZydisDisassembledInstruction& instruction,
               shared_ptr<vector<BBInfo>> blockAddresses) {
    auto sf = getFlag(builder, FLAG_SF);
    auto of = getFlag(builder, FLAG_OF);

    // auto dest = instruction.operands[0];
    // auto Value = GetOperandValue(builder, dest, 64);
    // auto ripval = GetRegisterValue(builder, ZYDIS_REGISTER_RIP);
    // auto newRip = createAddFolder(builder, Value, ripval, "jl");
    printvalue(sf);
    printvalue(of);
    auto condition = createXorFolder(builder, sf, of, "jl_Condition");

    branchHelper(builder, instruction, blockAddresses, condition, "jl",
                 branchnumber);

    branchnumber++;
  }
  void lift_jnl(IRBuilder<>& builder, ZydisDisassembledInstruction& instruction,
                shared_ptr<vector<BBInfo>> blockAddresses) {
    auto sf = getFlag(builder, FLAG_SF);
    auto of = getFlag(builder, FLAG_OF);

    // auto dest = instruction.operands[0];
    // auto Value = GetOperandValue(builder, dest, 64);
    // auto ripval = GetRegisterValue(builder, ZYDIS_REGISTER_RIP);
    // auto newRip = createAddFolder(builder, Value, ripval, "jnl");

    printvalue(sf);
    printvalue(of);

    auto condition =
        builder.CreateNot(createXorFolder(builder, sf, of), "jnl_Condition");

    branchHelper(builder, instruction, blockAddresses, condition, "jnl",
                 branchnumber);

    branchnumber++;
  }

  void lift_jnle(IRBuilder<>& builder,
                 ZydisDisassembledInstruction& instruction,
                 shared_ptr<vector<BBInfo>> blockAddresses) {

    auto sf = getFlag(builder, FLAG_SF);
    auto of = getFlag(builder, FLAG_OF);
    auto zf = getFlag(builder, FLAG_ZF);

    // auto dest = instruction.operands[0];
    // auto Value = GetOperandValue(builder, dest, 64);
    // auto ripval = GetRegisterValue(builder, ZYDIS_REGISTER_RIP);
    // auto newRip = createAddFolder(builder, Value, ripval, "jnle");
    //  Jump short if greater (ZF=0 and SF=OF).
    printvalue(sf) printvalue(zf) printvalue(of)

        auto sf_eq_of = createXorFolder(builder, sf, of); // 0-0 or 1-1 => 0
    auto sf_eq_of_not =
        builder.CreateNot(sf_eq_of, "jnle_SF_EQ_OF_NOT"); // 0 => 1
    auto zf_not = builder.CreateNot(zf, "jnle_ZF_NOT");   // zf == 0
    auto condition =
        createAndFolder(builder, sf_eq_of_not, zf_not, "jnle_Condition");

    branchHelper(builder, instruction, blockAddresses, condition, "jnle",
                 branchnumber);

    branchnumber++;
  }

  void lift_jbe(IRBuilder<>& builder, ZydisDisassembledInstruction& instruction,
                shared_ptr<vector<BBInfo>> blockAddresses) {

    auto cf = getFlag(builder, FLAG_CF);
    auto zf = getFlag(builder, FLAG_ZF);
    printvalue(cf) printvalue(zf) // auto dest = instruction.operands[0];

        // auto Value = GetOperandValue(builder, dest, 64);
        // auto ripval = GetRegisterValue(builder, ZYDIS_REGISTER_RIP);
        // auto newRip = createAddFolder(builder, Value, ripval, "jbe");

        auto condition = createOrFolder(builder, cf, zf, "jbe_Condition");

    branchHelper(builder, instruction, blockAddresses, condition, "jbe",
                 branchnumber);

    branchnumber++;
  }

  void lift_jb(IRBuilder<>& builder, ZydisDisassembledInstruction& instruction,
               shared_ptr<vector<BBInfo>> blockAddresses) {

    auto cf = getFlag(builder, FLAG_CF);
    printvalue(cf);
    // auto dest = instruction.operands[0];

    // auto Value = GetOperandValue(builder, dest, 64);
    // auto ripval = GetRegisterValue(builder, ZYDIS_REGISTER_RIP);
    // auto newRip = createAddFolder(builder, Value, ripval, "jb");

    auto condition = cf;
    branchHelper(builder, instruction, blockAddresses, condition, "jb",
                 branchnumber);

    branchnumber++;
  }

  void lift_jnb(IRBuilder<>& builder, ZydisDisassembledInstruction& instruction,
                shared_ptr<vector<BBInfo>> blockAddresses) {

    auto cf = getFlag(builder, FLAG_CF);
    printvalue(cf);
    // auto dest = instruction.operands[0];

    // auto Value = GetOperandValue(builder, dest, 64);
    // auto ripval = GetRegisterValue(builder, ZYDIS_REGISTER_RIP);
    // auto newRip = createAddFolder(builder, Value, ripval, "jnb");

    auto condition = builder.CreateNot(cf, "notCF");
    branchHelper(builder, instruction, blockAddresses, condition, "jnb",
                 branchnumber);

    branchnumber++;
  }

  void lift_jnbe(IRBuilder<>& builder,
                 ZydisDisassembledInstruction& instruction,
                 shared_ptr<vector<BBInfo>> blockAddresses) {

    auto cf = getFlag(builder, FLAG_CF);
    auto zf = getFlag(builder, FLAG_ZF);

    printvalue(cf);
    printvalue(zf);
    // auto dest = instruction.operands[0];

    // auto Value = GetOperandValue(builder, dest, 64);
    // auto ripval = GetRegisterValue(builder, ZYDIS_REGISTER_RIP);
    // auto newRip = createAddFolder(builder, Value, ripval, "jnbe");

    auto condition =
        createAndFolder(builder, builder.CreateNot(cf, "notCF"),
                        builder.CreateNot(zf, "notZF"), "jnbe_ja_Condition");

    branchHelper(builder, instruction, blockAddresses, condition, "jnbe_ja",
                 branchnumber);

    branchnumber++;
  }

  void lift_jo(IRBuilder<>& builder, ZydisDisassembledInstruction& instruction,
               shared_ptr<vector<BBInfo>> blockAddresses) {

    auto of = getFlag(builder, FLAG_OF);

    // auto dest = instruction.operands[0];

    // auto Value = GetOperandValue(builder, dest, 64);
    // auto ripval = GetRegisterValue(builder, ZYDIS_REGISTER_RIP);

    // auto newRip = createAddFolder(builder, Value, ripval, "jo");

    printvalue(of);
    branchHelper(builder, instruction, blockAddresses, of, "jo", branchnumber);

    branchnumber++;
  }

  void lift_jno(IRBuilder<>& builder, ZydisDisassembledInstruction& instruction,
                shared_ptr<vector<BBInfo>> blockAddresses) {

    auto of = getFlag(builder, FLAG_OF);

    // auto dest = instruction.operands[0];

    // auto Value = GetOperandValue(builder, dest, 64);
    // auto ripval = GetRegisterValue(builder, ZYDIS_REGISTER_RIP);

    // auto newRip = createAddFolder(builder, Value, ripval, "jno");

    of = builder.CreateNot(of);
    branchHelper(builder, instruction, blockAddresses, of, "jno", branchnumber);

    branchnumber++;
  }

  void lift_jp(IRBuilder<>& builder, ZydisDisassembledInstruction& instruction,
               shared_ptr<vector<BBInfo>> blockAddresses) {

    auto pf = getFlag(builder, FLAG_PF);
    printvalue(pf);
    // auto dest = instruction.operands[0];

    // auto Value = GetOperandValue(builder, dest, 64);
    // auto ripval = GetRegisterValue(builder, ZYDIS_REGISTER_RIP);

    // auto newRip = createAddFolder(builder, Value, ripval, "jp");

    branchHelper(builder, instruction, blockAddresses, pf, "jp", branchnumber);

    branchnumber++;
  }

  void lift_jnp(IRBuilder<>& builder, ZydisDisassembledInstruction& instruction,
                shared_ptr<vector<BBInfo>> blockAddresses) {

    auto pf = getFlag(builder, FLAG_PF);

    // auto dest = instruction.operands[0];

    // auto Value = GetOperandValue(builder, dest, 64);
    // auto ripval = GetRegisterValue(builder, ZYDIS_REGISTER_RIP);

    // auto newRip = createAddFolder(builder, Value, ripval, "jnp");

    pf = builder.CreateNot(pf);
    printvalue(pf);
    branchHelper(builder, instruction, blockAddresses, pf, "jnp", branchnumber);

    branchnumber++;
  }

} // namespace branches

namespace arithmeticsAndLogical {
  void lift_sbb(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {

    //

    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    Value* Lvalue = GetOperandValue(builder, dest, dest.size);
    Value* Rvalue = GetOperandValue(builder, src, dest.size);
    Value* cf = createZExtOrTruncFolder(builder, getFlag(builder, FLAG_CF),
                                        Rvalue->getType());

    Value* srcPlusCF = createAddFolder(builder, Rvalue, cf, "srcPlusCF");
    Value* tmpResult =
        createSubFolder(builder, Lvalue, srcPlusCF, "sbbTempResult");
    SetOperandValue(builder, dest, tmpResult);

    Value* newCF = createICMPFolder(builder, CmpInst::ICMP_ULT, Lvalue,
                                    srcPlusCF, "newCF");
    Value* sf = computeSignFlag(builder, tmpResult);
    Value* zf = computeZeroFlag(builder, tmpResult);
    Value* pf = computeParityFlag(builder, tmpResult);
    Value* af = computeAuxFlagSbb(builder, Lvalue, Rvalue, cf);

    auto of = computeOverflowFlagSbb(builder, Lvalue, Rvalue, cf, tmpResult);

    setFlag(builder, FLAG_CF, newCF);
    setFlag(builder, FLAG_SF, sf);
    setFlag(builder, FLAG_ZF, zf);
    setFlag(builder, FLAG_PF, pf);
    setFlag(builder, FLAG_AF, af);
    setFlag(builder, FLAG_OF, of);
    printvalue(Lvalue);
    printvalue(Rvalue);
    printvalue(tmpResult);
    printvalue(sf);
    printvalue(of);
  }

  /*

  (* RCL and RCR Instructions *)
  SIZE := OperandSize;
  CASE (determine count) OF
          SIZE := 8: tempCOUNT := (COUNT AND 1FH) MOD 9;
          SIZE := 16: tempCOUNT := (COUNT AND 1FH) MOD 17;
          SIZE := 32: tempCOUNT := COUNT AND 1FH;
          SIZE := 64: tempCOUNT := COUNT AND 3FH;
  ESAC;
  IF OperandSize = 64
          THEN COUNTMASK = 3FH;
          ELSE COUNTMASK = 1FH;
  FI;
  (* RCL Instruction Operation *)
  WHILE (tempCOUNT ≠ 0)
          DO
          tempCF := MSB(DEST);
          DEST := (DEST ∗ 2) + CF;
          CF := tempCF;
          tempCOUNT := tempCOUNT – 1;
          OD;
  ELIHW;
  IF (COUNT & COUNTMASK) = 1
          THEN OF := MSB(DEST) XOR CF;
          ELSE OF is undefined;
  FI;
  */

  void lift_rcl(IRBuilder<>& builder, ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto dest = instruction.operands[0];
    auto count = instruction.operands[1];

    Value* Lvalue = GetOperandValue(builder, dest, dest.size);
    Value* countValue = GetOperandValue(builder, count, dest.size);
    Value* carryFlag = getFlag(builder, FLAG_CF);

    unsigned long bitWidth = Lvalue->getType()->getIntegerBitWidth();
    unsigned maskC = bitWidth == 64 ? 0x3f : 0x1f;

    // Calculate actual count
    Value* countMask = ConstantInt::get(countValue->getType(), maskC);
    Value* actualCount = createAndFolder(builder, countValue, countMask, "actualCount");

    // Extend Lvalue to double width
    Type* wideType = Type::getIntNTy(context, dest.size * 2);
    Value* wideLvalue = createZExtFolder(builder, Lvalue, wideType);

    // Shift the carry flag into the LSB
    Value* cf_extended = createZExtFolder(builder, carryFlag, wideType);
    Value* shiftedInCF = createOrFolder(builder, wideLvalue, cf_extended, "shiftedincf");

    // Perform the rotation
    Value* shiftAmount = createZExtFolder(builder, actualCount, wideType);
    Value* bitWidthPlusOne = ConstantInt::get(wideType, bitWidth + 1);
    Value* inverseShiftAmount = createSubFolder(builder, bitWidthPlusOne, shiftAmount);

    Value* leftShifted = createShlFolder(builder, shiftedInCF, shiftAmount, "leftshifted");
    Value* rightShifted = createLShrFolder(builder, shiftedInCF, inverseShiftAmount, "rightshifted");
    Value* rotated = createOrFolder(builder, leftShifted, rightShifted);

    // Extract the result and new carry flag
    Value* result = createTruncFolder(builder, rotated, Lvalue->getType());
    Value* newCFShifted = createLShrFolder(builder, rotated, ConstantInt::get(wideType, bitWidth));
    Value* newCF = createTruncFolder(builder, newCFShifted, Type::getInt1Ty(context), "rclnewcf");

    // Calculate OF (only valid when count == 1)
    Value* resultMSB = createLShrFolder(builder, result, ConstantInt::get(result->getType(), bitWidth - 1));
    Value* msbAfterRotate = createTruncFolder(builder, resultMSB, Type::getInt1Ty(context), "rclmsbafterrotate");

    Value* one = ConstantInt::get(actualCount->getType(), 1);
    Value* isCountOne = createICMPFolder(builder, CmpInst::ICMP_EQ, actualCount, one);

    Value* newOF = createXorFolder(builder, newCF, msbAfterRotate);
    Value* currentOF = getFlag(builder, FLAG_OF);
    newOF = createSelectFolder(builder, isCountOne, newOF, currentOF);

    // Set the result and flags
    SetOperandValue(builder, dest, result);
    setFlag(builder, FLAG_CF, newCF);
    setFlag(builder, FLAG_OF, newOF);

    // Debug output
    printvalue(Lvalue);
    printvalue(countValue);
    printvalue(carryFlag);
    printvalue(actualCount);
    printvalue(shiftedInCF);
    printvalue(rotated);
    printvalue(result);
    printvalue(newCF);
    printvalue(newOF);
  }

  /*
          (* RCL and RCR Instructions *)
  SIZE := OperandSize;
  CASE (determine count) OF
          SIZE := 8: tempCOUNT := (COUNT AND 1FH) MOD 9;
          SIZE := 16: tempCOUNT := (COUNT AND 1FH) MOD 17;
          SIZE := 32: tempCOUNT := COUNT AND 1FH;
          SIZE := 64: tempCOUNT := COUNT AND 3FH;
  ESAC;
  IF OperandSize = 64
          THEN COUNTMASK = 3FH;
          ELSE COUNTMASK = 1FH;
  FI;
  (* RCR Instruction Operation *)
  IF (COUNT & COUNTMASK) = 1
          THEN OF := MSB(DEST) XOR CF;
          ELSE OF is undefined;
  FI;
  WHILE (tempCOUNT ≠ 0)
          DO
          tempCF := LSB(SRC);
          DEST := (DEST / 2) + (CF * 2SIZE);
          CF := tempCF;
          tempCOUNT := tempCOUNT – 1;
          OD;
  ELIHW;

  */
  void lift_rcr(IRBuilder<>& builder, ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto dest = instruction.operands[0];
    auto count = instruction.operands[1];

    Value* Lvalue = GetOperandValue(builder, dest, dest.size);
    Value* countValue = GetOperandValue(builder, count, dest.size);
    Value* carryFlag = getFlag(builder, FLAG_CF);

    unsigned long bitWidth = Lvalue->getType()->getIntegerBitWidth();
    unsigned maskC = bitWidth == 64 ? 0x3f : 0x1f;

    // Calculate actual count
    Value* countMask = ConstantInt::get(countValue->getType(), maskC);
    Value* actualCount = createAndFolder(builder, countValue, countMask, "actualCount");

    // Extend Lvalue to double width and shift left by 1 to make room for CF
    Type* wideType = Type::getIntNTy(context, dest.size * 2);
    Value* wideLvalue = createZExtFolder(builder, Lvalue, wideType);
    Value* shiftedLvalue =
        createShlFolder(builder, wideLvalue, ConstantInt::get(wideType, 1));

    // Insert the carry flag into the MSB
    Value* cf_extended = createZExtFolder(builder, carryFlag, wideType);
    Value* shiftedCF = createShlFolder(builder, cf_extended, ConstantInt::get(wideType, bitWidth));
    Value* shiftedInCF = createOrFolder(builder, shiftedLvalue, shiftedCF, "shiftedincf");

    // Perform the rotation
    Value* shiftAmount = createZExtFolder(builder, actualCount, wideType);
    Value* bitWidthPlusOne = ConstantInt::get(wideType, bitWidth + 1);
    Value* inverseShiftAmount = createSubFolder(builder, bitWidthPlusOne, shiftAmount);

    Value* rightShifted = createLShrFolder(builder, shiftedInCF, shiftAmount, "rightshifted");
    Value* leftShifted = createShlFolder(builder, shiftedInCF, inverseShiftAmount, "leftshifted");
    Value* rotated = createOrFolder(builder, rightShifted, leftShifted);

    // Extract the result and new carry flag
    Value* shiftedResult = createLShrFolder(builder, rotated, ConstantInt::get(wideType, 1));
    Value* result = createTruncFolder(builder, shiftedResult, Lvalue->getType());
    Value* newCF = createTruncFolder(builder, rotated, Type::getInt1Ty(context), "rcrnewcf");

    // Calculate OF (only valid when count == 1)
    Value* resultMSB = createLShrFolder(builder, result, ConstantInt::get(result->getType(), bitWidth - 1));
    Value* msbAfterRotate = createTruncFolder(builder, resultMSB, Type::getInt1Ty(context), "rcrmsbafterrotate");

    Value* LvalueMSB = createLShrFolder(builder, Lvalue, ConstantInt::get(Lvalue->getType(), bitWidth - 1));
    Value* msbBeforeRotate = createTruncFolder(builder, LvalueMSB, Type::getInt1Ty(context), "rcrmsbbeforerotate");

    Value* one = ConstantInt::get(actualCount->getType(), 1);
    Value* isCountOne = createICMPFolder(builder, CmpInst::ICMP_EQ, actualCount, one);

    Value* newOF = createXorFolder(builder, msbBeforeRotate, msbAfterRotate);
    Value* currentOF = getFlag(builder, FLAG_OF);
    newOF = createSelectFolder(builder, isCountOne, newOF, currentOF);

    // Set the result and flags
    SetOperandValue(builder, dest, result);
    setFlag(builder, FLAG_CF, newCF);
    setFlag(builder, FLAG_OF, newOF);

    // Debug output
    printvalue(Lvalue);
    printvalue(countValue);
    printvalue(carryFlag);
    printvalue(actualCount);
    printvalue(shiftedInCF);
    printvalue(rotated);
    printvalue(result);
    printvalue(newCF);
    printvalue(newOF);
  }

  void lift_not(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {

    auto dest = instruction.operands[0];

    auto Rvalue = GetOperandValue(builder, dest, dest.size);
    Rvalue = builder.CreateNot(
        Rvalue, "realnot-" + to_string(instruction.runtime_address) + "-");
    SetOperandValue(builder, dest, Rvalue,
                    to_string(instruction.runtime_address));

    printvalue(Rvalue);
    //  Flags Affected
    // None
  }

  void lift_neg(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {

    auto dest = instruction.operands[0];
    auto Rvalue = GetOperandValue(builder, dest, dest.size);

    auto cf = createICMPFolder(builder, CmpInst::ICMP_NE, Rvalue,
                               ConstantInt::get(Rvalue->getType(), 0), "cf");
    auto result = builder.CreateNeg(Rvalue, "neg");
    SetOperandValue(builder, dest, result);

    auto sf = computeSignFlag(builder, result);
    auto zf = computeZeroFlag(builder, result);
    auto pf = computeParityFlag(builder, result);
    Value* fifteen = ConstantInt::get(Rvalue->getType(), 0xf);
    auto af = createICMPFolder(builder, CmpInst::ICMP_NE,
                               createAndFolder(builder, Rvalue, fifteen),
                               ConstantInt::get(Rvalue->getType(), 0), "af");
    auto isZero =
        createICMPFolder(builder, CmpInst::ICMP_NE, Rvalue,
                         ConstantInt::get(Rvalue->getType(), 0), "zero");

    printvalue(Rvalue) printvalue(result) printvalue(sf);
    // OF is not cleared?

    Value* of;
    if (dest.size > 32)
      of = ConstantInt::getSigned(Rvalue->getType(), 0);
    else {
      of = createICMPFolder(builder, CmpInst::ICMP_EQ, result, Rvalue);
      of = createSelectFolder(builder, isZero, of,
                              ConstantInt::get(of->getType(), 0));
    }

    printvalue(of);
    // The CF flag set to 0 if the source operand is 0; otherwise it is set
    // to 1. The OF, SF, ZF, AF, and PF flags are set according to the
    // result.
    setFlag(builder, FLAG_CF, cf);
    setFlag(builder, FLAG_SF, sf);
    setFlag(builder, FLAG_ZF, zf);
    setFlag(builder, FLAG_PF, pf);
    setFlag(builder, FLAG_OF, of);
    setFlag(builder, FLAG_AF, af);
  }

  /*

  IF 64-Bit Mode and using REX.W
          THEN
                  countMASK := 3FH;
          ELSE
                  countMASK := 1FH;
  FI
  tempCOUNT := (COUNT AND countMASK);
  tempDEST := DEST;
  WHILE (tempCOUNT ≠ 0)
  DO
          IF instruction is SAL or SHL
                  THEN
                  CF := MSB(DEST);
          ELSE (* Instruction is SAR or SHR *)
                  CF := LSB(DEST);
          FI;
          IF instruction is SAL or SHL
                  THEN
                          DEST := DEST ∗ 2;
          ELSE
                  IF instruction is SAR
                          THEN
                                  DEST := DEST / 2; (* Signed divide, rounding
  toward negative infinity *) ELSE (* Instruction is SHR *) DEST := DEST / 2 ;
  (* Unsigned divide *) FI; FI; tempCOUNT := tempCOUNT – 1; OD;

  (* Determine overflow for the various instructions *)
  IF (COUNT and countMASK) = 1
          THEN
          IF instruction is SAL or SHL
                  THEN
                  OF := MSB(DEST) XOR CF;
          ELSE
          IF instruction is SAR
                  THEN
                  OF := 0;
          ELSE (* Instruction is SHR *)
                  OF := MSB(tempDEST);
          FI;
  FI;

  ELSE IF (COUNT AND countMASK) = 0
          THEN
          All flags unchanged;
  ELSE (* COUNT not 1 or 0 *)
  OF := undefined;
  FI;
  FI;

  */
  // maybe

  void lift_sar(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto dest = instruction.operands[0];
    auto count = instruction.operands[1];

    Value* Lvalue = GetOperandValue(builder, dest, dest.size,
                                    to_string(instruction.runtime_address));
    Value* countValue = GetOperandValue(builder, count, dest.size);

    Value* zero = ConstantInt::get(countValue->getType(), 0);
    uint8_t bitWidth = Lvalue->getType()->getIntegerBitWidth();
    uint8_t maskC = bitWidth == 64 ? 0x3f : 0x1f;

    Value* clampedCount = createAndFolder(
        builder, countValue, ConstantInt::get(countValue->getType(), maskC),
        "sarclamp");
    Value* result = builder.CreateAShr(
        Lvalue, clampedCount,
        "sar-lshr-" + to_string(instruction.runtime_address) + "-");

    Value* isZeroed = createICMPFolder(
        builder, CmpInst::ICMP_UGT, clampedCount,
        ConstantInt::get(clampedCount->getType(), bitWidth - 1));
    result = createSelectFolder(builder, isZeroed, zero, result);

    auto cfRvalue = createSubFolder(
        builder, clampedCount, ConstantInt::get(clampedCount->getType(), 1));
    auto cfShl = createShlFolder(
        builder, ConstantInt::get(cfRvalue->getType(), 1), cfRvalue);
    auto cfAnd = createAndFolder(builder, cfShl, Lvalue);
    auto cfValue = createICMPFolder(builder, CmpInst::ICMP_NE, cfAnd,
                                    ConstantInt::get(cfAnd->getType(), 0));

    Value* isCountOne =
        createICMPFolder(builder, CmpInst::ICMP_EQ, clampedCount,
                         ConstantInt::get(clampedCount->getType(), 1));
    Value* of = createSelectFolder(builder, isCountOne, builder.getInt1(0),
                                   getFlag(builder, FLAG_OF));

    Value* isNotZero =
        createICMPFolder(builder, CmpInst::ICMP_NE, clampedCount, zero);
    Value* oldcf = getFlag(builder, FLAG_CF);
    cfValue = createSelectFolder(builder, isNotZero, cfValue, oldcf);
    cfValue = createSelectFolder(
        builder, isZeroed, builder.CreateTrunc(zero, Type::getInt1Ty(context)),
        cfValue);

    Value* sf = computeSignFlag(builder, result);
    Value* zf = computeZeroFlag(builder, result);
    Value* pf = computeParityFlag(builder, result);
    printvalue(Lvalue) printvalue2(bitWidth) printvalue(countValue);
    printvalue(clampedCount) printvalue(result) printvalue(isNotZero);
    printvalue(cfValue) printvalue(oldcf);
    setFlag(builder, FLAG_CF, cfValue);
    setFlag(builder, FLAG_OF, of);
    setFlag(builder, FLAG_SF, sf);
    setFlag(builder, FLAG_ZF, zf);
    setFlag(builder, FLAG_PF, pf);

    SetOperandValue(builder, dest, result,
                    to_string(instruction.runtime_address));
    ;
  }
  // TODO fix
  void lift_shr(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto dest = instruction.operands[0];
    auto count = instruction.operands[1];

    Value* Lvalue = GetOperandValue(builder, dest, dest.size);
    Value* countValue = GetOperandValue(builder, count, dest.size);

    unsigned bitWidth = Lvalue->getType()->getIntegerBitWidth();
    unsigned maskC = bitWidth == 64 ? 0x3f : 0x1f;

    Value* clampedCount = createAndFolder(
        builder, countValue, ConstantInt::get(countValue->getType(), maskC),
        "shrclamp");

    Value* result = createLShrFolder(
        builder, Lvalue, clampedCount,
        "shr-lshr-" + to_string(instruction.runtime_address) + "-");
    Value* zero = ConstantInt::get(countValue->getType(), 0);
    Value* isZeroed = createICMPFolder(
        builder, CmpInst::ICMP_UGT, clampedCount,
        ConstantInt::get(clampedCount->getType(), bitWidth - 1));
    result = createSelectFolder(builder, isZeroed, zero, result, "shiftValue");

    Value* cfValue = builder.CreateTrunc(
        createLShrFolder(
            builder, Lvalue,
            createSubFolder(builder, clampedCount,
                            ConstantInt::get(clampedCount->getType(), 1)),
            "shrcf"),
        builder.getInt1Ty());

    Value* isCountOne =
        createICMPFolder(builder, CmpInst::ICMP_EQ, clampedCount,
                         ConstantInt::get(clampedCount->getType(), 1));
    Value* of = createICMPFolder(builder, CmpInst::ICMP_SLT, Lvalue,
                                 ConstantInt::get(Lvalue->getType(), 0));
    of = createSelectFolder(builder, isCountOne, of, getFlag(builder, FLAG_OF),
                            "of");

    Value* isNotZero =
        createICMPFolder(builder, CmpInst::ICMP_NE, clampedCount, zero);
    Value* oldcf = getFlag(builder, FLAG_CF);
    cfValue =
        createSelectFolder(builder, isNotZero, cfValue, oldcf, "cfValue1");
    cfValue = createSelectFolder(
        builder, isZeroed, builder.CreateTrunc(zero, Type::getInt1Ty(context)),
        cfValue, "cfValue2");
    Value* sf = computeSignFlag(builder, result);
    Value* zf = computeZeroFlag(builder, result);
    Value* pf = computeParityFlag(builder, result);
    printvalue(sf);
    printvalue(result);
    setFlag(builder, FLAG_CF, cfValue);
    setFlag(builder, FLAG_OF, of);
    setFlag(builder, FLAG_SF, sf);
    setFlag(builder, FLAG_ZF, zf);
    setFlag(builder, FLAG_PF, pf);

    printvalue(Lvalue) printvalue(clampedCount) printvalue(result)
        printvalue(isNotZero) printvalue(oldcf) printvalue(cfValue)
            SetOperandValue(builder, dest, result,
                            to_string(instruction.runtime_address));
  }

  void lift_shl(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto dest = instruction.operands[0];
    auto count = instruction.operands[1];

    Value* Lvalue = GetOperandValue(builder, dest, dest.size,
                                    to_string(instruction.runtime_address));
    Value* countValue = GetOperandValue(builder, count, dest.size);
    unsigned bitWidth = Lvalue->getType()->getIntegerBitWidth();
    unsigned maskC = bitWidth == 64 ? 0x3f : 0x1f;

    auto bitWidthValue = ConstantInt::get(countValue->getType(), bitWidth);

    Value* clampedCountValue = createAndFolder(
        builder, countValue, ConstantInt::get(countValue->getType(), maskC),
        "shlclamp");

    Value* result =
        createShlFolder(builder, Lvalue, clampedCountValue, "shl-shift");
    Value* zero = ConstantInt::get(countValue->getType(), 0);
    Value* isZeroed = createICMPFolder(
        builder, CmpInst::ICMP_UGT, clampedCountValue,
        ConstantInt::get(clampedCountValue->getType(), bitWidth - 1));
    result = createSelectFolder(builder, isZeroed, zero, result);

    Value* cfValue = createLShrFolder(
        builder, Lvalue,
        createSubFolder(builder, bitWidthValue, clampedCountValue), "shlcf");
    Value* one = ConstantInt::get(cfValue->getType(), 1);
    cfValue = createAndFolder(builder, cfValue, one, "shlcf");
    cfValue =
        createZExtOrTruncFolder(builder, cfValue, Type::getInt1Ty(context));

    auto countIsNotZero =
        createICMPFolder(builder, CmpInst::ICMP_NE, clampedCountValue,
                         ConstantInt::get(clampedCountValue->getType(), 0));

    auto cfRvalue =
        createSubFolder(builder, clampedCountValue,
                        ConstantInt::get(clampedCountValue->getType(), 1));
    auto cfShl = createShlFolder(builder, Lvalue, cfRvalue);
    auto cfIntT = cast<IntegerType>(cfShl->getType());
    auto cfRightCount = ConstantInt::get(cfIntT, cfIntT->getBitWidth() - 1);
    auto cfLow = createLShrFolder(builder, cfShl, cfRightCount, "lowcfshr");
    cfValue = createSelectFolder(
        builder, countIsNotZero,
        createZExtOrTruncFolder(builder, cfLow, Type::getInt1Ty(context)),
        getFlag(builder, FLAG_CF));
    cfValue = createSelectFolder(
        builder, isZeroed,
        createZExtOrTruncFolder(builder, zero, Type::getInt1Ty(context)),
        cfValue);

    Value* isCountOne =
        createICMPFolder(builder, CmpInst::ICMP_EQ, clampedCountValue,
                         ConstantInt::get(clampedCountValue->getType(), 1));

    Value* originalMSB = createLShrFolder(
        builder, Lvalue, ConstantInt::get(Lvalue->getType(), bitWidth - 1),
        "shlmsb");
    originalMSB = createAndFolder(
        builder, originalMSB, ConstantInt::get(Lvalue->getType(), 1), "shlmsb");
    originalMSB =
        createZExtOrTruncFolder(builder, originalMSB, Type::getInt1Ty(context));

    Value* cfAsMSB = createZExtOrTruncFolder(
        builder,
        createLShrFolder(builder, Lvalue,
                         ConstantInt::get(Lvalue->getType(), bitWidth - 1),
                         "shlcfasmsb"),
        Type::getInt1Ty(context));

    Value* resultMSB = createZExtOrTruncFolder(
        builder,
        createLShrFolder(builder, result,
                         ConstantInt::get(result->getType(), bitWidth - 1),
                         "shlresultmsb"),
        Type::getInt1Ty(context));

    Value* ofValue = createSelectFolder(
        builder, isCountOne, createXorFolder(builder, resultMSB, cfAsMSB),
        getFlag(builder, FLAG_OF));

    setFlag(builder, FLAG_CF, cfValue);
    setFlag(builder, FLAG_OF, ofValue);

    Value* sf = computeSignFlag(builder, result);
    Value* zf = computeZeroFlag(builder, result);
    Value* pf = computeParityFlag(builder, result);
    printvalue(Lvalue);
    printvalue(countValue);
    printvalue(clampedCountValue);
    printvalue(isCountOne);
    printvalue(result);
    printvalue(ofValue);
    printvalue(cfValue);
    setFlag(builder, FLAG_SF, sf);
    setFlag(builder, FLAG_ZF, zf);
    setFlag(builder, FLAG_PF, pf);

    SetOperandValue(builder, dest, result,
                    to_string(instruction.runtime_address));
  }

  void lift_bswap(IRBuilder<>& builder,
                  ZydisDisassembledInstruction& instruction) {
    auto dest = instruction.operands[0];

    auto Lvalue = GetOperandValue(builder, dest, dest.size);
    // if 16bit, 0 it
    if (dest.size == 16) {
      Value* zero = ConstantInt::get(Lvalue->getType(), 0);
      SetOperandValue(builder, dest, zero);
      return;
    }
    Value* newswappedvalue = ConstantInt::get(Lvalue->getType(), 0);
    Value* mask = ConstantInt::get(Lvalue->getType(), 0xff);
    for (unsigned i = 0; i < Lvalue->getType()->getIntegerBitWidth() / 8; i++) {
      // 0xff
      // b = a & 0xff >> 0
      // b = 0x78
      // nb |=  b << 24
      // nb |= 0x78000000
      // 0xff00
      // b = a & 0xff00 >> 8
      // b = 0x56
      // nb |= b << 16
      // nb = 0x78560000
      auto byte =
          createLShrFolder(builder, createAndFolder(builder, Lvalue, mask),
                           i * 8, "shlresultmsb");
      auto shiftby = Lvalue->getType()->getIntegerBitWidth() - (i + 1) * 8;
      auto newposbyte = createShlFolder(builder, byte, shiftby);
      newswappedvalue = createOrFolder(builder, newswappedvalue, newposbyte);
      mask = createShlFolder(builder, mask, 8);
    }

    SetOperandValue(builder, dest, newswappedvalue);
  }

  void lift_cmpxchg(IRBuilder<>& builder,
                    ZydisDisassembledInstruction& instruction) {

    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];
    auto accop = instruction.operands[2];

    auto Rvalue = GetOperandValue(builder, src, src.size);
    auto Lvalue = GetOperandValue(builder, dest, dest.size);
    auto accum = GetOperandValue(builder, accop, dest.size);

    auto sub = builder.CreateSub(accum, Lvalue);

    auto of = computeOverflowFlagSub(builder, Lvalue, Rvalue, sub);

    auto lowerNibbleMask = ConstantInt::get(Lvalue->getType(), 0xF);
    auto RvalueLowerNibble =
        createAndFolder(builder, Lvalue, lowerNibbleMask, "lvalLowerNibble");
    auto op2LowerNibble =
        createAndFolder(builder, Rvalue, lowerNibbleMask, "rvalLowerNibble");

    auto cf =
        createICMPFolder(builder, CmpInst::ICMP_UGT, Rvalue, Lvalue, "add_cf");
    auto af = createICMPFolder(builder, CmpInst::ICMP_ULT, RvalueLowerNibble,
                               op2LowerNibble, "add_af");

    auto sf = computeSignFlag(builder, sub);

    /*
    TEMP := DEST
    IF accumulator = TEMP
            THEN
                    ZF := 1;
                    DEST := SRC;
            ELSE
                    ZF := 0;
                    accumulator := TEMP;
                    DEST := TEMP;
    FI;
    */
    auto zf = createICMPFolder(builder, CmpInst::ICMP_EQ, accum, Lvalue);
    // if zf dest = src
    auto result = createSelectFolder(builder, zf, Rvalue, Lvalue);

    SetOperandValue(builder, dest, result);
    setFlag(builder, FLAG_OF, of);
    setFlag(builder, FLAG_CF, cf);
    setFlag(builder, FLAG_AF, af);
    setFlag(builder, FLAG_SF, sf);
    setFlag(builder, FLAG_ZF, zf);
  }

  void lift_xchg(IRBuilder<>& builder,
                 ZydisDisassembledInstruction& instruction) {

    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    auto Rvalue = GetOperandValue(builder, src, src.size);
    auto Lvalue = GetOperandValue(builder, dest, dest.size);

    printvalue(Lvalue) printvalue(Rvalue)

        SetOperandValue(builder, dest, Rvalue,
                        to_string(instruction.runtime_address));
    ;
    SetOperandValue(builder, src, Lvalue);
  }

  void lift_shld(IRBuilder<>& builder,
                 ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto dest = instruction.operands[0];
    auto source = instruction.operands[1];
    auto count = instruction.operands[2];

    auto Lvalue = GetOperandValue(builder, dest, dest.size);
    auto sourceValue = GetOperandValue(builder, source, dest.size);
    auto countValue = GetOperandValue(builder, count, dest.size);

    unsigned bitWidth = Lvalue->getType()->getIntegerBitWidth();
    auto effectiveCountValue = builder.CreateURem(
        countValue, ConstantInt::get(countValue->getType(), bitWidth),
        "effectiveShiftCount");

    auto shiftedDest =
        createShlFolder(builder, Lvalue, effectiveCountValue, "shiftedDest");
    auto complementCount = createSubFolder(
        builder, ConstantInt::get(countValue->getType(), bitWidth),
        effectiveCountValue, "complementCount");
    auto shiftedSource = createLShrFolder(builder, sourceValue, complementCount,
                                          "shiftedSource");
    auto resultValue =
        createOrFolder(builder, shiftedDest, shiftedSource, "shldResult");

    auto countIsNotZero =
        createICMPFolder(builder, CmpInst::ICMP_NE, effectiveCountValue,
                         ConstantInt::get(effectiveCountValue->getType(), 0));
    auto lastShiftedBitPosition =
        createSubFolder(builder, effectiveCountValue,
                        ConstantInt::get(effectiveCountValue->getType(), 1));
    auto lastShiftedBit = createAndFolder(
        builder, createLShrFolder(builder, Lvalue, lastShiftedBitPosition),
        ConstantInt::get(Lvalue->getType(), 1), "shldresultmsb");
    auto cf =
        createSelectFolder(builder, countIsNotZero,
                           createZExtOrTruncFolder(builder, lastShiftedBit,
                                                   Type::getInt1Ty(context)),
                           getFlag(builder, FLAG_CF));
    resultValue =
        createSelectFolder(builder, countIsNotZero, resultValue, Lvalue);

    auto isOne =
        createICMPFolder(builder, CmpInst::ICMP_EQ, effectiveCountValue,
                         ConstantInt::get(effectiveCountValue->getType(), 1));
    auto newOF = createXorFolder(
        builder,
        createLShrFolder(builder, Lvalue,
                         ConstantInt::get(Lvalue->getType(), bitWidth - 1),
                         "subof"),
        createLShrFolder(builder, resultValue,
                         ConstantInt::get(resultValue->getType(), bitWidth - 1),
                         "subof2"),
        "subxorof");
    auto of = createSelectFolder(
        builder, isOne,
        createZExtOrTruncFolder(builder, newOF, Type::getInt1Ty(context)),
        getFlag(builder, FLAG_OF));

    //  CF := BIT[DEST, SIZE – COUNT]; if shifted,
    setFlag(builder, FLAG_CF, cf);
    setFlag(builder, FLAG_OF, of);

    SetOperandValue(builder, dest, resultValue,
                    to_string(instruction.runtime_address));
  }

  void lift_shrd(IRBuilder<>& builder,
                 ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto dest = instruction.operands[0];
    auto source = instruction.operands[1];
    auto count = instruction.operands[2];

    auto Lvalue = GetOperandValue(builder, dest, dest.size);
    auto sourceValue = GetOperandValue(builder, source, dest.size);
    auto countValue = GetOperandValue(builder, count, dest.size);

    unsigned bitWidth = Lvalue->getType()->getIntegerBitWidth();
    auto effectiveCountValue = builder.CreateURem(
        countValue, ConstantInt::get(countValue->getType(), bitWidth),
        "effectiveShiftCount");

    auto shiftedDest =
        createLShrFolder(builder, Lvalue, effectiveCountValue, "shiftedDest");
    auto complementCount = createSubFolder(
        builder, ConstantInt::get(countValue->getType(), bitWidth),
        effectiveCountValue, "complementCount");
    auto shiftedSource =
        createShlFolder(builder, sourceValue, complementCount, "shiftedSource");
    auto resultValue =
        createOrFolder(builder, shiftedDest, shiftedSource, "shrdResult");

    // Calculate CF
    auto cfBitPosition =
        createSubFolder(builder, effectiveCountValue,
                        ConstantInt::get(effectiveCountValue->getType(), 1));
    Value* cf = createLShrFolder(builder, Lvalue, cfBitPosition);
    cf = createAndFolder(builder, cf, ConstantInt::get(cf->getType(), 1),
                         "shrdcf");
    cf = createZExtOrTruncFolder(builder, cf, Type::getInt1Ty(context));

    // Calculate OF, only when count is 1
    Value* isCountOne =
        createICMPFolder(builder, CmpInst::ICMP_EQ, effectiveCountValue,
                         ConstantInt::get(effectiveCountValue->getType(), 1));
    Value* mostSignificantBitOfDest = createLShrFolder(
        builder, Lvalue, ConstantInt::get(Lvalue->getType(), bitWidth - 1),
        "shlmsbdest");
    mostSignificantBitOfDest = createAndFolder(
        builder, mostSignificantBitOfDest,
        ConstantInt::get(mostSignificantBitOfDest->getType(), 1), "shrdmsb");
    Value* mostSignificantBitOfResult = createLShrFolder(
        builder, resultValue,
        ConstantInt::get(resultValue->getType(), bitWidth - 1), "shlmsbresult");
    mostSignificantBitOfResult = createAndFolder(
        builder, mostSignificantBitOfResult,
        ConstantInt::get(mostSignificantBitOfResult->getType(), 1), "shrdmsb2");
    Value* of = createXorFolder(builder, mostSignificantBitOfDest,
                                mostSignificantBitOfResult);
    of = createZExtOrTruncFolder(builder, of, Type::getInt1Ty(context));
    of = createSelectFolder(builder, isCountOne, of,
                            ConstantInt::getFalse(context));
    of = createZExtFolder(builder, of, Type::getInt1Ty(context));

    setFlag(builder, FLAG_CF, cf);
    setFlag(builder, FLAG_OF, of);

    SetOperandValue(builder, dest, resultValue,
                    to_string(instruction.runtime_address));
  }

  void lift_lea(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {

    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    auto Rvalue = GetEffectiveAddress(builder, src, dest.size);

    printvalue(Rvalue)

        SetOperandValue(builder, dest, Rvalue,
                        to_string(instruction.runtime_address));
    ;
  }

  // extract sub from this function, this is convoluted for no reason
  void lift_add_sub(IRBuilder<>& builder,
                    ZydisDisassembledInstruction& instruction) {
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    auto Rvalue = GetOperandValue(builder, src, dest.size);
    auto Lvalue = GetOperandValue(builder, dest, dest.size);

    Value* result = nullptr;
    Value* cf = nullptr;
    Value* af = nullptr;
    Value* of = nullptr;

    auto lowerNibbleMask = ConstantInt::get(Lvalue->getType(), 0xF);
    auto RvalueLowerNibble =
        createAndFolder(builder, Lvalue, lowerNibbleMask, "lvalLowerNibble");
    auto op2LowerNibble =
        createAndFolder(builder, Rvalue, lowerNibbleMask, "rvalLowerNibble");

    switch (instruction.info.mnemonic) {
    case ZYDIS_MNEMONIC_ADD: {
      result = createAddFolder(
          builder, Lvalue, Rvalue,
          "realadd-" + to_string(instruction.runtime_address) + "-");
      cf = createOrFolder(builder,
                          createICMPFolder(builder, CmpInst::ICMP_ULT, result,
                                           Lvalue, "add_cf1"),
                          createICMPFolder(builder, CmpInst::ICMP_ULT, result,
                                           Rvalue, "add_cf2"),
                          "add_cf");
      auto sumLowerNibble = createAddFolder(
          builder, RvalueLowerNibble, op2LowerNibble, "add_sumLowerNibble");
      af = createICMPFolder(builder, CmpInst::ICMP_UGT, sumLowerNibble,
                            lowerNibbleMask, "add_af");
      of = computeOverflowFlagAdd(builder, Lvalue, Rvalue, result);
      break;
    }
    case ZYDIS_MNEMONIC_SUB: {
      result = createSubFolder(
          builder, Lvalue, Rvalue,
          "realsub-" + to_string(instruction.runtime_address) + "-");

      of = computeOverflowFlagSub(builder, Lvalue, Rvalue, result);

      cf = createICMPFolder(builder, CmpInst::ICMP_UGT, Rvalue, Lvalue,
                            "add_cf");
      af = createICMPFolder(builder, CmpInst::ICMP_ULT, RvalueLowerNibble,
                            op2LowerNibble, "add_af");
      break;
    }
    default:
      break;
    }

    /*
    Flags Affected
    The OF, SF, ZF, AF, CF, and PF flags are set according to the result.
    */

    auto sf = computeSignFlag(builder, result);
    auto zf = computeZeroFlag(builder, result);
    auto pf = computeParityFlag(builder, result);

    setFlag(builder, FLAG_OF, of);
    setFlag(builder, FLAG_SF, sf);
    setFlag(builder, FLAG_ZF, zf);
    setFlag(builder, FLAG_AF, af);
    setFlag(builder, FLAG_CF, cf);
    setFlag(builder, FLAG_PF, pf);

    printvalue(Lvalue);
    printvalue(Rvalue);
    printvalue(result);
    printvalue(cf);
    printvalue(sf);
    printvalue(of);

    SetOperandValue(builder, dest, result);
  }

  void lift_imul2(IRBuilder<>& builder,
                  ZydisDisassembledInstruction& instruction, bool isSigned) {
    LLVMContext& context = builder.getContext();
    auto src = instruction.operands[0];
    auto Rvalue = GetRegisterValue(builder, ZYDIS_REGISTER_AL);

    Value* Lvalue = GetOperandValue(builder, src, src.size);
    if (isSigned) { // do this in a prettier way
      Lvalue =
          builder.CreateSExt(Lvalue, Type::getIntNTy(context, src.size * 2));

      Rvalue = builder.CreateSExtOrTrunc(
          Rvalue, Type::getIntNTy(context,
                                  src.size)); // make sure the size is correct,
                                              // 1 byte, GetRegisterValue doesnt
                                              // ensure we have the correct size
      Rvalue = builder.CreateSExtOrTrunc(Rvalue, Lvalue->getType());
    } else {
      Lvalue = createZExtFolder(builder, Lvalue,
                                Type::getIntNTy(context, src.size * 2));

      Rvalue = createZExtOrTruncFolder(
          builder, Rvalue,
          Type::getIntNTy(context,
                          src.size)); // make sure the size is correct, 1
                                      // byte, GetRegisterValue doesnt
                                      // ensure we have the correct size
      Rvalue = createZExtOrTruncFolder(builder, Rvalue, Lvalue->getType());
    }
    Value* result = builder.CreateMul(Rvalue, Lvalue);
    Value* lowerresult = builder.CreateTrunc(
        result, Type::getIntNTy(context, src.size), "lowerResult");
    Value* of;
    Value* cf;
    if (isSigned) {
      of = builder.CreateICmpNE(
          result, builder.CreateSExt(lowerresult, result->getType()));
      cf = of;
    } else {
      Value* highPart = builder.CreateLShr(result, src.size, "highPart");
      Value* highPartTruncated = builder.CreateTrunc(
          highPart, Type::getIntNTy(context, src.size), "truncatedHighPart");
      cf = builder.CreateICmpNE(highPartTruncated,
                                ConstantInt::get(result->getType(), 0), "cf");
      of = cf;
    }

    setFlag(builder, FLAG_CF, cf);
    setFlag(builder, FLAG_OF, of);
    printvalue(cf);
    printvalue(of);
    SetRegisterValue(builder, ZYDIS_REGISTER_AX, result);
    printvalue(Lvalue);
    printvalue(Rvalue);
    printvalue(result);
    // if imul modify cf and of flags
    // if not, dont do anything else
  }

  // TODO rewrite this
  void lift_imul(IRBuilder<>& builder,
                 ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();

    auto dest = instruction.operands[0]; // dest ?
    if (dest.size == 8 && instruction.info.operand_count_visible == 1) {
      lift_imul2(builder, instruction, 1);
      return;
    }
    auto src = instruction.operands[1];
    auto src2 = (instruction.info.operand_count_visible == 3)
                    ? instruction.operands[2]
                    : dest; // if exists third operand

    Value* Rvalue = GetOperandValue(builder, src, src.size);
    Value* Lvalue = GetOperandValue(builder, src2, src2.size);
    uint8_t initialSize = src.size;
    printvalue2(initialSize);
    printvalue(Rvalue);
    printvalue(Lvalue);
    Rvalue =
        builder.CreateSExt(Rvalue, Type::getIntNTy(context, initialSize * 2));
    Lvalue =
        builder.CreateSExt(Lvalue, Type::getIntNTy(context, initialSize * 2));

    Value* result = builder.CreateMul(Lvalue, Rvalue, "intmul");

    // Flags

    Value* highPart = builder.CreateLShr(result, initialSize, "highPart");
    Value* highPartTruncated = builder.CreateTrunc(
        highPart, Type::getIntNTy(context, initialSize), "truncatedHighPart");

    /*
    For the one operand form of the instruction, the CF and OF flags are set
    when significant bits are carried into the upper half of the result and
    cleared when the result fits exactly in the lower half of the result.
    For the two- and three-operand forms of the instruction, the CF and OF
    flags are set when the result must be truncated to fit in the
    destination operand size and cleared when the result fits exactly in the
    destination operand size. The SF, ZF, AF, and PF flags are undefined.
    */

    /*
    DEST := TruncateToOperandSize(TMP_XP);
    IF SignExtend(DEST) ≠ TMP_XP
    THEN CF := 1; OF := 1;
            ELSE CF := 0; OF := 0; FI;
    */

    Value* truncresult = builder.CreateTrunc(
        result, Type::getIntNTy(context, initialSize), "truncRes");

    Value* cf = builder.CreateICmpNE(
        result, builder.CreateSExt(truncresult, result->getType()), "cf");
    Value* of = cf;

    if (instruction.info.operand_count_visible == 3) {
      SetOperandValue(builder, dest, truncresult);
    } else if (instruction.info.operand_count_visible == 2) {
      SetOperandValue(builder, instruction.operands[0], truncresult);
    } else { // For one operand, result goes into ?dx:?ax if not a byte
             // operation
      auto splitResult = builder.CreateTruncOrBitCast(
          result, Type::getIntNTy(context, initialSize), "splitResult");
      Value* SEsplitResult = builder.CreateSExt(splitResult, result->getType());
      printvalue(splitResult);
      printvalue(result);
      cf = builder.CreateICmpNE(SEsplitResult, result);
      of = cf;
      printvalue(of);
      printvalue(result);
      printvalue(SEsplitResult);

      if (initialSize == 8) {
        SetOperandValue(builder, instruction.operands[1], result);
      } else {

        SetOperandValue(builder, instruction.operands[1], splitResult);
        SetOperandValue(builder, instruction.operands[2], highPartTruncated);
      }
    }

    setFlag(builder, FLAG_CF, cf);
    setFlag(builder, FLAG_OF, of);
    printvalue(Lvalue) printvalue(Rvalue) printvalue(result)
        printvalue(highPartTruncated) printvalue(of) printvalue(cf)
  }
  // rewrite this too
  void lift_mul(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {
    /*
    mul rdx
    [0] rdx
    [1] rax
    [2] rdx
    [3] flags
    */
    /*
    IF (Byte operation)
            THEN
                    AX := AL ∗ SRC;
            ELSE (* Word or doubleword operation *)
                    IF OperandSize = 16
                            THEN
                                    DX:AX := AX ∗ SRC;
                            ELSE IF OperandSize = 32
                                    THEN EDX:EAX := EAX ∗ SRC; FI;
                            ELSE (* OperandSize = 64 *)
                                    RDX:RAX := RAX ∗ SRC;
                    FI;
    FI;
    */

    LLVMContext& context = builder.getContext();
    auto src = instruction.operands[0];

    if (src.size == 8 && instruction.info.operand_count_visible == 1) {
      lift_imul2(builder, instruction, 0);
      return;
    }
    auto dest1 = instruction.operands[1]; // ax
    auto dest2 = instruction.operands[2];

    Value* Rvalue = GetOperandValue(builder, src, dest1.size);
    Value* Lvalue = GetOperandValue(builder, dest1, dest1.size);

    uint8_t initialSize = Rvalue->getType()->getIntegerBitWidth();
    printvalue2(initialSize);
    Rvalue = createZExtFolder(builder, Rvalue,
                              Type::getIntNTy(context, initialSize * 2));
    Lvalue = createZExtFolder(builder, Lvalue,
                              Type::getIntNTy(context, initialSize * 2));

    Value* result = builder.CreateMul(Lvalue, Rvalue, "intmul");

    // Flags
    auto resultType = Type::getIntNTy(context, initialSize);

    Value* highPart = builder.CreateLShr(result, initialSize, "highPart");
    Value* highPartTruncated = builder.CreateTrunc(
        highPart, Type::getIntNTy(context, initialSize), "truncatedHighPart");

    /* The OF and CF flags are set to 0 if the upper half of the result is
     * 0; otherwise, they are set to 1. The SF, ZF, AF, and PF flags are
     * undefined.
     */
    Value* cf = builder.CreateICmpNE(highPartTruncated,
                                     ConstantInt::get(resultType, 0), "cf");
    Value* of = cf;
    setFlag(builder, FLAG_CF, cf);
    setFlag(builder, FLAG_OF, of);

    auto splitResult = builder.CreateTruncOrBitCast(
        result, Type::getIntNTy(context, initialSize), "splitResult");
    // if not byte operation, result goes into ?dx:?ax

    SetOperandValue(builder, dest1, splitResult);
    SetOperandValue(builder, dest2, highPartTruncated);

    printvalue(Lvalue) printvalue(Rvalue) printvalue(result)
        printvalue(highPart) printvalue(highPartTruncated)
            printvalue(splitResult) printvalue(of) printvalue(cf)
  }

  void lift_div2(IRBuilder<>& builder,
                 ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto src = instruction.operands[0];
    auto dividend = GetRegisterValue(builder, ZYDIS_REGISTER_AX);

    Value* divisor = GetOperandValue(builder, src, src.size);
    divisor = createZExtFolder(builder, divisor,
                               Type::getIntNTy(context, src.size * 2));
    dividend = createZExtOrTruncFolder(builder, dividend, divisor->getType());
    Value* remainder = builder.CreateURem(dividend, divisor);
    Value* quotient = builder.CreateUDiv(dividend, divisor);

    SetRegisterValue(
        builder, ZYDIS_REGISTER_AL,
        createZExtOrTruncFolder(builder, quotient,
                                Type::getIntNTy(context, src.size)));

    SetRegisterValue(
        builder, ZYDIS_REGISTER_AH,
        createZExtOrTruncFolder(builder, remainder,
                                Type::getIntNTy(context, src.size)));

    printvalue(remainder);
    printvalue(quotient);
    printvalue(divisor);
    printvalue(dividend);
  }

  void lift_idiv2(IRBuilder<>& builder,
                  ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto src = instruction.operands[0];
    auto dividend = GetRegisterValue(builder, ZYDIS_REGISTER_AX);

    Value* divisor = GetOperandValue(builder, src, src.size);
    divisor =
        builder.CreateSExt(divisor, Type::getIntNTy(context, src.size * 2));
    dividend = builder.CreateSExtOrTrunc(dividend, divisor->getType());
    Value* remainder = builder.CreateSRem(dividend, divisor);
    Value* quotient = builder.CreateSDiv(dividend, divisor);

    SetRegisterValue(
        builder, ZYDIS_REGISTER_AL,
        createZExtOrTruncFolder(builder, quotient,
                                Type::getIntNTy(context, src.size)));

    SetRegisterValue(
        builder, ZYDIS_REGISTER_AH,
        createZExtOrTruncFolder(builder, remainder,
                                Type::getIntNTy(context, src.size)));

    printvalue(remainder);
    printvalue(quotient);
    printvalue(divisor);
    printvalue(dividend);
  }

  void lift_div(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {

    LLVMContext& context = builder.getContext();
    auto src = instruction.operands[0];
    if (src.size == 8) {
      lift_div2(builder, instruction);
      return;
    }
    auto dividendLowop = instruction.operands[1];  // eax
    auto dividendHighop = instruction.operands[2]; // edx

    auto Rvalue = GetOperandValue(builder, src, src.size);

    Value *dividendLow, *dividendHigh, *dividend;

    dividendLow = GetOperandValue(builder, dividendLowop, src.size);
    dividendHigh = GetOperandValue(builder, dividendHighop, src.size);

    dividendLow = createZExtFolder(builder, dividendLow,
                                   Type::getIntNTy(context, src.size * 2));
    dividendHigh =
        createZExtFolder(builder, dividendHigh, dividendLow->getType());
    uint8_t bitWidth = src.size;

    dividendHigh = builder.CreateShl(dividendHigh, bitWidth);
    printvalue2(bitWidth);
    printvalue(dividendLow);
    printvalue(dividendHigh);

    dividend = builder.CreateOr(dividendHigh, dividendLow);
    printvalue(dividend);
    Value* divide = createZExtFolder(builder, Rvalue, dividend->getType());
    Value *quotient, *remainder;
    if (isa<ConstantInt>(divide) && isa<ConstantInt>(dividend)) {

      APInt divideCI = cast<ConstantInt>(divide)->getValue();
      APInt dividendCI = cast<ConstantInt>(dividend)->getValue();

      APInt quotientCI = dividendCI.udiv(divideCI);
      APInt remainderCI = dividendCI.urem(divideCI);

      printvalue2(divideCI);
      printvalue2(dividendCI);
      printvalue2(quotientCI);
      printvalue2(remainderCI);
      quotient = ConstantInt::get(Rvalue->getType(), quotientCI);
      remainder = ConstantInt::get(Rvalue->getType(), remainderCI);
    } else {
      quotient = builder.CreateUDiv(dividend, divide);
      remainder = builder.CreateURem(dividend, divide);
    }
    SetOperandValue(
        builder, dividendLowop,
        createZExtOrTruncFolder(builder, quotient, Rvalue->getType()));

    SetOperandValue(
        builder, dividendHighop,
        createZExtOrTruncFolder(builder, remainder, Rvalue->getType()));

    printvalue(Rvalue) printvalue(dividend) printvalue(remainder)
        printvalue(quotient)
  }

  void lift_idiv(IRBuilder<>& builder,
                 ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto src = instruction.operands[0];
    if (src.size == 8) {
      lift_idiv2(builder, instruction);
      return;
    }
    auto dividendLowop = instruction.operands[1];  // eax
    auto dividendHighop = instruction.operands[2]; // edx

    auto Rvalue = GetOperandValue(builder, src, src.size);

    Value *dividendLow, *dividendHigh, *dividend;

    dividendLow = GetOperandValue(builder, dividendLowop, src.size);
    dividendHigh = GetOperandValue(builder, dividendHighop, src.size);

    dividendLow = createZExtFolder(builder, dividendLow,
                                   Type::getIntNTy(context, src.size * 2));
    dividendHigh =
        createZExtFolder(builder, dividendHigh, dividendLow->getType());
    uint8_t bitWidth = src.size;

    dividendHigh = builder.CreateShl(dividendHigh, bitWidth);
    printvalue2(bitWidth);
    printvalue(dividendLow);
    printvalue(dividendHigh);

    dividend = builder.CreateOr(dividendHigh, dividendLow);
    printvalue(dividend);
    Value* divide = builder.CreateSExt(Rvalue, dividend->getType());
    Value *quotient, *remainder;
    if (isa<ConstantInt>(divide) && isa<ConstantInt>(dividend)) {

      APInt divideCI = cast<ConstantInt>(divide)->getValue();
      APInt dividendCI = cast<ConstantInt>(dividend)->getValue();

      APInt quotientCI = dividendCI.sdiv(divideCI);
      APInt remainderCI = dividendCI.srem(divideCI);

      printvalue2(divideCI);
      printvalue2(dividendCI);
      printvalue2(quotientCI);
      printvalue2(remainderCI);
      quotient = ConstantInt::get(Rvalue->getType(), quotientCI);
      remainder = ConstantInt::get(Rvalue->getType(), remainderCI);
    } else {
      quotient = builder.CreateSDiv(dividend, divide);
      remainder = builder.CreateSRem(dividend, divide);
    }
    SetOperandValue(
        builder, dividendLowop,
        createZExtOrTruncFolder(builder, quotient, Rvalue->getType()));

    SetOperandValue(
        builder, dividendHighop,
        createZExtOrTruncFolder(builder, remainder, Rvalue->getType()));

    printvalue(Rvalue) printvalue(dividend) printvalue(remainder)
        printvalue(quotient)
  }

  void lift_xor(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];
    auto Rvalue = GetOperandValue(builder, src, dest.size);
    auto Lvalue = GetOperandValue(builder, dest, dest.size);
    auto result = createXorFolder(
        builder, Lvalue, Rvalue,
        "realxor-" + to_string(instruction.runtime_address) + "-");

    printvalue(Lvalue) printvalue(Rvalue) printvalue(result)

        auto sf = computeSignFlag(builder, result);
    auto zf = computeZeroFlag(builder, result);
    auto pf = computeParityFlag(builder, result);
    //  The OF and CF flags are cleared; the SF, ZF, and PF flags are set
    //  according to the result. The state of the AF flag is undefined.

    setFlag(builder, FLAG_SF, sf);
    setFlag(builder, FLAG_ZF, zf);
    setFlag(builder, FLAG_PF, pf);

    setFlag(builder, FLAG_OF,
            ConstantInt::getSigned(Type::getInt1Ty(context), 0));
    setFlag(builder, FLAG_CF,
            ConstantInt::getSigned(Type::getInt1Ty(context), 0));

    SetOperandValue(builder, dest, result);
  }

  void lift_or(IRBuilder<>& builder,
               ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];
    auto Rvalue = GetOperandValue(builder, src, dest.size);
    auto Lvalue = GetOperandValue(builder, dest, dest.size);
    auto result = createOrFolder(
        builder, Lvalue, Rvalue,
        "realor-" + to_string(instruction.runtime_address) + "-");

    printvalue(Lvalue);
    printvalue(Rvalue);
    printvalue(result);

    auto sf = computeSignFlag(builder, result);
    auto zf = computeZeroFlag(builder, result);
    auto pf = computeParityFlag(builder, result);
    printvalue(sf);
    // The OF and CF flags are cleared; the SF, ZF, and PF flags are set
    // according to the result. The state of the AF flag is undefined.

    setFlag(builder, FLAG_SF, sf);
    setFlag(builder, FLAG_ZF, zf);
    setFlag(builder, FLAG_PF, pf);

    setFlag(builder, FLAG_OF,
            ConstantInt::getSigned(Type::getInt1Ty(context), 0));
    setFlag(builder, FLAG_CF,
            ConstantInt::getSigned(Type::getInt1Ty(context), 0));

    SetOperandValue(builder, dest, result);
  }

  void lift_and(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];
    auto Rvalue = GetOperandValue(builder, src, dest.size);
    auto Lvalue = GetOperandValue(builder, dest, dest.size);

    auto result = createAndFolder(
        builder, Lvalue, Rvalue,
        "realand-" + to_string(instruction.runtime_address) + "-");

    auto sf = computeSignFlag(builder, result);
    auto zf = computeZeroFlag(builder, result);
    auto pf = computeParityFlag(builder, result);

    // The OF and CF flags are cleared; the SF, ZF, and PF flags are set
    // according to the result. The state of the AF flag is undefined.
    setFlag(builder, FLAG_SF, sf);
    setFlag(builder, FLAG_ZF, zf);
    setFlag(builder, FLAG_PF, pf);

    setFlag(builder, FLAG_OF,
            ConstantInt::getSigned(Type::getInt1Ty(context), 0));
    setFlag(builder, FLAG_CF,
            ConstantInt::getSigned(Type::getInt1Ty(context), 0));

    printvalue(Lvalue) printvalue(Rvalue) printvalue(result);

    SetOperandValue(builder, dest, result,
                    "and" + to_string(instruction.runtime_address));
  }

  /*

  tempCOUNT := (COUNT & COUNTMASK) MOD SIZE
  WHILE (tempCOUNT ≠ 0)
          DO
          tempCF := MSB(DEST);
          DEST := (DEST ∗ 2) + tempCF;
          tempCOUNT := tempCOUNT – 1;
          OD;
  ELIHW;
  IF (COUNT & COUNTMASK) ≠ 0
          THEN CF := LSB(DEST);
  FI;
  IF (COUNT & COUNTMASK) = 1
          THEN OF := MSB(DEST) XOR CF;
          ELSE OF is undefined;
  FI
  */
  void lift_rol(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];
    auto Lvalue = GetOperandValue(builder, dest, dest.size);
    auto Rvalue = GetOperandValue(builder, src, dest.size);

    unsigned bitWidth = Lvalue->getType()->getIntegerBitWidth();
    Rvalue = createAndFolder(builder, Rvalue,
                             ConstantInt::get(Rvalue->getType(), bitWidth - 1),
                             "maskRvalue");

    Value* shiftedLeft = createShlFolder(builder, Lvalue, Rvalue);
    Value* shiftedRight = createLShrFolder(
        builder, Lvalue,
        createSubFolder(builder, ConstantInt::get(Rvalue->getType(), bitWidth),
                        Rvalue),
        "rol");
    Value* result = createOrFolder(builder, shiftedLeft, shiftedRight);

    Value* lastBit =
        createAndFolder(builder, shiftedRight,
                        ConstantInt::get(Lvalue->getType(), 1), "rollastbit");
    Value* cf =
        createZExtOrTruncFolder(builder, lastBit, Type::getInt1Ty(context));

    Value* zero = ConstantInt::get(Rvalue->getType(), 0);
    Value* isNotZero =
        createICMPFolder(builder, CmpInst::ICMP_NE, Rvalue, zero);
    Value* oldcf = getFlag(builder, FLAG_CF);
    cf = createSelectFolder(builder, isNotZero, cf, oldcf);
    result = createSelectFolder(builder, isNotZero, result, Lvalue);

    // of = cf ^ MSB
    Value* newMSB = createLShrFolder(builder, result, bitWidth - 1, "rolmsb");
    Value* of = createXorFolder(
        builder, cf,
        createZExtOrTruncFolder(builder, newMSB, Type::getInt1Ty(context)));

    // Use Select to conditionally update OF based on whether the shift
    // amount is 1
    Value* isOneBitRotation =
        createICMPFolder(builder, CmpInst::ICMP_EQ, Rvalue,
                         ConstantInt::get(Rvalue->getType(), 1));
    Value* ofCurrent = getFlag(builder, FLAG_OF);

    of = createSelectFolder(builder, isOneBitRotation, of, ofCurrent);

    setFlag(builder, FLAG_CF, cf);
    setFlag(builder, FLAG_OF, of);

    printvalue(Lvalue) printvalue(Rvalue) printvalue(result)
        SetOperandValue(builder, dest, result);
  }

  /*

  tempCOUNT := (COUNT & COUNTMASK) MOD SIZE
  WHILE (tempCOUNT ≠ 0)
          DO
          tempCF := LSB(SRC);
          DEST := (DEST / 2) + (tempCF ∗ 2SIZE);
          tempCOUNT := tempCOUNT – 1;
          OD;
  ELIHW;
  IF (COUNT & COUNTMASK) ≠ 0
          THEN CF := MSB(DEST);
  FI;
  IF (COUNT & COUNTMASK) = 1
          THEN OF := MSB(DEST) XOR MSB − 1(DEST);
          ELSE OF is undefined;
  FI

  */
  void lift_ror(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];
    auto Lvalue = GetOperandValue(builder, dest, dest.size);
    auto Rvalue = GetOperandValue(builder, src, dest.size);

    auto size = ConstantInt::getSigned(Lvalue->getType(),
                                       Lvalue->getType()->getIntegerBitWidth());
    Rvalue = builder.CreateURem(Rvalue, size);

    Value* result = createOrFolder(
        builder, createLShrFolder(builder, Lvalue, Rvalue),
        createShlFolder(builder, Lvalue,
                        createSubFolder(builder, size, Rvalue)),
        "ror-" + std::to_string(instruction.runtime_address) + "-");

    Value* msb = createLShrFolder(
        builder, result,
        createSubFolder(
            builder, size,
            ConstantInt::get(
                context, APInt(Rvalue->getType()->getIntegerBitWidth(), 1))));
    Value* cf = createZExtOrTruncFolder(builder, msb, Type::getInt1Ty(context),
                                        "ror-cf");

    Value* secondMsb = createLShrFolder(
        builder, result,
        createSubFolder(
            builder, size,
            ConstantInt::get(
                context, APInt(Rvalue->getType()->getIntegerBitWidth(), 2))),
        "ror2ndmsb");
    auto ofDefined = createZExtOrTruncFolder(
        builder, createXorFolder(builder, msb, secondMsb), cf->getType());
    auto isOneBitRotation = createICMPFolder(
        builder, CmpInst::ICMP_EQ, Rvalue,
        ConstantInt::get(context,
                         APInt(Rvalue->getType()->getIntegerBitWidth(), 1)));
    Value* ofCurrent = getFlag(builder, FLAG_OF);
    Value* of = createSelectFolder(builder, isOneBitRotation, ofDefined,
                                   ofCurrent, "ror-of");

    setFlag(builder, FLAG_CF, cf);
    setFlag(builder, FLAG_OF, of);

    auto isZeroBitRotation = createICMPFolder(
        builder, CmpInst::ICMP_EQ, Rvalue,
        ConstantInt::get(context,
                         APInt(Rvalue->getType()->getIntegerBitWidth(), 0)),
        "iszerobit");
    result = createSelectFolder(builder, isZeroBitRotation, Lvalue, result,
                                "ror-result");

    printvalue(Lvalue) printvalue(Rvalue) printvalue(result)

        SetOperandValue(builder, dest, result);
  }

  void lift_inc_dec(IRBuilder<>& builder,
                    ZydisDisassembledInstruction& instruction) {
    auto operand = instruction.operands[0];

    Value* Lvalue = GetOperandValue(builder, operand, operand.size);

    Value* one = ConstantInt::get(Lvalue->getType(), 1, true);
    Value* result;
    Value* of;
    // The CF flag is not affected. The OF, SF, ZF, AF, and PF flags are set
    // according to the result.
    if (instruction.info.mnemonic == ZYDIS_MNEMONIC_INC) {
      // treat it as add r, 1 for flags
      result = createAddFolder(builder, Lvalue, one,
                               "inc-" + to_string(instruction.runtime_address) +
                                   "-");
      of = computeOverflowFlagAdd(builder, Lvalue, one, result);

    } else {
      // treat it as sub r, 1 for flags
      result = createSubFolder(builder, Lvalue, one,
                               "dec-" + to_string(instruction.runtime_address) +
                                   "-");
      of = computeOverflowFlagSub(builder, Lvalue, one, result);
    }

    printvalue(Lvalue) printvalue(result)

        Value* sf = computeSignFlag(builder, result);
    Value* zf = computeZeroFlag(builder, result);
    Value* pf = computeParityFlag(builder, result);

    printvalue(sf)

        setFlag(builder, FLAG_OF, of);
    setFlag(builder, FLAG_SF, sf);
    setFlag(builder, FLAG_ZF, zf);
    setFlag(builder, FLAG_PF, pf);
    SetOperandValue(builder, operand, result);
  }

  void lift_push(IRBuilder<>& builder,
                 ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto src = instruction.operands[0]; // value that we are pushing
    auto dest = instruction.operands[2];
    auto rsp = instruction.operands[1];

    auto Rvalue = GetOperandValue(builder, src, dest.size);
    auto RspValue = GetOperandValue(builder, rsp, rsp.size); // ?
    auto val = ConstantInt::getSigned(
        Type::getInt64Ty(context),
        dest.size / 8); // jokes on me apparently this is not a fixed value
    auto result = createSubFolder(
        builder, RspValue, val,
        "pushing_newrsp-" + to_string(instruction.runtime_address) + "-");

    printvalue(RspValue) printvalue(result) SetOperandValue(
        builder, rsp, result, to_string(instruction.runtime_address));
    ; // sub rsp 8 first,

    SetOperandValue(builder, dest, Rvalue,
                    to_string(instruction.runtime_address));
    ; // then mov rsp, val
  }

  void lift_pushfq(IRBuilder<>& builder,
                   ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto src = instruction.operands[2];  // value that we are pushing rflags
    auto dest = instruction.operands[1]; // [rsp]
    auto rsp = instruction.operands[0];  // rsp

    auto Rvalue = GetOperandValue(builder, src, dest.size);
    // auto Rvalue = GetRFLAGS(builder);
    auto RspValue = GetOperandValue(builder, rsp, rsp.size);

    auto val = ConstantInt::get(Type::getInt64Ty(context), 8);
    auto result = createSubFolder(builder, RspValue, val);

    SetOperandValue(builder, rsp, result,
                    to_string(instruction.runtime_address));
    ; // sub rsp 8 first,

    // pushFlags(builder, dest, Rvalue,
    // to_string(instruction.runtime_address));;
    SetOperandValue(builder, dest, Rvalue,
                    to_string(instruction.runtime_address));
    ; // then mov rsp, val
  }

  void lift_pop(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto dest = instruction.operands[0]; // value that we are pushing
    auto src = instruction.operands[2];
    auto rsp = instruction.operands[1];

    auto Rvalue = GetOperandValue(builder, src, dest.size,
                                  to_string(instruction.runtime_address));
    ;
    auto RspValue = GetOperandValue(builder, rsp, rsp.size,
                                    to_string(instruction.runtime_address));
    ;

    auto val = ConstantInt::getSigned(Type::getInt64Ty(context),
                                      dest.size / 8); // assuming its x64
    auto result = createAddFolder(
        builder, RspValue, val,
        "popping_new_rsp-" + to_string(instruction.runtime_address) + "-");

    printvalue(Rvalue) printvalue(RspValue) printvalue(result)

        SetOperandValue(builder, rsp, result); // then add rsp 8

    SetOperandValue(builder, dest, Rvalue,
                    to_string(instruction.runtime_address));
    ; // mov val, rsp first
  }

  void lift_popfq(IRBuilder<>& builder,
                  ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto dest = instruction.operands[2]; // value that we are pushing
    auto src = instruction.operands[1];  // [rsp]
    auto rsp = instruction.operands[0];  // rsp

    auto Rvalue = GetOperandValue(builder, src, dest.size,
                                  to_string(instruction.runtime_address));
    ;
    auto RspValue = GetOperandValue(builder, rsp, rsp.size,
                                    to_string(instruction.runtime_address));
    ;

    auto val = ConstantInt::getSigned(Type::getInt64Ty(context),
                                      8); // assuming its x64
    auto result = createAddFolder(
        builder, RspValue, val,
        "popfq-" + to_string(instruction.runtime_address) + "-");

    SetOperandValue(builder, dest, Rvalue,
                    to_string(instruction.runtime_address));
    ; // mov val, rsp first
    SetOperandValue(builder, rsp, result,
                    to_string(instruction.runtime_address));
    ; // then add rsp 8
  }

  void lift_adc(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    Value* Lvalue = GetOperandValue(builder, dest, dest.size);
    Value* Rvalue = GetOperandValue(builder, src, dest.size);

    Value* cf = getFlag(builder, FLAG_CF);
    cf = createZExtFolder(builder, cf, Lvalue->getType());

    Value* tempResult = createAddFolder(
        builder, Lvalue, Rvalue,
        "adc-temp-" + to_string(instruction.runtime_address) + "-");
    Value* result = createAddFolder(
        builder, tempResult, cf,
        "adc-result-" + to_string(instruction.runtime_address) + "-");
    // The OF, SF, ZF, AF, CF, and PF flags are set according to the result.

    printvalue(Lvalue) printvalue(Rvalue) printvalue(tempResult) printvalue(
        result)

        auto cfAfterFirstAdd = createOrFolder(
            builder,
            createICMPFolder(builder, CmpInst::ICMP_ULT, tempResult, Lvalue),
            createICMPFolder(builder, CmpInst::ICMP_ULT, tempResult, Rvalue));
    auto cfFinal = createOrFolder(
        builder, cfAfterFirstAdd,
        createICMPFolder(builder, CmpInst::ICMP_ULT, result, cf));

    auto lowerNibbleMask = ConstantInt::get(Lvalue->getType(), 0xF);
    auto destLowerNibble =
        createAndFolder(builder, Lvalue, lowerNibbleMask, "adcdst");
    auto srcLowerNibble =
        createAndFolder(builder, Rvalue, lowerNibbleMask, "adcsrc");
    auto sumLowerNibble =
        createAddFolder(builder, destLowerNibble, srcLowerNibble);
    auto af = createICMPFolder(builder, CmpInst::ICMP_UGT, sumLowerNibble,
                               lowerNibbleMask);

    auto of = computeOverflowFlagAdc(builder, Lvalue, Rvalue, cf, result);

    Value* sf = computeSignFlag(builder, result);
    Value* zf = computeZeroFlag(builder, result);
    Value* pf = computeParityFlag(builder, result);

    setFlag(builder, FLAG_OF, of);
    setFlag(builder, FLAG_AF, af);
    setFlag(builder, FLAG_CF, cfFinal);
    setFlag(builder, FLAG_SF, sf);
    setFlag(builder, FLAG_ZF, zf);
    setFlag(builder, FLAG_PF, pf);

    SetOperandValue(builder, dest, result);
  }

  void lift_xadd(IRBuilder<>& builder,
                 ZydisDisassembledInstruction& instruction) {
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    auto Lvalue = GetOperandValue(builder, dest, dest.size);
    auto Rvalue = GetOperandValue(builder, src, src.size);

    Value* sumValue = createAddFolder(
        builder, Lvalue, Rvalue,
        "xadd_sum-" + to_string(instruction.runtime_address) + "-");

    SetOperandValue(builder, dest, sumValue,
                    to_string(instruction.runtime_address));
    ;

    SetOperandValue(builder, src, Lvalue,
                    to_string(instruction.runtime_address));
    ;
    /*
    TEMP := SRC + DEST;
    SRC := DEST;
    DEST := TEMP;
    */
    printvalue(Lvalue) printvalue(Rvalue) printvalue(sumValue)

        auto cf = createOrFolder(
            builder,
            createICMPFolder(builder, CmpInst::ICMP_ULT, sumValue, Lvalue),
            createICMPFolder(builder, CmpInst::ICMP_ULT, sumValue, Rvalue));

    auto lowerNibbleMask = ConstantInt::get(Lvalue->getType(), 0xF);
    auto destLowerNibble =
        createAndFolder(builder, Lvalue, lowerNibbleMask, "xadddst");
    auto srcLowerNibble =
        createAndFolder(builder, Rvalue, lowerNibbleMask, "xaddsrc");
    auto sumLowerNibble =
        createAddFolder(builder, destLowerNibble, srcLowerNibble);
    auto af = createICMPFolder(builder, CmpInst::ICMP_UGT, sumLowerNibble,
                               lowerNibbleMask);

    auto resultSign = createICMPFolder(builder, CmpInst::ICMP_SLT, sumValue,
                                       ConstantInt::get(Lvalue->getType(), 0));
    auto destSign = createICMPFolder(builder, CmpInst::ICMP_SLT, Lvalue,
                                     ConstantInt::get(Lvalue->getType(), 0));
    auto srcSign = createICMPFolder(builder, CmpInst::ICMP_SLT, Rvalue,
                                    ConstantInt::get(Rvalue->getType(), 0));
    auto inputSameSign =
        createICMPFolder(builder, CmpInst::ICMP_EQ, destSign, srcSign);
    auto of = createAndFolder(
        builder, inputSameSign,
        createICMPFolder(builder, CmpInst::ICMP_NE, destSign, resultSign),
        "xaddof");

    Value* sf = computeSignFlag(builder, sumValue);
    Value* zf = computeZeroFlag(builder, sumValue);
    Value* pf = computeParityFlag(builder, sumValue);

    setFlag(builder, FLAG_OF, of);
    setFlag(builder, FLAG_AF, af);
    setFlag(builder, FLAG_CF, cf);
    setFlag(builder, FLAG_SF, sf);
    setFlag(builder, FLAG_ZF, zf);
    setFlag(builder, FLAG_PF, pf);

    // The CF, PF, AF, SF, ZF, and OF flags are set according to the result
    // of the addition, which is stored in the destination operand.
  }

  void lift_test(IRBuilder<>& builder,
                 ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    Value* Lvalue = GetOperandValue(builder, instruction.operands[0],
                                    instruction.operands[0].size);
    Value* Rvalue = GetOperandValue(builder, instruction.operands[1],
                                    instruction.operands[0].size);

    Value* testResult = createAndFolder(builder, Lvalue, Rvalue, "testAnd");

    Value* of = ConstantInt::get(Type::getInt64Ty(context), 0, "of");
    Value* cf = ConstantInt::get(Type::getInt64Ty(context), 0, "cf");

    Value* sf =
        createICMPFolder(builder, CmpInst::ICMP_SLT, testResult,
                         ConstantInt::get(testResult->getType(), 0), "sf");
    Value* zf =
        createICMPFolder(builder, CmpInst::ICMP_EQ, testResult,
                         ConstantInt::get(testResult->getType(), 0), "zf");
    Value* pf = computeParityFlag(builder, testResult);

    setFlag(builder, FLAG_OF, of);
    setFlag(builder, FLAG_CF, cf);
    setFlag(builder, FLAG_SF, sf);
    setFlag(builder, FLAG_ZF, zf);
    setFlag(builder, FLAG_PF, pf);
  }

  void lift_cmp(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {

    Value* Lvalue = GetOperandValue(builder, instruction.operands[0],
                                    instruction.operands[0].size);
    Value* Rvalue = GetOperandValue(builder, instruction.operands[1],
                                    instruction.operands[0].size);

    Value* cmpResult = createSubFolder(builder, Lvalue, Rvalue);

    Value* signL = createICMPFolder(builder, CmpInst::ICMP_SLT, Lvalue,
                                    ConstantInt::get(Lvalue->getType(), 0));
    Value* signR = createICMPFolder(builder, CmpInst::ICMP_SLT, Rvalue,
                                    ConstantInt::get(Rvalue->getType(), 0));
    Value* signResult =
        createICMPFolder(builder, CmpInst::ICMP_SLT, cmpResult,
                         ConstantInt::get(cmpResult->getType(), 0));

    Value* of = createOrFolder(
        builder,
        createAndFolder(builder, signL,
                        createAndFolder(builder, builder.CreateNot(signR),
                                        builder.CreateNot(signResult),
                                        "cmp-and1-")),
        createAndFolder(builder, builder.CreateNot(signL),
                        createAndFolder(builder, signR, signResult),
                        "cmp-and2-"),
        "cmp-OF-or");

    Value* cf = createICMPFolder(builder, CmpInst::ICMP_ULT, Lvalue, Rvalue);
    Value* zf = createICMPFolder(builder, CmpInst::ICMP_EQ, cmpResult,
                                 ConstantInt::get(cmpResult->getType(), 0));
    Value* sf = createICMPFolder(builder, CmpInst::ICMP_SLT, cmpResult,
                                 ConstantInt::get(cmpResult->getType(), 0));
    Value* pf = computeParityFlag(builder, cmpResult);

    setFlag(builder, FLAG_OF, of);
    setFlag(builder, FLAG_CF, cf);
    setFlag(builder, FLAG_SF, sf);
    setFlag(builder, FLAG_ZF, zf);
    setFlag(builder, FLAG_PF, pf);
  }

  void lift_rdtsc(IRBuilder<>& builder) {
    // cout << instruction.runtime_address << "\n";
    LLVMContext& context = builder.getContext();
    auto rdtscCall =
        builder.CreateIntrinsic(Intrinsic::readcyclecounter, {}, {});
    auto edxPart = createLShrFolder(builder, rdtscCall, 32, "to_edx");
    auto eaxPart = createZExtOrTruncFolder(builder, rdtscCall,
                                           Type::getInt32Ty(context), "to_eax");
    SetRegisterValue(builder, ZYDIS_REGISTER_EDX, edxPart);
    SetRegisterValue(builder, ZYDIS_REGISTER_EAX, eaxPart);
  }

  void lift_cpuid(IRBuilder<>& builder,
                  ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    // instruction.operands[0] = eax
    // instruction.operands[1] = ebx
    // instruction.operands[2] = ecx
    // instruction.operands[3] = edx
    /*

    c++
    #include <intrin.h>

    int getcpuid() {
            int cpuInfo[4];
            __cpuid(cpuInfo, 1);
            return cpuInfo[0] + cpuInfo[1];
    }

    ir
    define dso_local noundef i32 @getcpuid() #0 {
      %1 = alloca [4 x i32], align 16
      %2 = getelementptr inbounds [4 x i32], ptr %1, i64 0, i64 0
      %3 = call { i32, i32, i32, i32 } asm "xchgq %rbx,
    ${1:q}\0Acpuid\0Axchgq %rbx, ${1:q}", "={ax},=r,={cx},={dx},0,2"(i32 1,
    i32 0) %4 = getelementptr inbounds [4 x i32], ptr %1, i64 0, i64 0 %5 =
    extractvalue { i32, i32, i32, i32 } %3, 0 %6 = getelementptr inbounds
    i32, ptr %4, i32 0 store i32 %5, ptr %6, align 4 %7 = extractvalue {
    i32, i32, i32, i32 } %3, 1 %8 = getelementptr inbounds i32, ptr %4, i32
    1 store i32 %7, ptr %8, align 4 %9 = extractvalue { i32, i32, i32, i32 }
    %3, 2 %10 = getelementptr inbounds i32, ptr %4, i32 2 store i32 %9, ptr
    %10, align 4 %11 = extractvalue { i32, i32, i32, i32 } %3, 3 %12 =
    getelementptr inbounds i32, ptr %4, i32 3 store i32 %11, ptr %12, align
    4

      %13 = getelementptr inbounds [4 x i32], ptr %1, i64 0, i64 0
      %14 = load i32, ptr %13, align 16

      %15 = getelementptr inbounds [4 x i32], ptr %1, i64 0, i64 1
      %16 = load i32, ptr %15, align 4
      %17 = add nsw i32 %14, %16
      ret i32 %17
    }
    opt
    define dso_local noundef i32 @getcpuid() local_unnamed_addr {
      %1 = tail call { i32, i32, i32, i32 } asm "xchgq %rbx,
    ${1:q}\0Acpuid\0Axchgq %rbx, ${1:q}", "={ax},=r,={cx},={dx},0,2"(i32 1,
    i32 0) #0 %2 = extractvalue { i32, i32, i32, i32 } %1, 1 ret i32 %2
    }

    */
    // int cpuInfo[4];
    // ArrayType* CpuInfoTy = ArrayType::get(Type::getInt32Ty(context), 4);

    Value* eax = GetOperandValue(builder, instruction.operands[0],
                                 instruction.operands[0].size);
    // one is eax, other is always 0?
    std::vector<Type*> AsmOutputs = {
        Type::getInt32Ty(context), Type::getInt32Ty(context),
        Type::getInt32Ty(context), Type::getInt32Ty(context)};
    StructType* AsmStructType = StructType::get(context, AsmOutputs);

    std::vector<Type*> ArgTypes = {Type::getInt32Ty(context),
                                   Type::getInt32Ty(context)};

    // this is probably incorrect
    InlineAsm* IA =
        InlineAsm::get(FunctionType::get(AsmStructType, ArgTypes, false),
                       "xchgq %rbx, ${1:q}\ncpuid\nxchgq %rbx, ${1:q}",
                       "={ax},=r,={cx},={dx},0,2", true);

    std::vector<Value*> Args{eax, ConstantInt::get(eax->getType(), 0)};

    Value* cpuidCall = builder.CreateCall(IA, Args);

    Value* eaxv = builder.CreateExtractValue(cpuidCall, 0, "eax");
    Value* ebx = builder.CreateExtractValue(cpuidCall, 1, "ebx");
    Value* ecx = builder.CreateExtractValue(cpuidCall, 2, "ecx");
    Value* edx = builder.CreateExtractValue(cpuidCall, 3, "edx");

    SetOperandValue(builder, instruction.operands[0], eaxv);
    SetOperandValue(builder, instruction.operands[1], ebx);
    SetOperandValue(builder, instruction.operands[2], ecx);
    SetOperandValue(builder, instruction.operands[3], edx);
  }

} // namespace arithmeticsAndLogical

namespace flagOperation {
  void lift_setnz(IRBuilder<>& builder,
                  ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();

    auto dest = instruction.operands[0];

    Value* zf = getFlag(builder, FLAG_ZF);

    Value* result = createZExtFolder(builder, builder.CreateNot(zf),
                                     Type::getInt8Ty(context));

    SetOperandValue(builder, dest, result);
  }
  void lift_seto(IRBuilder<>& builder,
                 ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();

    auto dest = instruction.operands[0];

    Value* of = getFlag(builder, FLAG_OF);

    Value* result = createZExtFolder(builder, of, Type::getInt8Ty(context));

    SetOperandValue(builder, dest, result);
  }
  void lift_setno(IRBuilder<>& builder,
                  ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();

    auto dest = instruction.operands[0];

    Value* of = getFlag(builder, FLAG_OF);

    Value* notOf = builder.CreateNot(of, "notOF");

    Value* result = createZExtFolder(builder, notOf, Type::getInt8Ty(context));

    SetOperandValue(builder, dest, result);
  }

  void lift_setnb(IRBuilder<>& builder,
                  ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();

    auto dest = instruction.operands[0];

    Value* cf = getFlag(builder, FLAG_CF);

    Value* result =
        createICMPFolder(builder, CmpInst::ICMP_EQ, cf,
                         ConstantInt::get(Type::getInt1Ty(context), 0));

    Value* byteResult =
        createZExtFolder(builder, result, Type::getInt8Ty(context));

    SetOperandValue(builder, dest, byteResult);
  }

  void lift_setbe(IRBuilder<>& builder,
                  ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();

    Value* cf = getFlag(builder, FLAG_CF);
    Value* zf = getFlag(builder, FLAG_ZF);

    Value* condition = createOrFolder(builder, cf, zf, "setbe-or");

    Value* result =
        createZExtFolder(builder, condition, Type::getInt8Ty(context));

    auto dest = instruction.operands[0];
    SetOperandValue(builder, dest, result);
  }

  void lift_setnbe(IRBuilder<>& builder,
                   ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();

    Value* cf = getFlag(builder, FLAG_CF);
    Value* zf = getFlag(builder, FLAG_ZF);

    Value* condition = createAndFolder(builder, builder.CreateNot(cf),
                                       builder.CreateNot(zf), "setnbe-and");

    Value* result =
        createZExtFolder(builder, condition, Type::getInt8Ty(context));

    auto dest = instruction.operands[0];
    SetOperandValue(builder, dest, result);
  }

  void lift_setns(IRBuilder<>& builder,
                  ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();

    auto dest = instruction.operands[0];

    Value* sf = getFlag(builder, FLAG_SF);

    Value* result =
        createICMPFolder(builder, CmpInst::ICMP_EQ, sf,
                         ConstantInt::get(Type::getInt1Ty(context), 0));

    Value* byteResult =
        createZExtFolder(builder, result, Type::getInt8Ty(context));

    SetOperandValue(builder, dest, byteResult);
  }

  void lift_setp(IRBuilder<>& builder,
                 ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();

    Value* pf = getFlag(builder, FLAG_PF);

    Value* result = createZExtFolder(builder, pf, Type::getInt8Ty(context));

    auto dest = instruction.operands[0];

    SetOperandValue(builder, dest, result);
  }

  void lift_setnp(IRBuilder<>& builder,
                  ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto dest = instruction.operands[0];

    Value* pf = getFlag(builder, FLAG_PF);

    Value* resultValue = createZExtFolder(builder, builder.CreateNot(pf),
                                          Type::getInt8Ty(context));

    SetOperandValue(builder, dest, resultValue,
                    to_string(instruction.runtime_address));
  }

  void lift_setb(IRBuilder<>& builder,
                 ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();

    auto dest = instruction.operands[0];

    Value* cf = getFlag(builder, FLAG_CF);

    Value* result = createZExtFolder(builder, cf, Type::getInt8Ty(context));

    SetOperandValue(builder, dest, result);
  }

  void lift_sets(IRBuilder<>& builder,
                 ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    Value* sf = getFlag(builder, FLAG_SF);

    Value* result = createZExtFolder(builder, sf, Type::getInt8Ty(context));

    auto dest = instruction.operands[0];
    SetOperandValue(builder, dest, result);
  }

  void lift_stosx(IRBuilder<>& builder,
                  ZydisDisassembledInstruction& instruction) {

    auto dest = instruction.operands[0]; // xdi
    Value* destValue = GetOperandValue(builder, dest, dest.size);
    Value* DF = getFlag(builder, FLAG_DF);
    // if df is 1, +
    // else -
    auto destbitwidth = dest.size;

    auto one = ConstantInt::get(DF->getType(), 1);
    Value* Direction = builder.CreateSub(
        builder.CreateMul(DF, builder.CreateAdd(DF, one)), one);

    Value* result = createAddFolder(
        builder, destValue,
        builder.CreateMul(Direction,
                          ConstantInt::get(DF->getType(), destbitwidth)));
    SetOperandValue(builder, dest, result);
  }

  void lift_setz(IRBuilder<>& builder,
                 ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto dest = instruction.operands[0];

    Value* zf = getFlag(builder, FLAG_ZF);

    Value* extendedZF =
        createZExtFolder(builder, zf, Type::getInt8Ty(context), "setz_extend");

    SetOperandValue(builder, dest, extendedZF);
  }

  void lift_setnle(IRBuilder<>& builder,
                   ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto dest = instruction.operands[0];

    Value* zf = getFlag(builder, FLAG_ZF);
    Value* sf = getFlag(builder, FLAG_SF);
    Value* of = getFlag(builder, FLAG_OF);

    Value* zfNotSet =
        createICMPFolder(builder, CmpInst::ICMP_EQ, zf,
                         ConstantInt::get(Type::getInt1Ty(context), 0));

    Value* sfEqualsOf = createICMPFolder(builder, CmpInst::ICMP_EQ, sf, of);

    printvalue(zf) printvalue(sf) printvalue(of)

        Value* combinedCondition =
            createAndFolder(builder, zfNotSet, sfEqualsOf, "setnle-and");

    Value* byteResult =
        createZExtFolder(builder, combinedCondition, Type::getInt8Ty(context));

    SetOperandValue(builder, dest, byteResult);
  }

  void lift_setle(IRBuilder<>& builder,
                  ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    Value* zf = getFlag(builder, FLAG_ZF);
    Value* sf = getFlag(builder, FLAG_SF);
    Value* of = getFlag(builder, FLAG_OF);

    Value* sf_ne_of = createICMPFolder(builder, CmpInst::ICMP_NE, sf, of);
    Value* condition = createOrFolder(builder, zf, sf_ne_of, "setle-or");

    Value* result =
        createZExtFolder(builder, condition, Type::getInt8Ty(context));

    auto dest = instruction.operands[0];
    SetOperandValue(builder, dest, result);
  }

  void lift_setnl(IRBuilder<>& builder,
                  ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    Value* sf = getFlag(builder, FLAG_SF);
    Value* of = getFlag(builder, FLAG_OF);

    Value* condition = createICMPFolder(builder, CmpInst::ICMP_EQ, sf, of);

    Value* result =
        createZExtFolder(builder, condition, Type::getInt8Ty(context));

    auto dest = instruction.operands[0];
    SetOperandValue(builder, dest, result);
  }

  void lift_setl(IRBuilder<>& builder,
                 ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    Value* sf = getFlag(builder, FLAG_SF);
    Value* of = getFlag(builder, FLAG_OF);

    Value* condition = createICMPFolder(builder, CmpInst::ICMP_NE, sf, of);

    Value* result =
        createZExtFolder(builder, condition, Type::getInt8Ty(context));

    auto dest = instruction.operands[0];
    SetOperandValue(builder, dest, result);
  }

  void lift_bt(IRBuilder<>& builder,
               ZydisDisassembledInstruction& instruction) {

    auto dest = instruction.operands[0];
    auto bitIndex = instruction.operands[1];

    // If the bit base operand specifies a register, the instruction takes
    // the modulo 16, 32, or 64 of the bit offset operand (modulo size
    // depends on the mode and register size; 64-bit operands are available
    // only in 64-bit mode). If the bit base operand specifies a memory
    // location, the operand represents the address of the byte in memory
    // that contains the bit base (bit 0 of the specified byte) of the bit
    // string. The range of the bit position that can be referenced by the
    // offset operand depends on the operand size. CF := Bit(BitBase,
    // BitOffset);

    auto Lvalue = GetOperandValue(builder, dest, dest.size);
    auto bitIndexValue = GetOperandValue(builder, bitIndex, dest.size);

    unsigned LvalueBitW = cast<IntegerType>(Lvalue->getType())->getBitWidth();

    auto Rvalue = createAndFolder(
        builder, bitIndexValue,
        ConstantInt::get(bitIndexValue->getType(), LvalueBitW - 1));

    auto shl = createShlFolder(
        builder, ConstantInt::get(bitIndexValue->getType(), 1), Rvalue);

    auto andd = createAndFolder(builder, shl, Lvalue);

    auto cf = createICMPFolder(builder, CmpInst::ICMP_NE, andd,
                               ConstantInt::get(andd->getType(), 0));

    setFlag(builder, FLAG_CF, cf);
    printvalue(Rvalue);
    printvalue(Lvalue);
    printvalue(shl);
    printvalue(andd);
    printvalue(cf);
  }

  void lift_btr(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {
    auto base = instruction.operands[0];
    auto offset = instruction.operands[1];

    unsigned baseBitWidth = base.size;

    Value* bitOffset = GetOperandValue(builder, offset, base.size);

    Value* bitOffsetMasked = createAndFolder(
        builder, bitOffset,
        ConstantInt::get(bitOffset->getType(), baseBitWidth - 1),
        "bitOffsetMasked");

    Value* baseVal = GetOperandValue(builder, base, base.size);

    Value* bit = createLShrFolder(
        builder, baseVal, bitOffsetMasked,
        "btr-lshr-" + to_string(instruction.runtime_address) + "-");

    Value* one = ConstantInt::get(bit->getType(), 1);

    bit = createAndFolder(builder, bit, one, "btr-and");

    setFlag(builder, FLAG_CF, bit);

    Value* mask =
        createShlFolder(builder, ConstantInt::get(baseVal->getType(), 1),
                        bitOffsetMasked, "btr-shl");

    mask = builder.CreateNot(mask); // invert mask
    baseVal = createAndFolder(builder, baseVal, mask,
                              "btr-and-" +
                                  to_string(instruction.runtime_address) + "-");

    SetOperandValue(builder, base, baseVal);
    printvalue(bitOffset);
    printvalue(baseVal);
    printvalue(mask);
  }

  void lift_bsr(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    Value* Rvalue = GetOperandValue(builder, src, src.size);
    Value* isZero = createICMPFolder(builder, CmpInst::ICMP_EQ, Rvalue,
                                     ConstantInt::get(Rvalue->getType(), 0));
    setFlag(builder, FLAG_ZF, isZero);

    unsigned bitWidth = Rvalue->getType()->getIntegerBitWidth();

    Value* index = ConstantInt::get(Rvalue->getType(), bitWidth - 1);
    Value* zeroVal = ConstantInt::get(Rvalue->getType(), 0);
    Value* oneVal = ConstantInt::get(Rvalue->getType(), 1);

    Value* bitPosition = ConstantInt::get(Rvalue->getType(), -1);

    for (unsigned i = 0; i < bitWidth; ++i) {

      Value* mask = createShlFolder(builder, oneVal, index);

      Value* test = createAndFolder(builder, Rvalue, mask, "bsrtest");
      Value* isBitSet =
          createICMPFolder(builder, CmpInst::ICMP_NE, test, zeroVal);

      Value* tmpPosition =
          createSelectFolder(builder, isBitSet, index, bitPosition);

      Value* isPositionUnset =
          createICMPFolder(builder, CmpInst::ICMP_EQ, bitPosition,
                           ConstantInt::get(Rvalue->getType(), -1));
      bitPosition = createSelectFolder(builder, isPositionUnset, tmpPosition,
                                       bitPosition);

      index = createSubFolder(builder, index, oneVal);
    }

    SetOperandValue(builder, dest, bitPosition);
  }

  void lift_bsf(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    auto dest = instruction.operands[0];
    auto src = instruction.operands[1];

    Value* Rvalue = GetOperandValue(builder, src, src.size);

    Value* isZero = createICMPFolder(builder, CmpInst::ICMP_EQ, Rvalue,
                                     ConstantInt::get(Rvalue->getType(), 0));
    setFlag(builder, FLAG_ZF, isZero);

    Type* intType = Rvalue->getType();
    uint64_t intWidth = intType->getIntegerBitWidth();

    Value* result = ConstantInt::get(intType, intWidth);
    Value* one = ConstantInt::get(intType, 1);

    Value* continuecounting = ConstantInt::get(Type::getInt1Ty(context), 1);
    for (uint64_t i = 0; i < intWidth; ++i) {
      Value* bitMask =
          createShlFolder(builder, one, ConstantInt::get(intType, i));
      Value* bitSet = createAndFolder(builder, Rvalue, bitMask, "bsfbitset");
      Value* isBitZero = createICMPFolder(builder, CmpInst::ICMP_EQ, bitSet,
                                          ConstantInt::get(intType, 0));
      // continue until isBitZero is 1
      // 0010
      // if continuecounting, select
      Value* possibleResult = ConstantInt::get(intType, i);
      Value* condition =
          createAndFolder(builder, continuecounting, isBitZero, "bsfcondition");
      continuecounting = builder.CreateNot(isBitZero);
      result = createSelectFolder(builder, condition, result, possibleResult,
                                  "updateResultOnFirstNonZeroBit");
    }

    SetOperandValue(builder, dest, result);
  }

  void lift_btc(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {
    auto base = instruction.operands[0];
    auto offset = instruction.operands[1];

    unsigned baseBitWidth = base.size;

    Value* bitOffset = GetOperandValue(builder, offset, base.size);

    Value* bitOffsetMasked = createAndFolder(
        builder, bitOffset,
        ConstantInt::get(bitOffset->getType(), baseBitWidth - 1),
        "bitOffsetMasked");

    Value* baseVal = GetOperandValue(builder, base, base.size);

    Value* bit = createLShrFolder(
        builder, baseVal, bitOffsetMasked,
        "btc-lshr-" + to_string(instruction.runtime_address) + "-");

    Value* one = ConstantInt::get(bit->getType(), 1);

    bit = createAndFolder(builder, bit, one, "btc-and");

    setFlag(builder, FLAG_CF, bit);

    Value* mask =
        createShlFolder(builder, ConstantInt::get(baseVal->getType(), 1),
                        bitOffsetMasked, "btc-shl");

    baseVal = createXorFolder(builder, baseVal, mask,
                              "btc-and-" +
                                  to_string(instruction.runtime_address) + "-");

    SetOperandValue(builder, base, baseVal);
    printvalue(bitOffset);
    printvalue(baseVal);
    printvalue(mask);
  }

  void lift_lahf(IRBuilder<>& builder) {

    LLVMContext& context = builder.getContext();

    auto sf = getFlag(builder, FLAG_SF);
    auto zf = getFlag(builder, FLAG_ZF);
    auto af = getFlag(builder, FLAG_AF);
    auto pf = getFlag(builder, FLAG_PF);
    auto cf = getFlag(builder, FLAG_CF);

    printvalue(sf) printvalue(zf) printvalue(af) printvalue(pf) printvalue(cf);

    cf = createZExtFolder(builder, cf, Type::getInt8Ty(context));
    pf = createShlFolder(
        builder, createZExtFolder(builder, pf, Type::getInt8Ty(context)),
        FLAG_PF);
    af = createShlFolder(
        builder, createZExtFolder(builder, af, Type::getInt8Ty(context)),
        FLAG_AF);
    zf = createShlFolder(
        builder, createZExtFolder(builder, zf, Type::getInt8Ty(context)),
        FLAG_ZF);
    sf = createShlFolder(
        builder, createZExtFolder(builder, sf, Type::getInt8Ty(context)),
        FLAG_SF);
    Value* Rvalue =
        createOrFolder(builder,
                       createOrFolder(builder, createOrFolder(builder, cf, pf),
                                      createOrFolder(builder, af, sf)),
                       sf);

    printvalue(sf) printvalue(zf) printvalue(af) printvalue(pf) printvalue(cf);

    SetRegisterValue(builder, ZYDIS_REGISTER_AH, Rvalue);
  }

  void lift_stc(IRBuilder<>& builder) {
    LLVMContext& context = builder.getContext();

    setFlag(builder, FLAG_CF, ConstantInt::get(Type::getInt1Ty(context), 1));
  }

  void lift_cmc(IRBuilder<>& builder) {

    Value* cf = getFlag(builder, FLAG_CF);

    Value* one = ConstantInt::get(cf->getType(), 1);

    setFlag(builder, FLAG_CF, createXorFolder(builder, cf, one));
  }

  void lift_clc(IRBuilder<>& builder) {

    LLVMContext& context = builder.getContext();

    Value* clearedCF = ConstantInt::get(Type::getInt1Ty(context), 0);

    setFlag(builder, FLAG_CF, clearedCF);
  }

  void lift_cld(IRBuilder<>& builder) {

    LLVMContext& context = builder.getContext();

    Value* clearedDF = ConstantInt::get(Type::getInt1Ty(context), 0);

    setFlag(builder, FLAG_DF, clearedDF);
  }

  void lift_cli(IRBuilder<>& builder) {

    LLVMContext& context = builder.getContext();

    Value* resetIF = ConstantInt::get(Type::getInt1Ty(context), 0);

    setFlag(builder, FLAG_IF, resetIF);
  }

  void lift_bts(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {
    auto base = instruction.operands[0];
    auto offset = instruction.operands[1];

    unsigned baseBitWidth = base.size;

    Value* bitOffset = GetOperandValue(builder, offset, base.size);

    Value* bitOffsetMasked = createAndFolder(
        builder, bitOffset,
        ConstantInt::get(bitOffset->getType(), baseBitWidth - 1),
        "bitOffsetMasked");

    Value* baseVal = GetOperandValue(builder, base, base.size);

    Value* bit = createLShrFolder(
        builder, baseVal, bitOffsetMasked,
        "bts-lshr-" + to_string(instruction.runtime_address) + "-");

    Value* one = ConstantInt::get(bit->getType(), 1);

    bit = createAndFolder(builder, bit, one, "bts-and");

    setFlag(builder, FLAG_CF, bit);

    Value* mask =
        createShlFolder(builder, ConstantInt::get(baseVal->getType(), 1),
                        bitOffsetMasked, "bts-shl");

    baseVal = createOrFolder(builder, baseVal, mask,
                             "bts-or-" +
                                 to_string(instruction.runtime_address) + "-");

    SetOperandValue(builder, base, baseVal);
    printvalue(bitOffset);
    printvalue(baseVal);
    printvalue(mask);
  }

  void lift_cwd(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();

    Value* ax = createZExtOrTruncFolder(
        builder,
        GetOperandValue(builder, instruction.operands[1],
                        instruction.operands[1].size),
        Type::getInt16Ty(context));

    Value* signBit = computeSignFlag(builder, ax);

    Value* dx = createSelectFolder(
        builder,
        createICMPFolder(builder, CmpInst::ICMP_EQ, signBit,
                         ConstantInt::get(signBit->getType(), 0)),
        ConstantInt::get(Type::getInt16Ty(context), 0),
        ConstantInt::get(Type::getInt16Ty(context), 0xFFFF), "setDX");

    SetOperandValue(builder, instruction.operands[0], dx);
  }

  void lift_cdq(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    // if eax is -, then edx is filled with ones FFFF_FFFF
    Value* eax = createZExtOrTruncFolder(
        builder,
        GetOperandValue(builder, instruction.operands[1],
                        instruction.operands[1].size),
        Type::getInt32Ty(context));

    Value* signBit = computeSignFlag(builder, eax);

    Value* edx = createSelectFolder(
        builder,
        createICMPFolder(builder, CmpInst::ICMP_EQ, signBit,
                         ConstantInt::get(signBit->getType(), 0)),
        ConstantInt::get(Type::getInt32Ty(context), 0),
        ConstantInt::get(Type::getInt32Ty(context), 0xFFFFFFFF), "setEDX");

    SetOperandValue(builder, instruction.operands[0], edx);
  }

  void lift_cqo(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {

    LLVMContext& context = builder.getContext();
    // if rax is -, then rdx is filled with ones FFFF_FFFF_FFFF_FFFF
    Value* rax = createZExtOrTruncFolder(
        builder,
        GetOperandValue(builder, instruction.operands[1],
                        instruction.operands[1].size),
        Type::getInt64Ty(context));

    Value* signBit = computeSignFlag(builder, rax);

    Value* rdx = createSelectFolder(
        builder,
        createICMPFolder(builder, CmpInst::ICMP_EQ, signBit,
                         ConstantInt::get(signBit->getType(), 0)),
        ConstantInt::get(Type::getInt64Ty(context), 0),
        ConstantInt::get(Type::getInt64Ty(context), 0xFFFFFFFFFFFFFFFF),
        "setRDX");
    printvalue(rax) printvalue(signBit) printvalue(rdx)
        SetOperandValue(builder, instruction.operands[0], rdx);
  }

  void lift_cbw(IRBuilder<>& builder,
                ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    Value* al = createZExtOrTruncFolder(
        builder,
        GetOperandValue(builder, instruction.operands[1],
                        instruction.operands[1].size),
        Type::getInt8Ty(context));

    Value* ax = createSExtFolder(builder, al, Type::getInt16Ty(context), "cbw");

    SetOperandValue(builder, instruction.operands[0], ax);
  }

  void lift_cwde(IRBuilder<>& builder,
                 ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();
    Value* ax = createZExtOrTruncFolder(
        builder,
        GetOperandValue(builder, instruction.operands[1],
                        instruction.operands[1].size),
        Type::getInt16Ty(context));
    printvalue(ax);
    Value* eax =
        createSExtFolder(builder, ax, Type::getInt32Ty(context), "cwde");
    printvalue(eax);
    SetOperandValue(builder, instruction.operands[0], eax);
  }

  void lift_cdqe(IRBuilder<>& builder,
                 ZydisDisassembledInstruction& instruction) {
    LLVMContext& context = builder.getContext();

    Value* eax = createZExtOrTruncFolder(
        builder,
        GetOperandValue(builder, instruction.operands[1],
                        instruction.operands[1].size),
        Type::getInt32Ty(context), "cdqe-trunc");

    Value* rax =
        createSExtFolder(builder, eax, Type::getInt64Ty(context), "cdqe");

    SetOperandValue(builder, instruction.operands[0], rax);
  }

} // namespace flagOperation

void liftInstructionSemantics(IRBuilder<>& builder,
                              ZydisDisassembledInstruction& instruction,
                              shared_ptr<vector<BBInfo>> blockAddresses,
                              bool& run) {

  switch (instruction.info.mnemonic) {
  // movs
  case ZYDIS_MNEMONIC_MOVAPS: {
    mov::lift_movaps(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_MOVUPS:
  case ZYDIS_MNEMONIC_MOVZX:
  case ZYDIS_MNEMONIC_MOVSX:
  case ZYDIS_MNEMONIC_MOVSXD:
  case ZYDIS_MNEMONIC_MOV: {
    mov::lift_mov(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_MOVSB: {
    mov::lift_movsb(builder, instruction);
    break;
  }

    // cmov
  case ZYDIS_MNEMONIC_CMOVZ: {
    cmov::lift_cmovz(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_CMOVNZ: {
    cmov::lift_cmovnz(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_CMOVL: {
    cmov::lift_cmovl(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_CMOVB: {
    cmov::lift_cmovb(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_CMOVNB: {
    cmov::lift_cmovnb(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_CMOVNS: {
    cmov::lift_cmovns(builder, instruction);
    break;
  }

  case ZYDIS_MNEMONIC_CMOVBE: {
    cmov::lift_cmovbz(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_CMOVNBE: {
    cmov::lift_cmovnbz(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_CMOVNL: {
    cmov::lift_cmovnl(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_CMOVS: {
    cmov::lift_cmovs(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_CMOVNLE: {
    cmov::lift_cmovnle(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_CMOVLE: {
    cmov::lift_cmovle(builder, instruction);
    break;
  }

  case ZYDIS_MNEMONIC_CMOVO: {
    cmov::lift_cmovo(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_CMOVNO: {
    cmov::lift_cmovno(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_CMOVP: {
    cmov::lift_cmovp(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_CMOVNP: {
    cmov::lift_cmovnp(builder, instruction);
    break;
  }
    // branches

  case ZYDIS_MNEMONIC_RET: {
    branches::lift_ret(builder, instruction, blockAddresses, run);
    break;
  }

  case ZYDIS_MNEMONIC_JMP: {
    branches::lift_jmp(builder, instruction, blockAddresses, run);
    break;
  }

  case ZYDIS_MNEMONIC_JNZ: {
    branches::lift_jnz(builder, instruction, blockAddresses);
    break;
  }
  case ZYDIS_MNEMONIC_JZ: {
    branches::lift_jz(builder, instruction, blockAddresses);
    break;
  }
  case ZYDIS_MNEMONIC_JS: {
    branches::lift_js(builder, instruction, blockAddresses);
    break;
  }
  case ZYDIS_MNEMONIC_JNS: {
    branches::lift_jns(builder, instruction, blockAddresses);
    break;
  }
  case ZYDIS_MNEMONIC_JNBE: {

    branches::lift_jnbe(builder, instruction, blockAddresses);
    break;
  }
  case ZYDIS_MNEMONIC_JNB: {
    branches::lift_jnb(builder, instruction, blockAddresses);
    break;
  }
  case ZYDIS_MNEMONIC_JB: {
    branches::lift_jb(builder, instruction, blockAddresses);
    break;
  }
  case ZYDIS_MNEMONIC_JBE: {

    branches::lift_jbe(builder, instruction, blockAddresses);
    break;
  }
  case ZYDIS_MNEMONIC_JNLE: {
    branches::lift_jnle(builder, instruction, blockAddresses);
    break;
  }
  case ZYDIS_MNEMONIC_JLE: {

    branches::lift_jle(builder, instruction, blockAddresses);
    break;
  }
  case ZYDIS_MNEMONIC_JNL: {

    branches::lift_jnl(builder, instruction, blockAddresses);
    break;
  }
  case ZYDIS_MNEMONIC_JL: {

    branches::lift_jl(builder, instruction, blockAddresses);
    break;
  }
  case ZYDIS_MNEMONIC_JO: {

    branches::lift_jo(builder, instruction, blockAddresses);
    break;
  }
  case ZYDIS_MNEMONIC_JNO: {

    branches::lift_jno(builder, instruction, blockAddresses);
    break;
  }
  case ZYDIS_MNEMONIC_JP: {

    branches::lift_jp(builder, instruction, blockAddresses);
    break;
  }
  case ZYDIS_MNEMONIC_JNP: {

    branches::lift_jnp(builder, instruction, blockAddresses);
    break;
  }
    // arithmetics and logical operations

  case ZYDIS_MNEMONIC_XCHG: {
    arithmeticsAndLogical::lift_xchg(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_CMPXCHG: {
    arithmeticsAndLogical::lift_cmpxchg(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_NOT: {
    arithmeticsAndLogical::lift_not(builder, instruction);
    break;
  }

  case ZYDIS_MNEMONIC_BSWAP: {
    arithmeticsAndLogical::lift_bswap(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_NEG: {
    arithmeticsAndLogical::lift_neg(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_SAR: {
    arithmeticsAndLogical::lift_sar(builder, instruction);
    break;
  }

  case ZYDIS_MNEMONIC_SHL: {
    arithmeticsAndLogical::lift_shl(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_SHLD: {
    arithmeticsAndLogical::lift_shld(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_SHRD: {
    arithmeticsAndLogical::lift_shrd(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_SHR: {
    arithmeticsAndLogical::lift_shr(builder, instruction);
    break;
  }

  case ZYDIS_MNEMONIC_RCR: {
    arithmeticsAndLogical::lift_rcr(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_RCL: {
    arithmeticsAndLogical::lift_rcl(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_SBB: {
    arithmeticsAndLogical::lift_sbb(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_ADC: {
    arithmeticsAndLogical::lift_adc(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_XADD: {
    arithmeticsAndLogical::lift_xadd(builder, instruction);
    break;
  }

  case ZYDIS_MNEMONIC_LEA: {
    arithmeticsAndLogical::lift_lea(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_INC:
  case ZYDIS_MNEMONIC_DEC: {
    arithmeticsAndLogical::lift_inc_dec(builder, instruction);
    break;
  }

  case ZYDIS_MNEMONIC_MUL: {
    arithmeticsAndLogical::lift_mul(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_IMUL: {
    arithmeticsAndLogical::lift_imul(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_DIV: {
    arithmeticsAndLogical::lift_div(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_IDIV: {
    arithmeticsAndLogical::lift_idiv(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_SUB:
  case ZYDIS_MNEMONIC_ADD: {
    arithmeticsAndLogical::lift_add_sub(builder, instruction);

    break;
  }

  case ZYDIS_MNEMONIC_XOR: {
    arithmeticsAndLogical::lift_xor(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_OR: {
    arithmeticsAndLogical::lift_or(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_AND: {
    arithmeticsAndLogical::lift_and(builder, instruction);

    break;
  }
  case ZYDIS_MNEMONIC_ROR: {
    arithmeticsAndLogical::lift_ror(builder, instruction);

    break;
  }
  case ZYDIS_MNEMONIC_ROL: {
    arithmeticsAndLogical::lift_rol(builder, instruction);

    break;
  }

  case ZYDIS_MNEMONIC_PUSH: {
    arithmeticsAndLogical::lift_push(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_PUSHFQ: {
    arithmeticsAndLogical::lift_pushfq(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_POP: {
    arithmeticsAndLogical::lift_pop(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_POPFQ: {
    arithmeticsAndLogical::lift_popfq(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_TEST: {
    arithmeticsAndLogical::lift_test(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_CMP: {
    arithmeticsAndLogical::lift_cmp(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_RDTSC: {
    arithmeticsAndLogical::lift_rdtsc(builder);
    break;
  }
  case ZYDIS_MNEMONIC_CPUID: {
    arithmeticsAndLogical::lift_cpuid(builder, instruction);
    break;
  }

  case ZYDIS_MNEMONIC_CALL: {
    branches::lift_call(builder, instruction, blockAddresses);
    break;
  }

  // set and flags
  case ZYDIS_MNEMONIC_STOSB:
  case ZYDIS_MNEMONIC_STOSW:
  case ZYDIS_MNEMONIC_STOSD:
  case ZYDIS_MNEMONIC_STOSQ: {
    flagOperation::lift_stosx(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_SETZ: {
    flagOperation::lift_setz(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_SETNZ: {
    flagOperation::lift_setnz(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_SETO: {
    flagOperation::lift_seto(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_SETNO: {
    flagOperation::lift_setno(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_SETNB: {
    flagOperation::lift_setnb(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_SETNBE: {
    flagOperation::lift_setnbe(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_SETBE: {
    flagOperation::lift_setbe(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_SETNS: {
    flagOperation::lift_setns(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_SETP: {
    flagOperation::lift_setp(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_SETNP: {
    flagOperation::lift_setnp(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_SETB: {
    flagOperation::lift_setb(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_SETS: {
    flagOperation::lift_sets(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_SETNLE: {
    flagOperation::lift_setnle(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_SETLE: {
    flagOperation::lift_setle(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_SETNL: {
    flagOperation::lift_setnl(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_SETL: {
    flagOperation::lift_setl(builder, instruction);
    break;
  }

  case ZYDIS_MNEMONIC_BTR: {
    flagOperation::lift_btr(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_BSR: {
    flagOperation::lift_bsr(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_BSF: {
    flagOperation::lift_bsf(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_BTC: {
    flagOperation::lift_btc(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_LAHF: {
    flagOperation::lift_lahf(builder);
    break;
  }
  case ZYDIS_MNEMONIC_STC: {
    flagOperation::lift_stc(builder);
    break;
  }
  case ZYDIS_MNEMONIC_CMC: {
    flagOperation::lift_cmc(builder);
    break;
  }
  case ZYDIS_MNEMONIC_CLC: {
    flagOperation::lift_clc(builder);
    break;
  }
  case ZYDIS_MNEMONIC_CLD: {
    flagOperation::lift_cld(builder);
    break;
  }
  case ZYDIS_MNEMONIC_CLI: {
    flagOperation::lift_cli(builder);
    break;
  }
  case ZYDIS_MNEMONIC_BTS: {
    flagOperation::lift_bts(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_BT: {
    flagOperation::lift_bt(builder, instruction);
    break;
  }

  case ZYDIS_MNEMONIC_CDQ: { // these are not related to flags at all
    flagOperation::lift_cdq(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_CWDE: {
    flagOperation::lift_cwde(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_CWD: {
    flagOperation::lift_cwd(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_CQO: {
    flagOperation::lift_cqo(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_CDQE: {
    flagOperation::lift_cdqe(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_CBW: {
    flagOperation::lift_cbw(builder, instruction);
    break;
  }
  case ZYDIS_MNEMONIC_PAUSE:
  case ZYDIS_MNEMONIC_NOP: {
    break;
  }

  default: {
    cout << "not implemented: " << instruction.info.mnemonic
         << " runtime: " << hex << instruction.runtime_address << " "
         << instruction.text << "\n";

    std::string Filename = "output_notimplemented.ll";
    std::error_code EC;
    raw_fd_ostream OS(Filename, EC);
    builder.GetInsertBlock()->getParent()->getParent()->print(OS, nullptr);

    throw std::runtime_error("not implemented");
    exit(-2);
  }
  }
}

void liftInstruction(IRBuilder<>& builder,
                     ZydisDisassembledInstruction& instruction,
                     shared_ptr<vector<BBInfo>> blockAddresses, bool& run) {
  LLVMContext& context = builder.getContext();
  // RIP gets updated before execution of the instruction.
  auto val = ConstantInt::getSigned(Type::getInt64Ty(context),
                                    instruction.runtime_address +
                                        instruction.info.length);
  SetRegisterValue(builder, ZYDIS_REGISTER_RIP, val);
  auto rsp = GetRegisterValue(builder, ZYDIS_REGISTER_RSP);
  printvalue(rsp);

  // I hate how getFunctionInfo returns a string pointer
  if (auto funcInfo =
          funcsignatures::getFunctionInfo(instruction.runtime_address)) {
    callFunctionIR(funcInfo->name.c_str(), builder, funcInfo);
    cout << "calling: " << funcInfo->name.c_str() << "\n";
    auto next_jump = popStack(builder);

    // get [rsp], jump there
    auto RIP_value = cast<ConstantInt>(next_jump);
    auto jump_address = RIP_value->getZExtValue();

    auto bb = BasicBlock::Create(context, "returnToOrgCF",
                                 builder.GetInsertBlock()->getParent());
    builder.CreateBr(bb);

    blockAddresses->push_back(make_tuple(jump_address, bb, getRegisters()));
    run = 0;
    return;
  }

  uint64_t jump_address = instruction.runtime_address + instruction.info.length;
  APInt temp;
  if (!BinaryOperations::readMemory(jump_address, 1, temp) &&
      cast<ConstantInt>(rsp)->getValue() != STACKP_VALUE) {
    auto bb = BasicBlock::Create(context, "returnToOrgCF",
                                 builder.GetInsertBlock()->getParent());
    // actually call the function first

    auto functionName = BinaryOperations::getName(jump_address);
    cout << "calling : " << functionName << " addr: " << (uint64_t)jump_address
         << endl;

    callFunctionIR(functionName, builder, nullptr);

    auto next_jump = popStack(builder);

    // get [rsp], jump there
    auto RIP_value = cast<ConstantInt>(next_jump);
    jump_address = RIP_value->getZExtValue();

    builder.CreateBr(bb);

    blockAddresses->push_back(make_tuple(jump_address, bb, getRegisters()));
    run = 0;
    return;
  }

  // do something for prefixes like rep here
  liftInstructionSemantics(builder, instruction, blockAddresses, run);
}