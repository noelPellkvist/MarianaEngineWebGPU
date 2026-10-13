#include <stb_image.h>

#include <stdexcept>

#include <FileReader.hpp>
#include <Logger.hpp>
#include <fstream>
#include <sstream>
#include <filesystem>

std::string FileReader::LoadRawString(const std::string& path)
{
    std::filesystem::path realPath = std::string(RESOURCE_DIR) + path;
    std::ifstream file(realPath, std::ios::binary);
    if (!file.is_open()) {
        const std::string message = "Failed to open file: " + realPath.string();
        Logger::Error(message);
        throw std::runtime_error(message);
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

std::vector<uint8_t> FileReader::LoadPixelsFromImage(const std::string& path, int& width, int& height)
{
    int channels;
    unsigned char *pixelData = stbi_load((std::string(RESOURCE_DIR) + path).c_str(), &width, &height, &channels, 4);
    if (!pixelData) {
        throw std::runtime_error("Failed to load image: " + path);
    }
    size_t size = static_cast<size_t>(width) * height * 4;
    std::vector<uint8_t> pixels(pixelData, pixelData + size);
    stbi_image_free(pixelData);
    return pixels;
}
