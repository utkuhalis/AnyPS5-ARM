#include "prx/libSceAgc/DcbState/include/Workload.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// Workload states only steer the console's GPU scheduler, which has no counterpart here, so each
// command is a NOP carrying its arguments: it keeps the command buffer layout the title expects.
namespace {

std::uint32_t* WriteWorkloadsActive(CommandBuffer* buf, std::uint32_t streamId, const std::uint32_t* workloadIds, std::uint32_t workloadCount, const char* function) {
    Agc::Command::Require(workloadCount <= 0x3ffeu, function, "too many workloads");
    if (workloadCount != 0) Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(workloadIds), 4, function);
    auto* packet = Agc::Command::WriteNop(buf, workloadCount + 3u, function);
    packet[1] = streamId;
    packet[2] = workloadCount;
    std::copy_n(workloadIds, workloadCount, packet + 3);
    return packet;
}

std::uint32_t* WriteWorkloadComplete(CommandBuffer* buf, std::uint32_t streamId, std::uint32_t workloadId, const char* function) {
    auto* packet = Agc::Command::WriteNop(buf, 3, function);
    packet[1] = streamId;
    packet[2] = workloadId;
    return packet;
}

std::uint32_t* WriteWorkloadStreamInactive(CommandBuffer* buf, std::uint32_t streamId, const char* function) {
    auto* packet = Agc::Command::WriteNop(buf, 2, function);
    packet[1] = streamId;
    return packet;
}

}

extern "C" {

uint32_t* APS5_VABI sceAgcDcbSetWorkloadComplete(CommandBuffer* buf, uint32_t stream_id, uint32_t workload_id) {
    return WriteWorkloadComplete(buf, stream_id, workload_id, __func__);
}

uint32_t* APS5_VABI sceAgcDcbSetWorkloadsActive(CommandBuffer* buf, uint32_t stream_id, const uint32_t* workload_ids, uint32_t workload_count) {
    return WriteWorkloadsActive(buf, stream_id, workload_ids, workload_count, __func__);
}

uint32_t* APS5_VABI sceAgcDcbSetWorkloadStreamInactive(CommandBuffer* buf, uint32_t stream_id) {
    return WriteWorkloadStreamInactive(buf, stream_id, __func__);
}

uint32_t* APS5_VABI sceAgcAcbSetWorkloadComplete(CommandBuffer* buf, uint32_t stream_id, uint32_t workload_id) {
    return WriteWorkloadComplete(buf, stream_id, workload_id, __func__);
}

uint32_t* APS5_VABI sceAgcAcbSetWorkloadsActive(CommandBuffer* buf, uint32_t stream_id, const uint32_t* workload_ids, uint32_t workload_count) {
    return WriteWorkloadsActive(buf, stream_id, workload_ids, workload_count, __func__);
}

uint32_t* APS5_VABI sceAgcAcbSetWorkloadStreamInactive(CommandBuffer* buf, uint32_t stream_id) {
    return WriteWorkloadStreamInactive(buf, stream_id, __func__);
}

}
