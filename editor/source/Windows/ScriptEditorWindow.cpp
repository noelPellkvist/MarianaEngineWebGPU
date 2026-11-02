#include <Editor/Windows/ScriptEditorWindow.hpp>

#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Foundation/FileSystem.hpp>
#include <Mariana/Foundation/Logger.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>

namespace fs = std::filesystem;

struct ScriptEditorWindow::Tab
{
    AssetHandle<ScriptAsset> script;
    std::string name;
    std::string text;
    uint32_t version = 0; // the script's when the text was read or saved
    bool dirty = false;

    // What the drawing needs, made again when the text changes.
    std::vector<ScriptToken> tokens;
    std::vector<int> lineStarts;
    float width = 0.0f;
    bool layoutDirty = true;

    std::vector<ScriptDiagnostic> problems;
    double editedAt = 0.0;
    bool checkPending = true;

    // The caret as the text field last had it (byte offsets).
    int cursor = 0;
    int selectionStart = 0;
    int selectionEnd = 0;
    bool active = false;
    int jumpTo = -1;          // where to put the caret once the field is active
    bool focusText = false;
    bool keepCursor = false;  // Up / Down went to the completion list, not the caret
    int scrolledFor = -1;     // the caret position the view last followed

    // Autocomplete.
    bool requestCompletion = false;
    bool completing = false;
    bool accept = false;
    int wordStart = 0;
    int selected = 0;
    std::vector<ScriptCompletion> all;
    std::vector<ScriptCompletion> shown;
};

namespace
{
    // What the checker calls a tab's script: its resource path, so its requires resolve as in the game.
    std::string CheckName(const ScriptEditorWindow::Tab& tab)
    {
        const std::shared_ptr<ScriptAsset>& asset = tab.script.Get();
        const std::string resource = asset && !asset->file.empty() ? FileSystem::ToResourcePath(asset->file) : std::string();
        return resource.empty() ? tab.name : resource;
    }

    bool IsWordChar(unsigned int c)
    {
        return c < 128 && (std::isalnum(static_cast<int>(c)) || c == '_');
    }

    void Relayout(ScriptEditorWindow::Tab& tab)
    {
        tab.tokens = ScriptTokens(tab.text);
        tab.lineStarts.assign(1, 0);
        tab.width = 0.0f;
        int start = 0;
        for (int i = 0; i <= static_cast<int>(tab.text.size()); ++i)
        {
            if (i == static_cast<int>(tab.text.size()) || tab.text[static_cast<size_t>(i)] == '\n')
            {
                tab.width = std::max(tab.width, ImGui::CalcTextSize(tab.text.data() + start, tab.text.data() + i).x);
                if (i < static_cast<int>(tab.text.size()))
                {
                    start = i + 1;
                    tab.lineStarts.push_back(start);
                }
            }
        }
        tab.layoutDirty = false;
    }

    // The line and column (bytes) of an offset.
    std::pair<int, int> LineColumn(const ScriptEditorWindow::Tab& tab, int offset)
    {
        const auto it = std::upper_bound(tab.lineStarts.begin(), tab.lineStarts.end(), offset);
        const int line = static_cast<int>(it - tab.lineStarts.begin()) - 1;
        return { line, offset - tab.lineStarts[static_cast<size_t>(std::max(line, 0))] };
    }

    int Offset(const ScriptEditorWindow::Tab& tab, int line, int column)
    {
        line = std::clamp(line, 0, static_cast<int>(tab.lineStarts.size()) - 1);
        const int start = tab.lineStarts[static_cast<size_t>(line)];
        const int end = line + 1 < static_cast<int>(tab.lineStarts.size()) ? tab.lineStarts[static_cast<size_t>(line) + 1] - 1 : static_cast<int>(tab.text.size());
        return std::clamp(start + column, start, end);
    }

    // Where a line and column is on the screen, from where the text starts.
    ImVec2 At(const ScriptEditorWindow::Tab& tab, ImVec2 origin, int line, int column)
    {
        const int start = Offset(tab, line, 0);
        const int at = Offset(tab, line, column);
        return { origin.x + ImGui::CalcTextSize(tab.text.data() + start, tab.text.data() + at).x, origin.y + static_cast<float>(line) * ImGui::GetFontSize() };
    }

    ImU32 TokenColor(ScriptToken::Kind kind)
    {
        switch (kind)
        {
        case ScriptToken::Kind::Keyword: return IM_COL32(198, 120, 221, 255);
        case ScriptToken::Kind::Library: return IM_COL32(86, 182, 194, 255);
        case ScriptToken::Kind::Number: return IM_COL32(209, 154, 102, 255);
        case ScriptToken::Kind::String: return IM_COL32(152, 195, 121, 255);
        case ScriptToken::Kind::Comment: return IM_COL32(110, 118, 129, 255);
        case ScriptToken::Kind::Operator: return IM_COL32(171, 178, 191, 255);
        case ScriptToken::Kind::Name: break;
        }
        return IM_COL32(220, 223, 228, 255);
    }

    const char* KindLetter(ScriptCompletion::Kind kind)
    {
        switch (kind)
        {
        case ScriptCompletion::Kind::Method: return "m";
        case ScriptCompletion::Kind::Property: return "p";
        case ScriptCompletion::Kind::Keyword: return "k";
        case ScriptCompletion::Kind::String: return "s";
        case ScriptCompletion::Kind::Type: return "t";
        case ScriptCompletion::Kind::Variable: break;
        }
        return "v";
    }

    int TextCallback(ImGuiInputTextCallbackData* data)
    {
        auto& tab = *static_cast<ScriptEditorWindow::Tab*>(data->UserData);
        if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter)
        {
            const ImWchar c = data->EventChar;
            // Enter and Tab take the completion when the list is open.
            if (tab.completing && (c == '\n' || c == '\t'))
            {
                tab.accept = true;
                return 1;
            }
            if (c == '.' || c == ':' || c == '"')
                tab.requestCompletion = true;
            else if (!IsWordChar(c))
                tab.completing = false;
            return 0;
        }
        if (data->EventFlag != ImGuiInputTextFlags_CallbackAlways)
            return 0;
        if (tab.jumpTo >= 0)
        {
            data->CursorPos = data->SelectionStart = data->SelectionEnd = std::min(tab.jumpTo, data->BufTextLen);
            tab.jumpTo = -1;
        }
        if (tab.keepCursor)
        {
            data->CursorPos = tab.cursor;
            data->SelectionStart = tab.selectionStart;
            data->SelectionEnd = tab.selectionEnd;
            tab.keepCursor = false;
        }
        if (tab.accept)
        {
            tab.accept = false;
            if (tab.completing && tab.selected < static_cast<int>(tab.shown.size()))
            {
                const int end = data->CursorPos;
                const int start = std::clamp(tab.wordStart, 0, end);
                data->DeleteChars(start, end - start);
                data->InsertChars(start, tab.shown[static_cast<size_t>(tab.selected)].text.c_str());
            }
            tab.completing = false;
        }
        tab.cursor = data->CursorPos;
        tab.selectionStart = data->SelectionStart;
        tab.selectionEnd = data->SelectionEnd;
        return 0;
    }
}

ScriptEditorWindow::ScriptEditorWindow(GUI& gui, AssetManager2& assets, ScriptChecker& checker)
    : EditorWindow("Script Editor", gui, false), m_Assets(assets), m_Checker(checker)
{
}

ScriptEditorWindow::~ScriptEditorWindow() = default;

void ScriptEditorWindow::Open(const fs::path& file)
{
    AssetHandle<ScriptAsset> script = m_Assets.Load<ScriptAsset>(file);
    if (!script.Get())
    {
        Logger::Error("Could not open the script " + file.string());
        return;
    }
    m_open = true;
    for (size_t i = 0; i < m_Tabs.size(); ++i)
        if (m_Tabs[i]->script.GetID() == script.GetID())
        {
            m_Select = static_cast<int>(i);
            return;
        }
    auto tab = std::make_unique<Tab>();
    tab->script = script;
    tab->name = file.filename().string();
    tab->text = script.Get()->source;
    tab->version = script.Get()->Version();
    m_Tabs.push_back(std::move(tab));
    m_Select = static_cast<int>(m_Tabs.size()) - 1;
}

void ScriptEditorWindow::GoTo(int line, int column)
{
    if (m_Active < 0 || m_Active >= static_cast<int>(m_Tabs.size()))
        return;
    Tab& tab = *m_Tabs[static_cast<size_t>(m_Active)];
    if (tab.layoutDirty)
        Relayout(tab);
    tab.jumpTo = Offset(tab, line, column);
    tab.focusText = true;
}

const std::string& ScriptEditorWindow::Text() const
{
    static const std::string none;
    return m_Active >= 0 && m_Active < static_cast<int>(m_Tabs.size()) ? m_Tabs[static_cast<size_t>(m_Active)]->text : none;
}

void ScriptEditorWindow::Save(Tab& tab)
{
    const std::shared_ptr<ScriptAsset>& script = tab.script.Get();
    if (!script || script->file.empty())
        return;
    std::ofstream out(script->file, std::ios::binary);
    if (!out)
    {
        Logger::Error("Could not write " + script->file.string());
        return;
    }
    out << tab.text;
    out.close();
    // The running game takes it at once (its instances keep their state).
    script->SetSource(tab.text);
    tab.version = script->Version();
    tab.dirty = false;
    Logger::Info("Saved " + script->file.filename().string());
    if (onSaved)
        onSaved(script->file);
}

void ScriptEditorWindow::Draw()
{
    if (!m_open)
        return;
    ImGui::SetNextWindowSize(ImVec2(820.0f, 620.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(m_name.c_str(), &m_open))
    {
        ImGui::End();
        return;
    }
    if (m_Tabs.empty())
        ImGui::TextDisabled("Double-click a .luau file in Assets, or Edit a script in the inspector, to open it here.");

    if (ImGui::BeginTabBar("##scripts", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs))
    {
        int close = -1;
        for (size_t i = 0; i < m_Tabs.size(); ++i)
        {
            Tab& tab = *m_Tabs[i];
            bool keep = true;
            ImGuiTabItemFlags flags = tab.dirty ? ImGuiTabItemFlags_UnsavedDocument : 0;
            if (m_Select == static_cast<int>(i))
            {
                flags |= ImGuiTabItemFlags_SetSelected;
                m_Select = -1;
            }
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::BeginTabItem(tab.name.c_str(), &keep, flags))
            {
                m_Active = static_cast<int>(i);
                DrawTab(tab);
                ImGui::EndTabItem();
            }
            ImGui::PopID();
            if (!keep)
                close = static_cast<int>(i);
        }
        ImGui::EndTabBar();
        if (close >= 0)
        {
            m_Tabs.erase(m_Tabs.begin() + close);
            m_Active = std::min(m_Active, static_cast<int>(m_Tabs.size()) - 1);
        }
    }
    ImGui::End();
}

void ScriptEditorWindow::DrawTab(Tab& tab)
{
    const std::shared_ptr<ScriptAsset>& script = tab.script.Get();
    // Changed on disk (or by another editor) while nothing was typed here: show the new text.
    if (script && script->Version() != tab.version && !tab.dirty)
    {
        tab.text = script->source;
        tab.version = script->Version();
        tab.layoutDirty = true;
        tab.checkPending = true;
    }

    if (ImGui::Button("Save") || ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteFocused))
        Save(tab);
    ImGui::SameLine();
    const int errors = static_cast<int>(tab.problems.size());
    if (errors == 0)
        ImGui::TextDisabled(tab.checkPending ? "checking..." : "no problems");
    else
        ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.3f, 1.0f), "%d %s", errors, errors == 1 ? "problem" : "problems");
    ImGui::SameLine();
    ImGui::TextDisabled("  Ctrl+Space completes, Ctrl+S saves");

    // The problems under the text, a few lines tall when there are any.
    const float problemsHeight = tab.problems.empty() ? 0.0f
                                                      : std::min(5.0f, static_cast<float>(tab.problems.size())) * ImGui::GetTextLineHeightWithSpacing() +
                                                            ImGui::GetStyle().WindowPadding.y * 2.0f + ImGui::GetStyle().ItemSpacing.y;
    DrawCode(tab, ImGui::GetContentRegionAvail().y - problemsHeight);
    if (!tab.problems.empty() && ImGui::BeginChild("##problems", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
    {
        for (const ScriptDiagnostic& d : tab.problems)
        {
            ImGui::PushID(&d);
            ImGui::PushStyleColor(ImGuiCol_Text, d.syntax ? ImVec4(1.0f, 0.45f, 0.4f, 1.0f) : ImVec4(1.0f, 0.75f, 0.3f, 1.0f));
            const std::string row = "line " + std::to_string(d.line + 1) + ": " + d.message;
            if (ImGui::Selectable(row.c_str()))
                GoTo(d.line, d.column);
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
    }
    if (!tab.problems.empty())
        ImGui::EndChild();

    // Checked again a moment after typing stops.
    if (tab.checkPending && ImGui::GetTime() - tab.editedAt > 0.3)
    {
        tab.problems = m_Checker.Check(CheckName(tab), tab.text);
        tab.checkPending = false;
    }
}

void ScriptEditorWindow::DrawCode(Tab& tab, float height)
{
    if (tab.layoutDirty)
        Relayout(tab);
    const ImGuiStyle& style = ImGui::GetStyle();
    const float lineHeight = ImGui::GetFontSize();
    const int lines = static_cast<int>(tab.lineStarts.size());

    ImGui::PushStyleColor(ImGuiCol_ChildBg, style.Colors[ImGuiCol_FrameBg]);
    const bool visible = ImGui::BeginChild("##code", ImVec2(0.0f, std::max(height, 4.0f * lineHeight)), ImGuiChildFlags_Borders,
                                           ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PopStyleColor();
    if (!visible)
    {
        ImGui::EndChild();
        return;
    }

    // Keys meant for the completion list, taken before the text field sees them.
    const ImGuiID listId = ImGui::GetID("##completion");
    if (tab.active)
    {
        if (tab.completing)
        {
            const int count = static_cast<int>(tab.shown.size());
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            {
                tab.selected = (tab.selected + count - 1) % std::max(count, 1);
                tab.keepCursor = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            {
                tab.selected = (tab.selected + 1) % std::max(count, 1);
                tab.keepCursor = true;
            }
        }
        // Esc closes the list; it mustn't reach the text field, which would undo the typing.
        ImGui::SetKeyOwner(ImGuiKey_Escape, listId, ImGuiInputFlags_LockThisFrame);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, listId))
            tab.completing = false;
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Space))
            tab.requestCompletion = true;
    }

    // The gutter with the line numbers, then the text: as big as it is, so the child scrolls it.
    const float gutter = ImGui::CalcTextSize("00000").x;
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float textHeight = (static_cast<float>(lines) + 2.0f) * lineHeight + style.FramePadding.y * 2.0f;
    const float textWidth = std::max(tab.width + ImGui::CalcTextSize("    ").x + style.FramePadding.x * 2.0f, ImGui::GetContentRegionAvail().x - gutter);
    ImGui::SetCursorScreenPos(ImVec2(start.x + gutter, start.y));
    if (tab.focusText)
    {
        ImGui::SetKeyboardFocusHere();
        tab.focusText = false;
    }
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
    const ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_CallbackCharFilter |
                                      ImGuiInputTextFlags_NoHorizontalScroll;
    const bool changed = ImGui::InputTextMultiline("##text", &tab.text, ImVec2(textWidth, textHeight), flags, TextCallback, &tab);
    ImGui::PopStyleColor(3);
    tab.active = ImGui::IsItemActive();
    const ImVec2 origin(ImGui::GetItemRectMin().x + style.FramePadding.x, ImGui::GetItemRectMin().y + style.FramePadding.y);
    if (changed)
    {
        tab.dirty = true;
        tab.checkPending = true;
        tab.editedAt = ImGui::GetTime();
        Relayout(tab);
    }
    if (!tab.active)
        tab.completing = false;

    // The colours: each token drawn where the field has its (transparent) text.
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float top = ImGui::GetWindowPos().y - lineHeight;
    const float bottom = ImGui::GetWindowPos().y + ImGui::GetWindowHeight();
    for (int line = 0; line < lines; ++line)
    {
        const float y = origin.y + static_cast<float>(line) * lineHeight;
        if (y < top || y > bottom)
            continue;
        char number[16];
        std::snprintf(number, sizeof(number), "%4d", line + 1);
        draw->AddText(ImVec2(start.x, y), IM_COL32(100, 106, 116, 255), number);
    }
    for (const ScriptToken& token : tab.tokens)
    {
        if (origin.y + static_cast<float>(token.endLine) * lineHeight < top || origin.y + static_cast<float>(token.line) * lineHeight > bottom)
            continue;
        for (int line = token.line; line <= token.endLine; ++line)
        {
            const int from = Offset(tab, line, line == token.line ? token.column : 0);
            const int to = line == token.endLine ? Offset(tab, line, token.endColumn) : Offset(tab, line, 1 << 20);
            if (to > from)
                draw->AddText(At(tab, origin, line, from - Offset(tab, line, 0)), TokenColor(token.kind), tab.text.data() + from, tab.text.data() + to);
        }
    }

    // The problems, underlined with a zigzag; hovering one says what it is.
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    for (const ScriptDiagnostic& d : tab.problems)
    {
        const ImU32 color = d.syntax ? IM_COL32(240, 90, 80, 255) : IM_COL32(240, 170, 60, 255);
        for (int line = d.line; line <= std::max(d.line, d.endLine); ++line)
        {
            const int from = line == d.line ? d.column : 0;
            int to = line == d.endLine ? d.endColumn : (1 << 20);
            ImVec2 a = At(tab, origin, line, from);
            ImVec2 b = At(tab, origin, line, to);
            if (b.x - a.x < 4.0f)
                b.x = a.x + ImGui::CalcTextSize("x").x;
            const float y = a.y + lineHeight - 1.0f;
            for (float x = a.x; x < b.x; x += 4.0f)
            {
                draw->AddLine(ImVec2(x, y), ImVec2(std::min(x + 2.0f, b.x), y - 2.0f), color, 1.0f);
                draw->AddLine(ImVec2(std::min(x + 2.0f, b.x), y - 2.0f), ImVec2(std::min(x + 4.0f, b.x), y), color, 1.0f);
            }
            if (mouse.x >= a.x && mouse.x <= b.x && mouse.y >= a.y && mouse.y <= a.y + lineHeight && ImGui::IsWindowHovered())
                ImGui::SetTooltip("%s", d.message.c_str());
        }
    }

    // The view follows the caret when it moves.
    if (tab.active && tab.cursor != tab.scrolledFor)
    {
        tab.scrolledFor = tab.cursor;
        const auto [line, column] = LineColumn(tab, tab.cursor);
        const ImVec2 caret = At(tab, origin, line, column);
        const float viewTop = ImGui::GetWindowPos().y, viewBottom = viewTop + ImGui::GetWindowHeight() - lineHeight * 2.0f;
        if (caret.y < viewTop)
            ImGui::SetScrollY(ImGui::GetScrollY() - (viewTop - caret.y));
        else if (caret.y > viewBottom)
            ImGui::SetScrollY(ImGui::GetScrollY() + (caret.y - viewBottom));
        const float viewLeft = ImGui::GetWindowPos().x + gutter, viewRight = ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - 40.0f;
        if (caret.x < viewLeft)
            ImGui::SetScrollX(ImGui::GetScrollX() - (viewLeft - caret.x));
        else if (caret.x > viewRight)
            ImGui::SetScrollX(ImGui::GetScrollX() + (caret.x - viewRight));
    }

    // Completions: asked for after '.', ':', '"' or Ctrl+Space, then narrowed by what is typed.
    if (tab.requestCompletion && tab.active)
    {
        tab.requestCompletion = false;
        int wordStart = std::clamp(tab.cursor, 0, static_cast<int>(tab.text.size()));
        while (wordStart > 0 && IsWordChar(static_cast<unsigned char>(tab.text[static_cast<size_t>(wordStart) - 1])))
            --wordStart;
        tab.wordStart = wordStart;
        const auto [line, column] = LineColumn(tab, tab.cursor);
        tab.all = m_Checker.Complete(CheckName(tab), tab.text, line, column);
        tab.completing = !tab.all.empty();
        tab.selected = 0;
    }
    if (tab.completing)
    {
        std::string prefix;
        if (tab.cursor >= tab.wordStart && tab.cursor <= static_cast<int>(tab.text.size()))
            prefix = tab.text.substr(static_cast<size_t>(tab.wordStart), static_cast<size_t>(tab.cursor - tab.wordStart));
        else
            tab.completing = false;
        std::transform(prefix.begin(), prefix.end(), prefix.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        tab.shown.clear();
        for (const ScriptCompletion& c : tab.all)
        {
            std::string lower = c.text;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            if (lower.rfind(prefix, 0) == 0)
                tab.shown.push_back(c);
        }
        if (tab.shown.empty())
            tab.completing = false;
        tab.selected = std::clamp(tab.selected, 0, std::max(static_cast<int>(tab.shown.size()) - 1, 0));
    }
    if (tab.completing)
    {
        const auto [line, column] = LineColumn(tab, tab.wordStart);
        const ImVec2 at = At(tab, origin, line, column);
        ImGui::SetNextWindowPos(ImVec2(at.x, at.y + lineHeight + 2.0f));
        DrawCompletions(tab);
    }

    ImGui::EndChild();
}

void ScriptEditorWindow::DrawCompletions(Tab& tab)
{
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_Tooltip;
    if (!ImGui::Begin("##completions", nullptr, flags))
    {
        ImGui::End();
        return;
    }
    // Ten rows around the selected one.
    const int count = static_cast<int>(tab.shown.size());
    const int first = std::clamp(tab.selected - 4, 0, std::max(count - 10, 0));
    for (int i = first; i < count && i < first + 10; ++i)
    {
        const ScriptCompletion& c = tab.shown[static_cast<size_t>(i)];
        const bool selected = i == tab.selected;
        ImGui::TextColored(ImVec4(0.55f, 0.6f, 0.68f, 1.0f), "%s", KindLetter(c.kind));
        ImGui::SameLine();
        if (selected)
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.45f, 1.0f), "%s", c.text.c_str());
        else
            ImGui::TextUnformatted(c.text.c_str());
        if (!c.detail.empty() && selected)
        {
            std::string detail = c.detail.size() > 70 ? c.detail.substr(0, 67) + "..." : c.detail;
            ImGui::SameLine();
            ImGui::TextDisabled("%s", detail.c_str());
        }
    }
    if (count > 10)
        ImGui::TextDisabled("%d more", count - 10);
    ImGui::End();
}
