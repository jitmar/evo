#include "utils/thread_pool.h"
#include "spdlog/spdlog.h"

namespace evosim {

ThreadPool::ThreadPool(unsigned int threads)
    : soft_terminate_(false)
{
    spdlog::info("ThreadPool: Initializing with {} threads.", threads);
    workers_.reserve(threads);
    for (uint8_t i = 0; i < threads; ++i) {
        addThread();
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(tasks_mut_);
        soft_terminate_ = true;
    }
    tasks_cv_.notify_all();
    for (std::thread &worker: workers_)
        worker.join();
}

void ThreadPool::resize(unsigned int new_size) {
    if (new_size == workers_.size()) return;

    if (new_size > workers_.size()) {
        while (new_size > workers_.size()) {
            addThread();
        }
    } else {
        spdlog::warn("ThreadPool::resize() - shrinking pool size is not implemented yet. Current size: {}, requested: {}", workers_.size(), new_size);
    }
}

// TODO: Is addThread thread safe? Answer.
void ThreadPool::addThread() {
    workers_.emplace_back([this] {
        spdlog::debug("ThreadPool: Worker thread started.");
        for (;;) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(this->tasks_mut_);
                
                this->tasks_cv_.wait(lock, [this] {
                    // Wake up if
                    return (
                        // there's a task OR
                        !this->tasks_.empty() ||

                        // there are no tasks and soft termination is requested
                        (this->tasks_.empty() && this->soft_terminate_)
                    );
                });

                // there's a task
                if (!this->tasks_.empty()) {
                    // Move the task from the front of the queue to local variable
                    // The queue element remains but is now in a moved-from state
                    task = std::move(this->tasks_.front());
                    // Remove the moved-from element from the queue
                    this->tasks_.pop();
                }

                // there are no tasks and soft termination is requested
                else if (this->tasks_.empty() && this->soft_terminate_) {
                    lock.unlock();
                    return;
                }

                // spurious wakeup
                else {
                    continue;
                }
            }

            // Run the task without holding any lock
            try {
                task();
            } catch (const std::exception& e) {
                spdlog::error(
                    "ThreadPool: Exception in worker thread: {}", e.what());
            } catch (...) {
                spdlog::error(
                    "ThreadPool: Unknown exception in worker thread.");
            }
        }
    });
    spdlog::info("ThreadPool: Added new worker thread. Total threads: {}",
        workers_.size());
}

void ThreadPool::removeThread() {
    spdlog::warn("ThreadPool::removeThread() is not implemented yet. This functionality requires a redesign to avoid race conditions.");
}

} // namespace evosim
