#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>

namespace log::test_util
{
    const size_t BUFFER_SIZE = 4096; // 4KB buffer
    // 包装成类，防止不必要函数暴露
    class FileCmp
    {
    public:
        static void file_cmp(std::ifstream &ifs1, std::ifstream &ifs2)
        {
            ifs1.seekg(0, std::ios::end);
            ifs2.seekg(0, std::ios::end);
            size_t len1 = ifs1.tellg();
            size_t len2 = ifs2.tellg();
            if (len1 != len2)
            {
                std::cout << "Warning: The size of the two files is different\n";
                return;
            }
            ifs1.seekg(0);
            ifs2.seekg(0);
            size_t current_pos = 0;
            while (current_pos < len1)
            {
                size_t remaining = len1 - current_pos;
                size_t chunk_size = std::min(remaining, BUFFER_SIZE);
                ifs1.read(comp_buf1.data(), chunk_size);
                ifs2.read(comp_buf2.data(), chunk_size);
                // 比较当前块
                if (memcmp(comp_buf1.data(), comp_buf2.data(), chunk_size) != 0)
                {
                    // 块内逐字节找差异
                    for (size_t i = 0; i < chunk_size; ++i)
                    {
                        if (comp_buf1[i] != comp_buf2[i])
                        {
                             std::cout << "Warning: The first difference occurs at byte " <<current_pos + i + 1 << std::endl; // +1转换为从1开始计数
                             return;
                        }
                    }
                }
                current_pos += chunk_size;
            }
            std::cout << "Info: The data of the two files is exactly the same\n";
        }

    private:
        static std::vector<char> comp_buf1;
        static std::vector<char> comp_buf2;
    };
    inline std::vector<char> FileCmp::comp_buf1(BUFFER_SIZE);
    inline std::vector<char> FileCmp::comp_buf2(BUFFER_SIZE);
};