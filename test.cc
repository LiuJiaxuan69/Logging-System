#include <iostream>
#include <chrono>
#include "format.hpp"
#include "sink.hpp"
#include "logger.hpp"
using namespace std;
using namespace this_thread;
using namespace chrono_literals;

int main()
{
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
    builder->buildType(log::LoggerType::LOGGER_SYNC);
    builder->buildSink<log::StdOutLogSink>();
    builder->buildSink<log::FixedFileLogSink>("./Draft/logfile/test.txt");
    builder->buildSink<log::RollBySizeLogSink>("./Draft/logfile/test.txt", 1024 * 1024, true, true);
    builder->buildSink<log::RollByTimeLogSink>("./Draft/logfile/test.txt", 20, false);
    builder->buildFormat("test%%[test1][%T%d%{][%t][%p][%c][%f:%l] %m%n");
    log::Logger::ptr synlogger = builder->build();

    synlogger->debug(__FILE__, __LINE__,"{}{}", "测试日志", 123);
    synlogger->info(__FILE__, __LINE__,"{}{}", "测试日志", 123);
    synlogger->warning(__FILE__, __LINE__,"{}{}", "测试日志", 123);
    synlogger->error(__FILE__, __LINE__,"{}{}", "测试日志", 123);
    synlogger->fatal(__FILE__, __LINE__,"{}{}", "测试日志", 123);

    size_t cnt = 10000;
    while(cnt--)
    synlogger->fatal(__FILE__, __LINE__,"{}{}", "测试日志", 123);
    return 0;
}