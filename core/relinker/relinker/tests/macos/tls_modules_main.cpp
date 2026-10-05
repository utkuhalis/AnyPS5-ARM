// Two guest modules share a TLS variable: libowner exports it and libuser reads it, and every thread
// has its own copy. Exits 47 when every value is right, otherwise with a bit per wrong value.
extern "C" int bumpShared(int amount);
extern "C" int readShared();
extern "C" int scePthreadCreate(void** thread, const void* attr, void* (*entry)(void*), void* arg, const char* name);
extern "C" int scePthreadJoin(void* thread, void** value);
extern "C" [[noreturn]] void exit(int);

// Called through data pointers, so the executable also has RELA imports.
int (*volatile bumpPointer)(int) = bumpShared;
int (*volatile readPointer)() = readShared;

static void* worker(void*) {
    int failures = 0;
    if (readPointer() != 40) failures |= 4;                      // a new thread starts from the template
    if (bumpPointer(5) != 45 || readPointer() != 45) failures |= 8;
    return reinterpret_cast<void*>(static_cast<long>(failures));
}

extern "C" [[noreturn]] void _start(void*) {
    int failures = 0;
    if (bumpPointer(2) != 42) failures |= 1;
    if (readPointer() != 42) failures |= 2;                      // libuser sees libowner's copy
    void* thread = nullptr;
    void* result = nullptr;
    if (scePthreadCreate(&thread, nullptr, worker, nullptr, "worker") != 0) exit(100);
    scePthreadJoin(thread, &result);
    failures |= static_cast<int>(reinterpret_cast<long>(result));
    if (readPointer() != 42) failures |= 16;                     // the worker kept to its own copy
    exit(failures == 0 ? 47 : failures);
}
