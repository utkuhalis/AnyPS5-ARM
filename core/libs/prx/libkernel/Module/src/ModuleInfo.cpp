#include <cstdint>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#if defined(__APPLE__)
#include "prx/libc/include/specifics/linux/ElfTypes.hpp"
#include <dlfcn.h>
#include <mach-o/dyld.h>
#elif !defined(_WIN32)
#include <dlfcn.h>
#include <link.h>
#include <unistd.h>
#endif

extern "C" std::int32_t ModuleIdForImage_nid_no_patch(const void* native);

namespace {

#if !defined(_WIN32)
#ifdef __APPLE__
constexpr std::uint32_t PF_R = 4;
#endif
constexpr char GuestModuleSuffix[] = ".guest.prx";
constexpr std::int32_t ProtRead = 1;
constexpr std::int32_t ProtWrite = 2;
constexpr std::int32_t ProtExecute = 4;

struct ImageSearch {
    std::uintptr_t address;
    ModuleInfoEx* info;
    bool found;
};

bool Contains(const dl_phdr_info& image, std::uintptr_t begin, std::uint64_t size) {
    for (std::uint16_t i = 0; i < image.dlpi_phnum; ++i) {
        const auto& header = image.dlpi_phdr[i];
        if (header.p_type != PT_LOAD) continue;
        const std::uintptr_t start = image.dlpi_addr + header.p_vaddr;
        if (begin >= start && begin - start <= header.p_memsz && size <= header.p_memsz - (begin - start)) return true;
    }
    return false;
}

std::uint32_t ToU32(std::uint64_t value, const char* field) {
    if (value > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error(std::string("sceKernelGetModuleInfoFromAddr: ") + field + " exceeds 32 bits");
    return static_cast<std::uint32_t>(value);
}

std::uintptr_t EhFrameAddress(const dl_phdr_info& image, std::uintptr_t header) {
    if (!Contains(image, header, 8))
        throw std::runtime_error("sceKernelGetModuleInfoFromAddr: eh_frame_hdr outside the image");
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(header);
    if (bytes[0] != 1) throw std::runtime_error("sceKernelGetModuleInfoFromAddr: unsupported eh_frame_hdr version");
    const std::uintptr_t field = header + 4;
    switch (bytes[1]) {
    case 0x1b: {
        std::int32_t offset;
        std::memcpy(&offset, bytes + 4, sizeof(offset));
        return field + static_cast<std::intptr_t>(offset);
    }
    case 0x03: {
        std::uint32_t value;
        std::memcpy(&value, bytes + 4, sizeof(value));
        return value;
    }
    case 0x00: {
        std::uint64_t value;
        if (!Contains(image, header, 12))
            throw std::runtime_error("sceKernelGetModuleInfoFromAddr: eh_frame_hdr outside the image");
        std::memcpy(&value, bytes + 4, sizeof(value));
        return static_cast<std::uintptr_t>(value);
    }
    default:
        throw std::runtime_error("sceKernelGetModuleInfoFromAddr: unsupported eh_frame_ptr encoding");
    }
}

std::uint64_t EhFrameSize(const dl_phdr_info& image, std::uintptr_t frame) {
    std::uintptr_t position = frame;
    for (;;) {
        if (!Contains(image, position, 4))
            throw std::runtime_error("sceKernelGetModuleInfoFromAddr: eh_frame outside the image");
        std::uint32_t length;
        std::memcpy(&length, reinterpret_cast<const void*>(position), sizeof(length));
        position += 4;
        if (length == 0) return position - frame;
        std::uint64_t recordLength = length;
        if (length == 0xffffffffu) {
            if (!Contains(image, position, 8))
                throw std::runtime_error("sceKernelGetModuleInfoFromAddr: eh_frame outside the image");
            std::memcpy(&recordLength, reinterpret_cast<const void*>(position), sizeof(recordLength));
            position += 8;
        }
        if (!Contains(image, position, recordLength))
            throw std::runtime_error("sceKernelGetModuleInfoFromAddr: eh_frame record outside the image");
        position += recordLength;
    }
}

std::string ImageName(const dl_phdr_info& image) {
    std::string path = image.dlpi_name ? image.dlpi_name : "";
    if (path.empty()) {
        char executable[4096];
#ifdef __APPLE__
        std::uint32_t capacity = sizeof(executable);
        if (_NSGetExecutablePath(executable, &capacity) != 0) throw std::runtime_error("sceKernelGetModuleInfoFromAddr: cannot resolve the executable path");
        path = executable;
#else
        const auto length = ::readlink("/proc/self/exe", executable, sizeof(executable) - 1);
        if (length < 0) throw std::runtime_error("sceKernelGetModuleInfoFromAddr: cannot resolve the executable path");
        path.assign(executable, static_cast<std::size_t>(length));
#endif
    }
    std::string name = path.substr(path.find_last_of('/') + 1);
    if (name.size() > sizeof(GuestModuleSuffix) - 1 && name.ends_with(GuestModuleSuffix))
        name.resize(name.size() - (sizeof(GuestModuleSuffix) - 1));
    return name;
}

void Fill(const dl_phdr_info& image, ModuleInfoEx& info) {
    const auto name = ImageName(image);
    if (name.size() >= sizeof(info.name))
        throw std::runtime_error("sceKernelGetModuleInfoFromAddr: module name too long: " + name);
    std::memcpy(info.name, name.c_str(), name.size() + 1);
#ifndef __APPLE__
    info.tls_index = ToU32(image.dlpi_tls_modid, "TLS module index");
#endif
    for (std::uint16_t i = 0; i < image.dlpi_phnum; ++i) {
        const auto& header = image.dlpi_phdr[i];
        const std::uintptr_t address = image.dlpi_addr + header.p_vaddr;
        if (header.p_type == PT_LOAD && info.segment_count < std::size(info.segments)) {
            auto& segment = info.segments[info.segment_count++];
            segment.address = address;
            segment.size = ToU32(header.p_memsz, "segment size");
            segment.prot = ((header.p_flags & PF_R) ? ProtRead : 0) | ((header.p_flags & PF_W) ? ProtWrite : 0) | ((header.p_flags & PF_X) ? ProtExecute : 0);
        }
#ifndef __APPLE__
        else if (header.p_type == PT_TLS) {
            info.tls_init_addr = address;
            info.tls_init_size = ToU32(header.p_filesz, "TLS image size");
            info.tls_size = ToU32(header.p_memsz, "TLS size");
            info.tls_align = ToU32(header.p_align, "TLS alignment");
        }
#endif
        else if (header.p_type == PT_GNU_EH_FRAME) {
            info.eh_frame_hdr_addr = address;
            info.eh_frame_hdr_size = ToU32(header.p_memsz, "eh_frame_hdr size");
            info.eh_frame_addr = EhFrameAddress(image, address);
            info.eh_frame_size = ToU32(EhFrameSize(image, info.eh_frame_addr), "eh_frame size");
        }
#ifndef __APPLE__
        else if (header.p_type == PT_DYNAMIC) {
            for (const auto* entry = reinterpret_cast<const ElfW(Dyn)*>(address); entry->d_tag != DT_NULL; ++entry) {
                if (entry->d_tag == DT_INIT) info.init_proc_addr = image.dlpi_addr + entry->d_un.d_ptr;
                else if (entry->d_tag == DT_FINI) info.fini_proc_addr = image.dlpi_addr + entry->d_un.d_ptr;
            }
        }
#endif
    }
    info.ref_count = 1;
}

int FindImage(dl_phdr_info* image, std::size_t, void* data) {
    auto& search = *static_cast<ImageSearch*>(data);
    if (!Contains(*image, search.address, 1)) return 0;
    Fill(*image, *search.info);
    search.found = true;
    return 1;
}
#endif

}

extern "C" {

int APS5_VABI sceKernelGetModuleInfoFromAddr(std::uint64_t address, int flags, ModuleInfoEx* info) {
    if (!info) return SCE_KERNEL_ERROR_EFAULT;
    if (flags != 2) throw std::invalid_argument("sceKernelGetModuleInfoFromAddr: unsupported flags " + std::to_string(flags));
#if defined(_WIN32)
    if (info->st_size != sizeof(ModuleInfoEx))
        throw std::invalid_argument("sceKernelGetModuleInfoFromAddr: unsupported st_size " + std::to_string(info->st_size));
    (void)address;
    NotImplemented_nid_no_patch(__func__);
    return 0;
#elif defined(__APPLE__)
    // PPSA02929 calls this on exit with st_size left uninitialized; shadPS4 does not read it either.
    Dl_info symbol{};
    if (!::dladdr(reinterpret_cast<const void*>(address), &symbol) || symbol.dli_fname == nullptr) return SCE_KERNEL_ERROR_ESRCH;
    ModuleInfoEx result{};
    result.st_size = sizeof(ModuleInfoEx);
    ImageSearch search{static_cast<std::uintptr_t>(address), &result, false};
    dl_iterate_phdr(FindImage, &search);
    if (!search.found) return SCE_KERNEL_ERROR_ESRCH;
    const bool executable = symbol.dli_fbase == _dyld_get_image_header(0);
    void* native = ::dlopen(executable ? nullptr : symbol.dli_fname, RTLD_LAZY | RTLD_NOLOAD);
    result.id = ModuleIdForImage_nid_no_patch(native);
    if (native != nullptr) ::dlclose(native);
    *info = result;
    return 0;
#else
    if (info->st_size != sizeof(ModuleInfoEx))
        throw std::invalid_argument("sceKernelGetModuleInfoFromAddr: unsupported st_size " + std::to_string(info->st_size));
    Dl_info symbol{};
    link_map* native = nullptr;
    if (!dladdr1(reinterpret_cast<const void*>(address), &symbol, reinterpret_cast<void**>(&native), RTLD_DL_LINKMAP) || !native)
        return SCE_KERNEL_ERROR_ESRCH;
    ModuleInfoEx result{};
    result.st_size = sizeof(ModuleInfoEx);
    ImageSearch search{static_cast<std::uintptr_t>(address), &result, false};
    dl_iterate_phdr(FindImage, &search);
    if (!search.found) return SCE_KERNEL_ERROR_ESRCH;
    result.id = ModuleIdForImage_nid_no_patch(native);
    *info = result;
    return 0;
#endif
}

}
