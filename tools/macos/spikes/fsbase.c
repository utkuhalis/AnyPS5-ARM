#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <sys/syscall.h>
static sigjmp_buf jb;
static void h(int s){ siglongjmp(jb, s); }
static uint64_t tlsblock[64] = {0};
int main(void){
  signal(SIGILL, h); signal(SIGSEGV, h); signal(SIGBUS, h);
  tlsblock[0] = (uint64_t)tlsblock; tlsblock[1] = 0xdeadbeefcafe;
  int s;
  if (!(s = sigsetjmp(jb, 1))) {
    uint64_t base = (uint64_t)tlsblock, v;
    __asm__ volatile("wrfsbase %0" :: "r"(base));
    __asm__ volatile("movq %%fs:8, %0" : "=r"(v));
    printf("wrfsbase: fs:8 = %#llx -> %s\n", v, v == 0xdeadbeefcafe ? "WORKS" : "wrong");
  } else printf("wrfsbase: signal %d\n", s);
  if (!(s = sigsetjmp(jb, 1))) {
    uint64_t v; __asm__ volatile("movq %%fs:0, %0" : "=r"(v));
    printf("default fs:0 read = %#llx\n", v);
  } else printf("default fs:0 read: signal %d\n", s);
  // machdep syscall 3 = thread_fast_set_cthread_self (sets gs on x86_64 mac)
  // try i386_set_ldt-free path: syscall 0x3000003
  return 0;
}
