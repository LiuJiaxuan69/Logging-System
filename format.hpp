#pragma once

#include "message.hpp"
#include <cassert>
#include <sstream>
#include <tuple>
#include <vector>

/*
 格式化系统总览（Format / FormatItem）
 ------------------------------------------------------------------
 1) 模式串（_pattern）语法
     - 由“普通文本”和“占位符”两部分组成；普通文本原样输出。
     - 占位符以 '%' 开头，后接一个字母键；可选跟随一段子格式参数：%<key>{参数}。
     - 目前只有时间占位符 %d 会消费 {参数}，作为 strftime 的格式串；其它占位符即使写了 {…}
 也会被忽略，不报错。

 2) 支持的占位符（key → 含义）
     - d{fmt}：时间，fmt 为 strftime 格式（默认 %H:%M:%S，若空则回退到默认）。
     - T      ：制表符 \t。
     - t      ：线程ID（LogMsg::_tid）。
     - p      ：日志级别（toString(LogMsg::_level)）。
     - c      ：logger 名称（LogMsg::_name）。
     - f      ：源文件名（LogMsg::_file）。
     - l      ：源码行号（LogMsg::_line）。
     - m      ：日志正文（LogMsg::_payload）。
     - n      ：换行符 \n。

 3) 转义规则
     - %%  → 输出字面量 '%'
     - %{  → 输出字面量 '{'（不会进入“子格式”解析）
     - 右花括号 '}'：在非子格式环境中为普通字符，直接写即可；不支持 %} 作为转义。

 4) 错误与边界
     - 若 '%' 位于模式尾部或其后不是字母（例如 %1、%}），抛出异常："... is not a formatting character"。
     - 若进入子格式块（见 '%x{...'）却未匹配到 '}'，抛出异常："expected '}' after '{'"。
     - 子格式不支持嵌套大括号：遇到的第一个 '}' 即视为结束。

 5) 执行流程
     - 构造 Format 时调用 parsePattern() 将模式串编译为一组 FormatItem；随后每次 format()
 只需顺序输出，避免重复解析开销。

 6) 示例
     - 默认："[%d{%H:%M:%S}][%t][%p][%c][%f:%l] %m%n"
     - 自定义："[%d{%F %T}][%p] %m%n" → "[2025-09-22 14:33:10][INFO] message"。
*/

namespace ljxlog {

class FormatItem {
public:
    using ptr = std::shared_ptr<FormatItem>;

public:
    virtual void format(std::ostream &os, const LogMsg &msg) = 0;
};
class LineFormatItem : public FormatItem {
public:
    // 输出源码行号（LogMsg::_line）。构造参数目前未使用。
    LineFormatItem(const std::string &str) {};
    void format(std::ostream &os, const LogMsg &msg) override { os << msg._line; }
};
class TimeFormatItem : public FormatItem {
public:
    // 输出时间。_format 为 strftime 格式串；若传入为空则回退到默认 "%H:%M:%S"。
    // 支持的常见符号如：%F(YYYY-MM-DD)、%T(HH:MM:SS)、%Y、%m、%d、%H、%M、%S 等。
    TimeFormatItem(const std::string &str = "%H:%M:%S")
            : _format(str) {
        if (_format.empty())
            _format = "%H:%M:%S";
    };
    void format(std::ostream &os, const LogMsg &msg) override {
        time_t t = msg._time;
        struct tm _tm;
#ifdef _WIN32
        localtime_s(&_tm, &t);
#else
        localtime_r(&t, &_tm);
#endif
        char s[128];
        strftime(s, 127, _format.c_str(), &_tm);
        os << s;
    }

private:
    std::string _format;
};
class ThreadFormatItem : public FormatItem {
public:
    // 输出线程ID（LogMsg::_tid）。构造参数目前未使用。
    ThreadFormatItem(const std::string &str) {};
    void format(std::ostream &os, const LogMsg &msg) override { os << msg._tid; }
};
class NameFormatItem : public FormatItem {
public:
    // 输出 logger 名称（LogMsg::_name）。构造参数目前未使用。
    NameFormatItem(const std::string &str) {};
    void format(std::ostream &os, const LogMsg &msg) override { os << msg._name; }
};
class FileFormatItem : public FormatItem {
public:
    // 输出源文件名（LogMsg::_file）。构造参数目前未使用。
    FileFormatItem(const std::string &str) {};
    void format(std::ostream &os, const LogMsg &msg) override { os << msg._file; }
};
class PayLoadFormatItem : public FormatItem {
public:
    // 输出日志正文（LogMsg::_payload）。构造参数目前未使用。
    PayLoadFormatItem(const std::string &str) {};
    void format(std::ostream &os, const LogMsg &msg) override { os << msg._payload; }
};
class LevelFormatItem : public FormatItem {
public:
    // 输出日志级别字符串。构造参数目前未使用。
    LevelFormatItem(const std::string &str) {};
    void format(std::ostream &os, const LogMsg &msg) override { os << toString(msg._level); }
};
class TabFormatItem : public FormatItem {
public:
    // 输出制表符 '\t'。构造参数目前未使用。
    TabFormatItem(const std::string &str) {};
    void format(std::ostream &os, const LogMsg &msg) override { os << '\t'; }
};
class NLineFormatItem : public FormatItem {
public:
    // 输出换行符 '\n'（非平台专用行结束符）。构造参数目前未使用。
    NLineFormatItem(const std::string &str) {};
    void format(std::ostream &os, const LogMsg &msg) override { os << '\n'; }
};
class OtherFormatItem : public FormatItem {
public:
    // 输出普通文本片段（与占位符相对）。
    OtherFormatItem(const std::string &str)
            : message(str) {};
    void format(std::ostream &os, const LogMsg &msg) override { os << message; }

private:
    std::string message;
};
class Format {
public:
    using ptr = std::shared_ptr<Format>;
    Format(const std::string &pattern = "[%d{%H:%M:%S}][%t][%p][%c][%f:%l] %m%n")
            : _pattern(pattern) {
        assert(parsePattern());
    }
    void format(std::ostream &out, LogMsg &msg) {
        // 将预编译好的 _items 逐个写入输出流。
        for (auto &item : _items) { item->format(out, msg); }
        // 最后将 '\n' 也写入流中，确保每条日志独占一行
        out << '\n';
    };
    std::string format(LogMsg msg) {
        std::stringstream ss;
        for (auto &item : _items) { item->format(ss, msg); }
        // 最后将 '\n' 也写入流中，确保每条日志独占一行
        ss << '\n';
        return ss.str();
    }

private:
    bool parsePattern() {
        // 将模式串解析为三元组序列 (key, value, isFormatting)
        // - isFormatting = false：普通文本，key 为空，value 为文本内容
        // - isFormatting = true ：占位符，key 为占位符字母，value 为可选的 {…} 子格式内容
        //   其中 value 就是你在阅读代码时看到的那个变量，用来承接花括号里解析出来的参数字符串。
        int pos = 0;
        std::vector<std::tuple<std::string, std::string, bool>> v;  // true表示是格式化字符，否则为文本内容
        std::string text_inf;                                       // 文本信息
        while (pos < _pattern.size()) {
            // 若不是%，则将该段文本信息全部读完
            if (_pattern[pos] != '%') {
                while (pos < _pattern.size() && _pattern[pos] != '%') text_inf += _pattern[pos++];
                continue;
            }
            // 此时说明是%，则需要先检测%后面是不是还是%，若是，则转义为%
            // 第一种，%后面没有符号了，此时语法有误
            if (++pos == _pattern.size()) {
                throw std::runtime_error("expected a formatting character after '%'");
                return false;
            }
            // 第二种，%后面有符号，此时就需要分类讨论了
            // 1.%后面仍然为%，说明现在仍然在处理文本信息，先把这个%读取了，然后continue
            if (_pattern[pos] == '%' || _pattern[pos] == '{') {
                // '%%' → 输出 '%'；'%{' → 输出 '{'。均作为普通文本追加到 text_inf。
                text_inf += _pattern[pos++];
                continue;
            }
            // 到这来，说明确实没有文本内容了，先检测text_inf是否有内容，有的话就需要push
            if (text_inf.size()) {
                v.push_back({"", text_inf, false});
                text_inf.clear();
            }
            // 2.%后面不再是%，将其视为格式化字符
            // 初检测，如果压根就不是字母，则说明一定出错了
            if (!isalpha(_pattern[pos])) {
                throw std::runtime_error(_pattern.substr(pos - 1, 2) + " is not a formatting character");
                return false;
            }
            std::string key, value;
            key.push_back(_pattern[pos]);
            // 检测后面紧接着的是否是{,若是的则说明还有子格式需要处理
            if (++pos != _pattern.size() && _pattern[pos] == '{') {
                ++pos;
                // 读取到配对的 '}' 为止，将花括号内的内容保存到 value（即子格式参数）
                while (pos != _pattern.size() && _pattern[pos] != '}') value.push_back(_pattern[pos++]);
                // 压根没找到 }，格式有误
                if (pos == _pattern.size()) {
                    throw std::runtime_error("expected '}' after '{'");
                    return false;
                }
                ++pos;
            }
            v.push_back({key, value, true});
        }
        // 将内容映射到_items中
        for (auto &tp : v) {
            if (std::get<2>(tp) == false) {
                _items.push_back(FormatItem::ptr(new OtherFormatItem(std::get<1>(tp))));
            } else {
                std::string key = std::get<0>(tp), value = std::get<1>(tp);
                // 若不是时间元素却拥有value，说明格式有问题(开发阶段问题，利用assert检查即可)
                //  if(key != "d" && value.size())
                //  {
                //      assert(false);
                //      return false;
                //  }
                auto it = createItem(key, value);
                if (it.get() == nullptr) {
                    throw std::runtime_error("%" + key + " is not a formatting character");
                    return false;
                }
                _items.push_back(it);
            }
        }
        return true;
    }
    FormatItem::ptr createItem(const std::string &key, const std::string &value) {
        // 确保 key 是单个字符
        if (key.length() != 1)
            return nullptr;

        switch (key[0]) {
        case 'd':
            return FormatItem::ptr(new TimeFormatItem(value));
        case 'T':
            return FormatItem::ptr(new TabFormatItem(value));
        case 't':
            return FormatItem::ptr(new ThreadFormatItem(value));
        case 'p':
            return FormatItem::ptr(new LevelFormatItem(value));
        case 'c':
            return FormatItem::ptr(new NameFormatItem(value));
        case 'f':
            return FormatItem::ptr(new FileFormatItem(value));
        case 'l':
            return FormatItem::ptr(new LineFormatItem(value));
        case 'm':
            return FormatItem::ptr(new PayLoadFormatItem(value));
        case 'n':
            return FormatItem::ptr(new NLineFormatItem(value));
        default:
            return nullptr;
        }
    }

private:
    std::string _pattern;
    std::vector<FormatItem::ptr> _items;
};
};  // namespace ljxlog