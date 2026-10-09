#ifndef PRX_LIBC_INCLUDE_SPECIFICS_MACOS_EXECUTABLEPATH_HPP
#define PRX_LIBC_INCLUDE_SPECIFICS_MACOS_EXECUTABLEPATH_HPP

#ifdef __APPLE__
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <mach-o/dyld.h>
#include <stdexcept>
#include <string>

inline std::filesystem::path MacOsExecutablePath() {
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string path(size, '\0');
    if (_NSGetExecutablePath(path.data(), &size) != 0) throw std::runtime_error("Cannot locate the executable");
    path.resize(std::strlen(path.c_str()));
    return std::filesystem::canonical(path);
}
#endif

#endif
