#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t thread_nums) : stopped_(false)
{
    // 至少保证一个工作线程，避免任务永远排队
    if (thread_nums == 0)
    {
        thread_nums = 1;
    }
    for (size_t i = 0; i < thread_nums; ++i)
    {
        workers_.emplace_back([this]() { WorkerLoop(); });
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        stopped_ = true;
    }
    condition_.notify_all();
    // 等待所有工作线程退出（退出前会把已排队的任务执行完）
    for (std::thread &worker : workers_)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
}

void ThreadPool::WorkerLoop()
{
    while (true)
    {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this]() { return stopped_ || !tasks_.empty(); });
            // 池已停止且队列已空 → 线程退出；否则继续把剩余任务执行完
            if (stopped_ && tasks_.empty())
            {
                return;
            }
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();
    }
}
