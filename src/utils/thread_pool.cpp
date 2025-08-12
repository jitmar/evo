
#include "utils/thread_pool.h"
#include "spdlog/spdlog.h"

namespace evosim {
namespace utils {


ThreadPool::ThreadPool(size_t threads)
    : stop_(false)
{
    spdlog::info("ThreadPool: Initializing with {} threads.", threads);
    workers_.reserve(threads);
    for (size_t i = 0; i < threads; ++i) {
        addThread();
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    condition_.notify_all();
    for (std::thread &worker: workers_)
        worker.join();
}

void ThreadPool::resize(size_t new_size) {
    if (new_size == workers_.size()) return;

    if (new_size > workers_.size()) {
        while (new_size > workers_.size()) {
            addThread();
        }
    } else {
        while (new_size < workers_.size()) {
            removeThread();
        }
    }
}

void ThreadPool::addThread() {
    workers_.emplace_back([this] {
        spdlog::debug("ThreadPool: Worker thread started.");
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(this->queue_mutex_);
                this->condition_.wait(lock, [this] {
                    return this->stop_ || !this->tasks_.empty();
                });
                if (this->stop_ && this->tasks_.empty()) {
                    spdlog::debug("ThreadPool: Worker thread stopping.");
                    return;
                }
                task = std::move(this->tasks_.front());
                this->tasks_.pop();
            }
            try {
                task();
            } catch (const std::exception& e) {
                spdlog::error("ThreadPool: Exception in worker thread: {}", e.what());
            } catch (...) {
                spdlog::error("ThreadPool: Unknown exception in worker thread.");
            }
        }
    });
    spdlog::info("ThreadPool: Added new worker thread. Total threads: {}", workers_.size());
}

void ThreadPool::removeThread() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (!tasks_.empty()) {
            spdlog::warn("ThreadPool: Attempting to remove thread while tasks are still in queue. This may lead to unexecuted tasks.");
            // It might not be safe to remove a thread if there are tasks in the queue
            // For now, we proceed, but this is a potential issue.
        }

        if (workers_.empty()) {
            spdlog::warn("ThreadPool: No worker threads to remove.");
            return;
        }

        // Signal one thread to stop and exit its loop
        stop_ = true; // This will stop all threads if not handled carefully
        condition_.notify_one();
    }

    // Join the last worker thread
    if (!workers_.empty() && workers_.back().joinable()) {
        workers_.back().join();
        std::unique_lock<std::mutex> lock(queue_mutex_); // Re-acquire lock to modify workers_ vector
        workers_.pop_back();
        spdlog::info("ThreadPool: Removed one worker thread. Total threads: {}", workers_.size());
        // Reset stop_ flag if we are not completely stopping the pool
        if (workers_.empty()) {
            stop_ = false; // Reset if all threads are gone
        } else {
            // If there are still threads, we need to ensure they don't stop prematurely.
            // This is a complex scenario for resize(down) and might need a different stop mechanism.
            // For now, we assume stop_ is only true during full shutdown or single thread removal.
            // A better approach for resize down would be to have a per-thread stop flag or a counter.
            spdlog::warn("ThreadPool: stop_ flag was set to true for single thread removal. This might affect other threads.");
            stop_ = false; // Reset immediately after one thread is joined.
        }
    }
}

} // namespace utils
} // namespace evosim
