#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

extern "C" {
std::int32_t APS5_VABI sceTextToSpeech2Initialize(const void* param);
std::int32_t APS5_VABI sceTextToSpeech2GetSystemStatus();
std::int32_t APS5_VABI sceTextToSpeech2Open();
std::int32_t APS5_VABI sceTextToSpeech2Speak();
std::int32_t APS5_VABI sceTextToSpeech2Cancel();
std::int32_t APS5_VABI sceTextToSpeech2GetSpeechStatus();
std::int32_t APS5_VABI sceTextToSpeech2Close();
std::int32_t APS5_VABI sceTextToSpeech2Terminate();
}

int main() {
    constexpr auto unsupported = static_cast<std::int32_t>(0x8002002D);
    const std::uint32_t param[12]{0x2000000, 0x26c};
    if (sceTextToSpeech2Initialize(param) != unsupported) {
        std::puts("TextToSpeech2: initialization did not report the unsupported operation");
        std::abort();
    }
    using Call = std::int32_t (APS5_VABI*)();
    for (const Call call : {sceTextToSpeech2GetSystemStatus, sceTextToSpeech2Open, sceTextToSpeech2Speak, sceTextToSpeech2Cancel,
                            sceTextToSpeech2GetSpeechStatus, sceTextToSpeech2Close, sceTextToSpeech2Terminate}) {
        if (call() != unsupported) {
            std::puts("TextToSpeech2: a call after failed initialization did not report the unsupported operation");
            std::abort();
        }
    }
    std::puts("TextToSpeech2 tests passed");
    return 0;
}
