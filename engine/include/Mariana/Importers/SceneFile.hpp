#pragma once
#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Scene/ECS.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <vector>

class MaterialAsset;

// Reads or writes one component's fields in a scene file. The same function does both (it is given
// the component and calls Field for each member), so a component is described once. Names are the
// JSON keys; a field missing from the file keeps the value the component has.
class SceneArchive
{
public:
    bool IsLoading() const { return m_Loading; }

    void Field(const char* name, float& value);
    void Field(const char* name, int& value);
    void Field(const char* name, uint32_t& value);
    void Field(const char* name, uint8_t& value);
    void Field(const char* name, bool& value);
    void Field(const char* name, std::string& value);
    void Field(const char* name, glm::vec2& value);
    void Field(const char* name, glm::vec3& value);
    void Field(const char* name, glm::vec4& value);
    void Field(const char* name, glm::quat& value); // [x, y, z, w]
    void Field(const char* name, std::vector<float>& values);
    void Field(const char* name, std::vector<std::string>& values);
    void Field(const char* name, float* values, int count);
    template <typename E>
        requires std::is_enum_v<E>
    void Field(const char* name, E& value)
    {
        int raw = static_cast<int>(value);
        Field(name, raw);
        value = static_cast<E>(raw);
    }
    // An enum written by name (`names` in the enum's order); a number is read too.
    template <typename E>
        requires std::is_enum_v<E>
    void Field(const char* name, E& value, std::initializer_list<const char*> names)
    {
        const int raw = static_cast<int>(value);
        if (!m_Loading)
        {
            std::string text = raw >= 0 && raw < static_cast<int>(names.size()) ? names.begin()[raw] : std::to_string(raw);
            Field(name, text);
            return;
        }
        if (IsNumber(name))
        {
            int number = raw;
            Field(name, number);
            value = static_cast<E>(number);
            return;
        }
        std::string text;
        Field(name, text);
        for (size_t i = 0; i < names.size(); ++i)
            if (text == names.begin()[i])
                value = static_cast<E>(i);
    }

    // An asset: by its file's resource path ("/Models/Tree.glb", "/Models/Tree.glb#mesh/0" for what
    // a file holds) or a builtin's key; a material made in memory is written into the scene file.
    // Loading loads what it names.
    template <typename T>
    void Asset(const char* name, AssetHandle<T>& handle);
    template <typename T>
    void Assets(const char* name, std::vector<AssetHandle<T>>& handles);

    // A list of objects, each read or written by `item(archive, element)`.
    template <typename T, typename Fn>
    void List(const char* name, std::vector<T>& elements, Fn&& item);

    AssetManager2& Manager() const { return *m_Assets; }

    // Made by SceneFile.
    struct Context;
    SceneArchive(Context& context, void* json, bool loading, AssetManager2& assets);

private:
    template <typename T>
    AssetHandle<T> Load(const std::string& reference);
    bool Has(const char* name) const;
    bool IsNumber(const char* name) const;
    // Saving: how an asset is written (empty when it can't be). Loading: the file a whole-file
    // reference names, or the asset another reference names (loading the file it is in).
    std::string Reference(AssetID id, bool material);
    std::filesystem::path ResolveFile(const std::string& reference);
    AssetID Resolve(const std::string& reference);
    // Lists: the number of elements (saving: makes that many), or -1 when the file has none.
    int BeginList(const char* name, size_t count);
    SceneArchive Element(const char* name, int index);

    Context* m_Context;
    void* m_Json; // the JSON object read or written
    bool m_Loading;
    AssetManager2* m_Assets;
};

// .scene files: a scene's entities, their hierarchy and their components, as JSON:
//   { "version": 1, "dependencies": [ "/Models/Car.gltf", ... ],
//     "materials": [ { a material as in a .material file, textures by "image" or "asset" } ],
//     "entities": [ { "id": 0, "name": "Car", "parent": 3, "components": { "LocalTRS": { ... } },
//                     "tags": [ "ShadowCasterTag" ] } ] }
// Parents come before their children. Components are written by the serializer registered under
// their ECS name (the engine's own: LocalTRS, MeshComponent, ShadowCasterTag, CameraComponent,
// Animator, AnimationTargetEntity, SkinComponent; modules add theirs, e.g. the physics components);
// components with none are left out with a warning, transforms derived from LocalTRS are rebuilt.
// `dependencies` lists every file the scene uses (for packaging).
namespace SceneFile
{
    template <typename T>
    using Visit = std::function<void(SceneArchive&, T&)>;

    // Saves and loads component T (registered with the ECS as `name`) through `visit`.
    template <typename T>
    void RegisterComponent(const char* name, Visit<T> visit);
    template <typename T>
    void RegisterTag(const char* name);

    bool Save(const Scene& scene, AssetManager2& assets, const std::filesystem::path& file, std::string& error);
    // Adds the file's entities to `scene` (Scene::Clear it first to replace what is there); warnings
    // (a missing file, a component nobody reads) go to the log and to `warnings`.
    bool Load(Scene& scene, AssetManager2& assets, const std::filesystem::path& file, std::string& error, std::string* warnings = nullptr);
    // The files a scene uses (resource paths), read without loading it.
    std::vector<std::string> Dependencies(const std::filesystem::path& file, std::string& error);

    namespace detail
    {
        struct Serializer
        {
            std::string name;
            bool tag = false;
            std::function<bool(Entity)> has;
            std::function<void(Entity, SceneArchive&)> save;
            std::function<void(Entity, SceneArchive&)> load;
        };
        void Register(Serializer serializer);
    }
}

template <typename T>
void SceneArchive::Asset(const char* name, AssetHandle<T>& handle)
{
    constexpr bool material = std::is_same_v<T, MaterialAsset>;
    if (!m_Loading)
    {
        std::string reference = handle.IsValid() ? Reference(handle.GetID(), material) : std::string();
        Field(name, reference);
        return;
    }
    if (!Has(name))
        return;
    std::string reference;
    Field(name, reference);
    handle = Load<T>(reference);
}

template <typename T>
void SceneArchive::Assets(const char* name, std::vector<AssetHandle<T>>& handles)
{
    constexpr bool material = std::is_same_v<T, MaterialAsset>;
    std::vector<std::string> references;
    if (!m_Loading)
    {
        for (const AssetHandle<T>& h : handles)
            references.push_back(h.IsValid() ? Reference(h.GetID(), material) : std::string());
        Field(name, references);
        return;
    }
    if (!Has(name))
        return;
    Field(name, references);
    handles.clear();
    for (const std::string& reference : references)
        handles.push_back(Load<T>(reference));
}

template <typename T>
AssetHandle<T> SceneArchive::Load(const std::string& reference)
{
    if (reference.empty())
        return {};
    // A whole file is loaded as a T; what a file holds, builtins and the scene's own materials are found.
    const bool wholeFile = reference.find('#') == std::string::npos && reference.rfind("builtin://", 0) != 0;
    if (wholeFile)
    {
        const std::filesystem::path file = ResolveFile(reference);
        return file.empty() ? AssetHandle<T>{} : m_Assets->Load<T>(file);
    }
    const AssetID id = Resolve(reference);
    return id.IsValid() ? m_Assets->LoadAsset<T>(id) : AssetHandle<T>{};
}

template <typename T, typename Fn>
void SceneArchive::List(const char* name, std::vector<T>& elements, Fn&& item)
{
    const int count = BeginList(name, elements.size());
    if (count < 0)
        return;
    if (m_Loading)
        elements.resize(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        SceneArchive element = Element(name, i);
        item(element, elements[i]);
    }
}

template <typename T>
void SceneFile::RegisterComponent(const char* name, Visit<T> visit)
{
    detail::Register({ name, false,
        [](Entity e) { return e.Has<T>(); },
        [visit](Entity e, SceneArchive& archive) {
            T value = *e.TryGet<T>();
            visit(archive, value);
        },
        [visit](Entity e, SceneArchive& archive) {
            T value{};
            if (const T* existing = e.TryGet<T>())
                value = *existing;
            visit(archive, value);
            e.Add<T>(value);
        } });
}

template <typename T>
void SceneFile::RegisterTag(const char* name)
{
    detail::Register({ name, true,
        [](Entity e) { return e.HasTag<T>(); },
        [](Entity, SceneArchive&) {},
        [](Entity e, SceneArchive&) { e.AddTag<T>(); } });
}
