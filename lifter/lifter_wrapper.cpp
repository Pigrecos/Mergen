#include "lifter_wrapper.h"

#include <fstream>
#include <iostream>

// BBInfo
//
BBInfo* GetBlockInfo(lifterClass* lifter) { 
  BBInfo info = lifter->blockInfo;
  return new BBInfo(info);
}

void SetBlockInfo(lifterClass* lifter, BBInfo info) {
  lifter->blockInfo = info;
}

// LazyValue
//
LazyValue* LazyValue_CreateFromValue(LLVMValueRef value) {
  return new LazyValue(reinterpret_cast<llvm::Value*>(value));
}

LazyValue* LazyValue_CreateFromCompute(CompFunc compute_func) {
  // Creiamo una lambda che converte il risultato:
  auto lambda = [compute_func]() -> llvm::Value* {
    // Poiché LLVMValueRef è definito come un puntatore opaco a llvm::Value,
    // possiamo fare il cast (qui uso reinterpret_cast; in alcuni casi
    // static_cast può bastare)
    return reinterpret_cast<llvm::Value*>(compute_func());
  };

  // Utilizziamo il costruttore di LazyValue che accetta una ComputeFunc
  return new LazyValue(lambda);
}

void LazyValue_Destroy(LazyValue* handle) { delete handle; }

LLVMValueRef LazyValue_Get(LazyValue* handle) {
  auto lv = static_cast<LazyValue*>(handle);
  return reinterpret_cast<LLVMValueRef>(*lv->value);
}

void LazyValue_SetValue(LazyValue* handle, LLVMValueRef new_value) {
  auto lv = static_cast<LazyValue*>(handle);
  lv->value = std::make_optional(reinterpret_cast<llvm::Value*>(new_value));
}

void LazyValue_SetCompute(LazyValue* handle, CompFunc compute_func) {
  auto lambda = [compute_func]() -> llvm::Value* {
    return reinterpret_cast<llvm::Value*>(compute_func());
  };

  handle->setCalculation(lambda);
}

// Lifter
//
lifterClass* create_lifter_with_builder(LLVMBuilderRef builder) {
  
    llvm::IRBuilder<>* nativeBuilder = unwrap(builder);
    
    return new lifterClass(*nativeBuilder);
}

lifterClass* create_lifter(lifterClass* lifter) {
  return new lifterClass(*lifter);
}

void destroy_lifter(lifterClass* lifter) { delete lifter; }

void branchHelper(lifterClass* lifter, LLVMValueRef condition,
                  const char* instname, int numbered, int reverse) {
  if (!lifter || !condition || !instname)
    return;

  // Convertiamo LLVMValueRef a llvm::Value*
  llvm::Value* cond = reinterpret_cast<llvm::Value*>(condition);

  // Chiamiamo la funzione originale
  lifter->branchHelper(cond, std::string(instname), numbered, reverse != 0);
}

void liftInstruction(lifterClass* lifter) { 
    lifter->liftInstruction(); 
}

void liftInstructionSemantics(lifterClass* lifter) {
  lifter->liftInstructionSemantics();
}

// init
void Init_Flags(lifterClass* lifter) { lifter->Init_Flags(); }

void initDomTree(lifterClass* lifter, LLVMValueRef F) {
  // Convertiamo LLVMValueRef in llvm::Value*
  llvm::Value* val = reinterpret_cast<llvm::Value*>(F);

  // Verifichiamo che il valore sia effettivamente una funzione
  llvm::Function* func = llvm::dyn_cast<llvm::Function>(val);
  if (!func) {
    // Se non lo è, possiamo gestire l'errore in base alle esigenze
    // Ad esempio, potremmo loggare un errore, lanciare un'eccezione o
    // terminare la funzione
    return;
  }

  // Ora invochiamo la funzione membro di lifter passando la funzione come
  // riferimento
  lifter->initDomTree(*func);
}
// end init

// getters-setters
//
LLVMValueRef setFlag_Val(lifterClass* lifter, const Flag flag,
                         LLVMValueRef newValue) {
  // Convertiamo il parametro newValue da LLVMValueRef a llvm::Value*
  llvm::Value* nativeNewValue = reinterpret_cast<llvm::Value*>(newValue);

  // Chiamiamo il metodo originale sull'oggetto lifter.
  llvm::Value* result = lifter->setFlag(flag, nativeNewValue);

  // Convertiamo il risultato da llvm::Value* a LLVMValueRef e lo
  // restituiamo.
  return reinterpret_cast<LLVMValueRef>(result);
}

void setFlag_Func(lifterClass* lifter, const Flag flag, CompFunc calculation) {
  // Costruiamo una lambda che converte il risultato di calculation() da
  // LLVMValueRef a llvm::Value*
  auto lambda = [calculation]() -> llvm::Value* {
    // reinterpret_cast (oppure static_cast se appropriato) per convertire
    // da LLVMValueRef a llvm::Value*
    return reinterpret_cast<llvm::Value*>(calculation());
  };

  // Ora chiamiamo il metodo originale passando il flag e la lambda
  // convertita
  lifter->setFlag(flag, lambda);
}

LazyValue* getLazyFlag(lifterClass* lifter, const Flag flag) {
  // Chiamata alla funzione originale che restituisce un LazyValue per
  // valore
  LazyValue lv = lifter->getLazyFlag(flag);

  // Allochiamo un nuovo LazyValue sullo heap, copiando il valore ottenuto
  LazyValue* pLv = new LazyValue(lv);

  // Restituiamo il puntatore all'oggetto allocato
  return pLv;
}

LLVMValueRef getFlag(lifterClass* lifter, const Flag flag) {

  auto res = lifter->getFlag(flag);

  return reinterpret_cast<LLVMValueRef>(res);
}

void InitRegisters(lifterClass* lifter, LLVMValueRef function, ZyanU64 rip) {
  // Convertiamo LLVMValueRef in llvm::Value*
  llvm::Value* val = reinterpret_cast<llvm::Value*>(function);

  // Verifichiamo che il valore sia effettivamente una funzione
  llvm::Function* func = llvm::dyn_cast<llvm::Function>(val);
  if (!func) {
    // Se non lo è, possiamo gestire l'errore in base alle esigenze
    // Ad esempio, potremmo loggare un errore, lanciare un'eccezione o
    // terminare la funzione
    return;
  }
  lifter->InitRegisters(func, rip);
}

LLVMValueRef GetValueFromHighByteRegister(lifterClass* lifter,
                                          const ZydisRegister reg) {

  auto res = lifter->GetValueFromHighByteRegister(reg);

  return reinterpret_cast<LLVMValueRef>(res);
}

LLVMValueRef GetRegisterValue(lifterClass* lifter, const ZydisRegister key) {

  auto res = lifter->GetRegisterValue(key);

  return reinterpret_cast<LLVMValueRef>(res);
}

LLVMValueRef SetValueToHighByteRegister(lifterClass* lifter,
                                        const ZydisRegister reg,
                                        LLVMValueRef value) {

  // Convertiamo il parametro newValue da LLVMValueRef a llvm::Value*
  llvm::Value* nativeNewValue = reinterpret_cast<llvm::Value*>(value);

  auto res = lifter->SetValueToHighByteRegister(reg, nativeNewValue);

  return reinterpret_cast<LLVMValueRef>(res);
}

LLVMValueRef SetValueToSubRegister_8b(lifterClass* lifter,
                                      const ZydisRegister reg,
                                      LLVMValueRef value) {

  // Convertiamo il parametro newValue da LLVMValueRef a llvm::Value*
  llvm::Value* nativeNewValue = reinterpret_cast<llvm::Value*>(value);

  auto res = lifter->SetValueToSubRegister_8b(reg, nativeNewValue);

  return reinterpret_cast<LLVMValueRef>(res);
}

LLVMValueRef SetValueToSubRegister_16b(lifterClass* lifter,
                                       const ZydisRegister reg,
                                       LLVMValueRef value) {

  // Convertiamo il parametro newValue da LLVMValueRef a llvm::Value*
  llvm::Value* nativeNewValue = reinterpret_cast<llvm::Value*>(value);

  auto res = lifter->SetValueToSubRegister_16b(reg, nativeNewValue);

  return reinterpret_cast<LLVMValueRef>(res);
}

void SetRegisterValue(lifterClass* lifter, const ZydisRegister key,
                      LLVMValueRef value) {

  // Convertiamo il parametro newValue da LLVMValueRef a llvm::Value*
  llvm::Value* nativeNewValue = reinterpret_cast<llvm::Value*>(value);

  lifter->SetRegisterValue(key, nativeNewValue);
}

void SetRFLAGSValue(lifterClass* lifter, LLVMValueRef value) {

  // Convertiamo il parametro newValue da LLVMValueRef a llvm::Value*
  llvm::Value* nativeNewValue = reinterpret_cast<llvm::Value*>(value);

  lifter->SetRFLAGSValue(nativeNewValue);
}

PATH_info solvePath(lifterClass* lifter, LLVMValueRef function, uint64_t& dest,
                    LLVMValueRef simplifyValue) {

  // Convertiamo il parametro newValue da LLVMValueRef a llvm::Value*
  llvm::Value* simplifyVal = reinterpret_cast<llvm::Value*>(simplifyValue);

  // Convertiamo LLVMValueRef in llvm::Value*
  llvm::Value* val = reinterpret_cast<llvm::Value*>(function);

  // Verifichiamo che il valore sia effettivamente una funzione
  llvm::Function* func = llvm::dyn_cast<llvm::Function>(val);
  if (!func) {
    // Se non lo è, possiamo gestire l'errore in base alle esigenze
    // Ad esempio, potremmo loggare un errore, lanciare un'eccezione o
    // terminare la funzione
    return PATH_info::PATH_unsolved;
  }

  return lifter->solvePath(func, dest, simplifyVal);
}

LLVMValueRef popStack(lifterClass* lifter, int size) {

  auto res = lifter->popStack(size);

  return reinterpret_cast<LLVMValueRef>(res);
}

void pushFlags(lifterClass* lifter, LLVMValueRef* values, size_t count,
               const char* address) {

  // Costruisce lo std::vector a partire dall'array values
  std::vector<llvm::Value*> flags;
  flags.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    // Convertiamo da LLVMValueRef a llvm::Value*
    flags.push_back(reinterpret_cast<llvm::Value*>(values[i]));
  }

  // Converte la stringa C in std::string
  std::string addr(address ? address : "");

  // Chiama la funzione originale
  lifter->pushFlags(flags, addr);
}

// Restituisce un array allocato dinamicamente di LLVMValueRef
// e imposta *outCount al numero di elementi.
LLVMValueRef* GetRFLAGS(lifterClass* lifter, size_t* outCount) {

  // Ottiene il vettore dalla funzione C++ originale
  std::vector<llvm::Value*> vec = lifter->GetRFLAGS();

  // Imposta il conteggio
  if (outCount) {
    *outCount = vec.size();
  }

  // Alloca un array di LLVMValueRef per restituirlo all'interfaccia C
  LLVMValueRef* arr = new LLVMValueRef[vec.size()];

  // Copia ogni elemento convertendo da llvm::Value* a LLVMValueRef
  for (size_t i = 0; i < vec.size(); ++i) {
    arr[i] = reinterpret_cast<LLVMValueRef>(vec[i]);
  }

  return arr;
}

// Funzione per liberare la memoria allocata da lifter_GetRFLAGS
void lifter_FreeRFLAGS(LLVMValueRef* arr) { delete[] arr; }

LLVMValueRef GetOperandValue(lifterClass* lifter, const ZydisDecodedOperand& op,
                             const int possiblesize,
                             const char* address) {

  // Converte la stringa C in std::string
  std::string addr(address ? address : "");

  auto res = lifter->GetOperandValue(op, possiblesize, addr);

  return reinterpret_cast<LLVMValueRef>(res);
}

LLVMValueRef SetOperandValue(lifterClass* lifter, const ZydisDecodedOperand& op,
                             LLVMValueRef value,
                             const char* address) {

  // Converte la stringa C in std::string
  std::string addr(address ? address : "");

  // Convertiamo LLVMValueRef in llvm::Value*
  llvm::Value* val = reinterpret_cast<llvm::Value*>(value);

  auto res = lifter->SetOperandValue(op, val, addr);

  return reinterpret_cast<LLVMValueRef>(res);
}

LLVMValueRef GetRFLAGSValue(lifterClass* lifter) {

  auto res = lifter->GetRFLAGSValue();

  return reinterpret_cast<LLVMValueRef>(res);
}

// end getters-setters
//

// misc
//
LLVMValueRef GetEffectiveAddress(lifterClass* lifter,
                                 const ZydisDecodedOperand& op,
                                 const int possiblesize) {

  auto res = lifter->GetEffectiveAddress(op, possiblesize);

  return reinterpret_cast<LLVMValueRef>(res);
}

LLVMValueRef computeSignFlag(lifterClass* lifter, LLVMValueRef value) {

  auto res = lifter->computeSignFlag(reinterpret_cast<llvm::Value*>(value));

  return reinterpret_cast<LLVMValueRef>(res);
}

LLVMValueRef computeZeroFlag(lifterClass* lifter, LLVMValueRef value) {

  auto res = lifter->computeZeroFlag(reinterpret_cast<llvm::Value*>(value));

  return reinterpret_cast<LLVMValueRef>(res);
}

LLVMValueRef computeParityFlag(lifterClass* lifter, LLVMValueRef value) {

  auto res = lifter->computeParityFlag(reinterpret_cast<llvm::Value*>(value));

  return reinterpret_cast<LLVMValueRef>(res);
}

LLVMValueRef computeAuxFlag(lifterClass* lifter, LLVMValueRef Lvalue,
                            LLVMValueRef Rvalue, LLVMValueRef result) {

  auto lV = reinterpret_cast<llvm::Value*>(Lvalue);
  auto rV = reinterpret_cast<llvm::Value*>(Rvalue);
  auto resV = reinterpret_cast<llvm::Value*>(result);

  auto res = lifter->computeAuxFlag(lV, rV, resV);

  return reinterpret_cast<LLVMValueRef>(res);
}

LLVMValueRef computeOverflowFlagSbb(lifterClass* lifter, LLVMValueRef Lvalue,
                                    LLVMValueRef Rvalue, LLVMValueRef cf,
                                    LLVMValueRef sub) {

  auto lV = reinterpret_cast<llvm::Value*>(Lvalue);
  auto rV = reinterpret_cast<llvm::Value*>(Rvalue);
  auto cfV = reinterpret_cast<llvm::Value*>(cf);
  auto subV = reinterpret_cast<llvm::Value*>(sub);

  auto res = lifter->computeOverflowFlagSbb(lV, rV, cfV, subV);

  return reinterpret_cast<LLVMValueRef>(res);
}

LLVMValueRef computeOverflowFlagSub(lifterClass* lifter, LLVMValueRef Lvalue,
                                    LLVMValueRef Rvalue, LLVMValueRef sub) {

  auto lV = reinterpret_cast<llvm::Value*>(Lvalue);
  auto rV = reinterpret_cast<llvm::Value*>(Rvalue);
  auto subV = reinterpret_cast<llvm::Value*>(sub);

  auto res = lifter->computeOverflowFlagSub(lV, rV, subV);

  return reinterpret_cast<LLVMValueRef>(res);
}

LLVMValueRef computeOverflowFlagAdd(lifterClass* lifter, LLVMValueRef Lvalue,
                                    LLVMValueRef Rvalue, LLVMValueRef add) {

  auto lV = reinterpret_cast<llvm::Value*>(Lvalue);
  auto rV = reinterpret_cast<llvm::Value*>(Rvalue);
  auto addV = reinterpret_cast<llvm::Value*>(add);

  auto res = lifter->computeOverflowFlagAdd(lV, rV, addV);

  return reinterpret_cast<LLVMValueRef>(res);
}

LLVMValueRef computeOverflowFlagAdc(lifterClass* lifter, LLVMValueRef Lvalue,
                                    LLVMValueRef Rvalue, LLVMValueRef cf,
                                    LLVMValueRef add) {

  auto lV = reinterpret_cast<llvm::Value*>(Lvalue);
  auto rV = reinterpret_cast<llvm::Value*>(Rvalue);
  auto cfV = reinterpret_cast<llvm::Value*>(cf);
  auto addV = reinterpret_cast<llvm::Value*>(add);

  auto res = lifter->computeOverflowFlagAdc(lV, rV, cfV, addV);

  return reinterpret_cast<LLVMValueRef>(res);
}
// end misc
//

// folders
//
LLVMValueRef createSelectFolder(lifterClass* lifter, LLVMValueRef C,
                                LLVMValueRef True, LLVMValueRef False,
                                const char* name) {

  // Converte il const char* in llvm::Twine: se name è NULL, usa una Twine vuota
  llvm::Twine twineName = name ? llvm::Twine(name) : llvm::Twine("");

  llvm::Value* result = lifter->createSelectFolder(
      reinterpret_cast<llvm::Value*>(C), reinterpret_cast<llvm::Value*>(True),
      reinterpret_cast<llvm::Value*>(True), twineName);

  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef createGEPFolder(lifterClass* lifter, LLVMTypeRef Type,
                             LLVMValueRef Address, LLVMValueRef Base,
                             const char* name) {

  // Converte il const char* in llvm::Twine: se name è NULL, usa una Twine vuota
  llvm::Twine twineName = name ? llvm::Twine(name) : llvm::Twine("");

  llvm::Value* result =
      lifter->createGEPFolder(reinterpret_cast<llvm::Type*>(Type),
                              reinterpret_cast<llvm::Value*>(Address),
                              reinterpret_cast<llvm::Value*>(Base), twineName);

  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef createAddFolder(lifterClass* lifter, LLVMValueRef LHS,
                             LLVMValueRef RHS, const char* name) {

  // Converte il const char* in llvm::Twine: se name è NULL, usa una Twine vuota
  llvm::Twine twineName = name ? llvm::Twine(name) : llvm::Twine("");

  llvm::Value* result =
      lifter->createAddFolder(reinterpret_cast<llvm::Value*>(LHS),
                              reinterpret_cast<llvm::Value*>(RHS), twineName);

  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef createSubFolder(lifterClass* lifter, LLVMValueRef LHS,
                             LLVMValueRef RHS, const char* name) {

  // Converte il const char* in llvm::Twine: se name è NULL, usa una Twine vuota
  llvm::Twine twineName = name ? llvm::Twine(name) : llvm::Twine("");

  llvm::Value* result =
      lifter->createSubFolder(reinterpret_cast<llvm::Value*>(LHS),
                              reinterpret_cast<llvm::Value*>(RHS), twineName);

  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef createOrFolder(lifterClass* lifter, LLVMValueRef LHS,
                            LLVMValueRef RHS, const char* name) {

  // Converte il const char* in llvm::Twine: se name è NULL, usa una Twine vuota
  llvm::Twine twineName = name ? llvm::Twine(name) : llvm::Twine("");

  llvm::Value* result =
      lifter->createOrFolder(reinterpret_cast<llvm::Value*>(LHS),
                             reinterpret_cast<llvm::Value*>(RHS), twineName);

  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef createXorFolder(lifterClass* lifter, LLVMValueRef LHS,
                             LLVMValueRef RHS, const char* name) {

  // Converte il const char* in llvm::Twine: se name è NULL, usa una Twine vuota
  llvm::Twine twineName = name ? llvm::Twine(name) : llvm::Twine("");

  llvm::Value* result =
      lifter->createXorFolder(reinterpret_cast<llvm::Value*>(LHS),
                              reinterpret_cast<llvm::Value*>(RHS), twineName);

  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef LLVMCreateICMPFolder(lifterClass* lifter, LLVMIntPredicate pred,
                                  LLVMValueRef lhs, LLVMValueRef rhs,
                                  const char* name) {
  // Converte il const char* in llvm::Twine: se name è NULL, usa una Twine vuota
  llvm::Twine twineName = name ? llvm::Twine(name) : llvm::Twine("");

  // Chiama la funzione originale; il cast dell'enum è possibile se i valori
  // corrispondono
  llvm::Value* result =
      lifter->createICMPFolder(static_cast<llvm::CmpInst::Predicate>(pred),
                               reinterpret_cast<llvm::Value*>(lhs),
                               reinterpret_cast<llvm::Value*>(rhs), twineName);

  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef LLVMFolderBinOps(lifterClass* lifter, LLVMValueRef lhs,
                              LLVMValueRef rhs, const char* name,
                              LLVMOpcode opcode) {

  // Converte la stringa C in una llvm::Twine (usando una stringa vuota se name
  // è NULL)
  llvm::Twine twineName = name ? llvm::Twine(name) : llvm::Twine("");

  // Chiama la funzione originale, convertendo opcode all’enum C++
  // corrispondente
  llvm::Value* result = lifter->folderBinOps(
      reinterpret_cast<llvm::Value*>(lhs), reinterpret_cast<llvm::Value*>(rhs),
      twineName, static_cast<llvm::Instruction::BinaryOps>(opcode));

  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef LLVMCreateNotFolder(lifterClass* lifter, LLVMValueRef lhs,
                                 const char* name) {

  llvm::Twine twineName = (name ? llvm::Twine(name) : llvm::Twine(""));
  llvm::Value* result =
      lifter->createNotFolder(reinterpret_cast<llvm::Value*>(lhs), twineName);
  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef LLVMCreateMulFolder(lifterClass* lifter, LLVMValueRef lhs,
                                 LLVMValueRef rhs, const char* name) {
  llvm::Twine twineName = (name ? llvm::Twine(name) : llvm::Twine(""));
  llvm::Value* result =
      lifter->createMulFolder(reinterpret_cast<llvm::Value*>(lhs),
                              reinterpret_cast<llvm::Value*>(rhs), twineName);
  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef LLVMCreateSDivFolder(lifterClass* lifter, LLVMValueRef lhs,
                                  LLVMValueRef rhs,
                                  const char* name) {
  llvm::Twine twineName = (name ? llvm::Twine(name) : llvm::Twine(""));
  llvm::Value* result =
      lifter->createSDivFolder(reinterpret_cast<llvm::Value*>(lhs),
                               reinterpret_cast<llvm::Value*>(rhs), twineName);
  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef LLVMCreateUDivFolder(lifterClass* lifter, LLVMValueRef lhs,
                                  LLVMValueRef rhs,
                                  const char* name) {
  llvm::Twine twineName = (name ? llvm::Twine(name) : llvm::Twine(""));
  llvm::Value* result =
      lifter->createUDivFolder(reinterpret_cast<llvm::Value*>(lhs),
                               reinterpret_cast<llvm::Value*>(rhs), twineName);
  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef LLVMCreateSRemFolder(lifterClass* lifter, LLVMValueRef lhs,
                                  LLVMValueRef rhs,
                                  const char* name) {
  llvm::Twine twineName = (name ? llvm::Twine(name) : llvm::Twine(""));
  llvm::Value* result =
      lifter->createSRemFolder(reinterpret_cast<llvm::Value*>(lhs),
                               reinterpret_cast<llvm::Value*>(rhs), twineName);
  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef LLVMCreateURemFolder(lifterClass* lifter, LLVMValueRef lhs,
                                  LLVMValueRef rhs,
                                  const char* name) {
  llvm::Twine twineName = (name ? llvm::Twine(name) : llvm::Twine(""));
  llvm::Value* result =
      lifter->createURemFolder(reinterpret_cast<llvm::Value*>(lhs),
                               reinterpret_cast<llvm::Value*>(rhs), twineName);
  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef LLVMCreateAShrFolder(lifterClass* lifter, LLVMValueRef lhs,
                                  LLVMValueRef rhs,
                                  const char* name) {
  llvm::Twine twineName = (name ? llvm::Twine(name) : llvm::Twine(""));
  llvm::Value* result =
      lifter->createAShrFolder(reinterpret_cast<llvm::Value*>(lhs),
                               reinterpret_cast<llvm::Value*>(rhs), twineName);
  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef LLVMCreateAndFolder(lifterClass* lifter, LLVMValueRef lhs,
                                 LLVMValueRef rhs, const char* name) {
  llvm::Twine twineName = (name ? llvm::Twine(name) : llvm::Twine(""));
  llvm::Value* result =
      lifter->createAndFolder(reinterpret_cast<llvm::Value*>(lhs),
                              reinterpret_cast<llvm::Value*>(rhs), twineName);
  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef LLVMCreateTruncFolder(lifterClass* lifter, LLVMValueRef v,
                                   LLVMTypeRef destTy,
                                   const char* name) {
  llvm::Twine twineName = (name ? llvm::Twine(name) : llvm::Twine(""));
  llvm::Value* result = lifter->createTruncFolder(
      reinterpret_cast<llvm::Value*>(v), reinterpret_cast<llvm::Type*>(destTy),
      twineName);
  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef LLVMCreateZExtFolder(lifterClass* lifter, LLVMValueRef v,
                                  LLVMTypeRef destTy,
                                  const char* name) {
  llvm::Twine twineName = (name ? llvm::Twine(name) : llvm::Twine(""));
  llvm::Value* result = lifter->createZExtFolder(
      reinterpret_cast<llvm::Value*>(v), reinterpret_cast<llvm::Type*>(destTy),
      twineName);
  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef LLVMCreateZExtOrTruncFolder(lifterClass* lifter, LLVMValueRef v,
                                         LLVMTypeRef destTy,
                                         const char* name) {
  llvm::Twine twineName = (name ? llvm::Twine(name) : llvm::Twine(""));
  llvm::Value* result = lifter->createZExtOrTruncFolder(
      reinterpret_cast<llvm::Value*>(v), reinterpret_cast<llvm::Type*>(destTy),
      twineName);
  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef LLVMCreateSExtFolder(lifterClass* lifter, LLVMValueRef v,
                                  LLVMTypeRef destTy,
                                  const char* name) {
  llvm::Twine twineName = (name ? llvm::Twine(name) : llvm::Twine(""));
  llvm::Value* result = lifter->createSExtFolder(
      reinterpret_cast<llvm::Value*>(v), reinterpret_cast<llvm::Type*>(destTy),
      twineName);
  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef LLVMCreateSExtOrTruncFolder(lifterClass* lifter, LLVMValueRef v,
                                         LLVMTypeRef destTy,
                                         const char* name) {
  llvm::Twine twineName = (name ? llvm::Twine(name) : llvm::Twine(""));
  llvm::Value* result = lifter->createSExtOrTruncFolder(
      reinterpret_cast<llvm::Value*>(v), reinterpret_cast<llvm::Type*>(destTy),
      twineName);
  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef LLVMCreateLShrFolder(lifterClass* lifter, LLVMValueRef lhs,
                                  LLVMValueRef rhs,
                                  const char* name) {
  llvm::Twine twineName = (name ? llvm::Twine(name) : llvm::Twine(""));
  llvm::Value* result =
      lifter->createLShrFolder(reinterpret_cast<llvm::Value*>(lhs),
                               reinterpret_cast<llvm::Value*>(rhs), twineName);
  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef LLVMCreateLShrFolderWithUInt(lifterClass* lifter, LLVMValueRef lhs,
                                          uint64_t rhs,
                                          const char* name) {
  llvm::Twine twineName = (name ? llvm::Twine(name) : llvm::Twine(""));
  llvm::Value* result = lifter->createLShrFolder(
      reinterpret_cast<llvm::Value*>(lhs), rhs, twineName);
  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef LLVMCreateShlFolder(lifterClass* lifter, LLVMValueRef lhs,
                                 LLVMValueRef rhs, const char* name) {
  llvm::Twine twineName = (name ? llvm::Twine(name) : llvm::Twine(""));
  llvm::Value* result =
      lifter->createShlFolder(reinterpret_cast<llvm::Value*>(lhs),
                              reinterpret_cast<llvm::Value*>(rhs), twineName);
  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef LLVMCreateShlFolderWithUInt(lifterClass* lifter, LLVMValueRef lhs,
                                         uint64_t rhs,
                                         const char* name) {
  llvm::Twine twineName = (name ? llvm::Twine(name) : llvm::Twine(""));
  llvm::Value* result = lifter->createShlFolder(
      reinterpret_cast<llvm::Value*>(lhs), rhs, twineName);
  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef createInstruction(lifterClass* lifter, const unsigned opcode,
                               LLVMValueRef operand1, LLVMValueRef operand2,
                               LLVMTypeRef destType,
                               const char* name) {
  llvm::Twine twineName = (name ? llvm::Twine(name) : llvm::Twine(""));
  llvm::Value* result = lifter->createInstruction(
      opcode, reinterpret_cast<llvm::Value*>(operand1),
      reinterpret_cast<llvm::Value*>(operand2),
      reinterpret_cast<llvm::Type*>(destType), twineName);
  return reinterpret_cast<LLVMValueRef>(result);
}

LLVMValueRef LLVMDoPatternMatching(lifterClass* lifter, LLVMOpcode opcode,
                                   LLVMValueRef op0, LLVMValueRef op1) {
  // Convertiamo l'enum da LLVMOpcode a llvm::Instruction::BinaryOps
  llvm::Instruction::BinaryOps binOp =
      static_cast<llvm::Instruction::BinaryOps>(opcode);

  // Converte gli argomenti da LLVMValueRef a llvm::Value*
  llvm::Value* nativeOp0 = reinterpret_cast<llvm::Value*>(op0);
  llvm::Value* nativeOp1 = reinterpret_cast<llvm::Value*>(op1);

  // Chiama la funzione originale
  llvm::Value* result = lifter->doPatternMatching(binOp, nativeOp0, nativeOp1);

  // Converte il risultato in LLVMValueRef e lo restituisce
  return reinterpret_cast<LLVMValueRef>(result);
}
// end folders
//

// analysis
//
void markMemPaged(lifterClass* lifter, const int64_t start, const int64_t end) {
  lifter->markMemPaged(start, end);
}

bool isMemPaged(lifterClass* lifter, const int64_t address) {
  return lifter->isMemPaged(address);
}

isPaged isValuePaged(lifterClass* lifter, LLVMValueRef address,
                     LLVMValueRef ctxI) {
  // Converte da puntatori opachi agli oggetti C++ originali
  llvm::Value* nativeAddress = reinterpret_cast<llvm::Value*>(address);
  llvm::Instruction* nativeCtxI = reinterpret_cast<llvm::Instruction*>(ctxI);

  // Chiama la funzione originale
  return lifter->isValuePaged(nativeAddress, nativeCtxI);
}

void pagedCheck(lifterClass* lifter, LLVMValueRef address, LLVMValueRef ctxI) {
  // Converte da puntatori opachi agli oggetti C++ originali
  llvm::Value* nativeAddress = reinterpret_cast<llvm::Value*>(address);
  llvm::Instruction* nativeCtxI = reinterpret_cast<llvm::Instruction*>(ctxI);

  // Chiama la funzione originale
  lifter->pagedCheck(nativeAddress, nativeCtxI);
}

void loadMemoryOp(lifterClass* lifter, LLVMValueRef inst) {
  lifter->loadMemoryOp(reinterpret_cast<llvm::Value*>(inst));
}

void insertMemoryOp(lifterClass* lifter, StoreInst* inst) {
  // Converti il puntatore opaco in un puntatore al tipo C++ corretto.
  llvm::StoreInst* storeInst = reinterpret_cast<llvm::StoreInst*>(inst);

  // Chiama la funzione originale.
  lifter->insertMemoryOp(storeInst);
}

LLVMValueRef extractBytes(lifterClass* lifter, LLVMValueRef value,
                          const uint8_t startOffset, const uint8_t endOffset) {

  auto res = lifter->extractBytes(reinterpret_cast<llvm::Value*>(value),
                                  startOffset, endOffset);

  return reinterpret_cast<LLVMValueRef>(res);
}
// end analysis
//

LLVMBuilderRef get_builder_from_lifter(lifterClass* lifter) {
  return wrap(&lifter->builder);
}

LLVMValueRef get_Function(lifterClass* lifter) {
  return reinterpret_cast<LLVMValueRef>(lifter->fnc);
}

void set_Function(lifterClass* lifter, LLVMValueRef F) {
  lifter->fnc = reinterpret_cast<llvm::Function*>(F);
}

void LF_final_optpass(LLVMValueRef clonedFuncx) {
  final_optpass(reinterpret_cast<llvm::Function*>(clonedFuncx));
}

// return 0 = OK
// return 1 = failed open
// return 2 = failed read
// return 3 = Not PE
uint16_t LF_SetFileData(const char* filename) {

  ifstream ifs(filename, ios::binary);
  if (!ifs.is_open()) {
    cout << "Failed to open the file." << endl;
    return 1;
  }

  // Calcola la dimensione del file
  ifs.seekg(0, std::ios::end);
  size_t fileSize = static_cast<size_t>(ifs.tellg());
  ifs.seekg(0, std::ios::beg);


  // Alloca un buffer dinamico per mantenere i dati del file per tutta la vita
  // della DLL
  uint8_t* fileData = new uint8_t[fileSize];
  if (!ifs.read(reinterpret_cast<char*>(fileData), fileSize)) {
    std::cout << "Failed to read the file." << std::endl;
    delete[] fileData;
    return 2;
  }
  ifs.close();

  // Verifica che il file sia un PE
  auto dosHeader = reinterpret_cast<win::dos_header_t*>(fileData);
  if (*reinterpret_cast<unsigned short*>(fileData) != 0x5a4d) {
    std::cout << "Only PE files are supported." << std::endl;
    delete[] fileData;
    return 3;
  }
  
  const uint16_t IMAGE_NT_OPTIONAL_HDR32_MAGIC = 0x10b;
  const uint16_t IMAGE_NT_OPTIONAL_HDR64_MAGIC = 0x20b;

  auto ntHeaders = reinterpret_cast<win::nt_headers_t<true>*>(fileData + dosHeader->e_lfanew);
  uint16_t PEmagic = ntHeaders->optional_header.magic;
  arch_mode is64Bit = (arch_mode)(PEmagic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);

  // Inizializza il namespace BinaryOperations; la funzione initBases imposta il puntatore globale data_g
  BinaryOperations::initBases(fileData, is64Bit);

  // NOTA: Non deallocare fileData qui, perché deve rimanere valido per tutta la  vita della DLL.
  return 0;
}

bool BO_isWrittenTo(uint64_t addr) {
  return BinaryOperations::isWrittenTo(addr);
}

void LF_SETRun(lifterClass* lifter, bool lRun) { 
    lifter->run = lRun; 
}

void LF_SETFinished(lifterClass* lifter, bool lFinished) {
  lifter->finished = lFinished;
}

bool LF_GETRun(lifterClass* lifter) { 
    return lifter->run; 
}

bool LF_GETFinished(lifterClass* lifter) { 
    return lifter->finished; 
}

void LF_SetInstruz(lifterClass* lifter, ZydisDecodedInstruction instruction) {
  lifter->instruction = instruction;
}

void LF_SetInstruzOperand(lifterClass* lifter, ZydisDecodedOperand* ops) {
  for (size_t i = 0; i < ZYDIS_MAX_OPERAND_COUNT; i++) {
    lifter->operands[i] = ops[i];
  }
}

TLifterRef* LF_GetLifters(size_t* count) {
  if (count)
    *count = lifters.size();
 
  if (lifters.empty())
    return nullptr;

  // Alloca un array C per restituire i puntatori.
  TLifterRef* arr = new TLifterRef[lifters.size()];
  for (size_t i = 0; i < lifters.size(); i++) {
    arr[i] = reinterpret_cast<TLifterRef>(lifters[i]);
  }
  return arr;
}

void LF_SetLifters(TLifterRef* arr, size_t count) {
  lifters.clear();
  for (size_t i = 0; i < count; i++) {
    lifters.push_back(reinterpret_cast<lifterClass*>(arr[i]));
  }
}

void LF_FreeLifters(TLifterRef* array) { 
    delete[] array;
}

void LF_SetdbgCallBack(DebugCallback cbDbg) {
  debugging::g_dbgCallback = cbDbg;
}

void LF_SetMemoryCallBack(MemoryMappingCallback cbMem) {
  BinaryOperations::gMemoryMappingCallback = cbMem;
}

// semantics definition
void lift_movs_X(lifterClass* lifter) { lifter->lift_movs_X(); }
// void lift_movaps(lifterClass* lifter) { lifter->lift_movaps(); }
void lift_mov(lifterClass* lifter) { lifter->lift_mov(); }
void lift_cmovbz(lifterClass* lifter) { lifter->lift_cmovbz(); }
void lift_cmovnbz(lifterClass* lifter) { lifter->lift_cmovnbz(); }
void lift_cmovz(lifterClass* lifter) { lifter->lift_cmovz(); }
void lift_cmovnz(lifterClass* lifter) { lifter->lift_cmovnz(); }
void lift_cmovl(lifterClass* lifter) { lifter->lift_cmovl(); }
void lift_cmovnl(lifterClass* lifter) { lifter->lift_cmovnl(); }
void lift_cmovb(lifterClass* lifter) { lifter->lift_cmovb(); }
void lift_cmovnb(lifterClass* lifter) { lifter->lift_cmovnb(); }
void lift_cmovns(lifterClass* lifter) { lifter->lift_cmovns(); }
void lift_cmovs(lifterClass* lifter) { lifter->lift_cmovs(); }
void lift_cmovnle(lifterClass* lifter) { lifter->lift_cmovnle(); }
void lift_cmovle(lifterClass* lifter) { lifter->lift_cmovle(); }
void lift_cmovo(lifterClass* lifter) { lifter->lift_cmovo(); }
void lift_cmovno(lifterClass* lifter) { lifter->lift_cmovno(); }
void lift_cmovp(lifterClass* lifter) { lifter->lift_cmovp(); }
void lift_cmovnp(lifterClass* lifter) { lifter->lift_cmovnp(); }
void lift_popcnt(lifterClass* lifter) { lifter->lift_popcnt(); }
//
void lift_call(lifterClass* lifter) { lifter->lift_call(); }
void lift_ret(lifterClass* lifter) { lifter->lift_ret(); }
void lift_jmp(lifterClass* lifter) { lifter->lift_jmp(); }
void lift_jnz(lifterClass* lifter) { lifter->lift_jnz(); }
void lift_jz(lifterClass* lifter) { lifter->lift_jz(); }
void lift_js(lifterClass* lifter) { lifter->lift_js(); }
void lift_jns(lifterClass* lifter) { lifter->lift_jns(); }
void lift_jle(lifterClass* lifter) { lifter->lift_jle(); }
void lift_jl(lifterClass* lifter) { lifter->lift_jl(); }
void lift_jnl(lifterClass* lifter) { lifter->lift_jnl(); }
void lift_jnle(lifterClass* lifter) { lifter->lift_jnle(); }
void lift_jbe(lifterClass* lifter) { lifter->lift_jbe(); }
void lift_jb(lifterClass* lifter) { lifter->lift_jb(); }
void lift_jnb(lifterClass* lifter) { lifter->lift_jnb(); }
void lift_jnbe(lifterClass* lifter) { lifter->lift_jnbe(); }
void lift_jo(lifterClass* lifter) { lifter->lift_jo(); }
void lift_jno(lifterClass* lifter) { lifter->lift_jno(); }
void lift_jp(lifterClass* lifter) { lifter->lift_jp(); }
void lift_jnp(lifterClass* lifter) { lifter->lift_jnp(); }
//
void lift_sbb(lifterClass* lifter) { lifter->lift_sbb(); }
void lift_rcl(lifterClass* lifter) { lifter->lift_rcl(); }
void lift_rcr(lifterClass* lifter) { lifter->lift_rcr(); }
void lift_not(lifterClass* lifter) { lifter->lift_not(); }
void lift_neg(lifterClass* lifter) { lifter->lift_neg(); }
void lift_sar(lifterClass* lifter) { lifter->lift_sar(); }
void lift_shr(lifterClass* lifter) { lifter->lift_shr(); }
void lift_shl(lifterClass* lifter) { lifter->lift_shl(); }
void lift_bswap(lifterClass* lifter) { lifter->lift_bswap(); }
void lift_cmpxchg(lifterClass* lifter) { lifter->lift_cmpxchg(); }
void lift_xchg(lifterClass* lifter) { lifter->lift_xchg(); }
void lift_shld(lifterClass* lifter) { lifter->lift_shld(); }
void lift_shrd(lifterClass* lifter) { lifter->lift_shrd(); }
void lift_lea(lifterClass* lifter) { lifter->lift_lea(); }
void lift_add_sub(lifterClass* lifter) { lifter->lift_add_sub(); }
void lift_imul2(lifterClass* lifter, const bool isSigned) {
  lifter->lift_imul2(isSigned);
}
void lift_imul(lifterClass* lifter) { lifter->lift_imul(); }
void lift_mul(lifterClass* lifter) { lifter->lift_mul(); }
// void lift_div2(lifterClass* lifter) { lifter->lift_div2(); }
void lift_div(lifterClass* lifter) { lifter->lift_div(); }
// void lift_idiv2(lifterClass* lifter) { lifter->lift_idiv2(); }
void lift_idiv(lifterClass* lifter) { lifter->lift_idiv(); }
void lift_xor(lifterClass* lifter) { lifter->lift_xor(); }
void lift_or(lifterClass* lifter) { lifter->lift_or(); }
void lift_and(lifterClass* lifter) { lifter->lift_and(); }
void lift_andn(lifterClass* lifter) { lifter->lift_andn(); }
void lift_rol(lifterClass* lifter) { lifter->lift_rol(); }
void lift_ror(lifterClass* lifter) { lifter->lift_ror(); }
void lift_inc(lifterClass* lifter) { lifter->lift_inc(); }
void lift_dec(lifterClass* lifter) { lifter->lift_dec(); }
void lift_push(lifterClass* lifter) { lifter->lift_push(); }
void lift_pushfq(lifterClass* lifter) { lifter->lift_pushfq(); }
void lift_pop(lifterClass* lifter) { lifter->lift_pop(); }
void lift_popfq(lifterClass* lifter) { lifter->lift_popfq(); }

void lift_leave(lifterClass* lifter) { lifter->lift_leave(); }

void lift_adc(lifterClass* lifter) { lifter->lift_adc(); }
void lift_xadd(lifterClass* lifter) { lifter->lift_xadd(); }
void lift_test(lifterClass* lifter) { lifter->lift_test(); }
void lift_cmp(lifterClass* lifter) { lifter->lift_cmp(); }
void lift_rdtsc(lifterClass* lifter) { lifter->lift_rdtsc(); }
void lift_cpuid(lifterClass* lifter) { lifter->lift_cpuid(); }
void lift_pext(lifterClass* lifter) { lifter->lift_pext(); }
//
void lift_setnz(lifterClass* lifter) { lifter->lift_setnz(); }
void lift_seto(lifterClass* lifter) { lifter->lift_seto(); }
void lift_setno(lifterClass* lifter) { lifter->lift_setno(); }
void lift_setnb(lifterClass* lifter) { lifter->lift_setnb(); }
void lift_setbe(lifterClass* lifter) { lifter->lift_setbe(); }
void lift_setnbe(lifterClass* lifter) { lifter->lift_setnbe(); }
void lift_setns(lifterClass* lifter) { lifter->lift_setns(); }
void lift_setp(lifterClass* lifter) { lifter->lift_setp(); }
void lift_setnp(lifterClass* lifter) { lifter->lift_setnp(); }
void lift_setb(lifterClass* lifter) { lifter->lift_setb(); }
void lift_sets(lifterClass* lifter) { lifter->lift_sets(); }
void lift_stosx(lifterClass* lifter) { lifter->lift_stosx(); }
void lift_setz(lifterClass* lifter) { lifter->lift_setz(); }
void lift_setnle(lifterClass* lifter) { lifter->lift_setnle(); }
void lift_setle(lifterClass* lifter) { lifter->lift_setle(); }
void lift_setnl(lifterClass* lifter) { lifter->lift_setnl(); }
void lift_setl(lifterClass* lifter) { lifter->lift_setl(); }
void lift_bt(lifterClass* lifter) { lifter->lift_bt(); }
void lift_btr(lifterClass* lifter) { lifter->lift_btr(); }
void lift_bts(lifterClass* lifter) { lifter->lift_bts(); }
void lift_lzcnt(lifterClass* lifter) { lifter->lift_lzcnt(); }
void lift_bzhi(lifterClass* lifter) { lifter->lift_bzhi(); }
void lift_bsr(lifterClass* lifter) { lifter->lift_bsr(); }
void lift_bsf(lifterClass* lifter) { lifter->lift_bsf(); }
void lift_blsr(lifterClass* lifter) { lifter->lift_blsr(); }
void lift_tzcnt(lifterClass* lifter) { lifter->lift_tzcnt(); }
void lift_btc(lifterClass* lifter) { lifter->lift_btc(); }
void lift_lahf(lifterClass* lifter) { lifter->lift_lahf(); }
void lift_sahf(lifterClass* lifter) { lifter->lift_sahf(); }
void lift_std(lifterClass* lifter) { lifter->lift_std(); }
void lift_stc(lifterClass* lifter) { lifter->lift_stc(); }
void lift_cmc(lifterClass* lifter) { lifter->lift_cmc(); }
void lift_clc(lifterClass* lifter) { lifter->lift_clc(); }
void lift_cld(lifterClass* lifter) { lifter->lift_cld(); }
void lift_cli(lifterClass* lifter) { lifter->lift_cli(); }
void lift_cwd(lifterClass* lifter) { lifter->lift_cwd(); }
void lift_cdq(lifterClass* lifter) { lifter->lift_cdq(); }
void lift_cqo(lifterClass* lifter) { lifter->lift_cqo(); }
void lift_cbw(lifterClass* lifter) { lifter->lift_cbw(); }
void lift_cwde(lifterClass* lifter) { lifter->lift_cwde(); }
void lift_cdqe(lifterClass* lifter) { lifter->lift_cdqe(); }
void lift_bextr(lifterClass* lifter) { lifter->lift_bextr(); }
// end semantics definition
    

