#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>

extern "C" {
std::int32_t APS5_VABI sceSystemGestureOpen(std::int32_t, const void*);
int APS5_VABI sceSystemGestureGetPrimitiveTouchEvents(std::int32_t, void*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceSystemGestureGetPrimitiveTouchEventsCount(std::int32_t);
int APS5_VABI sceSystemGestureGetTouchEvents(std::int32_t, const void*, void*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceSystemGestureGetPrimitiveTouchEventByIndex(std::int32_t, std::uint32_t, void*);
int APS5_VABI sceSystemGestureAppendTouchRecognizer(std::int32_t, void*);
int APS5_VABI sceSystemGestureRemoveTouchRecognizer(std::int32_t, void*);
int APS5_VABI sceSystemGestureResetTouchRecognizer(std::int32_t, void*);
int APS5_VABI sceSystemGestureUpdateTouchRecognizer(std::int32_t, void*);
int APS5_VABI sceSystemGestureUpdateTouchRecognizerRectangle(std::int32_t, void*, const void*);
int APS5_VABI sceSystemGestureResetPrimitiveTouchRecognizer(std::int32_t);
int APS5_VABI sceSystemGestureUpdateAllTouchRecognizer(std::int32_t);
int APS5_VABI sceSystemGestureUpdatePrimitiveTouchRecognizer(std::int32_t, const void*);
int APS5_VABI sceSystemGestureCreateTouchRecognizer(std::int32_t, void*, std::int32_t, const void*, const void*);
int APS5_VABI sceSystemGestureGetTouchRecognizerInformation(std::int32_t, const void*, void*);
int APS5_VABI sceSystemGestureGetPrimitiveTouchEventByPrimitiveID(std::int32_t, std::uint16_t, void*);
int APS5_VABI sceSystemGestureGetTouchEventByEventID(std::int32_t, const void*, std::uint32_t, void*);
}

namespace {

struct Rectangle {
    float x;
    float y;
    float width;
    float height;
    std::uint8_t reserve[8];
};

struct RecognizerInformation {
    std::int32_t gestureType;
    Rectangle rectangle;
    std::uint64_t updatedTime;
    std::uint8_t reserve[256];
};

}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    constexpr int invalidArgument = static_cast<int>(0x80D10002);
    constexpr int invalidHandle = static_cast<int>(0x80D10003);
    const std::int32_t handle = sceSystemGestureOpen(0, nullptr);

    std::array<unsigned char, 64> buffer;
    buffer.fill(0x5a);
    const auto original = buffer;
    std::uint32_t count = 7;
    Require(sceSystemGestureGetPrimitiveTouchEvents(handle, buffer.data(), 4, &count) == 0);
    Require(count == 0);
    Require(buffer == original);

    count = 7;
    Require(sceSystemGestureGetPrimitiveTouchEvents(handle, buffer.data(), 4, nullptr) == invalidArgument);
    Require(sceSystemGestureGetPrimitiveTouchEvents(handle + 1, buffer.data(), 4, &count) == invalidHandle);
    Require(count == 7);

    Require(sceSystemGestureGetPrimitiveTouchEventsCount(handle) == 0);
    Require(sceSystemGestureGetPrimitiveTouchEventsCount(handle + 1) == invalidHandle);

    constexpr int indexOutOfArray = static_cast<int>(0x80D10005);
    std::array<std::uint64_t, 361> recognizer{};
    Rectangle rectangle{10.0f, 20.0f, 300.0f, 400.0f, {}};
    count = 7;
    Require(sceSystemGestureGetTouchEvents(handle, recognizer.data(), buffer.data(), 4, &count) == 0 && count == 0);
    Require(sceSystemGestureGetTouchEvents(handle, recognizer.data(), buffer.data(), 4, nullptr) == invalidArgument);
    Require(sceSystemGestureGetTouchEvents(handle + 1, recognizer.data(), buffer.data(), 4, &count) == invalidHandle);
    Require(sceSystemGestureGetPrimitiveTouchEventByIndex(handle, 0, buffer.data()) == indexOutOfArray);
    Require(sceSystemGestureGetPrimitiveTouchEventByIndex(handle + 1, 0, buffer.data()) == invalidHandle);
    Require(buffer == original);

    Require(sceSystemGestureAppendTouchRecognizer(handle, recognizer.data()) == 0);
    Require(sceSystemGestureRemoveTouchRecognizer(handle, recognizer.data()) == 0);
    Require(sceSystemGestureResetTouchRecognizer(handle, recognizer.data()) == 0);
    Require(sceSystemGestureUpdateTouchRecognizer(handle, recognizer.data()) == 0);
    Require(sceSystemGestureUpdateTouchRecognizerRectangle(handle, recognizer.data(), &rectangle) == 0);
    Require(sceSystemGestureResetPrimitiveTouchRecognizer(handle) == 0);
    Require(sceSystemGestureUpdateAllTouchRecognizer(handle) == 0);
    Require(sceSystemGestureUpdatePrimitiveTouchRecognizer(handle, nullptr) == 0);
    Require(sceSystemGestureAppendTouchRecognizer(handle, nullptr) == invalidArgument);
    Require(sceSystemGestureUpdateTouchRecognizerRectangle(handle, recognizer.data(), nullptr) == invalidArgument);
    Require(sceSystemGestureAppendTouchRecognizer(handle + 1, recognizer.data()) == invalidHandle);
    Require(sceSystemGestureRemoveTouchRecognizer(handle, nullptr) == invalidArgument);
    Require(sceSystemGestureResetTouchRecognizer(handle, nullptr) == invalidArgument);
    Require(sceSystemGestureUpdateTouchRecognizer(handle, nullptr) == invalidArgument);
    Require(sceSystemGestureUpdateTouchRecognizerRectangle(handle, nullptr, &rectangle) == invalidArgument);
    Require(sceSystemGestureRemoveTouchRecognizer(handle + 1, recognizer.data()) == invalidHandle);
    Require(sceSystemGestureResetTouchRecognizer(handle + 1, recognizer.data()) == invalidHandle);
    Require(sceSystemGestureUpdateTouchRecognizer(handle + 1, recognizer.data()) == invalidHandle);
    Require(sceSystemGestureUpdateTouchRecognizerRectangle(handle + 1, recognizer.data(), &rectangle) == invalidHandle);
    Require(sceSystemGestureResetPrimitiveTouchRecognizer(handle + 1) == invalidHandle);
    Require(sceSystemGestureUpdateAllTouchRecognizer(handle + 1) == invalidHandle);
    Require(sceSystemGestureUpdatePrimitiveTouchRecognizer(handle + 1, nullptr) == invalidHandle);

    Require(sceSystemGestureCreateTouchRecognizer(handle, recognizer.data(), 2, &rectangle, nullptr) == 0);
    RecognizerInformation information{};
    information.updatedTime = 99;
    Require(sceSystemGestureGetTouchRecognizerInformation(handle, recognizer.data(), &information) == 0);
    Require(information.gestureType == 2 && information.rectangle.x == 10.0f && information.rectangle.height == 400.0f && information.updatedTime == 0);
    const Rectangle moved{1.0f, 2.0f, 3.0f, 4.0f, {}};
    Require(sceSystemGestureUpdateTouchRecognizerRectangle(handle, recognizer.data(), &moved) == 0);
    Require(sceSystemGestureGetTouchRecognizerInformation(handle, recognizer.data(), &information) == 0);
    Require(information.gestureType == 2 && information.rectangle.x == 1.0f && information.rectangle.height == 4.0f);
    Require(sceSystemGestureGetTouchRecognizerInformation(handle, recognizer.data(), nullptr) == invalidArgument);
    Require(sceSystemGestureGetTouchRecognizerInformation(handle + 1, recognizer.data(), &information) == invalidHandle);
    Require(sceSystemGestureGetPrimitiveTouchEventByPrimitiveID(handle, 0, buffer.data()) == indexOutOfArray);
    Require(sceSystemGestureGetPrimitiveTouchEventByPrimitiveID(handle, 0, nullptr) == invalidArgument);
    Require(sceSystemGestureGetPrimitiveTouchEventByPrimitiveID(handle + 1, 0, buffer.data()) == invalidHandle);
    std::array<unsigned char, 168> touchEvent{};
    Require(sceSystemGestureGetTouchEventByEventID(handle, recognizer.data(), 0, touchEvent.data()) == indexOutOfArray);
    Require(sceSystemGestureGetTouchEventByEventID(handle, nullptr, 0, touchEvent.data()) == invalidArgument);
    Require(sceSystemGestureGetTouchEventByEventID(handle + 1, recognizer.data(), 0, touchEvent.data()) == invalidHandle);
    return 0;
}
