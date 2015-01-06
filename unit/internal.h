#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/AsmParser/Parser.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>
#include <memory>

namespace {

std::unique_ptr<llvm::Module> parseAssembly(const char *Assembly) {
  std::unique_ptr<llvm::Module> M(new llvm::Module("Module", llvm::getGlobalContext()));

  llvm::SMDiagnostic Error;
  bool Parsed =
      llvm::ParseAssemblyString(Assembly, M.get(), Error, M->getContext()) == M.get();

  std::string ErrMsg;
  llvm::raw_string_ostream OS(ErrMsg);
  Error.print("", OS);

  // A failure here means that the test itself is buggy.
  if (!Parsed)
    llvm::report_fatal_error(OS.str().c_str());

  return M;
}

}
