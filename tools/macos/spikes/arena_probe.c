#include <stdio.h>
#include <stdint.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
int main(void){
  const uint64_t G = 1ull<<28; int prev=-1;
  for (uint64_t a = 0x200000000ull; a < 0xFC00000000ull; a += G) {
    mach_vm_address_t addr = a;
    kern_return_t kr = mach_vm_allocate(mach_task_self(), &addr, G, VM_FLAGS_FIXED);
    int ok = kr == KERN_SUCCESS;
    if (ok != prev) printf("%s from %#llx%s%s\n", ok ? "OK  " : "BUSY", a, ok ? "" : " : ", ok ? "" : mach_error_string(kr));
    prev = ok;
  }
  printf("end %#llx\n", 0xFC00000000ull);
}
