#pragma once

#include <Editor/Windows/EditorWindow.hpp>
#include <Mariana/Scripting/Script.hpp>
#include <Mariana/ScriptTools/ScriptChecker.hpp>
#include <Mariana/ScriptTools/ScriptTokens.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class AssetManager2;

// Edits scripts inside the editor: a tab per open script, syntax colours from Luau's lexer, the type
// checker's problems underlined and listed (checked as you type), autocomplete (Ctrl+Space, or by
// itself after '.', ':' and '"'; Up/Down pick, Enter or Tab take it, Esc closes), Ctrl+S saves, and
// the game takes the new code at once (also while playing).
class ScriptEditorWindow final : public EditorWindow
{
public:
    ScriptEditorWindow(GUI& gui, AssetManager2& assets, ScriptChecker& checker);
    ~ScriptEditorWindow() override;
    void Draw() override;

    // Opens a script file in a tab (or shows the tab it has), and the window.
    void Open(const std::filesystem::path& file);
    // Puts the open tab's cursor at a line and column (from 0) and the keyboard on the text.
    void GoTo(int line, int column);
    // The text of the open tab (empty when none is open).
    const std::string& Text() const;
    // Called with a script's file after it was saved (the web build hands it to the browser).
    std::function<void(const std::filesystem::path&)> onSaved;

    struct Tab;

private:
    AssetManager2& m_Assets;
    ScriptChecker& m_Checker;
    std::vector<std::unique_ptr<Tab>> m_Tabs;
    int m_Active = -1;
    int m_Select = -1; // a tab to bring to the front next frame

    void DrawTab(Tab& tab);
    void DrawCode(Tab& tab, float height);
    void DrawCompletions(Tab& tab);
    void Save(Tab& tab);
};
