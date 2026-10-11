#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static const KernelEvent& requireEvent(const KernelEvent* ev, const char* caller) {
    if (ev == nullptr) {
        throw std::runtime_error(std::string(caller) + ": null event");
    }
    return *ev;
}

extern "C" {

intptr_t APS5_VABI sceKernelGetEventData(const KernelEvent* ev) {
    return requireEvent(ev, __func__).data;
}

// As with kevent, an event flagged EV_ERROR carries its errno in data; the error is returned as
// the matching SCE kernel error code.
int APS5_VABI sceKernelGetEventError(const KernelEvent* ev) {
    constexpr std::uint16_t EventError = 0x4000;
    constexpr std::uint32_t KernelErrorBase = 0x80020000u;
    const auto& event = requireEvent(ev, __func__);
    if ((event.flags & EventError) == 0 || event.data <= 0 || event.data > 0xffff) return 0;
    return static_cast<int>(KernelErrorBase | static_cast<std::uint32_t>(event.data));
}

intptr_t APS5_VABI sceKernelGetEventFflags(const KernelEvent* ev) {
    return static_cast<intptr_t>(requireEvent(ev, __func__).fflags);
}

int APS5_VABI sceKernelGetEventFilter(const KernelEvent* ev) {
    return requireEvent(ev, __func__).filter;
}

uintptr_t APS5_VABI sceKernelGetEventId(const KernelEvent* ev) {
    return requireEvent(ev, __func__).ident;
}

void* APS5_VABI sceKernelGetEventUserData(const KernelEvent* ev) {
    return requireEvent(ev, __func__).udata;
}

}
