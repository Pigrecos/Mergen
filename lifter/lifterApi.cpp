#include "lifterApi.h"
#include "lifter.h"
#include <vector>

extern "C" {
LIFTER_API void* CreateLifter() { return new Lifter(); }

LIFTER_API void DestroyLifter(void* lifter) {
        delete static_cast<Lifter*>(lifter);
}

LIFTER_API void InitializeLifter(void* lifter, const char* filename,
                                 uint64_t startAddr) {
        static_cast<Lifter*>(lifter)->initializeLifter(filename, startAddr);
}

LIFTER_API void LiftFunction(void* lifter) {
        static_cast<Lifter*>(lifter)->liftFunction();
}

LIFTER_API void
LiftSingleInstruction(void* lifter, ZydisDisassembledInstruction* instruction) {
        static_cast<Lifter*>(lifter)->liftSingleInstruction(*instruction);
}

LIFTER_API LLVMContextRef GetLifterContext(void* lifter) {
        return static_cast<Lifter*>(lifter)->getContextRef();
}

LIFTER_API LLVMModuleRef GetLifterModule(void* lifter) {
        return static_cast<Lifter*>(lifter)->getModuleRef();
}

LIFTER_API void SetLifterRetTable(void* lifter, const RetItem* table, size_t size) {
        auto* l = static_cast<Lifter*>(lifter);
        std::vector<RetItem> cppTable;
        cppTable.reserve(size);
        for (size_t i = 0; i < size; ++i) {
                cppTable.push_back({table[i].Address, table[i].RetAddress});
        }
        l->setRetTable(cppTable);
}

LIFTER_API size_t GetLifterRetTableSize(void* lifter) {
        auto* l = static_cast<Lifter*>(lifter);
        return l->getRetTable().size();
}

LIFTER_API void GetLifterRetTable(void* lifter, RetItem* outTable, size_t size) {
        auto* l = static_cast<Lifter*>(lifter);
        const auto& cppTable = l->getRetTable();
        size_t copySize = std::min(size, cppTable.size());
        for (size_t i = 0; i < copySize; ++i) {
                outTable[i].Address = cppTable[i].Address;
                outTable[i].RetAddress = cppTable[i].RetAddress;
        }
}

LIFTER_API void SetLifterOpTable(void* lifter, const OpaquePredicateEntry* table, size_t size) {
        auto* l = static_cast<Lifter*>(lifter);
        std::vector<OpaquePredicateEntry> cppTable;
        cppTable.reserve(size);
        for (size_t i = 0; i < size; ++i) {
                cppTable.push_back({table[i].address, table[i].jmpAddress});
        }
        l->setOpTable(cppTable);
}

LIFTER_API size_t GetLifterOpTableSize(void* lifter) {
        auto* l = static_cast<Lifter*>(lifter);
        return l->getOpTable().size();
}

LIFTER_API void GetLifterOpTable(void* lifter, OpaquePredicateEntry* outTable, size_t size) {
        auto* l = static_cast<Lifter*>(lifter);
        const auto& cppTable = l->getOpTable();
        size_t copySize = std::min(size, cppTable.size());
        for (size_t i = 0; i < copySize; ++i) {
                outTable[i].address = cppTable[i].address;
                outTable[i].jmpAddress = cppTable[i].jmpAddress;
        }
}

}