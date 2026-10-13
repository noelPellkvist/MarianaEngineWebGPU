#include <Rendering/RenderPipeline.hpp>
#include <ICamera.hpp>
#include <FileReader.hpp>
#include <Logger.hpp>

#include <json.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>

namespace
{
    using Json = nlohmann::ordered_json;

    // Floats written with 6 decimals at most, so 0.1f is saved as 0.1.
    double Clean(float value)
    {
        return std::round(static_cast<double>(value) * 1e6) / 1e6;
    }

    uint32_t ComponentCount(EffectParamType type)
    {
        switch (type)
        {
            case EffectParamType::Vec2: return 2;
            case EffectParamType::Vec3:
            case EffectParamType::Color3: return 3;
            case EffectParamType::Vec4:
            case EffectParamType::Color4: return 4;
            default: return 1;
        }
    }

    // The type is only a guess (from the JSON kind) until the effect it belongs to is loaded.
    bool ReadParameter(const std::string& name, const Json& value, RenderPipelineDesc::Parameter& out)
    {
        out.name = name;
        out.value = glm::vec4(0.0f);
        if (value.is_boolean())
        {
            out.type = EffectParamType::Bool;
            out.value.x = value.get<bool>() ? 1.0f : 0.0f;
            return true;
        }
        if (value.is_number())
        {
            out.type = value.is_number_integer() ? EffectParamType::Int : EffectParamType::Float;
            out.value.x = value.get<float>();
            return true;
        }
        if (value.is_array() && !value.empty() && value.size() <= 4)
        {
            static const EffectParamType vectors[] = { EffectParamType::Float, EffectParamType::Vec2, EffectParamType::Vec3, EffectParamType::Vec4 };
            out.type = vectors[value.size() - 1];
            for (size_t i = 0; i < value.size(); ++i)
            {
                if (!value[i].is_number())
                    return false;
                out.value[static_cast<int>(i)] = value[i].get<float>();
            }
            return true;
        }
        return false;
    }

    Json WriteParameter(const RenderPipelineDesc::Parameter& parameter)
    {
        switch (parameter.type)
        {
            case EffectParamType::Bool: return parameter.value.x != 0.0f;
            case EffectParamType::Int: return static_cast<int>(std::lround(parameter.value.x));
            case EffectParamType::Float: return Clean(parameter.value.x);
            default:
            {
                Json array = Json::array();
                for (uint32_t i = 0; i < ComponentCount(parameter.type); ++i)
                    array.push_back(Clean(parameter.value[static_cast<int>(i)]));
                return array;
            }
        }
    }
}

bool RenderPipelineDesc::Load(const std::string& path, RenderPipelineDesc& out, std::string& error)
{
    out = RenderPipelineDesc{};
    try
    {
        const Json file = Json::parse(FileReader::LoadRawString(path));
        out.name = file.value("name", std::filesystem::path(path).stem().string());
        out.exposure = file.value("exposure", out.exposure);

        if (const auto scene = file.find("scene"); scene != file.end() && scene->is_object())
        {
            out.scene.shadows = scene->value("shadows", out.scene.shadows);
            out.scene.shadowDistance = scene->value("shadowDistance", out.scene.shadowDistance);
            out.scene.shadowMapSize = scene->value("shadowMapSize", out.scene.shadowMapSize);
        }

        if (const auto effects = file.find("effects"); effects != file.end())
        {
            if (!effects->is_array())
            {
                error = path + ": \"effects\" must be an array";
                return false;
            }
            for (const Json& entry : *effects)
            {
                Effect effect;
                effect.path = entry.value("effect", std::string());
                if (effect.path.empty())
                {
                    error = path + ": an effect entry has no \"effect\" file";
                    return false;
                }
                effect.enabled = entry.value("enabled", true);
                if (const auto parameters = entry.find("parameters"); parameters != entry.end() && parameters->is_object())
                {
                    for (const auto& [name, value] : parameters->items())
                    {
                        Parameter parameter;
                        if (ReadParameter(name, value, parameter))
                            effect.parameters.push_back(parameter);
                        else
                            Logger::Warning(path + ": parameter \"" + name + "\" of " + effect.path + " is not a number, boolean or array of up to 4 numbers");
                    }
                }
                out.effects.push_back(std::move(effect));
            }
        }
    }
    catch (const std::exception& e)
    {
        error = path + ": " + e.what();
        return false;
    }
    return true;
}

bool RenderPipelineDesc::Save(const std::string& path, std::string& error) const
{
    Json file;
    file["name"] = name;
    file["exposure"] = Clean(exposure);
    file["scene"] = Json{ { "shadows", scene.shadows }, { "shadowDistance", Clean(scene.shadowDistance) }, { "shadowMapSize", scene.shadowMapSize } };

    Json list = Json::array();
    for (const Effect& effect : effects)
    {
        Json entry;
        entry["effect"] = effect.path;
        entry["enabled"] = effect.enabled;
        Json parameters = Json::object();
        for (const Parameter& parameter : effect.parameters)
            parameters[parameter.name] = WriteParameter(parameter);
        entry["parameters"] = parameters;
        list.push_back(entry);
    }
    file["effects"] = list;

    const std::filesystem::path fullPath = std::string(RESOURCE_DIR) + path;
    std::error_code ec;
    std::filesystem::create_directories(fullPath.parent_path(), ec);
    std::ofstream stream(fullPath, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        error = "Could not write " + fullPath.string();
        return false;
    }
    stream << file.dump(2) << "\n";
    return static_cast<bool>(stream);
}

void RenderPipelineDesc::Apply(EffectStack& stack, SceneRenderer& sceneRenderer, ICamera* camera) const
{
    sceneRenderer.SetSettings(scene);
    if (camera)
        camera->SetExposure(exposure);

    // Effects are loaded anew, so edits to their files show up too.
    stack.Clear();
    for (const Effect& effect : effects)
    {
        EffectInstance* instance = stack.Add(effect.path);
        if (!instance)
            continue;
        instance->enabled = effect.enabled;
        for (const Parameter& parameter : effect.parameters)
            if (!instance->SetParam(parameter.name, parameter.value))
                Logger::Warning("Pipeline \"" + name + "\": " + effect.path + " has no parameter \"" + parameter.name + "\"");
    }
}

RenderPipelineDesc RenderPipelineDesc::Capture(const EffectStack& stack, const SceneRenderer& sceneRenderer, const ICamera* camera)
{
    RenderPipelineDesc desc;
    desc.scene = sceneRenderer.Settings();
    if (camera)
        desc.exposure = camera->Exposure();

    for (const auto& instance : stack.Effects())
    {
        Effect effect;
        effect.path = instance->Definition().path;
        effect.enabled = instance->enabled;
        for (size_t i = 0; i < instance->ParamCount(); ++i)
            effect.parameters.push_back({ instance->ParamInfo(i).name, instance->ParamInfo(i).type, instance->ParamValue(i) });
        desc.effects.push_back(std::move(effect));
    }
    return desc;
}
