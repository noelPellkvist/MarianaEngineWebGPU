#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <glm/glm.hpp>

#include <Mariana/Rendering/RenderGraph.hpp>

class Buffer;
class Shader2;
class Material2;

// Where an effect runs: before lighting (it writes scene textures the lit pass reads, like
// AmbientOcclusion), or in the post-process chain on the lit HDR image.
enum class EffectStage { BeforeLighting, PostProcess };

enum class EffectParamType { Float, Int, Bool, Vec2, Vec3, Vec4, Color3, Color4 };

// A parameter declared by an effect file. Values are packed into the effect's uniform block
// (`params` in its shaders) following WGSL layout rules.
struct EffectParamInfo
{
    std::string name;
    EffectParamType type = EffectParamType::Float;
    glm::vec4 defaultValue{0.0f};
    float min = 0.0f;
    float max = 1.0f;
    uint32_t offset = 0; // in the uniform block
};

// An effect file, parsed: its parameters, private textures and passes. Shared by every instance.
//
// File format (JSON):
//   {
//     "name": "Bloom", "stage": "PostProcess" | "BeforeLighting", "final": false,
//     "parameters": { "intensity": { "type": "float", "default": 1.0, "min": 0, "max": 4 }, ... },
//     "textures":   { "half": { "format": "rgba16float", "scale": 0.5 }, ... },
//     "passes": [ { "name": "...", "shader": "/Shaders/Effects/x.wgsl",
//                   "inputs": { "source": "Input" }, "output": "half", "constants": [1, 0, 0, 0] }, ... ]
//   }
// Texture names in inputs/outputs are the effect's own textures, "Input"/"Output" (the post chain:
// the image coming in and the one going out), or any render graph texture (e.g. "SceneDepth").
// A "final" effect writes the screen (tonemapping) and always runs last.
//
// Each pass shader only writes `fn fragmentMain(in: FullscreenInput) -> @location(0) vec4f`; the
// effect system prepends the declarations: `params` (the parameters), `passInfo` (outputSize: xy
// pixels, zw 1 / pixels; constants: the pass's "constants"), `camera` (the scene camera),
// `linearClamp` (sampler), every input as a texture named after its binding name, and the
// fullscreen vertex shader. `FullscreenInput` has `position` (pixel centre) and `uv`.
struct EffectDefinition
{
    struct Texture
    {
        std::string name;
        GraphTextureDesc desc;
    };
    struct Pass
    {
        std::string name;
        std::string shaderPath;
        std::string source; // the shader file's WGSL
        std::vector<std::pair<std::string, std::string>> inputs; // binding name -> texture name
        std::string output;
        glm::vec4 constants{0.0f};
    };

    std::string name;
    std::string path;
    EffectStage stage = EffectStage::PostProcess;
    bool final = false;
    std::vector<EffectParamInfo> params;
    uint32_t paramsSize = 16;
    std::string paramsWGSL; // the generated `struct EffectParams`
    std::vector<Texture> textures;
    std::vector<Pass> passes;

    // Reads and validates an effect file (path relative to the resources folder, like "/Effects/Bloom.effect").
    static std::shared_ptr<EffectDefinition> Load(const std::string& path, std::string& error);
};

// One effect in use: its definition, current parameter values and enabled state, and the
// pipelines of its passes once it is in a render graph.
class EffectInstance
{
public:
    explicit EffectInstance(std::shared_ptr<const EffectDefinition> definition);
    ~EffectInstance();

    const EffectDefinition& Definition() const { return *m_Definition; }
    const std::string& Name() const { return m_Definition->name; }

    bool enabled = true;

    size_t ParamCount() const { return m_Values.size(); }
    const EffectParamInfo& ParamInfo(size_t index) const { return m_Definition->params[index]; }
    glm::vec4& ParamValue(size_t index) { return m_Values[index]; }
    const glm::vec4& ParamValue(size_t index) const { return m_Values[index]; }
    // By name; false if the effect has no such parameter.
    bool SetParam(const std::string& name, const glm::vec4& value);
    bool GetParam(const std::string& name, glm::vec4& value) const;

    // Adds the passes and private textures to the graph (owned by this instance). For post
    // effects `input`/`output` are what "Input"/"Output" stand for.
    void AddToGraph(RenderGraph& graph, Buffer& cameraBuffer, const std::string& input, const std::string& output);
    // Uploads parameters that changed.
    void Update();

private:
    struct PassGPU;

    std::shared_ptr<const EffectDefinition> m_Definition;
    std::string m_Prefix; // of this instance's private texture names
    std::vector<glm::vec4> m_Values;
    std::vector<glm::vec4> m_WrittenValues;
    std::unique_ptr<Buffer> m_ParamsBuffer;
    std::unique_ptr<Buffer> m_PassInfoBuffer;
    std::vector<std::unique_ptr<PassGPU>> m_Passes;
    RenderGraph* m_Graph = nullptr;

    std::string Resolve(const std::string& name, const std::string& input, const std::string& output) const;
    void BindInputs();
    void WritePassInfo();
};

// The effects applied to the scene, in order: before-lighting effects, then the post-process chain
// from SceneColor to the screen, ending with the final effect (or a plain present when there is none).
// Effects can be added from files, removed, reordered and toggled at any time.
class EffectStack
{
public:
    EffectStack(RenderGraph& graph, Buffer& cameraBuffer);
    ~EffectStack();

    // Loads an effect file and appends it; nullptr (and an error in the log) if it can't be loaded.
    EffectInstance* Add(const std::string& path);
    void Remove(EffectInstance* effect);
    // Removes every effect (the screen then shows SceneColor through the plain present).
    void Clear();
    // Moves an effect `delta` places within its stage.
    void Move(EffectInstance* effect, int delta);
    // The first effect with that name, or nullptr.
    EffectInstance* Find(const std::string& name);

    const std::vector<std::unique_ptr<EffectInstance>>& Effects() const { return m_Effects; }

    // Call after toggling `enabled` (adding, removing and moving already do).
    void MarkDirty() { m_Dirty = true; }
    // Rebuilds the stack's part of the graph when needed and uploads parameters; once per frame.
    void Update();

private:
    RenderGraph& m_Graph;
    Buffer& m_CameraBuffer;
    std::vector<std::unique_ptr<EffectInstance>> m_Effects;
    std::unique_ptr<EffectInstance> m_Present; // used when no final effect is enabled
    std::vector<bool> m_BuiltEnabled;
    bool m_Dirty = true;

    void Rebuild();
};
