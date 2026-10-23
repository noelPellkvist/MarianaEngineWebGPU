#include <Editor/TransformUndo.hpp>
#include <Editor/UndoStack.hpp>

#include <cstring>

namespace
{
    bool Same(const LocalTRS& a, const LocalTRS& b)
    {
        return std::memcmp(a.pos, b.pos, sizeof(a.pos)) == 0 && std::memcmp(a.rot_quat, b.rot_quat, sizeof(a.rot_quat)) == 0 &&
               std::memcmp(a.scl, b.scl, sizeof(a.scl)) == 0;
    }

    void Assign(Entity entity, const LocalTRS& value)
    {
        LocalTRS* trs = entity.IsValid() ? entity.Get<LocalTRS>() : nullptr;
        if (!trs || Same(*trs, value))
            return;
        std::memcpy(trs->pos, value.pos, sizeof(trs->pos));
        std::memcpy(trs->rot_quat, value.rot_quat, sizeof(trs->rot_quat));
        std::memcpy(trs->scl, value.scl, sizeof(trs->scl));
        ++trs->version;
    }
}

void TransformUndo::Track(Entity entity, const LocalTRS& before, bool held, UndoStack& undo, const char* stepName)
{
    const LocalTRS* now = entity.IsValid() ? entity.TryGet<LocalTRS>() : nullptr;
    const bool same = m_open && m_entity.RawId() == entity.RawId();
    if (now && !Same(*now, before) && !same)
    {
        if (m_open)
            Finish(undo);
        m_entity = entity;
        m_before = before;
        m_name = std::string(stepName) + " " + (entity.GetName() ? entity.GetName() : "entity");
        m_open = true;
    }
    if (m_open && m_entity.RawId() == entity.RawId() && !held)
        Finish(undo);
}

void TransformUndo::Finish(UndoStack& undo)
{
    m_open = false;
    const LocalTRS* now = m_entity.IsValid() ? m_entity.TryGet<LocalTRS>() : nullptr;
    if (!now || Same(*now, m_before))
        return;
    const LocalTRS old = m_before, changed = *now;
    Entity entity = m_entity;
    undo.Push(m_name, [entity, old]() { Assign(entity, old); }, [entity, changed]() { Assign(entity, changed); });
}
