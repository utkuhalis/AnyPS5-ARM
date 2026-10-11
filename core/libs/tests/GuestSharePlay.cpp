#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdlib>
#include <exception>

extern "C" {
int APS5_VABI sceSharePlayInitialize(void*, std::size_t);
int APS5_VABI sceSharePlayTerminate(void);
}

static void Require(bool value) { if (!value) std::abort(); }

template <typename TCall>
static bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

int main() {
    alignas(16) static unsigned char heap[0x1800];
    Require(sceSharePlayInitialize(nullptr, sizeof(heap)) == 0);
    Require(sceSharePlayTerminate() == 0);
    Require(sceSharePlayInitialize(heap, sizeof(heap)) == 0);
    Require(sceSharePlayTerminate() == 0);
    Require(Throws([&] { sceSharePlayInitialize(heap, 0); }));
    Require(Throws([&] { sceSharePlayInitialize(nullptr, 0); }));
}
