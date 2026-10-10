# Native arm64 (no Rosetta): design and plan

Branch `native-arm64`. The goal is to run converted titles as arm64 processes, with no Rosetta. `main` keeps the Rosetta port ([MACOS_PORT.md](MACOS_PORT.md)).

## Where Rosetta sits today

- **Guest code:** the relinked guest x86-64 code calls host prx functions directly. dyld writes host addresses into the guest's own GOT slots, and both sides use the SysV calling convention (`APS5_VABI` is empty off Windows).
- **Host code:** host code calls guest code through plain function pointers:
  - the process entry (`Aps5StartGuest`)
  - module init/fini
  - threads (`scePthreadCreate`), Ult, Fiber
  - `atexit`, `__cxa_atexit`, thread destructors, TSD destructors, once
  - signals, C++ exception callbacks and the DWARF unwinder
  - qsort/bsearch, locale facets, guest malloc replacement
  - AvPlayer, Ngs2, Font, Agc command buffer callbacks
- **Rosetta's role:** it runs all of it, guest code and prx libraries alike, because a process has one architecture.

## Approach: translate guest code ahead of time

The relinker already decodes guest code (TLS scan, `--to-intel`). In the arm64 mode it translates every discovered guest function to arm64 at conversion time. The prx libraries build natively for arm64. A first `cmake -DCMAKE_OSX_ARCHITECTURES=arm64` build fails in only 6 of about 1250 compile steps:

- `setjmp`/`longjmp` asm
- the Fiber stack switch
- the guest TLS allocate asm
- TSC reads
- a few `__builtin_ia32_*` exports
- the 80-bit `long double` asserts

### Guest state

- **Registers:** translated code keeps the x86-64 architectural state in a per-thread `GuestState`: 16 GPRs, rip, flags, the 16 ymm registers, mxcsr, and the fs base.
- **Memory is shared:** guest memory is the host address space, so guest loads and stores are plain host loads and stores at the same addresses. No MMU is emulated, and guest pointers and host pointers are the same values.
- **Stack:** the guest stack is real memory. Guest `rsp` lives in `GuestState`, and translated functions use the native stack only for their own temporaries.

### Translation

- **Pipeline:** x86-64 is decoded with Capstone and lifted to LLVM IR, one LLVM function per guest function, operating on `GuestState`. LLVM optimizes it and emits arm64.
- **Flags:** flags are computed into `GuestState` fields; LLVM drops the dead ones.
- **SIMD:** SSE and AVX map to LLVM vector types, and 256-bit operations are lowered to two 128-bit NEON operations by LLVM.
- **Function discovery:**
  - the ELF entry, `DT_INIT`/`INIT_ARRAY`
  - exported and local symbols
  - direct call targets
  - code addresses in relocations (`R_X86_64_RELATIVE` into executable segments)
  - recursive descent over basic blocks
- **Indirect jumps:** an indirect jump inside a function dispatches over that function's known block addresses.
- **Indirect calls:** they go through a runtime lookup of guest address to native function. An unknown target stops the process with its guest address.

### Calling across the boundary

- **Guest to host:** a call whose target is outside the guest code ranges is a host function. The bridge passes rdi, rsi, rdx, rcx, r8, r9 as x0–x5, guest stack arguments 7 and 8 as x6 and x7, and xmm0–7 as v0–v7. It returns x0/x1 into rax/rdx and v0/v1 into xmm0/xmm1. Both ABIs split integer and vector arguments into separate register files, so this covers every export with up to 8 integer and 8 vector arguments passed in registers. Variadic exports differ (Apple arm64 passes variadic arguments on the stack), and so do `va_list`, structure returns (rdi vs x8) and `long double`. Those exports need per-signature bridges; there are 26 variadic `APS5_VABI` exports.
- **Host to guest:** at each discovered guest function entry, the original x86 bytes are overwritten with one arm64 instruction, `b <native entry>`. A host function pointer to guest code therefore runs natively, with no change to the many callback sites. The native entry builds or reuses the thread's `GuestState`, copies the AAPCS arguments into the SysV registers, pushes a sentinel return address on the guest stack, runs the translated function, and returns rax/xmm0 as x0/v0. This needs function entries aligned to 4 bytes, which clang's 16-byte function alignment gives. Guest code that reads its own instruction bytes would see the overlay.
- **TLS:** `fs:` accesses read `GuestState.fsBase`, which is set per thread from the guest TLS allocator. The `gs:`/pthread-key stubs of the Rosetta mode are not used.

### Hard parts, in order of risk

1. **Non-local control flow:** guest `setjmp`/`longjmp`, C++ exception landing pads (a guest unwinder resuming mid-function), and the Fiber and Ult context switches. Translated functions nest native frames, so a guest transfer to an older frame must also unwind native frames. The plan is to resume through the thread's outermost guest entry, or to switch to a dispatch-loop model if that proves insufficient.
2. **Memory ordering:** x86 is TSO and arm64 is weakly ordered. The hardware TSO mode of Apple silicon has no public interface. Guest loads and stores can be emitted as acquire and release (`LDAPR`/`STLR`); that costs throughput, so it is a translation option.
3. **Coverage:** every instruction the titles use must be lifted exactly. Each one gets differential tests against the same code running under Rosetta.
4. **Tooling at conversion time:** the converter would need LLVM and a linker, about 100 MB. A direct arm64 emitter is the alternative if that size is a problem.

## Milestones

1. The prx libraries build for arm64. The x86-only parts throw until they are ported.
2. The smallest fixture (`import_fixture`: `puts` and `exit` through the GOT) runs as an arm64 process.
3. The C and C++ fixtures run: TLS, threads, modules, exceptions.
4. Breakout runs with video, pad and audio.
5. Dreaming Sarah runs.
