#include "prx/libSceAgcDriver/Graphics/include/PaddingImages.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "RuntimeAbi.hpp"
#include <mutex>

namespace AgcDriver::Graphics {

namespace {

constexpr std::uint32_t FirstImageBinding = ShaderRecompiler::RuntimeAbi::FirstImageBinding;
constexpr std::uint32_t FirstComparisonImageBinding = ShaderRecompiler::RuntimeAbi::FirstComparisonImageBinding;
constexpr std::uint32_t FirstStorageImageBinding = ShaderRecompiler::RuntimeAbi::FirstStorageImageBinding;
constexpr std::uint32_t SamplersBinding = static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::Samplers);

struct Shape {
    VkImageType type;
    VkImageViewType viewType;
    std::uint32_t layers;
    VkSampleCountFlagBits samples;
};

Shape ShapeOf(std::uint32_t dimension) {
    switch (dimension) {
        case 0: return {VK_IMAGE_TYPE_1D, VK_IMAGE_VIEW_TYPE_1D, 1u, VK_SAMPLE_COUNT_1_BIT};
        case 1: return {VK_IMAGE_TYPE_1D, VK_IMAGE_VIEW_TYPE_1D_ARRAY, 1u, VK_SAMPLE_COUNT_1_BIT};
        case 2: return {VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, 1u, VK_SAMPLE_COUNT_1_BIT};
        case 3: return {VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D_ARRAY, 1u, VK_SAMPLE_COUNT_1_BIT};
        case 4: return {VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, 1u, VK_SAMPLE_COUNT_4_BIT};
        case 5: return {VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D_ARRAY, 1u, VK_SAMPLE_COUNT_4_BIT};
        default: return {VK_IMAGE_TYPE_3D, VK_IMAGE_VIEW_TYPE_3D, 1u, VK_SAMPLE_COUNT_1_BIT};
    }
}

}

PaddingImages::PaddingImages(const Context& context) : context(context) {
    std::lock_guard lock(GuestMemory::GpuMutex());
    CommandBatch batch(context);
    std::vector<VkImageMemoryBarrier> toGeneral;
    try {
        for (std::uint32_t binding = FirstImageBinding; binding < SamplersBinding; ++binding) {
            auto& entry = entries[binding];
            if (!create(binding, entry)) continue;
            const bool depth = binding >= FirstComparisonImageBinding && binding < FirstStorageImageBinding;
            const VkImageAspectFlags aspect = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
            VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransfer.image = entry.image;
            toTransfer.subresourceRange = {aspect, 0, 1, 0, 1};
            context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(batch.Handle(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);
            if (depth) {
                const VkClearDepthStencilValue zero{0.0f, 0u};
                context.Function<PFN_vkCmdClearDepthStencilImage>("vkCmdClearDepthStencilImage")(batch.Handle(), entry.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &zero, 1, &toTransfer.subresourceRange);
            } else {
                const VkClearColorValue zero{};
                context.Resolved(&DeviceFunctions::cmdClearColorImage, "vkCmdClearColorImage")(batch.Handle(), entry.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &zero, 1, &toTransfer.subresourceRange);
            }
            auto ready = toTransfer;
            ready.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            ready.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            ready.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            ready.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            toGeneral.push_back(ready);
        }
        if (!toGeneral.empty()) context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(batch.Handle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, static_cast<std::uint32_t>(toGeneral.size()), toGeneral.data());
        batch.SubmitAndWait();
    } catch (...) {
        release();
        throw;
    }
}

PaddingImages::~PaddingImages() {
    release();
}

VkImageView PaddingImages::View(std::uint32_t heapBinding) const {
    return heapBinding < entries.size() ? entries[heapBinding].view : VK_NULL_HANDLE;
}

bool PaddingImages::create(std::uint32_t heapBinding, Entry& entry) {
    const bool storage = heapBinding >= FirstStorageImageBinding;
    const bool comparison = !storage && heapBinding >= FirstComparisonImageBinding;
    const auto first = storage ? FirstStorageImageBinding : comparison ? FirstComparisonImageBinding : FirstImageBinding;
    const auto numericClass = (heapBinding - first) / 7u;
    const auto shape = ShapeOf((heapBinding - first) % 7u);
    VkFormat format;
    if (comparison) format = VK_FORMAT_D32_SFLOAT;
    else if (!storage) format = numericClass == 0u ? VK_FORMAT_R8G8B8A8_UNORM : numericClass == 1u ? VK_FORMAT_R8G8B8A8_UINT : VK_FORMAT_R8G8B8A8_SINT;
    else format = numericClass == 0u ? VK_FORMAT_R32G32B32A32_SFLOAT : numericClass == 1u ? VK_FORMAT_R32G32B32A32_UINT : numericClass == 2u ? VK_FORMAT_R32_UINT : VK_FORMAT_R64_UINT;
    const VkImageUsageFlags usage = (storage ? VK_IMAGE_USAGE_STORAGE_BIT : VK_IMAGE_USAGE_SAMPLED_BIT) | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VkImageFormatProperties supported{};
    if (context.imageFormatProperties(context.physical, format, shape.type, VK_IMAGE_TILING_OPTIMAL, usage, 0, &supported) != VK_SUCCESS || (supported.sampleCounts & shape.samples) == 0) return false;
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType = shape.type;
    info.format = format;
    info.extent = {1u, 1u, 1u};
    info.mipLevels = 1;
    info.arrayLayers = shape.layers;
    info.samples = shape.samples;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &entry.image), "vkCreateImage padding image");
    VkMemoryRequirements requirements{};
    context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, entry.image, &requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &entry.memory), "vkAllocateMemory padding image");
    Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, entry.image, entry.memory, 0), "vkBindImageMemory padding image");
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = entry.image;
    viewInfo.viewType = shape.viewType;
    viewInfo.format = format;
    viewInfo.subresourceRange = {static_cast<VkImageAspectFlags>(comparison ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT), 0, 1, 0, shape.layers};
    Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &entry.view), "vkCreateImageView padding image");
    return true;
}

void PaddingImages::release() noexcept {
    for (auto& entry : entries) {
        if (entry.view) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, entry.view, nullptr);
        if (entry.image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, entry.image, nullptr);
        if (entry.memory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, entry.memory, nullptr);
        entry = {};
    }
}

}
