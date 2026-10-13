#pragma once
#include <string>

class Logger {
public:
    enum class Level {
        Info,
        Warning,
        Error
    };

    static void Info(const std::string& msg);
    static void Warning(const std::string& msg);
    static void Error(const std::string& msg);

private:
    static void log(Level level, const std::string& msg);

    static constexpr const char* RESET  = "\033[0m";
    static constexpr const char* GREEN  = "\033[32m";
    static constexpr const char* YELLOW = "\033[33m";
    static constexpr const char* RED    = "\033[31m";
    static constexpr const char* BOLD   = "\033[1m";
};
