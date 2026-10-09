#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_ASMFUNCTION_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_ASMFUNCTION_HPP

// Directives that open and close a global function written in top-level asm, and the assembler
// spelling of a C symbol, for each object format. Mach-O prefixes C symbols with an underscore and
// has no .type/.size.
#if defined(_WIN32)
#define APS5_ASM_SYMBOL(name) name
#define APS5_ASM_FUNCTION(name) ".globl " name "\n.def " name "; .scl 2; .type 32; .endef\n" name ":\n"
#define APS5_ASM_FUNCTION_END(name) ""
#elif defined(__APPLE__)
#define APS5_ASM_SYMBOL(name) "_" name
#define APS5_ASM_FUNCTION(name) ".globl _" name "\n_" name ":\n"
#define APS5_ASM_FUNCTION_END(name) ""
#else
#define APS5_ASM_SYMBOL(name) name
#define APS5_ASM_FUNCTION(name) ".globl " name "\n.type " name ", @function\n" name ":\n"
#define APS5_ASM_FUNCTION_END(name) ".size " name ", .-" name "\n"
#endif

#endif
