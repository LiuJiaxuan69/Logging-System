#include <iostream>
#include "format.hpp"
#include "sink.hpp"
using namespace std;

int main()
{
    log::Format fmt("test%%[test1][%T%d%{][%t][%p][%c][%f:%l] %m%n");
    log::LogMsg msg("ljx", "test.cc", 8, "This is just a test", log::Level::INFO);
    std::string inf = fmt.format(msg);
    // log::LogSink::ptr out_sink = log::sinkCreate<log::StdOutLogSink>();
    // log::LogSink::ptr fixed_sink = log::sinkCreate<log::FixedFileLogSink>("./Draft/logfile/test.txt");
    // log::LogSink::ptr rolling_sink = log::sinkCreate<log::RollBySizeLogSink>\
    ("./Draft/logfile/test.txt", 1024 * 1024, true);
    // out_sink->log(inf.c_str(), inf.size());
    // int cnt = 10;
    // while(cnt--)
    // fixed_sink->log(inf.c_str(), inf.size());
    // size_t cur_size = 0;
    // while(cur_size < 1024 * 1024 * 10)
    // {
    //     rolling_sink->log(inf.c_str(), inf.size());
    //     cur_size += inf.size();
    // }
    log::LogSink::ptr roll_by_time = log::sinkCreate<log::RollByTimeLogSink>("./Draft/logfile/test.txt", 20, false);
    while(true)
    {
        roll_by_time->log(inf.c_str(), inf.size());
        sleep(3);
    }

    return 0;
}