// Exercises the lifter: arithmetic and flags, signed/unsigned compares, shifts, mul/div, loops,
// recursion, indirect calls to guest code, and a byte-wise checksum. Prints the checksum digits.
typedef unsigned long long u64; typedef long long i64; typedef unsigned u32; typedef int i32;
int puts(const char*); void exit(int);
__attribute__((noinline)) static u64 fib(u32 n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
__attribute__((noinline)) static i64 sdiv(i64 a, i64 b) { return a / b + a % b; }
__attribute__((noinline)) static u64 udiv(u64 a, u64 b) { return a / b * 3 + a % b; }
__attribute__((noinline)) static u32 rot(u32 x, u32 n) { return (x << n) | (x >> (32 - n)); }
__attribute__((noinline)) static i32 cmp(i32 a, i32 b) { return (a < b) * 1 + (a > b) * 2 + ((u32)a < (u32)b) * 4 + (a == b) * 8; }
__attribute__((noinline)) static u64 twice(u64 x) { return x * 2; }
__attribute__((noinline)) static u64 thrice(u64 x) { return x * 3; }
static u64 (*volatile table[2])(u64) = {twice, thrice};
__attribute__((noinline)) static u64 mix(u64 h, u64 v) { h ^= v; h *= 0x100000001b3ull; return h ^ (h >> 29); }
void _start(void* block) {
    (void)block;
    u64 h = 0xcbf29ce484222325ull;
    h = mix(h, fib(20));
    h = mix(h, (u64)sdiv(-1000003, 17)); h = mix(h, (u64)sdiv(1000003, -17));
    h = mix(h, udiv(0xfedcba9876543210ull, 12345));
    for (u32 i = 1; i < 32; i += 5) h = mix(h, rot(0x80000001u + i, i));
    for (i32 a = -2; a <= 2; ++a) for (i32 b = -2; b <= 2; ++b) h = mix(h, (u64)cmp(a, b));
    for (int i = 0; i < 10; ++i) h = mix(h, table[i & 1]((u64)i * 1000));
    i64 s = -12345; h = mix(h, (u64)(s >> 3)); h = mix(h, (u64)((u64)s >> 3)); h = mix(h, (u64)(signed char)(s & 0xff));
    __int128 p = (__int128)(i64)0x7fffffffffffull * -0x123456789ll; h = mix(h, (u64)(p >> 64)); h = mix(h, (u64)p);
    char text[17]; for (int i = 0; i < 16; ++i) text[i] = "0123456789abcdef"[(h >> (60 - 4 * i)) & 15]; text[16] = 0;
    puts(text);
    exit((int)(h & 0x7f));
}
