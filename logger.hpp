#pragma once

#include <mutex>
#include <format>
#include "format.hpp"
#include "level.hpp"
#include "sink.hpp"
#include "message.hpp"
#include "looper.hpp"

/*
 日志器（Logger）体系结构说明
 ------------------------------------------------------------
 角色：
  - Logger：抽象基类，封装通用的等级过滤、消息格式化、落地管理流程。
  - SyncLogger：同步日志器，调用线程内直接写入 sink。
  - AsyncLogger：异步日志器，组合 AsyncLooper（消息分发器）；logManage 将消息投递给 _looper 统一处理。
  - Builder：建造者，提供按需配置 Logger 的便捷 API。

 数据流：
  debug/info/warning/error/fatal → 早期等级过滤 → log()
  → std::vformat(fmt, args...) 组装正文 → 构造 LogMsg
  → Format::format(...) 应用模式串 → 得到最终字符串
  → logManage(...) 分发到各 Sink（同步或异步）

 用法建议：
  - 通常配合宏传入文件名和行号：
      #define LOG_INFO(logger, fmt, ...) (logger)->info(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
  - Builder 在未设置格式与 sink 的情况下，默认使用 Format 默认模式与 StdOutLogSink。
  - _limit_level 控制最低输出等级：只有 >= limit 的日志才会被输出。

 线程安全：
  - SyncLogger 与 AsyncLogger 均使用 _mtx 保护对 _sinks 的访问；
  - 若 sink 自身不是线程安全的，建议保持加锁范围包含 sink->log 调用。

 异常处理：
  - 文本格式化使用 std::vformat，若 fmt 与参数不匹配会抛出 std::format_error，
    这里降级为输出 "Invalid log format: <fmt>"，避免影响后续日志。
*/

// 建造者模式实现日志器的多种分类管理，并简化用户操作
namespace ljxlog
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
        // logger_name：日志器名称（用于格式化 %c）
        // format     ：格式器（Format），决定日志文本的最终输出格式
        // sinks      ：输出目的地集合（控制台、文件、滚动文件等）
        // limit_level：输出等级下限（小于该等级的日志直接丢弃）
        Logger(const std::string &logger_name, Format::ptr format,
               std::vector<LogSink::ptr> &sinks, Level limit_level = Level::DEBUG)
            : _logger_name(logger_name), _format(format), _sinks(sinks), _limit_level(limit_level) {}
        virtual ~Logger() = default;
    public:
        virtual void flush() = 0;
        template <class... Args>
        // DEBUG 级别输出；filename/line 建议使用 __FILE__/__LINE__ 宏传入
        void debug(const std::string &filename, size_t line, const char *fmt, Args... args)
        {
            // 先检查该等级是否需要落地
            if (Level::DEBUG < _limit_level)
                return;
            log(Level::DEBUG, std::move(filename), line, fmt, args...);
        }
        template <class... Args>
        // INFO 级别输出
        void info(const std::string &filename, size_t line, const char *fmt, Args... args)
        {
            // 先检查该等级是否需要落地
            if (Level::INFO < _limit_level)
                return;
            log(Level::INFO, std::move(filename), line, fmt, args...);
        }
        template <class... Args>
        // WARNING 级别输出
        void warning(const std::string &filename, size_t line, const char *fmt, Args... args)
        {
            // 先检查该等级是否需要落地
            if (Level::WARNING < _limit_level)
                return;
            log(Level::WARNING, std::move(filename), line, fmt, args...);
        }
        template <class... Args>
        // ERROR 级别输出
        void error(const std::string &filename, size_t line, const char *fmt, Args... args)
        {
            // 先检查该等级是否需要落地
            if (Level::ERROR < _limit_level)
                return;
            log(Level::ERROR, std::move(filename), line, fmt, args...);
        }
        template <class... Args>
        // FATAL 级别输出
        void fatal(const std::string &filename, size_t line, const char *fmt, Args... args)
        {
            // 先检查该等级是否需要落地
            if (Level::FATAL < _limit_level)
                return;
            log(Level::FATAL, std::move(filename), line, fmt, args...);
        }

    public:
        // 建造者实现，注：不需要指挥者，因为指挥者主要用于确定建造次序的，这里的日志器实现并不需要次序性
        // 对于次序性不确定的日志器，将构造顺序的权利交给用户是最好的
        class Builder
        {
        public:
            // limit_level：最低输出等级
            Builder(LoggerType type = LoggerType::LOGGER_SYNC, Level limit_level = Level::DEBUG)
                : _limit_level(limit_level)
            {
            }
            virtual ~Builder() = default;
        public:
            // 设置日志器名称（用于 %c）
            void buildLoggerName(const std::string &name)
            {
                _logger_name = name;
            }
            // 设置最低输出等级
            void buildLimitLevel(const Level &level)
            {
                _limit_level = level;
            }
            // 选择日志器类型：同步 / 异步
            void buildType(const LoggerType &type)
            {
                _type = type;
            }
            // 设置格式模式串（例如 "[%d{%F %T}][%p][%c] %m%n"）
            void buildFormat(const std::string format)
            {
                _format = std::make_shared<Format>(format);
            }
            // 直接设置已有 Format 实例
            void buildFormat(const Format::ptr &format)
            {
                _format = format;
            }
            // 添加一个 sink（如 StdOutLogSink、FileLogSink 等），参数由具体 sink 的构造函数决定
            template <class T, class... Args>
            void buildSink(Args &&...args)
            {
                auto sink = sinkCreate<T>(std::forward<Args>(args)...);
                _sinks.push_back(sink);
            }
            // 完成构建：若未设置 _format，则使用默认格式；若未设置 sink，则默认使用 StdOutLogSink
            virtual ptr build() = 0;

        protected:
            std::string _logger_name;
            std::vector<LogSink::ptr> _sinks;
            Format::ptr _format;
            std::atomic<Level> _limit_level;
            LoggerType _type;
            // 异步空间检查策略已固定为强制检查，移除开关
        };

    protected:
        template <class... Args>
        // 统一的日志输出核心：
        // 1) std::vformat 组装正文（可能抛出 std::format_error → 降级为固定提示）
        // 2) 封装为 LogMsg，并交给 Format 生成最终文本
        // 3) 交由具体 Logger 的 logManage 落地
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
        // 由子类实现：决定如何把文本写入多个 sink。
        virtual void logManage(const std::string &msg) = 0;
        std::mutex _mtx;
        std::string _logger_name;
        std::vector<LogSink::ptr> _sinks;
        Format::ptr _format;
        std::atomic<Level> _limit_level;
    };

    // 同步日志器
    class SyncLogger : public Logger
    {
    public:
        SyncLogger(const std::string &logger_name, Format::ptr format,
                   std::vector<LogSink::ptr> &sinks, Level limit_level = Level::DEBUG)
            : Logger(logger_name, format, sinks, limit_level) {}
        ~SyncLogger() override {
            flush();
        }
    public:
        void flush() override {
            // 关闭所有落地方案的文件流做截断
            for(auto &sink: _sinks) {
                sink->close();
            }
        }
    private:
        // 同步写入：在持有 _mtx 的情况下，逐个 sink 写入，调用线程会被阻塞直到写完。
        void logManage(const std::string &msg) override
        {
            std::unique_lock<std::mutex> lock(_mtx);
            if (_sinks.empty())
            {
                return;
            }
            for (auto &sink : _sinks)
                sink->log(msg.c_str(), msg.size());
        }
    };

    // 异步日志器
    class AsyncLogger : public Logger
    {
    public:
        AsyncLogger(const std::string &logger_name, Format::ptr format,
                    std::vector<LogSink::ptr> &sinks, Level limit_level = Level::DEBUG)
            : Logger(logger_name, format, sinks, limit_level),
              _looper(std::make_shared<AsyncLooper>(std::bind(&AsyncLogger::logSink, this, std::placeholders::_1))) {}
        ~AsyncLogger() override {
            flush();
        }
    public:
        void flush() override {
            _looper->flush();
            // 然后关闭所有落地方案的文件流做截断
            for(auto &sink: _sinks) {
                sink->close();
            }
        }
    private:
        // 当前实现：仍是直接写入 sink；
        // 若希望完全异步，应在此将 msg 投递给 _looper，由 logSink(Buffer&) 统一落地。
        void logManage(const std::string &msg) override
        {
            _looper->push(msg);
        }

        // looper 回调：当有 Buffer 可读时，批量写入各 sink。
        void logSink(Buffer &buffer)
        {
            if (_sinks.empty())
            {
                return;
            }
            for (auto &sink : _sinks)
                sink->log(buffer.begin(), buffer.readAbleSize());
        }

    private:
        AsyncLooper::ptr _looper;
    };
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
            return std::make_shared<AsyncLogger>(_logger_name, _format, _sinks, _limit_level);
        }
    };
    class LoggerManager
    {
    public:
        using ptr = std::shared_ptr<LoggerManager>;
        static LoggerManager &getInstance()
        {
            return _lm;
        }
        // 直接返回 root 日志器
        Logger::ptr getRootLogger()
        {
            return _root_logger;
        }
        Logger::ptr getLogger(const std::string &name)
        {
            std::unique_lock<std::mutex> lock(_mtx);
            auto it = _loggers.find(name);
            if (it != _loggers.end())
            {
                return it->second;
            }
            return _root_logger;
        }
        void addLogger(const std::string &name, Logger::ptr logger)
        {
            std::unique_lock<std::mutex> lock(_mtx);
            if (_loggers.find(name) == _loggers.end())
            {
                _loggers[name] = logger;
            }
        }

    private:
        LoggerManager()
        {
            std::unique_ptr<Logger::Builder> builder = std::make_unique<LocalLoggerBuilder>();
            builder->buildLoggerName("root");
            builder->buildType(LoggerType::LOGGER_SYNC);
            _root_logger = builder->build();
            _loggers["root"] = _root_logger;
        }
        ~LoggerManager() = default;
        LoggerManager(const LoggerManager &) = delete;
        LoggerManager &operator=(const LoggerManager &) = delete;
        LoggerManager(LoggerManager &&) = delete;
        LoggerManager &operator=(LoggerManager &&) = delete;

    private:
        std::unordered_map<std::string, Logger::ptr> _loggers;
        std::mutex _mtx;
        Logger::ptr _root_logger;
        static LoggerManager _lm;
    };
    inline LoggerManager LoggerManager::_lm = LoggerManager();

    // 全局日志器建造者，构建全局唯一的日志器，且生命周期覆盖整个进程
    class GlobalLoggerBuilder : public Logger::Builder
    {
    public:
        Logger::ptr build() override
        {
            // 空处理
            if (_logger_name.empty())
            {
                throw std::runtime_error("日志名为空，无法构建全局日志");
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
            Logger::ptr logger;
            if (_type == LoggerType::LOGGER_SYNC)
            {
                logger = std::make_shared<SyncLogger>(_logger_name, _format, _sinks, _limit_level);
            }
            else
            {
                logger = std::make_shared<AsyncLogger>(_logger_name, _format, _sinks, _limit_level);
            }
            // 将 logger 插入到 LoggerManager 的 哈希表中去，交由 LoggerManager 管理从而确保 Logger 被全局管理
            LoggerManager::getInstance().addLogger(_logger_name, logger);
            return logger;
        }
    };
}