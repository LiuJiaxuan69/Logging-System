#include "ljxlog.hpp"
#include "stopwatch.hpp"
#include "test_framework.hpp"
#include "trash_file_sink.hpp"
#include <thread>

using namespace ljxlog;

namespace
{
    class NullSink : public LogSink
    {
    public:
        void log(const char *, size_t) override {}
        void close() override {}
    };
};

TEST(sync_logger_stress_mt5)
{
    std::shared_ptr<Logger::Builder> builder = std::make_shared<GlobalLoggerBuilder>();
    builder->buildType(LoggerType::LOGGER_SYNC);
    builder->buildFormat("%m");
    builder->buildLimitLevel(Level::DEBUG);
    builder->buildLoggerName("sync_mt5");
    builder->buildSink<TrashFileSink>();
    auto lg = builder->build();
    const int N = 1000000;
    const std::string inf = std::string(50 - 1, 'A'); // -1 because Format adds a '\n' automatically
    const size_t bytes_per_record = inf.size() + 1;
    double elapsed_ms = 0.0;
    {
        ScopedTimer st("sync_mt5", std::cout, &elapsed_ms);
        std::thread threads[5];
        for (int t = 0; t < 5; ++t)
        {
            threads[t] = std::thread([&]() {
                for (int i = 0; i < N; ++i) LOGINFO(lg, "{}", inf);
                lg->flush(); // Ensure all logs are processed before stopping the timer
            });
        }
        for (int t = 0; t < 5; ++t)
            threads[t].join();
    }
    std::cout << "[RESULT] sync logger 5 threads: " << N << " logs of "
              << inf.size() << " bytes in " << elapsed_ms << "ms, "
              << mb_per_sec(N * bytes_per_record, elapsed_ms) << " MB/s\n";
}