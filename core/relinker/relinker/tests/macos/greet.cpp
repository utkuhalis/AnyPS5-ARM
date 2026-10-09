// A bundled guest module: dynamic TLS (__tls_get_addr), initial-exec TLS (TPOFF64 through %fs) and an
// initializer.
extern "C" int puts(const char*);

__attribute__((visibility("hidden"))) thread_local int counter = 5;
__attribute__((visibility("hidden"))) thread_local int initialExec __attribute__((tls_model("initial-exec"))) = 7;
static int initialized = 0;

__attribute__((constructor)) static void setup() { initialized = 10; }

extern "C" int greet(int value) {
    counter += value;
    puts("hello from a guest module");
    return counter + initialized + initialExec;
}
