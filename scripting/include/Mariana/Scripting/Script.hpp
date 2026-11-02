#pragma once
#include <Mariana/Assets/AssetManager.hpp>

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// A Luau script (a .luau or .lua file, or source given by code) and the bytecode it compiles to.
// A script is a module: it returns a table whose functions (Start, Update, LateUpdate, OnDestroy,
// OnContactBegin, ...) run for every entity that has the script, each with its own `self`, and
// whose `properties` table names the values (numbers, booleans, strings, vectors) the inspector
// edits per entity, with their defaults:
//
//   local Spin = {}
//   Spin.properties = { speed = 90 }
//   function Spin:Update(dt) self.entity:Rotate(vec3(0, 1, 0), self.speed * dt) end
//   return Spin
class ScriptAsset
{
public:
    std::string source;
    std::string bytecode; // what the VM loads; empty when it didn't compile
    std::string error;    // why it didn't
    std::filesystem::path file; // where it was read from (empty when code gave the source)

    // Compiles `text` as the script's source; bumps the version.
    void SetSource(std::string text);
    // Reads the file again when it changed on disk since it was read; true when it did.
    bool ReloadIfChanged();
    // Changes with every new source (running instances take the new code).
    uint32_t Version() const { return m_Version; }

    static std::shared_ptr<ScriptAsset> Load(const std::filesystem::path& file, std::string& error);
    // Makes .luau and .lua files load as ScriptAssets.
    static void RegisterLoader(AssetManager2& assets);

    // The script `require(name)` means in the script at resource path `from` (empty: a script in
    // /Scripts): a name starting with "/" is a resource path, any other is relative to the folder
    // `from` is in ("./Util", "../Lib/Util", "Util"); without an extension .luau, else .lua. The
    // resource path of that file, or empty when there is none.
    static std::string ResolveRequire(const std::string& name, const std::string& from);

private:
    uint32_t m_Version = 0;
    std::filesystem::file_time_type m_WriteTime{};
};

// A value set for one of a script's properties on an entity.
enum class ScriptValueType : uint8_t { Number, Bool, String, Vector };
struct ScriptValue
{
    std::string name;
    ScriptValueType type = ScriptValueType::Number;
    float number = 0.0f;
    bool boolean = false;
    std::string text;
    glm::vec3 vector{ 0.0f };
};

// One script on an entity, and the values set for its properties (the others are the script's
// defaults). A disabled script keeps its state but isn't updated.
struct ScriptSlot
{
    AssetHandle<ScriptAsset> script;
    bool enabled = true;
    std::vector<ScriptValue> values;
};

// The scripts an entity runs, in order (component "Scripts").
struct Scripts
{
    std::vector<ScriptSlot> scripts;
};
