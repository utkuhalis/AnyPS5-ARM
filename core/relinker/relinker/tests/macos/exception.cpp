// A guest program in the shape of a PS5 executable: _start gets the argument block, everything else
// comes from libc.prx by NID.
extern "C" int puts(const char*);
extern "C" [[noreturn]] void exit(int);

struct Error { int code; };

static int cleanups = 0;
struct Guard { ~Guard() { ++cleanups; } };

__attribute__((noinline)) int thrower(int value) {
    Guard guard;
    if (value > 0) throw Error{value * 2};
    return 0;
}

__attribute__((noinline)) int run(int value) {
    try {
        thrower(value);
    } catch (const Error& error) {
        return error.code + cleanups;
    }
    return 1;
}

extern "C" [[noreturn]] void _start(void*) {
    puts("guest C++ exception test");
    exit(run(21));
}
