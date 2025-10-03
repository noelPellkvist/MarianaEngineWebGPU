#pragma once
#include <vector>
#include <webgpu/webgpu_cpp.h>
#include <string>
#include <memory>
#include <type_traits>
#include <concepts>
#include <any>
#include <stdexcept>
#include <vector>

#include <Shader.hpp>
#include <Sampler.hpp>
#include <Texture.hpp>

class IMaterial
{
    public:
        IMaterial(uint32_t index);
        ~IMaterial();

        void InitMaterial(IShader& shader, std::vector<Texture> textures);

        const wgpu::BindGroup& GetBindGroup(uint32_t index);

        virtual void UpdateMaterialProperties(const std::any& data) = 0;
    
    protected:
        std::vector<Texture> m_Textures;
        std::vector<wgpu::Sampler> samplers;
        wgpu::BindGroup MaterialBindGroup;
        IShader* m_Shader = nullptr;
        uint32_t bufferIndex;

        void LoadSampler(int minFilter, int magFilter, WrapMode wrapS, WrapMode wrapT);
};

template<typename MaterialData>
class Material : public IMaterial
{
    using IMaterial::IMaterial;
    private:
        MaterialData data;

    public:
        void UpdateMaterialProperties(const std::any& matData) override
        {
            try {
                const MaterialData& d = std::any_cast<const MaterialData&>(matData);
                // Now update your internal data and GPU buffer(s) as needed:
                data = d;                 // store locally
                m_Shader->UpdateMaterialBuffer(data, bufferIndex);
            }
            catch (const std::bad_any_cast& e) {
                // handle type mismatch (caller passed wrong MaterialData type)
                throw std::runtime_error("Material::UpdateMaterialProperties: bad any_cast - wrong type passed");
            }
        }
        
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