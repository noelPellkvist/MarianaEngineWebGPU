#include <Mariana/Importers/SceneFile.hpp>
#include <Mariana/Importers/ImageImporter.hpp>
#include <Mariana/Importers/MaterialFile.hpp>
#include <Mariana/Animation/AnimationGraph.hpp>
#include <Mariana/Animation/Animator.hpp>
#include <Mariana/Animation/Skeleton.hpp>
#include <Mariana/Animation/Skin.hpp>
#include <Mariana/Foundation/FileSystem.hpp>
#include <Mariana/Foundation/Logger.hpp>
#include <Mariana/Rendering/BuiltinAssets.hpp>
#include <Mariana/Rendering/Components.hpp>
#include <Mariana/Rendering/MaterialAsset.hpp>
#include <Mariana/Scene/Prefab.hpp>

#include "Importers/MaterialJson.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

using MaterialJson::Json;

// What one Save or Load works with besides the JSON.
struct SceneArchive::Context
{
    AssetManager2* assets = nullptr;
    // Saving: materials made in memory, written into the file, and every file the scene uses.
    std::vector<const MaterialAsset*> materials;
    std::vector<AssetID> materialIds;
    std::unordered_map<const MaterialAsset*, int> materialIndex;
    std::set<std::string> dependencies;
    std::vector<AssetID> graphs; // animation graphs used (their clips are dependencies too)
    // Materials a model file holds that changed since it loaded, and how the scene refers to them.
    std::vector<std::pair<AssetID, std::string>> overrides;
    // Loading: the file's own materials, and the files loaded for what they hold (kept while loading).
    std::vector<AssetHandle<MaterialAsset>> loadedMaterials;
    std::vector<AssetHandle<Prefab>> keep;
    std::set<std::string> warnings;

    void Warn(const std::string& warning) { warnings.insert(warning); }
};

namespace
{
    using Serializer = SceneFile::detail::Serializer;

    std::vector<Serializer>& Registry()
    {
        static std::vector<Serializer> serializers;
        return serializers;
    }

    // What isn't saved as a component: the name (saved apart), and runtime components (rebuilt).
    bool Derived(const std::string& name)
    {
        return name == "NameComponent" || ECS::IsRuntimeComponent(name.c_str());
    }

    // The .material file a material is kept in (its own, or the one it was extracted or saved to).
    std::filesystem::path MaterialFileOf(const AssetMetadata& meta)
    {
        if (meta.path.extension() == ".material")
            return meta.path;
        if (meta.source.extension() == ".material")
            return meta.source;
        return {};
    }

    const Serializer* FindSerializer(std::string_view name)
    {
        for (const Serializer& s : Registry())
            if (s.name == name)
                return &s;
        return nullptr;
    }

    void RegisterEngineComponents();

    void EnsureEngineComponents()
    {
        static bool registered = false;
        if (registered)
            return;
        registered = true;
        RegisterEngineComponents();
    }

    void RegisterEngineComponents()
    {
        SceneFile::RegisterComponent<LocalTRS>("LocalTRS", [](SceneArchive& a, LocalTRS& t) {
            a.Field("position", t.pos, 3);
            a.Field("rotation", t.rot_quat, 4); // x, y, z, w
            a.Field("scale", t.scl, 3);
            if (a.IsLoading())
                ++t.version;
        });
        SceneFile::RegisterComponent<MeshComponent>("MeshComponent", [](SceneArchive& a, MeshComponent& c) {
            a.Asset("mesh", c.mesh);
            a.Assets("materials", c.materials);
        });
        SceneFile::RegisterTag<ShadowCasterTag>("ShadowCasterTag");
        SceneFile::RegisterComponent<CameraComponent>("CameraComponent", [](SceneArchive& a, CameraComponent& c) {
            a.Field("fov", c.fov);
            a.Field("near", c.nearClip);
            a.Field("far", c.farClip);
        });
        SceneFile::RegisterComponent<AnimationTargetEntity>("AnimationTargetEntity", [](SceneArchive& a, AnimationTargetEntity& t) {
            a.Field("node", t.nodeIndex);
        });
        SceneFile::RegisterComponent<SkinComponent>("SkinComponent", [](SceneArchive& a, SkinComponent& s) {
            a.Asset("skin", s.skin);
        });
        SceneFile::RegisterComponent<Animator>("Animator", [](SceneArchive& a, Animator& animator) {
            a.Asset("skeleton", animator.skeleton);
            a.List("clips", animator.clips, [](SceneArchive& e, NamedClip& clip) {
                e.Field("name", clip.name);
                e.Asset("clip", clip.clip);
                if (e.IsLoading() || !clip.events.empty())
                    e.List("events", clip.events, [](SceneArchive& ev, AnimationEvent& event) {
                        ev.Field("time", event.time);
                        ev.Field("name", event.name);
                    });
            });
            a.Asset("graph", animator.graph);
            // The graph's parameters where they differ from its defaults (triggers aren't kept).
            struct SavedParameter
            {
                std::string name;
                float value = 0.0f;
                glm::vec3 vector{ 0.0f };
            };
            const AnimationGraph* graph = animator.graph.Get().get();
            std::vector<SavedParameter> parameters;
            if (graph && !a.IsLoading() && a.ForFile())
            {
                for (const AnimationParameter& p : graph->parameters)
                {
                    if (p.type == AnimationParameterType::Trigger)
                        continue;
                    SavedParameter saved{ p.name, animator.GetFloat(p.name), animator.GetVector(p.name) };
                    if (p.type == AnimationParameterType::Vector ? saved.vector != p.defaultVector : saved.value != p.defaultValue)
                        parameters.push_back(saved);
                }
            }
            if (a.IsLoading() || !parameters.empty())
                a.List("parameters", parameters, [&animator, graph](SceneArchive& e, SavedParameter& p) {
                    e.Field("name", p.name);
                    const int index = graph ? graph->FindParameter(p.name) : -1;
                    if (index < 0)
                        return;
                    switch (graph->parameters[static_cast<size_t>(index)].type)
                    {
                    case AnimationParameterType::Float:
                        e.Field("value", p.value);
                        if (e.IsLoading())
                            animator.SetFloat(p.name, p.value);
                        break;
                    case AnimationParameterType::Bool:
                    {
                        bool on = p.value != 0.0f;
                        e.Field("value", on);
                        if (e.IsLoading())
                            animator.SetBool(p.name, on);
                        break;
                    }
                    case AnimationParameterType::Vector:
                        e.Field("value", p.vector);
                        if (e.IsLoading())
                            animator.SetVector(p.name, p.vector);
                        break;
                    case AnimationParameterType::Trigger:
                        break;
                    }
                });
            // Each state machine's state where it isn't the entry one.
            std::vector<std::string> states;
            if (!a.IsLoading())
                states = animator.CurrentStates();
            if (a.IsLoading() || !states.empty())
                a.Field("states", states);
            if (a.IsLoading())
                for (const std::string& state : states)
                    animator.CrossFadeState(state, 0.0f);
            a.Field("speed", animator.speed);
            a.Field("playing", animator.playing);
            a.Field("rootMotion", animator.rootMotion);
            a.Field("rootBone", animator.rootBone);
            // What plays (without a graph).
            int clip = animator.CurrentClip();
            bool loop = animator.Loop();
            float time = animator.Time();
            a.Field("clip", clip);
            a.Field("loop", loop);
            a.Field("time", time);
            // Only a change replays it (code setting another field leaves it playing as it is).
            const bool changed = clip != animator.CurrentClip() || loop != animator.Loop() || time != animator.Time();
            if (a.IsLoading() && changed && clip >= 0 && clip < animator.ClipCount())
            {
                animator.Play(clip, loop);
                animator.SetTime(time);
            }
        });
    }

    bool ReadJson(const std::filesystem::path& file, Json& json, std::string& error)
    {
        try
        {
            std::ifstream stream(file, std::ios::binary);
            if (!stream)
            {
                error = "Could not open " + file.string();
                return false;
            }
            std::ostringstream text;
            text << stream.rdbuf();
            json = Json::parse(text.str());
            return true;
        }
        catch (const std::exception& e)
        {
            error = file.string() + ": " + e.what();
            return false;
        }
    }

    // A file's resource path with the spelling it has on disk (asset keys are lower case on Windows).
    std::string ResourcePathOf(const std::string& path)
    {
        std::error_code error;
        std::filesystem::path file = std::filesystem::canonical(std::filesystem::path(path), error);
        if (error)
            file = path;
        return FileSystem::ToResourcePath(file);
    }

    Json& J(void* json) { return *static_cast<Json*>(json); }

    // A scene's lighting, both ways.
    void VisitLighting(SceneArchive& a, LightingSettings& l)
    {
        a.Field("sunAzimuth", l.sunAzimuth);
        a.Field("sunElevation", l.sunElevation);
        a.Field("sunColor", l.sunColor);
        a.Field("sunIntensity", l.sunIntensity);
        a.Field("skyIntensity", l.skyIntensity);
        a.Field("reflectionIntensity", l.reflectionIntensity);
        a.Field("skyRotation", l.skyRotation);
    }

    // A material written into the scene file, its textures by reference.
    Json WriteMaterial(SceneArchive::Context& context, AssetManager2& assets, const MaterialAsset& material)
    {
        return MaterialJson::Write(material, [&](MaterialTexture, const MaterialAsset::TextureSlot& slot, Json& entry) {
            Json holder = Json::object();
            SceneArchive textures(context, &holder, false, assets);
            AssetHandle<Texture> texture = slot.texture;
            textures.Asset("asset", texture);
            if (!holder.contains("asset") || holder["asset"].get<std::string>().empty())
                return false;
            entry["asset"] = holder["asset"];
            return true;
        });
    }

    std::shared_ptr<MaterialAsset> ReadMaterial(SceneArchive::Context& context, AssetManager2& assets, const Json& json)
    {
        return MaterialJson::Read(json, [&](MaterialTexture, const Json& slot) {
            Json holder = Json::object();
            holder["asset"] = slot.value("asset", std::string());
            SceneArchive textures(context, &holder, true, assets);
            AssetHandle<Texture> texture;
            textures.Asset("asset", texture);
            return texture;
        });
    }

    // One entity's components and tags (not its name, parent or children).
    void WriteComponents(SceneArchive::Context& context, AssetManager2& assets, const Scene& scene, Entity e, Json& entity)
    {
        Json components = Json::object();
        Json tags = Json::array();
        for (const Serializer& s : Registry())
        {
            if (!s.has(e))
                continue;
            if (s.tag)
            {
                tags.push_back(s.name);
                continue;
            }
            Json object = Json::object();
            SceneArchive archive(context, &object, false, assets);
            s.save(e, archive);
            components[s.name] = std::move(object);
        }
        scene.ForEachComponent(e, [&](const ComponentView& view) {
            if (view.name && !Derived(view.name) && !FindSerializer(view.name))
                context.Warn(std::string("Left out ") + view.name + " (nothing saves it)");
        });
        if (!components.empty())
            entity["components"] = std::move(components);
        if (!tags.empty())
            entity["tags"] = std::move(tags);
    }

    void ReadComponents(SceneArchive::Context& context, AssetManager2& assets, Entity e, Json& entry, const std::string& name)
    {
        if (const auto components = entry.find("components"); components != entry.end() && components->is_object())
        {
            for (auto& [component, object] : components->items())
            {
                const Serializer* s = FindSerializer(component);
                if (!s || s->tag)
                {
                    context.Warn("Can't read " + component + " (nothing loads it)");
                    continue;
                }
                SceneArchive archive(context, &object, true, assets);
                s->load(e, archive);
            }
        }
        if (const auto tags = entry.find("tags"); tags != entry.end() && tags->is_array())
        {
            for (const Json& tag : *tags)
            {
                const Serializer* s = tag.is_string() ? FindSerializer(tag.get<std::string>()) : nullptr;
                if (!s)
                {
                    context.Warn("Can't read the tag " + (tag.is_string() ? tag.get<std::string>() : std::string("?")) + " on " + name);
                    continue;
                }
                SceneArchive archive(context, &entry, true, assets);
                s->load(e, archive);
            }
        }
    }

    // FNV-1a over what a scene file would hold.
    struct Hasher
    {
        uint64_t value = 1469598103934665603ull;

        void Bytes(const void* data, size_t size)
        {
            const auto* bytes = static_cast<const unsigned char*>(data);
            for (size_t i = 0; i < size; ++i)
            {
                value ^= bytes[i];
                value *= 1099511628211ull;
            }
        }
        template <typename T>
        void Pod(const T& v) { Bytes(&v, sizeof(T)); }
        void String(std::string_view text)
        {
            Pod(text.size());
            Bytes(text.data(), text.size());
        }
    };

    void HashMaterial(Hasher& h, const MaterialAsset& material)
    {
        h.Pod(material.properties);
        h.Pod(material.doubleSided);
        h.Pod(material.GetAlphaMode());
        for (uint32_t t = 0; t < static_cast<uint32_t>(MaterialTexture::Count); ++t)
        {
            const MaterialAsset::TextureSlot& slot = material.GetTexture(static_cast<MaterialTexture>(t));
            const AssetID id = slot.texture.IsValid() ? slot.texture.GetID() : AssetID{};
            h.Pod(id.high);
            h.Pod(id.low);
            const SamplerSettings& s = slot.sampler;
            h.Pod(s.addressU);
            h.Pod(s.addressV);
            h.Pod(s.magFilter);
            h.Pod(s.minFilter);
            h.Pod(s.mipmapFilter);
            h.Pod(s.mipmaps);
            h.Pod(slot.uvSet);
            h.Pod(slot.transform.offset);
            h.Pod(slot.transform.rotation);
            h.Pod(slot.transform.scale);
        }
    }

    class SignatureVisitor final : public FieldVisitor
    {
    public:
        SignatureVisitor(Hasher& hasher, AssetManager2& assets) : m_Hasher(hasher), m_Assets(assets) {}

        bool SeesFile() const override { return true; }

        void Visit(const char* name, const Value& value) override
        {
            m_Hasher.String(name);
            switch (value.type)
            {
            case Value::Type::Float: m_Hasher.Pod(*static_cast<const float*>(value.data)); break;
            case Value::Type::Int: m_Hasher.Pod(*static_cast<const int*>(value.data)); break;
            case Value::Type::UInt: m_Hasher.Pod(*static_cast<const uint32_t*>(value.data)); break;
            case Value::Type::Bool: m_Hasher.Pod(*static_cast<const bool*>(value.data)); break;
            case Value::Type::String: m_Hasher.String(*static_cast<const std::string*>(value.data)); break;
            case Value::Type::Floats: m_Hasher.Bytes(value.data, sizeof(float) * static_cast<size_t>(value.count)); break;
            case Value::Type::Quat: m_Hasher.Bytes(value.data, sizeof(glm::quat)); break;
            case Value::Type::FloatList:
            {
                const auto& list = *static_cast<const std::vector<float>*>(value.data);
                m_Hasher.Pod(list.size());
                m_Hasher.Bytes(list.data(), list.size() * sizeof(float));
                break;
            }
            case Value::Type::StringList:
                for (const std::string& text : *static_cast<const std::vector<std::string>*>(value.data))
                    m_Hasher.String(text);
                break;
            }
        }

        void VisitAsset(const char* name, AssetID id) override
        {
            m_Hasher.String(name);
            m_Hasher.Pod(id.high);
            m_Hasher.Pod(id.low);
            // The materials the scene file holds (made in memory, or a model's with the scene's
            // changes) count with what they are; .material files are saved on their own.
            const AssetMetadata* meta = id.IsValid() ? m_Assets.GetMetadata(id) : nullptr;
            if (!meta || meta->type != AssetType::Material || !MaterialFileOf(*meta).empty())
                return;
            if (!meta->key.empty() && meta->key.find("#material/") == std::string::npos)
                return;
            if (const std::shared_ptr<MaterialAsset> material = m_Assets.Get<MaterialAsset>(id))
                HashMaterial(m_Hasher, *material);
        }

        void VisitList(const char* name, int count) override
        {
            m_Hasher.String(name);
            m_Hasher.Pod(count);
        }

    private:
        Hasher& m_Hasher;
        AssetManager2& m_Assets;
    };
}

SceneArchive::SceneArchive(Context& context, void* json, bool loading, AssetManager2& assets)
    : m_Context(&context), m_Json(json), m_Loading(loading), m_Assets(&assets)
{
}

SceneArchive::SceneArchive(FieldVisitor& visitor, bool loading, AssetManager2& assets)
    : m_Visitor(&visitor), m_Loading(loading), m_Assets(&assets)
{
}

void SceneArchive::NamedField(const char* name, std::string& text, std::initializer_list<const char*> names)
{
    if (m_Visitor)
    {
        FieldVisitor::Value value{ FieldVisitor::Value::Type::String, &text };
        value.names = names.begin();
        value.nameCount = static_cast<int>(names.size());
        m_Visitor->Visit(name, value);
        return;
    }
    Field(name, text);
}

bool SceneArchive::Has(const char* name) const
{
    return J(m_Json).contains(name);
}

bool SceneArchive::IsNumber(const char* name) const
{
    if (m_Visitor)
        return m_Visitor->IsNumber(name);
    const Json& json = J(m_Json);
    const auto it = json.find(name);
    return it != json.end() && it->is_number();
}

void SceneArchive::Field(const char* name, float& value)
{
    if (m_Visitor)
    {
        m_Visitor->Visit(name, { FieldVisitor::Value::Type::Float, &value });
        return;
    }
    Json& json = J(m_Json);
    if (!m_Loading)
        json[name] = MaterialJson::Clean(value);
    else if (const auto it = json.find(name); it != json.end() && it->is_number())
        value = it->get<float>();
}

void SceneArchive::Field(const char* name, int& value)
{
    if (m_Visitor)
    {
        m_Visitor->Visit(name, { FieldVisitor::Value::Type::Int, &value });
        return;
    }
    Json& json = J(m_Json);
    if (!m_Loading)
        json[name] = value;
    else if (const auto it = json.find(name); it != json.end() && it->is_number())
        value = it->get<int>();
}

void SceneArchive::Field(const char* name, uint32_t& value)
{
    if (m_Visitor)
    {
        m_Visitor->Visit(name, { FieldVisitor::Value::Type::UInt, &value });
        return;
    }
    Json& json = J(m_Json);
    if (!m_Loading)
        json[name] = value;
    else if (const auto it = json.find(name); it != json.end() && it->is_number())
        value = it->get<uint32_t>();
}

void SceneArchive::Field(const char* name, uint8_t& value)
{
    uint32_t wide = value;
    Field(name, wide);
    value = static_cast<uint8_t>(wide);
}

void SceneArchive::Field(const char* name, bool& value)
{
    if (m_Visitor)
    {
        m_Visitor->Visit(name, { FieldVisitor::Value::Type::Bool, &value });
        return;
    }
    Json& json = J(m_Json);
    if (!m_Loading)
        json[name] = value;
    else if (const auto it = json.find(name); it != json.end() && it->is_boolean())
        value = it->get<bool>();
}

void SceneArchive::Field(const char* name, std::string& value)
{
    if (m_Visitor)
    {
        m_Visitor->Visit(name, { FieldVisitor::Value::Type::String, &value });
        return;
    }
    Json& json = J(m_Json);
    if (!m_Loading)
        json[name] = value;
    else if (const auto it = json.find(name); it != json.end() && it->is_string())
        value = it->get<std::string>();
}

void SceneArchive::Field(const char* name, float* values, int count)
{
    if (m_Visitor)
    {
        m_Visitor->Visit(name, { FieldVisitor::Value::Type::Floats, values, count });
        return;
    }
    Json& json = J(m_Json);
    if (!m_Loading)
    {
        json[name] = MaterialJson::Array(values, count);
        return;
    }
    const auto it = json.find(name);
    if (it == json.end() || !it->is_array())
        return;
    for (int i = 0; i < count && i < static_cast<int>(it->size()); ++i)
        if ((*it)[i].is_number())
            values[i] = (*it)[i].get<float>();
}

void SceneArchive::Field(const char* name, glm::vec2& value) { Field(name, &value.x, 2); }
void SceneArchive::Field(const char* name, glm::vec3& value) { Field(name, &value.x, 3); }
void SceneArchive::Field(const char* name, glm::vec4& value) { Field(name, &value.x, 4); }

void SceneArchive::Field(const char* name, glm::quat& value)
{
    if (m_Visitor)
    {
        m_Visitor->Visit(name, { FieldVisitor::Value::Type::Quat, &value });
        return;
    }
    float xyzw[4] = { value.x, value.y, value.z, value.w };
    Field(name, xyzw, 4);
    value = glm::quat(xyzw[3], xyzw[0], xyzw[1], xyzw[2]);
}

void SceneArchive::Field(const char* name, std::vector<float>& values)
{
    if (m_Visitor)
    {
        m_Visitor->Visit(name, { FieldVisitor::Value::Type::FloatList, &values });
        return;
    }
    Json& json = J(m_Json);
    if (!m_Loading)
    {
        json[name] = MaterialJson::Array(values.data(), static_cast<int>(values.size()));
        return;
    }
    const auto it = json.find(name);
    if (it == json.end() || !it->is_array())
        return;
    values.clear();
    for (const Json& v : *it)
        if (v.is_number())
            values.push_back(v.get<float>());
}

void SceneArchive::Field(const char* name, std::vector<std::string>& values)
{
    if (m_Visitor)
    {
        m_Visitor->Visit(name, { FieldVisitor::Value::Type::StringList, &values });
        return;
    }
    Json& json = J(m_Json);
    if (!m_Loading)
    {
        json[name] = values;
        return;
    }
    const auto it = json.find(name);
    if (it == json.end() || !it->is_array())
        return;
    values.clear();
    for (const Json& v : *it)
        values.push_back(v.is_string() ? v.get<std::string>() : std::string());
}

int SceneArchive::BeginList(const char* name, size_t count)
{
    Json& json = J(m_Json);
    if (!m_Loading)
    {
        Json list = Json::array();
        for (size_t i = 0; i < count; ++i)
            list.push_back(Json::object());
        json[name] = std::move(list);
        return static_cast<int>(count);
    }
    const auto it = json.find(name);
    return it == json.end() || !it->is_array() ? -1 : static_cast<int>(it->size());
}

SceneArchive SceneArchive::Element(const char* name, int index)
{
    return SceneArchive(*m_Context, &J(m_Json)[name][static_cast<size_t>(index)], m_Loading, *m_Assets);
}

std::string SceneArchive::Reference(AssetID id, bool material)
{
    const AssetMetadata* meta = m_Assets->GetMetadata(id);
    if (!meta)
        return {};
    const std::string& key = meta->key;
    if (key.rfind("builtin://", 0) == 0)
        return key;

    // A file, or what a file holds ("<file>#<what>"). A model's material extracted to a .material
    // file is that file (the model loads it from there).
    const size_t hash = key.find('#');
    std::string filePart = key.substr(0, hash);
    std::string inner = hash == std::string::npos ? std::string() : key.substr(hash);
    const std::filesystem::path materialFile = meta->type == AssetType::Material ? MaterialFileOf(*meta) : std::filesystem::path();
    const bool extracted = !materialFile.empty() && !inner.empty();
    if (extracted)
    {
        filePart = materialFile.string();
        inner.clear();
    }
    const std::string resource = filePart.empty() ? std::string() : ResourcePathOf(extracted || meta->path.empty() || !inner.empty() ? filePart : meta->path.string());
    if (!resource.empty())
    {
        m_Context->dependencies.insert(resource);
        if (meta->type == AssetType::AnimationGraph && std::find(m_Context->graphs.begin(), m_Context->graphs.end(), id) == m_Context->graphs.end())
            m_Context->graphs.push_back(id);
        if (meta->type == AssetType::Material && inner.rfind("#material/", 0) == 0)
        {
            // A model's material changed since it loaded: the scene keeps the change.
            const std::shared_ptr<MaterialAsset> asset = m_Assets->Get<MaterialAsset>(id);
            const bool listed = std::any_of(m_Context->overrides.begin(), m_Context->overrides.end(), [&](const auto& o) { return o.first == id; });
            if (asset && asset->IsModified() && !listed)
                m_Context->overrides.emplace_back(id, resource + inner);
        }
        if (meta->type == AssetType::Material && inner.empty())
        {
            // A .material file: its images too.
            std::string error;
            for (const MaterialFile::ImageRef& image : MaterialFile::Images(FileSystem::Resolve(resource), error))
                if (std::string path = FileSystem::ToResourcePath(image.file); !path.empty())
                    m_Context->dependencies.insert(path);
        }
        return resource + inner;
    }

    // A material made in memory (a primitive's own, a copy made unique) goes into the scene file.
    if (material)
    {
        if (std::shared_ptr<MaterialAsset> asset = m_Assets->Get<MaterialAsset>(id))
        {
            auto found = m_Context->materialIndex.find(asset.get());
            if (found == m_Context->materialIndex.end())
            {
                found = m_Context->materialIndex.emplace(asset.get(), static_cast<int>(m_Context->materials.size())).first;
                m_Context->materials.push_back(asset.get());
                m_Context->materialIds.push_back(id);
            }
            return "#material/" + std::to_string(found->second);
        }
    }
    m_Context->Warn(meta->name + " isn't in a file under the project's folders, so the scene can't refer to it");
    return {};
}

std::filesystem::path SceneArchive::ResolveFile(const std::string& reference)
{
    const std::filesystem::path file = FileSystem::Resolve(reference);
    if (file.empty())
        m_Context->Warn("Missing " + reference);
    return file;
}

AssetID SceneArchive::Resolve(const std::string& reference)
{
    if (reference.rfind("builtin://", 0) == 0)
    {
        const AssetID id = BuiltinAssets::Resolve(*m_Assets, reference);
        if (!id.IsValid())
            m_Context->Warn("No builtin " + reference);
        return id;
    }

    const size_t hash = reference.find('#');
    const std::string path = reference.substr(0, hash);
    const std::string inner = reference.substr(hash + 1);
    if (path.empty())
    {
        // The scene's own materials.
        if (inner.rfind("material/", 0) == 0)
        {
            const int index = std::atoi(inner.c_str() + 9);
            if (index >= 0 && index < static_cast<int>(m_Context->loadedMaterials.size()))
                return m_Context->loadedMaterials[static_cast<size_t>(index)].GetID();
        }
        m_Context->Warn("No " + reference + " in the scene");
        return {};
    }

    const std::filesystem::path file = ResolveFile(path);
    if (file.empty())
        return {};
    if (inner == "srgb" || inner == "linear")
        return ImageImporter::LoadTexture(*m_Assets, file, inner == "srgb").GetID();
    const std::string key = AssetManager2::MakeKey(file) + "#" + inner;
    AssetID id = m_Assets->FindID(key);
    if (!id.IsValid() || !m_Assets->IsLoaded(id))
    {
        // What a model holds: load the model.
        AssetHandle<Prefab> prefab = m_Assets->Load<Prefab>(file);
        if (prefab.IsValid())
            m_Context->keep.push_back(prefab);
        id = m_Assets->FindID(key);
    }
    if (!id.IsValid())
        m_Context->Warn("Nothing at " + reference);
    return id;
}

namespace SceneFile
{
    const detail::Serializer* FindComponent(std::string_view name)
    {
        EnsureEngineComponents();
        return FindSerializer(name);
    }

    void ForEachComponent(const std::function<void(const detail::Serializer&)>& fn)
    {
        EnsureEngineComponents();
        for (const Serializer& s : Registry())
            fn(s);
    }

    namespace detail
    {
        void Register(Serializer serializer)
        {
            EnsureEngineComponents(); // the engine's own come first in files
            for (Serializer& existing : Registry())
            {
                if (existing.name == serializer.name)
                {
                    existing = std::move(serializer);
                    return;
                }
            }
            Registry().push_back(std::move(serializer));
        }
    }

    bool Save(const Scene& scene, AssetManager2& assets, const std::filesystem::path& file, std::string& error, const SceneSettings* settings)
    {
        EnsureEngineComponents();
        SceneArchive::Context context;
        context.assets = &assets;

        Json entities = Json::array();
        int next = 0;
        std::function<void(Entity, int)> visit = [&](Entity e, int parent) {
            const int id = next++;
            Json entity = Json::object();
            entity["id"] = id;
            entity["name"] = e.GetName() ? e.GetName() : "";
            if (parent >= 0)
                entity["parent"] = parent;
            WriteComponents(context, assets, scene, e, entity);
            entities.push_back(std::move(entity));
            e.ForEachChild([&](Entity child) { visit(child, id); });
        };
        scene.ForEachRoot([&](Entity root) { visit(root, -1); });

        // The materials made in memory, their textures by reference.
        Json materials = Json::array();
        for (size_t i = 0; i < context.materials.size(); ++i)
        {
            Json material = WriteMaterial(context, assets, *context.materials[i]);
            const AssetMetadata* meta = assets.GetMetadata(context.materialIds[i]);
            Json named = Json::object();
            named["name"] = meta ? meta->name : std::string("Material");
            for (auto& [key, value] : material.items())
                named[key] = value;
            materials.push_back(std::move(named));
        }

        // The scene's changes to the materials its models hold.
        Json overrides = Json::array();
        for (const auto& [id, reference] : context.overrides)
        {
            const std::shared_ptr<MaterialAsset> material = assets.Get<MaterialAsset>(id);
            if (!material)
                continue;
            Json entry = Json::object();
            entry["material"] = reference;
            Json written = WriteMaterial(context, assets, *material);
            for (auto& [key, value] : written.items())
                entry[key] = value;
            overrides.push_back(std::move(entry));
        }

        // An animation graph's clips come from other files.
        for (const AssetID& id : context.graphs)
        {
            if (std::shared_ptr<AnimationGraph> graph = assets.Get<AnimationGraph>(id))
            {
                for (const NamedClip& clip : graph->clips)
                    if (clip.clip.IsValid())
                        if (const AssetMetadata* meta = clip.clip.GetMetadata())
                            if (std::string path = ResourcePathOf(meta->key.substr(0, meta->key.find('#'))); !path.empty())
                                context.dependencies.insert(path);
            }
        }

        // A pipeline is a file the scene uses too.
        if (settings && !settings->pipeline.empty())
            context.dependencies.insert(settings->pipeline);

        Json json = Json::object();
        json["version"] = 1;
        json["dependencies"] = std::vector<std::string>(context.dependencies.begin(), context.dependencies.end());
        if (settings)
        {
            Json& out = json["settings"];
            out["lighting"] = Json::object();
            LightingSettings lighting = settings->lighting;
            SceneArchive archive(context, &out["lighting"], false, assets);
            VisitLighting(archive, lighting);
            if (!settings->pipeline.empty())
                out["pipeline"] = settings->pipeline;
        }
        json["materials"] = std::move(materials);
        if (!overrides.empty())
            json["materialOverrides"] = std::move(overrides);
        json["entities"] = std::move(entities);

        for (const std::string& warning : context.warnings)
            Logger::Warning("Scene " + file.filename().string() + ": " + warning);

        std::error_code ec;
        std::filesystem::create_directories(file.parent_path(), ec);
        std::ofstream stream(file, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "Could not write " + file.string();
            return false;
        }
        stream << json.dump(2) << "\n";
        return static_cast<bool>(stream);
    }

    bool Load(Scene& scene, AssetManager2& assets, const std::filesystem::path& file, std::string& error, std::string* warnings, SceneSettings* settings)
    {
        EnsureEngineComponents();
        Json json;
        if (!ReadJson(file, json, error))
            return false;
        const auto list = json.find("entities");
        if (list == json.end() || !list->is_array())
        {
            error = file.string() + " has no entities";
            return false;
        }

        SceneArchive::Context context;
        context.assets = &assets;

        if (settings)
        {
            *settings = SceneSettings{};
            if (const auto found = json.find("settings"); found != json.end() && found->is_object())
            {
                if (const auto lighting = found->find("lighting"); lighting != found->end() && lighting->is_object())
                {
                    SceneArchive archive(context, &*lighting, true, assets);
                    VisitLighting(archive, settings->lighting);
                    settings->hasLighting = true;
                }
                settings->pipeline = found->value("pipeline", std::string());
            }
        }

        // The scene's own materials first: components refer to them.
        if (const auto materials = json.find("materials"); materials != json.end() && materials->is_array())
        {
            for (const Json& entry : *materials)
            {
                std::shared_ptr<MaterialAsset> material = ReadMaterial(context, assets, entry);
                context.loadedMaterials.push_back(assets.RegisterAsset<MaterialAsset>(material, AssetType::Material, entry.value("name", std::string("Material"))));
            }
        }

        // Its changes to its models' materials, onto the materials the models load.
        if (const auto overrides = json.find("materialOverrides"); overrides != json.end() && overrides->is_array())
        {
            for (Json& entry : *overrides)
            {
                AssetHandle<MaterialAsset> target;
                SceneArchive archive(context, &entry, true, assets);
                archive.Asset("material", target);
                if (target.IsValid())
                    target.Get()->CopySettingsFrom(*ReadMaterial(context, assets, entry));
            }
        }

        std::unordered_map<int, Entity> made;
        for (Json& entry : *list)
        {
            const std::string name = entry.value("name", std::string("Entity"));
            Entity e = scene.Instantiate(name.c_str());
            made[entry.value("id", -1)] = e;
            if (const auto parent = entry.find("parent"); parent != entry.end() && parent->is_number())
            {
                if (const auto p = made.find(parent->get<int>()); p != made.end())
                    e.SetParent(p->second);
                else
                    context.Warn(name + "'s parent comes after it");
            }
            ReadComponents(context, assets, e, entry, name);
        }

        for (const std::string& warning : context.warnings)
        {
            Logger::Warning("Scene " + file.filename().string() + ": " + warning);
            if (warnings)
                *warnings += (warnings->empty() ? "" : "\n") + warning;
        }
        return true;
    }

    std::vector<std::string> Dependencies(const std::filesystem::path& file, std::string& error)
    {
        std::vector<std::string> files;
        Json json;
        if (!ReadJson(file, json, error))
            return files;
        if (const auto list = json.find("dependencies"); list != json.end() && list->is_array())
            for (const Json& path : *list)
                if (path.is_string())
                    files.push_back(path.get<std::string>());
        return files;
    }

    uint64_t Signature(const Scene& scene, AssetManager2& assets, const SceneSettings* settings)
    {
        EnsureEngineComponents();
        Hasher h;
        SignatureVisitor visitor(h, assets);
        std::function<void(Entity)> visit = [&](Entity e) {
            h.String(e.GetName() ? e.GetName() : "");
            for (const Serializer& s : Registry())
            {
                if (!s.has(e))
                    continue;
                h.String(s.name);
                SceneArchive archive(visitor, false, assets);
                s.save(e, archive);
            }
            h.Pod('(');
            e.ForEachChild(visit);
            h.Pod(')');
        };
        scene.ForEachRoot(visit);
        if (settings)
        {
            h.Pod(settings->lighting.sunAzimuth);
            h.Pod(settings->lighting.sunElevation);
            h.Pod(settings->lighting.sunColor);
            h.Pod(settings->lighting.sunIntensity);
            h.Pod(settings->lighting.skyIntensity);
            h.Pod(settings->lighting.reflectionIntensity);
            h.Pod(settings->lighting.skyRotation);
            h.String(settings->pipeline);
        }
        return h.value;
    }

    EntityState SaveEntity(Entity entity, AssetManager2& assets)
    {
        EnsureEngineComponents();
        SceneArchive::Context context;
        context.assets = &assets;
        Json json = Json::object();
        json["name"] = entity.GetName() ? entity.GetName() : "";
        WriteComponents(context, assets, *entity.GetScene(), entity, json);
        EntityState state;
        state.json = json.dump();
        for (const AssetID& id : context.materialIds)
            state.materials.push_back(assets.LoadAsset<MaterialAsset>(id));
        return state;
    }

    void LoadEntity(Entity entity, AssetManager2& assets, const EntityState& state)
    {
        EnsureEngineComponents();
        Json json = Json::parse(state.json, nullptr, false);
        if (json.is_discarded() || !entity.IsValid())
            return;
        SceneArchive::Context context;
        context.assets = &assets;
        context.loadedMaterials = state.materials;

        // What it has now that wasn't saved goes.
        const Json& components = json.contains("components") ? json["components"] : Json::object();
        const Json& tags = json.contains("tags") ? json["tags"] : Json::array();
        for (const Serializer& s : Registry())
        {
            if (!s.has(entity))
                continue;
            const bool saved = s.tag ? std::find(tags.begin(), tags.end(), s.name) != tags.end() : components.contains(s.name);
            if (!saved)
                s.remove(entity);
        }
        const std::string name = json.value("name", std::string());
        entity.SetName(name.c_str());
        ReadComponents(context, assets, entity, json, name);
        for (const std::string& warning : context.warnings)
            Logger::Warning(name + ": " + warning);
    }
}
