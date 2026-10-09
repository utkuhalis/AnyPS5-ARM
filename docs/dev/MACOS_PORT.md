# Apple Silicon (macOS) port

Status: feasibility done, native port in progress. Nothing runs on macOS yet.

## Approach

Guest code stays x86-64 and runs under Rosetta 2, the same way as on an x86-64 host: no emulator is added. The port has three parts:

1. The system libraries in [core/libs/prx](../../core/libs/prx) build as x86-64 macOS dylibs (`-DCMAKE_OSX_ARCHITECTURES=x86_64`).
2. A macOS ELF loader maps the relinker's Linux ELF output and binds its NID imports to those dylibs. The libraries already assume ELF guest images (`dl_iterate_phdr`, `link_map`, `.eh_frame_hdr`), so loading ELF keeps them unchanged. Writing Mach-O instead would mean reworking all of that.
3. Vulkan runs through MoltenVK.

A native ARM64 build was rejected: it would need a full x86-64 to ARM64 binary translator.

## Feasibility results

The probes are in [tools/macos/spikes](../../tools/macos/spikes) (`run.sh [libMoltenVK.dylib]`). They were measured on an M3 Pro with macOS 26.6 and MoltenVK 1.4.2.

| Check | Result |
|---|---|
| `gs:` TSD and pthread key reads | work |
| AVX2, FMA, BMI2 | work |
| JIT and self-modifying code, 4 KiB `mprotect` | work (link with `-Wl,-pagezero_size,0x4000`) |
| `fs` base (`wrfsbase`, default `fs:` reads) | **unavailable**: SIGILL and SIGSEGV |
| Guest arena `0x2_0000_0000`–`0xFC_0000_0000` | free except `0x2_0000_0000`–`0x2_1000_0000` and `0xF_C000_0000`–`0x70_0000_0000` (Rosetta) |

Consequences:

- Linux output keeps `fs:` TLS accesses, and they cannot run here. The relinker needs a mode that rewrites them, as [WindowsTlsBuilder](../../core/relinker/elfpatcher/src/windows/WindowsTlsBuilder.cpp) does for PE. The resolver would read a pthread key slot through `gs:` instead of the TEB.
- [GuestArena](../../core/libs/prx/libc/src/GuestArena.cpp) needs a second reserved hole for the Rosetta range.

MoltenVK provides every hard Vulkan requirement except `robustness2.nullDescriptor`. That feature only fills unused slots of typed image heaps ([ShaderResources.cpp](../../core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp)), so binding a 1x1 dummy image there works around it. Geometry shaders, mesh shaders, `shaderFloat64`, `depthBounds` and conservative rasterization are missing; titles that need them will not run.

## Build status (x86_64 macOS, full build)

Configuration succeeds, including the FFmpeg macOS x64 prebuilt. 14 objects fail:

- ELF-only assembler directives (`.type`, `.size`, `.hidden`) and missing `_` symbol prefixes: `JumpBuffer.cpp`, `RegisterContext.cpp`, `libSceFiber/Export.cpp`, `ExportMacros.hpp`
- `link.h`: `GuestAllocations.cpp`, `Rtld.cpp`, `ModuleInfo.cpp`, `DynamicLoader.cpp`
- Linux APIs: `dirent64`/`getdents64`, `st_*tim`, `RUSAGE_THREAD`, `sched_getcpu`, `pthread_getattr_np`
- `DirectMemory.cpp` treats non-Linux as Windows
- `std::chrono::clock_cast` (libc++)
- `-Wl,--no-as-needed`
- `__builtin_sysv_va_*` in tests

[nid_patcher](../../core/libs/nid) handles only ELF and PE, so Mach-O export renaming is still to be done.
