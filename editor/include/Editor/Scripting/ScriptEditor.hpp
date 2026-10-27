#pragma once

#include <filesystem>
#include <string>

class AssetManager2;
class InspectorWindow;
class ScriptWorld;
class UndoStack;

// The editor's side of MarianaScripting (the library itself has no UI).
namespace ScriptEditor
{
    // The inspector's Scripts section (each script with its properties, its errors, and adding
    // scripts by drop, from the project's files or as a new file) and its Add Component entry.
    // Edits are undoable; made while playing, they reach the running scripts.
    void RegisterInspector(InspectorWindow& inspector, UndoStack& undo, ScriptWorld& scripts, AssetManager2& assets,
                           std::filesystem::path projectDir);

    // A new script from the template in `folder`, named `name` (made a valid identifier; a number
    // is added when the file exists). Returns the file, or an empty path when it couldn't be written.
    std::filesystem::path CreateScript(const std::filesystem::path& folder, const std::string& name);

    // Opens a file with the app the system has for it (natively; nothing on the web).
    void OpenWithSystem(const std::filesystem::path& file);
}
