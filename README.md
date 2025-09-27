# Logging-System

一个轻量级、可扩展的同步/异步 C++ 日志系统。支持 std::format 风格占位符、可配置模式串、同步/异步两种日志器、文件滚动与自定义落地（sink）。

## 快速上手

最简单的方式是通过 `ljxlog.hpp` 提供的一行初始化与便捷宏。

### 控制台日志（同步）

```cpp
#include "ljxlog.hpp"
using namespace ljxlog;

int main() {
		// 同步控制台 logger（默认模式："[%d{%H:%M:%S}][%p] %m%n"）
		auto lg = init_stdout_logger("console", Level::DEBUG);

		INFO("hello {}");              // 使用默认 logger（宏内部自动注入 __FILE__/__LINE__）
		LOGINFO(lg, "num={} str={}", 42, "ok"); // 显式指定 logger
}
```

### 文件日志（异步 + 按大小滚动）

```cpp
#include "ljxlog.hpp"
using namespace ljxlog;

int main() {
		// 目录会自动创建；到达 10MB 触发滚动，文件名自动带时间/序号后缀
		auto lg = init_async_file_logger(
				"file", "./Draft/logfile/app.log", 10 * 1024 * 1024,
				Level::INFO, "[%d{%F %T}][%t][%p][%c][%f:%l] %m%n");

		LOGINFO(lg, "user={} action={}", 1001, "login");
}
```

### 自定义模式串（format）

模式串支持以下占位符：
- d{fmt}: 时间（strftime 格式，如 %F %T）
- T: 制表符\t
- t: 线程 ID
- p: 日志等级
- c: logger 名称
- f: 源文件名
- l: 行号
- m: 正文
- n: 换行符

示例：`"[%d{%F %T}][%p] %m%n"` → `[2025-09-27 10:00:00][INFO] message`。

提示：宏会传入文件/行号，但是否输出取决于模式串（只有包含 %f/%l 才会打印）。

## 项目架构

整体结构见根目录各头文件：

- 格式化（`format.hpp`）
	- 解析模式串为一组 FormatItem（时间/线程/级别/文件/行号/正文/换行等），按序渲染。
	- 构造时预解析；format 时仅顺序输出，性能稳定。

- 日志器（`logger.hpp`）
	- Logger 抽象基类：提供等级过滤、std::format 组装正文、调用 Format 渲染、分发到 sink。
	- SyncLogger：在调用线程内串行写入所有 sink（内部互斥）。
	- AsyncLogger：生产者线程只入队，消费者线程批量落地（基于 `looper.hpp`）。支持 flush/析构排空。
	- Builder：链式构建（设置名称/等级/类型/格式/落地），并能注册到 LoggerManager 统一管理；`ljxlog.hpp` 基于 Builder 封装了便捷初始化。

- 异步分发（`looper.hpp`）
	- 双缓冲 + 条件变量，push 时按字节数做背压，loop 线程回调写入。

- 缓冲（`buffer.hpp`）
	- 追加式字节缓冲，提供 begin()/append()/readAbleSize() 等原语。

- 落地（`sink.hpp`）
	- StdOutLogSink：标准输出
	- FixedFileLogSink：固定文件
	- RollBySizeLogSink：按大小滚动（写前预检查，按换行切割，精确控制文件大小）
	- RollByTimeLogSink：按时间滚动

## 落地扩展与自定义

每个落地都是继承 `LogSink` 实现 `log(const char*, size_t)` 与可选的 `close()` 即可：

```cpp
class MySink : public LogSink {
public:
		void log(const char* d, size_t n) override {
				// 写到网络 / 内存 / 第三方系统…
		}
		void close() override { /* 可选收尾 */ }
};

// 通过 Builder 安装
std::unique_ptr<Logger::Builder> b = std::make_unique<GlobalLoggerBuilder>();
b->buildLoggerName("custom");
b->buildType(LoggerType::LOGGER_ASYNC);
b->buildFormat("%m%n");
b->buildSink<MySink>();
auto lg = b->build();
```

也可以一次安装多个 sink，日志会被广播到所有落地。

## 性能数据（TrashFileSink 磁盘写场景）

在一台 2 核 2GB Linux 虚拟机上，我们进行了 3 轮压测（每轮含4个用例）：

- 负载：每条日志 49 字节正文（总约 50B/条，含换行）
- 条数：每个用例 1,000,000 条
- 落地：TrashFileSink（固定容量环形文件，真实磁盘写，文件不增长）
- 计时口径：包含异步 flush 排空

结果汇总（单位：MB/s，越大越好）：

第一轮：
- async 单线程：11.3163
- async 5 线程：4.26752
- sync 单线程：5.47903
- sync 5 线程：1.65319

第二轮：
- async 单线程：9.75334
- async 5 线程：4.20709
- sync 单线程：4.58941
- sync 5 线程：1.66723

第三轮：
- async 单线程：11.3959
- async 5 线程：3.64851
- sync 单线程：5.07511
- sync 5 线程：1.73009

结论：在真实磁盘写入场景中，异步日志在单线程与多线程下都显著快于同步。这是因为 I/O 延迟被消费者线程隔离，生产线程仅承担格式化+入队，形成计算与 I/O 的流水线；而同步方式会在写磁盘时阻塞调用线程。在 2 核机器上，1P1C（1 生产 + 1 消费）尤其能充分利用两核并行。

提示：数值存在一定波动，受页缓存、调度与后台回写影响。做更稳态对比可延长压测时长、进行预热并固定是否刷盘策略。

## 构建与运行

项目使用 CMake 构建。示例步骤：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

你也可以直接在代码中使用 `ljxlog.hpp` 封装的初始化函数与宏进行集成。

## 注意事项

- 使用 std::format 风格 `{}` 占位符，避免与 printf `%d/%s` 混用。
- 模式串是否输出文件/行号取决于是否包含 `%f/%l`。
- 若你需要“每条日志固定字节数”，请让模式串只包含 `%m` 或 `%m%n`，并据此调整正文长度（是否含换行 +1 字节）。
- 异步日志需要在程序退出前 `flush()` 或销毁 logger，以确保队列排空。
- 同步日志同样可以调用 `flush()` ，不过其功能仅仅是关闭 sink 的缓冲区，比如说如果 sink 将数据落地到某个文件， 那么调用 `flush()` 会将该文件给关闭掉，确保所有缓冲数据都写入文件中。

## 许可证

MIT
