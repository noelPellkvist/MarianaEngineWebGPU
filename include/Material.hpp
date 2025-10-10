#pragma once
#include <vector>
#include <string>
#include <memory>
#include <type_traits>
#include <concepts>
#include <any>
#include <stdexcept>
#include <vector>

#include <Shader.hpp>
#include <Texture.hpp>

class IMaterial
{
    public:
        IMaterial(uint32_t index);
        ~IMaterial();

        void InitMaterial(IShader& shader, std::vector<Texture> textures);

        void* GetBindGroup(uint32_t index);

        virtual void UpdateMaterialProperties(const std::any& data) = 0;
    
    protected:
        struct Impl;
        std::vector<Texture> m_Textures;
        IShader* m_Shader = nullptr;
        uint32_t bufferIndex;
        std::unique_ptr<Impl> impl;

        void LoadSampler();
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