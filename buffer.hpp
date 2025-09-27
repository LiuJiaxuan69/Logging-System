#pragma once

#include <iostream>
#include <vector>

/*
 可增长字节缓冲区（Append-only, 非线程安全）
 ------------------------------------------------------------
 用途：
  - 为异步日志/批量写场景提供简单的顺序写入（push）与顺序读取（begin/pop）能力。
  - 仅支持“尾部追加 + 从头消费”的单调语义：write_ptr 只前进、不回退；
    read_ptr 也只前进、不回退。reset() 可清空逻辑内容以复用空间。

 设计要点：
  - 初始容量较大（8MiB），以减少频繁扩容；
  - 当可写空间不足时按策略扩容：
      * 若本次写入长度 len < 阈值（8MiB），采用指数扩容（容量翻倍），直至足够；
      * 否则采用线性扩容（每次 +1MiB），直至足够。
  - 不做“前移/压实”（compact）：即使前部数据已被 pop，write_ptr 也不会回到空出的位置，
    因此长时间运行且持续 push/pop 的模式下，可能发生多次扩容；可通过 reset() 或 swap() 复用空间。

  线程安全：
  - 本类未做任何并发保护，若在多线程间共享需由调用方加锁。

  注意：
  - begin() 返回的指针在后续 push 触发扩容后可能失效，请在 push 之前消费完或复制数据。
  - pop(len) 会在越界时抛出异常；len 不应大于 readAbleSize()。
  - push(data, len) 要求 data 指向有效内存（当 len>0）。
*/

namespace ljxlog
{
    // 初始容量：8MiB
    const size_t BUFFER_DEFAULT_SIZE = 8 * 1024 * 1024;
    // 线性扩容步长：1MiB（仅当 len >= 阈值时采用线性增长）
    const size_t BUFFER_INCREACE_SIZE = 1 * 1024 * 1024;
    // 扩容策略切换阈值：8MiB（以本次 push 的 len 为判断依据）
    const size_t BUFFER_THRESHOLD_SIZE = 8 * 1024 * 1024;
    class Buffer
    {
    public:
        // 构造：read_ptr / write_ptr 均为 0，容器按默认容量分配
        Buffer()
            : _read_ptr(0),
              _write_ptr(0),
              _container(BUFFER_DEFAULT_SIZE)
        {}
        // 是否无可读数据
        bool empty() { return _read_ptr == _write_ptr; }
        // 可读字节数（write_ptr - read_ptr）
        size_t readAbleSize() { return _write_ptr - _read_ptr; }
        // 可写字节数（capacity - write_ptr）
        size_t writeAbleSize() { return _container.size() - _write_ptr; }
        // 逻辑清空：仅重置读写指针，不收缩容量
        void reset() { _write_ptr = _read_ptr = 0; }
        // 交换缓冲区的内部状态（指针与容器）
        void swap(Buffer &buffer)
        {
            std::swap(_read_ptr, buffer._read_ptr);
            std::swap(_write_ptr, buffer._write_ptr);
            _container.swap(buffer._container);
        }
        // 追加写入 len 字节数据；必要时扩容
        void push(const char *data, size_t len)
        {
            ensureEnoughSpace(len);
            std::copy(data, data + len, _container.data() + _write_ptr);
            _write_ptr += len;
        }
        // 获取当前可读区域起始指针（注意：后续 push 导致的扩容会使该指针失效）
        const char *begin() {return _container.data() + _read_ptr;}
        // 消费 len 字节数据；越界时抛出异常
        void pop(size_t len) {
            if(len > readAbleSize()) throw std::runtime_error("Unauthorized access to container");
            _read_ptr += len;
        }

    private:
        // 确保至少可写 len 字节；根据 len 与阈值选择扩容策略
        void ensureEnoughSpace(size_t len)
        {
            //写入长度超出可写长度范围，需要扩容
            while(len > writeAbleSize())
            {
                //小于阈值则指数增长扩容
                if(len < BUFFER_THRESHOLD_SIZE) _container.resize(_container.size() << 1);
                //否则线性增长
                else _container.resize(_container.size() + BUFFER_INCREACE_SIZE);
            }
        }
        // 读指针：指向当前可读区域起点
        size_t _read_ptr;
        // 写指针：指向下一个可写位置
        size_t _write_ptr;
        // 存储容器：连续字节序列；扩容使用 resize，可能导致内存重分配
        std::vector<char> _container;
    };
};