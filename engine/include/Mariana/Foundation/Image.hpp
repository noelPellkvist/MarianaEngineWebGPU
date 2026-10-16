#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// CPU images (RGBA8): decoding, scaling and PNG encoding. No GPU work, so safe on any thread.
namespace Image
{
    struct Pixels
    {
        int width = 0;
        int height = 0;
        std::vector<uint8_t> rgba; // width * height * 4, rows top to bottom

        bool Empty() const { return rgba.empty(); }
    };

    // Any format stb_image reads (PNG, JPEG, TGA, BMP, HDR, ...), converted to RGBA8.
    bool Load(const std::filesystem::path& file, Pixels& out, std::string& error);
    bool SavePNG(const std::filesystem::path& file, const Pixels& image, std::string& error);

    // Halves the image (2x2 box filter, colors weighted by alpha) until neither side is larger
    // than `maxSize` (0 = as is).
    Pixels Downscale(Pixels image, uint32_t maxSize);
}
