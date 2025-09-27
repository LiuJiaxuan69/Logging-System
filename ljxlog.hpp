#pragma once
#include "logger.hpp"

namespace ljxlog {
    // 默认 logger 名称（用于宏查找）
    inline std::string& default_logger_name()
    {
        static std::string name = "default";
        return name;
    }
    // 返回 root logger
    inline Logger::ptr get_root_logger() {
        return LoggerManager::getInstance().getRootLogger();
    }
    // 根据名字返回对应 logger，若不存在则返回 root logger
    inline Logger::ptr get_logger(const std::string& name) {
        return LoggerManager::getInstance().getLogger(name);
    }
    // 返回 default logger
    inline Logger::ptr get_default_logger() {
        return get_logger(default_logger_name());
    }
    // 一行初始化：异步 + 按大小滚动的文件 logger
    inline Logger::ptr init_async_file_logger(const std::string& name,
                                              const std::string& file_path,
                                              size_t max_size_bytes,
                                              Level min_level = Level::DEBUG,
                                              const std::string& pattern = "[%d{%H:%M:%S}][%t][%p][%c][%f:%l] %m%n")
    {
        std::unique_ptr<Logger::Builder> builder = std::make_unique<GlobalLoggerBuilder>();
        builder->buildLoggerName(name);
        builder->buildLimitLevel(min_level);
        builder->buildType(LoggerType::LOGGER_ASYNC);
        builder->buildSink<RollBySizeLogSink>(file_path, max_size_bytes);
        builder->buildFormat(pattern);
        auto lg = builder->build(); // 一般已注册到 LoggerManager
        default_logger_name() = name;
        return lg;
    }

    // 一行初始化：同步控制台 logger
    inline Logger::ptr init_stdout_logger(const std::string& name = "default",
                                          Level min_level = Level::DEBUG,
                                          const std::string& pattern = "[%d{%H:%M:%S}][%p] %m%n")
    {
        std::unique_ptr<Logger::Builder> builder = std::make_unique<GlobalLoggerBuilder>();
        builder->buildLoggerName(name);
        builder->buildLimitLevel(min_level);
        builder->buildType(LoggerType::LOGGER_SYNC);
        builder->buildSink<StdOutLogSink>();
        builder->buildFormat(pattern);
        auto lg = builder->build();
        default_logger_name() = name;
        return lg;
    }
};

// 便捷宏：通过指定的 logger 进行日志落地（自动携带 __FILE__/__LINE__，并进行空指针保护）
#define LOGDEBUG(logger, fmt, ...)   do { auto __lg = (logger); if (__lg) __lg->debug  (__FILE__, __LINE__, fmt, ##__VA_ARGS__); } while (0)
#define LOGINFO(logger, fmt, ...)    do { auto __lg = (logger); if (__lg) __lg->info   (__FILE__, __LINE__, fmt, ##__VA_ARGS__); } while (0)
#define LOGWARNING(logger, fmt, ...) do { auto __lg = (logger); if (__lg) __lg->warning(__FILE__, __LINE__, fmt, ##__VA_ARGS__); } while (0)
#define LOGERROR(logger, fmt, ...)   do { auto __lg = (logger); if (__lg) __lg->error  (__FILE__, __LINE__, fmt, ##__VA_ARGS__); } while (0)
#define LOGFATAL(logger, fmt, ...)   do { auto __lg = (logger); if (__lg) __lg->fatal  (__FILE__, __LINE__, fmt, ##__VA_ARGS__); } while (0)

// 便捷宏：通过默认 logger 进行日志落地（自动携带 __FILE__/__LINE__，并进行空指针保护）
#define DEBUG(fmt, ...)   do { auto __lg = ljxlog::get_default_logger(); if (__lg) __lg->debug  (__FILE__, __LINE__, fmt, ##__VA_ARGS__); } while (0)
#define INFO(fmt, ...)    do { auto __lg = ljxlog::get_default_logger(); if (__lg) __lg->info   (__FILE__, __LINE__, fmt, ##__VA_ARGS__); } while (0)
#define WARNING(fmt, ...) do { auto __lg = ljxlog::get_default_logger(); if (__lg) __lg->warning(__FILE__, __LINE__, fmt, ##__VA_ARGS__); } while (0)
#define ERROR(fmt, ...)   do { auto __lg = ljxlog::get_default_logger(); if (__lg) __lg->error  (__FILE__, __LINE__, fmt, ##__VA_ARGS__); } while (0)
#define FATAL(fmt, ...)   do { auto __lg = ljxlog::get_default_logger(); if (__lg) __lg->fatal  (__FILE__, __LINE__, fmt, ##__VA_ARGS__); } while (0)