#pragma once

#include "level.hpp"
#include "util.hpp"
#include <memory>
#include <thread>

namespace log
{
    struct LogMsg {
        size_t _line;//行号
        time_t _time;//时间
        std::thread::id _tid;//线程ID
        std::string _name;//名称
        std::string _file;//文件名
        std::string _payload;//消息
        Level _level;//等级
        LogMsg(const std::string &name, const std::string file, size_t line, const std::string &&payload, 
            Level level): _name(name), _file(file), _payload(std::move(payload)), _level(level),
            _line(line), _time(Date::now()), _tid(std::this_thread::get_id()) {}
    };
};