
#include "FunctionSignatures.h"
#include "GEPTracker.h"
#include "PathSolver.h"
#include "includes.h"
#include "lifterClass.h"
#include "nt/nt_headers.hpp"
#include "utils.h"
#include <fstream>
#include <iostream>
#include <llvm/IR/IRBuilderFolder.h>
#include <llvm/Support/NativeFormatting.h>

// Definizione di costanti per KUSER_SHARED_DATA e dimensioni TEB / PEB.
// Questi valori sono da adattare al tuo contesto, qui sono solo esempi.
#define KUSER_SHARED_DATA_BASE 0x7FFE0000ULL
#define KUSER_SHARED_DATA_SIZE 0x1000ULL
#define TEB_SIZE 0x2000ULL
#define PEB_SIZE 0x1000ULL

// Struttura per tenere gli indirizzi reali e quelli mappati dall'emulatore.
typedef struct _EmuAddress {
  uint64_t Real_TEB;      // Indirizzo reale del TEB
  uint64_t Real_PEB;      // Indirizzo reale del PEB
  uint64_t Real_TEB_TLS;  // Indirizzo reale del TLS nel TEB
  uint64_t Emu_TLS_Addr;  // Indirizzo emulato del TLS
  uint64_t TLS_Size;      // Dimensione del TLS
  uint64_t Emu_TEB_Addr;  // Indirizzo emulato del TEB
  uint64_t Emu_PEB_Addr;  // Indirizzo emulato del PEB
  uint64_t Emu_KUSD_Addr; // Indirizzo emulato del KUSER_SHARED_DATA
} EmuAddress;

vector<lifterClass*> lifters;
uint64_t original_address = 0;
unsigned int pathNo = 0;
// consider having this function in a class, later we can use multi-threading to
// explore different paths
unsigned int breaking = 0;
arch_mode is64Bit;

// Funzione callback per mappare un indirizzo "originale" in uno "emulato".
extern "C" uint64_t cbMapMem(uint64_t originalVA) {
  uint64_t newVA = originalVA;
  EmuAddress emuAddress;

  // Imposta i valori direttamente (modifica questi valori secondo le tue
  // necessità)
  emuAddress.Real_TEB     = 0x0000000000209000ULL; // Esempio: indirizzo reale TEB
  emuAddress.Real_PEB     = 0x0000000000208000ULL; // Esempio: indirizzo reale PEB
  emuAddress.Real_TEB_TLS = 0x00000000000EED60ULL; // Esempio: indirizzo reale TLS nel TEB
  emuAddress.Emu_TLS_Addr = 0x000000015D372050ULL; // Esempio: indirizzo emulato TLS
  emuAddress.TLS_Size     = 0x00000000004BD000ULL; // Esempio: dimensione TLS
  emuAddress.Emu_TEB_Addr = 0x000000015D831000ULL; // Esempio: indirizzo emulato TEB
  emuAddress.Emu_PEB_Addr = 0x000000015D830000ULL; // Esempio: indirizzo emulato PEB
  emuAddress.Emu_KUSD_Addr= 0x000000015D82F000ULL; // Esempio: indirizzo emulato KUSER_SHARED_DATA

  // Se l'indirizzo originale appartiene all'area KUSER_SHARED_DATA, mappa in Emu_KUSD_Addr
  if (originalVA >= KUSER_SHARED_DATA_BASE &&  originalVA < (KUSER_SHARED_DATA_BASE + KUSER_SHARED_DATA_SIZE)) {
    uint64_t offset = originalVA - KUSER_SHARED_DATA_BASE;
    newVA           = emuAddress.Emu_KUSD_Addr + offset;
  }
  // Se l'indirizzo originale appartiene al TEB reale
  else if (originalVA >= emuAddress.Real_TEB && originalVA < (emuAddress.Real_TEB + TEB_SIZE)) {
    uint64_t offset = originalVA - emuAddress.Real_TEB;
    newVA           = emuAddress.Emu_TEB_Addr + offset;
  }
  // Se l'indirizzo originale appartiene al PEB reale
  else if (originalVA >= emuAddress.Real_PEB && originalVA < (emuAddress.Real_PEB + PEB_SIZE)) {
    uint64_t offset = originalVA - emuAddress.Real_PEB;
    newVA           = emuAddress.Emu_PEB_Addr + offset;
  }
  // Se l'indirizzo originale è uguale a 0x58, mappa in base al TEB emulato
  else if (originalVA == 0x58ULL) {
    newVA = emuAddress.Emu_TEB_Addr + originalVA;
  }
  // Se l'indirizzo originale coincide con il Real_TEB_TLS, restituisci un
  // indirizzo fisso (esempio)
  else if (originalVA == emuAddress.Real_TEB_TLS) {
    newVA = 0x000000015D833000ULL;
  }

  // Per debug, puoi stampare il mapping (se hai un sistema di log, o
  // semplicemente usare printf)
  printf("Mapping 0x%llX -> 0x%llX\n", originalVA, newVA);

  return newVA;
}

void asm_to_zydis_to_lift(ZyanU8* data) {
  ZydisDecoder decoder;
  ZydisDecoderInit(&decoder,
                   is64Bit ? ZYDIS_MACHINE_MODE_LONG_64
                           : ZYDIS_MACHINE_MODE_LEGACY_32,
                   is64Bit ? ZYDIS_STACK_WIDTH_64 : ZYDIS_STACK_WIDTH_32);

  BinaryOperations::initBases(data, is64Bit); // sigh ?
  BinaryOperations::gMemoryMappingCallback = cbMapMem;

  while (lifters.size() > 0) {
    lifterClass* lifter = lifters.back();

    uint64_t offset = BinaryOperations::address_to_mapped_address(
        lifter->blockInfo.runtime_address);
    debugging::doIfDebug([&]() {
      const auto printv =
          "runtime_addr: " + to_string(lifter->blockInfo.runtime_address) +
          " offset:" + to_string(offset) + " byte there: 0x" +
          to_string((int)*(data + offset)) + "\n" +
          "offset: " + to_string(offset) +
          " file_base: " + to_string(original_address) +
          " runtime: " + to_string(lifter->blockInfo.runtime_address) + "\n";
      printvalue2(printv);
    });

    lifter->builder.SetInsertPoint(lifter->blockInfo.block);

    lifter->run = 1;

    while ((lifter->run && !lifter->finished)) {

      if (BinaryOperations::isWrittenTo(lifter->blockInfo.runtime_address)) {
        printvalueforce2(lifter->blockInfo.runtime_address);
        UNREACHABLE("Found Self Modifying Code! we dont support it");
      }

      ZydisDecoderDecodeFull(&decoder, data + offset, 15,
                             &(lifter->instruction), lifter->operands);

      ++(lifter->counter);
      auto counter = debugging::increaseInstCounter() - 1;

      debugging::doIfDebug([&]() {
        ZydisFormatter formatter;

        ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);
        char buffer[256];
        ZyanU64 runtime_address = 0;
        ZydisFormatterFormatInstruction(
            &formatter, &(lifter->instruction), lifter->operands,
            lifter->instruction.operand_count_visible, &buffer[0],
            sizeof(buffer), runtime_address, ZYAN_NULL);
        const auto ct = (format_hex_no_prefix(lifter->counter, 0));
        printvalue2(ct);
        const auto inst = buffer;
        printvalue2(inst);
        const auto runtime = lifter->blockInfo.runtime_address;
        printvalue2(runtime);
      });

      lifter->blockInfo.runtime_address += lifter->instruction.length;
      lifter->liftInstruction();
      if (lifter->finished) {

        lifter->run = 0;
        lifters.pop_back();

        debugging::doIfDebug([&]() {
          std::string Filename = "output_path_" + to_string(++pathNo) + ".ll";
          std::error_code EC;
          raw_fd_ostream OS(Filename, EC);
          lifter->fnc->getParent()->print(OS, nullptr);
        });
        outs() << "next lifter instance\n";

        delete lifter;
        break;
      }

      offset += lifter->instruction.length;
    }
  }
}

void InitFunction_and_LiftInstructions(const ZyanU64 runtime_address, std::vector<uint8_t> fileData) {

  auto fileBase = fileData.data();
  LLVMContext context;
  string mod_name = "my_lifting_module";
  llvm::Module lifting_module = llvm::Module(mod_name.c_str(), context);

  vector<llvm::Type*> argTypes;
  argTypes.push_back(llvm::Type::getInt64Ty(context));
  argTypes.push_back(llvm::Type::getInt64Ty(context));
  argTypes.push_back(llvm::Type::getInt64Ty(context));
  argTypes.push_back(llvm::Type::getInt64Ty(context));
  argTypes.push_back(llvm::Type::getInt64Ty(context));
  argTypes.push_back(llvm::Type::getInt64Ty(context));
  argTypes.push_back(llvm::Type::getInt64Ty(context));
  argTypes.push_back(llvm::Type::getInt64Ty(context));
  argTypes.push_back(llvm::Type::getInt64Ty(context));
  argTypes.push_back(llvm::Type::getInt64Ty(context));
  argTypes.push_back(llvm::Type::getInt64Ty(context));
  argTypes.push_back(llvm::Type::getInt64Ty(context));
  argTypes.push_back(llvm::Type::getInt64Ty(context));
  argTypes.push_back(llvm::Type::getInt64Ty(context));
  argTypes.push_back(llvm::Type::getInt64Ty(context));
  argTypes.push_back(llvm::Type::getInt64Ty(context));
  argTypes.push_back(llvm::PointerType::get(context, 0));
  argTypes.push_back(llvm::PointerType::get(context, 0)); // temp fix TEB

  auto functionType = llvm::FunctionType::get(llvm::Type::getInt64Ty(context), argTypes, 0);

  const string function_name = "main";
  auto function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, function_name.c_str(), lifting_module);

  const string block_name = "entry";
  auto bb = llvm::BasicBlock::Create(context, block_name.c_str(), function);
  llvm::IRBuilder<> builder = llvm::IRBuilder<>(bb);

  // auto RegisterList = InitRegisters(builder, function, runtime_address);

  lifterClass* main = new lifterClass(builder);
  main->InitRegisters(function, runtime_address);
  main->blockInfo = BBInfo(runtime_address, bb);

  main->fnc = function;
  main->initDomTree(*function);
  auto dosHeader = (win::dos_header_t*)fileBase;
  if (*(unsigned short*)fileBase != 0x5a4d) {
    UNREACHABLE("Only PE files are supported");
  }

  auto IMAGE_NT_OPTIONAL_HDR32_MAGIC = 0x10b;
  auto IMAGE_NT_OPTIONAL_HDR64_MAGIC = 0x20b;

  auto ntHeaders = (win::nt_headers_t<true>*)(fileBase + dosHeader->e_lfanew);
  auto PEmagic = ntHeaders->optional_header.magic;

  is64Bit = (arch_mode)(PEmagic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);

  auto processHeaders = [fileBase, runtime_address, main](const void* ntHeadersBase) -> uint64_t {
    uint64_t address, imageSize, stackSize;

    if (is64Bit) {
      auto ntHeaders =  reinterpret_cast<const win::nt_headers_t<true>*>(ntHeadersBase);
      address = ntHeaders->optional_header.image_base;
      imageSize = ntHeaders->optional_header.size_image;
      stackSize = ntHeaders->optional_header.size_stack_reserve;
    } else {
      auto ntHeaders = reinterpret_cast<const win::nt_headers_t<false>*>(ntHeadersBase);
      address = ntHeaders->optional_header.image_base;
      imageSize = ntHeaders->optional_header.size_image;
      stackSize = ntHeaders->optional_header.size_stack_reserve;
    }

    const uint64_t RVA = static_cast<uint64_t>(runtime_address - address);
    const uint64_t fileOffset = BinaryOperations::RvaToFileOffset(ntHeadersBase, RVA);
    const uint8_t* dataAtAddress = reinterpret_cast<const uint8_t*>(fileBase) + fileOffset;

    std::cout << std::hex << "0x" << static_cast<int>(*dataAtAddress)
              << std::endl;

    std::cout << "address: " << address << " imageSize: " << imageSize
              << " filebase: " << reinterpret_cast<uint64_t>(fileBase)
              << " fOffset: " << fileOffset << " RVA: " << RVA
              << " stackSize: " << stackSize << std::endl;

    // PEB
    main->markMemPaged(0x208000, 0x209000 + 0x2000);
    // TEB
    main->markMemPaged(0x209000, 0x209000 + 0x2000);
    // KUSER_SHARED_DATA
    main->markMemPaged(0x7FFE0000, 0x7FFE0000 + 0x1000); 
    main->markMemPaged(STACKP_VALUE - stackSize, STACKP_VALUE + stackSize);
    printvalue2(stackSize);
    main->markMemPaged(address, address + imageSize);
    return address;
  };

  original_address = processHeaders(fileBase + dosHeader->e_lfanew);

  funcsignatures::search_signatures(fileData);
  funcsignatures::createOffsetMap(); // ?
  for (const auto& [key, value] : funcsignatures::siglookup) {
    value.display();
  }
  auto ms = timer::getTimer();
  std::cout << "\n" << std::dec << ms << " milliseconds has past" << std::endl;

  // blockAddresses->push_back(make_tuple(runtime_address, bb,
  // RegisterList));
  lifters.push_back(main);

  asm_to_zydis_to_lift(fileBase);

  ms = timer::getTimer();

  cout << "\nlifting complete, " << dec << ms << " milliseconds has past"
       << endl;
  const string Filename_noopt = "output_no_opts.ll";
  error_code EC_noopt;
  llvm::raw_fd_ostream OS_noopt(Filename_noopt, EC_noopt);

  lifting_module.print(OS_noopt, nullptr);

  cout << "\nwriting complete, " << dec << ms << " milliseconds has past"
       << endl;
  final_optpass(function);
  const string Filename = "output.ll";
  error_code EC;
  llvm::raw_fd_ostream OS(Filename, EC);

  lifting_module.print(OS, nullptr);

  return;
}

int main(int argc, char* argv[]) {
  vector<string> args(argv, argv + argc);
  argparser::parseArguments(args);
  timer::startTimer();
  // use parser
  if (args.size() < 3) {
    cerr << "Usage: " << args[0] << " <filename> <startAddr>" << endl;
    return 1;
  }

  // debugging::enableDebug();

  const char* filename = args[1].c_str();
  uint64_t startAddr = stoull(args[2], nullptr, 0);

  ifstream ifs(filename, ios::binary);
  if (!ifs.is_open()) {
    cout << "Failed to open the file." << endl;
    return 1;
  }

  ifs.seekg(0, std::ios::end);
  std::vector<uint8_t> fileData(ifs.tellg());
  ifs.seekg(0, std::ios::beg);

  if (!ifs.read((char*)fileData.data(), fileData.size())) {
    cout << "Failed to read the file." << endl;
    return 1;
  }
  ifs.close();

  InitFunction_and_LiftInstructions(startAddr, fileData);
  auto milliseconds = timer::stopTimer();
  std::cout << "\n"
            << std::dec << milliseconds << " milliseconds has past"
            << std::endl;
  std::cout << "Lifted and optimized " << debugging::increaseInstCounter() - 1
            << " total insts";
}
