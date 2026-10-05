// A bundled guest module with an AMD-only instruction (SSE4a EXTRQ); --to-intel moves it into a stub
// in the module's own __AMDSTUB segment.
typedef long long Vector __attribute__((vector_size(16)));

extern "C" unsigned long long amdExtract(unsigned long long value) {
    const Vector extracted = __builtin_ia32_extrqi(Vector{static_cast<long long>(value), 0}, 8, 8);
    return static_cast<unsigned long long>(extracted[0]);
}
