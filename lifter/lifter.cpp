

#include "FunctionSignatures.h"
#include "GEPTracker.h"
#include "OperandUtils.h"
#include "Semantics.h"
#include "includes.h"
#include "lifter.h"
#include "lifterApi.h"
#include "nt/nt_headers.hpp"
#include "utils.h"
#include <cstdlib>
#include <fstream>

#include "callback.h"
#include <sstream>
#include <iostream>

// new datatype for BBInfo? Something like a domtree
vector<BBInfo> added_blocks_addresses;
uint64_t original_address = 0;

std::vector<RetItem> returnTable;
std::vector<OpaquePredicateEntry> opaquePredicates;

static OutputCallback g_callback = nullptr;

extern "C" {

LIFTER_API void SetOutputCallback(OutputCallback callback) {
  g_callback = callback;
}
}

// Classe personalizzata per sostituire std::cout
class CallbackBuffer : public std::stringbuf {
public:
  virtual int sync() {
    if (g_callback) {
      g_callback(str().c_str());
    }
    str("");
    return 0;
  }
};

// Sostituisci il buffer di cout con il nostro buffer personalizzato
static CallbackBuffer callbackBuffer;
// static std::streambuf* oldBuffer = std::cout.rdbuf(&callbackBuffer);

std::vector<RetItem> LoadRetItemsFromFile(const char* filename) {
  std::vector<RetItem> items;
  std::ifstream file(filename);
  std::string line;

  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << filename << std::endl;
    return items;
  }

  while (std::getline(file, line)) {
    std::istringstream iss(line);
    std::string addressStr, retAddrsStr;

    if (std::getline(iss, addressStr, '=') && std::getline(iss, retAddrsStr)) {
      RetItem item;
      item.Address = std::stoull(addressStr, nullptr, 16); // Assume hex input
      item.RetAddress =
          std::stoull(retAddrsStr, nullptr, 16); // Assume hex input
      items.push_back(item);
    }
  }

  return items;
}

std::vector<OpaquePredicateEntry> loadOpaquePredicates(const char* filename) {
  std::vector<OpaquePredicateEntry> items;
  std::ifstream file(filename);
  std::string line;

  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << filename << std::endl;
    return items;
  }

  while (std::getline(file, line)) {
    std::istringstream iss(line);
    std::string addressStr, jmpAddrsStr;

    if (std::getline(iss, addressStr, '=') && std::getline(iss, jmpAddrsStr)) {
      OpaquePredicateEntry item;
      item.address = std::stoull(addressStr, nullptr, 16); // Assume hex input
      item.jmpAddress =
          std::stoull(jmpAddrsStr, nullptr, 16); // Assume hex input
      items.push_back(item);
    }
  }

  return items;
}

// consider having this function in a class, later we can use multi-threading to
// explore different paths
void asm_to_zydis_to_lift(IRBuilder<>& builder, ZyanU8* data,
                          ZyanU64 runtime_address,
                          shared_ptr<vector<BBInfo>> blockAddresses,
                          ZyanU64 file_base) {

  bool run = 1;
  while (run) {

    while (blockAddresses->size() > 0) {

      runtime_address = get<0>(blockAddresses->back());
      uint64_t offset = FileHelper::address_to_mapped_address((void*)file_base,
                                                              runtime_address);

      debugging::doIfDebug([&]() {
        cout << "runtime_addr: " << runtime_address << " offset:" << offset
             << " byte there: 0x" << (int)*(uint8_t*)(file_base + offset)
             << endl;
        cout << "offset: " << offset << " file_base?: " << original_address
             << " runtime: " << runtime_address << endl;
      });

      auto nextBasicBlock = get<1>(blockAddresses->back());
      added_blocks_addresses.push_back(blockAddresses->back());

      builder.SetInsertPoint(nextBasicBlock);

      // will use this for exploring multiple branches
      setRegisters(get<2>(blockAddresses->back()));
      //

      // update only when its needed
      blockAddresses->pop_back();

      BinaryOperations::initBases((void*)file_base, data);
      size_t last_value;

      bool run = 1;

      if (!blockAddresses->empty()) {
        last_value = get<0>(blockAddresses->back());
      } else {

        last_value = 0;
      }

      for (; run && runtime_address > 0;) {
        if (BinaryOperations::isWrittenTo(runtime_address)) {
          printvalueforce2(runtime_address);
          outs() << "SelfModifyingCode!\n";
          outs().flush();
        }

        if ((blockAddresses->size() == 0 ||
             (last_value == get<0>(blockAddresses->back())))) {

          // why tf compiler tells this is unused?
          ZydisDisassembledInstruction instruction;
          ZydisDisassembleIntel(ZYDIS_MACHINE_MODE_LONG_64, runtime_address,
                                data + offset, 15, &instruction);

          auto counter = debugging::increaseInstCounter() - 1;
          debugging::doIfDebug([&]() {
            cout << hex << counter << ":" << instruction.text << "\n";
            cout << "runtime: " << instruction.runtime_address << endl;
          });

          liftInstruction(builder, instruction, blockAddresses, run);

          offset += instruction.info.length;
          runtime_address += instruction.info.length;

          if (runtime_address == 0x151D6AB00)
            runtime_address = 0;

        } else {
          break;
        }
      }
    }
    run = 0;
  }
}

void InitFunction_and_LiftInstructions(ZyanU64 runtime_address,
                                       uint64_t file_base) {
  ZydisDecoder decoder;
  ZydisFormatter formatter;

  ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
  ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);

  LLVMContext context;
  string mod_name = "my_lifting_module";
  llvm::Module lifting_module = llvm::Module(mod_name.c_str(), context);

  vector<llvm::Type*> argTypes;
  // Aggiungiamo 16 argomenti Int64 per i registri generali
  for (int i = 0; i < 16; ++i) {
    argTypes.push_back(llvm::Type::getInt64Ty(context));
  }
  // Aggiungiamo 16 argomenti Int128 per i registri XMM
  for (int i = 0; i < 16; ++i) {
    argTypes.push_back(llvm::Type::getInt128Ty(context));
  }
  // Aggiungiamo l'argomento per il puntatore alla memoria
  argTypes.push_back(llvm::PointerType::get(context, 0));
  argTypes.push_back(llvm::PointerType::get(context, 0)); // temp fix TEB

  auto functionType = llvm::FunctionType::get(llvm::Type::getInt64Ty(context), argTypes, 0);

  string function_name = "main";
  auto function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, function_name.c_str(), lifting_module);

  string block_name = "entry";
  auto bb = llvm::BasicBlock::Create(context, block_name.c_str(), function);
  llvm::IRBuilder<> builder = llvm::IRBuilder<>(bb);

  auto RegisterList = InitRegisters(builder, function, runtime_address);

  GEPStoreTracker::initDomTree(*function);

  shared_ptr<vector<BBInfo>> blockAddresses = make_shared<vector<BBInfo>>();

  blockAddresses->push_back(make_tuple(runtime_address, bb, RegisterList));

  asm_to_zydis_to_lift(builder, (uint8_t*)file_base, runtime_address, blockAddresses, file_base);

  string Filename = "output.ll";
  error_code EC;
  llvm::raw_fd_ostream OS(Filename, EC);

  if (EC) {
    llvm::errs() << "Could not open file: " << EC.message();
    return;
  }

  lifting_module.print(OS, nullptr);

  return;
}

Lifter::Lifter() : fileBase(nullptr), startAddress(0) {}

Lifter::~Lifter() {
  // Pulizia se necessaria
}

void Lifter::initializeLifter(const char* filename, uint64_t startAddr) {

  ifstream ifs(filename, ios::binary);
  if (!ifs.is_open()) {
    cout << "Failed to open the file." << endl;
  }

  ifs.seekg(0, ios::end);
  std::streamsize fileSize = ifs.tellg();
  ifs.seekg(0, ios::beg);

  this->fileData.resize(fileSize);

  if (!ifs.read(reinterpret_cast<char*>(this->fileData.data()), fileSize)) {
    cout << "Failed to read the file." << endl;
    ifs.close();
    return;
  }
  ifs.close();

  this->fileBase = this->fileData.data();

  FileHelper::setFileBase(this->fileBase);

  auto dosHeader = (win::dos_header_t*)this->fileBase;
  auto ntHeaders =
      (win::nt_headers_x64_t*)(this->fileBase + dosHeader->e_lfanew);
  auto ADDRESS = ntHeaders->optional_header.image_base;
  uintptr_t RVA = static_cast<uintptr_t>(startAddr - ADDRESS);
  uintptr_t fileOffset = FileHelper::RvaToFileOffset(ntHeaders, RVA);
  uint8_t* dataAtAddress = this->fileBase + fileOffset;
  cout << hex << "0x" << (int)*dataAtAddress << endl;
  original_address = ADDRESS;
  cout << "address: " << ADDRESS << " filebase: " << (uintptr_t)this->fileBase
       << " fOffset: " << fileOffset << " RVA: " << RVA << endl;

  this->startAddress = startAddr;

  string mod_name = "my_lifting_module";
  lifting_module =
      std::make_unique<llvm::Module>(mod_name.c_str(), this->context);

  this->blockAddresses = make_shared<vector<BBInfo>>();

  ZydisDecoderInit(&this->decoder, ZYDIS_MACHINE_MODE_LONG_64,
                   ZYDIS_STACK_WIDTH_64);
  ZydisFormatterInit(&this->formatter, ZYDIS_FORMATTER_STYLE_INTEL);
}

void Lifter::liftFunction() {

  initFunction();

  asm_to_zydis_to_lift(*this->builder, this->fileBase, this->startAddress,
                       this->blockAddresses, (uintptr_t)this->fileBase);

  string Filename = "output.ll";
  error_code EC;
  llvm::raw_fd_ostream OS(Filename, EC);

  if (EC) {
    llvm::errs() << "Could not open file: " << EC.message();
    return;
  }

  lifting_module->print(OS, nullptr);
}

void Lifter::liftSingleInstruction(ZydisDisassembledInstruction& instruction) {

  initFunction();

  bool run = true;
  liftInstruction(*this->builder, instruction, this->blockAddresses, run);
}

void Lifter::initFunction() {

  ZyanU64 runtime_address = this->startAddress;

  vector<llvm::Type*> argTypes;
  // Aggiungiamo 16 argomenti Int64 per i registri generali
  for (int i = 0; i < 16; ++i) {
    argTypes.push_back(llvm::Type::getInt64Ty(context));
  }
  // Aggiungiamo 16 argomenti Int128 per i registri XMM
  for (int i = 0; i < 16; ++i) {
    argTypes.push_back(llvm::Type::getInt128Ty(context));
  }
  // Aggiungiamo l'argomento per il puntatore alla memoria
  argTypes.push_back(llvm::PointerType::get(context, 0));

  auto functionType = llvm::FunctionType::get(
      llvm::Type::getInt64Ty(this->context), argTypes, 0);

  string function_name = "main";
  this->mainFunc =
      llvm::Function::Create(functionType, llvm::Function::ExternalLinkage,
                             function_name.c_str(), *this->lifting_module);

  string block_name = "entry";
  auto bb = llvm::BasicBlock::Create(this->context, block_name.c_str(),
                                     this->mainFunc);
  this->builder = std::make_unique<llvm::IRBuilder<>>(bb);

  auto RegisterList =
      InitRegisters(*this->builder, this->mainFunc, runtime_address);

  GEPStoreTracker::initDomTree(*this->mainFunc);

  this->blockAddresses->push_back(
      make_tuple(runtime_address, bb, RegisterList));
}

LLVMContextRef Lifter::getContextRef() { return llvm::wrap(&context); }

LLVMModuleRef Lifter::getModuleRef() {
  return llvm::wrap(lifting_module.get());
}

void Lifter::setRetTable(const std::vector<RetItem>& table) {
  retTable = table;

  returnTable = table;
}

const std::vector<RetItem>& Lifter::getRetTable() const { return retTable; }

void Lifter::setOpTable(const std::vector<OpaquePredicateEntry>& table) {
  opaquePred = table;

  opaquePredicates = table;
}

const std::vector<OpaquePredicateEntry>& Lifter::getOpTable() const {
  return opaquePred;
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

  returnTable = LoadRetItemsFromFile("table.txt");
  opaquePredicates = loadOpaquePredicates("opaque_predicates.txt");

  // void* lifter = CreateLifter();
  // InitializeLifter(lifter, filename, startAddr);
  // LiftFunction(lifter);

  // LLVMContextRef context = GetLifterContext(lifter);
  // LLVMModuleRef module = GetLifterModule(lifter);

  // DestroyLifter(lifter);

  ifstream ifs(filename, ios::binary);
  if (!ifs.is_open()) {
    cout << "Failed to open the file." << endl;
    return 1;
  }

  ifs.seekg(0, ios::end);
  vector<uint8_t> fileData(ifs.tellg());
  ifs.seekg(0, ios::beg);

  if (!ifs.read((char*)fileData.data(), fileData.size())) {
    cout << "Failed to read the file." << endl;
    return 1;
  }
  ifs.close();

  auto fileBase = fileData.data();

  FileHelper::setFileBase(fileBase);

  auto dosHeader = (win::dos_header_t*)fileBase;
  auto ntHeaders = (win::nt_headers_x64_t*)(fileBase + dosHeader->e_lfanew);
  auto ADDRESS = ntHeaders->optional_header.image_base;
  uint64_t RVA = static_cast<uint64_t>(startAddr - ADDRESS);
  uint64_t fileOffset = FileHelper::RvaToFileOffset(ntHeaders, RVA);
  uint8_t* dataAtAddress = fileBase + fileOffset;
  cout << hex << "0x" << (int)*dataAtAddress << endl;
  original_address = ADDRESS;
  cout << "address: " << ADDRESS << " filebase: " << (uint64_t)fileBase
       << " fOffset: " << fileOffset << " RVA: " << RVA << endl;

  funcsignatures::search_signatures(fileData);
  funcsignatures::createOffsetMap();
  for (const auto& [key, value] : funcsignatures::siglookup) {
    value.display();
  }

  long long ms = timer::getTimer();
  cout << "\n" << dec << ms << " milliseconds has past" << endl;

  InitFunction_and_LiftInstructions(startAddr, (uint64_t)fileBase);
  long long milliseconds = timer::stopTimer();
  cout << "\n" << dec << milliseconds << " milliseconds has past" << endl;
  cout << "Executed " << debugging::increaseInstCounter() - 1 << " total insts";
}
