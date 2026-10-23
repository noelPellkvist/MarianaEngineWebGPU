#pragma once

#include <Mariana/Scene/ECS.hpp>

#include <string>

class UndoStack;

// Turns changes to an entity's transform (LocalTRS) into undo steps, one per drag or edit: call
// Track every frame with the transform as it was before this frame's widgets ran and whether a
// widget is still held. The step is recorded once nothing is held, or when another entity's edit
// starts.
class TransformUndo
{
public:
    void Track(Entity entity, const LocalTRS& before, bool held, UndoStack& undo, const char* stepName = "Move");

private:
    void Finish(UndoStack& undo);

    Entity m_entity;
    LocalTRS m_before{};
    std::string m_name;
    bool m_open = false;
};
