"""Helpers that build PS5-style ELF executables and modules from C/C++ with LLVM: every external
or exported symbol (except _start) is renamed to its NID, and stub libraries stand in for the prx."""
import os, shutil, struct, subprocess, sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from nid import nid
LLVM = os.environ.get("ANYPS5_LLVM_BIN", "/opt/homebrew/opt/llvm/bin")
TARGET = "--target=x86_64-unknown-freebsd13"

def available():
    return all(pathlib.Path(LLVM, tool).exists() for tool in ("clang++", "llvm-nm", "llvm-objcopy")) and shutil.which("ld.lld") is not None

def compile(source, obj, pic=False, extra=()):
    subprocess.run([f"{LLVM}/clang++", TARGET, "-fPIC" if pic else "-fPIE", "-O1", "-fexceptions", "-fno-stack-protector",
                    "-ffreestanding", "-fno-builtin", "-nostdinc++", *extra, "-c", source, "-o", obj], check=True)

def symbols(obj, *flags):
    return subprocess.run([f"{LLVM}/llvm-nm", *flags, "--format=just-symbols", obj], check=True, capture_output=True, text=True).stdout.split()

def nidify(obj, out, keep=("_start",)):
    names = [n for n in symbols(obj, "-u") + symbols(obj, "-g", "--defined-only") if n not in keep]
    mapping = {n: nid(n) for n in names}
    pathlib.Path(out + ".map").write_text("".join(f"{a} {b}\n" for a, b in mapping.items()))
    subprocess.run([f"{LLVM}/llvm-objcopy", f"--redefine-syms={out}.map", obj, out], check=True)
    return mapping

def stub_library(names, out, soname):
    lines = ["\t.text"]
    for name in names:
        if name.startswith("_ZTV"):
            lines += ["\t.data", f'\t.globl "{nid(name)}"', f'\t.type "{nid(name)}",@object', f'\t.size "{nid(name)}",24', f'"{nid(name)}":', "\t.quad 0, 0, 0", "\t.text"]
        else:
            lines += [f'\t.globl "{nid(name)}"', f'\t.type "{nid(name)}",@function', f'"{nid(name)}":', "\tret"]
    pathlib.Path(out + ".s").write_text("\n".join(lines) + "\n")
    subprocess.run([f"{LLVM}/clang", TARGET, "-c", out + ".s", "-o", out + ".o"], check=True)
    subprocess.run(["ld.lld", "-shared", "-soname", soname, out + ".o", "-o", out], check=True)

def link_module(objs, libs, out, soname):
    subprocess.run(["ld.lld", "-shared", "-z", "now", "--hash-style=sysv", "--eh-frame-hdr", "-soname", soname, *objs, *libs, "-o", out], check=True)

def link_executable(objs, libs, out):
    subprocess.run(["ld.lld", "-pie", "-z", "now", "--hash-style=sysv", "--eh-frame-hdr", "--no-dynamic-linker", "-e", "_start", *objs, *libs, "-o", out], check=True)

def clear_static_tls_flag(path):
    """The guest module reader accepts DT_FLAGS = DF_BIND_NOW only; ld.lld adds DF_STATIC_TLS for an
    initial-exec variable. Clearing it lets the macOS TPOFF64 handling run on a test module."""
    data = bytearray(pathlib.Path(path).read_bytes())
    phoff, = struct.unpack_from("<Q", data, 32)
    phnum, = struct.unpack_from("<H", data, 56)
    for index in range(phnum):
        kind, _, offset, _, _, size = struct.unpack_from("<IIQQQQ", data, phoff + index * 56)
        if kind != 2: continue
        for entry in range(offset, offset + size, 16):
            tag, value = struct.unpack_from("<qQ", data, entry)
            if tag == 30: struct.pack_into("<qQ", data, entry, 30, value & 8)
    pathlib.Path(path).write_bytes(data)
