#pragma once
#include "includes.h"
#include "llvm-c/Core.h"

class Lifter {
  public:
    Lifter();
    ~Lifter();

    void initializeLifter(const char* filename, uint64_t startAddr);
    void liftFunction();
    void liftSingleInstruction(ZydisDisassembledInstruction& instruction);
    LLVMContextRef getContextRef();
    LLVMModuleRef getModuleRef();
    // Return Table
    void setRetTable(const std::vector<RetItem>& table);
    const std::vector<RetItem>& getRetTable() const;
    // Opaque predicates
    void setOpTable(const std::vector<OpaquePredicateEntry>& table);
    const std::vector<OpaquePredicateEntry>& getOpTable() const;

  private:
    // Membri privati per mantenere lo stato
    std::vector<uint8_t> fileData;
    uint8_t* fileBase;
    uint64_t startAddress;
    llvm::Function* mainFunc;
    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> lifting_module;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    std::shared_ptr<std::vector<BBInfo>> blockAddresses;
    ZydisDecoder decoder;
    ZydisFormatter formatter;
    std::vector<RetItem> retTable;
    std::vector<OpaquePredicateEntry> opaquePred;

    void initFunction();
    // ... altri metodi privati necessari
};