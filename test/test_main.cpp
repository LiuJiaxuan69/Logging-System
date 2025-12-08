// 测试框架主程序（测试运行器）
// 核心能力：执行注册的测试用例 + 命令行参数筛选测试用例
// 失败规则：断言失败(exit(1))/抛异常均视为测试失败
#include "test_framework.hpp"
#include <exception>   // 捕获标准异常
#include <cstring>     // 字符串比较（strcmp）
#include <vector>      // 存储选中的测试用例
#include <string>      // 命令行参数处理

int main(int argc, char **argv)
{
    // 命令行参数解析：支持3种模式
    // --list         仅列出所有注册的测试用例名
    // --case <name>  仅执行指定名称的单个测试用例
    // --filter <sub> 执行名称包含指定子串的所有测试用例
    bool list = false;          // 是否仅列出用例
    std::string exact;          // --case 指定的精准匹配名
    std::string filter;         // --filter 指定的模糊匹配子串

    // 遍历命令行参数（跳过argv[0]：程序名）
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--list") == 0)
            list = true;
        else if (std::strcmp(argv[i], "--case") == 0 && i + 1 < argc)
        {
            exact = argv[++i];  // 取--case后紧跟的用例名
        }
        else if (std::strcmp(argv[i], "--filter") == 0 && i + 1 < argc)
        {
            filter = argv[++i]; // 取--filter后紧跟的匹配子串
        }
    }

    // 获取所有已注册的测试用例
    auto &all = tinytest::registry();

    // 仅列出所有测试用例名，执行后退出
    if (list)
    {
        for (auto &t : all)
            std::cout << t.name << "\n";
        return 0;
    }

    // 根据命令行参数筛选要执行的测试用例
    std::vector<std::reference_wrapper<tinytest::TestCase>> chosen;
    for (auto &t : all)
    {
        if (!exact.empty())
        {
            // 精准匹配：仅执行指定名称的用例
            if (exact == t.name)
                chosen.push_back(t);
        }
        else if (!filter.empty())
        {
            // 模糊匹配：执行名称包含指定子串的用例
            if (std::string(t.name).find(filter) != std::string::npos)
                chosen.push_back(t);
        }
        else
        {
            // 无筛选：执行所有用例
            chosen.push_back(t);
        }
    }

    // 无匹配用例时报错退出
    if (chosen.empty())
    {
        std::cerr << "No tests selected\n";
        return 1;
    }

    // 执行选中的测试用例，统计失败状态
    int failed = 0;
    for (auto &r : chosen)
    {
        auto &t = r.get();  // 解引用获取测试用例对象
        try
        {
            t.fn();  // 执行测试用例函数
        }
        catch (const std::exception &e)
        {
            // 捕获标准异常并记录失败
            std::cerr << "Test '" << t.name << "' threw: " << e.what() << "\n";
            failed = 1;
        }
        catch (...)
        {
            // 捕获未知异常并记录失败
            std::cerr << "Test '" << t.name << "' threw unknown exception\n";
            failed = 1;
        }
    }

    // 测试失败：输出提示并返回非0退出码
    if (failed)
    {
        std::cerr << "Some tests failed\n";
        return 1;
    }

    // 所有测试通过：输出成功信息并返回0
    std::cout << "All tests passed (" << chosen.size() << ")\n";
    return 0;
}