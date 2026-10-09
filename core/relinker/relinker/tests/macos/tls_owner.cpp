// A guest module that exports a TLS variable; its own accesses are general dynamic as well.
__thread int shared = 40;

extern "C" int bumpShared(int amount) {
    shared += amount;
    return shared;
}
