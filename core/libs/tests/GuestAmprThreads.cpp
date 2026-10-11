#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>

struct GuestXtime {
    std::int64_t sec;
    std::int64_t nsec;
};

extern "C" {
int APS5_VABI _Thrd_join_nid_postfix(Pthread thread, int* code);
int APS5_VABI _Thrd_sleep_nid_postfix(const GuestXtime* target, GuestXtime* remaining);
void APS5_VABI _Lock_shared_ptr_spin_lock_nid_postfix(void);
void APS5_VABI _Unlock_shared_ptr_spin_lock_nid_postfix(void);
int APS5_VABI wcstombs_s_nid_postfix(std::size_t* result, char* destination, std::size_t capacity, const std::uint16_t* source, std::size_t limit);
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI pthread_detach_nid_postfix(Pthread thread);
int APS5_VABI clock_gettime_nid_postfix(int clockId, KernelTimespec* tp);
}

static constexpr int ThrdSuccess = 0;
static constexpr int ThrdError = 4;
static constexpr int GuestEinval = 22;
static constexpr int GuestErange = 34;
static constexpr int GuestEilseq = 86;
static constexpr std::int64_t NanosPerSecond = 1000000000;
static constexpr auto Failed = static_cast<std::size_t>(-1);

#define Require(value) RequireAt((value), __LINE__)

static void RequireAt(bool value, int line) {
    if (value) return;
    std::fprintf(stderr, "requirement failed at line %d\n", line);
    std::abort();
}

static std::int64_t RealtimeNanos() {
    KernelTimespec now{};
    Require(clock_gettime_nid_postfix(0, &now) == 0);
    return now.tv_sec * NanosPerSecond + now.tv_nsec;
}

static GuestXtime XtimeAfter(std::int64_t nanos) {
    const auto target = RealtimeNanos() + nanos;
    return {target / NanosPerSecond, target % NanosPerSecond};
}

static void* APS5_VABI ReturnArgument(void* arg) {
    return arg;
}

static void* APS5_VABI WaitForRelease(void* arg) {
    auto* release = static_cast<std::atomic<bool>*>(arg);
    while (!release->load()) std::this_thread::yield();
    return nullptr;
}

struct SpinContext {
    std::atomic<int> inside{0};
    std::atomic<bool> overlapped{false};
    int counter = 0;
};

static constexpr int SpinThreads = 4;
static constexpr int SpinIterations = 20000;

static void* APS5_VABI SpinWorker(void* arg) {
    auto& context = *static_cast<SpinContext*>(arg);
    for (int iteration = 0; iteration < SpinIterations; ++iteration) {
        _Lock_shared_ptr_spin_lock_nid_postfix();
        if (context.inside.fetch_add(1) != 0) context.overlapped = true;
        ++context.counter;
        context.inside.fetch_sub(1);
        _Unlock_shared_ptr_spin_lock_nid_postfix();
    }
    return nullptr;
}

static Pthread Start(PthreadEntry entry, void* arg) {
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, entry, arg, nullptr) == 0);
    return thread;
}

static void TestThreadJoin() {
    int code = 0;
    Require(_Thrd_join_nid_postfix(Start(ReturnArgument, reinterpret_cast<void*>(std::intptr_t{42})), &code) == ThrdSuccess);
    Require(code == 42);
    code = 0;
    Require(_Thrd_join_nid_postfix(Start(ReturnArgument, reinterpret_cast<void*>(std::intptr_t{-7})), &code) == ThrdSuccess);
    Require(code == -7);
    code = 5;
    Require(_Thrd_join_nid_postfix(Start(ReturnArgument, reinterpret_cast<void*>(std::intptr_t{0x100000003})), &code) == ThrdSuccess);
    Require(code == 3);
    Require(_Thrd_join_nid_postfix(Start(ReturnArgument, nullptr), nullptr) == ThrdSuccess);

    static std::atomic<bool> release{false};
    const auto detached = Start(WaitForRelease, &release);
    Require(pthread_detach_nid_postfix(detached) == 0);
    code = 9;
    Require(_Thrd_join_nid_postfix(detached, &code) == ThrdError);
    Require(code == 9);
    release = true;
}

static void TestThreadSleep() {
    const auto target = XtimeAfter(50000000);
    Require(_Thrd_sleep_nid_postfix(&target, nullptr) == 0);
    Require(RealtimeNanos() >= target.sec * NanosPerSecond + target.nsec);

    const auto past = XtimeAfter(-NanosPerSecond);
    const auto before = RealtimeNanos();
    Require(_Thrd_sleep_nid_postfix(&past, nullptr) == 0);
    Require(RealtimeNanos() - before < NanosPerSecond / 2);

    const auto carried = XtimeAfter(30000000);
    const GuestXtime unnormalized{carried.sec - 2, carried.nsec + 2 * NanosPerSecond};
    Require(_Thrd_sleep_nid_postfix(&unnormalized, nullptr) == 0);
    Require(RealtimeNanos() >= carried.sec * NanosPerSecond + carried.nsec);

    const auto borrowed = XtimeAfter(20000000);
    const GuestXtime negative{borrowed.sec + 1, borrowed.nsec - NanosPerSecond};
    Require(_Thrd_sleep_nid_postfix(&negative, nullptr) == 0);
    Require(RealtimeNanos() >= borrowed.sec * NanosPerSecond + borrowed.nsec);

    bool rejected = false;
    try {
        _Thrd_sleep_nid_postfix(nullptr, nullptr);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected);
}

static void TestSpinLock() {
    SpinContext context;
    std::array<Pthread, SpinThreads> threads{};
    for (auto& thread : threads) thread = Start(SpinWorker, &context);
    for (auto thread : threads) Require(_Thrd_join_nid_postfix(thread, nullptr) == ThrdSuccess);
    Require(!context.overlapped);
    Require(context.counter == SpinThreads * SpinIterations);
    _Lock_shared_ptr_spin_lock_nid_postfix();
    _Unlock_shared_ptr_spin_lock_nid_postfix();
}

static void TestWcstombs() {
    const std::uint16_t text[] = {u'a', u'b', u'c', 0};
    const std::uint16_t wide[] = {u'a', 0x100, 0};
    std::size_t result = 0;
    char buffer[8];

    std::memset(buffer, 'x', sizeof(buffer));
    Require(wcstombs_s_nid_postfix(&result, buffer, sizeof(buffer), text, sizeof(buffer)) == 0);
    Require(result == 3 && std::strcmp(buffer, "abc") == 0);

    std::memset(buffer, 'x', sizeof(buffer));
    Require(wcstombs_s_nid_postfix(&result, buffer, sizeof(buffer), text, 2) == 0);
    Require(result == 2 && std::strcmp(buffer, "ab") == 0);

    result = 0;
    Require(wcstombs_s_nid_postfix(&result, nullptr, 0, text, 0) == 0);
    Require(result == 3);

    std::memset(buffer, 'x', sizeof(buffer));
    Require(wcstombs_s_nid_postfix(&result, buffer, 3, text, 3) == GuestErange);
    Require(result == Failed && buffer[0] == '\0');

    result = 0;
    Require(wcstombs_s_nid_postfix(&result, nullptr, 4, text, 4) == GuestEinval);
    Require(result == Failed);

    std::memset(buffer, 'x', sizeof(buffer));
    Require(wcstombs_s_nid_postfix(&result, buffer, sizeof(buffer), nullptr, sizeof(buffer)) == GuestEinval);
    Require(result == Failed && buffer[0] == '\0');

    std::memset(buffer, 'x', sizeof(buffer));
    Require(wcstombs_s_nid_postfix(nullptr, buffer, sizeof(buffer), text, sizeof(buffer)) == GuestEinval);
    Require(buffer[0] == '\0');

    std::memset(buffer, 'x', sizeof(buffer));
    Require(wcstombs_s_nid_postfix(&result, buffer, 0, text, 4) == GuestEinval);
    Require(result == Failed && buffer[0] == 'x');

    std::memset(buffer, 'x', sizeof(buffer));
    Require(wcstombs_s_nid_postfix(&result, buffer, sizeof(buffer), wide, sizeof(buffer)) == GuestEilseq);
    Require(result == Failed && buffer[0] == 'a' && buffer[1] == '\0');

    Require(wcstombs_s_nid_postfix(&result, nullptr, 0, wide, 0) == GuestEilseq);
    Require(result == Failed);
}

int main() {
    TestThreadJoin();
    TestThreadSleep();
    TestSpinLock();
    TestWcstombs();
    return 0;
}
