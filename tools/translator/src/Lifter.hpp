#ifndef TOOLS_TRANSLATOR_SRC_LIFTER_HPP
#define TOOLS_TRANSLATOR_SRC_LIFTER_HPP

#include "Elf.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace llvm {
class LLVMContext;
class Module;
}

namespace Translator {

struct Statistics {
    std::size_t functions = 0;
    std::size_t instructions = 0;
    std::map<std::string, std::size_t> unsupported;
};

// Lifts the guest's x86-64 functions to LLVM IR over GuestState (runtime/GuestState.hpp). Each guest
// function becomes `void g_<offset>(GuestState*)`; aps5_guest_functions maps guest offsets to them.
class Lifter {
public:
    Lifter(const Elf& elf, llvm::LLVMContext& context);
    ~Lifter();

    void Discover();
    std::unique_ptr<llvm::Module> Lift();
    const Statistics& Stats() const { return stats; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    Statistics stats;
};

}

#endif
