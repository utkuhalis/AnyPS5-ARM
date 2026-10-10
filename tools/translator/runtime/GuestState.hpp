#ifndef TOOLS_TRANSLATOR_RUNTIME_GUESTSTATE_HPP
#define TOOLS_TRANSLATOR_RUNTIME_GUESTSTATE_HPP

#include <cstddef>
#include <cstdint>

// The x86-64 architectural state translated code works on. The translator emits field offsets
// directly (tools/translator/src/Lifter.cpp, StateLayout), so the layout is fixed.
struct alignas(16) GuestState {
    std::uint64_t gpr[16];
    std::uint64_t rip;
    std::uint8_t cf, pf, af, zf, sf, of, df, reserved;
    std::uint64_t fsBase;
    std::uint64_t gsBase;
    std::uint8_t ymm[16][32];
    std::uint32_t mxcsr;
    std::uint32_t reserved2[3];
};

namespace GuestStateLayout {
constexpr std::size_t Gpr = 0;
constexpr std::size_t Rip = 128;
constexpr std::size_t Flags = 136;
constexpr std::size_t FsBase = 144;
constexpr std::size_t GsBase = 152;
constexpr std::size_t Ymm = 160;
constexpr std::size_t Mxcsr = 672;
constexpr std::size_t Size = 688;
}

static_assert(offsetof(GuestState, rip) == GuestStateLayout::Rip);
static_assert(offsetof(GuestState, cf) == GuestStateLayout::Flags);
static_assert(offsetof(GuestState, fsBase) == GuestStateLayout::FsBase);
static_assert(offsetof(GuestState, gsBase) == GuestStateLayout::GsBase);
static_assert(offsetof(GuestState, ymm) == GuestStateLayout::Ymm);
static_assert(offsetof(GuestState, mxcsr) == GuestStateLayout::Mxcsr);
static_assert(sizeof(GuestState) == GuestStateLayout::Size);

enum GuestRegister { Rax, Rcx, Rdx, Rbx, Rsp, Rbp, Rsi, Rdi, R8, R9, R10, R11, R12, R13, R14, R15 };

#endif
