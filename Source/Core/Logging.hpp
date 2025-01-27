#pragma once
#include <string>

class Logging
{
    public:
        static void PrintInfo(std::string msg);
        static void PrintSuccess(std::string msg);
        static void PrintWarning(std::string msg);
        static void PrintError(std::string msg);
};