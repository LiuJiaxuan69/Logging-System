#include "message.hpp"
#include <vector>
#include <cassert>
#include <sstream>
#include <tuple>

namespace log
{
    
    class FormatItem
    {
    public:
        using ptr = std::shared_ptr<FormatItem>;
    public:
        virtual void format(std::ostream &os, const LogMsg &msg) = 0;
    };
    class LineFormatItem: public FormatItem
    {
    public:
        LineFormatItem(const std::string &str){};
        void format(std::ostream &os, const LogMsg &msg) override
        {
            os << msg._line;
        }
    };
    class TimeFormatItem: public FormatItem
    {
    public:
        TimeFormatItem(const std::string &str = "%H:%M:%S"):_format(str){
            if(_format.empty()) _format = "%H:%M:%S";
        };
        void format(std::ostream &os, const LogMsg &msg) override
        {
            time_t t = msg._time;
            struct tm _tm;
            localtime_r(&t, &_tm);
            char s[128];
            strftime(s, 127, _format.c_str(), &_tm);
            os << s;
        }
    private:
        std::string _format;
    };
    class ThreadFormatItem: public FormatItem
    {
    public:
    ThreadFormatItem(const std::string &str){};
        void format(std::ostream &os, const LogMsg &msg) override
        {
            os << msg._tid;
        }
    };
    class NameFormatItem: public FormatItem
    {
    public:
    NameFormatItem(const std::string &str){};
        void format(std::ostream &os, const LogMsg &msg) override
        {
            os << msg._name;
        }
    };
    class FileFormatItem: public FormatItem
    {
    public:
    FileFormatItem(const std::string &str){};
        void format(std::ostream &os, const LogMsg &msg) override
        {
            os << msg._file;
        }
    };
    class PayLoadFormatItem: public FormatItem
    {
    public:
    PayLoadFormatItem(const std::string &str){};
        void format(std::ostream &os, const LogMsg &msg) override
        {
            os << msg._payload;
        }
    };
    class LevelFormatItem: public FormatItem
    {
    public:
    LevelFormatItem(const std::string &str){};
        void format(std::ostream &os, const LogMsg &msg) override
        {
            os << toString(msg._level);
        }
    };
    class TabFormatItem: public FormatItem
    {
    public:
    TabFormatItem(const std::string &str){};
        void format(std::ostream &os, const LogMsg &msg) override
        {
            os << '\t';
        }
    };
    class NLineFormatItem: public FormatItem
    {
    public:
    NLineFormatItem(const std::string &str){};
        void format(std::ostream &os, const LogMsg &msg) override
        {
            os << '\n';
        }
    };
    class OtherFormatItem: public FormatItem
    {
    public:
    OtherFormatItem(const std::string &str):message(str){};
        void format(std::ostream &os, const LogMsg &msg) override
        {
            os << message;
        }
    private:
        std::string message;
    };
    class Format
    {
    public:
        Format(const std::string &pattern = "[%d{%H:%M:%S}][%t][%p][%c][%f:%l] %m%n"):_pattern(pattern)
        {
            assert(parsePattern());
        }
        void format(std::ostream &out, LogMsg &msg)
        {
            for(auto &item: _items)
            {
                item->format(out, msg);
            }
        };
        std::string format(LogMsg msg)
        {
            std::stringstream ss;
            for(auto &item: _items)
            {
                item->format(ss, msg);
            }
            return ss.str();
        }
    private:
        bool parsePattern()
        {
            int pos = 0;
            std::vector<std::tuple<std::string, std::string, bool>> v;//true表示是格式化字符，否则为文本内容
            std::string text_inf;//文本信息
            while(pos < _pattern.size())
            {
                //若不是%，则将该段文本信息全部读完
                if(_pattern[pos] != '%')
                {
                    while(pos < _pattern.size() && _pattern[pos] != '%') text_inf += _pattern[pos++];
                    continue;
                }
                //此时说明是%，则需要先检测%后面是不是还是%，若是，则转义为%
                //第一种，%后面没有符号了，此时语法有误
                if(++pos == _pattern.size())
                {
                    throw std::runtime_error("expected a formatting character after '%'");
                    return false;
                }
                //第二种，%后面有符号，此时就需要分类讨论了
                //1.%后面仍然为%，说明现在仍然在处理文本信息，先把这个%读取了，然后continue
                if(_pattern[pos] == '%' || _pattern[pos] == '{')
                {
                    text_inf += _pattern[pos++];
                    continue;
                }
                //到这来，说明确实没有文本内容了，先检测text_inf是否有内容，有的话就需要push
                if(text_inf.size())
                {
                    v.push_back({"", text_inf, false});
                    text_inf.clear();
                }
                //2.%后面不再是%，将其视为格式化字符
                //初检测，如果压根就不是字母，则说明一定出错了
                if(!isalpha(_pattern[pos]))
                {
                    throw std::runtime_error(_pattern.substr(pos - 1, 2) + " is not a formatting character");
                    return false;
                }
                std::string key, value;
                key.push_back(_pattern[pos]);
                //检测后面紧接着的是否是{,若是的则说明还有子格式需要处理
                if(++pos == _pattern.size()) break;
                if(_pattern[pos] == '{')
                {
                    ++pos;
                    while(pos != _pattern.size() && _pattern[pos] != '}') value.push_back(_pattern[pos++]);
                    //压根没找到 }，格式有误
                    if(pos == _pattern.size())
                    {
                        throw std::runtime_error("expected '}' after '%'");
                        return false;
                    }
                    ++pos;
                }
                v.push_back({key, value, true});
            }
            //将内容映射到_items中
            for(auto &tp: v)
            {
                if(std::get<2>(tp) == false)
                {
                    _items.push_back(FormatItem::ptr(new OtherFormatItem(std::get<1>(tp))));
                }
                else
                {
                    std::string key = std::get<0>(tp), value = std::get<1>(tp);
                    //若不是时间元素却拥有value，说明格式有问题(开发阶段问题，利用assert检查即可)
                    // if(key != "d" && value.size())
                    // {
                    //     assert(false);
                    //     return false;
                    // }
                    auto it = createItem(key, value);
                    if(it.get() == nullptr)
                    {
                        throw std::runtime_error("%" + key + " is not a formatting character");
                        return false;
                    }
                    _items.push_back(it);
                }
            }
            return true;
        }
        FormatItem::ptr createItem(const std::string &key, const std::string &value)
        {
            if(key == "d") return FormatItem::ptr(new TimeFormatItem(value));
            if(key == "T") return FormatItem::ptr(new TabFormatItem(value));
            if(key == "t") return FormatItem::ptr(new ThreadFormatItem(value));
            if(key == "p") return FormatItem::ptr(new LevelFormatItem(value));
            if(key == "c") return FormatItem::ptr(new NameFormatItem(value));
            if(key == "f") return FormatItem::ptr(new FileFormatItem(value));
            if(key == "l") return FormatItem::ptr(new LineFormatItem(value));
            if(key == "m") return FormatItem::ptr(new PayLoadFormatItem(value));
            if(key == "n") return FormatItem::ptr(new NLineFormatItem(value));
            return nullptr;
        }
    private:
        std::string _pattern;
        std::vector<FormatItem::ptr> _items;
    };
};