#include <Mariana/ScriptTools/ScriptTypes.hpp>

#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Importers/SceneFile.hpp>

#include <fstream>
#include <sstream>
#include <vector>

namespace
{
    std::string ReadText(const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary);
        std::ostringstream text;
        text << stream.rdbuf();
        return text.str();
    }

    // A field's Luau type.
    std::string TypeOf(const FieldVisitor::Value& value)
    {
        using Type = FieldVisitor::Value::Type;
        switch (value.type)
        {
        case Type::Float:
        case Type::Int:
        case Type::UInt: return "number";
        case Type::Bool: return "boolean";
        case Type::String:
        {
            if (value.nameCount == 0)
                return "string";
            std::string names;
            for (int i = 0; i < value.nameCount; ++i)
                names += (i ? " | \"" : "\"") + std::string(value.names[i]) + "\"";
            return names;
        }
        case Type::Floats: return value.count == 2 || value.count == 3 ? "vector" : "{ number }";
        case Type::Quat: return "quat";
        case Type::FloatList: return "{ number }";
        case Type::StringList: return "{ string }";
        }
        return "any";
    }

    // The fields a component's scene file function visits, with their types.
    class Describer final : public FieldVisitor
    {
    public:
        void Visit(const char* name, const Value& value) override { fields.push_back({ name, TypeOf(value) }); }
        std::vector<std::pair<std::string, std::string>> fields;
    };

    class NoFields final : public FieldVisitor
    {
    public:
        void Visit(const char*, const Value&) override {}
    };

    void Replace(std::string& text, const std::string& marker, const std::string& with)
    {
        const size_t at = text.find(marker);
        if (at != std::string::npos)
            text.replace(at, marker.size(), with);
    }
}

namespace ScriptTypes
{
    std::string Definitions()
    {
        std::string text = ReadText(std::filesystem::path(MARIANA_SCRIPT_TOOLS_RESOURCES) / "Core.d.luau");

        // Each component's fields, read from a default one on an entity of a scene of its own.
        AssetManager2 assets;
        Scene scene;
        scene.UpdateComponentRegistry();
        std::string components, methods, names;
        SceneFile::ForEachComponent([&](const SceneFile::detail::Serializer& serializer) {
            names += (names.empty() ? "\"" : " | \"") + serializer.name + "\"";
            if (serializer.tag)
            {
                methods += "    function Add(self, component: \"" + serializer.name + "\"): Component\n";
                return;
            }
            Entity entity = scene.Instantiate();
            NoFields none;
            SceneArchive adding(none, true, assets);
            serializer.load(entity, adding);
            Describer describer;
            SceneArchive reading(describer, false, assets);
            serializer.save(entity, reading);

            const std::string base = serializer.name == "Animator" ? "AnimatorMethods" : "Component";
            components += "declare extern type " + serializer.name + " extends " + base + " with\n";
            for (const auto& [field, type] : describer.fields)
                components += "    " + field + ": " + type + "\n";
            components += "end\n\n";
            methods += "    function Get(self, component: \"" + serializer.name + "\"): " + serializer.name + "?\n";
            methods += "    function Add(self, component: \"" + serializer.name + "\", fields: { [string]: any }?): " + serializer.name + "\n";
        });
        components += "export type ComponentName = " + (names.empty() ? std::string("string") : names) + "\n";
        Replace(text, "-- @components\n", components);
        Replace(text, "    -- @componentMethods\n", methods);
        return text;
    }

    bool Write(const std::filesystem::path& file, std::string& error)
    {
        const std::string text = Definitions();
        std::error_code ec;
        if (std::filesystem::exists(file, ec) && ReadText(file) == text)
            return true;
        std::filesystem::create_directories(file.parent_path(), ec);
        std::ofstream stream(file, std::ios::binary);
        if (!stream)
        {
            error = "Could not write " + file.string();
            return false;
        }
        stream << text;
        return true;
    }
}
