#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

// The editor's undo history: each step is a name and how to undo and redo it. Pushing a step
// drops what could be redone; past kMaxSteps the oldest steps are forgotten. Steps should check
// that what they touch still exists (an entity may have been deleted since).
class UndoStack
{
public:
    static constexpr size_t kMaxSteps = 256;

    // Records an edit that has already been done.
    void Push(std::string name, std::function<void()> undo, std::function<void()> redo);

    bool CanUndo() const { return !m_undo.empty(); }
    bool CanRedo() const { return !m_redo.empty(); }
    const std::string& UndoName() const { return m_undo.back().name; }
    const std::string& RedoName() const { return m_redo.back().name; }

    void Undo();
    void Redo();
    void Clear();

private:
    struct Step
    {
        std::string name;
        std::function<void()> undo;
        std::function<void()> redo;
    };

    std::vector<Step> m_undo;
    std::vector<Step> m_redo;
};
