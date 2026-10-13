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

class Renderpass;
class Material2
{
    friend class Renderpass;
    public:
        Material2();
        ~Material2();

        Material2& InitFromShader(Shader2& shader);

        Material2& Build();

        Material2& SetTexture(std::string name, Texture& texture);
        // Overrides the sampler's type defaults for this material only.
        Material2& SetSampler(std::string name, const SamplerSettings& settings);

    private:
        BindGroup m_Bindgroup;
        Shader2* m_Shader = nullptr;
        std::unordered_map<std::string, ShaderResource*> m_ResourceMap;
        void* GetBindGroup();
};