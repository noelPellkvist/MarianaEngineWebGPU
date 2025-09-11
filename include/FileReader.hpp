#pragma once
#include <string>
#include <vector>

class FileReader
{
    public:
        static std::string LoadRawString(const std::string& path);
        static std::vector<unsigned char> LoadPixelsFromImage(const std::string& path, int& width, int& height);
};