#include <Editor/EditorWindows/Base.hpp>
#include <imgui.h>

EditorWindowBase::EditorWindowBase(std::string name, bool visible)
    : m_name(std::move(name))
    , m_visible(visible)
{
}

void EditorWindowBase::Draw()
{
    if (!m_visible) {
        return;
    }

    bool open = m_visible;
    if (ImGui::Begin(m_name.c_str(), &open, m_flags)) {
        DrawContent();
    }
    ImGui::End();
    m_visible = open;
}
