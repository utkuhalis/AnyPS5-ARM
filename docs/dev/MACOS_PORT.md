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
| ctest, full suite on MoltenVK | 471 of 483 pass |
| Dreaming Sarah (PPSA02929), relinked with `--macos --to-intel` | boots to the animated main menu; about 19 fps measured while dumping every 60th frame |

The 12 tests that still fail:

- **Apple GPU or Metal limits (7):**
  - 64-bit buffer atomics: `buffer_unaligned_base`
  - shaderFloat64: `interpolation_f16`
  - 32 KiB threadgroup memory against the 40 KiB LDS the test uses: `lds_src2`
  - multisampled storage images: the four `prepared_shaders*` tests
- **A product that should give `-0.0` gives `+0.0` (4):** `f32_denormal_flush`, `f32_output_modifier`, `packed_alu`, `sdwa_float_selectors`. See [TechnicalDebt](TechnicalDebt.md).
- **`pixel_interlock`:** SPIRV-Cross writes MSL that Metal rejects for fragment shader interlock.

Changes on top of the merged port:

- [GuestMemory.cpp](../../core/libs/prx/libSceAgcDriver/Execution/src/GuestMemory.cpp) and [DynamicLoader.cpp](../../core/libs/prx/libkernel/Module/src/DynamicLoader.cpp) compile on macOS. `main` had added Linux-only code to both after the port was based.
- When the device has no `robustness2.nullDescriptor` (MoltenVK), the empty slots of the typed image heaps bind zeroed [padding images](../../core/libs/prx/libSceAgcDriver/Graphics/src/PaddingImages.cpp). Before this, every draw and dispatch failed on macOS.
- Shader stages without host subgroups (MoltenVK: vertex and tessellation evaluation) run each invocation as a one-lane wave instead of failing validation.
- Guest thread destructors register through `_tlv_atexit`. libSystem's `__cxa_thread_atexit` returns void, and reading its result as an int freed a live registration twice.
- `dlsym`'s default scope finds the main program through dyld.
- Guest `mprotect` rounds to 16 KiB pages relative to the guest image's load bias. dyld only aligns a slide to 4 KiB, so rounding the absolute address protected the wrong pages (SIGBUS in Dreaming Sarah). Guest segments get maxprot rwx, and the image's no-access padding segments are registered.
- Without storage MSAA, multisampled image dimensions are emitted single-sample. This matches the single-sample images the driver creates; Dreaming Sarah's first shader clears an MSAA surface with `image_store`.
- Merged upstream #1653: no fragment barycentric on MoltenVK. SPIRV-Cross cannot write `PerVertexKHR` to MSL.
- MoltenVK's Metal argument buffers are enabled through `VK_EXT_layer_settings`. Together with upstream #1580 (merged here), they lift the 31 storage buffers per stage limit.
- Test fixes for macOS:
  - case-insensitive APFS in `guest_path_case`
  - the portability loader in `oversized_descriptor_sets`
  - the fixtures' temporary directory, about 720 MiB per run, is now cleaned up

## Feasibility probes

[tools/macos/spikes](../../tools/macos/spikes) holds the probes used to choose this route:

- gs TSD reads
- AVX2, FMA and BMI2
- JIT code
- fs base: unavailable under Rosetta
- the guest arena layout
- MoltenVK features

Run them with `run.sh [libMoltenVK.dylib]`.
