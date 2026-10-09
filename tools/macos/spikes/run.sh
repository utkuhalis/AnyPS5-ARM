#!/bin/sh
# Rosetta 2 / MoltenVK feasibility probes for the Apple Silicon port.
# Usage: tools/macos/spikes/run.sh [path/to/libMoltenVK.dylib]
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
out=$(mktemp -d)
clang -arch x86_64 -O1 "$here/rosetta.c" -o "$out/rosetta" -Wl,-pagezero_size,0x4000
clang -arch x86_64 "$here/arena_probe.c" -o "$out/arena_probe" -Wl,-pagezero_size,0x4000
clang -arch x86_64 -mfsgsbase "$here/fsbase.c" -o "$out/fsbase"
echo "== rosetta"; "$out/rosetta"
echo "== guest arena"; "$out/arena_probe"
echo "== fs base"; "$out/fsbase" || true
if [ -n "$1" ]; then
    clang -arch x86_64 -I"$root/3rdparty/Vulkan-Headers/include" "$here/vkprobe.c" -o "$out/vkprobe"
    echo "== MoltenVK"; MVK_CONFIG_LOG_LEVEL=0 "$out/vkprobe" "$1"
fi
