#pragma once

#include <iostream>
#include <ctime>
#include <sys/stat.h>
#include <filesystem>

namespace fs = std::filesystem;

namespace log
{
    class Date
    {
    public:
        static time_t now()
        {
            return std::time(nullptr);
        }
    };
    class File
    {
    public:
        static bool exists(const std::string &pathname)
        {
            struct stat st;
            if (stat(pathname.c_str(), &st) < 0)
                return false;
            return true;
        }
        static std::string getPath(const std::string &pathname)
        {
            size_t pos = pathname.find_last_of("/\\");
            if (pos == std::string::npos)
                return ".";
            return pathname.substr(0, pos + 1);
        }
        static void createDirectory(const std::string &pathname)
        {
            //./abc/bcd/cde
            size_t pos = 0, begin = 0;
            while (begin < pathname.size())
            {
                pos = pathname.find_first_of("/\\", begin);
                // 路径即为目录名且目录不存在则创建
                if (pos == std::string::npos)
                {
                    if (!exists(pathname))
                    #ifdef _WIN32
                        fs::create_directory(pathname.c_str());
                    #else
                        mkdir(pathname.c_str(), 0777);
                    #endif
                    return;
                }
                else
                {
                    std::string parent_path = pathname.substr(0, pos);
                    begin = pos + 1;
                    #ifdef _WIN32
                    if (!exists(pathname))
                        fs::create_directory(parent_path.c_str());
                    #else
                        mkdir(parent_path.c_str(), 0777);
                    #endif
                }
            }
        }
    };
};