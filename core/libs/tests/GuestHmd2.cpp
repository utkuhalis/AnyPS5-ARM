#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

extern "C" {
std::int32_t APS5_VABI sceHmd2Initialize(const void* param);
std::int32_t APS5_VABI sceHmd2Open();
std::int32_t APS5_VABI sceHmd2Close();
std::int32_t APS5_VABI sceHmd2GetDeviceInformation();
std::int32_t APS5_VABI sceHmd2GetFieldOfViewWithoutHandle();
std::int32_t APS5_VABI sceHmd2ReprojectionInitialize();
std::int32_t APS5_VABI sceHmd2ReprojectionQueryBufferSizeAlign();
std::int32_t APS5_VABI sceHmd2SetVibration();
}

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "HMD2: %s\n", message);
        std::abort();
    }
}

}

int main() {
    const std::uint8_t param[16]{};
    Require(sceHmd2Initialize(param) == static_cast<std::int32_t>(0x81110016), "initialization did not report the unsupported feature");
    Require(sceHmd2Initialize(nullptr) == static_cast<std::int32_t>(0x81110016), "initialization without a param did not report the unsupported feature");
    using Call = std::int32_t (APS5_VABI*)();
    for (Call call : {sceHmd2Open, sceHmd2Close, sceHmd2GetDeviceInformation, sceHmd2GetFieldOfViewWithoutHandle, sceHmd2ReprojectionInitialize,
             sceHmd2ReprojectionQueryBufferSizeAlign, sceHmd2SetVibration}) {
        Require(call() == static_cast<std::int32_t>(0x81110016), "a headset call did not report the unsupported feature");
    }
    std::puts("HMD2 tests passed");
    return 0;
}
