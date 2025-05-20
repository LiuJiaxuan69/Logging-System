#include <iostream>
#include <chrono>
#include "format.hpp"
#include "sink.hpp"
#include "logger.hpp"
#include "test_util/file_cmp.hpp"
#include "buffer.hpp"
using namespace std;
using namespace this_thread;
using namespace chrono_literals;

int main()
{
    // std::ifstream ifs("./Draft/logfile/test.txt", std::ios::binary);
    // ifs.seekg(0, std::ios::end);
    // size_t fsize = ifs.tellg();
    // ifs.seekg(std::ios::beg);
    // string body;
    // body.resize(fsize);
    // ifs.read(&body.front(), fsize);
    // ifs.close();
    // log::Buffer buffer;
    // for(int i = 0; i < fsize; ++i)
    // {
    //     buffer.push(body.data() + i, 1);
    // }
    // std::ofstream ofs("./Draft/logfile/tmp.txt", std::ios::binary); 
    // while(buffer.readAbleSize())
    // {
    //     ofs.write(buffer.begin(), 1);
    //     buffer.pop(1);
    // }
    // ofs.close();
    // std::ifstream ifs1("./Draft/logfile/test.txt", std::ios::binary);
    // std::ifstream ifs2("./Draft/logfile/tmp.txt", std::ios::binary);
    // log::test_util::FileCmp::file_cmp(ifs1, ifs2);
    // log::Format::ptr fmt(new log::Format("test%%[test1][%T%d%{][%t][%p][%c][%f:%l] %m%n"));
    // log::LogMsg msg("ljx", "test.cc", 8, "This is just a test", log::Level::INFO);
    // log::LogSink::ptr out_sink = log::sinkCreate<log::StdOutLogSink>();
    // log::LogSink::ptr fixed_sink = log::sinkCreate<log::FixedFileLogSink>("./Draft/logfile/test.txt");
    // log::LogSink::ptr rolling_sink = log::sinkCreate<log::RollBySizeLogSink>\
    // ("./Draft/logfile/test.txt", 1024 * 1024, true);
    // log::LogSink::ptr roll_by_time = log::sinkCreate<log::RollByTimeLogSink>("./Draft/logfile/test.txt", 20, false);
    // vector<log::LogSink::ptr> sinks{out_sink, fixed_sink, rolling_sink, roll_by_time};
    // // vector<log::LogSink::ptr> sinks{out_sink};
    // log::SyncLogger synlogger("synlogger", fmt, sinks, log::Level::DEBUG);

    unique_ptr<log::LocalLoggerBuilder> builder(new log::LocalLoggerBuilder());
    builder->buildLimitLevel(log::Level::DEBUG);
    builder->buildLoggerName("root");
    builder->buildType(log::LoggerType::LOGGER_ASYNC);
    builder->buildSink<log::StdOutLogSink>();
    builder->buildSink<log::FixedFileLogSink>("./Draft/logfile/test.txt");
    builder->buildSink<log::RollBySizeLogSink>("./Draft/logfile/test.txt", 1024 * 1024, true, false);
    builder->buildSink<log::RollByTimeLogSink>("./Draft/logfile/test.txt", 20, false);
    builder->buildFormat("test%%[test1][%T%d%{][%t][%p][%c][%f:%l] %m%n");
    log::Logger::ptr logger = builder->build();

    // logger->debug(__FILE__, __LINE__,"{}{}", "测试日志", 123);
    // logger->info(__FILE__, __LINE__,"{}{}", "测试日志", 123);
    // logger->warning(__FILE__, __LINE__,"{}{}", "测试日志", 123);
    // logger->error(__FILE__, __LINE__,"{}{}", "测试日志", 123);
    // logger->fatal(__FILE__, __LINE__,"{}{}", "测试日志", 123);

    size_t cnt = 10000;
    while(cnt--)
    logger->fatal(__FILE__, __LINE__,"{}{}{}", cnt, ":测试日志", 123);
    return 0;
}