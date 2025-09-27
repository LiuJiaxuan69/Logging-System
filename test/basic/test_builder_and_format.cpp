#include <string>
#include <vector>
#include <sstream>
#include "test_framework.hpp"
#include "logger.hpp"
#include "format.hpp"
#include "sink.hpp"

using namespace ljxlog;

namespace
{
    // 捕获型 sink：用于收集格式化输出以便断言
    class CaptureSink : public LogSink
    {
    public:
        std::string data;
        void log(const char *d, size_t n) override { data.append(d, n); }
    };
}

// 目的：验证 LocalLoggerBuilder 在未显式设置 format/sink 时能够补齐默认值并成功构建
TEST(builder_default_and_format_items)
{
    // Default builder fills missing parts
    std::unique_ptr<Logger::Builder> b = std::make_unique<LocalLoggerBuilder>();
    b->buildLoggerName("b1");
    auto lg = b->build();
    EXPECT_TRUE(lg != nullptr);
}

// 目的：验证格式占位符输出与转义规则
// 关注点：
// - [%p][%c][%f:%l]
// - '%%' → '%', '%{' → '{'
// - 正文参数化
TEST(format_placeholders_and_escape)
{
    auto fmt = std::make_shared<Format>("[%p][%c][%f:%l] %% %{ %m");
    std::vector<LogSink::ptr> sinks;
    auto cap = std::make_shared<CaptureSink>();
    sinks.push_back(cap);
    SyncLogger lg("nm", fmt, sinks, Level::DEBUG);
    lg.info(__FILE__, __LINE__, "msg {}", 3);
    EXPECT_TRUE(cap->data.find("[INFO][nm]") != std::string::npos);
    EXPECT_TRUE(cap->data.find("% {") != std::string::npos);
    EXPECT_TRUE(cap->data.find("msg 3") != std::string::npos);
}
