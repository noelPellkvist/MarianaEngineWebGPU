#include "Resources.h"
#include <fstream>
#include <sstream>
#include <string>

wgpu::ShaderModule Resources::LoadShader(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        return nullptr;
    }
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    std::string shaderSource(size, ' ');
    file.seekg(0);
    file.read(shaderSource.data(), size);

    wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
  wgslDesc.code = shaderSource.c_str();

  wgpu::ShaderModuleDescriptor shaderModuleDescriptor{
      .nextInChain = &wgslDesc};

    return device.CreateShaderModule(&shaderModuleDescriptor);
}