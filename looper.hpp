#pragma once

#include <iostream>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include "buffer.hpp"

/*
 异步事件循环（AsyncLooper）
 ------------------------------------------------------------
 目标：
  - 提供一个简单的“生产者-消费者”模型，生产者线程将字符串任务 push 到缓冲区；
    内部循环线程 loop() 取出任务并通过回调 Func 进行处理（如写入多个 sink）。

  设计要点：
  - 双缓冲（_push_task / _pop_task）+ 互换（swap）：保护区只在持锁条件下交换一次，锁外进行耗时处理，降低锁竞争。
  - 条件变量：
      * _push_cond：当可写空间不足时阻塞 push，直到可写空间满足（或消费者处理完毕唤醒）。
      * _pop_cond ：当有任务可取或停止信号到来时唤醒消费者。
  - 停止语义：
      * stop() 将 _running 置为 false，并唤醒消费者；
      * loop() 在“停止且 _push_task 为空”时退出，保证已提交的任务都被处理完成。

    使用建议：
    - push 会等待直到 _push_task 有足够可写空间（采用强制检查，体现背压语义）。
  - 回调 Func 签名为 void(Buffer&)：请只读取 buffer.begin()/readAbleSize() 指向的数据，并在回调内尽快处理。
  - 本类不是线程安全的通用队列，仅适用于“同一 looper 内多生产者/单消费者”的模式。

    注意：
  - begin() 返回的指针在 push 导致缓冲区扩容后可能失效；本实现通过“交换缓冲区后在锁外消费”减少此风险。
  - 停止时先发信号再 join()，避免死锁；stop() 需在对象析构过程中可安全调用。
*/

namespace ljxlog
{
    class AsyncLooper
    {
    public:
        using Func = std::function<void(Buffer &)>;
        using ptr = std::shared_ptr<AsyncLooper>;
    public:
      // cb：消费者回调
        AsyncLooper(const Func &cb)
            : _task_manage(cb),
              _running(true),
              _looper(&AsyncLooper::loop, this) {}
        ~AsyncLooper() { stop(); }

        // 生产：提交一条字符串任务
        void push(const std::string &msg)
        {
            //停止任务调度则结束任务添加操作
            if(_running == false) return;
            //否则在每个生命周期内添加一个任务
            {
                std::unique_lock<std::mutex> lock(_mtx);
                // 等待可写空间 >= 本次任务大小；当前策略会在空间不足时阻塞生产者
                _push_cond.wait(lock, [&](){return _push_task.writeAbleSize() >= msg.size();});
                _push_task.push(msg.c_str(), msg.size());
            }
            //此时任务调度线程就可以开始处理任务了
            _pop_cond.notify_all();
        }
    private:
        // 事件循环，检测是否有任务可以处理，若有任务则交换缓冲区（上一次锁即可）
        void loop()
        {
            //即便停止任务调度，任务队列中的任务仍需全部完成才能结束，故不能以_running的真与否来判断函数是否继续运行
            while(true)
            {
                //生命周期结束后释放锁
                {
                    std::unique_lock<std::mutex> lock(_mtx);
                    //只有在任务真正被处理完且_running为false的时候才能退出事件循环，而后回收该线程
                    if(!_running && _push_task.empty()) return;
                    //否则继续任务处理
                    //stop或者有任务待处理都可以直接继续运行代码，无需阻塞
                    _pop_cond.wait(lock, [&](){return !_push_task.empty() || !_running;});
                    // 将生产缓冲与消费缓冲交换；交换后在锁外处理，缩短持锁时间
                    // 此时可能任务已经停止了，需要重新检查_running的值
                    if(!_running && _push_task.empty()) return;
                    // 限制单次处理的最大字节数，防止单次任务
                    _pop_task.swap(_push_task);
                }
                // 唤醒可能在等待可写空间的生产者
                _push_cond.notify_all();
                // 唤醒生产者继续生产数据后，消费者就可以调用回调函数处理数据了，读写不冲突
                _task_manage(_pop_task);
                _pop_task.reset();
                if(_flushing && _push_task.empty() && _pop_task.empty()) {
                    std::unique_lock<std::mutex> lock(_flush_mtx);
                    _cv.notify_all();
                }
            }
        }
        // 停止任务调度
        void stop()
        {
            // 发出停止信号并唤醒消费者，让其在处理完现有任务后正常退出
            _running = false;
            _pop_cond.notify_all();
            // 等待 loop 线程结束；要求 _looper 已创建且可 join
            _looper.join();
        }
    public:
        void flush() {
            std::unique_lock<std::mutex> lock(_flush_mtx);
            _flushing = true;
            // 此时 loop 仍然在运行，因此需要等待 push_task 和 pop_task 都为空
            _cv.wait(lock, [&](){return _push_task.empty() && _pop_task.empty();});
            _flushing = false;
            return;
        }

    private:
        std::atomic<bool> _running;         // 决定当前工作是否继续运行
        std::atomic<bool> _flushing = false; // 是否正在刷新
        std::condition_variable _push_cond; // 是否满足任务添加条件
        std::condition_variable _pop_cond;  // 是否满足任务获取条件
        std::condition_variable _cv;        // 是否满足任务刷新条件
        std::mutex _flush_mtx;              // 刷新条件对应锁
        std::mutex _mtx;                    // 条件变量相对应锁
        Buffer _push_task;                  // 任务添加缓冲区
        Buffer _pop_task;                   // 任务获取缓冲区
        std::thread _looper;                // 事务循环处理器
    private:
        Func _task_manage;
    };
};