#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <future>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "ThreadPool.h"

// 自由函数：验证函数指针也能直接提交
int Square(int x) { return x * x; }

int main()
{
    // 1. 基本提交：无参 / 带参 / 函数指针，future 取结果
    {
        ThreadPool pool(4);
        auto f1 = pool.Submit([]() { return 42; });
        auto f2 = pool.Submit([](int a, int b) { return a + b; }, 20, 22);
        auto f3 = pool.Submit(Square, 7);
        assert(f1.get() == 42);
        assert(f2.get() == 42);
        assert(f3.get() == 49);
    }

    // 2. 批量任务全部执行，结果正确（0^2 + 1^2 + ... + 999^2）
    {
        ThreadPool pool(8);
        std::vector<std::future<int>> results;
        results.reserve(1000);
        for (int i = 0; i < 1000; ++i)
        {
            results.emplace_back(pool.Submit([i]() { return i * i; }));
        }
        long long sum = 0;
        for (auto &r : results)
        {
            sum += r.get();
        }
        assert(sum == 999LL * 1000 * 1999 / 6);
    }

    // 3. 任务抛出的异常通过 future 传播到提交线程
    {
        ThreadPool pool(2);
        auto bad = pool.Submit([]() -> int { throw std::runtime_error("boom"); });
        bool caught = false;
        try
        {
            bad.get();
        }
        catch (const std::runtime_error &e)
        {
            caught = (std::string(e.what()) == "boom");
        }
        assert(caught);
    }

    // 4. 任务确实分布在多个工作线程上执行（每个任务睡 1ms，保证并行重叠）
    {
        ThreadPool pool(4);
        std::vector<std::future<std::thread::id>> ids;
        for (int i = 0; i < 64; ++i)
        {
            ids.emplace_back(pool.Submit([]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                return std::this_thread::get_id();
            }));
        }
        std::set<std::thread::id> distinct;
        for (auto &f : ids)
        {
            distinct.insert(f.get());
        }
        assert(distinct.size() >= 2);
    }

    // 5. 析构前仍在排队的任务也会被全部执行完
    {
        std::atomic<int> counter(0);
        {
            ThreadPool pool(4);
            for (int i = 0; i < 200; ++i)
            {
                pool.Submit([&counter]() { ++counter; });
            }
        }
        assert(counter.load() == 200);
    }

    printf("test_threadpool: all tests passed.\n");
    return 0;
}
