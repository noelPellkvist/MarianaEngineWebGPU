#include <Mariana/Scripting/Script.hpp>

#include <Mariana/Foundation/Logger.hpp>

#include "Luau/Compiler.h"

#include <fstream>
#include <sstream>

namespace
{
    Luau::CompileOptions Options()
    {
        Luau::CompileOptions options;
        options.optimizationLevel = 1; // keeps line numbers and names for errors
        options.debugLevel = 1;
        options.vectorCtor = "vec3";   // vec3(x, y, z) builds a vector in place
        // Luau resolves paths like `time.time` once, when a script loads, and keeps the value (an
        // "import"); `time` changes every frame, so it is read each time instead.
        static const char* const kMutableGlobals[] = { "time", nullptr };
        options.mutableGlobals = kMutableGlobals;
        return options;
    }

    bool ReadText(const std::filesystem::path& file, std::string& text, std::string& error)
    {
        std::ifstream stream(file, std::ios::binary);
        if (!stream)
        {
            error = "Could not open " + file.string();
            return false;
        }
        std::ostringstream content;
        content << stream.rdbuf();
        text = content.str();
        return true;
    }
}

void ScriptAsset::SetSource(std::string text)
{
    source = std::move(text);
    bytecode = Luau::compile(source, Options());
    error.clear();
    // A failed compile gives a 0 byte and the message (":line: what").
    if (!bytecode.empty() && bytecode[0] == 0)
    {
        error = (file.empty() ? std::string("script") : file.filename().string()) + bytecode.substr(1);
        bytecode.clear();
    }
    ++m_Version;
}

bool ScriptAsset::ReloadIfChanged()
{
    if (file.empty())
        return false;
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(file, ec);
    if (ec || time == m_WriteTime)
        return false;
    std::string text, readError;
    if (!ReadText(file, text, readError))
        return false; // still being written: next time
    m_WriteTime = time;
    if (text == source)
        return false;
    SetSource(std::move(text));
    return true;
}

std::shared_ptr<ScriptAsset> ScriptAsset::Load(const std::filesystem::path& file, std::string& error)
{
    std::string text;
    if (!ReadText(file, text, error))
        return nullptr;
    auto script = std::make_shared<ScriptAsset>();
    script->file = file;
    std::error_code ec;
    script->m_WriteTime = std::filesystem::last_write_time(file, ec);
    script->SetSource(std::move(text));
    return script;
}

void ScriptAsset::RegisterLoader(AssetManager2& assets)
{
    assets.RegisterLoader<ScriptAsset>(AssetType::Script, { ".luau", ".lua" },
        [](AssetManager2&, const AssetMetadata& meta) {
            std::string error;
            std::shared_ptr<ScriptAsset> script = Load(meta.path, error);
            if (!script)
                Logger::Error(error);
            else if (!script->error.empty())
                Logger::Warning(script->error);
            return script;
        });
}
