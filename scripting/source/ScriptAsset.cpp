#include <Mariana/Scripting/Script.hpp>

#include <Mariana/Foundation/FileSystem.hpp>
#include <Mariana/Foundation/Logger.hpp>

#include "Luau/Compiler.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

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

std::string ScriptAsset::ResolveRequire(const std::string& name, const std::string& from)
{
    if (name.empty())
        return {};
    std::string path = name;
    if (name[0] != '/')
    {
        const size_t slash = from.rfind('/');
        path = (slash == std::string::npos || slash == 0 ? std::string("/Scripts") : from.substr(0, slash)) + "/" + name;
    }
    // "." and ".." taken out.
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= path.size())
    {
        const size_t end = std::min(path.find('/', start), path.size());
        const std::string part = path.substr(start, end - start);
        if (part == "..")
        {
            if (!parts.empty())
                parts.pop_back();
        }
        else if (!part.empty() && part != ".")
            parts.push_back(part);
        start = end + 1;
    }
    std::string clean;
    for (const std::string& part : parts)
        clean += "/" + part;
    if (clean.empty())
        return {};
    const bool hasExtension = clean.size() > 5 && (clean.ends_with(".luau") || clean.ends_with(".lua"));
    for (const std::string& candidate : hasExtension ? std::vector<std::string>{ clean } : std::vector<std::string>{ clean + ".luau", clean + ".lua" })
        if (!FileSystem::Resolve(candidate).empty())
            return candidate;
    return {};
}
