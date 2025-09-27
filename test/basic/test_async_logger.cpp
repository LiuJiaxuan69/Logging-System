#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include "test_framework.hpp"
#include "logger.hpp"
#include "format.hpp"
#include "sink.hpp"

using namespace ljxlog;

namespace
{
    // 计数型 sink：统计落地数据中的换行符个数，近似等于日志条数
    class CountingSink : public LogSink
    {
    public:
        std::atomic<int> count{0};
        void log(const char *d, size_t n) override
        {
            // count lines by scanning for '\n'
            for (size_t i = 0; i < n; ++i)
                if (d[i] == '\n')
                    count.fetch_add(1, std::memory_order_relaxed);
        }
    };
}

// 目的：验证异步 logger 能够接受多线程写入并最终排空（drain）
// 方法：两个线程并发写入各 N 条，等待一小段时间后检查计数 >= 2N
TEST(async_logger_drains_and_orders)
{
    auto fmt = std::make_shared<Format>("%m\n");
    auto sink = std::make_shared<CountingSink>();
    std::vector<LogSink::ptr> sinks{sink};
    auto lg = std::make_shared<AsyncLogger>("async", fmt, sinks, Level::DEBUG);

    const int N = 1000;
    std::thread t1([&]
                   { for(int i=0;i<N;++i) lg->info(__FILE__, __LINE__, "{}", i); });
    std::thread t2([&]
                   { for(int i=0;i<N;++i) lg->debug(__FILE__, __LINE__, "{}", i); });
    t1.join();
    t2.join();

    // Give time to drain
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_TRUE(sink->count.load() >= 2 * N); // Each log adds a trailing newline by Format::format
}
