#include "prx/libc/include/GuestWriteWatch.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

#if defined(__linux__)
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/fs.h>
#include <linux/userfaultfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

#if defined(__linux__) && defined(PAGEMAP_SCAN) && defined(UFFD_FEATURE_WP_ASYNC)
#define APS5_WRITE_WATCH 1
#elif defined(__APPLE__)
#define APS5_WRITE_WATCH 1
#endif

namespace GuestWriteWatch {
namespace {

#if defined(__linux__) && defined(PAGEMAP_SCAN) && defined(UFFD_FEATURE_WP_ASYNC)
constexpr std::uintptr_t PageBytes = 4096;

class Watch {
public:
    static Watch& Get() {
        static Watch watch;
        return watch;
    }

    bool Available() const {
        return _pagemap >= 0;
    }

    void Register(std::uintptr_t begin, std::uintptr_t end) {
        if (!Available() || end <= begin) return;
        std::unique_lock lock(_lock);
        remove(_ranges, begin, end);
        remove(_fresh, begin, end);
        uffdio_register registration{};
        registration.range.start = begin;
        registration.range.len = end - begin;
        registration.mode = UFFDIO_REGISTER_MODE_WP;
        if (ioctl(_uffd, UFFDIO_REGISTER, &registration) != 0) {
            static bool reported = false;
            if (!reported) {
                reported = true;
                std::fprintf(stderr, "[memory] write watch: cannot register 0x%llx+0x%llx (%s); the range stays unwatched\n", static_cast<unsigned long long>(begin), static_cast<unsigned long long>(end - begin), std::strerror(errno));
            }
            return;
        }
        insert(_ranges, begin, end);
        insert(_fresh, begin, end);
    }

    bool Unregister(std::uintptr_t begin, std::uintptr_t end) {
        if (!Available() || end <= begin) return false;
        std::unique_lock lock(_lock);
        const bool watched = remove(_ranges, begin, end);
        remove(_fresh, begin, end);
        return watched;
    }

    bool Covers(std::uintptr_t begin, std::uintptr_t end) {
        if (!Available()) return false;
        std::shared_lock lock(_lock);
        return covers(begin, end);
    }

    bool Collect(std::uintptr_t begin, std::uintptr_t end, void (*written)(void*, std::uintptr_t, std::uintptr_t), void* context) {
        if (!Available()) return false;
        begin &= ~(PageBytes - 1);
        end = (end + PageBytes - 1) & ~(PageBytes - 1);
        if (end <= begin) return true;
        std::vector<std::pair<std::uintptr_t, std::uintptr_t>> fresh;
        {
            std::unique_lock lock(_lock);
            if (!covers(begin, end)) return false;
            auto it = _fresh.upper_bound(begin);
            if (it != _fresh.begin()) --it;
            for (; it != _fresh.end() && it->first < end; ++it) {
                const auto from = std::max(it->first, begin);
                const auto to = std::min(it->second, end);
                if (from < to) fresh.emplace_back(from, to);
            }
            if (!fresh.empty()) remove(_fresh, begin, end);
        }
        for (const auto& [from, to] : fresh) {
            written(context, from, to);
            if (protect(from, to)) continue;
            std::unique_lock lock(_lock);
            for (const auto& [left, right] : fresh) {
                if (!covers(left, right)) continue;
                remove(_fresh, left, right);
                insert(_fresh, left, right);
            }
            written(context, begin, end);
            return false;
        }
        std::array<page_region, 256> regions;
        auto cursor = begin;
        while (cursor < end) {
            pm_scan_arg scan{};
            scan.size = sizeof(scan);
            scan.flags = PM_SCAN_WP_MATCHING | PM_SCAN_CHECK_WPASYNC;
            scan.start = cursor;
            scan.end = end;
            scan.vec = reinterpret_cast<std::uintptr_t>(regions.data());
            scan.vec_len = regions.size();
            scan.category_mask = PAGE_IS_WRITTEN;
            scan.return_mask = PAGE_IS_WRITTEN;
            const auto count = ioctl(_pagemap, PAGEMAP_SCAN, &scan);
            if (count < 0) {
                written(context, cursor, end);
                return false;
            }
            for (long i = 0; i < count; ++i) written(context, regions[i].start, regions[i].end);
            if (scan.walk_end <= cursor || scan.walk_end >= end) break;
            cursor = scan.walk_end;
        }
        return true;
    }

private:
    Watch() {
        if (std::getenv("APS5_NO_WRITE_WATCH") == nullptr) open();
    }

    void open() {
        _uffd = static_cast<int>(syscall(SYS_userfaultfd, O_CLOEXEC | O_NONBLOCK));
        if (_uffd < 0 && errno == EPERM) _uffd = static_cast<int>(syscall(SYS_userfaultfd, O_CLOEXEC | O_NONBLOCK | UFFD_USER_MODE_ONLY));
        if (_uffd < 0) return unavailable("userfaultfd");
        uffdio_api api{};
        api.api = UFFD_API;
        api.features = UFFD_FEATURE_WP_ASYNC | UFFD_FEATURE_WP_UNPOPULATED;
        if (ioctl(_uffd, UFFDIO_API, &api) != 0 || (api.features & UFFD_FEATURE_WP_ASYNC) == 0 || (api.features & UFFD_FEATURE_WP_UNPOPULATED) == 0) return unavailable("asynchronous userfaultfd write protection");
        const int pagemap = ::open("/proc/self/pagemap", O_RDONLY | O_CLOEXEC);
        if (pagemap < 0) return unavailable("/proc/self/pagemap");
        if (!probe(pagemap)) {
            close(pagemap);
            return unavailable("PAGEMAP_SCAN");
        }
        _pagemap = pagemap;
    }

    void unavailable(const char* what) {
        const int error = errno;
        if (_uffd >= 0) close(_uffd);
        _uffd = -1;
        std::fprintf(stderr, "[memory] write watch unavailable: %s failed (%s); guest memory is compared instead\n", what, std::strerror(error));
    }

    bool protect(std::uintptr_t begin, std::uintptr_t end) const {
        uffdio_writeprotect protection{};
        protection.range.start = begin;
        protection.range.len = end - begin;
        protection.mode = UFFDIO_WRITEPROTECT_MODE_WP;
        return ioctl(_uffd, UFFDIO_WRITEPROTECT, &protection) == 0;
    }

    bool probe(int pagemap) {
        constexpr std::uintptr_t probeBytes = 4 * PageBytes;
        void* pages = mmap(nullptr, probeBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (pages == MAP_FAILED) return false;
        const auto address = reinterpret_cast<std::uintptr_t>(pages);
        auto* bytes = static_cast<volatile char*>(pages);
        bytes[0] = 1;
        uffdio_register registration{};
        registration.range.start = address;
        registration.range.len = probeBytes;
        registration.mode = UFFDIO_REGISTER_MODE_WP;
        bool working = ioctl(_uffd, UFFDIO_REGISTER, &registration) == 0 && protect(address, address + probeBytes);
        std::array<page_region, 4> regions{};
        const auto scan = [&]() -> long {
            pm_scan_arg arguments{};
            arguments.size = sizeof(arguments);
            arguments.flags = PM_SCAN_WP_MATCHING | PM_SCAN_CHECK_WPASYNC;
            arguments.start = address;
            arguments.end = address + probeBytes;
            arguments.vec = reinterpret_cast<std::uintptr_t>(regions.data());
            arguments.vec_len = regions.size();
            arguments.category_mask = PAGE_IS_WRITTEN;
            arguments.return_mask = PAGE_IS_WRITTEN;
            const auto count = ioctl(pagemap, PAGEMAP_SCAN, &arguments);
            if (count < 0) return -1;
            long pagesWritten = 0;
            for (long i = 0; i < count; ++i) pagesWritten += static_cast<long>((regions[i].end - regions[i].start) / PageBytes);
            return pagesWritten;
        };
        working = working && scan() == 0;
        if (working) {
            bytes[0] = 2;
            bytes[2 * PageBytes] = 1;
            working = scan() == 2 && scan() == 0;
        }
        munmap(pages, probeBytes);
        return working;
    }

    bool covers(std::uintptr_t begin, std::uintptr_t end) const {
        auto next = _ranges.upper_bound(begin);
        if (next == _ranges.begin()) return false;
        return std::prev(next)->second >= end;
    }

    static void insert(std::map<std::uintptr_t, std::uintptr_t>& ranges, std::uintptr_t begin, std::uintptr_t end) {
        auto next = ranges.upper_bound(begin);
        if (next != ranges.begin() && std::prev(next)->second == begin) {
            begin = std::prev(next)->first;
            ranges.erase(std::prev(next));
        }
        if (next != ranges.end() && next->first == end) {
            end = next->second;
            ranges.erase(next);
        }
        ranges.emplace(begin, end);
    }

    static bool remove(std::map<std::uintptr_t, std::uintptr_t>& ranges, std::uintptr_t begin, std::uintptr_t end) {
        bool removed = false;
        auto it = ranges.upper_bound(begin);
        if (it != ranges.begin()) --it;
        while (it != ranges.end() && it->first < end) {
            const auto rangeBegin = it->first;
            const auto rangeEnd = it->second;
            if (rangeEnd <= begin) {
                ++it;
                continue;
            }
            removed = true;
            it = ranges.erase(it);
            if (rangeBegin < begin) ranges.emplace(rangeBegin, begin);
            if (rangeEnd > end) ranges.emplace(end, rangeEnd);
        }
        return removed;
    }

    int _uffd = -1;
    int _pagemap = -1;
    std::shared_mutex _lock;
    std::map<std::uintptr_t, std::uintptr_t> _ranges;
    std::map<std::uintptr_t, std::uintptr_t> _fresh;
};

const bool g_opened = (Watch::Get(), true);
#elif defined(__APPLE__)
constexpr std::uintptr_t PageBytes = 4096;

// macOS has no userfaultfd. A watched page is write-protected after each collect, and the first
// write to it faults into the handler below, which records the page as written and makes it writable
// again. Each page carries its state in one word whose lock bit orders the handler against collects.
class Watch {
public:
    static Watch& Get() {
        static Watch watch;
        return watch;
    }

    bool Available() const {
        return _available;
    }

    void Register(std::uintptr_t begin, std::uintptr_t end) {
        if (!Available() || end <= begin) return;
        std::unique_lock lock(_lock);
        remove(_ranges, begin, end);
        insert(_ranges, begin, end);
        refresh(begin, end, true);
    }

    bool Unregister(std::uintptr_t begin, std::uintptr_t end) {
        if (!Available() || end <= begin) return false;
        std::unique_lock lock(_lock);
        if (!remove(_ranges, begin, end)) return false;
        for (auto page = begin & ~(PageBytes - 1); page < end; page += PageBytes) {
            auto* state = slot(page, false);
            if (state == nullptr) continue;
            const auto held = acquire(*state);
            if (held & Armed) unprotect(page);
            release(*state, coversAny(page, page + PageBytes) ? Registered | Written : 0u);
        }
        return true;
    }

    void Reprotect(std::uintptr_t begin, std::uintptr_t end) {
        if (!Available() || end <= begin) return;
        std::unique_lock lock(_lock);
        auto it = _ranges.upper_bound(begin);
        if (it != _ranges.begin()) --it;
        for (; it != _ranges.end() && it->first < end; ++it) {
            const auto from = std::max(it->first, begin);
            const auto to = std::min(it->second, end);
            if (from < to) refresh(from, to, false);
        }
    }

    void HostWrite(std::uintptr_t begin, std::uintptr_t end) {
        if (!Available() || end <= begin) return;
        std::shared_lock lock(_lock);
        for (auto page = begin & ~(PageBytes - 1); page < end; page += PageBytes) {
            auto* state = slot(page, false);
            if (state == nullptr) continue;
            const auto held = acquire(*state);
            if ((held & Tracked) != 0 && (held & Armed) != 0) unprotect(page);
            release(*state, (held & Tracked) != 0 ? (held & ~Armed) | Written : held);
        }
    }

    bool Covers(std::uintptr_t begin, std::uintptr_t end) {
        if (!Available()) return false;
        std::shared_lock lock(_lock);
        return covers(begin, end);
    }

    bool Writable(std::uintptr_t begin, std::uintptr_t end) {
        if (!Available() || end <= begin) return false;
        for (auto page = begin & ~(PageBytes - 1); page < end; page += PageBytes) {
            auto* state = slot(page, false);
            if (state == nullptr || (state->load(std::memory_order_acquire) & Tracked) == 0) return false;
        }
        return true;
    }

    bool Collect(std::uintptr_t begin, std::uintptr_t end, void (*written)(void*, std::uintptr_t, std::uintptr_t), void* context) {
        if (!Available()) return false;
        begin &= ~(PageBytes - 1);
        end = (end + PageBytes - 1) & ~(PageBytes - 1);
        if (end <= begin) return true;
        std::vector<std::pair<std::uintptr_t, std::uintptr_t>> runs;
        const auto report = [&](std::uintptr_t page) {
            if (!runs.empty() && runs.back().second == page) {
                runs.back().second = page + PageBytes;
            } else {
                runs.emplace_back(page, page + PageBytes);
            }
        };
        {
            std::shared_lock lock(_lock);
            if (!covers(begin, end)) return false;
            for (auto page = begin; page < end; page += PageBytes) {
                auto* state = slot(page, false);
                if (state == nullptr) {
                    report(page);
                    continue;
                }
                auto held = acquire(*state);
                if ((held & Tracked) == 0) {
                    release(*state, held);
                    report(page);
                    continue;
                }
                if ((held & Armed) == 0 && protect(page)) held |= Armed;
                release(*state, (held & Armed) != 0 ? held & ~Written : held);
                if ((held & (Written | Armed)) != Armed) report(page);
            }
        }
        for (const auto& [from, to] : runs) written(context, from, to);
        return true;
    }

    bool HandleFault(std::uintptr_t address) {
        const auto page = address & ~(PageBytes - 1);
        auto* state = slot(page, false);
        if (state == nullptr) return false;
        const auto held = acquire(*state);
        if ((held & Tracked) == 0) {
            release(*state, held);
            return false;
        }
        if ((held & Armed) == 0) {
            vm_prot_t protection = 0;
            Region region;
            const bool writable = query(page, protection, region) && (protection & VM_PROT_WRITE) != 0;
            release(*state, held);
            return writable;
        }
        unprotect(page);
        release(*state, (held & ~Armed) | Written);
        return true;
    }

private:
    static constexpr std::uint32_t Locked = 1u << 0;
    static constexpr std::uint32_t Registered = 1u << 1;
    static constexpr std::uint32_t Tracked = 1u << 2;
    static constexpr std::uint32_t Armed = 1u << 3;
    static constexpr std::uint32_t Written = 1u << 4;
    static constexpr unsigned LeafShift = 30;
    static constexpr std::size_t LeafCount = std::size_t{1} << (47 - LeafShift);
    static constexpr std::size_t LeafPages = (std::size_t{1} << LeafShift) / PageBytes;

    struct Region {
        std::uintptr_t begin = 0;
        std::uintptr_t end = 0;
        vm_prot_t protection = 0;
    };

    Watch() {
        if (std::getenv("APS5_NO_WRITE_WATCH") != nullptr) return;
        _leaves = new std::atomic<std::atomic<std::uint8_t>*>[LeafCount]();
        install(SIGBUS, _previousBus);
        install(SIGSEGV, _previousSegv);
        _available = true;
    }

    static void install(int signal, struct sigaction& previous) {
        struct sigaction action{};
        action.sa_sigaction = &onFault;
        action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER;
        sigemptyset(&action.sa_mask);
        sigaction(signal, &action, &previous);
    }

    static void onFault(int signal, siginfo_t* info, void* context) {
        auto& watch = Get();
        if (watch._available && watch.HandleFault(reinterpret_cast<std::uintptr_t>(info->si_addr))) return;
        const auto& previous = signal == SIGBUS ? watch._previousBus : watch._previousSegv;
        if ((previous.sa_flags & SA_SIGINFO) != 0 && previous.sa_sigaction != nullptr) {
            previous.sa_sigaction(signal, info, context);
            return;
        }
        if (previous.sa_handler != SIG_DFL && previous.sa_handler != SIG_IGN && previous.sa_handler != nullptr) {
            previous.sa_handler(signal);
            return;
        }
        sigaction(signal, &previous, nullptr);
    }

    // Leaves are made by Register and never freed, so the fault handler reads them without a lock.
    std::atomic<std::uint8_t>* slot(std::uintptr_t page, bool create) {
        const auto leaf = page >> LeafShift;
        if (leaf >= LeafCount) return nullptr;
        auto* entries = _leaves[leaf].load(std::memory_order_acquire);
        if (entries == nullptr) {
            if (!create) return nullptr;
            entries = new std::atomic<std::uint8_t>[LeafPages]();
            _leaves[leaf].store(entries, std::memory_order_release);
        }
        return &entries[(page >> 12) & (LeafPages - 1)];
    }

    static std::uint8_t acquire(std::atomic<std::uint8_t>& state) {
        auto value = state.load(std::memory_order_relaxed);
        for (;;) {
            if ((value & Locked) == 0 && state.compare_exchange_weak(value, static_cast<std::uint8_t>(value | Locked), std::memory_order_acquire, std::memory_order_relaxed)) return value;
            if (value & Locked) value = state.load(std::memory_order_relaxed);
        }
    }

    static void release(std::atomic<std::uint8_t>& state, std::uint32_t value) {
        state.store(static_cast<std::uint8_t>(value & ~Locked), std::memory_order_release);
    }

    static bool protect(std::uintptr_t page) {
        return mprotect(reinterpret_cast<void*>(page), PageBytes, PROT_READ) == 0;
    }

    static void unprotect(std::uintptr_t page) {
        mprotect(reinterpret_cast<void*>(page), PageBytes, PROT_READ | PROT_WRITE);
    }

    // Tracks the pages of [begin, end) that are whole inside a watched range and readable and writable
    // without execute (or protected by the watch itself); the others are reported by every collect.
    void refresh(std::uintptr_t begin, std::uintptr_t end, bool registering) {
        Region region;
        for (auto page = begin & ~(PageBytes - 1); page < end; page += PageBytes) {
            auto* state = slot(page, registering);
            if (state == nullptr) continue;
            const auto held = acquire(*state);
            vm_prot_t protection = 0;
            const bool trackable = covers(page, page + PageBytes) && query(page, protection, region) && (protection == (VM_PROT_READ | VM_PROT_WRITE) || ((held & Armed) != 0 && protection == VM_PROT_READ));
            if (!trackable && (held & Armed) != 0) unprotect(page);
            release(*state, trackable ? Registered | Tracked | Written | (held & Armed) : Registered | Written);
        }
    }

    static bool query(std::uintptr_t page, vm_prot_t& protection, Region& region) {
        if (page >= region.begin && page + PageBytes <= region.end) {
            protection = region.protection;
            return true;
        }
        mach_vm_address_t address = page;
        mach_vm_size_t size = 0;
        vm_region_basic_info_data_64_t info{};
        mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t object = MACH_PORT_NULL;
        if (mach_vm_region(mach_task_self(), &address, &size, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &count, &object) != KERN_SUCCESS) return false;
        if (address > page || address + size < page + PageBytes) return false;
        region = {static_cast<std::uintptr_t>(address), static_cast<std::uintptr_t>(address + size), info.protection};
        protection = info.protection;
        return true;
    }

    bool covers(std::uintptr_t begin, std::uintptr_t end) const {
        auto next = _ranges.upper_bound(begin);
        if (next == _ranges.begin()) return false;
        return std::prev(next)->second >= end;
    }

    bool coversAny(std::uintptr_t begin, std::uintptr_t end) const {
        auto it = _ranges.upper_bound(begin);
        if (it != _ranges.begin() && std::prev(it)->second > begin) return true;
        return it != _ranges.end() && it->first < end;
    }

    static void insert(std::map<std::uintptr_t, std::uintptr_t>& ranges, std::uintptr_t begin, std::uintptr_t end) {
        auto next = ranges.upper_bound(begin);
        if (next != ranges.begin() && std::prev(next)->second == begin) {
            begin = std::prev(next)->first;
            ranges.erase(std::prev(next));
        }
        if (next != ranges.end() && next->first == end) {
            end = next->second;
            ranges.erase(next);
        }
        ranges.emplace(begin, end);
    }

    static bool remove(std::map<std::uintptr_t, std::uintptr_t>& ranges, std::uintptr_t begin, std::uintptr_t end) {
        bool removed = false;
        auto it = ranges.upper_bound(begin);
        if (it != ranges.begin()) --it;
        while (it != ranges.end() && it->first < end) {
            const auto rangeBegin = it->first;
            const auto rangeEnd = it->second;
            if (rangeEnd <= begin) {
                ++it;
                continue;
            }
            removed = true;
            it = ranges.erase(it);
            if (rangeBegin < begin) ranges.emplace(rangeBegin, begin);
            if (rangeEnd > end) ranges.emplace(end, rangeEnd);
        }
        return removed;
    }

    bool _available = false;
    std::atomic<std::atomic<std::uint8_t>*>* _leaves = nullptr;
    struct sigaction _previousBus{};
    struct sigaction _previousSegv{};
    std::shared_mutex _lock;
    std::map<std::uintptr_t, std::uintptr_t> _ranges;
};

const bool g_opened = (Watch::Get(), true);
#endif

}

bool GuestWriteWatchAvailable_nid_postfix() {
#ifdef APS5_WRITE_WATCH
    return Watch::Get().Available();
#else
    return false;
#endif
}

void GuestWriteWatchRegister_nid_postfix(const void* pointer, std::size_t bytes) {
#ifdef APS5_WRITE_WATCH
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    Watch::Get().Register(begin, begin + bytes);
#else
    static_cast<void>(pointer);
    static_cast<void>(bytes);
#endif
}

bool GuestWriteWatchUnregister_nid_postfix(const void* pointer, std::size_t bytes) {
#ifdef APS5_WRITE_WATCH
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    return Watch::Get().Unregister(begin, begin + bytes);
#else
    static_cast<void>(pointer);
    static_cast<void>(bytes);
    return false;
#endif
}

bool GuestWriteWatchCovers_nid_postfix(std::uintptr_t address, std::size_t bytes) {
#ifdef APS5_WRITE_WATCH
    return bytes != 0 && address + bytes > address && Watch::Get().Covers(address, address + bytes);
#else
    static_cast<void>(address);
    static_cast<void>(bytes);
    return false;
#endif
}

bool GuestWriteWatchCollect_nid_postfix(std::uintptr_t address, std::size_t bytes, void (*written)(void* context, std::uintptr_t begin, std::uintptr_t end), void* context) {
#ifdef APS5_WRITE_WATCH
    if (bytes == 0 || address + bytes < address) return false;
    return Watch::Get().Collect(address, address + bytes, written, context);
#else
    static_cast<void>(address);
    static_cast<void>(bytes);
    static_cast<void>(written);
    static_cast<void>(context);
    return false;
#endif
}

void GuestWriteWatchReprotect_nid_postfix(const void* pointer, std::size_t bytes) {
#if defined(__APPLE__)
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    Watch::Get().Reprotect(begin, begin + bytes);
#else
    static_cast<void>(pointer);
    static_cast<void>(bytes);
#endif
}

void GuestWriteWatchHostWrite_nid_postfix(const void* pointer, std::size_t bytes) {
#if defined(__APPLE__)
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    Watch::Get().HostWrite(begin, begin + bytes);
#else
    static_cast<void>(pointer);
    static_cast<void>(bytes);
#endif
}

bool GuestWriteWatchWritable_nid_postfix(std::uintptr_t address, std::size_t bytes) {
#if defined(__APPLE__)
    return bytes != 0 && address + bytes > address && Watch::Get().Writable(address, address + bytes);
#else
    static_cast<void>(address);
    static_cast<void>(bytes);
    return false;
#endif
}

}
