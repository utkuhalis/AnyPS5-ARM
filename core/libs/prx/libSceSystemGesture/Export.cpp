#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// No touch input is emulated: recognizers exist but never report events.
static constexpr int32_t GESTURE_HANDLE = 1;
static constexpr int SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80D10002);
static constexpr int SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE = static_cast<int>(0x80D10003);
static constexpr int SCE_SYSTEM_GESTURE_ERROR_INDEX_OUT_OF_ARRAY = static_cast<int>(0x80D10005);

namespace {

// The recognizer's opaque storage keeps what the title configured, so it can be reported back.
struct RecognizerConfig {
    int32_t type;
    SystemGestureRectangle rectangle;
};
static_assert(sizeof(RecognizerConfig) <= sizeof(SystemGestureTouchRecognizer));

void StoreConfig(SystemGestureTouchRecognizer* recognizer, const RecognizerConfig& config) {
    std::memcpy(recognizer->reserve, &config, sizeof(config));
}

RecognizerConfig LoadConfig(const SystemGestureTouchRecognizer* recognizer) {
    RecognizerConfig config;
    std::memcpy(&config, recognizer->reserve, sizeof(config));
    return config;
}

}

extern "C" {

int APS5_VABI sceSystemGestureAppendTouchRecognizer(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer) {
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!recognizer) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceSystemGestureClose(int32_t gesture_handle) {
    return gesture_handle == GESTURE_HANDLE ? 0 : SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
}

int APS5_VABI sceSystemGestureCreateTouchRecognizer(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer, int32_t type, const SystemGestureRectangle* rectangle, const void* param) {
    (void)param;
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!recognizer) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    std::memset(recognizer, 0, sizeof(*recognizer));
    RecognizerConfig config{type, {}};
    if (rectangle) config.rectangle = *rectangle;
    StoreConfig(recognizer, config);
    return 0;
}

int APS5_VABI sceSystemGestureFinalizePrimitiveTouchRecognizer(void) {
    return 0;
}

int APS5_VABI sceSystemGestureGetPrimitiveTouchEventByIndex(int32_t gesture_handle, uint32_t index, SystemGesturePrimitiveTouchEvent* event) {
    (void)index;
    (void)event;
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    return SCE_SYSTEM_GESTURE_ERROR_INDEX_OUT_OF_ARRAY;
}

// No touch primitive or event ever exists, so a lookup by ID finds nothing, as a lookup by index does.
int APS5_VABI sceSystemGestureGetPrimitiveTouchEventByPrimitiveID(int32_t gesture_handle, uint16_t primitiveId, SystemGesturePrimitiveTouchEvent* event) {
    (void)primitiveId;
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!event) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    return SCE_SYSTEM_GESTURE_ERROR_INDEX_OUT_OF_ARRAY;
}

int APS5_VABI sceSystemGestureGetPrimitiveTouchEvents(int32_t gesture_handle, SystemGesturePrimitiveTouchEvent* event_buffer, uint32_t capacity_of_buffer, uint32_t* number_of_event) {
    (void)event_buffer;
    (void)capacity_of_buffer;
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!number_of_event) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    *number_of_event = 0;
    return 0;
}

int APS5_VABI sceSystemGestureGetPrimitiveTouchEventsCount(int32_t gesture_handle) {
    return gesture_handle == GESTURE_HANDLE ? 0 : SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
}

int APS5_VABI sceSystemGestureGetTouchEventByEventID(int32_t gesture_handle, const SystemGestureTouchRecognizer* recognizer, uint32_t eventId, SystemGestureTouchEvent* event) {
    (void)eventId;
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!recognizer || !event) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    return SCE_SYSTEM_GESTURE_ERROR_INDEX_OUT_OF_ARRAY;
}

int APS5_VABI sceSystemGestureGetTouchEventByIndex(int32_t gesture_handle, const SystemGestureTouchRecognizer* recognizer, uint32_t index, SystemGestureTouchEvent* event) {
    (void)gesture_handle;
    (void)recognizer;
    (void)index;
    (void)event;
    return SCE_SYSTEM_GESTURE_ERROR_INDEX_OUT_OF_ARRAY;
}

int APS5_VABI sceSystemGestureGetTouchEvents(int32_t gesture_handle, const SystemGestureTouchRecognizer* recognizer, SystemGestureTouchEvent* event_buffer, uint32_t capacity_of_buffer, uint32_t* number_of_event) {
    (void)recognizer;
    (void)event_buffer;
    (void)capacity_of_buffer;
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!number_of_event) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    *number_of_event = 0;
    return 0;
}

int APS5_VABI sceSystemGestureGetTouchEventsCount(int32_t gesture_handle, const SystemGestureTouchRecognizer* recognizer) {
    (void)recognizer;
    return gesture_handle == GESTURE_HANDLE ? 0 : SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
}

int APS5_VABI sceSystemGestureGetTouchRecognizerInformation(int32_t gesture_handle, const SystemGestureTouchRecognizer* recognizer, SystemGestureTouchRecognizerInformation* information) {
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!recognizer || !information) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    const RecognizerConfig config = LoadConfig(recognizer);
    std::memset(information, 0, sizeof(*information));
    information->gesture_type = config.type;
    information->rectangle = config.rectangle;
    return 0;
}

int APS5_VABI sceSystemGestureInitializePrimitiveTouchRecognizer(const void* param) {
    (void)param;
    return 0;
}

int32_t APS5_VABI sceSystemGestureOpen(int32_t input_type, const void* param) {
    (void)input_type;
    (void)param;
    return GESTURE_HANDLE;
}

int APS5_VABI sceSystemGestureRemoveTouchRecognizer(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer) {
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!recognizer) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceSystemGestureResetPrimitiveTouchRecognizer(int32_t gesture_handle) {
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    return 0;
}

int APS5_VABI sceSystemGestureResetTouchRecognizer(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer) {
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!recognizer) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceSystemGestureUpdateAllTouchRecognizer(int32_t gesture_handle) {
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    return 0;
}

int APS5_VABI sceSystemGestureUpdatePrimitiveTouchRecognizer(int32_t gesture_handle, const void* param) {
    (void)param;
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    return 0;
}

int APS5_VABI sceSystemGestureUpdateTouchRecognizer(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer) {
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!recognizer) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceSystemGestureUpdateTouchRecognizerRectangle(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer, const SystemGestureRectangle* rectangle) {
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!recognizer) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    if (!rectangle) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    RecognizerConfig config = LoadConfig(recognizer);
    config.rectangle = *rectangle;
    StoreConfig(recognizer, config);
    return 0;
}

}
