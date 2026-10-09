// Two guest threads each see their own copy of an executable TLS variable.
extern "C" int scePthreadCreate(void** thread, const void* attr, void* (*entry)(void*), void* arg, const char* name);
extern "C" int scePthreadJoin(void* thread, void** value);
extern "C" [[noreturn]] void exit(int);

thread_local int value = 100;

// Called through a data pointer, so the executable also has a RELA import.
int (*volatile create)(void**, const void*, void* (*)(void*), void*, const char*) = scePthreadCreate;

static void* worker(void* argument) {
    value += static_cast<int>(reinterpret_cast<long>(argument));
    return reinterpret_cast<void*>(static_cast<long>(value));
}

extern "C" [[noreturn]] void _start(void*) {
    void* first = nullptr;
    void* second = nullptr;
    if (create(&first, nullptr, worker, reinterpret_cast<void*>(1L), "first") != 0) exit(1);
    if (create(&second, nullptr, worker, reinterpret_cast<void*>(2L), "second") != 0) exit(2);
    void* firstValue = nullptr;
    void* secondValue = nullptr;
    scePthreadJoin(first, &firstValue);
    scePthreadJoin(second, &secondValue);
    const int result = static_cast<int>(reinterpret_cast<long>(firstValue)) - 100 + (static_cast<int>(reinterpret_cast<long>(secondValue)) - 100) * 10 + (value == 100 ? 30 : 0);
    exit(result);
}
