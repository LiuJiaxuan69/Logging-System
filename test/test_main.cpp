// 测试主程序
// 作用：
// - 作为极简测试运行器，按注册顺序执行所有测试用例
// - 支持命令行筛选：--list/--case/--filter，便于 CTest/VS Code 单测粒度控制
// - 约定：任一测试抛异常或断言失败（EXPECT_* 触发 exit(1)）即视为失败
#include "test_framework.hpp"
#include <exception>
#include <cstring>
#include <vector>
#include <string>

int main(int argc, char **argv)
{
    // 命令行参数说明：
    // --list         列出全部测试用例名后退出
    // --case <name>  精确匹配某个测试用例名并执行
    // --filter <sub> 执行名称包含 <sub> 的一组测试
    // CLI: --list | --case <name> | --filter <substr>
    bool list = false;
    std::string exact;
    std::string filter;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--list") == 0)
            list = true;
        else if (std::strcmp(argv[i], "--case") == 0 && i + 1 < argc)
        {
            exact = argv[++i];
        }
        else if (std::strcmp(argv[i], "--filter") == 0 && i + 1 < argc)
        {
            filter = argv[++i];
        }
    }
    auto &all = tinytest::registry();
    if (list)
    {
        for (auto &t : all)
            std::cout << t.name << "\n";
        return 0;
    }

    std::vector<std::reference_wrapper<tinytest::TestCase>> chosen;
    for (auto &t : all)
    {
        if (!exact.empty())
        {
            if (exact == t.name)
                chosen.push_back(t);
        }
        else if (!filter.empty())
        {
            if (std::string(t.name).find(filter) != std::string::npos)
                chosen.push_back(t);
        }
        else
        {
            chosen.push_back(t);
        }
    }
    if (chosen.empty())
    {
        std::cerr << "No tests selected\n";
        return 1;
    }

    int failed = 0;
    for (auto &r : chosen)
    {
        auto &t = r.get();
        try
        {
            t.fn();
        }
        catch (const std::exception &e)
        {
            std::cerr << "Test '" << t.name << "' threw: " << e.what() << "\n";
            failed = 1;
        }
        catch (...)
        {
            std::cerr << "Test '" << t.name << "' threw unknown exception\n";
            failed = 1;
        }
    }
    if (failed)
    {
        std::cerr << "Some tests failed\n";
        return 1;
    }
    std::cout << "All tests passed (" << chosen.size() << ")\n";
    return 0;
}
