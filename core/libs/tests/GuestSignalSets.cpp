#include "prx/libc/include/general/VabiMacros.hpp"
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

struct GuestSignalSet {
    std::uint32_t bits[4];
};

extern "C" {
int APS5_VABI sigemptyset_nid_postfix(GuestSignalSet*);
int APS5_VABI sigfillset_nid_postfix(GuestSignalSet*);
int APS5_VABI sigaddset_nid_postfix(GuestSignalSet*, int);
int APS5_VABI sigdelset_nid_postfix(GuestSignalSet*, int);
int APS5_VABI sigismember_nid_postfix(const GuestSignalSet*, int);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI _sigprocmask_nid_postfix(int, const GuestSignalSet*, GuestSignalSet*);
int APS5_VABI TitleSigsetjmp(std::uint64_t* buffer, int saveMask);
[[noreturn]] void APS5_VABI siglongjmp_nid_postfix(std::uint64_t* buffer, int value);
}

#ifdef _WIN32
#define TEST_ASM_FUNCTION(name) ".globl " name "\n.def " name "; .scl 2; .type 32; .endef\n" name ":\n"
#define TEST_ASM_CALL(name) "    call " name "\n"
#elif defined(__APPLE__)
#define TEST_ASM_FUNCTION(name) ".globl _" name "\n_" name ":\n"
#define TEST_ASM_CALL(name) "    call _" name "\n"
#else
#define TEST_ASM_FUNCTION(name) ".globl " name "\n.type " name ", @function\n" name ":\n"
#define TEST_ASM_CALL(name) "    call " name "@PLT\n"
#endif

asm(".text\n"
    TEST_ASM_FUNCTION("TitleSigsetjmp")
    "    movl %esi, 88(%rdi)\n"
    "    testl %esi, %esi\n"
    "    jz 2f\n"
    "    pushq %rdi\n"
    "    movq %rdi, %rcx\n"
    "    movq $1, %rdi\n"
    "    movq $0, %rsi\n"
    "    leaq 72(%rcx), %rdx\n"
    TEST_ASM_CALL("_sigprocmask_nid_postfix")
    "    popq %rdi\n"
    "2:\n"
    "    movq %rdi, %rcx\n"
    "    movq 0(%rsp), %rdx\n"
    "    movq %rdx, 0(%rcx)\n"
    "    movq %rbx, 8(%rcx)\n"
    "    movq %rsp, 16(%rcx)\n"
    "    movq %rbp, 24(%rcx)\n"
    "    movq %r12, 32(%rcx)\n"
    "    movq %r13, 40(%rcx)\n"
    "    movq %r14, 48(%rcx)\n"
    "    movq %r15, 56(%rcx)\n"
    "    fnstcw 64(%rcx)\n"
    "    xorq %rax, %rax\n"
    "    ret\n");

static void Require(bool value) { if (!value) std::abort(); }

static bool Equals(const GuestSignalSet& set, std::uint32_t w0, std::uint32_t w1, std::uint32_t w2, std::uint32_t w3) {
    return set.bits[0] == w0 && set.bits[1] == w1 && set.bits[2] == w2 && set.bits[3] == w3;
}

static std::uint32_t BlockedMask() {
    GuestSignalSet current{};
    Require(_sigprocmask_nid_postfix(1, nullptr, &current) == 0);
    return current.bits[0];
}

static void SetBlockedMask(std::uint32_t mask) {
    const GuestSignalSet set{{mask, 0, 0, 0}};
    Require(_sigprocmask_nid_postfix(3, &set, nullptr) == 0);
}

static int JumpBack(int saveMask, int value, std::uint32_t maskAtSet, std::uint32_t maskAtJump) {
    std::uint64_t buffer[12]{};
    volatile int jumped = 0;
    SetBlockedMask(maskAtSet);
    const int result = TitleSigsetjmp(buffer, saveMask);
    if (jumped == 0) {
        Require(result == 0);
        jumped = 1;
        SetBlockedMask(maskAtJump);
        siglongjmp_nid_postfix(buffer, value);
    }
    return result;
}

static void CheckSiglongjmp() {
    Require(JumpBack(1, 0, 0x5u, 0x30u) == 1);
    Require(BlockedMask() == 0x5u);
    Require(JumpBack(1, 7, 0x9u, 0u) == 7);
    Require(BlockedMask() == 0x9u);
    Require(JumpBack(0, -3, 0x5u, 0x30u) == -3);
    Require(BlockedMask() == 0x30u);
    SetBlockedMask(0);
}

int main() {
    CheckSiglongjmp();
    static constexpr int Untouched = 5;
    GuestSignalSet set{{0x12345678u, 0x9abcdef0u, 0xffffffffu, 1u}};
    *__error_nid_postfix() = Untouched;
    Require(sigemptyset_nid_postfix(&set) == 0);
    Require(Equals(set, 0, 0, 0, 0));
    Require(*__error_nid_postfix() == Untouched);

    Require(sigfillset_nid_postfix(&set) == 0);
    Require(Equals(set, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu));
    for (int signal = 1; signal <= 128; ++signal) Require(sigismember_nid_postfix(&set, signal) == 1);
    Require(*__error_nid_postfix() == Untouched);

    Require(sigemptyset_nid_postfix(&set) == 0);
    Require(sigaddset_nid_postfix(&set, 1) == 0);
    Require(Equals(set, 1u, 0, 0, 0));
    Require(sigaddset_nid_postfix(&set, 32) == 0);
    Require(Equals(set, 0x80000001u, 0, 0, 0));
    Require(sigaddset_nid_postfix(&set, 33) == 0);
    Require(Equals(set, 0x80000001u, 1u, 0, 0));
    Require(sigaddset_nid_postfix(&set, 65) == 0);
    Require(Equals(set, 0x80000001u, 1u, 1u, 0));
    Require(sigaddset_nid_postfix(&set, 96) == 0);
    Require(Equals(set, 0x80000001u, 1u, 0x80000001u, 0));
    Require(sigaddset_nid_postfix(&set, 128) == 0);
    Require(Equals(set, 0x80000001u, 1u, 0x80000001u, 0x80000000u));
    Require(sigaddset_nid_postfix(&set, 33) == 0);
    Require(Equals(set, 0x80000001u, 1u, 0x80000001u, 0x80000000u));
    Require(*__error_nid_postfix() == Untouched);

    Require(sigismember_nid_postfix(&set, 1) == 1);
    Require(sigismember_nid_postfix(&set, 2) == 0);
    Require(sigismember_nid_postfix(&set, 32) == 1);
    Require(sigismember_nid_postfix(&set, 33) == 1);
    Require(sigismember_nid_postfix(&set, 64) == 0);
    Require(sigismember_nid_postfix(&set, 65) == 1);
    Require(sigismember_nid_postfix(&set, 96) == 1);
    Require(sigismember_nid_postfix(&set, 97) == 0);
    Require(sigismember_nid_postfix(&set, 128) == 1);
    Require(*__error_nid_postfix() == Untouched);

    Require(sigdelset_nid_postfix(&set, 1) == 0);
    Require(Equals(set, 0x80000000u, 1u, 0x80000001u, 0x80000000u));
    Require(sigdelset_nid_postfix(&set, 32) == 0);
    Require(Equals(set, 0, 1u, 0x80000001u, 0x80000000u));
    Require(sigdelset_nid_postfix(&set, 33) == 0);
    Require(Equals(set, 0, 0, 0x80000001u, 0x80000000u));
    Require(sigdelset_nid_postfix(&set, 96) == 0);
    Require(Equals(set, 0, 0, 1u, 0x80000000u));
    Require(sigdelset_nid_postfix(&set, 128) == 0);
    Require(Equals(set, 0, 0, 1u, 0));
    Require(sigdelset_nid_postfix(&set, 2) == 0);
    Require(Equals(set, 0, 0, 1u, 0));
    Require(sigismember_nid_postfix(&set, 128) == 0);
    Require(*__error_nid_postfix() == Untouched);

    for (const int invalid : {0, -1, 129, 1000, INT_MIN, INT_MAX}) {
        *__error_nid_postfix() = 0;
        Require(sigaddset_nid_postfix(&set, invalid) == -1 && *__error_nid_postfix() == 22);
        *__error_nid_postfix() = 0;
        Require(sigdelset_nid_postfix(&set, invalid) == -1 && *__error_nid_postfix() == 22);
        *__error_nid_postfix() = 0;
        Require(sigismember_nid_postfix(&set, invalid) == -1 && *__error_nid_postfix() == 22);
        Require(Equals(set, 0, 0, 1u, 0));
    }

    Require(sigfillset_nid_postfix(&set) == 0);
    for (const int invalid : {0, -1, 129, 1000, INT_MIN, INT_MAX}) {
        Require(sigaddset_nid_postfix(&set, invalid) == -1 && *__error_nid_postfix() == 22);
        Require(sigdelset_nid_postfix(&set, invalid) == -1 && *__error_nid_postfix() == 22);
        Require(sigismember_nid_postfix(&set, invalid) == -1 && *__error_nid_postfix() == 22);
        Require(Equals(set, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu));
    }

    const GuestSignalSet empty{{0, 0, 0, 0}};
    Require(sigismember_nid_postfix(&empty, 1) == 0);
    Require(sigismember_nid_postfix(&empty, 128) == 0);
    return 0;
}
