#include <Mariana/Rendering/RenderGraph.hpp>
#include <Mariana/Foundation/Logger.hpp>

#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_set>

RenderGraph::RenderGraph() = default;
RenderGraph::~RenderGraph() = default;

void RenderGraph::AddTexture(const std::string& name, const GraphTextureDesc& desc, Owner owner)
{
    if (IsBackbuffer(name))
    {
        Logger::Error("RenderGraph: \"Backbuffer\" is reserved for the swapchain");
        return;
    }
    Resource& resource = m_Resources[name];
    const bool changed = !resource.texture || resource.desc.format != desc.format || resource.desc.msaa != desc.msaa;
    resource.desc = desc;
    resource.owner = owner;
    if (!resource.texture)
        resource.texture = std::make_unique<Texture>();
    if (changed)
        resource.created = false;
    m_Dirty = true;
}

void RenderGraph::SetFallback(const std::string& name, Texture* fallback)
{
    m_Resources[name].fallback = fallback;
    if (!m_Resources[name].texture)
        m_Resources[name].texture = std::make_unique<Texture>();
    m_Dirty = true;
}

Renderpass& RenderGraph::AddPass(GraphPassDesc desc, Owner owner)
{
    // The pass's target layout comes from the textures it names.
    std::vector<TextureFormat> formats;
    bool msaa = false;
    for (const std::string& target : desc.colorTargets)
    {
        if (IsBackbuffer(target))
        {
            formats.push_back(m_BackbufferFormat);
            continue;
        }
        const GraphTextureDesc* textureDesc = GetDesc(target);
        if (!textureDesc)
        {
            Logger::Error("RenderGraph: pass \"" + desc.name + "\" renders into unknown texture \"" + target + "\"");
            formats.push_back(TextureFormat::RGBA8Unorm);
            continue;
        }
        formats.push_back(textureDesc->format);
        msaa = msaa || textureDesc->msaa;
    }
    if (!desc.depthTarget.empty())
    {
        if (const GraphTextureDesc* depthDesc = GetDesc(desc.depthTarget))
            msaa = msaa || depthDesc->msaa;
        else
            Logger::Error("RenderGraph: pass \"" + desc.name + "\" uses unknown depth texture \"" + desc.depthTarget + "\"");
    }

    auto pass = std::make_unique<Pass>();
    pass->renderpass = std::make_unique<Renderpass>(msaa, !desc.depthTarget.empty(), formats, m_Width, m_Height);
    pass->desc = std::move(desc);
    pass->owner = owner;
    Renderpass& renderpass = *pass->renderpass;
    m_Passes.push_back(std::move(pass));
    m_Dirty = true;
    return renderpass;
}

void RenderGraph::RemoveOwner(Owner owner)
{
    m_Passes.erase(std::remove_if(m_Passes.begin(), m_Passes.end(), [owner](const std::unique_ptr<Pass>& pass) { return pass->owner == owner; }), m_Passes.end());
    for (auto it = m_Resources.begin(); it != m_Resources.end();)
        it = it->second.owner == owner ? m_Resources.erase(it) : std::next(it);
    m_Callbacks.erase(std::remove_if(m_Callbacks.begin(), m_Callbacks.end(), [owner](const Callback& c) { return c.owner == owner; }), m_Callbacks.end());
    m_Compiled.clear();
    m_Dirty = true;
}

void RenderGraph::SetOutputSize(uint32_t width, uint32_t height)
{
    width = std::max(width, 1u);
    height = std::max(height, 1u);
    if (width == m_Width && height == m_Height)
        return;
    m_Width = width;
    m_Height = height;
    m_Dirty = true;
}

void RenderGraph::SetBackbufferFormat(TextureFormat format)
{
    if (format == m_BackbufferFormat)
        return;
    m_BackbufferFormat = format;
    m_Dirty = true;
}

void RenderGraph::SizeOf(const Resource& resource, uint32_t& width, uint32_t& height) const
{
    if (resource.desc.width > 0 && resource.desc.height > 0)
    {
        width = resource.desc.width;
        height = resource.desc.height;
        return;
    }
    width = std::max(1u, static_cast<uint32_t>(std::floor(m_Width * resource.desc.scale)));
    height = std::max(1u, static_cast<uint32_t>(std::floor(m_Height * resource.desc.scale)));
}

void RenderGraph::GetSize(const std::string& name, uint32_t& width, uint32_t& height) const
{
    if (IsBackbuffer(name))
    {
        width = m_Width;
        height = m_Height;
        return;
    }
    auto it = m_Resources.find(name);
    if (it == m_Resources.end())
    {
        width = height = 1;
        return;
    }
    SizeOf(it->second, width, height);
}

void RenderGraph::Compile()
{
    m_Dirty = false;
    const size_t passCount = m_Passes.size();

    // Who writes each name, in insertion order. Resolve targets count as written.
    std::unordered_map<std::string, std::vector<size_t>> writers;
    for (size_t p = 0; p < passCount; ++p)
    {
        const GraphPassDesc& desc = m_Passes[p]->desc;
        for (const std::string& name : desc.colorTargets) writers[name].push_back(p);
        for (const std::string& name : desc.resolveTargets) writers[name].push_back(p);
        if (!desc.depthTarget.empty()) writers[desc.depthTarget].push_back(p);
    }

    // A pass depends on every writer of what it reads, and on earlier writers of what it writes
    // (it continues their work).
    std::vector<std::vector<size_t>> dependencies(passCount);
    for (size_t p = 0; p < passCount; ++p)
    {
        const GraphPassDesc& desc = m_Passes[p]->desc;
        auto dependOn = [&](const std::string& name, bool earlierOnly) {
            auto it = writers.find(name);
            if (it == writers.end())
                return;
            for (size_t w : it->second)
                if (w != p && (!earlierOnly || w < p))
                    dependencies[p].push_back(w);
        };
        for (const std::string& name : desc.reads) dependOn(name, false);
        for (const std::string& name : desc.colorTargets) dependOn(name, true);
        if (!desc.depthTarget.empty()) dependOn(desc.depthTarget, true);
    }

    // Keep what leads to the backbuffer or is marked keep.
    std::vector<bool> needed(passCount, false);
    std::vector<size_t> work;
    for (size_t p = 0; p < passCount; ++p)
    {
        const GraphPassDesc& desc = m_Passes[p]->desc;
        const bool presents = std::find(desc.colorTargets.begin(), desc.colorTargets.end(), kBackbuffer) != desc.colorTargets.end();
        if (desc.keep || presents)
        {
            needed[p] = true;
            work.push_back(p);
        }
    }
    while (!work.empty())
    {
        const size_t p = work.back();
        work.pop_back();
        for (size_t d : dependencies[p])
        {
            if (!needed[d])
            {
                needed[d] = true;
                work.push_back(d);
            }
        }
    }

    // Topological order, preferring insertion order among passes that are ready.
    std::vector<size_t> remaining(passCount, 0);
    std::vector<std::vector<size_t>> dependents(passCount);
    for (size_t p = 0; p < passCount; ++p)
    {
        if (!needed[p])
            continue;
        std::sort(dependencies[p].begin(), dependencies[p].end());
        dependencies[p].erase(std::unique(dependencies[p].begin(), dependencies[p].end()), dependencies[p].end());
        for (size_t d : dependencies[p])
        {
            ++remaining[p];
            dependents[d].push_back(p);
        }
    }
    std::priority_queue<size_t, std::vector<size_t>, std::greater<size_t>> ready;
    for (size_t p = 0; p < passCount; ++p)
        if (needed[p] && remaining[p] == 0)
            ready.push(p);

    m_Compiled.clear();
    while (!ready.empty())
    {
        const size_t p = ready.top();
        ready.pop();
        m_Compiled.push_back(m_Passes[p].get());
        for (size_t next : dependents[p])
            if (--remaining[next] == 0)
                ready.push(next);
    }
    size_t neededCount = std::count(needed.begin(), needed.end(), true);
    if (m_Compiled.size() != neededCount)
    {
        Logger::Error("RenderGraph: passes depend on each other in a cycle; running them in insertion order");
        m_Compiled.clear();
        for (size_t p = 0; p < passCount; ++p)
            if (needed[p])
                m_Compiled.push_back(m_Passes[p].get());
    }

    // Create every declared texture at its current size (shaders may bind them before any pass
    // that uses them is compiled in), except names that resolve to their fallback.
    for (auto& [name, resource] : m_Resources)
    {
        bool written = false;
        if (auto it = writers.find(name); it != writers.end())
            for (size_t w : it->second)
                written = written || needed[w];
        resource.written = written;

        if (!resource.texture || (!written && resource.fallback))
            continue;
        uint32_t width = 0, height = 0;
        SizeOf(resource, width, height);
        if (resource.created && width == resource.width && height == resource.height)
            continue;

        const bool depth = resource.desc.format == TextureFormat::Depth24Plus || resource.desc.format == TextureFormat::Depth32Float;
        if (depth)
            resource.texture->CreateDepthTexture(resource.desc.format, width, height, resource.desc.msaa);
        else
            resource.texture->CreateRenderTexture(resource.desc.format, width, height, resource.desc.msaa);
        resource.width = width;
        resource.height = height;
        resource.created = true;
    }

    // Attach the textures, and load what an earlier pass in this frame already rendered.
    std::unordered_set<std::string> touched;
    for (Pass* pass : m_Compiled)
    {
        const GraphPassDesc& desc = pass->desc;
        std::vector<Texture*> colors;
        std::vector<bool> load;
        for (const std::string& name : desc.colorTargets)
        {
            colors.push_back(IsBackbuffer(name) ? nullptr : &GetTexture(name));
            load.push_back(!touched.insert(name).second);
        }
        std::vector<Texture*> resolves;
        for (const std::string& name : desc.resolveTargets)
        {
            resolves.push_back(&GetTexture(name));
            touched.insert(name);
        }
        Texture* depth = nullptr;
        bool loadDepth = false;
        if (!desc.depthTarget.empty())
        {
            depth = &GetTexture(desc.depthTarget);
            loadDepth = !touched.insert(desc.depthTarget).second;
        }
        uint32_t width = m_Width, height = m_Height;
        if (!desc.colorTargets.empty())
            GetSize(desc.colorTargets[0], width, height);
        else if (!desc.depthTarget.empty())
            GetSize(desc.depthTarget, width, height);
        pass->renderpass->SetTargets(colors, resolves, depth, width, height);
        pass->renderpass->SetLoadOps(load, loadDepth);
        pass->renderpass->recordCallback = pass->desc.record
            ? [pass]() { pass->desc.record(*pass->renderpass); }
            : std::function<void()>();
    }

    // Bind groups referencing graph textures are rebuilt after every compile: textures may have been
    // recreated, and names may now resolve to their fallback (or stop doing so).
    for (const Callback& c : m_Callbacks)
        c.callback();
}

void RenderGraph::Execute(void* encoder, void* backbuffer)
{
    if (m_Dirty)
        Compile();

    for (Pass* pass : m_Compiled)
    {
        // A disabled pass keeps what it rendered last time (e.g. an unchanged shadow map).
        if (!pass->renderpass->IsEnabled())
            continue;
        pass->renderpass->Start(encoder, backbuffer);
    }
}

Texture& RenderGraph::GetTexture(const std::string& name)
{
    Resource& resource = m_Resources[name];
    if (!resource.texture)
        resource.texture = std::make_unique<Texture>();
    if (!resource.written && resource.fallback)
        return *resource.fallback;
    return *resource.texture;
}

bool RenderGraph::HasTexture(const std::string& name) const
{
    return IsBackbuffer(name) || m_Resources.count(name) != 0;
}

bool RenderGraph::IsWritten(const std::string& name) const
{
    auto it = m_Resources.find(name);
    return it != m_Resources.end() && it->second.written;
}

const GraphTextureDesc* RenderGraph::GetDesc(const std::string& name) const
{
    auto it = m_Resources.find(name);
    return it == m_Resources.end() ? nullptr : &it->second.desc;
}

void RenderGraph::OnTexturesChanged(Owner owner, std::function<void()> callback)
{
    m_Callbacks.push_back({ owner, std::move(callback) });
}

std::vector<std::string> RenderGraph::CompiledPassNames() const
{
    std::vector<std::string> names;
    for (const Pass* pass : m_Compiled)
        names.push_back(pass->desc.name);
    return names;
}
