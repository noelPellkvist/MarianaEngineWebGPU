#pragma once

#include <glm/glm.hpp>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

class AssetManager2;
class InspectorWindow;
class ScriptChecker;
class ScriptWorld;
class UndoStack;
struct ScriptDebugShape;

// The editor's side of MarianaScripting (the library itself has no UI).
namespace ScriptEditor
{
    // The inspector's Scripts section (each script with its properties, its errors and what the type
    // checker found, and adding scripts by drop, from the project's files or as a new file) and its
    // Add Component entry. Edits are undoable; made while playing, they reach the running scripts.
    // A script's Edit button calls `edit` with its file.
    void RegisterInspector(InspectorWindow& inspector, UndoStack& undo, ScriptWorld& scripts, ScriptChecker& checker, AssetManager2& assets,
                           std::filesystem::path projectDir, std::function<void(const std::filesystem::path&)> edit);

    // The project's types for VS Code: the API's definitions in <project>/.mariana/types/ (rewritten
    // when they change) and, when the project has none, .vscode/settings.json pointing the Luau
    // Language Server at them.
    void WriteProjectTypes(const std::filesystem::path& projectDir);

    // A new script from the template in `folder`, named `name` (made a valid identifier; a number
    // is added when the file exists). Returns the file, or an empty path when it couldn't be written.
    std::filesystem::path CreateScript(const std::filesystem::path& folder, const std::string& name);

    // Opens a file with the app the system has for it (natively; nothing on the web).
    void OpenWithSystem(const std::filesystem::path& file);

    // The shapes scripts draw (gizmos) over a view's image: lines, spheres and boxes as lines (no
    // depth test), text at its point.
    void DrawGizmos(const std::vector<ScriptDebugShape>& shapes, const glm::mat4& viewProj, glm::vec2 imageMin, glm::vec2 imageSize);
}
