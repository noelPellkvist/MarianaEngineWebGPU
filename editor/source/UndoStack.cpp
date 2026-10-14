#include <Editor/UndoStack.hpp>

#include <utility>

void UndoStack::Push(std::string name, std::function<void()> undo, std::function<void()> redo)
{
    m_redo.clear();
    m_undo.push_back({ std::move(name), std::move(undo), std::move(redo) });
    if (m_undo.size() > kMaxSteps)
        m_undo.erase(m_undo.begin());
}

void UndoStack::Undo()
{
    if (m_undo.empty())
        return;
    Step step = std::move(m_undo.back());
    m_undo.pop_back();
    step.undo();
    m_redo.push_back(std::move(step));
}

void UndoStack::Redo()
{
    if (m_redo.empty())
        return;
    Step step = std::move(m_redo.back());
    m_redo.pop_back();
    step.redo();
    m_undo.push_back(std::move(step));
}

void UndoStack::Clear()
{
    m_undo.clear();
    m_redo.clear();
}
