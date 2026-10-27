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
            });
            a.Asset("graph", animator.graph);
            a.Field("speed", animator.speed);
            a.Field("playing", animator.playing);
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

    // A file, or what a file holds ("<file>#<what>").
    const size_t hash = key.find('#');
    const std::string filePart = key.substr(0, hash);
    const std::string inner = hash == std::string::npos ? std::string() : key.substr(hash);
    const std::string resource = filePart.empty() ? std::string() : ResourcePathOf(meta->path.empty() || !inner.empty() ? filePart : meta->path.string());
    if (!resource.empty())
    {
        m_Context->dependencies.insert(resource);
        if (meta->type == AssetType::AnimationGraph && std::find(m_Context->graphs.begin(), m_Context->graphs.end(), id) == m_Context->graphs.end())
            m_Context->graphs.push_back(id);
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

    bool Save(const Scene& scene, AssetManager2& assets, const std::filesystem::path& file, std::string& error)
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
            entities.push_back(std::move(entity));
            e.ForEachChild([&](Entity child) { visit(child, id); });
        };
        scene.ForEachRoot([&](Entity root) { visit(root, -1); });

        // The materials made in memory, their textures by reference.
        Json materials = Json::array();
        for (size_t i = 0; i < context.materials.size(); ++i)
        {
            Json material = MaterialJson::Write(*context.materials[i], [&](MaterialTexture, const MaterialAsset::TextureSlot& slot, Json& entry) {
                Json holder = Json::object();
                SceneArchive textures(context, &holder, false, assets);
                AssetHandle<Texture> texture = slot.texture;
                textures.Asset("asset", texture);
                if (!holder.contains("asset") || holder["asset"].get<std::string>().empty())
                    return false;
                entry["asset"] = holder["asset"];
                return true;
            });
            const AssetMetadata* meta = assets.GetMetadata(context.materialIds[i]);
            Json named = Json::object();
            named["name"] = meta ? meta->name : std::string("Material");
            for (auto& [key, value] : material.items())
                named[key] = value;
            materials.push_back(std::move(named));
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

        Json json = Json::object();
        json["version"] = 1;
        json["dependencies"] = std::vector<std::string>(context.dependencies.begin(), context.dependencies.end());
        json["materials"] = std::move(materials);
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

    bool Load(Scene& scene, AssetManager2& assets, const std::filesystem::path& file, std::string& error, std::string* warnings)
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
        SceneArchive root(context, &json, true, assets);

        // The scene's own materials first: components refer to them.
        if (const auto materials = json.find("materials"); materials != json.end() && materials->is_array())
        {
            for (const Json& entry : *materials)
            {
                std::shared_ptr<MaterialAsset> material = MaterialJson::Read(entry, [&](MaterialTexture, const Json& slot) {
                    Json holder = Json::object();
                    holder["asset"] = slot.value("asset", std::string());
                    SceneArchive textures(context, &holder, true, assets);
                    AssetHandle<Texture> texture;
                    textures.Asset("asset", texture);
                    return texture;
                });
                context.loadedMaterials.push_back(assets.RegisterAsset<MaterialAsset>(material, AssetType::Material, entry.value("name", std::string("Material"))));
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
                        context.Warn("Can't read the tag " + (tag.is_string() ? tag.get<std::string>() : std::string("?")));
                        continue;
                    }
                    SceneArchive archive(context, &json, true, assets);
                    s->load(e, archive);
                }
            }
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
}
