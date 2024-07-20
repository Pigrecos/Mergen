#pragma once
#include "includes.h"
#include "llvm-c/Core.h"

#ifdef _WIN32
#define LIFTER_API __declspec(dllexport)
#else
#define LIFTER_API
#endif

extern "C" {
    LIFTER_API void* CreateLifter();
    LIFTER_API void DestroyLifter(void* lifter);
    LIFTER_API void InitializeLifter(void* lifter, const char* filename, uint64_t startAddr);
    LIFTER_API void LiftFunction(void* lifter);
    LIFTER_API void LiftSingleInstruction(void* lifter, ZydisDisassembledInstruction* instruction);
    LIFTER_API LLVMContextRef GetLifterContext(void* lifter);
    LIFTER_API LLVMModuleRef GetLifterModule(void* lifter);
    // Return table
    LIFTER_API void SetLifterRetTable(void* lifter, const RetItem* table,   size_t size);
    LIFTER_API size_t GetLifterRetTableSize(void* lifter);
    LIFTER_API void GetLifterRetTable(void* lifter, RetItem* outTable, size_t size);
    // Opaque predicates
    LIFTER_API void SetLifterOpTable(void* lifter, const OpaquePredicateEntry* table, size_t size);
    LIFTER_API size_t GetLifterOpTableSize(void* lifter);
    LIFTER_API void GetLifterOpTable(void* lifter, OpaquePredicateEntry* outTable, size_t size);
    }