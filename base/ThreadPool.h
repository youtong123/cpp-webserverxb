#pragma once
#include "common.h"

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// 通用线程池：固定线程数 + 任务队列（mutex + condition_variable 保护）
// Submit 支持任意可调用对象与任意参数（可变参数模板），
// 内部用 std::packaged_task 打包任务，返回 std::future 取结果 / 捕获异常。
class ThreadPool
{
public:
    DISALLOW_COPY_AND_MOVE(ThreadPool);

    explicit ThreadPool(size_t thread_nums = std::thread::hardware_concurrency());
    ~ThreadPool();

    // 提交任务，返回 future；任务抛出的异常会存入 future（调用 get() 时重新抛出）
    template <typename F, typename... Args>
    auto Submit(F &&f, Args &&...args) -> std::future<typename std::result_of<F(Args...)>::type>
    {
        using ReturnType = typename std::result_of<F(Args...)>::type;

        // packaged_task 只可移动，用 shared_ptr 包一层才能放进 std::function
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<ReturnType> result = task->get_future();
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stopped_)
            {
                throw std::runtime_error("ThreadPool::Submit: pool has been stopped");
            }
            tasks_.emplace([task]() { (*task)(); });
        }
        condition_.notify_one();
        return result;
    }

private:
    void WorkerLoop();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool stopped_;
};
