#ifndef GAMEDECK_THREAD_POOL_H
#define GAMEDECK_THREAD_POOL_H

// 简单线程池：用于执行阻塞的 Docker 命令，避免阻塞 asio 事件循环
//
// 实现要点：
//   - 固定数量工作线程，任务入队后由空闲线程取出执行
//   - 使用 std::function 作为任务类型，支持任意可调用对象
//   - 条件变量 + 互斥锁实现线程安全的任务队列
//   - 析构时通知所有线程退出并 join

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace gamedeck
{

class ThreadPool
{
 public:
    explicit ThreadPool(size_t threadCount = 4)
    {
        workers_.reserve(threadCount);
        for (size_t i = 0; i < threadCount; ++i)
        {
            workers_.emplace_back([this]() { workerLoop(); });
        }
    }

    ~ThreadPool()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_)
        {
            if (t.joinable()) t.join();
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // 提交任务，返回 std::future 以便获取结果
    template <typename F>
    auto submit(F&& f) -> std::future<decltype(f())>
    {
        using ResultType = decltype(f());
        auto task = std::make_shared<std::packaged_task<ResultType()>>(
            std::forward<F>(f));
        std::future<ResultType> result = task->get_future();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stop_)
            {
                throw std::runtime_error("ThreadPool 已停止");
            }
            tasks_.emplace([task]() { (*task)(); });
        }
        cv_.notify_one();
        return result;
    }

 private:
    void workerLoop()
    {
        while (true)
        {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this]() { return stop_ || !tasks_.empty(); });

                if (stop_ && tasks_.empty())
                {
                    return;
                }

                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
};

}  // namespace gamedeck

#endif  // GAMEDECK_THREAD_POOL_H
