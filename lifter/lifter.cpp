

#include "FunctionSignatures.h"
#include "GEPTracker.h"
#include "OperandUtils.h"
#include "PathSolver.h"
#include "includes.h"
#include "lifterClass.h"
#include "nt/nt_headers.hpp"
#include "utils.h"
#include <fstream>

vector<lifterClass*> lifters;
uint64_t original_address = 0;


std::vector<RetItem> lifterClass::LoadRetItemsFromFile(const char* filename) {
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

  returnTable = items;

  return items;
}

std::vector<OpaquePredicateEntry> lifterClass::loadOpaquePredicates(const char* filename) {
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

  opaquePredicates = items;

  return items;
}

// consider having this function in a class, later we can use multi-threading to
// explore different paths
void asm_to_zydis_to_lift(ZyanU8* data, ZyanU64 runtime_address,  ZyanU64 file_base) {

  while (lifters.size() > 0) {
    lifterClass* lifter = lifters.back();
    runtime_address = get<0>(lifter->blockInfo);
    uint64_t offset = FileHelper::address_to_mapped_address((void*)file_base,
                                                            runtime_address);

    debugging::doIfDebug([&]() {
      cout << "runtime_addr: " << runtime_address << " offset:" << offset
           << " byte there: 0x" << (int)*(uint8_t*)(file_base + offset) << endl;
      cout << "offset: " << offset << " file_base?: " << original_address
           << " runtime: " << runtime_address << endl;
    });

    auto nextBasicBlock = get<1>(lifter->blockInfo);

    lifter->builder.SetInsertPoint(nextBasicBlock);

    // will use this for exploring multiple branches
    lifter->setRegisters(get<2>(lifter->blockInfo));

    BinaryOperations::initBases((void*)file_base, data);

    lifter->run = 1;

    for (; lifter->run && runtime_address > 0;) {
      if (BinaryOperations::isWrittenTo(runtime_address)) {
        printvalueforce2(runtime_address);
        outs() << "SelfModifyingCode!\n";
        outs().flush();
      }

      // why tf compiler tells this is unused?
      ZydisDisassembledInstruction instruction;
      ZydisDisassembleIntel(ZYDIS_MACHINE_MODE_LONG_64, runtime_address, data + offset, 15, &instruction);
      lifter->instruction = &instruction;
      auto counter = debugging::increaseInstCounter() - 1;
      debugging::doIfDebug([&]() {
        cout << hex << counter << ":" << instruction.text << "\n";
        cout << "runtime: " << instruction.runtime_address << endl;
      });

      lifter->liftInstruction();
      if (lifter->finished) {

        lifter->run = 0;
        lifters.pop_back();
      }

      offset += instruction.info.length;
      runtime_address += instruction.info.length;

      if (runtime_address == 0x151D6AB00)
        runtime_address = runtime_address;

      if (runtime_address == 0x15531E7F6) // 0x15531E7F6: xor rax, qword ptr [r8]
        runtime_address = runtime_address;
    }
  }
}

void InitFunction_and_LiftInstructions(ZyanU64 runtime_address, uint64_t file_base) {
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

  // auto RegisterList = InitRegisters(builder, function, runtime_address);

  GEPStoreTracker::initDomTree(*function);
  lifterClass* main = new lifterClass(builder);
  auto RegisterList = main->InitRegisters(function, runtime_address);
  main->blockInfo = make_tuple(runtime_address, bb, RegisterList);

  main->LoadRetItemsFromFile("table.txt");
  main->loadOpaquePredicates("opaque_predicates.txt");

  main->memoryConcretization.addConcretization(5493503224, 0);
  main->memoryConcretization.addConcretization(88, 17937920);
  main->memoryConcretization.addConcretization(17937920, 5832560720);
  // teb alloc
  GEPStoreTracker::markMemPaged(17937920, 17937920 + 0x1838);
  //Kuser_shared_Data
  GEPStoreTracker::markMemPaged(0x000000007ffe000, 0x000000007ffe0000 + 0x1000);

  // blockAddresses->push_back(make_tuple(runtime_address, bb, RegisterList));
  lifters.push_back(main);

  asm_to_zydis_to_lift((uint8_t*)file_base, runtime_address, file_base);

  long long ms = timer::stopTimer();
  timer::startTimer();

  cout << "\nlifting complete, " << dec << ms << " milliseconds has past"
       << endl;

  final_optpass(function);
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
  auto imageSize = ntHeaders->optional_header.size_image;
  auto stackSize = ntHeaders->optional_header.size_stack_reserve;
  GEPStoreTracker::markMemPaged((int64_t)(STACKP_VALUE - stackSize), (int64_t)(STACKP_VALUE + stackSize));
  GEPStoreTracker::markMemPaged(ADDRESS, ADDRESS + imageSize);

  uint64_t RVA = static_cast<uint64_t>(startAddr - ADDRESS);
  uint64_t fileOffset = FileHelper::RvaToFileOffset(ntHeaders, RVA);
  uint8_t* dataAtAddress = fileBase + fileOffset;
  cout << hex << "0x" << (int)*dataAtAddress << endl;
  original_address = ADDRESS;
  cout << "address: " << ADDRESS << " imageSize: " << imageSize
       << " filebase: " << (uint64_t)fileBase << " fOffset: " << fileOffset
       << " RVA: " << RVA << endl;

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
