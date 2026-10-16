#include <Mariana/Foundation/Image.hpp>

#include <stb_image.h>
#include <stb_image_write.h>

#include <algorithm>
#include <utility>

namespace Image
{
    bool Load(const std::filesystem::path& file, Pixels& out, std::string& error)
    {
        int channels = 0;
        stbi_uc* data = stbi_load(file.string().c_str(), &out.width, &out.height, &channels, 4);
        if (!data)
        {
            error = "Could not read image " + file.string() + (stbi_failure_reason() ? std::string(": ") + stbi_failure_reason() : std::string());
            out = {};
            return false;
        }
        out.rgba.assign(data, data + static_cast<size_t>(out.width) * out.height * 4);
        stbi_image_free(data);
        return true;
    }

    bool SavePNG(const std::filesystem::path& file, const Pixels& image, std::string& error)
    {
        std::error_code ec;
        std::filesystem::create_directories(file.parent_path(), ec);
        if (image.Empty() || !stbi_write_png(file.string().c_str(), image.width, image.height, 4, image.rgba.data(), image.width * 4))
        {
            error = "Could not write " + file.string();
            return false;
        }
        return true;
    }

    Pixels Downscale(Pixels image, uint32_t maxSize)
    {
        while (maxSize > 0 && (static_cast<uint32_t>(image.width) > maxSize || static_cast<uint32_t>(image.height) > maxSize))
        {
            Pixels half;
            half.width = std::max(image.width / 2, 1);
            half.height = std::max(image.height / 2, 1);
            half.rgba.resize(static_cast<size_t>(half.width) * half.height * 4);
            for (int y = 0; y < half.height; ++y)
            {
                const int y0 = std::min(y * 2, image.height - 1), y1 = std::min(y * 2 + 1, image.height - 1);
                for (int x = 0; x < half.width; ++x)
                {
                    const int x0 = std::min(x * 2, image.width - 1), x1 = std::min(x * 2 + 1, image.width - 1);
                    const uint8_t* texels[4] = {
                        &image.rgba[(static_cast<size_t>(y0) * image.width + x0) * 4], &image.rgba[(static_cast<size_t>(y0) * image.width + x1) * 4],
                        &image.rgba[(static_cast<size_t>(y1) * image.width + x0) * 4], &image.rgba[(static_cast<size_t>(y1) * image.width + x1) * 4] };
                    // Colors weighted by alpha, so transparent texels don't bleed their color in.
                    uint32_t alpha = 0;
                    for (const uint8_t* t : texels)
                        alpha += t[3];
                    uint8_t* out = &half.rgba[(static_cast<size_t>(y) * half.width + x) * 4];
                    for (int c = 0; c < 3; ++c)
                    {
                        uint32_t sum = 0, plain = 0;
                        for (const uint8_t* t : texels)
                        {
                            sum += t[c] * t[3];
                            plain += t[c];
                        }
                        out[c] = static_cast<uint8_t>(alpha > 0 ? (sum + alpha / 2) / alpha : (plain + 2) / 4);
                    }
                    out[3] = static_cast<uint8_t>((alpha + 2) / 4);
                }
            }
            image = std::move(half);
        }
        return image;
    }
}
