#include <SkyLight.hpp>
#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>

namespace
{
    // Faces are averaged down to about this many texels per side first: L2 harmonics only keep
    // the lowest frequencies anyway.
    constexpr int kSampleSize = 32;

    // Constants of the 9 real spherical harmonics basis functions.
    constexpr float kBasis[9] = { 0.282095f, 0.488603f, 0.488603f, 0.488603f, 1.092548f, 1.092548f, 0.315392f, 1.092548f, 0.546274f };
    // Cosine-lobe convolution per band (pi, 2pi/3, pi/4), divided by pi.
    constexpr float kCosineLobe[9] = { 1.0f, 2.0f / 3.0f, 2.0f / 3.0f, 2.0f / 3.0f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f };

    float SrgbToLinear(float c)
    {
        return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
    }

    // Direction through (u, v) in [-1, 1] on a face, matching GPU cubemap sampling (v points down).
    glm::vec3 FaceDirection(int face, float u, float v)
    {
        switch (face)
        {
            case 0:  return {  1.0f, -v,   -u    };
            case 1:  return { -1.0f, -v,    u    };
            case 2:  return {  u,     1.0f, v    };
            case 3:  return {  u,    -1.0f, -v   };
            case 4:  return {  u,    -v,    1.0f };
            default: return { -u,    -v,   -1.0f };
        }
    }
}

std::array<glm::vec4, 9> ComputeSkyIrradianceSH(const std::vector<std::vector<uint8_t>>& faces, int size)
{
    std::array<glm::vec4, 9> result{};
    if (faces.size() != 6 || size <= 0)
        return result;

    float toLinear[256];
    for (int i = 0; i < 256; ++i)
        toLinear[i] = SrgbToLinear(i / 255.0f);

    const int block = std::max(1, size / kSampleSize);
    const int samples = size / block;

    std::array<glm::vec3, 9> sh{};
    float totalWeight = 0.0f;
    for (int face = 0; face < 6; ++face)
    {
        if (faces[face].size() < static_cast<size_t>(size) * size * 4)
            return result;
        const uint8_t* pixels = faces[face].data();

        for (int sy = 0; sy < samples; ++sy)
        {
            for (int sx = 0; sx < samples; ++sx)
            {
                // Mean radiance of the block, averaged in linear space.
                glm::vec3 radiance(0.0f);
                for (int y = sy * block; y < (sy + 1) * block; ++y)
                {
                    const uint8_t* texel = pixels + (static_cast<size_t>(y) * size + static_cast<size_t>(sx) * block) * 4;
                    for (int x = 0; x < block; ++x, texel += 4)
                        radiance += glm::vec3(toLinear[texel[0]], toLinear[texel[1]], toLinear[texel[2]]);
                }
                radiance /= static_cast<float>(block * block);

                const float u = (sx + 0.5f) / samples * 2.0f - 1.0f;
                const float v = (sy + 0.5f) / samples * 2.0f - 1.0f;
                const glm::vec3 onFace = FaceDirection(face, u, v);
                // Solid angle of the sample: texels towards a face's corners cover less of the sphere.
                const float lengthSq = glm::dot(onFace, onFace);
                const float weight = 4.0f / (samples * samples) / (lengthSq * std::sqrt(lengthSq));
                const glm::vec3 d = onFace / std::sqrt(lengthSq);

                const float polynomial[9] = { 1.0f, d.y, d.z, d.x, d.x * d.y, d.y * d.z, 3.0f * d.z * d.z - 1.0f, d.x * d.z, d.x * d.x - d.y * d.y };
                for (int i = 0; i < 9; ++i)
                    sh[i] += radiance * (kBasis[i] * polynomial[i] * weight);
                totalWeight += weight;
            }
        }
    }

    // The weights should add up to the whole sphere (4 pi); this removes the sampling error.
    const float normalize = 4.0f * glm::pi<float>() / totalWeight;
    // Folding the basis constants in leaves only the polynomial in N for the shader.
    for (int i = 0; i < 9; ++i)
        result[i] = glm::vec4(sh[i] * (normalize * kCosineLobe[i] * kBasis[i]), 0.0f);
    return result;
}
