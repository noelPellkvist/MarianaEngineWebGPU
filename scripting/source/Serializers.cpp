// The Scripts component in the ECS and in scene files, and putting scripts on entities.
#include <Mariana/Scripting/ScriptWorld.hpp>

#include <Mariana/Foundation/FileSystem.hpp>
#include <Mariana/Foundation/Logger.hpp>
#include <Mariana/Importers/SceneFile.hpp>

void ScriptWorld::RegisterComponents()
{
    ECS::RegisterComponent<Scripts>("Scripts");
    SceneFile::RegisterComponent<Scripts>("Scripts", [](SceneArchive& a, Scripts& s) {
        a.List("scripts", s.scripts, [](SceneArchive& e, ScriptSlot& slot) {
            e.Asset("script", slot.script);
            e.Field("enabled", slot.enabled);
            e.List("values", slot.values, [](SceneArchive& v, ScriptValue& value) {
                v.Field("name", value.name);
                v.Field("type", value.type, { "Number", "Bool", "String", "Vector" });
                switch (value.type)
                {
                case ScriptValueType::Number: v.Field("value", value.number); break;
                case ScriptValueType::Bool: v.Field("value", value.boolean); break;
                case ScriptValueType::String: v.Field("value", value.text); break;
                case ScriptValueType::Vector: v.Field("value", value.vector); break;
                }
            });
        });
    });
}

std::filesystem::path ScriptWorld::ResourceRoot()
{
    return MARIANA_SCRIPTING_RESOURCES;
}

bool ScriptWorld::AddScript(Entity entity, AssetManager2& assets, const std::string& resource, std::vector<ScriptValue> values)
{
    const std::filesystem::path file = FileSystem::Resolve(resource);
    AssetHandle<ScriptAsset> script = file.empty() ? AssetHandle<ScriptAsset>{} : assets.Load<ScriptAsset>(file);
    if (!script.Get())
    {
        Logger::Error("No script " + resource);
        return false;
    }
    if (!entity.Has<Scripts>())
        entity.Add<Scripts>(Scripts{});
    entity.Get<Scripts>()->scripts.push_back(ScriptSlot{ script, true, std::move(values) });
    return true;
}
