#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <immintrin.h>
#include <errno.h>

__attribute__((target("avx2,fma,bmi2,f16c")))
static int avx2_test(void) {
    __m256i a = _mm256_set1_epi32(3), b = _mm256_set1_epi32(4);
    __m256i c = _mm256_mullo_epi32(a, b);
    int out[8]; _mm256_storeu_si256((__m256i*)out, c);
    __m256 x = _mm256_set1_ps(2.f), y = _mm256_fmadd_ps(x, x, x);
    float f[8]; _mm256_storeu_ps(f, y);
    unsigned p = _pdep_u32(0xff, 0xf0f0);
    return out[7] == 12 && f[0] == 6.f && p == 0xf0f0;
}

int main(void) {
    int translated = 0; size_t sz = sizeof translated;
    sysctlbyname("sysctl.proc_translated", &translated, &sz, NULL, 0);
    printf("translated (Rosetta): %d\n", translated);
    printf("getpagesize: %d\n", getpagesize());

    // gs-relative TSD read: slot 0 holds pthread_self on macOS x86_64
    uintptr_t gs0; __asm__ volatile("movq %%gs:0, %0" : "=r"(gs0));
    printf("gs:[0] = %#lx pthread_self = %p -> %s\n", gs0, (void*)pthread_self(), gs0 == (uintptr_t)pthread_self() ? "OK" : "MISMATCH");

    // custom pthread key accessed via gs:[key*8]
    pthread_key_t key; pthread_key_create(&key, NULL);
    pthread_setspecific(key, (void*)0x1234567);
    uintptr_t v; __asm__ volatile("movq %%gs:(,%1,8), %0" : "=r"(v) : "r"((uintptr_t)key));
    printf("pthread key %lu via gs = %#lx -> %s\n", (unsigned long)key, v, v == 0x1234567 ? "OK" : "FAIL");

    printf("AVX2/FMA/BMI2: %s\n", avx2_test() ? "OK" : "FAIL");

    // fixed mapping at typical guest addresses
    uintptr_t addrs[] = {0x400000, 0x10000000, 0x200000000, 0x800000000, 0x1000000000};
    for (int i = 0; i < 5; i++) {
        void* p = mmap((void*)addrs[i], 0x10000, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON|MAP_FIXED, -1, 0);
        printf("MAP_FIXED %#lx: %s (%s)\n", addrs[i], p == (void*)addrs[i] ? "OK" : "FAIL", p == MAP_FAILED ? strerror(errno) : "");
    }
    // 4K granularity
    void* q = mmap((void*)0x20001000, 0x1000, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON|MAP_FIXED, -1, 0);
    printf("4K-aligned MAP_FIXED 0x20001000: %s\n", q == (void*)0x20001000 ? "OK" : "FAIL");
    int pr = mprotect((void*)0x20001000, 0x1000, PROT_READ);
    printf("4K mprotect: %s\n", pr == 0 ? "OK" : "FAIL");

    // RWX code emit & run
    unsigned char code[] = {0xb8, 0x2a, 0, 0, 0, 0xc3}; // mov eax,42; ret
    void* c = mmap(NULL, 0x4000, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
    memcpy(c, code, sizeof code);
    mprotect(c, 0x4000, PROT_READ|PROT_EXEC);
    printf("emitted code returns: %d\n", ((int(*)(void))c)());
    // self-modifying code
    mprotect(c, 0x4000, PROT_READ|PROT_WRITE|PROT_EXEC);
    ((unsigned char*)c)[1] = 7;
    printf("after SMC returns: %d\n", ((int(*)(void))c)());
    return 0;
}
