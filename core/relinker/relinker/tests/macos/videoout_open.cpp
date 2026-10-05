// Opens video out and waits without flipping: SDL starts and pumps events on the main thread, but no
// window or Vulkan device is created. A safe probe for AppKit start-up on macOS.
extern "C" {
int sceVideoOutOpen(int userId, int busType, int index, const void* parameter);
int sceVideoOutClose(int handle);
int sceKernelUsleep(unsigned microseconds);
int puts(const char*);
[[noreturn]] void exit(int);
}

// Called through a data pointer, so the executable also has a RELA import.
int (*volatile sleepFor)(unsigned) = sceKernelUsleep;

extern "C" [[noreturn]] void _start(void*) {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    if (handle <= 0) exit(1);
    sleepFor(1500000);
    sceVideoOutClose(handle);
    puts("video out opened and closed");
    exit(0);
}
