#include <fstream>
#include <string>
#include <cstdio>
#include <filesystem>
#include "test_framework.hpp"
#include "logger.hpp"
#include "sink.hpp"
#include "format.hpp"

using namespace ljxlog;
// 目的：验证 FixedFileLogSink 能正确创建文件并写入数据
// 验证点：
// - 目标路径下生成文件
// - 文件大小大于等于预期（内容非空）
namespace fs = std::filesystem;

static std::string tempDir() { return std::string("./Draft/tests_out/"); }

TEST(fixed_file_sink_basic)
{
    fs::create_directories(tempDir());
    std::string path = tempDir() + "fixed_basic.log";
    // Clean existing
    std::error_code ec;
    fs::remove(path, ec);

    {
        auto sink = std::make_shared<FixedFileLogSink>(path);
        const std::string msg = std::string("file sink 7\n");
        sink->log(msg.data(), msg.size());
        // sink closed at end of scope
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    std::error_code ec2;
    auto sz = fs::file_size(path, ec2);
    EXPECT_TRUE(!ec2);
    EXPECT_TRUE(sz >= 10);
}
