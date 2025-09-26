#pragma once
#include <vector>
#include <webgpu/webgpu_cpp.h>
#include <string>
#include <memory>
#include <type_traits>
#include <concepts>

#include <Shader.hpp>
#include <Sampler.hpp>
#include <Texture.hpp>

class IMaterial
{
    public:
        IMaterial();
        ~IMaterial();

        void InitMaterial(Shader& shader, std::vector<Texture> textures);
        wgpu::BindGroup& GetTextureBindGroup() { return bindGroup; }
    
    private:
        std::vector<Texture> m_Textures;
        std::vector<wgpu::Sampler> samplers;
        wgpu::BindGroup bindGroup;

        void LoadSampler(int minFilter, int magFilter, WrapMode wrapS, WrapMode wrapT);
};

template<typename MaterialData>
class Material : public IMaterial
{
    private:
        MaterialData data;
        uint32_t bufferIndex;
};

template<typename T>
concept DerivedFromIMaterial = std::derived_from<std::remove_cvref_t<T>, IMaterial>;

struct MaterialInstance
{
    std::shared_ptr<IMaterial> material;

    template<typename T>
    requires DerivedFromIMaterial<T>
    explicit MaterialInstance(T&& concrete)
        : material(std::make_shared<std::remove_cvref_t<T>>(std::forward<T>(concrete))) {}
};