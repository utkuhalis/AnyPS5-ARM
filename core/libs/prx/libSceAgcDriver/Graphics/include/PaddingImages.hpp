#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PADDINGIMAGES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PADDINGIMAGES_HPP

#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include <array>
#include <cstdint>

namespace AgcDriver::Graphics {

class PaddingImages {
public:
    explicit PaddingImages(const Context& context);
    ~PaddingImages();
    PaddingImages(const PaddingImages&) = delete;
    PaddingImages& operator=(const PaddingImages&) = delete;

    VkImageView View(std::uint32_t heapBinding) const;

private:
    struct Entry {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
    };

    bool create(std::uint32_t heapBinding, Entry& entry);
    void release() noexcept;

    Context context;
    std::array<Entry, 64> entries{};
};

}

#endif
