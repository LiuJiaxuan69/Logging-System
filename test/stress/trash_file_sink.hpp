#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <filesystem>
#include <system_error>
#include <cassert>
#include <errno.h>
#include <algorithm>
#ifdef _WIN32
#  include <windows.h>
#  include <io.h>
#  include <fcntl.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#else
#  include <unistd.h>
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <sys/types.h>
#endif
#include "sink.hpp"

namespace ljxlog {

// 固定容量环形文件落地：写同一个文件，循环覆盖，不增长
class TrashFileSink : public LogSink {
public:
    // capacity_bytes: 文件固定容量（默认 16MiB）
    // fsync_every: 每写入多少次后做一次 fdatasync(0=从不)，用于更真实的磁盘测试
    explicit TrashFileSink(size_t capacity_bytes = 16 * 1024 * 1024,
                           uint32_t fsync_every = 0) {
        init_shared_file(capacity_bytes);
        _fsync_every = fsync_every;
    }

    void log(const char* d, size_t n) override {
        if (n == 0 || s_fd.load(std::memory_order_acquire) < 0 || s_cap == 0) return;

        // 若单条超过容量，仅保留最后 cap 字节
        if (n > s_cap) {
            d += (n - s_cap);
            n  = s_cap;
        }

        // 分配写入区间（原子递增，全实例共享）
        const uint64_t start = s_widx.fetch_add(n, std::memory_order_relaxed);
        const size_t   pos   = static_cast<size_t>(start % s_cap);
        const size_t   first = std::min(n, s_cap - pos);

        // 按偏移写入（Windows 使用 WriteFile+OVERLAPPED 实现 pwrite 语义）
        write_all(s_fd.load(std::memory_order_relaxed), d, first, static_cast<off_t>(pos));
        if (first < n) {
            write_all(s_fd.load(std::memory_order_relaxed), d + first, n - first, 0);
        }

        if (_fsync_every) {
            const uint64_t c = s_sync_cnt.fetch_add(1, std::memory_order_relaxed) + 1;
            if (c % _fsync_every == 0) {
                portable_fdatasync(s_fd.load(std::memory_order_relaxed));
            }
        }
    }

    void close() override {
        // 不关闭共享 fd，避免并发测试中的"他人还在写"导致竞态。
        // 如需强制落盘，可在此调用 fdatasync。
        if (_fsync_every) {
            int fd = s_fd.load(std::memory_order_relaxed);
            if (fd >= 0) portable_fdatasync(fd);
        }
    }

private:
    // --- 平台适配：fdatasync/pwrite/ftruncate 封装 ---
#ifdef _WIN32
    static int portable_fdatasync(int fd) {
        HANDLE h = (HANDLE)_get_osfhandle(fd);
        if (h == INVALID_HANDLE_VALUE) { errno = EBADF; return -1; }
        return ::FlushFileBuffers(h) ? 0 : -1;
    }

    static ssize_t portable_pwrite(int fd, const char* buf, size_t n, off_t off) {
        HANDLE h = (HANDLE)_get_osfhandle(fd);
        if (h == INVALID_HANDLE_VALUE) { errno = EBADF; return -1; }
        size_t total = 0;
        while (total < n) {
            OVERLAPPED ov{};
            unsigned long long uoff = static_cast<unsigned long long>(off) + total;
            ov.Offset     = static_cast<DWORD>(uoff & 0xFFFFFFFFull);
            ov.OffsetHigh = static_cast<DWORD>((uoff >> 32) & 0xFFFFFFFFull);
            DWORD to_write = static_cast<DWORD>(std::min<size_t>(n - total, 1u << 20));
            DWORD wrote = 0;
            if (!::WriteFile(h, buf + total, to_write, &wrote, &ov)) {
                errno = EIO;
                return (total > 0) ? static_cast<ssize_t>(total) : -1;
            }
            total += wrote;
            if (wrote == 0) break;
        }
        return static_cast<ssize_t>(total);
    }

    static int portable_ftruncate(int fd, off_t length) {
        HANDLE h = (HANDLE)_get_osfhandle(fd);
        if (h == INVALID_HANDLE_VALUE) { errno = EBADF; return -1; }
        LARGE_INTEGER li{}; li.QuadPart = length;
        if (!::SetFilePointerEx(h, li, nullptr, FILE_BEGIN)) { errno = EIO; return -1; }
        if (!::SetEndOfFile(h)) { errno = EIO; return -1; }
        return 0;
    }
#else
    static int portable_fdatasync(int fd) { return ::fdatasync(fd); }
    static ssize_t portable_pwrite(int fd, const char* buf, size_t n, off_t off) { return ::pwrite(fd, buf, n, off); }
    static int portable_ftruncate(int fd, off_t length) { return ::ftruncate(fd, length); }
#endif

    static void write_all(int fd, const char* buf, size_t n, off_t off) {
        size_t written = 0;
        while (written < n) {
            ssize_t rc = portable_pwrite(fd, buf + written, n - written, off + static_cast<off_t>(written));
            if (rc < 0) {
                if (errno == EINTR) continue;
                // 不抛异常，尽量不影响压测流程
                break;
            }
            if (rc == 0) break;
            written += static_cast<size_t>(rc);
        }
    }

    static void init_shared_file(size_t capacity_bytes) {
        std::call_once(s_once, [&](){
            // 统一垃圾文件路径（无需外部传入）
            const std::string dir  = "./Draft/logfile/stress_trash";
            const std::string path = dir + "/trash.bin";
            std::error_code ec;
            std::filesystem::create_directories(dir, ec); // 忽略错误由 open 报告

            int oflags = O_RDWR | O_CREAT;
#ifdef _WIN32
            oflags |= O_BINARY;
#endif
            int fd = ::open(path.c_str(), oflags, 0644);
            if (fd < 0) {
                s_fd.store(-1, std::memory_order_release);
                return;
            }

            // 固定大小：预分配并截断到 cap
            s_cap = capacity_bytes ? capacity_bytes : (16 * 1024 * 1024);
            if (portable_ftruncate(fd, static_cast<off_t>(s_cap)) != 0) {
                ::close(fd);
                s_fd.store(-1, std::memory_order_release);
                return;
            }

            s_fd.store(fd, std::memory_order_release);
            s_widx.store(0, std::memory_order_release);
            s_sync_cnt.store(0, std::memory_order_release);
        });
    }

private:
    uint32_t _fsync_every{0};

    // 所有实例共享的文件与写入游标
    static inline std::once_flag       s_once;
    static inline std::atomic<int>     s_fd{-1};
    static inline size_t               s_cap{0};
    static inline std::atomic<uint64_t> s_widx{0};
    static inline std::atomic<uint64_t> s_sync_cnt{0};
};

} // namespace ljxlog