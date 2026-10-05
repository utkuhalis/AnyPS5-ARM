#ifdef __APPLE__

#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#include <pthread.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>

// AppKit (windows, events, game controllers) only works on the main thread, and a guest never hands
// its thread back. The macOS entry stub therefore starts the guest on another thread through
// Aps5StartGuest and keeps the main thread in its run loop; libraries send their AppKit work there
// with Aps5RunOnMainThread.

namespace {

struct GuestStart {
    void (*entry)(void*, void*);
    void* block;
};

std::atomic<bool> mainLoopRunning {false};

void* RunGuest(void* argument) {
    const auto start = *static_cast<const GuestStart*>(argument);
    start.entry(start.block, nullptr);
    std::fprintf(stderr, "guest entry returned\n");
    std::abort();
}

void KeepAlive(CFRunLoopTimerRef, void*) {}

}

extern "C" {

// Runs entry(block, nullptr) like the Linux entry stub does, on a thread with the main thread's stack
// size, then serves the main queue forever. The guest ends the process with exit().
[[noreturn]] void Aps5StartGuest_nid_no_patch(void (*entry)(void*, void*), void* block) {
    static GuestStart start;
    start = {entry, block};
    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes) != 0) std::abort();
    const auto stackSize = std::max<std::size_t>(pthread_get_stacksize_np(pthread_self()), 8u << 20);
    if (pthread_attr_setstacksize(&attributes, stackSize) != 0) std::abort();
    // Set before the guest runs: its first main-thread call waits for the run loop below.
    mainLoopRunning.store(true, std::memory_order_release);
    pthread_t thread;
    if (pthread_create(&thread, &attributes, RunGuest, &start) != 0) {
        std::fprintf(stderr, "cannot start the guest thread\n");
        std::abort();
    }
    pthread_attr_destroy(&attributes);
    // A run loop without sources returns at once; the timer keeps it running.
    CFRunLoopTimerRef timer = CFRunLoopTimerCreate(nullptr, 1.0e10, 1.0e10, 0, 0, KeepAlive, nullptr);
    CFRunLoopAddTimer(CFRunLoopGetMain(), timer, kCFRunLoopCommonModes);
    for (;;) CFRunLoopRun();
}

// Runs function(context) on the main thread and waits for it. Without the main loop (a host program
// that never called Aps5StartGuest) the function runs in place.
void Aps5RunOnMainThread_nid_no_patch(void (*function)(void*), void* context) {
    if (pthread_main_np() != 0 || !mainLoopRunning.load(std::memory_order_acquire)) {
        function(context);
        return;
    }
    dispatch_sync_f(dispatch_get_main_queue(), context, function);
}

}

#endif
