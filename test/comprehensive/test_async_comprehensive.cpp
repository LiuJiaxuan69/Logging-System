#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <algorithm>
#include "test_framework.hpp"
#include "logger.hpp"
#include "format.hpp"
#include "sink.hpp"

using namespace ljxlog;

// 目的：验证异步 logger 在高并发下日志不丢失且数据正确
// 方法：启动多个线程，每个线程写入多条日志，等待所有线程结束
//       由于是异步写入，无法精确验证日志顺序，但可以检查总行数与内容正确性

namespace
{
    // vector 缓冲区 sink：将日志行存入 vector 以便检查
    class VectorSink : public LogSink
    {
    public:
        std::vector<std::string> lines;
        std::mutex mtx;
        void log(const char *d, size_t n) override
        {
            // 简单按行拆分并存入 vector
            std::lock_guard<std::mutex> lock(mtx);
            lines.emplace_back(std::string(d, n)); 
        }
    };
}
TEST(async_logger_comprehensive)
{
    // 日志器 1
    auto fmt = std::make_shared<Format>("%m");
    auto sink = std::make_shared<VectorSink>();
    std::vector<LogSink::ptr> sinks{sink};
    // 需先删删除掉该目录下包含 async_comprehensive 的日志，避免旧日志干扰测试
    const std::string out_dir = "./Draft/logfile/async/";
    const std::string file_prefix = "async_comprehensive.log";
    fs::create_directories(out_dir);

    // 清理旧文件，避免干扰
    try
    {
        // 确保目录存在
        fs::create_directories(out_dir);

        // 遍历目录删除匹配文件
        for (const auto &entry : fs::directory_iterator(out_dir))
        {
            if (entry.is_regular_file())
            {
                const std::string filename = entry.path().filename().string();
                if (filename.find(file_prefix) != std::string::npos)
                {
                    std::cout << "Deleted: " << entry.path() << "\n";
                    fs::remove(entry.path());
                }
            }
        }
    }
    catch (const fs::filesystem_error &e)
    {
        std::cerr << "Filesystem error: " << e.what() << "\n";
    }

    sinks.emplace_back(std::make_shared<RollBySizeLogSink>((fs::path(out_dir) / file_prefix).string(), 1024));
    auto lg1 = std::make_shared<AsyncLogger>("async-1", fmt, sinks, Level::DEBUG);
    // 日志器 2 -- 需额外测试低于 INFO 的等级不落地
    auto fmt2 = std::make_shared<Format>("[%d{%F %T}][%p] %m");
    auto lg2 = std::make_shared<AsyncLogger>("async-2", fmt2, sinks, Level::INFO);
    // 多线程写入
    const int num_threads = 5;
    const int logs_per_thread = 200;
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t)
    {
        threads.emplace_back([&, t]()
                             {
            for(int i=0;i<logs_per_thread;++i) {
                lg1->debug(__FILE__, __LINE__, "Logger1 - Thread {} Log {}", t, i);
                lg2->info(__FILE__, __LINE__, "Logger2 - Thread {} Log {}", t, i);
                lg2->debug(__FILE__, __LINE__, "Logger2 - This debug log should not appear");
            } });
    }
    for (auto &th : threads)
        th.join();
    // 等待一小段时间以确保异步日志落地
    lg1->flush();
    lg2->flush();
    // 检查 sink 中的日志行数与内容
    int expected_lines = num_threads * logs_per_thread; // lg1 debug or lg2 info
    // 分别根据 vector 缓冲区 和 roll by size sink 进行检查，需确保两者日志都符合要求且两者日志完全一致
    // 首先检查两个日志器打印的总日志数量是否正确
    // 不能直接简单判断，因为日志是大概率有粘包的，异步日志并不是消费者生产者一起工作的，日志可能会累积，我们应当统计数组中'\n'的数量
    std::vector<std::string> ulti_lines;
    {
        for (const auto &line : sink->lines)
        {
            // 按行拆分
            size_t start = 0;
            size_t end = line.find('\n');
            while (end != std::string::npos)
            {
                std::string log_line = line.substr(start, end - start);
                if (!log_line.empty())
                {
                    ulti_lines.push_back(log_line);
                }
                start = end + 1;
                end = line.find('\n', start);
            }
        }
    }
    EXPECT_EQ(ulti_lines.size(), expected_lines * 2); // lg1 + lg2
    // 检查日志内容正确性
    int count_lg1 = 0, count_lg2 = 0;
    for (const auto &line : sink->lines)
    {
        // 一个语句里面可能既有 lg1 的日志也有 lg2 的日志
        size_t start = 0;
        size_t end = line.find('\n');
        while (end != std::string::npos)
        {
            std::string log_line = line.substr(start, end - start);
            if (log_line.find("Logger1 - Thread") != std::string::npos)
                ++count_lg1;
            else if (log_line.find("Logger2 - Thread") != std::string::npos)
                ++count_lg2;
            start = end + 1;
            end = line.find('\n', start);
        }
    }
    EXPECT_EQ(count_lg1, expected_lines);
    EXPECT_EQ(count_lg2, expected_lines);
    // 读取文件，检查 RollBySizeLogSink 的日志内容(将数据读取到 vector 中，只要内容和 sink->lines 一致即可)
    std::vector<std::string> file_lines;
    try
    {
        for (const auto &entry : fs::directory_iterator(out_dir))
        {
            if (entry.is_regular_file())
            {
                const std::string filename = entry.path().filename().string();
                if (filename.find(file_prefix) != std::string::npos)
                {
                    std::ifstream infile(entry.path());
                    if (!infile.is_open())
                    {
                        FALL("Failed to open log file: %s", entry.path().c_str());
                    }
                    std::string line;
                    while (std::getline(infile, line))
                    {
                        if (!line.empty())
                        {
                            file_lines.push_back(line);
                        }
                    }
                    infile.close();
                }
            }
        }
    }
    catch (const fs::filesystem_error &e)
    {
        FALL("Filesystem error while reading logs: %s", e.what());
    }
    EXPECT_EQ(file_lines.size(), expected_lines * 2);
    // 排序后比较
    std::sort(file_lines.begin(), file_lines.end());
    std::sort(ulti_lines.begin(), ulti_lines.end());
    for (size_t i = 0; i < file_lines.size(); ++i)
    {
        EXPECT_EQ(file_lines[i], ulti_lines[i]);
    }
}