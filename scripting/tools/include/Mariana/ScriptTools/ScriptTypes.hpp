#pragma once
#include <filesystem>
#include <string>

// The Mariana script API as a Luau definitions file, for type checking and autocomplete.
namespace ScriptTypes
{
    // The hand-written core (scripting/tools/resources/Core.d.luau) with a type per
    // component the app registered (its fields as scene files save them: enums as their names,
    // two or three floats as vectors) and entity:Get / Add overloads by component name. Call after
    // the app registered its components.
    std::string Definitions();
    // Writes the definitions to `file` when it holds something else (VS Code's luau-lsp reads it).
    bool Write(const std::filesystem::path& file, std::string& error);
}
