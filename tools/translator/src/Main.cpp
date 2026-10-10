#include "Elf.hpp"
#include "Lifter.hpp"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Triple.h>

#include <cstdio>
#include <exception>
#include <memory>
#include <string>

namespace {

void Optimize(llvm::Module& module, llvm::TargetMachine& machine) {
    llvm::LoopAnalysisManager loops;
    llvm::FunctionAnalysisManager functions;
    llvm::CGSCCAnalysisManager cgscc;
    llvm::ModuleAnalysisManager modules;
    llvm::PassBuilder builder(&machine);
    builder.registerModuleAnalyses(modules);
    builder.registerCGSCCAnalyses(cgscc);
    builder.registerFunctionAnalyses(functions);
    builder.registerLoopAnalyses(loops);
    builder.crossRegisterProxies(loops, functions, cgscc, modules);
    builder.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2).run(module, modules);
}

}

// aps5-translator <guest.elf> <output.o> [--emit-llvm]: lifts the guest's x86-64 code and writes an
// arm64 Mach-O object to link with the translator runtime and the host libraries.
int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <guest.elf> <output.o> [--emit-llvm]\n", argv[0]);
        return 2;
    }
    const bool emitLlvm = argc > 3 && std::string(argv[3]) == "--emit-llvm";
    try {
        const Translator::Elf elf(argv[1]);
        llvm::LLVMContext context;
        Translator::Lifter lifter(elf, context);
        lifter.Discover();
        auto module = lifter.Lift();
        if (llvm::verifyModule(*module, &llvm::errs())) {
            if (emitLlvm) module->print(llvm::errs(), nullptr);
            std::fprintf(stderr, "lifted module does not verify\n");
            return 1;
        }

        LLVMInitializeAArch64TargetInfo();
        LLVMInitializeAArch64Target();
        LLVMInitializeAArch64TargetMC();
        LLVMInitializeAArch64AsmPrinter();
        const llvm::Triple triple("arm64-apple-macosx13.0.0");
        std::string error;
        const auto* target = llvm::TargetRegistry::lookupTarget(triple, error);
        if (target == nullptr) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
        std::unique_ptr<llvm::TargetMachine> machine(target->createTargetMachine(triple, "apple-m1", "", {}, llvm::Reloc::PIC_));
        module->setTargetTriple(triple);
        module->setDataLayout(machine->createDataLayout());
        Optimize(*module, *machine);

        std::error_code code;
        llvm::raw_fd_ostream output(argv[2], code, llvm::sys::fs::OF_None);
        if (code) {
            std::fprintf(stderr, "cannot write %s: %s\n", argv[2], code.message().c_str());
            return 1;
        }
        if (emitLlvm) {
            module->print(output, nullptr);
        } else {
            llvm::legacy::PassManager passes;
            if (machine->addPassesToEmitFile(passes, output, nullptr, llvm::CodeGenFileType::ObjectFile)) {
                std::fprintf(stderr, "the target cannot emit an object file\n");
                return 1;
            }
            passes.run(*module);
        }

        const auto& stats = lifter.Stats();
        std::fprintf(stderr, "%zu functions, %zu instructions lifted\n", stats.functions, stats.instructions);
        for (const auto& [what, count] : stats.unsupported) std::fprintf(stderr, "  unsupported: %s (%zu)\n", what.c_str(), count);
    } catch (const std::exception& exception) {
        std::fprintf(stderr, "%s\n", exception.what());
        return 1;
    }
    return 0;
}
