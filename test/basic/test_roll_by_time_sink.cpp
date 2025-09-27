#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>
#include <chrono>
#include "test_framework.hpp"
#include "ljxlog.hpp"
#include "sink.hpp"

using namespace ljxlog;
// 目的：验证按时间间隔滚动的 sink 能在时间到达时创建新文件
// 设定：时间间隔 1 秒，写入三次并在两次写入间 sleep > 1s，预期至少生成 2 个文件
namespace fs = std::filesystem;

static std::string tdir() { return std::string("./Draft/logfile/time/"); }

TEST(roll_by_time_creates_multiple_files)
{
    fs::create_directories(tdir());
    std::string prefix = tdir() + std::string("timed.log");

    // Clean old
    for (auto &p : fs::directory_iterator(tdir()))
    {
        auto name = p.path().filename().string();
        if (name.rfind("timed.log", 0) == 0)
        {
            std::error_code ec;
            fs::remove(p.path(), ec);
        }
    }

    // Build a sync logger with RollByTimeLogSink (short gap)
    std::vector<LogSink::ptr> sinks;
    sinks.push_back(std::make_shared<RollByTimeLogSink>(prefix, size_t(1), false));
    auto fmt = std::make_shared<Format>("%m");
    auto lg = std::make_shared<SyncLogger>("time", fmt, sinks, Level::DEBUG);

    LOGINFO(lg, "a");
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    LOGINFO(lg, "b");
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    LOGINFO(lg, "c");

    int count = 0;
    for (auto &p : fs::directory_iterator(tdir()))
        if (p.path().filename().string().rfind("timed.log", 0) == 0)
            ++count;
    EXPECT_TRUE(count >= 2);
}
