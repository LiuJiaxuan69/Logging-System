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
// 目的：验证按大小滚动的 sink 在任意时刻切分出的文件大小不超过上限
// 方法：生成很多固定长度日志，触发多次滚动，检查目录中的所有日志文件大小均 <= 上限
namespace fs = std::filesystem;

static std::string dir() { return std::string("./Draft/logfile/size/"); }

static size_t file_size(const std::string &p)
{
    std::error_code ec;
    return fs::file_size(p, ec);
}

TEST(roll_by_size_never_exceed)
{
    fs::create_directories(dir());
    std::string prefix = dir() + std::string("sized.log");

    // Clear previous files
    for (auto &p : fs::directory_iterator(dir()))
    {
        auto name = p.path().filename().string();
        if (name.rfind("sized.log", 0) == 0)
        {
            std::error_code ec;
            fs::remove(p.path(), ec);
        }
    }

    auto lg = ljxlog::init_async_file_logger("size_logger", prefix, 1024, Level::DEBUG, "%m");

    std::string msg(200, 'A'); // 200 bytes + pattern minimal
    for (int i = 0; i < 50; ++i)
    {
        LOGINFO(lg, "{}\n", msg.c_str());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify all created files are <= 1024 bytes
    int counted = 0;
    for (auto &p : fs::directory_iterator(dir()))
    {
        if (p.path().filename().string().rfind("sized.log", 0) == 0)
        {
            size_t sz = file_size(p.path().string());
            EXPECT_TRUE(sz <= 1024);
            counted++;
        }
    }
    EXPECT_TRUE(counted > 0);
}
