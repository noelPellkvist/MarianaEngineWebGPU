#pragma once
// What the inspector sections other modules add (physics, scripting) share: editing a component as
// undo steps, and a section header whose close button removes the component.
#include <Editor/UndoStack.hpp>
#include <Editor/Windows/InspectorWindow.hpp>
#include <Mariana/Scene/ECS.hpp>
#include <imgui.h>

#include <functional>
#include <string>

namespace InspectorComponents
{
    // Edits component T of `entity` through `draw` (which returns whether it changed something) as
    // one undo step per change: a drag or typing is one step, recorded once no widget is held.
    template <typename T>
    void EditComponent(Entity entity, UndoStack& undo, const char* stepName, const std::function<bool(T&)>& draw)
    {
        struct Pending
        {
            uint64_t entity = 0;
            T before{};
            bool open = false;
        };
        static Pending pending;

        T* value = entity.Get<T>();
        if (!value)
            return;
        const T before = *value;
        if (draw(*value) && !(pending.open && pending.entity == entity.RawId()))
        {
            pending.entity = entity.RawId();
            pending.before = before;
            pending.open = true;
        }
        if (pending.open && pending.entity == entity.RawId() && !ImGui::IsAnyItemActive())
        {
            pending.open = false;
            const T old = pending.before;
            const T now = *value;
            undo.Push(stepName,
                [entity, old]() mutable { if (entity.IsValid()) if (T* c = entity.Get<T>()) *c = old; },
                [entity, now]() mutable { if (entity.IsValid()) if (T* c = entity.Get<T>()) *c = now; });
        }
    }

    // A section header with a close button, which removes the component (deferred, undoable).
    template <typename T>
    bool Section(InspectorWindow& inspector, UndoStack& undo, Entity entity, const char* title)
    {
        bool keep = true;
        const bool open = ImGui::CollapsingHeader(title, &keep, ImGuiTreeNodeFlags_DefaultOpen);
        if (!keep)
        {
            inspector.Defer([entity, &undo, title]() mutable {
                const T* current = entity.TryGet<T>();
                if (!current)
                    return;
                const T copy = *current;
                entity.Remove<T>();
                undo.Push(std::string("Remove ") + title,
                    [entity, copy]() mutable { if (entity.IsValid()) entity.Add<T>(copy); },
                    [entity]() mutable { if (entity.IsValid()) entity.Remove<T>(); });
            });
        }
        return open && keep;
    }
}
