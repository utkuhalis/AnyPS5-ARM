// Instructions the PS5's Zen 2 has and Rosetta does not (SSE4a, CLZERO, MONITORX/MWAITX, and RDSEED,
// RDPID and CLWB, which Intel hosts have), here and in a guest module. Under Rosetta they raise SIGILL
// unless --to-intel replaced them; the program exits 47 when every result is right, otherwise with a
// bit per wrong result.
typedef long long Vector __attribute__((vector_size(16)));

extern "C" unsigned long long amdExtract(unsigned long long value);
extern "C" [[noreturn]] void exit(int);

// Called through a data pointer, so the executable also has a RELA import.
unsigned long long (*volatile extractPointer)(unsigned long long) = amdExtract;
volatile unsigned long long input = 0x1122334455667788ull;
volatile unsigned long long insertion = 0xab;
alignas(64) unsigned char line[64];

extern "C" [[noreturn]] void _start(void*) {
    int failures = 0;
    if (extractPointer(input) != 0x77) failures |= 1;
    const Vector inserted = __builtin_ia32_insertqi(Vector{static_cast<long long>(input), 0}, Vector{static_cast<long long>(insertion), 0}, 8, 16);
    if (static_cast<unsigned long long>(inserted[0]) != 0x1122334455ab7788ull) failures |= 2;
    volatile unsigned char* bytes = line;
    for (int index = 0; index < 64; ++index) bytes[index] = 0x5a;
    __builtin_ia32_clzero(line);
    for (int index = 0; index < 64; ++index) if (bytes[index] != 0) failures |= 4;
    __builtin_ia32_clwb(line);
    unsigned long long seed = 0;
    if (__builtin_ia32_rdseed64_step(&seed) != 1) failures |= 8;
    if (__builtin_ia32_rdpid() != 0) failures |= 16;   // every thread is on processor 0
    __builtin_ia32_monitorx(line, 0, 0);
    __builtin_ia32_mwaitx(2, 0, 1000);   // timer enabled, 1000 cycles
    exit(failures == 0 ? 47 : failures);
}
