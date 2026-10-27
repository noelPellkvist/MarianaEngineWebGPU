#pragma once
// What the engine's scenarios share: a result per scenario, a log line for it, formatting.
#include <cstdio>
#include <functional>
#include <string>

namespace EngineScenarios
{
    struct Result
    {
        std::string name;
        std::string detail;
        bool passed = false;
    };

    using Log = std::function<void(const std::string&)>;

    namespace detail
    {
        inline std::string Format(const char* format, double a = 0, double b = 0, double c = 0, double d = 0, double e = 0)
        {
            char text[512];
            std::snprintf(text, sizeof(text), format, a, b, c, d, e);
            return text;
        }

        inline Result Finish(const Log& log, const std::string& name, const std::string& detail, bool passed)
        {
            log(std::string(passed ? "  ok    " : "  FAIL  ") + name + ": " + detail);
            return { name, detail, passed };
        }
    }
}
