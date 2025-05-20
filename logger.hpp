#pragma once

#include "format.hpp"
#include "level.hpp"
#include "sink.hpp"
#include "message.hpp"
#include "looper.hpp"
#include <mutex>
#include <format>

//建造者模式实现日志器的多种分类管理，并简化用户操作
namespace log
{
    enum class LoggerType
    {
        LOGGER_SYNC = 0,
        LOGGER_ASYNC
    };
    class Logger
    {
    public:
        using ptr = std::shared_ptr<Logger>;
        Logger(const std::string &logger_name, Format::ptr format,
               std::vector<LogSink::ptr> &sinks, Level limit_level = Level::DEBUG)
            : _logger_name(logger_name), _format(format), _sinks(sinks), _limit_level(limit_level) {}
        template <class... Args>
        void debug(const std::string &filename, size_t line, const char *fmt, Args... args)
        {
            // 先检查该等级是否需要落地
            if (Level::DEBUG < _limit_level)
                return;
            log(Level::DEBUG, std::move(filename), line, fmt, args...);
        }
        template <class... Args>
        void info(const std::string &filename, size_t line, const char *fmt, Args... args)
        {
            // 先检查该等级是否需要落地
            if (Level::INFO < _limit_level)
                return;
            log(Level::INFO, std::move(filename), line, fmt, args...);
        }
        template <class... Args>
        void warning(const std::string &filename, size_t line, const char *fmt, Args... args)
        {
            // 先检查该等级是否需要落地
            if (Level::WARNING < _limit_level)
                return;
            log(Level::WARNING, std::move(filename), line, fmt, args...);
        }
        template <class... Args>
        void error(const std::string &filename, size_t line, const char *fmt, Args... args)
        {
            // 先检查该等级是否需要落地
            if (Level::ERROR < _limit_level)
                return;
            log(Level::ERROR, std::move(filename), line, fmt, args...);
        }
        template <class... Args>
        void fatal(const std::string &filename, size_t line, const char *fmt, Args... args)
        {
            // 先检查该等级是否需要落地
            if (Level::FATAL < _limit_level)
                return;
            log(Level::FATAL, std::move(filename), line, fmt, args...);
        }

    public:
        //建造者实现，注：不需要指挥者，因为指挥者主要用于确定建造次序的，这里的日志器实现并不需要次序性
        //对于次序性不确定的日志器，将构造顺序的权利交给用户是最好的
        class Builder
        {
        public:
            Builder(Level limit_level = Level::DEBUG, bool check_space = true)
                :_limit_level(limit_level),
                _check_space(check_space)
            {
            }
            void buildLoggerName(const std::string &name)
            {
                _logger_name = name;
            }
            void buildLimitLevel(const Level &level)
            {
                _limit_level = level;
            }
            void buildType(const LoggerType &type)
            {
                _type = type;
            }
            void buildFormat(const std::string format)
            {
                _format = std::make_shared<Format>(format);
            }
            void buildFormat(const Format::ptr &format)
            {
                _format = format;
            }
            template <class T, class... Args>
            void buildSink(Args &&...args)
            {
                auto sink = sinkCreate<T>(std::forward<Args>(args)...);
                _sinks.push_back(sink);
            }
            void buildCheckWay(bool check_space) { _check_space = check_space; }
            virtual ptr build() = 0;

        protected:
            std::string _logger_name;
            std::vector<LogSink::ptr> _sinks;
            Format::ptr _format;
            std::atomic<Level> _limit_level;
            LoggerType _type;
            bool _check_space; //异步日志器使用
        };

    protected:
        template <class... Args>
        void log(Level level, const std::string &filename, size_t line, const char *fmt, Args const... args)
        {
            std::string msg;
            try
            {
                msg = std::vformat(fmt, std::make_format_args(args...));
            }
            catch (const std::format_error &e)
            {
                msg = "Invalid log format: " + std::string(fmt);
            }
            // std::string msg = std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...));
            LogMsg lmsg(_logger_name, filename, line, std::move(msg), level);
            std::stringstream ss;
            _format->format(ss, lmsg);
            logManage(ss.str());
        }
        virtual void logManage(const std::string &msg) = 0;
        std::mutex _mtx;
        std::string _logger_name;
        std::vector<LogSink::ptr> _sinks;
        Format::ptr _format;
        std::atomic<Level> _limit_level;
    };

    //同步日志器
    class SyncLogger : public Logger
    {
    public:
        SyncLogger(const std::string &logger_name, Format::ptr format,
                   std::vector<LogSink::ptr> &sinks, Level limit_level = Level::DEBUG)
            : Logger(logger_name, format, sinks, limit_level) {}

    private:
        void logManage(const std::string &msg) override
        {
            std::unique_lock<std::mutex> lock(_mtx);
            if (_sinks.empty()) { return; }
            for (auto &sink : _sinks)
                sink->log(msg.c_str(), msg.size());
        }
    };

    //异步日志器
    class AsyncLogger : public Logger
    {
    public:
        AsyncLogger(const std::string &logger_name, Format::ptr format,
                   std::vector<LogSink::ptr> &sinks, Level limit_level = Level::DEBUG, bool check_space = true)
            : Logger(logger_name, format, sinks, limit_level),
            _looper(std::make_shared<AsyncLooper>(std::bind(&AsyncLogger::logSink, this, std::placeholders::_1), check_space)){}

    private:
        
        void logManage(const std::string &msg) override
        {
            std::unique_lock<std::mutex> lock(_mtx);
            for (auto &sink : _sinks)
                sink->log(msg.c_str(), msg.size());
        }

        void logSink(Buffer &buffer)
        {
            if (_sinks.empty()) { return; }
            for(auto &sink: _sinks)
                sink->log(buffer.begin(), buffer.readAbleSize());
        }
    private:
        AsyncLooper::ptr _looper;
    };
    // class AsyncLogger : public Logger
    // {
    // public:
    //     void logSink(const std::string &msg) override
    //     {}
    // };
    class LocalLoggerBuilder : public Logger::Builder
    {
    public:
        Logger::ptr build() override
        {
            // 空处理
            if (_logger_name.empty())
            {
                throw std::runtime_error("日志名为空，无法构建本地日志");
            }
            if (_format.get() == nullptr)
            {
                _format = std::make_shared<Format>();
            }
            if (_sinks.empty())
            {
                _sinks.push_back(sinkCreate<StdOutLogSink>());
            }
            // 日志器分类处理
            if (_type == LoggerType::LOGGER_SYNC)
            {
                return std::make_shared<SyncLogger>(_logger_name, _format, _sinks, _limit_level);
            }
            return std::make_shared<AsyncLogger>(_logger_name, _format, _sinks, _limit_level, _check_space);
        }
    };
};