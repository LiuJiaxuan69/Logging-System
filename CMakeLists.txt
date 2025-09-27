cmake_minimum_required(VERSION 3.12)
project(Test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_COMPILER clang-19)
set(CMAKE_CXX_COMPILER clang++-19)

add_compile_options(-stdlib=libc++)
add_link_options(-stdlib=libc++)

# 公共头文件接口库（不编译任何文件）
add_library(test_common INTERFACE)

# 统一管理所有测试需要的 include 路径
target_include_directories(test_common INTERFACE
    ${CMAKE_SOURCE_DIR}       # 主项目头文件
    ${CMAKE_SOURCE_DIR}/test  # test 公共头文件
)

# 统一测试入口点
add_executable(test_main
    test/test_main.cpp
)

# 测试入口所需要的 include 路径
target_include_directories(test_main PRIVATE
    ${CMAKE_SOURCE_DIR}/test/basic  # test 公共头文件
)

# 公共 test_main 源文件
set(TEST_MAIN ${CMAKE_SOURCE_DIR}/test/test_main.cpp)

# 基础功能测试
# Functional tests target
add_executable(basic_tests
    ${TEST_MAIN}
    test/basic/test_stdout_macros.cpp
    test/basic/test_fixed_file_sink.cpp
    test/basic/test_roll_by_size_sink.cpp
    test/basic/test_roll_by_time_sink.cpp
    test/basic/test_async_logger.cpp
    test/basic/test_builder_and_format.cpp
)
target_link_libraries(basic_tests PRIVATE test_common pthread)

# 启用 CTest 并注册用例
include(CTest)
enable_testing()

# 把 basic_tests 注册为一个 CTest 测试
add_test(NAME basic_tests COMMAND basic_tests)

# 将每个测试用例分别注册，便于 VS Code 显示逐个绿/红点
add_test(NAME basic.logger_param_macros_basic   COMMAND basic_tests --case logger_param_macros_basic)
add_test(NAME basic.fixed_file_sink_basic       COMMAND basic_tests --case fixed_file_sink_basic)
add_test(NAME basic.roll_by_size_never_exceed   COMMAND basic_tests --case roll_by_size_never_exceed)
add_test(NAME basic.roll_by_time_creates_multiple_files COMMAND basic_tests --case roll_by_time_creates_multiple_files)
add_test(NAME basic.async_logger_drains_and_orders COMMAND basic_tests --case async_logger_drains_and_orders)
add_test(NAME basic.builder_default_and_format_items COMMAND basic_tests --case builder_default_and_format_items)
add_test(NAME basic.format_placeholders_and_escape COMMAND basic_tests --case format_placeholders_and_escape)

set_tests_properties(
    basic.logger_param_macros_basic
    basic.fixed_file_sink_basic
    basic.roll_by_size_never_exceed
    basic.roll_by_time_creates_multiple_files
    basic.async_logger_drains_and_orders
    basic.builder_default_and_format_items
    basic.format_placeholders_and_escape
    PROPERTIES LABELS "basic"
)

# 综合功能测试
add_executable(comprehensive_tests
    ${TEST_MAIN}
    test/comprehensive/test_async_comprehensive.cpp
    test/comprehensive/test_sync_comprehensive.cpp
)
target_link_libraries(comprehensive_tests PRIVATE test_common pthread)

# 把 comprehensive_tests 注册为一个 CTest 测试
add_test(NAME comprehensive.async_logger_comprehensive COMMAND comprehensive_tests --case async_logger_comprehensive)
add_test(NAME comprehensive.sync_logger_comprehensive COMMAND comprehensive_tests --case sync_logger_comprehensive)

set_tests_properties(
    comprehensive.async_logger_comprehensive
    comprehensive.sync_logger_comprehensive
    PROPERTIES LABELS "comprehensive"
)

# 压力测试
add_executable(stress_tests
    ${TEST_MAIN}
    test/stress/test_async_st_stress.cpp    # 单线程异步日志压力测试
    test/stress/test_async_mt5_stress.cpp   # 5 线程异步日志压力测试
    test/stress/test_sync_st_stress.cpp     # 单线程同步日志压力测试
    test/stress/test_sync_mt5_stress.cpp    # 5 线程同步日志压力测试
)
target_link_libraries(stress_tests PRIVATE test_common pthread)

# 把 stress_tests 注册为一个 CTest 测试
add_test(NAME stress.async_logger_stress_st COMMAND stress_tests --case async_logger_stress_st)
add_test(NAME stress.async_logger_stress_mt5 COMMAND stress_tests --case async_logger_stress_mt5)
add_test(NAME stress.sync_logger_stress_st COMMAND stress_tests --case sync_logger_stress_st)
add_test(NAME stress.sync_logger_stress_mt5 COMMAND stress_tests --case sync_logger_stress_mt5)

set_tests_properties(
    stress.async_logger_stress_st
    stress.async_logger_stress_mt5
    stress.sync_logger_stress_st
    stress.sync_logger_stress_mt5
    PROPERTIES LABELS "stress"
)
