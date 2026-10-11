#include "prx/libc/include/general/VabiMacros.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

struct UsbdTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

}

extern "C" {
std::int32_t APS5_VABI sceUsbdInit();
void APS5_VABI sceUsbdExit();
std::int64_t APS5_VABI sceUsbdGetDeviceList(void*** list);
void APS5_VABI sceUsbdFreeDeviceList(void** list, std::int32_t unrefDevices);
std::int32_t APS5_VABI sceUsbdHandleEventsTimeout(const UsbdTimeval* timeout);
std::int32_t APS5_VABI sceUsbdOpen(void* device, void** deviceHandle);
void APS5_VABI sceUsbdClose(void* deviceHandle);
void* APS5_VABI sceUsbdAllocTransfer(std::int32_t isoPackets);
void APS5_VABI sceUsbdFreeTransfer(void* transfer);
void APS5_VABI sceUsbdFillInterruptTransfer(void* transfer, void* deviceHandle, std::uint8_t endpoint, std::uint8_t* buffer, std::int32_t length,
    void* callback, void* userData, std::uint32_t timeout);
std::int32_t APS5_VABI sceUsbdSubmitTransfer(void* transfer);
std::int32_t APS5_VABI sceUsbdCancelTransfer(void* transfer);
std::int32_t APS5_VABI sceUsbdEventHandlingOk();
void APS5_VABI sceUsbdUnrefDevice(void* device);
std::int32_t APS5_VABI sceUsbdGetDeviceDescriptor(void* device, void* descriptor);
std::int32_t APS5_VABI sceUsbdGetConfigDescriptor(void* device, std::uint8_t configIndex, void** config);
void APS5_VABI sceUsbdFreeConfigDescriptor(void* config);
std::int32_t APS5_VABI sceUsbdCheckConnected(void* deviceHandle);
std::int32_t APS5_VABI sceUsbdClaimInterface(void* deviceHandle, std::int32_t interfaceNumber);
std::int32_t APS5_VABI sceUsbdControlTransfer(void* deviceHandle, std::uint8_t requestType, std::uint8_t request, std::uint16_t value,
    std::uint16_t index, std::uint8_t* data, std::uint16_t length, std::uint32_t timeout);
std::int32_t APS5_VABI sceUsbdGetStringDescriptor(void* deviceHandle, std::uint8_t descriptorIndex, std::uint16_t languageId, std::uint8_t* data,
    std::int32_t length);
}

namespace {

constexpr std::int32_t invalidArgument = static_cast<std::int32_t>(0x80240002);
constexpr std::int32_t noDevice = static_cast<std::int32_t>(0x80240004);
constexpr std::int32_t notFound = static_cast<std::int32_t>(0x80240005);

template <typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "USBD: %s\n", message);
        std::abort();
    }
}

}

int main() {
    Require(sceUsbdInit() == 0, "initialization failed");
    void** list = nullptr;
    Require(sceUsbdGetDeviceList(&list) == 0, "a device was listed");
    Require(list != nullptr && list[0] == nullptr, "device list is not an empty null-terminated array");
    sceUsbdFreeDeviceList(list, 1);
    Require(sceUsbdGetDeviceList(nullptr) == invalidArgument, "null list accepted");
    Require(sceUsbdHandleEventsTimeout(nullptr) == invalidArgument, "null timeout accepted");
    const UsbdTimeval invalid{0, 1000000};
    Require(sceUsbdHandleEventsTimeout(&invalid) == invalidArgument, "out-of-range microseconds accepted");
    const UsbdTimeval timeout{0, 50000};
    const auto start = std::chrono::steady_clock::now();
    Require(sceUsbdHandleEventsTimeout(&timeout) == 0, "event handling failed");
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(45), "event handling returned before the timeout");
    int fakeDevice = 0;
    void* handle = &fakeDevice;
    Require(sceUsbdOpen(nullptr, &handle) == invalidArgument, "null device opened");
    Require(sceUsbdOpen(&fakeDevice, &handle) == noDevice && handle == nullptr, "a foreign device opened");
    sceUsbdClose(nullptr);
    Require(Throws([&] { sceUsbdClose(&fakeDevice); }), "a foreign handle closed");
    sceUsbdUnrefDevice(nullptr);
    Require(Throws([&] { sceUsbdUnrefDevice(&fakeDevice); }), "a foreign device unreferenced");
    std::uint8_t descriptor[18]{};
    Require(sceUsbdGetDeviceDescriptor(&fakeDevice, descriptor) == noDevice, "a descriptor was read");
    Require(sceUsbdGetDeviceDescriptor(&fakeDevice, nullptr) == invalidArgument, "null descriptor accepted");
    void* config = &fakeDevice;
    Require(sceUsbdGetConfigDescriptor(&fakeDevice, 0, &config) == noDevice && config == nullptr, "a configuration was read");
    sceUsbdFreeConfigDescriptor(nullptr);
    Require(Throws([&] { sceUsbdFreeConfigDescriptor(&fakeDevice); }), "a foreign configuration freed");
    Require(sceUsbdCheckConnected(nullptr) == invalidArgument, "null handle checked");
    Require(sceUsbdCheckConnected(&fakeDevice) == noDevice, "a device is connected");
    Require(sceUsbdClaimInterface(&fakeDevice, -1) == invalidArgument, "negative interface claimed");
    Require(sceUsbdClaimInterface(&fakeDevice, 0) == noDevice, "an interface was claimed");
    Require(sceUsbdControlTransfer(&fakeDevice, 0x80, 6, 0x100, 0, nullptr, 18, 0) == invalidArgument, "null control data accepted");
    Require(sceUsbdControlTransfer(&fakeDevice, 0x80, 6, 0x100, 0, descriptor, 18, 0) == noDevice, "a control transfer ran");
    Require(sceUsbdGetStringDescriptor(&fakeDevice, 1, 0x409, descriptor, 0) == invalidArgument, "empty string buffer accepted");
    Require(sceUsbdEventHandlingOk() == 1, "event handling refused");

    Require(sceUsbdAllocTransfer(-1) == nullptr, "negative isochronous packet count accepted");
    void* transfer = sceUsbdAllocTransfer(2);
    Require(transfer != nullptr, "transfer allocation failed");
    Require(*reinterpret_cast<std::int32_t*>(static_cast<std::uint8_t*>(transfer) + 56) == 2, "isochronous packet count not stored");
    std::uint8_t buffer[8]{};
    sceUsbdFillInterruptTransfer(transfer, &fakeDevice, 0x81, buffer, sizeof(buffer), nullptr, &fakeDevice, 100);
    auto* bytes = static_cast<std::uint8_t*>(transfer);
    Require(*reinterpret_cast<void**>(bytes) == &fakeDevice && bytes[9] == 0x81 && bytes[10] == 3, "interrupt transfer header not filled");
    Require(*reinterpret_cast<std::int32_t*>(bytes + 20) == sizeof(buffer), "interrupt transfer length not filled");
    Require(*reinterpret_cast<std::uint8_t**>(bytes + 48) == buffer, "interrupt transfer buffer not filled");
    Require(sceUsbdSubmitTransfer(transfer) == noDevice, "a transfer was submitted");
    Require(sceUsbdCancelTransfer(transfer) == notFound, "an idle transfer was cancelled");
    Require(sceUsbdSubmitTransfer(nullptr) == invalidArgument, "null transfer submitted");
    sceUsbdFreeTransfer(transfer);
    sceUsbdFreeTransfer(nullptr);
    sceUsbdExit();
    std::puts("USBD tests passed");
    return 0;
}
