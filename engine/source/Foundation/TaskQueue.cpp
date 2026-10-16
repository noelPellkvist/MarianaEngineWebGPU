#include <Mariana/Foundation/TaskQueue.hpp>

#include <utility>

TaskQueue::TaskQueue()
{
#if !defined(__EMSCRIPTEN__)
    m_Thread = std::thread([this]() {
        std::unique_lock<std::mutex> lock(m_Mutex);
        while (true)
        {
            m_Wake.wait(lock, [this]() { return m_Stop || !m_Tasks.empty(); });
            if (m_Stop)
                return;
            std::function<void()> task = std::move(m_Tasks.front());
            m_Tasks.pop_front();
            ++m_Running;
            lock.unlock();
            task();
            lock.lock();
            --m_Running;
        }
    });
#endif
}

TaskQueue::~TaskQueue()
{
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Stop = true;
        m_Tasks.clear();
    }
    m_Wake.notify_all();
    if (m_Thread.joinable())
        m_Thread.join();
}

void TaskQueue::Enqueue(std::function<void()> task)
{
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Tasks.push_back(std::move(task));
    }
    m_Wake.notify_one();
}

size_t TaskQueue::Pending() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Tasks.size() + m_Running;
}

void TaskQueue::Pump(std::chrono::milliseconds budget)
{
#if defined(__EMSCRIPTEN__)
    const auto start = std::chrono::steady_clock::now();
    do
    {
        std::function<void()> task;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            if (m_Tasks.empty())
                return;
            task = std::move(m_Tasks.front());
            m_Tasks.pop_front();
        }
        task();
    } while (std::chrono::steady_clock::now() - start < budget);
#else
    (void)budget;
#endif
}
