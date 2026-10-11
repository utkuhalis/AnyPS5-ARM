#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
extern "C" {
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI close_nid_postfix(int);
int APS5_VABI sceKernelFcntl(int, int, ...);
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t);
int APS5_VABI sceKernelClose(int);
}
static void Require(bool value) { if (!value) std::abort(); }
static constexpr int GetFlags = 3;
static constexpr int SetFlags = 4;
static constexpr int ReadWrite = 2;
static constexpr int NonBlocking = 4;
static constexpr int SceEbadf = static_cast<int>(0x80020009u);
static constexpr int SceEinval = static_cast<int>(0x80020016u);
static constexpr int SceEnotsup = static_cast<int>(0x8002002du);
int main() {
    const int socket = socket_nid_postfix(2, 2, 0);
    Require(socket >= 0);
    Require(sceKernelFcntl(socket, GetFlags) == ReadWrite);
    Require(sceKernelFcntl(socket, SetFlags, ReadWrite | NonBlocking) == 0);
    Require(sceKernelFcntl(socket, GetFlags) == (ReadWrite | NonBlocking));
    Require(sceKernelFcntl(socket, SetFlags, 0) == 0);
    Require(sceKernelFcntl(socket, GetFlags) == ReadWrite);
    Require(sceKernelFcntl(socket, SetFlags, 0x8000) == SceEnotsup);
    Require(sceKernelFcntl(socket, GetFlags) == ReadWrite);
    Require(sceKernelFcntl(socket, 5) == SceEinval);
    Require(sceKernelFcntl(socket + 4096, GetFlags) == SceEbadf);
    Require(close_nid_postfix(socket) == 0);
    Require(sceKernelFcntl(socket, GetFlags) == SceEbadf);
    Require(sceKernelFcntl(-1, GetFlags) == SceEbadf);
    Require(sceKernelFcntl(12345, GetFlags) == SceEbadf);
    const int file = sceKernelOpen("kernel_fcntl_probe.bin", 0x202, 0644);
    Require(file >= 0);
    bool rejected = false;
    try {
        sceKernelFcntl(file, GetFlags);
    } catch (const std::runtime_error& error) {
        rejected = std::string(error.what()).rfind("sceKernelFcntl: file descriptors are not implemented", 0) == 0;
    }
    Require(rejected);
    Require(sceKernelClose(file) == 0);
    Require(sceKernelFcntl(file, GetFlags) == SceEbadf);
    std::remove("kernel_fcntl_probe.bin");
    return 0;
}
