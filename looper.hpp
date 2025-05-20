#pragma once

#include <iostream>
#include <mutex>
#include <condition_variable>
#include <functional>
#include "buffer.hpp"

namespace log
{
    class AsyncLooper
    {
    public:
        using Func = std::function<void(Buffer &)>;
        using ptr = std::shared_ptr<AsyncLooper>;
    public:
        AsyncLooper(const Func &cb, bool check_space = true)
            : _task_manage(cb),
              _running(true),
              _looper(loop, this),
              _check_space(check_space) {}
        ~AsyncLooper() { stop(); }

    private:
        void push(std::string &msg)
        {
            //停止任务调度则结束任务添加操作
            if(_running == false) return;
            //否则在每个生命周期内添加一个任务
            {
                std::unique_lock<std::mutex> lock(_mtx);
                _push_cond.wait(lock, [&](){return _push_task.writeAbleSize() >= msg.size();});
                _push_task.push(msg.c_str(), msg.size());
            }
            //此时任务调度线程就可以开始处理任务了
            _pop_cond.notify_all();
        }
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
                    if(!_running && _pop_task.empty()) return;
                    //否则继续任务处理
                    //stop或者有任务待处理都可以直接继续运行代码，无需阻塞
                    _pop_cond.wait(lock, [&](){return !_push_task.empty() || !_running;});
                    _pop_task.swap(_push_task);
                }
                _push_cond.notify_all();
                // 唤醒生产者继续生产数据后，消费者就可以调用回调函数处理数据了，读写不冲突
                _task_manage(_pop_task);
                _pop_task.reset();
            }
        }
        // 停止任务调度
        void stop()
        {
            _running = false;
            _pop_cond.notify_all();
            _looper.join();
        }

    private:
        std::atomic<bool> _running;         // 决定当前工作是否继续运行
        std::condition_variable _push_cond; // 是否满足任务添加条件
        std::condition_variable _pop_cond;  // 是否满足任务获取条件
        std::mutex _mtx;                    // 条件变量相对应锁
        Buffer _push_task;                  // 任务添加缓冲区
        Buffer _pop_task;                   // 任务获取缓冲区
        std::thread _looper;                // 事务循环处理器
        bool _check_space;                  // 是否检查生产剩余空间是否够用，若不检查可能会触发扩容操作（这并非安全的）
    private:
        Func _task_manage;
    };
};