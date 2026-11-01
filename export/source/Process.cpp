#include <Mariana/Export/Process.hpp>

#include <cstdlib>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace
{
#ifdef _WIN32
    std::wstring Wide(const std::string& text)
    {
        if (text.empty())
            return {};
        const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        std::wstring wide(static_cast<size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
        return wide;
    }

    std::string Narrow(const std::wstring& text)
    {
        if (text.empty())
            return {};
        const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string narrow(static_cast<size_t>(length), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), narrow.data(), length, nullptr, nullptr);
        return narrow;
    }

    std::string LastError()
    {
        wchar_t* message = nullptr;
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, GetLastError(), 0,
                       reinterpret_cast<wchar_t*>(&message), 0, nullptr);
        std::string text = message ? Narrow(message) : std::string("unknown error");
        LocalFree(message);
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
            text.pop_back();
        return text;
    }

    // One argument as CommandLineToArgvW reads it back.
    void Quote(const std::wstring& arg, std::wstring& out)
    {
        if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos)
        {
            out += arg;
            return;
        }
        out += L'"';
        for (size_t i = 0;; ++i)
        {
            size_t backslashes = 0;
            while (i < arg.size() && arg[i] == L'\\')
            {
                ++i;
                ++backslashes;
            }
            if (i == arg.size())
            {
                out.append(backslashes * 2, L'\\');
                break;
            }
            if (arg[i] == L'"')
                out.append(backslashes * 2 + 1, L'\\');
            else
                out.append(backslashes, L'\\');
            out += arg[i];
        }
        out += L'"';
    }

    std::wstring CommandLine(const std::vector<std::string>& args)
    {
        std::wstring line;
        for (const std::string& arg : args)
        {
            if (!line.empty())
                line += L' ';
            Quote(Wide(arg), line);
        }
        return line;
    }

    // This process's environment with `changes` made (names compare without case), as
    // CreateProcessW takes it; empty when nothing changes.
    std::wstring Environment(const std::vector<std::pair<std::string, std::string>>& changes)
    {
        if (changes.empty())
            return {};
        std::vector<std::wstring> entries;
        if (wchar_t* block = GetEnvironmentStringsW())
        {
            for (const wchar_t* p = block; *p; p += wcslen(p) + 1)
                entries.emplace_back(p);
            FreeEnvironmentStringsW(block);
        }
        for (const auto& [name, value] : changes)
        {
            const std::wstring key = Wide(name) + L"=";
            std::erase_if(entries, [&](const std::wstring& entry) { return _wcsnicmp(entry.c_str(), key.c_str(), key.size()) == 0; });
            if (!value.empty())
                entries.push_back(key + Wide(value));
        }
        std::wstring out;
        for (const std::wstring& entry : entries)
        {
            out += entry;
            out += L'\0';
        }
        out += L'\0';
        return out;
    }

    // A job that ends every process in it when it is closed: what a program started goes with it.
    HANDLE MakeJob()
    {
        HANDLE job = CreateJobObjectW(nullptr, nullptr);
        if (!job)
            return nullptr;
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
        return job;
    }

    // Starts `args` suspended in `job`, then lets it run; `output` (or NUL) gets its stdout and stderr.
    bool Spawn(const std::vector<std::string>& args, const Process::Options& options, HANDLE job, HANDLE output, PROCESS_INFORMATION& info,
               std::string& error)
    {
        if (args.empty())
        {
            error = "Nothing to run";
            return false;
        }
        SECURITY_ATTRIBUTES inherit{ sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
        HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, 0, nullptr);
        HANDLE sink = output ? output : CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, 0, nullptr);

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = input;
        startup.hStdOutput = sink;
        startup.hStdError = sink;

        std::wstring command = CommandLine(args);
        std::wstring environment = Environment(options.environment);
        const std::wstring directory = options.workingDirectory.empty() ? std::wstring() : options.workingDirectory.wstring();
        const BOOL started = CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
                                            environment.empty() ? nullptr : environment.data(), directory.empty() ? nullptr : directory.c_str(), &startup, &info);
        if (!started)
            error = "Could not start " + args[0] + ": " + LastError();
        CloseHandle(input);
        if (!output)
            CloseHandle(sink);
        if (!started)
            return false;
        if (job)
            AssignProcessToJobObject(job, info.hProcess);
        ResumeThread(info.hThread);
        CloseHandle(info.hThread);
        info.hThread = nullptr;
        return true;
    }
#endif
}

namespace Process
{
#ifdef _WIN32
    int Run(const std::vector<std::string>& args, const Options& options, const std::function<void(const std::string&)>& line, std::string& error)
    {
        SECURITY_ATTRIBUTES inherit{ sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
        HANDLE read = nullptr, write = nullptr;
        if (!CreatePipe(&read, &write, &inherit, 0))
        {
            error = "Could not make a pipe: " + LastError();
            return -1;
        }
        SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0);

        HANDLE job = MakeJob();
        PROCESS_INFORMATION info{};
        const bool started = Spawn(args, options, job, write, info, error);
        CloseHandle(write); // only the program holds the writing end now
        if (!started)
        {
            CloseHandle(read);
            if (job)
                CloseHandle(job);
            return -1;
        }

        // Lines as they come; the pipe ends when everything holding its writing end is gone.
        std::atomic<bool> drained = false;
        std::thread reader([&]() {
            std::string pending;
            char buffer[4096];
            DWORD count = 0;
            while (ReadFile(read, buffer, sizeof(buffer), &count, nullptr) && count > 0)
            {
                pending.append(buffer, count);
                size_t end;
                while ((end = pending.find('\n')) != std::string::npos)
                {
                    std::string text = pending.substr(0, end);
                    if (!text.empty() && text.back() == '\r')
                        text.pop_back();
                    pending.erase(0, end + 1);
                    if (line)
                        line(text);
                }
            }
            if (!pending.empty() && line)
                line(pending);
            drained = true;
        });

        bool cancelled = false;
        while (WaitForSingleObject(info.hProcess, 100) == WAIT_TIMEOUT)
        {
            if (options.cancel && options.cancel->load())
            {
                cancelled = true;
                if (job)
                    TerminateJobObject(job, 1);
                else
                    TerminateProcess(info.hProcess, 1);
            }
        }
        DWORD code = 0;
        GetExitCodeProcess(info.hProcess, &code);
        CloseHandle(info.hProcess);
        // What it left running (build servers that kept the pipe) goes with the job, so the pipe ends;
        // a reader still waiting after that is waiting for something that broke away from the job.
        if (job)
            CloseHandle(job);
        for (int i = 0; i < 40 && !drained; ++i)
            Sleep(50);
        if (!drained)
            CancelSynchronousIo(static_cast<HANDLE>(reader.native_handle()));
        reader.join();
        CloseHandle(read);
        if (cancelled)
        {
            error = "Cancelled";
            return -1;
        }
        return static_cast<int>(code);
    }

    struct Background::Impl
    {
        HANDLE job = nullptr;
        HANDLE process = nullptr;
    };

    Background::Background() : m_Impl(std::make_unique<Impl>()) {}

    Background::~Background() { Stop(); }

    bool Background::Start(const std::vector<std::string>& args, const Options& options, std::string& error)
    {
        Stop();
        m_Impl->job = MakeJob();
        PROCESS_INFORMATION info{};
        if (!Spawn(args, options, m_Impl->job, nullptr, info, error))
        {
            Stop();
            return false;
        }
        m_Impl->process = info.hProcess;
        return true;
    }

    void Background::Stop()
    {
        if (m_Impl->job)
        {
            TerminateJobObject(m_Impl->job, 0);
            CloseHandle(m_Impl->job);
            m_Impl->job = nullptr;
        }
        if (m_Impl->process)
        {
            CloseHandle(m_Impl->process);
            m_Impl->process = nullptr;
        }
    }

    bool Background::IsRunning() const
    {
        return m_Impl->process && WaitForSingleObject(m_Impl->process, 0) == WAIT_TIMEOUT;
    }

    bool Launch(const std::vector<std::string>& args, const std::filesystem::path& workingDirectory, std::string& error)
    {
        if (args.empty())
        {
            error = "Nothing to run";
            return false;
        }
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION info{};
        std::wstring command = CommandLine(args);
        const std::wstring directory = workingDirectory.wstring();
        if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_BREAKAWAY_FROM_JOB | DETACHED_PROCESS, nullptr, directory.empty() ? nullptr : directory.c_str(),
                            &startup, &info) &&
            !CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, DETACHED_PROCESS, nullptr, directory.empty() ? nullptr : directory.c_str(), &startup, &info))
        {
            error = "Could not start " + args[0] + ": " + LastError();
            return false;
        }
        CloseHandle(info.hThread);
        CloseHandle(info.hProcess);
        return true;
    }

    std::filesystem::path FindOnPath(const std::string& name)
    {
        std::wstring found(MAX_PATH, L'\0');
        const std::wstring wide = Wide(name);
        const DWORD length = SearchPathW(nullptr, wide.c_str(), L".exe", static_cast<DWORD>(found.size()), found.data(), nullptr);
        if (length == 0 || length >= found.size())
            return {};
        found.resize(length);
        return found;
    }
#else
    int Run(const std::vector<std::string>& args, const Options&, const std::function<void(const std::string&)>&, std::string& error)
    {
        error = "Running " + (args.empty() ? std::string("programs") : args[0]) + " needs the desktop editor on Windows";
        return -1;
    }

    struct Background::Impl
    {
    };

    Background::Background() : m_Impl(std::make_unique<Impl>()) {}
    Background::~Background() = default;

    bool Background::Start(const std::vector<std::string>&, const Options&, std::string& error)
    {
        error = "Running programs needs the desktop editor on Windows";
        return false;
    }

    void Background::Stop() {}
    bool Background::IsRunning() const { return false; }

    bool Launch(const std::vector<std::string>&, const std::filesystem::path&, std::string& error)
    {
        error = "Running programs needs the desktop editor on Windows";
        return false;
    }

    std::filesystem::path FindOnPath(const std::string&) { return {}; }
#endif
}
