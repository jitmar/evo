#include "utils/event_loop.h"
#include <spdlog/spdlog.h>

namespace evosim {

EventLoop::EventLoop() : running_(false) {}

EventLoop::~EventLoop() {
    stop();
}

// TODO: Make it thread-safe to call start/stop multiple times
void EventLoop::start() {
    std::lock_guard<std::mutex> lock(start_stop_mutex_);
    if (running_) return;
    running_ = true;

    // TODO: Consider using a thread pool for more complex scheduling needs
    worker_thread_ = std::thread(&EventLoop::workerThread, this);
    spdlog::info("EventLoop started");
}

void EventLoop::stop() {
    std::lock_guard<std::mutex> lock(start_stop_mutex_);
    if (!running_) return;

    // TODO: Check if the condition variable is necessary here
    // TODO: notify_one or notify_all?
    queue_cv_.notify_one(); // Wake up the worker thread to terminate
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    running_ = false;
    spdlog::info("EventLoop stopped");
}

void EventLoop::emitSignal(EventType type) {
    spdlog::info("Emitting event: {}", static_cast<int>(type));
}

void EventLoop::afterSignal(EventType type) {
    spdlog::info("Event received: {}", static_cast<int>(type));
}

// void EventLoop::workerThread() {
//     while (true) {
//         std::function<void()> task;
//         {
//             std::unique_lock<std::mutex> lock(queue_mutex_);
//             // TDOD: Validate the waiting condition
//             queue_cv_.wait(lock, [this] { return !running_ || !task_queue_.empty(); });
//             if (!running_ && task_queue_.empty()) break;
//             task = std::move(task_queue_.front());
//             task_queue_.pop();
//         }
//         task();
//     }
// }

} // namespace evosim