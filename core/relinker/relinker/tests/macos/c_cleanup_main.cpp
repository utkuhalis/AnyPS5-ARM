// A guest C++ exception passes through guest C frames with cleanup handlers. Exits 47 when it is
// caught and every cleanup ran once, otherwise with a bit per wrong case.
extern "C" void CallWithCleanup(void (*function)(), int* count);
extern "C" [[noreturn]] void exit(int);

struct Error { int code; };

static int cleanups = 0;

[[gnu::noinline]] static void Throw() { throw Error{42}; }
[[gnu::noinline]] static void ThrowThroughCleanup() { CallWithCleanup(Throw, &cleanups); }

[[gnu::noinline]] static bool Caught(void (*function)(), int expectedCleanups) {
    cleanups = 0;
    try {
        CallWithCleanup(function, &cleanups);
    } catch (const Error& error) {
        return error.code == 42 && cleanups == expectedCleanups;
    }
    return false;
}

extern "C" [[noreturn]] void _start(void*) {
    int failures = 0;
    if (!Caught(Throw, 1)) failures |= 1;
    if (!Caught(ThrowThroughCleanup, 2)) failures |= 2;
    exit(failures == 0 ? 47 : failures);
}
