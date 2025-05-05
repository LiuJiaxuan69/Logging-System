#pragma once

#include <iostream>

namespace log
{
    enum class Level
    {
        UNKNOW = 0,
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        FATAL,
        OFF
    };

    inline const char* toString(Level level)
    {
        switch (level)
        {
            case Level::DEBUG:   return "DEBUG";
            case Level::INFO:    return "INFO";
            case Level::WARNING: return "WARNING";
            case Level::ERROR:   return "ERROR";
            case Level::FATAL:   return "FATAL";
            case Level::OFF:     return "OFF";
            default:            return "UNKNOWN";
        }
    }
}