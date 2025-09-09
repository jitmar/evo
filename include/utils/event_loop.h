#pragma once

#include "core/event.h"

#include <future>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace evosim {

class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    void start();
    void stop();

    // TODO: Generalize the event payload mechanism
    // TODO: Support dynamic event types
    void emitSignal(EventType type);

    // // Schedule a generic task
    // std::future<void> scheduleTask(std::function<void()> task);

    // Register an event callback
    void afterSignal(EventType type);

private:

    // TODO: Better name maybe, since it's exactly a worker thread
    // more like worker function
    void workerThread();
    // // void emitEvent(const Event& event);

    std::thread worker_thread_;

    // // TDOD: Consider using two variables, one to indicate "should stop" and
    // // another to indicate "is running"
    std::atomic<bool> running_{false};
    // std::queue<std::function<void()>> task_queue_;
    // std::mutex queue_mutex_;
    std::mutex start_stop_mutex_;
    std::condition_variable queue_cv_;
};

} // namespace evosim
