#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include "test_framework.hpp"
#include "ljxlog.hpp"
#include "logger.hpp"
#include "format.hpp"
#include "sink.hpp"

using namespace ljxlog;

namespace
{
    // 捕获型 sink：将落地的内容累积到字符串，便于断言检查
    class CapturingSink : public LogSink
    {
    public:
        std::string data;
        void log(const char *d, size_t n) override { data.append(d, n); }
    };
}

// 用例：验证带 logger 参数的便捷宏（LOGINFO/LOGDEBUG）
// 目标：
// - 宏会自动注入 __FILE__/__LINE__
// - std::format 风格占位符正常格式化
TEST(logger_param_macros_basic)
{
    std::vector<LogSink::ptr> sinks;
    auto cap = std::make_shared<CapturingSink>();
    sinks.push_back(cap);
    auto fmt = std::make_shared<Format>("%m");
    auto lg = std::make_shared<SyncLogger>("cap_local", fmt, sinks, Level::DEBUG);
    LOGINFO(lg, "hello {}", 42);
    LOGDEBUG(lg, "pi {:.2f}", 3.14159);
    EXPECT_TRUE(cap->data.find("hello 42") != std::string::npos);
    EXPECT_TRUE(cap->data.find("pi 3.14") != std::string::npos);
}
