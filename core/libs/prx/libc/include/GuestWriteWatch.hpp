#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GUESTWRITEWATCH_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GUESTWRITEWATCH_HPP

#include <cstddef>
#include <cstdint>

namespace GuestWriteWatch {

extern "C" {

bool GuestWriteWatchAvailable_nid_postfix();
void GuestWriteWatchRegister_nid_postfix(const void* pointer, std::size_t bytes);
bool GuestWriteWatchUnregister_nid_postfix(const void* pointer, std::size_t bytes);
bool GuestWriteWatchCovers_nid_postfix(std::uintptr_t address, std::size_t bytes);
bool GuestWriteWatchCollect_nid_postfix(std::uintptr_t address, std::size_t bytes, void (*written)(void* context, std::uintptr_t begin, std::uintptr_t end), void* context);
// macOS: re-reads the protection of a watched range after the guest changed it, and opens a watched
// range before the kernel writes into it (a write-protected page would fail the system call).
void GuestWriteWatchReprotect_nid_postfix(const void* pointer, std::size_t bytes);
void GuestWriteWatchHostWrite_nid_postfix(const void* pointer, std::size_t bytes);
// macOS: whether a read-only range is only write-protected by the watch, and the guest may write it.
bool GuestWriteWatchWritable_nid_postfix(std::uintptr_t address, std::size_t bytes);

}

}

#endif
