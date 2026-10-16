#include <Mariana/Foundation/FileWatcher.hpp>

#include <unordered_set>
#include <utility>

FileWatcher::FileWatcher(std::filesystem::path root, std::chrono::milliseconds interval)
    : m_Root(std::move(root)), m_Interval(interval)
{
#if !defined(__EMSCRIPTEN__)
    m_Thread = std::thread([this]() {
        std::unique_lock<std::mutex> lock(m_Mutex);
        while (!m_Stop)
        {
            lock.unlock();
            Scan();
            lock.lock();
            m_Wake.wait_for(lock, m_Interval, [this]() { return m_Stop; });
        }
    });
#endif
}

FileWatcher::~FileWatcher()
{
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Stop = true;
    }
    m_Wake.notify_all();
    if (m_Thread.joinable())
        m_Thread.join();
}

void FileWatcher::Poll(const std::function<void(const Event&)>& onEvent)
{
#if defined(__EMSCRIPTEN__)
    const auto now = std::chrono::steady_clock::now();
    if (m_LastScan == std::chrono::steady_clock::time_point{} || now - m_LastScan >= m_Interval)
    {
        m_LastScan = now;
        Scan();
    }
#endif
    std::vector<Event> events;
    bool scanned = false;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        events.swap(m_Pending);
        scanned = m_ScanDone;
    }
    for (const Event& event : events)
        onEvent(event);
    m_Delivered = scanned;
}

bool FileWatcher::Stat(const std::filesystem::path& file, uint64_t& size, int64_t& modified)
{
    size = 0;
    modified = 0;
    std::error_code ec;
    if (!std::filesystem::is_regular_file(file, ec))
        return false;
    const uint64_t bytes = std::filesystem::file_size(file, ec);
    if (ec)
        return false;
    const auto time = std::filesystem::last_write_time(file, ec);
    if (ec)
        return false;
    size = bytes;
    modified = time.time_since_epoch().count();
    return true;
}

void FileWatcher::Scan()
{
    // Only the scanning thread touches m_Known.
    std::vector<Event> found;
    std::unordered_set<std::string> seen;
    std::error_code ec;
    std::filesystem::recursive_directory_iterator it(m_Root, std::filesystem::directory_options::skip_permission_denied, ec);
    for (const std::filesystem::recursive_directory_iterator end; !ec && it != end; it.increment(ec))
    {
        const std::filesystem::directory_entry& entry = *it;
        const std::string name = entry.path().filename().string();
        if (entry.is_directory(ec))
        {
            if (!name.empty() && name[0] == '.')
                it.disable_recursion_pending();
            continue;
        }
        // Asked fresh (not the listing's copy, which can lag behind a file being written).
        FileState state;
        if (!Stat(entry.path(), state.size, state.modified))
            continue;
        const std::string key = entry.path().generic_string();
        seen.insert(key);

        auto known = m_Known.find(key);
        if (known == m_Known.end())
        {
            m_Known.emplace(key, state);
            found.push_back({ entry.path(), Change::Added, state.size, state.modified });
        }
        else if (known->second.size != state.size || known->second.modified != state.modified)
        {
            known->second = state;
            found.push_back({ entry.path(), Change::Modified, state.size, state.modified });
        }
    }
    for (auto known = m_Known.begin(); known != m_Known.end();)
    {
        if (seen.count(known->first))
        {
            ++known;
            continue;
        }
        found.push_back({ std::filesystem::path(known->first), Change::Removed, 0, 0 });
        known = m_Known.erase(known);
    }

    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Pending.insert(m_Pending.end(), std::make_move_iterator(found.begin()), std::make_move_iterator(found.end()));
    m_ScanDone = true;
}
