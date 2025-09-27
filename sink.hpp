#pragma once

#include <memory>
#include <fstream>
#include <cassert>
#include <atomic>
#include "util.hpp"

namespace ljxlog
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
        virtual void close() {};
    private:
        std::atomic<bool> _flushing = false;
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
            std::lock_guard<std::mutex> lock(_file_mtx);
            _ofs.write(data, len);
            assert(_ofs.good());
        }
        void close() override
        {
            std::lock_guard<std::mutex> lock(_file_mtx);
            if(_ofs.is_open()) {
                _ofs.close();
            }
        }
        ~FixedFileLogSink()
        {
            _ofs.close();
        }

    private:
        std::string _filename;
        std::ofstream _ofs;
        std::mutex _file_mtx;
    };

    // 滚动文件落地
    class RollBySizeLogSink : public LogSink
    {
    public:
        RollBySizeLogSink(const std::string &filename, size_t max_size, bool cst_inc = false)
            : _filename(filename),
              _max_size(max_size),
              _cur_size(0),
              _cur_suffix(1),
              _last_time(0),
              _cst_inc(cst_inc)
        {
            if(max_size == 0) throw std::runtime_error("文件大小不能为0");
            File::createDirectory(File::getPath(filename));
        }
        void log(const char *data, size_t len) override
        {
            std::lock_guard<std::mutex> lock(_file_mtx);
            // 如果没有创建文件，则创建一个新文件
            if(!_ofs.is_open())
            {
                std::string new_file_name = newFileName();
                _ofs.open(new_file_name, std::ios::app | std::ios::binary);
                assert(_ofs.is_open());
                _cur_size = 0;
            }
            // 不应该急着直接将日志落地，首先应当检查缓冲区能否完全写进当前日志文件
            size_t cur_len = len;
            bool size_maybe_too_large = false;
            while(_cur_size + cur_len > _max_size) {
                // 首先计算出预截断位置
                size_t pos = _max_size - _cur_size;
                // 查询从文件开始到 pos，最后一个 '\n' 的位置
                size_t last_n_pos = pos;
                while(last_n_pos > 0 && data[last_n_pos - 1] != '\n') --last_n_pos;
                // 若找到了，则将 [0, last_n_pos) 写入当前文件
                if(last_n_pos > 0) {
                    _ofs.write(data, last_n_pos);
                    assert(_ofs.good());
                    // 更新当前文件大小
                    _cur_size += last_n_pos;
                    // 更新 data 指针与 cur_len
                    data += last_n_pos;
                    cur_len -= last_n_pos;
                    if(cur_len == 0) return; // 全部写完，直接返回
                    // 否则需要新建一个文件继续写入剩余数据
                    _ofs.close();
                    std::string new_file_name = newFileName();
                    _ofs.open(new_file_name, std::ios::app | std::ios::binary);
                    assert(_ofs.is_open());
                    _cur_size = 0;
                }
                else {
                    // 如果 size_maybe_too_large 已经是 true，则说明已经尝试过创建新文件了，仍然找不到 '\n'，只能强制截断到 pos 位置
                    if(size_maybe_too_large) {
                        _ofs.write(data, pos);
                        assert(_ofs.good());
                        // 更新当前文件大小
                        _cur_size += pos;
                        // 更新 data 指针与 cur_len
                        data += pos;
                        cur_len -= pos;
                        size_maybe_too_large = false;
                    }
                    else {
                        // 否则可能是因为日志长度太大，超过日志本身大小，但也有可能是当前日志写满了，需要写入下一个日志，故先创建一个新的文件
                        size_maybe_too_large = true;
                    }
                    // 不论如何一定是需要再创建一个新文件的，然后继续尝试写入剩余数据
                    // 新建一个文件继续写入剩余数据
                    _ofs.close();
                    std::string new_file_name = newFileName();
                    _ofs.open(new_file_name, std::ios::app | std::ios::binary);
                    assert(_ofs.is_open());
                    _cur_size = 0;
                }
            }
            // 此时剩余数据可以直接写入当前文件
            _ofs.write(data, cur_len);
            assert(_ofs.good());
            _cur_size += cur_len;
        }
        void close() override {
            std::lock_guard<std::mutex> lock(_file_mtx);
            if(_ofs.is_open()) {
                _ofs.close();
            }
        }
        std::string newFileName()
        {
            time_t t = ljxlog::Date::now();
            struct tm _tm;
            #ifdef _WIN32
            localtime_s(&_tm, &t);
            #else
            localtime_r(&t, &_tm);
            #endif
            char s[128];
            strftime(s, 127, "%Y%m%d%H%M%S", &_tm);
            std::string ret = _filename + s;
            if(!_cst_inc)
            {
                if (_last_time != t) _cur_suffix = 1;
                _last_time = t;
            }
            ret += "-" + std::to_string(_cur_suffix++);
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
        // 超出，若不提前检查，可能会在文件大小超出范围后被检查出来
        bool _cst_inc; //是否让文件后缀不断增加，若不断增加，即便文件名不同，也会继承上次的文件后缀加一作为该文件的后缀，
        //否则每次文件名不同的时候会使用新的后缀（后缀从1开始重新计算）
        // 文件流互斥锁
        std::mutex _file_mtx;
    };

    template <class T, class... Args>
        requires std::is_base_of_v<LogSink, T> // 约束
    inline LogSink::ptr sinkCreate(Args &&...args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
};

// 扩展模块自定义区域
namespace ljxlog
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
            if(time_gap == 0)
            {
                throw std::runtime_error("不能使得文件创建的时间间隔为0");
            }
            File::createDirectory(File::getPath(filename));
        }

        void log(const char *data, size_t len) override
        {
            std::lock_guard<std::mutex> lock(_file_mtx);
            checkStat(Date::now());
            _ofs.write(data, len);
            assert(_ofs.good());
        }
        void close() override {
            std::lock_guard<std::mutex> lock(_file_mtx);
            if(_ofs.is_open()) {
                _ofs.close();
            }
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
            time_t t = ljxlog::Date::now();
            struct tm _tm;
            #ifdef _WIN32
            localtime_s(&_tm, &t);
            #else
            localtime_r(&t, &_tm);
            #endif
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
        // 文件流互斥锁
        std::mutex _file_mtx;
    };
};