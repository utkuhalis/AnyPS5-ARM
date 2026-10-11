#include <atomic>
#include "prx/libc/include/General.hpp"

// The EA Play partner service needs PlayStation Network; the library only tracks its own lifetime.
namespace {

constexpr int SCE_NP_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80550001);
constexpr int SCE_NP_ERROR_NOT_INITIALIZED = static_cast<int>(0x80550002);

std::atomic<bool> g_initialized{false};

}

extern "C" {

int APS5_VABI sceNpEAAccessInitialize(void) {
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) return SCE_NP_ERROR_ALREADY_INITIALIZED;
    return 0;
}

int APS5_VABI sceNpEAAccessTerminate(void) {
    bool expected = true;
    if (!g_initialized.compare_exchange_strong(expected, false)) return SCE_NP_ERROR_NOT_INITIALIZED;
    return 0;
}

}
