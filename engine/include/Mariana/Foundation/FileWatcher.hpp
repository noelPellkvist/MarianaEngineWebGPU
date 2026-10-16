#pragma once
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Watches a folder tree for files being added, changed or removed, by comparing each file's size
// and last write time between scans. Natively it scans on its own thread every `interval`; on the
// web (no threads) Poll() scans when the interval has passed. Folders whose name starts with '.'
// (caches such as .mariana) are skipped.
class FileWatcher
{
public:
    enum class Change { Added, Modified, Removed };

    struct Event
    {
        std::filesystem::path file;
        Change change = Change::Added;
        uint64_t size = 0;
        int64_t modified = 0; // last write time, in the file clock's ticks
    };

    explicit FileWatcher(std::filesystem::path root, std::chrono::milliseconds interval = std::chrono::milliseconds(500));
    ~FileWatcher();

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    const std::filesystem::path& Root() const { return m_Root; }

    // A file's size and last write time as the watcher compares them; false (and zeros) when it
    // isn't a file. For code that wants to compare its own reads with the watcher's events.
    static bool Stat(const std::filesystem::path& file, uint64_t& size, int64_t& modified);

    // Calls `onEvent` (on the calling thread) for every change found since the last call. The
    // first scan reports every file there is as Added.
    void Poll(const std::function<void(const Event&)>& onEvent);
    // Whether Poll has handed out the whole first scan, i.e. every file there was at the start.
    bool Scanned() const { return m_Delivered; }

private:
    struct FileState
    {
        uint64_t size = 0;
        int64_t modified = 0;
    };

    std::filesystem::path m_Root;
    std::chrono::milliseconds m_Interval;
    std::unordered_map<std::string, FileState> m_Known;

    std::mutex m_Mutex;
    std::vector<Event> m_Pending;
    std::condition_variable m_Wake;
    bool m_Stop = false;
    bool m_ScanDone = false;  // under m_Mutex
    bool m_Delivered = false; // Poll's thread only
    std::thread m_Thread;
    std::chrono::steady_clock::time_point m_LastScan{};

    void Scan();
};
