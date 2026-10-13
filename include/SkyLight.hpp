#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

// Diffuse light from a sky cubemap as 9 spherical harmonics coefficients (L2, rgb in xyz). They are
// already convolved with the cosine lobe and divided by pi, so `skyIrradiance(N) * albedo` in
// test.wgsl is the diffuse color. `faces` are sRGB RGBA8, `size` pixels square, in cube order
// (+X, -X, +Y, -Y, +Z, -Z) and oriented the way the GPU samples a cubemap.
std::array<glm::vec4, 9> ComputeSkyIrradianceSH(const std::vector<std::vector<uint8_t>>& faces, int size);
