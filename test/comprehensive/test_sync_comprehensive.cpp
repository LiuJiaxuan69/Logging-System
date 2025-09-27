#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include "test_framework.hpp"
#include "logger.hpp"
#include "format.hpp"
#include "sink.hpp"

using namespace ljxlog;
namespace fs = std::filesystem;

namespace {
// 线程安全内存落地：收集完整行（含换行）
class VectorSink : public LogSink {
public:
    std::vector<std::string> lines;
    std::mutex mtx;
    void log(const char* d, size_t n) override {
        std::lock_guard<std::mutex> lk(mtx);
        lines.emplace_back(d, n);
    }
};
}

// 同步综合测试：多线程写 SyncLogger（内部加锁串行到各 sink）
// 要求：
// - 每线程固定条数；
// - 每条正文严格 50 字节（不含换行）；
// - 按大小滚动文件：每个文件大小 ≤ 1024；
// - 文件内容与内存落地一致（忽略顺序）。
TEST(sync_logger_comprehensive)
{
    const std::string out_dir = "./Draft/logfile/sync";
    const std::string file_prefix = "sync_comprehensive.log";
    fs::create_directories(out_dir);

    // 清理旧文件
    for (auto& e : fs::directory_iterator(out_dir)) {
        if (e.is_regular_file() && e.path().filename().string().rfind(file_prefix, 0) == 0) {
            fs::remove(e.path());
        }
    }

    // 仅输出正文，Format 实现会自动在末尾追加一个换行，便于按行校验
    auto fmt = std::make_shared<Format>("%m");

    auto mem_sink  = std::make_shared<VectorSink>();
    auto file_sink = std::make_shared<RollBySizeLogSink>((fs::path(out_dir) / file_prefix).string(), 1024);
    std::vector<LogSink::ptr> sinks{mem_sink, file_sink};

    // 同步 logger（内部有互斥，保证一次只写一个完整消息到所有 sink）
    auto lg = std::make_shared<SyncLogger>("sync-comprehensive", fmt, sinks, Level::DEBUG);

    const int num_threads = 4;
    const int logs_per_thread = 300;
    const size_t line_bytes = 50; // 正文长度

    auto make_line = [&](int t, int i)->std::string{
        std::string prefix = "thread" + std::to_string(t) + ":"; // t<10 → 8 字节
        char seq[8]; std::snprintf(seq, sizeof(seq), "%06d", i);  // 6 位序号
        size_t used = prefix.size() + 6;
        if (used > line_bytes) FALL("prefix+seq exceeds target");
        return prefix + std::string(seq, 6) + std::string(line_bytes - used, 'A');
    };

    // 并发写入（同步 logger 会串行到 sink，因此无需担心粘包）
    std::vector<std::thread> ths;
    for (int t = 0; t < num_threads; ++t) {
        ths.emplace_back([&, t]{
            for (int i = 0; i < logs_per_thread; ++i) {
                auto line = make_line(t, i);
                if (line.size() != line_bytes) FALL("line len != 50");
                lg->info(__FILE__, __LINE__, "{}", line);
            }
        });
    }
    for (auto& th : ths) th.join();

    // 为确保文件缓冲刷盘，销毁 logger 与文件 sink 后再读（mem_sink 另持引用即可）
    lg->flush();

    // 内存落地校验
    std::vector<std::string> mem_lines;
    {
        std::lock_guard<std::mutex> lk(mem_sink->mtx);
        mem_lines = mem_sink->lines;
    }
    const size_t expected_total = static_cast<size_t>(num_threads) * logs_per_thread;
    EXPECT_EQ(mem_lines.size(), expected_total);
    for (auto& ln : mem_lines) {
        if (ln.empty() || ln.back() != '\n') FALL("missing newline");
        EXPECT_EQ(ln.size(), line_bytes + 1);
    }
    // 每线程条数
    for (int t = 0; t < num_threads; ++t) {
        std::string prefix = "thread" + std::to_string(t) + ":";
        size_t cnt = 0; for (auto& ln : mem_lines) if (ln.rfind(prefix, 0) == 0) ++cnt;
        EXPECT_EQ(cnt, static_cast<size_t>(logs_per_thread));
    }

    // 文件校验：大小限制与内容一致
    std::vector<std::string> file_lines;
    for (auto& e : fs::directory_iterator(out_dir)) {
        if (!e.is_regular_file()) continue;
        const auto name = e.path().filename().string();
        if (name.rfind(file_prefix, 0) != 0) continue;
        auto sz = fs::file_size(e.path());
        if (sz > 1024) FALL("rolled file exceeds 1024: %s", e.path().c_str());
        std::ifstream ifs(e.path(), std::ios::binary);
        std::string line; while (std::getline(ifs, line)) { EXPECT_EQ(line.size(), line_bytes); file_lines.emplace_back(line); }
    }

    std::vector<std::string> mem_no_nl; mem_no_nl.reserve(mem_lines.size());
    for (auto& s : mem_lines) mem_no_nl.emplace_back(s.begin(), s.end()-1);
    EXPECT_EQ(file_lines.size(), mem_no_nl.size());
    std::sort(file_lines.begin(), file_lines.end());
    std::sort(mem_no_nl.begin(), mem_no_nl.end());
    for (size_t i = 0; i < file_lines.size(); ++i) EXPECT_EQ(file_lines[i], mem_no_nl[i]);
}
