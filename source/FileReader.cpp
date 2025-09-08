#include <FileReader.hpp>
#include <fstream>
#include <sstream>
#include <filesystem>

std::string FileReader::LoadRawString(const std::string& path)
{
    std::filesystem::path realPath = std::string(RESOURCE_DIR) + path;
    std::ifstream file(realPath);
    if (!file.is_open()) {
        return nullptr;
    }
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    std::string Source(size, ' ');
    file.seekg(0);
    file.read(Source.data(), size);
    return Source;
}
