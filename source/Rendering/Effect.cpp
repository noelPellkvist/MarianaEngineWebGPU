#include <Rendering/Effect.hpp>
#include <Buffer.hpp>
#include <Shader.hpp>
#include <Material.hpp>
#include <FileReader.hpp>
#include <Logger.hpp>
#include <UniformLayout.hpp>
#include <json.hpp>

#include <algorithm>
#include <cstring>
#include <filesystem>

namespace
{
    using Json = nlohmann::ordered_json;

    // Per-pass constants (passInfo in effect shaders): one dynamic slot per pass.
    struct EffectPassInfo
    {
        glm::vec4 outputSize{0.0f}; // xy: pixels, zw: 1 / pixels
        glm::vec4 constants{0.0f};
    };

    struct ParamType
    {
        const char* name;
        const char* wgsl;
        uint32_t size;
        uint32_t align;
        EffectParamType type;
    };

    const ParamType kParamTypes[] = {
        { "float",  "f32",   4,  4, EffectParamType::Float },
        { "int",    "i32",   4,  4, EffectParamType::Int },
        { "bool",   "u32",   4,  4, EffectParamType::Bool },
        { "vec2",   "vec2f", 8,  8, EffectParamType::Vec2 },
        { "vec3",   "vec3f", 12, 16, EffectParamType::Vec3 },
        { "vec4",   "vec4f", 16, 16, EffectParamType::Vec4 },
        { "color3", "vec3f", 12, 16, EffectParamType::Color3 },
        { "color4", "vec4f", 16, 16, EffectParamType::Color4 },
    };

    const ParamType& TypeOf(EffectParamType type)
    {
        for (const ParamType& t : kParamTypes)
            if (t.type == type)
                return t;
        return kParamTypes[0];
    }

    bool ParseFormat(const std::string& name, TextureFormat& format)
    {
        static const std::pair<const char*, TextureFormat> kFormats[] = {
            { "r8unorm", TextureFormat::R8Unorm },
            { "r16float", TextureFormat::R16Float },
            { "r32float", TextureFormat::R32Float },
            { "rg16float", TextureFormat::RG16Float },
            { "rgba8unorm", TextureFormat::RGBA8Unorm },
            { "rgba16float", TextureFormat::RGBA16Float },
            { "rgba32float", TextureFormat::RGBA32Float },
            { "rgb10a2unorm", TextureFormat::RGB10A2Unorm },
        };
        for (const auto& [n, f] : kFormats)
        {
            if (name == n)
            {
                format = f;
                return true;
            }
        }
        return false;
    }

    // A number or an array of up to four numbers.
    glm::vec4 ReadVector(const Json& value)
    {
        glm::vec4 v(0.0f);
        if (value.is_number())
            v.x = value.get<float>();
        else if (value.is_boolean())
            v.x = value.get<bool>() ? 1.0f : 0.0f;
        else if (value.is_array())
            for (size_t i = 0; i < value.size() && i < 4; ++i)
                v[static_cast<int>(i)] = value[i].get<float>();
        return v;
    }

    bool IsDepth(TextureFormat format)
    {
        return format == TextureFormat::Depth24Plus || format == TextureFormat::Depth32Float;
    }

    const char* kCameraWGSL = R"(struct CameraInfoData {
    projection: mat4x4<f32>,
    view: mat4x4<f32>,
    viewProj: mat4x4<f32>,
    invView: mat4x4<f32>,
    invProj: mat4x4<f32>,
    invViewProj: mat4x4<f32>,
    position: vec3<f32>,
    exposure: f32,
};
)";

    const char* kFullscreenWGSL = R"(struct PassInfo {
    outputSize: vec4f, // xy: pixels, zw: 1 / pixels
    constants: vec4f,
};
struct FullscreenInput {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
};
@group(0) @binding(0) var<uniform> params: EffectParams;
@group(0) @binding(1) var<uniform> passInfo: PassInfo;
@group(0) @binding(2) var<uniform> camera: CameraInfoData;
@group(0) @binding(3) var linearClamp: sampler;

// One triangle that covers the whole target.
@vertex
fn vertexMain(@builtin(vertex_index) index: u32) -> FullscreenInput {
    let p = vec2f(f32((index << 1u) & 2u), f32(index & 2u));
    var out: FullscreenInput;
    out.position = vec4f(p * 2.0 - 1.0, 0.0, 1.0);
    out.uv = vec2f(p.x, 1.0 - p.y);
    return out;
}
)";
}

#pragma region EffectDefinition

std::shared_ptr<EffectDefinition> EffectDefinition::Load(const std::string& path, std::string& error)
{
    auto definition = std::make_shared<EffectDefinition>();
    definition->path = path;

    try
    {
        const Json file = Json::parse(FileReader::LoadRawString(path));
        definition->name = file.value("name", std::filesystem::path(path).stem().string());

        const std::string stage = file.value("stage", "PostProcess");
        if (stage == "BeforeLighting")
            definition->stage = EffectStage::BeforeLighting;
        else if (stage != "PostProcess")
            throw std::runtime_error("unknown stage \"" + stage + "\"");
        definition->final = file.value("final", false);
        if (definition->final && definition->stage != EffectStage::PostProcess)
            throw std::runtime_error("only post-process effects can be final");

        // Parameters, packed in file order with WGSL uniform layout rules.
        std::string fields;
        uint32_t offset = 0;
        if (file.contains("parameters"))
        {
            for (const auto& [name, desc] : file["parameters"].items())
            {
                const std::string typeName = desc.value("type", "float");
                const ParamType* type = nullptr;
                for (const ParamType& t : kParamTypes)
                    if (typeName == t.name)
                        type = &t;
                if (!type)
                    throw std::runtime_error("parameter \"" + name + "\" has unknown type \"" + typeName + "\"");

                EffectParamInfo param;
                param.name = name;
                param.type = type->type;
                if (desc.contains("default"))
                    param.defaultValue = ReadVector(desc["default"]);
                param.min = desc.value("min", 0.0f);
                param.max = desc.value("max", 1.0f);
                offset = (offset + type->align - 1) / type->align * type->align;
                param.offset = offset;
                offset += type->size;
                definition->params.push_back(param);
                fields += "    " + name + ": " + type->wgsl + ",\n";
            }
        }
        definition->paramsSize = std::max(16u, (offset + 15u) / 16u * 16u);
        if (fields.empty())
            fields = "    _unused: vec4f,\n";
        definition->paramsWGSL = "struct EffectParams {\n" + fields + "};\n";

        if (file.contains("textures"))
        {
            for (const auto& [name, desc] : file["textures"].items())
            {
                EffectDefinition::Texture texture;
                texture.name = name;
                if (!ParseFormat(desc.value("format", "rgba16float"), texture.desc.format))
                    throw std::runtime_error("texture \"" + name + "\" has an unknown format");
                texture.desc.scale = desc.value("scale", 1.0f);
                texture.desc.width = desc.value("width", 0u);
                texture.desc.height = desc.value("height", 0u);
                definition->textures.push_back(texture);
            }
        }

        for (const Json& desc : file.at("passes"))
        {
            EffectDefinition::Pass pass;
            pass.name = desc.value("name", "Pass " + std::to_string(definition->passes.size()));
            pass.shaderPath = desc.at("shader").get<std::string>();
            pass.source = FileReader::LoadRawString(pass.shaderPath);
            if (desc.contains("inputs"))
                for (const auto& [binding, texture] : desc["inputs"].items())
                    pass.inputs.emplace_back(binding, texture.get<std::string>());
            pass.output = desc.at("output").get<std::string>();
            if (desc.contains("constants"))
                pass.constants = ReadVector(desc["constants"]);
            definition->passes.push_back(std::move(pass));
        }
        if (definition->passes.empty())
            throw std::runtime_error("an effect needs at least one pass");
    }
    catch (const std::exception& e)
    {
        error = path + ": " + e.what();
        return nullptr;
    }
    return definition;
}

#pragma endregion

#pragma region EffectInstance

struct EffectInstance::PassGPU
{
    std::unique_ptr<Shader2> shader;
    Material2 material;
    std::vector<std::string> inputs; // resolved graph texture names, in binding order
    std::string output;
    std::string formatKey;           // target and input formats the pipeline was built for
};

EffectInstance::EffectInstance(std::shared_ptr<const EffectDefinition> definition)
    : m_Definition(std::move(definition))
{
    // Private textures are named per instance, so an effect can be in a stack more than once.
    static uint32_t s_Instances = 0;
    m_Prefix = m_Definition->name + "#" + std::to_string(++s_Instances) + "/";

    for (const EffectParamInfo& param : m_Definition->params)
        m_Values.push_back(param.defaultValue);

    m_ParamsBuffer = std::make_unique<Buffer>(RawBufferLayout{ m_Definition->paramsSize, BufferType::Uniform });
    m_ParamsBuffer->Build();
    EffectPassInfo info{};
    m_PassInfoBuffer = std::make_unique<Buffer>(UniformBufferLayout(true, info, info.outputSize, info.constants));
    m_PassInfoBuffer->Build();
}

EffectInstance::~EffectInstance()
{
    if (m_Graph)
        m_Graph->RemoveOwner(this);
}

bool EffectInstance::SetParam(const std::string& name, const glm::vec4& value)
{
    for (size_t i = 0; i < m_Values.size(); ++i)
    {
        if (m_Definition->params[i].name == name)
        {
            m_Values[i] = value;
            return true;
        }
    }
    return false;
}

bool EffectInstance::GetParam(const std::string& name, glm::vec4& value) const
{
    for (size_t i = 0; i < m_Values.size(); ++i)
    {
        if (m_Definition->params[i].name == name)
        {
            value = m_Values[i];
            return true;
        }
    }
    return false;
}

std::string EffectInstance::Resolve(const std::string& name, const std::string& input, const std::string& output) const
{
    if (name == "Input")
        return input;
    if (name == "Output")
        return output;
    for (const EffectDefinition::Texture& texture : m_Definition->textures)
        if (texture.name == name)
            return m_Prefix + name;
    return name;
}

void EffectInstance::AddToGraph(RenderGraph& graph, Buffer& cameraBuffer, const std::string& input, const std::string& output)
{
    m_Graph = &graph;
    const EffectDefinition& definition = *m_Definition;

    for (const EffectDefinition::Texture& texture : definition.textures)
        graph.AddTexture(m_Prefix + texture.name, texture.desc, this);

    m_Passes.resize(definition.passes.size());
    for (size_t p = 0; p < definition.passes.size(); ++p)
    {
        const EffectDefinition::Pass& pass = definition.passes[p];
        if (!m_Passes[p])
            m_Passes[p] = std::make_unique<PassGPU>();
        PassGPU& gpu = *m_Passes[p];

        gpu.output = Resolve(pass.output, input, output);
        gpu.inputs.clear();
        GraphPassDesc desc;
        desc.name = definition.name + ": " + pass.name;
        desc.colorTargets = { gpu.output };
        std::string formatKey = gpu.output == RenderGraph::kBackbuffer ? "screen" : std::to_string(static_cast<uint32_t>(graph.GetDesc(gpu.output) ? graph.GetDesc(gpu.output)->format : TextureFormat::Undefined));
        for (const auto& [binding, texture] : pass.inputs)
        {
            const std::string resolved = Resolve(texture, input, output);
            gpu.inputs.push_back(resolved);
            desc.reads.push_back(resolved);
            const GraphTextureDesc* inputDesc = graph.GetDesc(resolved);
            formatKey += "," + std::string(inputDesc && IsDepth(inputDesc->format) ? "d" : "c");
        }
        const uint32_t slot = static_cast<uint32_t>(p);
        PassGPU* gpuPtr = &gpu;
        desc.record = [gpuPtr, slot](Renderpass& renderpass) {
            renderpass.SetShader2(*gpuPtr->shader);
            renderpass.SetBufferIndex("PassInfo", slot);
            if (!gpuPtr->inputs.empty())
                renderpass.SetMaterial2(*gpuPtr->shader, gpuPtr->material);
            renderpass.DrawFullscreen();
        };
        Renderpass& renderpass = graph.AddPass(desc, this);

        // Pipelines only depend on the formats involved, so they survive re-adding the effect.
        if (gpu.shader && gpu.formatKey == formatKey)
            continue;
        gpu.formatKey = formatKey;
        gpu.shader = std::make_unique<Shader2>(desc.name.c_str());
        gpu.shader->Group(0)
            .AddBuffer("EffectParams", 0, *m_ParamsBuffer)
            .AddBuffer("PassInfo", 1, *m_PassInfoBuffer)
            .AddBuffer("CameraInfo", 2, cameraBuffer)
            .AddSampler("linearClamp", 3, SamplerType_LinearClamp);

        std::string inputsWGSL;
        for (size_t i = 0; i < pass.inputs.size(); ++i)
        {
            const GraphTextureDesc* inputDesc = graph.GetDesc(gpu.inputs[i]);
            const bool depth = inputDesc && IsDepth(inputDesc->format);
            gpu.shader->Group(1).AddTexture(pass.inputs[i].first.c_str(), static_cast<uint32_t>(i), depth ? TextureType_Depth : TextureType_2D);
            inputsWGSL += "@group(1) @binding(" + std::to_string(i) + ") var " + pass.inputs[i].first + (depth ? ": texture_depth_2d;\n" : ": texture_2d<f32>;\n");
        }
        if (!pass.inputs.empty())
            gpu.shader->SetMaterialGroup(1);

        gpu.shader->SetWGSL("// ---- generated by the effect system (Effect.hpp) ----\n" + std::string(kCameraWGSL) + definition.paramsWGSL +
                            kFullscreenWGSL + inputsWGSL + "// ---- " + pass.shaderPath + " ----\n" + pass.source);
        gpu.shader->SetRenderpass(&renderpass);
        gpu.shader->Build();
        if (!pass.inputs.empty())
            gpu.material.InitFromShader(*gpu.shader);
    }

    graph.OnTexturesChanged(this, [this]() {
        BindInputs();
        WritePassInfo();
    });
    m_WrittenValues.clear(); // upload the parameters once more
}

void EffectInstance::BindInputs()
{
    const EffectDefinition& definition = *m_Definition;
    for (size_t p = 0; p < m_Passes.size(); ++p)
    {
        PassGPU& gpu = *m_Passes[p];
        if (gpu.inputs.empty())
            continue;
        for (size_t i = 0; i < gpu.inputs.size(); ++i)
            gpu.material.SetTexture(definition.passes[p].inputs[i].first, m_Graph->GetTexture(gpu.inputs[i]));
        gpu.material.Build();
    }
}

void EffectInstance::WritePassInfo()
{
    for (size_t p = 0; p < m_Passes.size(); ++p)
    {
        uint32_t width = 1, height = 1;
        m_Graph->GetSize(m_Passes[p]->output, width, height);
        EffectPassInfo info;
        info.outputSize = glm::vec4(width, height, 1.0f / width, 1.0f / height);
        info.constants = m_Definition->passes[p].constants;
        m_PassInfoBuffer->Write(info, p);
    }
}

void EffectInstance::Update()
{
    if (m_Values == m_WrittenValues)
        return;

    std::vector<uint8_t> data(m_Definition->paramsSize, 0);
    for (size_t i = 0; i < m_Values.size(); ++i)
    {
        const EffectParamInfo& param = m_Definition->params[i];
        const glm::vec4& v = m_Values[i];
        uint8_t* dst = data.data() + param.offset;
        switch (param.type)
        {
            case EffectParamType::Int:  { const int32_t x = static_cast<int32_t>(v.x); std::memcpy(dst, &x, 4); break; }
            case EffectParamType::Bool: { const uint32_t x = v.x != 0.0f ? 1u : 0u; std::memcpy(dst, &x, 4); break; }
            default: std::memcpy(dst, &v, TypeOf(param.type).size); break;
        }
    }
    m_ParamsBuffer->WriteRawBytes(data.data(), data.size(), 0);
    m_WrittenValues = m_Values;
}

#pragma endregion

#pragma region EffectStack

EffectStack::EffectStack(RenderGraph& graph, Buffer& cameraBuffer)
    : m_Graph(graph), m_CameraBuffer(cameraBuffer)
{
}

EffectStack::~EffectStack()
{
    m_Graph.RemoveOwner(this);
}

EffectInstance* EffectStack::Add(const std::string& path)
{
    std::string error;
    std::shared_ptr<EffectDefinition> definition = EffectDefinition::Load(path, error);
    if (!definition)
    {
        Logger::Error("Could not load effect " + error);
        return nullptr;
    }
    m_Effects.push_back(std::make_unique<EffectInstance>(std::move(definition)));
    m_Dirty = true;
    return m_Effects.back().get();
}

void EffectStack::Remove(EffectInstance* effect)
{
    m_Effects.erase(std::remove_if(m_Effects.begin(), m_Effects.end(), [effect](const std::unique_ptr<EffectInstance>& e) { return e.get() == effect; }), m_Effects.end());
    m_Dirty = true;
}

void EffectStack::Move(EffectInstance* effect, int delta)
{
    auto it = std::find_if(m_Effects.begin(), m_Effects.end(), [effect](const std::unique_ptr<EffectInstance>& e) { return e.get() == effect; });
    if (it == m_Effects.end())
        return;
    const EffectStage stage = effect->Definition().stage;
    // Step over effects of other stages: order only matters within one.
    size_t index = static_cast<size_t>(it - m_Effects.begin());
    while (delta != 0)
    {
        const int step = delta > 0 ? 1 : -1;
        size_t other = index;
        do
        {
            if ((step < 0 && other == 0) || (step > 0 && other + 1 >= m_Effects.size()))
                return;
            other += step;
        } while (m_Effects[other]->Definition().stage != stage);
        std::swap(m_Effects[index], m_Effects[other]);
        index = other;
        delta -= step;
        m_Dirty = true;
    }
}

EffectInstance* EffectStack::Find(const std::string& name)
{
    for (auto& effect : m_Effects)
        if (effect->Name() == name)
            return effect.get();
    return nullptr;
}

void EffectStack::Rebuild()
{
    m_Dirty = false;
    m_Graph.RemoveOwner(this);
    for (auto& effect : m_Effects)
        m_Graph.RemoveOwner(effect.get());
    if (m_Present)
        m_Graph.RemoveOwner(m_Present.get());

    m_BuiltEnabled.clear();
    for (auto& effect : m_Effects)
        m_BuiltEnabled.push_back(effect->enabled);

    for (auto& effect : m_Effects)
        if (effect->enabled && effect->Definition().stage == EffectStage::BeforeLighting)
            effect->AddToGraph(m_Graph, m_CameraBuffer, "", "");

    // The post chain: each effect reads the previous one's result; the final one writes the screen.
    EffectInstance* final = nullptr;
    std::vector<EffectInstance*> chain;
    for (auto& effect : m_Effects)
    {
        if (!effect->enabled || effect->Definition().stage != EffectStage::PostProcess)
            continue;
        if (effect->Definition().final)
            final = final ? final : effect.get();
        else
            chain.push_back(effect.get());
    }
    if (!final)
    {
        if (!m_Present)
        {
            std::string error;
            if (std::shared_ptr<EffectDefinition> present = EffectDefinition::Load("/Effects/Present.effect", error))
                m_Present = std::make_unique<EffectInstance>(std::move(present));
            else
                Logger::Error("Could not load the present effect " + error);
        }
        final = m_Present.get();
    }

    std::string current = "SceneColor";
    for (size_t i = 0; i < chain.size(); ++i)
    {
        const std::string next = "PostColor" + std::to_string(i);
        m_Graph.AddTexture(next, GraphTextureDesc{ TextureFormat::RGBA16Float, 1.0f }, this);
        chain[i]->AddToGraph(m_Graph, m_CameraBuffer, current, next);
        current = next;
    }
    if (final)
        final->AddToGraph(m_Graph, m_CameraBuffer, current, RenderGraph::kBackbuffer);
}

void EffectStack::Update()
{
    if (m_BuiltEnabled.size() != m_Effects.size())
        m_Dirty = true;
    for (size_t i = 0; i < m_Effects.size() && !m_Dirty; ++i)
        m_Dirty = m_Effects[i]->enabled != m_BuiltEnabled[i];
    if (m_Dirty)
        Rebuild();

    for (auto& effect : m_Effects)
        if (effect->enabled)
            effect->Update();
    if (m_Present)
        m_Present->Update();
}

#pragma endregion
