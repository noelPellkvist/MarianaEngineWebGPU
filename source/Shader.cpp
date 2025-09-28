#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> 
#include <glm/ext/matrix_clip_space.hpp>

#include <Shader.hpp>
#include <Logger.hpp>
#include <Init.hpp>
#include <VertexBufferLayout.hpp>
#include <Mesh.hpp>
#include <UniformLayout.hpp>
#include <moved_later/GLTFLoader.hpp>
#include <ICamera.hpp>

// struct UBO {
//   glm::vec3 lightDir;
// };

// struct TransformData {
//   glm::mat4x4 modelMatrix;
//   glm::mat4x4 normalMatrix;
// };

// UBO ubo{};
// UniformLayout uboLayout(false, ubo, ubo.lightDir);

// TransformData modelsBuffer{};
// UniformLayout modelsLayout(true, modelsBuffer, modelsBuffer.modelMatrix, modelsBuffer.normalMatrix);

// Shader::Shader(uint8_t textureCount) : m_TextureCount(textureCount)
// {
// }

void IShader::FixTextureBindings()
{
  std::vector<wgpu::BindGroupLayoutEntry> entries;
  entries.resize(m_TextureCount + 1);
  for(size_t i = 0; i < m_TextureCount; i++)
  {
    entries[i] = {};
    entries[i].binding = i;
    entries[i].visibility = wgpu::ShaderStage::Fragment;
    entries[i].texture.sampleType = wgpu::TextureSampleType::Float;
    entries[i].texture.viewDimension = wgpu::TextureViewDimension::e2D;
  }

  entries[m_TextureCount] = {};
  entries[m_TextureCount].binding = 5;
  entries[m_TextureCount].visibility = wgpu::ShaderStage::Fragment;
  entries[m_TextureCount].sampler.type = wgpu::SamplerBindingType::Filtering;

  wgpu::BindGroupLayoutDescriptor textureBindingLayout{};
  textureBindingLayout.entryCount = entries.size();
  textureBindingLayout.entries = entries.data();
  m_PerDrawBindLayout = device.CreateBindGroupLayout(&textureBindingLayout);
}