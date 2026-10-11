#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceSystemService/SystemService.hpp"

// The launcher module wraps libSceErrorDialog, libSceShare and libSceSystemService with the same results.

namespace {

constexpr int ERROR_DIALOG_ERROR_PARAM = static_cast<int>(0x80ED0003u);
constexpr int ERROR_DIALOG_ERROR_INVALID_STATE = static_cast<int>(0x80ED0005u);
constexpr std::size_t ERROR_DIALOG_PARAM_SIZE = 16;
constexpr int SHARE_ERROR_INVALID_PARAM = static_cast<int>(0x81960002);

std::mutex g_dialogLock;
bool g_dialogOpen = false;

}

extern "C" {

int APS5_VABI ErrorDialogClose(void) {
 std::lock_guard lock(g_dialogLock);
 if (!g_dialogOpen) return ERROR_DIALOG_ERROR_INVALID_STATE;
 g_dialogOpen = false;
 return 0;
}

int APS5_VABI ErrorDialogOpen(const void* param) {
 if (param == nullptr) return ERROR_DIALOG_ERROR_PARAM;
 std::int32_t size = 0;
 std::memcpy(&size, param, sizeof(size));
 if (static_cast<std::size_t>(size) != ERROR_DIALOG_PARAM_SIZE) return ERROR_DIALOG_ERROR_PARAM;
 std::lock_guard lock(g_dialogLock);
 if (g_dialogOpen) return ERROR_DIALOG_ERROR_INVALID_STATE;
 g_dialogOpen = true;
 return 0;
}

int APS5_VABI ShareGetCurrentStatus(uint32_t feature_flag, ShareCurrentStatus* status) {
 if (feature_flag == 0 || status == nullptr) return SHARE_ERROR_INVALID_PARAM;
 std::memset(status, 0, sizeof(*status));
 return 0;
}

int APS5_VABI ShareInitialize(size_t heap_size, int thread_priority, uint64_t affinity_mask) {
 (void)heap_size;
 (void)thread_priority;
 (void)affinity_mask;
 return 0;
}

int APS5_VABI ShareTerminate(void) {
 return 0;
}

int APS5_VABI SystemServiceParamGetInt(int param_id, int* value) {
 if (value == nullptr) return SYSTEM_SERVICE_ERROR_PARAMETER;
 *value = SystemServiceParamInt(param_id);
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI SystemServiceParamGetString(int param_id, char* buf, size_t buf_size) {
 if (buf == nullptr || param_id != SYSTEM_SERVICE_PARAM_ID_SYSTEM_NAME || buf_size < SYSTEM_SERVICE_MAX_SYSTEM_NAME_LENGTH) {
  return SYSTEM_SERVICE_ERROR_PARAMETER;
 }
 constexpr char SystemName[] = "PS5";
 std::memcpy(buf, SystemName, sizeof(SystemName));
 return SYSTEM_SERVICE_OK;
}

}
