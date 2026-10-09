# Apple Silicon (macOS) port: fork status

This branch combines current `main` with the community macOS port from [mugurc/AnyPS5 `macos-port`](https://github.com/mugurc/AnyPS5/tree/macos-port), which upstream is reviewing in pieces (#927, #1597, #1499, #1686). It adds the fixes needed to build and run on top of current `main`.

Guest code stays x86-64 and runs under Rosetta 2. The relinker writes Mach-O (`--macos`), the prx libraries build as x86-64 dylibs, and Vulkan runs through MoltenVK. Apple has announced that macOS 27 is the last release with full Rosetta support, so this route has a limited lifetime.

## Build

```sh
git submodule update --init --recursive
cmake -S . -B build-mac -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 -DBUILD_TESTING=ON
ninja -C build-mac all libs
```

Requirements:

- Xcode command-line tools
- Rosetta 2
- Ninja
- the [Vulkan SDK](https://vulkan.lunarg.com/sdk/home#mac) (universal loader and MoltenVK)
- for the guest fixtures only: `brew install llvm lld`

## Run

```sh
build-mac/core/relinker/relinker --macos --to-intel source/eboot.bin out/eboot
python3 tools/package_macos_app.py --relinked out --game source --libs build-mac/core/libs/libs --vulkan ~/VulkanSDK/<version>/macOS Title.app
```

## Status

Measured on an M3 Pro with macOS 26.6, Vulkan SDK 1.4.363.0 and MoltenVK:

| Check | Result |
|---|---|
| Full build (all targets and 115 prx libraries) | builds |
| `macos_fixtures`: argv, imports, TLS, C++ exceptions, C cleanup, threads, modules, TLS across modules, `--to-intel` | 10/10 run under Rosetta |
| Breakout guest (video out, pad, audio out), relinked and packaged as `.app` | runs at about 60 fps |
| Commercial titles | not tested yet |

Changes on top of the merged port:

- [GuestMemory.cpp](../../core/libs/prx/libSceAgcDriver/Execution/src/GuestMemory.cpp) and [DynamicLoader.cpp](../../core/libs/prx/libkernel/Module/src/DynamicLoader.cpp) compile on macOS. `main` had added Linux-only code to both after the port was based.
- When the device has no `robustness2.nullDescriptor` (MoltenVK), the empty slots of the typed image heaps that `main` introduced bind zeroed [padding images](../../core/libs/prx/libSceAgcDriver/Graphics/src/PaddingImages.cpp). Before this, every draw and dispatch failed on macOS.

## Feasibility probes

[tools/macos/spikes](../../tools/macos/spikes) holds the probes used to choose this route:

- gs TSD reads
- AVX2, FMA and BMI2
- JIT code
- fs base: unavailable under Rosetta
- the guest arena layout
- MoltenVK features

Run them with `run.sh [libMoltenVK.dylib]`.
