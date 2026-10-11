#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// These shell modules wrap libSceRemoteplay and libSceShare, neither of which has a host session.

namespace {

constexpr int REMOTEPLAY_CONNECTION_STATUS_DISCONNECT = 0;
constexpr int SHARE_ERROR_INVALID_PARAM = static_cast<int>(0x81960002);

}

extern "C" {

int APS5_VABI RemotePlayGetConnectionStatus(int user_id, int* status) {
 (void)user_id;
 if (!status) APS5_INVALID_ARG_EX;
 *status = REMOTEPLAY_CONNECTION_STATUS_DISCONNECT;
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

}
