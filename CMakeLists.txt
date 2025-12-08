# ==============================================================================
# 【全局基础配置】：工程级别的核心约定，保证编译环境统一、语法兼容
# ==============================================================================
# 1. 指定CMake最低版本要求：低于3.12会报错，确保后续语法（如INTERFACE库、CTest）兼容
cmake_minimum_required(VERSION 3.12)

# 2. 定义工程信息：
#    - NAME: Test（工程名，无实际编译作用，仅标识）
#    - LANGUAGES: CXX（仅启用C++编译，排除C/ASM等无关语言，减少编译开销）
project(Test LANGUAGES CXX)

# 3. 配置C++标准：
#    - CMAKE_CXX_STANDARD 23：指定使用C++23标准
#    - CMAKE_CXX_STANDARD_REQUIRED ON：强制要求编译器支持C++23，不允许降级（如自动用C++20）
#    - 注：结合后续clang-19，保证C++23特性完整支持
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 4. 显式指定编译器：
#    - 强制使用clang-19（C/C++分别指定，这里仅用CXX但保留C配置，便于扩展）
#    - 目的：统一编译环境，避免系统默认gcc/libstdc++的兼容性问题
set(CMAKE_C_COMPILER clang-19)
set(CMAKE_CXX_COMPILER clang++-19)

# 5. 配置编译/链接参数：
#    - -stdlib=libc++：强制使用clang配套的libc++标准库（替代gcc的libstdc++）
#    - add_compile_options：给所有编译目标添加编译参数
#    - add_link_options：给所有链接目标添加链接参数
#    - 注：libc++对C++23特性支持更完整，适合日志库的现代语法
add_compile_options(-stdlib=libc++)
add_link_options(-stdlib=libc++)

# ==============================================================================
# 【公共接口库】：封装所有测试的通用配置，避免重复代码（CMake最佳实践）
# ==============================================================================
# 1. 创建INTERFACE类型的空库（无源码，仅传递配置）：
#    - INTERFACE库：不生成二进制文件，仅作为"配置容器"
#    - 命名：test_common（标识为测试通用配置）
add_library(test_common INTERFACE)

# 2. 绑定公共头文件路径（INTERFACE表示：依赖该库的目标会自动继承这些路径）：
#    - ${CMAKE_SOURCE_DIR}：工程根目录（日志库核心头文件所在路径）
#    - ${CMAKE_SOURCE_DIR}/test：test目录（测试工具/公共宏定义头文件）
#    - 作用：所有测试目标只需链接test_common，无需重复写include路径
target_include_directories(test_common INTERFACE
    ${CMAKE_SOURCE_DIR}       # 主项目：日志库核心头文件
    ${CMAKE_SOURCE_DIR}/test  # 测试侧：公共测试工具/宏定义头文件
)

# 3. 定义测试入口文件变量：
#    - TEST_MAIN：存储test_main.cpp的绝对路径（所有测试目标复用该入口）
#    - 目的：统一测试入口（包含main函数、测试框架初始化、用例解析逻辑）
set(TEST_MAIN ${CMAKE_SOURCE_DIR}/test/test_main.cpp)

# ==============================================================================
# 【基础功能测试模块】：验证日志库单个核心功能的正确性（最小单元测试）
# ==============================================================================
# 1. 创建基础测试可执行文件：
#    - 目标名：basic_tests（标识基础功能测试）
#    - 源码组成：
#      - ${TEST_MAIN}：通用测试入口（解析--case参数、运行指定用例）
#      - test/basic/*.cpp：各基础功能的测试源码（如日志输出宏、文件 sink、异步日志等）
add_executable(basic_tests
    ${TEST_MAIN}
    test/basic/test_stdout_macros.cpp          # 测试日志输出宏（stdout）
    test/basic/test_fixed_file_sink.cpp        # 测试固定文件 sink（不滚动）
    test/basic/test_roll_by_size_sink.cpp      # 测试按大小滚动文件 sink
    test/basic/test_roll_by_time_sink.cpp      # 测试按时间滚动文件 sink
    test/basic/test_async_logger.cpp           # 测试异步日志核心逻辑
    test/basic/test_builder_and_format.cpp     # 测试日志构建器和格式化逻辑
)

# 2. 链接依赖（PRIVATE表示：配置仅作用于basic_tests，不传递）：
#    - test_common：继承公共头文件路径
#    - pthread：链接线程库（日志库涉及多线程，必须依赖）
target_link_libraries(basic_tests PRIVATE test_common pthread)

# ==============================================================================
# 【CTest测试框架配置】：注册测试用例，支持精细化运行/调试（CMake内置测试工具）
# ==============================================================================
# 1. 引入CTest模块 + 启用测试功能：
#    - include(CTest)：加载CTest核心逻辑
#    - enable_testing()：激活当前目录的测试功能（必须调用，否则add_test无效）
include(CTest)
enable_testing()

# 2. 注册基础测试「整体用例」：
#    - NAME：basic_tests（测试用例名，唯一标识）
#    - COMMAND：basic_tests（运行可执行文件，无参数时执行所有基础用例）
add_test(NAME basic_tests COMMAND basic_tests)

# 3. 注册基础测试「单个用例」：
#    - 每个用例通过--case参数指定（test_main.cpp内部解析该参数，只运行单个用例）
#    - 命名规范：basic.用例名（便于分类和过滤）
#    - 目的：支持精准调试（如只跑某一个失败的用例，无需运行全部）
add_test(NAME basic.logger_param_macros_basic   COMMAND basic_tests --case logger_param_macros_basic)          # 测试日志参数宏基础功能
add_test(NAME basic.fixed_file_sink_basic       COMMAND basic_tests --case fixed_file_sink_basic)              # 测试固定文件sink基础功能
add_test(NAME basic.roll_by_size_never_exceed   COMMAND basic_tests --case roll_by_size_never_exceed)          # 测试按大小滚动-不超限场景
add_test(NAME basic.roll_by_time_creates_multiple_files COMMAND basic_tests --case roll_by_time_creates_multiple_files)  # 测试按时间滚动-生成多文件
add_test(NAME basic.async_logger_drains_and_orders COMMAND basic_tests --case async_logger_drains_and_orders)  # 测试异步日志-排空和顺序性
add_test(NAME basic.builder_default_and_format_items COMMAND basic_tests --case builder_default_and_format_items)  # 测试构建器-默认配置和格式化项
add_test(NAME basic.format_placeholders_and_escape COMMAND basic_tests --case format_placeholders_and_escape)  # 测试格式化-占位符和转义

# 4. 给基础测试用例打标签：
#    - PROPERTIES LABELS "basic"：为用例添加"basic"标签
#    - 作用：支持按标签过滤测试（如ctest -L basic 只运行基础测试）
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

# ==============================================================================
# 【综合功能测试模块】：验证日志库多组件协同的端到端场景
# ==============================================================================
# 1. 创建综合测试可执行文件：
#    - 目标名：comprehensive_tests（标识综合功能测试）
#    - 源码组成：通用入口 + 综合场景测试源码（异步/同步日志全流程）
add_executable(comprehensive_tests
    ${TEST_MAIN}
    test/comprehensive/test_async_comprehensive.cpp  # 异步日志综合场景测试（多sink+多线程+格式化）
    test/comprehensive/test_sync_comprehensive.cpp   # 同步日志综合场景测试（全功能覆盖）
)

# 2. 链接依赖（同基础测试）
target_link_libraries(comprehensive_tests PRIVATE test_common pthread)

# 3. 注册综合测试单个用例：
#    - 命名规范：comprehensive.用例名（分类标识）
#    - 覆盖异步/同步日志的全流程场景
add_test(NAME comprehensive.async_logger_comprehensive COMMAND comprehensive_tests --case async_logger_comprehensive)  # 异步日志综合测试
add_test(NAME comprehensive.sync_logger_comprehensive COMMAND comprehensive_tests --case sync_logger_comprehensive)    # 同步日志综合测试

# 4. 给综合测试用例打标签：便于过滤（ctest -L comprehensive）
set_tests_properties(
    comprehensive.async_logger_comprehensive
    comprehensive.sync_logger_comprehensive
    PROPERTIES LABELS "comprehensive"
)

# ==============================================================================
# 【压力测试模块】：验证日志库在高并发/高吞吐量下的性能和稳定性
# ==============================================================================
# 1. 创建压力测试可执行文件：
#    - 目标名：stress_tests（标识压力测试）
#    - 源码组成：通用入口 + 压力测试源码（单/多线程、同步/异步对比）
add_executable(stress_tests
    ${TEST_MAIN}
    test/stress/test_async_st_stress.cpp    # 单线程异步日志压力测试（基础性能基准）
    test/stress/test_async_mt5_stress.cpp   # 5线程异步日志压力测试（多线程并发）
    test/stress/test_sync_st_stress.cpp     # 单线程同步日志压力测试（对比基准）
    test/stress/test_sync_mt5_stress.cpp    # 5线程同步日志压力测试（多线程并发）
)

# 2. 链接依赖（同基础测试）
target_link_libraries(stress_tests PRIVATE test_common pthread)

# 3. 注册压力测试单个用例：
#    - 命名规范：stress.用例名（分类标识）
#    - 覆盖不同线程模型的压力场景，便于对比性能
add_test(NAME stress.async_logger_stress_st COMMAND stress_tests --case async_logger_stress_st)    # 异步日志-单线程压力测试
add_test(NAME stress.async_logger_stress_mt5 COMMAND stress_tests --case async_logger_stress_mt5)  # 异步日志-5线程压力测试
add_test(NAME stress.sync_logger_stress_st COMMAND stress_tests --case sync_logger_stress_st)      # 同步日志-单线程压力测试
add_test(NAME stress.sync_logger_stress_mt5 COMMAND stress_tests --case sync_logger_stress_mt5)    # 同步日志-5线程压力测试

# 4. 给压力测试用例打标签：便于过滤（ctest -L stress）
set_tests_properties(
    stress.async_logger_stress_st
    stress.async_logger_stress_mt5
    stress.sync_logger_stress_st
    stress.sync_logger_stress_mt5
    PROPERTIES LABELS "stress"
)

# ==============================================================================
# 【使用说明（补充注释）】：
# 1. 编译：mkdir build && cd build && cmake .. && make -j8
# 2. 运行所有测试：ctest
# 3. 运行指定标签测试：ctest -L basic（基础）/ctest -L stress（压力）
# 4. 运行单个用例：ctest -R basic.fixed_file_sink_basic
# 5. 直接运行可执行文件：./basic_tests --case fixed_file_sink_basic（精准调试）
# ==============================================================================