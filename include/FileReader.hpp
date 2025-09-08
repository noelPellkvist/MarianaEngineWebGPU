#pragma once
#include <string>

class FileReader
{
    public:
        static std::string LoadRawString(const std::string& path);
};