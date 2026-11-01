#pragma once
#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Rendering/StandardPBR.hpp>
#include <Mariana/Scene/ECS.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

class MaterialAsset;

// Where a SceneArchive made for code rather than a file sends the fields (scripting reads and writes
// components by field name through it): Visit gets each field the component's function names, with
// its type and where its value is; reading takes the value, writing puts one there.
class FieldVisitor
{
public:
    struct Value
    {
        enum class Type : uint8_t { Float, Int, UInt, Bool, String, Floats, Quat, FloatList, StringList };
        Type type;
        void* data;
        int count = 0; // Floats: how many
        // An enum given by name (a String): the names it can have.
        const char* const* names = nullptr;
        int nameCount = 0;
    };
    virtual ~FieldVisitor() = default;
    virtual void Visit(const char* name, const Value& value) = 0;
    // Writing: whether the value given for `name` is a number (an enum given by its number).
    virtual bool IsNumber(const char*) const { return false; }

    // A visitor that sees what a file keeps (a scene's signature) also gets the asset references
    // and the lists (each element's fields follow VisitList), and not the transient fields.
    virtual bool SeesFile() const { return false; }
    virtual void VisitAsset(const char*, AssetID) {}
    virtual void VisitList(const char*, int) {}
};

// Reads or writes one component's fields in a scene file. The same function does both (it is given
// the component and calls Field for each member), so a component is described once. Names are the
// JSON keys; a field missing from the file keeps the value the component has. Made over a
// FieldVisitor, the same function reads or writes fields for code (assets and lists are left out).
class SceneArchive
{
public:
    // Writing into the component (a file being loaded, code setting a field).
    bool IsLoading() const { return m_Loading; }
    // Whether lists and assets are visited too: a file, or a visitor that sees what one keeps.
    bool ForFile() const { return !m_Visitor || m_Visitor->SeesFile(); }

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
            NamedField(name, text, names);
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
        NamedField(name, text, names);
        for (size_t i = 0; i < names.size(); ++i)
            if (text == names.begin()[i])
                value = static_cast<E>(i);
    }

    // A field code reads and writes but files don't keep (a vehicle's pedals, a character's walk).
    template <typename T>
    void Transient(const char* name, T& value)
    {
        if (m_Visitor && !m_Visitor->SeesFile())
            Field(name, value);
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
    // Over a visitor: reading the component's fields (loading = false) or writing them.
    SceneArchive(FieldVisitor& visitor, bool loading, AssetManager2& assets);

private:
    template <typename T>
    AssetHandle<T> Load(const std::string& reference);
    // A string field that is one of `names` (an enum); a visitor is told the names.
    void NamedField(const char* name, std::string& text, std::initializer_list<const char*> names);
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

    Context* m_Context = nullptr;
    void* m_Json = nullptr; // the JSON object read or written
    FieldVisitor* m_Visitor = nullptr;
    bool m_Loading;
    AssetManager2* m_Assets;
};

// What a scene keeps besides its entities: its lighting and the render pipeline it is seen with.
struct SceneSettings
{
    LightingSettings lighting;
    std::string pipeline; // resource path of a .pipeline file; empty = the app's choice
    // Loading: whether the file had them (older files don't).
    bool hasLighting = false;
};

// .scene files: a scene's entities, their hierarchy and their components, as JSON:
//   { "version": 1, "dependencies": [ "/Models/Car.gltf", ... ],
//     "settings": { "lighting": { "sunAzimuth": 270, ... }, "pipeline": "/Pipelines/Default.pipeline" },
//     "materials": [ { a material as in a .material file, textures by "image" or "asset" } ],
//     "materialOverrides": [ { "material": "/Models/Car.glb#material/2", the material as above } ],
//     "entities": [ { "id": 0, "name": "Car", "parent": 3, "components": { "LocalTRS": { ... } },
//                     "tags": [ "ShadowCasterTag" ] } ] }
// Parents come before their children. Components are written by the serializer registered under
// their ECS name (the engine's own: LocalTRS, MeshComponent, ShadowCasterTag, CameraComponent,
// Animator, AnimationTargetEntity, SkinComponent; modules add theirs, e.g. the physics components);
// components with none are left out with a warning, transforms derived from LocalTRS are rebuilt.
// `dependencies` lists every file the scene uses (for packaging). `materialOverrides` are the
// scene's edits to materials a model file holds (changed since it loaded, not extracted to a
// .material file): loading puts them back onto the model's material.
namespace SceneFile
{
    template <typename T>
    using Visit = std::function<void(SceneArchive&, T&)>;

    // Saves and loads component T (registered with the ECS as `name`) through `visit`.
    template <typename T>
    void RegisterComponent(const char* name, Visit<T> visit);
    template <typename T>
    void RegisterTag(const char* name);

    bool Save(const Scene& scene, AssetManager2& assets, const std::filesystem::path& file, std::string& error, const SceneSettings* settings = nullptr);
    // Adds the file's entities to `scene` (Scene::Clear it first to replace what is there); warnings
    // (a missing file, a component nobody reads) go to the log and to `warnings`; the file's
    // settings to `settings`.
    bool Load(Scene& scene, AssetManager2& assets, const std::filesystem::path& file, std::string& error, std::string* warnings = nullptr,
              SceneSettings* settings = nullptr);
    // The files a scene uses (resource paths), read without loading it.
    std::vector<std::string> Dependencies(const std::filesystem::path& file, std::string& error);

    // A hash of what saving would write (entities, their names, hierarchy, components and tags,
    // the materials they use, the settings): equal signatures, the same file. How an editor knows
    // a scene changed since it was saved, whatever changed it.
    uint64_t Signature(const Scene& scene, AssetManager2& assets, const SceneSettings* settings = nullptr);

    // One entity's components as a file has them, to put back later onto the same entity (or
    // another): keeping what changed while a game played after the scene is restored.
    struct EntityState
    {
        std::string json;
        std::vector<AssetHandle<MaterialAsset>> materials; // the ones made in memory it uses
    };
    EntityState SaveEntity(Entity entity, AssetManager2& assets);
    // Gives the entity the saved components (adding, replacing, and removing those it has that
    // weren't saved), its name too.
    void LoadEntity(Entity entity, AssetManager2& assets, const EntityState& state);

    namespace detail
    {
        struct Serializer
        {
            std::string name;
            bool tag = false;
            std::function<bool(Entity)> has;
            std::function<void(Entity, SceneArchive&)> save;
            std::function<void(Entity, SceneArchive&)> load; // adds the component if it isn't there
            std::function<void(Entity)> remove;
            std::function<void(const Scene&, const std::function<void(Entity)>&)> each; // none for tags
        };
        void Register(Serializer serializer);
    }

    // The serializer of a component by its ECS name (nullptr if it has none): how code reaches
    // components by name (scripting). Every app registers its components before it looks one up.
    const detail::Serializer* FindComponent(std::string_view name);
    // Every component (and tag) scene files know, in registration order.
    void ForEachComponent(const std::function<void(const detail::Serializer&)>& fn);
}

template <typename T>
void SceneArchive::Asset(const char* name, AssetHandle<T>& handle)
{
    constexpr bool material = std::is_same_v<T, MaterialAsset>;
    if (m_Visitor)
    {
        if (!m_Loading && m_Visitor->SeesFile())
            m_Visitor->VisitAsset(name, handle.IsValid() ? handle.GetID() : AssetID{});
        return;
    }
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
    if (m_Visitor)
    {
        if (!m_Loading && m_Visitor->SeesFile())
        {
            m_Visitor->VisitList(name, static_cast<int>(handles.size()));
            for (const AssetHandle<T>& h : handles)
                m_Visitor->VisitAsset(name, h.IsValid() ? h.GetID() : AssetID{});
        }
        return;
    }
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
    if (m_Visitor)
    {
        if (m_Loading || !m_Visitor->SeesFile())
            return;
        m_Visitor->VisitList(name, static_cast<int>(elements.size()));
        for (T& element : elements)
        {
            SceneArchive archive(*m_Visitor, false, *m_Assets);
            item(archive, element);
        }
        return;
    }
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
            // Saving reads the component itself (a copy may not keep everything, e.g. an
            // Animator's state machines); visiting writes nothing back unless loading.
            visit(archive, *const_cast<T*>(e.TryGet<T>()));
        },
        [visit](Entity e, SceneArchive& archive) {
            // An existing component is changed in place: a copy may not keep everything (an
            // Animator's state machines would start again from their entry states).
            if (T* existing = e.Get<T>())
            {
                visit(archive, *existing);
                return;
            }
            T value{};
            visit(archive, value);
            e.Add<T>(value);
        },
        [](Entity e) { e.Remove<T>(); },
        [](const Scene& scene, const std::function<void(Entity)>& fn) {
            scene.Each<T>([&](Entity e, const T&) { fn(e); });
        } });
}

template <typename T>
void SceneFile::RegisterTag(const char* name)
{
    detail::Register({ name, true,
        [](Entity e) { return e.HasTag<T>(); },
        [](Entity, SceneArchive&) {},
        [](Entity e, SceneArchive&) { e.AddTag<T>(); },
        [](Entity e) { e.RemoveTag<T>(); },
        nullptr });
}
