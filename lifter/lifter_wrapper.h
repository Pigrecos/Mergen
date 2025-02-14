#include "lifterClass.h"
#include "llvm-c/Core.h"

typedef std::function<LLVMValueRef()> CompFunc;

typedef lifterClass* TLifterRef;

// define a macro for the calling convention and export type
#define EXPORTCALL  extern "C" __declspec(dllexport)

// BBInfo
//
EXPORTCALL BBInfo* GetBlockInfo(lifterClass* lifter);
EXPORTCALL void SetBlockInfo(lifterClass* lifter, BBInfo info);
 
// LazyValue
//
EXPORTCALL LazyValue* LazyValue_CreateFromValue(LLVMValueRef value);
EXPORTCALL LazyValue* LazyValue_CreateFromCompute(CompFunc compute_func);
EXPORTCALL void LazyValue_Destroy(LazyValue* handle);
EXPORTCALL LLVMValueRef LazyValue_Get(LazyValue* handle);
EXPORTCALL void LazyValue_SetValue(LazyValue* handle, LLVMValueRef new_value);
EXPORTCALL void LazyValue_SetCompute(LazyValue* handle, CompFunc compute_func);

// Lifter
//
EXPORTCALL lifterClass* create_lifter_with_builder(LLVMBuilderRef builder);
EXPORTCALL lifterClass* create_lifter(lifterClass* lifter);
EXPORTCALL void destroy_lifter(lifterClass* lifter);
EXPORTCALL void branchHelper(lifterClass* lifter, LLVMValueRef condition, const char* instname, int numbered, int reverse);
EXPORTCALL void liftInstruction(lifterClass* lifter);
EXPORTCALL void liftInstructionSemantics(lifterClass* lifter);

// init
EXPORTCALL void Init_Flags(lifterClass* lifter);
EXPORTCALL void initDomTree(lifterClass* lifter, LLVMValueRef F);
// end init

// getters-setters
//
EXPORTCALL LLVMValueRef setFlag_Val(lifterClass* lifter, const Flag flag, LLVMValueRef newValue = nullptr);
EXPORTCALL void setFlag_Func(lifterClass* lifter, const Flag flag, CompFunc calculation);
EXPORTCALL LazyValue* getLazyFlag(lifterClass* lifter, const Flag flag);
EXPORTCALL LLVMValueRef getFlag(lifterClass* lifter, const Flag flag);
EXPORTCALL void InitRegisters(lifterClass* lifter, LLVMValueRef function, ZyanU64 rip);
EXPORTCALL LLVMValueRef GetValueFromHighByteRegister(lifterClass* lifter,const ZydisRegister reg);
EXPORTCALL LLVMValueRef GetRegisterValue(lifterClass* lifter, const ZydisRegister key);
EXPORTCALL LLVMValueRef SetValueToHighByteRegister(lifterClass* lifter, const ZydisRegister reg, LLVMValueRef value);
EXPORTCALL LLVMValueRef SetValueToSubRegister_8b(lifterClass* lifter, const ZydisRegister reg, LLVMValueRef value);
EXPORTCALL LLVMValueRef SetValueToSubRegister_16b(lifterClass* lifter, const ZydisRegister reg, LLVMValueRef value);
EXPORTCALL void SetRegisterValue(lifterClass* lifter, const ZydisRegister key, LLVMValueRef value);
EXPORTCALL void SetRFLAGSValue(lifterClass* lifter, LLVMValueRef value);
EXPORTCALL PATH_info solvePath(lifterClass* lifter, LLVMValueRef function, uint64_t& dest, LLVMValueRef simplifyValue);
EXPORTCALL LLVMValueRef popStack(lifterClass* lifter, int size);
EXPORTCALL void pushFlags(lifterClass* lifter, LLVMValueRef* values,  size_t count, const char* address);
EXPORTCALL LLVMValueRef* GetRFLAGS(lifterClass* lifter, size_t* outCount);
EXPORTCALL void lifter_FreeRFLAGS(LLVMValueRef* arr);
EXPORTCALL LLVMValueRef GetOperandValue(lifterClass* lifter, const ZydisDecodedOperand& op, const int possiblesize, const char* address = nullptr);
EXPORTCALL LLVMValueRef SetOperandValue(lifterClass* lifter, const ZydisDecodedOperand& op, LLVMValueRef value, const char* address = nullptr);
EXPORTCALL LLVMValueRef GetRFLAGSValue(lifterClass* lifter);
// end getters-setters
//

// misc
//
EXPORTCALL LLVMValueRef GetEffectiveAddress(lifterClass* lifter,const ZydisDecodedOperand& op,  const int possiblesize);
EXPORTCALL LLVMValueRef computeSignFlag(lifterClass* lifter, LLVMValueRef value);
EXPORTCALL LLVMValueRef computeZeroFlag(lifterClass* lifter, LLVMValueRef value);
EXPORTCALL LLVMValueRef computeParityFlag(lifterClass* lifter, LLVMValueRef value);
EXPORTCALL LLVMValueRef computeAuxFlag(lifterClass* lifter, LLVMValueRef Lvalue, LLVMValueRef Rvalue, LLVMValueRef result);
EXPORTCALL LLVMValueRef computeOverflowFlagSbb(lifterClass* lifter, LLVMValueRef Lvalue, LLVMValueRef Rvalue, LLVMValueRef cf, LLVMValueRef sub);
EXPORTCALL LLVMValueRef computeOverflowFlagSub(lifterClass* lifter, LLVMValueRef Lvalue, LLVMValueRef Rvalue, LLVMValueRef sub);
EXPORTCALL LLVMValueRef computeOverflowFlagAdd(lifterClass* lifter, LLVMValueRef Lvalue, LLVMValueRef Rvalue, LLVMValueRef add);
EXPORTCALL LLVMValueRef computeOverflowFlagAdc(lifterClass* lifter, LLVMValueRef Lvalue, LLVMValueRef Rvalue, LLVMValueRef cf, LLVMValueRef add);
// end misc
//

// folders
//
EXPORTCALL LLVMValueRef createSelectFolder(lifterClass* lifter, LLVMValueRef C, LLVMValueRef True, LLVMValueRef False, const char* name = nullptr);
EXPORTCALL LLVMValueRef createGEPFolder(lifterClass* lifter, LLVMTypeRef Type, LLVMValueRef Address, LLVMValueRef Base, const char* name = nullptr);
EXPORTCALL LLVMValueRef createAddFolder(lifterClass* lifter, LLVMValueRef LHS, LLVMValueRef RHS, const char* name = nullptr);
EXPORTCALL LLVMValueRef createSubFolder(lifterClass* lifter, LLVMValueRef LHS, LLVMValueRef RHS, const char* name = nullptr);
EXPORTCALL LLVMValueRef createOrFolder(lifterClass* lifter, LLVMValueRef LHS, LLVMValueRef RHS, const char* name = nullptr);
EXPORTCALL LLVMValueRef createXorFolder(lifterClass* lifter, LLVMValueRef LHS, LLVMValueRef RHS, const char* name = nullptr);
EXPORTCALL LLVMValueRef LLVMCreateICMPFolder(lifterClass* lifter, LLVMIntPredicate pred, LLVMValueRef lhs, LLVMValueRef rhs, const char* name = nullptr);
EXPORTCALL LLVMValueRef LLVMFolderBinOps(lifterClass* lifter, LLVMValueRef lhs, LLVMValueRef rhs, const char* name, LLVMOpcode opcode);
EXPORTCALL LLVMValueRef LLVMCreateNotFolder(lifterClass* lifter, LLVMValueRef lhs, const char* name = nullptr);
EXPORTCALL LLVMValueRef LLVMCreateMulFolder(lifterClass* lifter, LLVMValueRef lhs, LLVMValueRef rhs, const char* name = nullptr);
EXPORTCALL LLVMValueRef LLVMCreateSDivFolder(lifterClass* lifter, LLVMValueRef lhs, LLVMValueRef rhs, const char* name = nullptr);
EXPORTCALL LLVMValueRef LLVMCreateUDivFolder(lifterClass* lifter, LLVMValueRef lhs, LLVMValueRef rhs, const char* name = nullptr);
EXPORTCALL LLVMValueRef LLVMCreateSRemFolder(lifterClass* lifter, LLVMValueRef lhs, LLVMValueRef rhs, const char* name = nullptr);
EXPORTCALL LLVMValueRef LLVMCreateURemFolder(lifterClass* lifter, LLVMValueRef lhs, LLVMValueRef rhs, const char* name = nullptr);
EXPORTCALL LLVMValueRef LLVMCreateAShrFolder(lifterClass* lifter, LLVMValueRef lhs, LLVMValueRef rhs, const char* name = nullptr);
EXPORTCALL LLVMValueRef LLVMCreateAndFolder(lifterClass* lifter, LLVMValueRef lhs, LLVMValueRef rhs, const char* name = nullptr);
EXPORTCALL LLVMValueRef LLVMCreateTruncFolder(lifterClass* lifter, LLVMValueRef v, LLVMTypeRef destTy, const char* name = nullptr);
EXPORTCALL LLVMValueRef LLVMCreateZExtFolder(lifterClass* lifter, LLVMValueRef v, LLVMTypeRef destTy, const char* name = nullptr);
EXPORTCALL LLVMValueRef LLVMCreateZExtOrTruncFolder(lifterClass* lifter, LLVMValueRef v, LLVMTypeRef destTy, const char* name = nullptr);
EXPORTCALL LLVMValueRef LLVMCreateSExtFolder(lifterClass* lifter, LLVMValueRef v, LLVMTypeRef destTy, const char* name = nullptr);
EXPORTCALL LLVMValueRef LLVMCreateSExtOrTruncFolder(lifterClass* lifter, LLVMValueRef v, LLVMTypeRef destTy, const char* name = nullptr);
EXPORTCALL LLVMValueRef LLVMCreateLShrFolder(lifterClass* lifter, LLVMValueRef lhs, LLVMValueRef rhs, const char* name = nullptr);
EXPORTCALL LLVMValueRef LLVMCreateLShrFolderWithUInt(lifterClass* lifter, LLVMValueRef lhs, uint64_t rhs, const char* name = nullptr);
EXPORTCALL LLVMValueRef LLVMCreateShlFolder(lifterClass* lifter, LLVMValueRef lhs, LLVMValueRef rhs, const char* name = nullptr);
EXPORTCALL LLVMValueRef LLVMCreateShlFolderWithUInt(lifterClass* lifter, LLVMValueRef lhs, uint64_t rhs, const char* name = nullptr);
EXPORTCALL LLVMValueRef createInstruction(lifterClass* lifter, const unsigned opcode, LLVMValueRef operand1, LLVMValueRef operand2, LLVMTypeRef destType, const char* name = nullptr);
EXPORTCALL LLVMValueRef LLVMDoPatternMatching(lifterClass* lifter, LLVMOpcode opcode, LLVMValueRef op0, LLVMValueRef op1);
// end folders
//

// analysis
//
EXPORTCALL void markMemPaged(lifterClass* lifter, const int64_t start, const int64_t end);
EXPORTCALL bool isMemPaged(lifterClass* lifter, const int64_t address);
EXPORTCALL isPaged isValuePaged(lifterClass* lifter, LLVMValueRef address, LLVMValueRef ctxI);
EXPORTCALL void pagedCheck(lifterClass* lifter, LLVMValueRef address, LLVMValueRef ctxI);
EXPORTCALL void loadMemoryOp(lifterClass* lifter, LLVMValueRef inst);;
EXPORTCALL void insertMemoryOp(lifterClass* lifter, StoreInst* inst);
// end analysis
//

EXPORTCALL LLVMBuilderRef get_builder_from_lifter(lifterClass* lifter);
EXPORTCALL LLVMValueRef get_Function(lifterClass* lifter);
EXPORTCALL void set_Function(lifterClass* lifter, LLVMValueRef F);
EXPORTCALL void LF_final_optpass(LLVMValueRef clonedFuncx);
EXPORTCALL uint16_t LF_SetFileData(const char* filename);
EXPORTCALL void LF_SETRun(lifterClass* lifter, bool lRun);
EXPORTCALL void LF_SETFinished(lifterClass* lifter, bool lFinished);
EXPORTCALL bool LF_GETRun(lifterClass* lifter);
EXPORTCALL bool LF_GETFinished(lifterClass* lifter);
EXPORTCALL void LF_SetInstruz(lifterClass* lifter, ZydisDecodedInstruction instruction);
EXPORTCALL void LF_SetInstruzOperand(lifterClass* lifter, ZydisDecodedOperand* ops);
// Restituisce un array allocato dinamicamente di TLifterRef e imposta *count al numero di elementi.
EXPORTCALL TLifterRef* LF_GetLifters(size_t* count);
EXPORTCALL void LF_SetLifters(TLifterRef* arr, size_t count);
EXPORTCALL void LF_FreeLifters(TLifterRef* array);
EXPORTCALL void LF_SetdbgCallBack(DebugCallback cbDbg);
EXPORTCALL void LF_SetMemoryCallBack(MemoryMappingCallback cbMem);

// binaryOperation
EXPORTCALL bool BO_isWrittenTo(uint64_t addr);

// semantics definition
EXPORTCALL void lift_movs_X(lifterClass* lifter);
// EXPORTCALL void lift_movaps(lifterClass* lifter);
EXPORTCALL void lift_mov(lifterClass* lifter);
EXPORTCALL void lift_cmovbz(lifterClass* lifter);
EXPORTCALL void lift_cmovnbz(lifterClass* lifter);
EXPORTCALL void lift_cmovz(lifterClass* lifter);
EXPORTCALL void lift_cmovnz(lifterClass* lifter);
EXPORTCALL void lift_cmovl(lifterClass* lifter);
EXPORTCALL void lift_cmovnl(lifterClass* lifter);
EXPORTCALL void lift_cmovb(lifterClass* lifter);
EXPORTCALL void lift_cmovnb(lifterClass* lifter);
EXPORTCALL void lift_cmovns(lifterClass* lifter);
EXPORTCALL void lift_cmovs(lifterClass* lifter);
EXPORTCALL void lift_cmovnle(lifterClass* lifter);
EXPORTCALL void lift_cmovle(lifterClass* lifter);
EXPORTCALL void lift_cmovo(lifterClass* lifter);
EXPORTCALL void lift_cmovno(lifterClass* lifter);
EXPORTCALL void lift_cmovp(lifterClass* lifter);
EXPORTCALL void lift_cmovnp(lifterClass* lifter);
EXPORTCALL void lift_popcnt(lifterClass* lifter);
//
EXPORTCALL void lift_call(lifterClass* lifter);
EXPORTCALL void lift_ret(lifterClass* lifter);
EXPORTCALL void lift_jmp(lifterClass* lifter);
EXPORTCALL void lift_jnz(lifterClass* lifter);
EXPORTCALL void lift_jz(lifterClass* lifter);
EXPORTCALL void lift_js(lifterClass* lifter);
EXPORTCALL void lift_jns(lifterClass* lifter);
EXPORTCALL void lift_jle(lifterClass* lifter);
EXPORTCALL void lift_jl(lifterClass* lifter);
EXPORTCALL void lift_jnl(lifterClass* lifter);
EXPORTCALL void lift_jnle(lifterClass* lifter);
EXPORTCALL void lift_jbe(lifterClass* lifter);
EXPORTCALL void lift_jb(lifterClass* lifter);
EXPORTCALL void lift_jnb(lifterClass* lifter);
EXPORTCALL void lift_jnbe(lifterClass* lifter);
EXPORTCALL void lift_jo(lifterClass* lifter);
EXPORTCALL void lift_jno(lifterClass* lifter);
EXPORTCALL void lift_jp(lifterClass* lifter);
EXPORTCALL void lift_jnp(lifterClass* lifter);
//
EXPORTCALL void lift_sbb(lifterClass* lifter);
EXPORTCALL void lift_rcl(lifterClass* lifter);
EXPORTCALL void lift_rcr(lifterClass* lifter);
EXPORTCALL void lift_not(lifterClass* lifter);
EXPORTCALL void lift_neg(lifterClass* lifter);
EXPORTCALL void lift_sar(lifterClass* lifter);
EXPORTCALL void lift_shr(lifterClass* lifter);
EXPORTCALL void lift_shl(lifterClass* lifter);
EXPORTCALL void lift_bswap(lifterClass* lifter);
EXPORTCALL void lift_cmpxchg(lifterClass* lifter);
EXPORTCALL void lift_xchg(lifterClass* lifter);
EXPORTCALL void lift_shld(lifterClass* lifter);
EXPORTCALL void lift_shrd(lifterClass* lifter);
EXPORTCALL void lift_lea(lifterClass* lifter);
EXPORTCALL void lift_add_sub(lifterClass* lifter);
EXPORTCALL void lift_imul2(lifterClass* lifter, const bool isSigned);
EXPORTCALL void lift_imul(lifterClass* lifter);
EXPORTCALL void lift_mul(lifterClass* lifter);
// EXPORTCALL void lift_div2(lifterClass* lifter);
EXPORTCALL void lift_div(lifterClass* lifter);
// EXPORTCALL void lift_idiv2(lifterClass* lifter);
EXPORTCALL void lift_idiv(lifterClass* lifter);
EXPORTCALL void lift_xor(lifterClass* lifter);
EXPORTCALL void lift_or(lifterClass* lifter);
EXPORTCALL void lift_and(lifterClass* lifter);
EXPORTCALL void lift_andn(lifterClass* lifter);
EXPORTCALL void lift_rol(lifterClass* lifter);
EXPORTCALL void lift_ror(lifterClass* lifter);
EXPORTCALL void lift_inc(lifterClass* lifter);
EXPORTCALL void lift_dec(lifterClass* lifter);
EXPORTCALL void lift_push(lifterClass* lifter);
EXPORTCALL void lift_pushfq(lifterClass* lifter);
EXPORTCALL void lift_pop(lifterClass* lifter);
EXPORTCALL void lift_popfq(lifterClass* lifter);

EXPORTCALL void lift_leave(lifterClass* lifter);

EXPORTCALL void lift_adc(lifterClass* lifter);
EXPORTCALL void lift_xadd(lifterClass* lifter);
EXPORTCALL void lift_test(lifterClass* lifter);
EXPORTCALL void lift_cmp(lifterClass* lifter);
EXPORTCALL void lift_rdtsc(lifterClass* lifter);
EXPORTCALL void lift_cpuid(lifterClass* lifter);
EXPORTCALL void lift_pext(lifterClass* lifter);
//
EXPORTCALL void lift_setnz(lifterClass* lifter);
EXPORTCALL void lift_seto(lifterClass* lifter);
EXPORTCALL void lift_setno(lifterClass* lifter);
EXPORTCALL void lift_setnb(lifterClass* lifter);
EXPORTCALL void lift_setbe(lifterClass* lifter);
EXPORTCALL void lift_setnbe(lifterClass* lifter);
EXPORTCALL void lift_setns(lifterClass* lifter);
EXPORTCALL void lift_setp(lifterClass* lifter);
EXPORTCALL void lift_setnp(lifterClass* lifter);
EXPORTCALL void lift_setb(lifterClass* lifter);
EXPORTCALL void lift_sets(lifterClass* lifter);
EXPORTCALL void lift_stosx(lifterClass* lifter);
EXPORTCALL void lift_setz(lifterClass* lifter);
EXPORTCALL void lift_setnle(lifterClass* lifter);
EXPORTCALL void lift_setle(lifterClass* lifter);
EXPORTCALL void lift_setnl(lifterClass* lifter);
EXPORTCALL void lift_setl(lifterClass* lifter);
EXPORTCALL void lift_bt(lifterClass* lifter);
EXPORTCALL void lift_btr(lifterClass* lifter);
EXPORTCALL void lift_bts(lifterClass* lifter);
EXPORTCALL void lift_lzcnt(lifterClass* lifter);
EXPORTCALL void lift_bzhi(lifterClass* lifter);
EXPORTCALL void lift_bsr(lifterClass* lifter);
EXPORTCALL void lift_bsf(lifterClass* lifter);
EXPORTCALL void lift_blsr(lifterClass* lifter);
EXPORTCALL void lift_tzcnt(lifterClass* lifter);
EXPORTCALL void lift_btc(lifterClass* lifter);
EXPORTCALL void lift_lahf(lifterClass* lifter);
EXPORTCALL void lift_sahf(lifterClass* lifter);
EXPORTCALL void lift_std(lifterClass* lifter);
EXPORTCALL void lift_stc(lifterClass* lifter);
EXPORTCALL void lift_cmc(lifterClass* lifter);
EXPORTCALL void lift_clc(lifterClass* lifter);
EXPORTCALL void lift_cld(lifterClass* lifter);
EXPORTCALL void lift_cli(lifterClass* lifter);
EXPORTCALL void lift_cwd(lifterClass* lifter);
EXPORTCALL void lift_cdq(lifterClass* lifter);
EXPORTCALL void lift_cqo(lifterClass* lifter);
EXPORTCALL void lift_cbw(lifterClass* lifter);
EXPORTCALL void lift_cwde(lifterClass* lifter);
EXPORTCALL void lift_cdqe(lifterClass* lifter);
EXPORTCALL void lift_bextr(lifterClass* lifter);
// end semantics definition

