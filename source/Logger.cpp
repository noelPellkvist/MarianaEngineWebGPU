#include <Logger.hpp>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

void Logger::Info(const std::string& msg) {
    log(Level::Info, msg);
}

void Logger::Warning(const std::string& msg) {
    log(Level::Warning, msg);
}

void Logger::Error(const std::string& msg) {
    log(Level::Error, msg);
}

void Logger::log(Level level, const std::string& msg) {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S");

    switch (level) {
        case Level::Info:
            std::cout << GREEN  << "[INFO "    << oss.str() << "] "
                      << RESET << msg << std::endl;
            break;
        case Level::Warning:
            std::cout << YELLOW << "[WARNING " << oss.str() << "] "
                      << RESET << msg << std::endl;
            break;
        case Level::Error:
            std::cout << BOLD << RED << "[ERROR "   << oss.str() << "] "
                      << RESET << msg << std::endl;
            break;
    }
}
