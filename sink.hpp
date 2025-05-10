#pragma once

#include "util.hpp"
#include <memory>
#include <fstream>
#include <cassert>

namespace log
{
    // 简单工厂模式，基类
    class LogSink
    {
    public:
        using ptr = std::shared_ptr<LogSink>;

    public:
        LogSink() {};
        virtual ~LogSink() {};
        virtual void log(const char *data, size_t len) = 0;
    };

    // 标准输出落地
    class StdOutLogSink : public LogSink
    {
    public:
        StdOutLogSink() {};
        void log(const char *data, size_t len) override
        {
            std::cout.write(data, len);
        }
    };

    // 指定文件落地
    class FixedFileLogSink : public LogSink
    {
    public:
        FixedFileLogSink(const std::string &filename)
            : _filename(filename)
        {
            File::createDirectory(File::getPath(filename));
            _ofs.open(_filename, std::ios::app | std::ios::binary);
            assert(_ofs.is_open());
        }
        void log(const char *data, size_t len) override
        {
            _ofs.write(data, len);
            assert(_ofs.good());
        }
        ~FixedFileLogSink()
        {
            _ofs.close();
        }

    private:
        std::string _filename;
        std::ofstream _ofs;
    };

    // 滚动文件落地
    class RollBySizeLogSink : public LogSink
    {
    public:
        RollBySizeLogSink(const std::string &filename, size_t max_size, bool prev_check = false)
            : _filename(filename),
              _max_size(max_size),
              _cur_suffix(1),
              _last_time(0),
              _prev_check(prev_check)
        {
            File::createDirectory(File::getPath(filename));
        }
        void log(const char *data, size_t len) override
        {
            checkStat(len);
            _ofs.write(data, len);
            _cur_size += len;
            assert(_ofs.good());
        }
        void checkStat(size_t len)
        {
            // 日志的大小不能超过文件最大容忍大小本身
            if (len > _max_size)
                throw std::runtime_error("The log is too long!");
            // 若继续写文件会导致长度溢出，则需要重新开一个文件
            if (!_ofs.is_open() || !_prev_check && _cur_size + len > _max_size || _prev_check && _cur_size >= _max_size)
            {
                _ofs.close();
                std::string new_file_name = newFileName();
                _ofs.open(new_file_name, std::ios::app | std::ios::binary);
                assert(_ofs.is_open());
                _cur_size = 0;
            }
        }
        std::string newFileName()
        {
            time_t t = log::Date::now();
            struct tm _tm;
            localtime_r(&t, &_tm);
            char s[128];
            strftime(s, 127, "%Y%m%d%H%M%S", &_tm);
            std::string ret = _filename + s;
            if (_last_time != t)
                _cur_suffix = 1;
            ret += "-" + std::to_string(_cur_suffix++);
            _last_time = t;
            return ret;
        }
        ~RollBySizeLogSink()
        {
            _ofs.close();
        }

    private:
        std::string _filename;
        std::ofstream _ofs;
        size_t _max_size;
        size_t _cur_size;
        size_t _cur_suffix;
        size_t _last_time;
        bool _prev_check; // 是否提前检查下一次写入数据后文件大小会不会
        // 超出，若不提前检查，可能会在文件大小超出范围后被检查出来
    };

    template <class T, class... Args>
        requires std::is_base_of_v<LogSink, T> // 约束
    inline LogSink::ptr sinkCreate(Args &&...args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
};

// 扩展模块自定义区域
namespace log
{
    enum class gaptype
    {
        Minute,
        Hour,
        Day,
    };
    // example
    class RollByTimeLogSink : public LogSink
    {
    public:
        explicit RollByTimeLogSink(const std::string &filename, gaptype time_gap, bool is_by_system = false)
            : _filename(filename),
              _last_gap(0),
              _is_by_system(is_by_system),
              _last_time(0)
        {
            File::createDirectory(File::getPath(filename));
            switch (time_gap)
            {
            case gaptype::Minute:
                _time_gap = 60;
                break;
            case gaptype::Hour:
                _time_gap = 3600;
                break;
            case gaptype::Day:
                _time_gap = 3600 * 24;
                break;
            default:
                break;
            }
        }

        explicit RollByTimeLogSink(const std::string &filename, size_t time_gap, bool is_by_system = false)
            : _filename(filename),
              _last_gap(0),
              _time_gap(time_gap),
              _is_by_system(is_by_system),
              _last_time(0)
        {
            File::createDirectory(File::getPath(filename));
        }

        void log(const char *data, size_t len) override
        {
            checkStat(Date::now());
            _ofs.write(data, len);
            assert(_ofs.good());
        }
        void checkStat(time_t t)
        {
            // 若继续写文件会导致长度溢出，则需要重新开一个文件
            bool create_new_file = false;
            if(_is_by_system && _last_gap != t / _time_gap)
            {
                _last_gap = t / _time_gap;
                create_new_file = true;
            }
            else if(!_is_by_system && t - _last_time > _time_gap)
            {
                _last_time = _last_time == 0? t: _last_time + _time_gap;
                create_new_file = true;
            }
            if (!_ofs.is_open() || create_new_file)
            {
                _ofs.close();
                std::string new_file_name = newFileName();
                _ofs.open(new_file_name, std::ios::app | std::ios::binary);
                assert(_ofs.is_open());
            }
        }
        std::string newFileName()
        {
            time_t t = log::Date::now();
            struct tm _tm;
            localtime_r(&t, &_tm);
            char s[128];
            strftime(s, 127, "%Y%m%d%H%M%S", &_tm);
            std::string ret = _filename + s;
            return ret;
        }
        ~RollByTimeLogSink()
        {
            _ofs.close();
        }

    private:
        std::string _filename;
        std::ofstream _ofs;
        size_t _time_gap;
        size_t _last_gap;
        bool _is_by_system; // 是否直接通过系统时间来计算时间间隔，
        // 可能会导致第一时间段的实际时间间隔小于期望时间间隔
        size_t _last_time; // 若不按照系统时间来算，则需要按照时间戳来计算实际时间间隔
    };
}