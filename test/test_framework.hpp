#pragma once
#include <vector>
#include <string>
#include <functional>
#include <iostream>
#include <cstdlib>

namespace tinytest
{
    struct TestCase
    {
        const char *name;
        std::function<void()> fn;
    };
    inline std::vector<TestCase> &registry()
    {
        static std::vector<TestCase> r;
        return r;
    }
    struct Registrar
    {
        Registrar(const char *n, std::function<void()> f) { registry().push_back({n, std::move(f)}); }
    };
}

#define TEST(name)                                     \
    void name();                                       \
    static tinytest::Registrar _r_##name(#name, name); \
    void name()

#define FALL(fmt, ...) do { \
    fprintf(stderr, "FALL: " fmt "\n", ##__VA_ARGS__); \
    exit(1); \
} while(0)

#define EXPECT_TRUE(cond)                                                                      \
    do                                                                                         \
    {                                                                                          \
        if (!(cond))                                                                           \
        {                                                                                      \
            std::cerr << "EXPECT_TRUE failed: " #cond " at " __FILE__ ":" << __LINE__ << "\n"; \
            std::exit(1);                                                                      \
        }                                                                                      \
    } while (0)
    
#define EXPECT_EQ(a, b)                                                                                                                 \
    do                                                                                                                                  \
    {                                                                                                                                   \
        auto _va = (a);                                                                                                                 \
        auto _vb = (b);                                                                                                                 \
        if (!(_va == _vb))                                                                                                              \
        {                                                                                                                               \
            std::cerr << "EXPECT_EQ failed: " #a " vs " #b " got [" << _va << "] [" << _vb << "] at " __FILE__ ":" << __LINE__ << "\n"; \
            std::cerr << a << " is not equal to " << b << "\n"; \
            std::exit(1);                                                                                                               \
        }                                                                                                                               \
    } while (0)

