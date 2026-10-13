#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <Texture.hpp>
#include <Renderpass.hpp>

// Size and format of a render graph texture. Unless a fixed width and height are given, the size
// is a fraction of the graph's output size (the screen), so resizing is automatic.
struct GraphTextureDesc
{
    TextureFormat format = TextureFormat::RGBA16Float;
    float scale = 1.0f;
    uint32_t width = 0;  // fixed size when both are non-zero
    uint32_t height = 0;
    bool msaa = false;   // 4 samples; only usable as an attachment of an MSAA pass
};

// One pass of the graph: what it renders into and what it samples, by texture name, plus its
// recording callback. "Backbuffer" is the swapchain image.
struct GraphPassDesc
{
    std::string name;
    std::vector<std::string> colorTargets;
    // MSAA passes: single-sample textures that colorTargets resolve into (same order, may be shorter).
    std::vector<std::string> resolveTargets;
    std::string depthTarget;
    std::vector<std::string> reads;
    // Recorded even when nothing in the graph reads its results (e.g. something read back on the CPU).
    bool keep = false;
    std::function<void(Renderpass&)> record;
};

// Frame graph over named textures. Passes are ordered by what they read and write (insertion order
// otherwise), passes whose results nothing uses are skipped, and the graph owns every texture it
// names. A later writer of a texture loads what an earlier one left; the first one clears it.
// Everything a feature or effect adds can be tagged with an owner and removed again as a whole.
class RenderGraph
{
public:
    using Owner = const void*;
    static constexpr const char* kBackbuffer = "Backbuffer";

    RenderGraph();
    ~RenderGraph();

    void AddTexture(const std::string& name, const GraphTextureDesc& desc, Owner owner = nullptr);
    // What readers get when no pass in the compiled graph writes `name` (e.g. white for missing AO).
    void SetFallback(const std::string& name, Texture* fallback);
    // The pass is created immediately, so shaders can be built against it (its target formats are
    // known from the textures it names, which must be added first).
    Renderpass& AddPass(GraphPassDesc desc, Owner owner = nullptr);
    // Removes every pass and texture added with `owner`, and its texture callbacks.
    void RemoveOwner(Owner owner);

    void SetOutputSize(uint32_t width, uint32_t height);
    void SetBackbufferFormat(TextureFormat format);
    uint32_t OutputWidth() const { return m_Width; }
    uint32_t OutputHeight() const { return m_Height; }

    // Orders and culls the passes and (re)creates textures. Execute calls it when anything changed.
    void Compile();
    // Records every compiled, enabled pass; `backbuffer` is the swapchain view (wgpu::TextureView*).
    void Execute(void* encoder, void* backbuffer);

    // The texture behind a name: the graph's own, or its fallback when nothing writes it.
    Texture& GetTexture(const std::string& name);
    bool HasTexture(const std::string& name) const;
    bool IsWritten(const std::string& name) const;
    const GraphTextureDesc* GetDesc(const std::string& name) const;
    // Pixel size a texture has (or will have) at the current output size.
    void GetSize(const std::string& name, uint32_t& width, uint32_t& height) const;

    // Called after every Compile, so owners can rebuild bind groups that reference graph textures
    // (they may have been recreated, or a name may now resolve to its fallback).
    void OnTexturesChanged(Owner owner, std::function<void()> callback);

    // Names of the compiled passes in execution order, for debugging.
    std::vector<std::string> CompiledPassNames() const;

private:
    struct Resource
    {
        GraphTextureDesc desc;
        std::unique_ptr<Texture> texture; // allocated once, so pointers to it stay valid
        Owner owner = nullptr;
        Texture* fallback = nullptr;
        uint32_t width = 0, height = 0;   // size of the GPU texture currently created
        bool created = false;
        bool written = false;
    };
    struct Pass
    {
        GraphPassDesc desc;
        std::unique_ptr<Renderpass> renderpass;
        Owner owner = nullptr;
    };
    struct Callback
    {
        Owner owner;
        std::function<void()> callback;
    };

    std::unordered_map<std::string, Resource> m_Resources;
    std::vector<std::unique_ptr<Pass>> m_Passes;
    std::vector<Pass*> m_Compiled;
    std::vector<Callback> m_Callbacks;
    uint32_t m_Width = 1;
    uint32_t m_Height = 1;
    TextureFormat m_BackbufferFormat = TextureFormat::BGRA8Unorm;
    bool m_Dirty = true;

    bool IsBackbuffer(const std::string& name) const { return name == kBackbuffer; }
    void SizeOf(const Resource& resource, uint32_t& width, uint32_t& height) const;
};
