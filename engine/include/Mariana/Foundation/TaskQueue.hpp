#pragma once
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

// Runs tasks one after another off the calling thread: natively on a worker thread of its own; on
// the web (no threads) Pump() runs them on the calling thread instead. Tasks must not touch the
// GPU or anything else that belongs to the main thread; hand results back through your own queue.
class TaskQueue
{
public:
    TaskQueue();
    // Waits for the task that is running, drops the ones that haven't started.
    ~TaskQueue();

    TaskQueue(const TaskQueue&) = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;

    void Enqueue(std::function<void()> task);
    // Tasks queued or running.
    size_t Pending() const;

    // Web: runs queued tasks until `budget` is used up (at least one). Native: nothing to do.
    void Pump(std::chrono::milliseconds budget);

private:
    mutable std::mutex m_Mutex;
    std::condition_variable m_Wake;
    std::deque<std::function<void()>> m_Tasks;
    size_t m_Running = 0;
    bool m_Stop = false;
    std::thread m_Thread;
};
