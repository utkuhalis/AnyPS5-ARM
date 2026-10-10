#include "GuestState.hpp"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>

// Emitted by the translator: the guest image (its lowest segment at offset 0), its size, the table
// of translated functions sorted by guest offset, and the guest entry point.
struct TranslatedFunction {
    std::uint64_t offset;
    void (*code)(GuestState*);
};

extern "C" {
extern std::uint8_t aps5_guest_image[];
extern const std::uint64_t aps5_guest_image_size;
extern const TranslatedFunction aps5_guest_functions[];
extern const std::uint64_t aps5_guest_function_count;
extern const std::uint64_t aps5_guest_entry;

void aps5_host_bridge(GuestState* state, std::uint64_t target);
}

namespace {

constexpr std::size_t GuestStackBytes = 8u << 20;

thread_local GuestState* currentState = nullptr;

const TranslatedFunction* Find(std::uint64_t address) {
    const auto base = reinterpret_cast<std::uint64_t>(aps5_guest_image);
    if (address < base || address - base >= aps5_guest_image_size) return nullptr;
    const auto offset = address - base;
    const auto* begin = aps5_guest_functions;
    const auto* end = aps5_guest_functions + aps5_guest_function_count;
    const auto* found = std::lower_bound(begin, end, offset, [](const TranslatedFunction& function, std::uint64_t value) { return function.offset < value; });
    return found != end && found->offset == offset ? found : nullptr;
}

[[noreturn]] void Fail(const char* what, std::uint64_t address) {
    const auto base = reinterpret_cast<std::uint64_t>(aps5_guest_image);
    std::fprintf(stderr, "[translator] %s 0x%" PRIx64 " (guest offset 0x%" PRIx64 ")\n", what, address, address - base);
    std::abort();
}

GuestState* NewState() {
    auto* state = static_cast<GuestState*>(std::aligned_alloc(alignof(GuestState), sizeof(GuestState)));
    std::memset(state, 0, sizeof(*state));
    state->mxcsr = 0x1f80;
    void* stack = mmap(nullptr, GuestStackBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack == MAP_FAILED) Fail("cannot map a guest stack for", 0);
    state->gpr[Rsp] = reinterpret_cast<std::uint64_t>(stack) + GuestStackBytes - 64;
    return state;
}

}

extern "C" {

// A guest call or jump whose target the translator could not resolve statically. The caller has
// pushed the guest return address; a translated callee pops it with its ret, a host callee does not.
void aps5_call(GuestState* state, std::uint64_t target) {
    if (const auto* function = Find(target)) {
        function->code(state);
        return;
    }
    const auto base = reinterpret_cast<std::uint64_t>(aps5_guest_image);
    if (target >= base && target - base < aps5_guest_image_size) Fail("call into untranslated guest code at", target);
    aps5_host_bridge(state, target);
    state->gpr[Rsp] += 8;
}

[[noreturn]] void aps5_trap(GuestState* state, std::uint64_t address) {
    static_cast<void>(state);
    Fail("guest trap (ud2/int3) at", address);
}

[[noreturn]] void aps5_unsupported(GuestState* state, std::uint64_t address) {
    static_cast<void>(state);
    Fail("untranslated instruction at", address);
}

}

int main(int argc, char** argv) {
    currentState = NewState();
    auto* state = currentState;
    // The FreeBSD start block the guest entry receives: argc, argv[], NULL, envp NULL.
    auto* block = static_cast<std::uint64_t*>(std::calloc(static_cast<std::size_t>(argc) + 4, sizeof(std::uint64_t)));
    block[0] = static_cast<std::uint64_t>(argc);
    for (int i = 0; i < argc; ++i) block[1 + i] = reinterpret_cast<std::uint64_t>(argv[i]);
    state->gpr[Rdi] = reinterpret_cast<std::uint64_t>(block);
    state->gpr[Rsi] = 0;
    const auto entry = reinterpret_cast<std::uint64_t>(aps5_guest_image) + aps5_guest_entry;
    const auto* function = Find(entry);
    if (function == nullptr) Fail("no translated guest entry at", entry);
    state->gpr[Rsp] -= 8;
    *reinterpret_cast<std::uint64_t*>(state->gpr[Rsp]) = 0;
    function->code(state);
    return static_cast<int>(state->gpr[Rax]);
}
