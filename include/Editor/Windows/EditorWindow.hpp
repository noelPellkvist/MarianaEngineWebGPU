#pragma once

#include <string>
#include <GUI.hpp>

class EditorWindow
{
public:
    explicit EditorWindow(std::string name, GUI& gui, bool open = true)
        : m_name(std::move(name)), m_gui(gui), m_open(open)
    {
    }

    virtual ~EditorWindow() = default;

    const std::string& Name() const { return m_name; }
    bool IsOpen() const { return m_open; }
    void SetOpen(bool open) { m_open = open; }

    virtual void Draw() = 0;

protected:
    std::string m_name;
    bool m_open;
    GUI& m_gui;
};
