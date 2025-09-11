#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <stdexcept>

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
