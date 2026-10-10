"""Translates the macOS fixtures to native arm64 and runs them without Rosetta.

usage: run_native_fixtures.py <build-translator> <arm64 libs directory> [work directory]"""
import pathlib, shutil, subprocess, sys, tempfile

HERE = pathlib.Path(__file__).resolve().parent
FIXTURES = HERE.parents[2] / "core/relinker/relinker/tests/macos"
sys.path.insert(0, str(FIXTURES))

import guesttools
from import_fixture import fixture as import_fixture


def compiled_smoke(directory):
    guesttools.compile(str(HERE / "lifter_smoke.c"), str(directory / "main.o"), extra=["-x", "c"])
    names = guesttools.symbols(str(directory / "main.o"), "-u")
    guesttools.nidify(str(directory / "main.o"), str(directory / "main.nid.o"))
    guesttools.stub_library(names, str(directory / "libc.prx"), "libc.prx")
    guesttools.link_executable([str(directory / "main.nid.o")], [str(directory / "libc.prx")], str(directory / "eboot.elf"))


def translate_and_run(build, libraries, directory, expected_exit, expected_output):
    translated = subprocess.run([str(build / "aps5-translator"), str(directory / "eboot.elf"), str(directory / "guest.o")], capture_output=True, text=True)
    if translated.returncode != 0:
        return f"translation failed: {translated.stderr.strip()}"
    shutil.rmtree(directory / "libs", ignore_errors=True)
    (directory / "libs").mkdir()
    shutil.copy2(libraries / "libc.prx", directory / "libs" / "libc.prx")
    subprocess.run(["clang++", "-arch", "arm64", str(directory / "guest.o"), str(build / "libaps5-translator-runtime.a"), str(directory / "libs/libc.prx"),
                    "-Wl,-rpath,@executable_path/libs", "-o", str(directory / "eboot")], check=True)
    executed = subprocess.run([str(directory / "eboot")], capture_output=True, text=True, timeout=60, cwd=directory)
    if executed.returncode != expected_exit or executed.stdout.strip() != expected_output:
        return f"exit {executed.returncode}, expected {expected_exit}; output {executed.stdout.strip()!r}: {executed.stderr.strip()[-300:]}"
    return None


def main():
    build, libraries = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
    temporary = len(sys.argv) <= 3
    work = pathlib.Path(sys.argv[3]) if not temporary else pathlib.Path(tempfile.mkdtemp(prefix="anyps5-native-"))
    cases = [("import", lambda d: (d / "eboot.elf").write_bytes(import_fixture()), 0, "hello from a PS5 ELF on macOS")]
    if guesttools.available():
        # The expected values come from the same source compiled for the host.
        cases.append(("lifter-smoke", compiled_smoke, 28, "3b0c6e4bf8726a1c"))
    failures = 0
    for name, build_fixture, expected_exit, expected_output in cases:
        directory = work / name
        shutil.rmtree(directory, ignore_errors=True)
        directory.mkdir(parents=True)
        build_fixture(directory)
        error = translate_and_run(build, libraries, directory, expected_exit, expected_output)
        print(f"{'PASS' if error is None else 'FAIL'} {name}" + ("" if error is None else f": {error}"))
        failures += error is not None
    if failures:
        raise SystemExit(f"{failures} native fixture(s) failed; their files are in {work}")
    if temporary:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
