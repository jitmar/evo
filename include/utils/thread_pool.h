
#pragma once

#include <vector>
#include <queue>

// TODO: Consider std::jthread
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <type_traits>  // For std::invoke_result_t

#include <cstdint>

namespace evosim {

// TODO: Ponder if the class should be static
// TODO: Can threads be paused and resumed as the need be
class ThreadPool {
public:

    // TODO: Support dynamic resizing of the pool and load balancing
    ThreadPool(unsigned int threads = std::thread::hardware_concurrency());
    ~ThreadPool();

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;

    // TODO: Check if resize is needed
    void resize(unsigned int new_size);

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    void addThread();
    void removeThread();

    // TODO: Add option for hard softdown, currently 'soft_terminate_' should
    // dictate termination only when there are no tasks left
    bool soft_terminate_;

    std::mutex tasks_mut_;
    std::condition_variable tasks_cv_;
};

template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>>
{
    using return_type = std::invoke_result_t<F, Args...>;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        [f = std::forward<F>(f), ...args = std::forward<Args>(args)]() mutable -> return_type {
            return std::invoke(f, args...);
        }
    );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(tasks_mut_);

        if(soft_terminate_)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        tasks_.emplace([task](){ (*task)(); });
    }
    tasks_cv_.notify_one();
    return res;
}

} // namespace evosim
