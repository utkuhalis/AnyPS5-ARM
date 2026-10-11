#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <filesystem>
#include <new>

static int RenamePath_nid_no_patch(const std::filesystem::path& source, const std::filesystem::path& destination) {
    constexpr int GuestEnoent = 2;
    constexpr int GuestEio = 5;
    constexpr int GuestEnotdir = 20;
    constexpr int GuestEisdir = 21;
    constexpr int GuestEinval = 22;
    constexpr int GuestEnotempty = 66;
    namespace fs = std::filesystem;
    std::error_code error;
    const auto sourceStatus = fs::symlink_status(source, error);
    if (error && error != std::errc::no_such_file_or_directory) return FilesystemError(error);
    if (sourceStatus.type() == fs::file_type::not_found) return GuestEnoent;

    const auto parent = destination.parent_path().empty() ? fs::path(".") : destination.parent_path();
    error.clear();
    const auto parentStatus = fs::status(parent, error);
    if (error) return FilesystemError(error);
    if (!fs::exists(parentStatus)) return GuestEnoent;
    if (!fs::is_directory(parentStatus)) return GuestEnotdir;

    error.clear();
    const auto destinationStatus = fs::symlink_status(destination, error);
    if (error && error != std::errc::no_such_file_or_directory) return FilesystemError(error);
    const bool destinationExists = destinationStatus.type() != fs::file_type::not_found;
    const bool sourceIsDirectory = fs::is_directory(sourceStatus);
    const bool destinationIsDirectory = destinationExists && fs::is_directory(destinationStatus);
    bool sameFile = false;
    if (destinationExists) {
        error.clear();
        sameFile = fs::equivalent(source, destination, error);
        // libc++ reports not_supported for a dangling destination where libstdc++ reports ENOENT.
        if (error == std::errc::no_such_file_or_directory || error == std::errc::not_supported) error.clear();
        if (error) return FilesystemError(error);
        if (!sameFile && sourceIsDirectory && !destinationIsDirectory) return GuestEnotdir;
        if (!sameFile && !sourceIsDirectory && destinationIsDirectory) return GuestEisdir;
    }
    if (sourceIsDirectory && !sameFile) {
        error.clear();
        const auto canonicalSource = fs::weakly_canonical(source, error);
        if (error) return FilesystemError(error);
        const auto canonicalDestination = fs::weakly_canonical(destination, error);
        if (error) return FilesystemError(error);
        const auto relative = canonicalDestination.lexically_relative(canonicalSource);
        if (!relative.empty() && *relative.begin() != "..") return GuestEinval;
    }
    if (destinationExists && !sameFile && destinationIsDirectory) {
        error.clear();
        if (!fs::is_empty(destination, error)) {
            return error ? FilesystemError(error) : GuestEnotempty;
        }
    }
#ifdef _WIN32
    if (!sameFile && sourceIsDirectory && destinationIsDirectory) {
        error.clear();
        if (!fs::remove(destination, error)) return error ? FilesystemError(error) : GuestEio;
    }
#endif

    error.clear();
    fs::rename(source, destination, error);
    return error ? FilesystemError(error) : 0;
}

extern "C" int APS5_VABI rename_nid_postfix(const char* from, const char* to) {
    if (!from || !to) { errno = 14; return -1; }
    if (!*from || !*to) { errno = 2; return -1; }
    try {
        const auto source = ResolvePath_nid_no_patch(from);
        const auto destination = ResolvePath_nid_no_patch(to);
        const int error = RenamePath_nid_no_patch(source, destination);
        if (error) { errno = error; return -1; }
        RecordWrittenPath_nid_no_patch(source);
        RecordWrittenPath_nid_no_patch(destination);
        return 0;
    } catch (const std::bad_alloc&) { errno = 12; return -1; }
      catch (const std::filesystem::filesystem_error& error) {
        errno = FilesystemError(error.code());
        return -1;
    }
}
