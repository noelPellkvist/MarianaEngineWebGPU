#include <Editor/Physics/PhysicsEditor.hpp>

#include <Editor/UndoStack.hpp>
#include <Editor/Windows/InspectorWindow.hpp>
#include <Mariana/Graphics/Mesh.hpp>
#include <Mariana/Physics/ColliderGeometry.hpp>
#include <Mariana/Physics/PhysicsWorld.hpp>
#include <Mariana/Rendering/Components.hpp>
#include <imgui.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>

namespace
{
    const char* const kMotionNames[] = { "Static", "Kinematic", "Dynamic" };

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

    template <typename T>
    void AddWithUndo(UndoStack& undo, Entity entity, const char* name, const T& value)
    {
        if (entity.TryGet<T>())
            return;
        entity.Add<T>(value);
        PhysicsEditor::FitCollidersToMesh(entity);
        const T added = *entity.TryGet<T>();
        undo.Push(std::string("Add ") + name,
            [entity]() mutable { if (entity.IsValid()) entity.Remove<T>(); },
            [entity, added]() mutable { if (entity.IsValid()) entity.Add<T>(added); });
    }

    bool DragVec3(const char* label, glm::vec3& v, float speed = 0.01f)
    {
        return ImGui::DragFloat3(label, glm::value_ptr(v), speed);
    }

    // The collider fields every shape has.
    bool DrawColliderCommon(Collider& c)
    {
        bool changed = DragVec3("Center", c.center);
        glm::vec3 euler = glm::degrees(glm::eulerAngles(c.rotation));
        for (int i = 0; i < 3; ++i)
            if (std::abs(euler[i]) < 0.0005f)
                euler[i] = 0.0f; // not "-0.000"
        if (DragVec3("Rotation", euler, 0.5f))
        {
            c.rotation = glm::normalize(glm::quat(glm::radians(euler)));
            changed = true;
        }
        changed |= ImGui::SliderFloat("Friction", &c.friction, 0.0f, 2.0f, "%.2f");
        changed |= ImGui::SliderFloat("Bounciness", &c.restitution, 0.0f, 1.0f, "%.2f");
        if (ImGui::DragFloat("Density", &c.density, 10.0f, 1.0f, 100000.0f, "%.0f kg/m3"))
        {
            c.density = std::max(c.density, 1.0f);
            changed = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Gives the mass of rigid bodies whose mass is 0");
        changed |= ImGui::Checkbox("Is Trigger", &c.isTrigger);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Doesn't collide; reports what enters it");
        return changed;
    }

    bool FitButton(Entity entity)
    {
        const MeshComponent* mesh = entity.TryGet<MeshComponent>();
        if (!mesh || !mesh->mesh.Get() || !mesh->mesh.Get()->hasBounds)
            return false;
        return ImGui::SmallButton("Fit to mesh");
    }

    // The body's state while the simulation runs.
    void DrawLive(const PhysicsWorld& world, Entity entity)
    {
        BodyInfo info;
        if (!world.GetBodyInfo(entity, info))
        {
            ImGui::TextDisabled("Simulated while playing");
            return;
        }
        if (info.motion == MotionType::Dynamic)
            ImGui::TextDisabled("%s, %.2f m/s, %.1f kg", info.sleeping ? "Sleeping" : "Moving", glm::length(info.linearVelocity), info.mass);
        else
            ImGui::TextDisabled("%s body%s", kMotionNames[static_cast<int>(info.motion)], info.sensor ? " (trigger)" : "");
    }

    // Lines from world space onto the viewport image, cut at the near plane.
    struct Painter
    {
        ImDrawList* drawList;
        glm::mat4 viewProj;
        glm::vec2 min, size;

        ImVec2 Screen(const glm::vec4& clip) const
        {
            const glm::vec2 ndc = glm::vec2(clip) / clip.w;
            return ImVec2(min.x + (ndc.x * 0.5f + 0.5f) * size.x, min.y + (0.5f - ndc.y * 0.5f) * size.y);
        }

        void Line(const glm::vec3& a, const glm::vec3& b, ImU32 color) const
        {
            constexpr float kNear = 1e-3f;
            glm::vec4 ca = viewProj * glm::vec4(a, 1.0f);
            glm::vec4 cb = viewProj * glm::vec4(b, 1.0f);
            if (ca.w < kNear && cb.w < kNear)
                return;
            if (ca.w < kNear)
                ca = glm::mix(ca, cb, (kNear - ca.w) / (cb.w - ca.w));
            else if (cb.w < kNear)
                cb = glm::mix(cb, ca, (kNear - cb.w) / (ca.w - cb.w));
            drawList->AddLine(Screen(ca), Screen(cb), color, 1.5f);
        }

        // An arc around `center` in the plane of `u` and `v`, from angle `from` to `to`.
        void Arc(const glm::vec3& center, const glm::vec3& u, const glm::vec3& v, float radius, float from, float to, ImU32 color, int segments = 24) const
        {
            glm::vec3 previous = center + (u * std::cos(from) + v * std::sin(from)) * radius;
            for (int i = 1; i <= segments; ++i)
            {
                const float angle = from + (to - from) * static_cast<float>(i) / segments;
                const glm::vec3 point = center + (u * std::cos(angle) + v * std::sin(angle)) * radius;
                Line(previous, point, color);
                previous = point;
            }
        }
    };

    void DrawGeometry(const Painter& painter, const ColliderGeometry& g)
    {
        const ImU32 color = g.collider->isTrigger ? IM_COL32(90, 210, 255, 220) : IM_COL32(110, 235, 130, 220);
        const glm::vec3 x = g.rotation * glm::vec3(1, 0, 0), y = g.rotation * glm::vec3(0, 1, 0), z = g.rotation * glm::vec3(0, 0, 1);
        const float pi = glm::pi<float>(), twoPi = glm::two_pi<float>();
        switch (g.shape)
        {
        case ColliderShape::Box:
        {
            glm::vec3 corners[8];
            for (int i = 0; i < 8; ++i)
                corners[i] = g.position + x * (i & 1 ? g.halfExtents.x : -g.halfExtents.x) + y * (i & 2 ? g.halfExtents.y : -g.halfExtents.y) + z * (i & 4 ? g.halfExtents.z : -g.halfExtents.z);
            for (int i = 0; i < 8; ++i)
                for (int bit : { 1, 2, 4 })
                    if (!(i & bit))
                        painter.Line(corners[i], corners[i | bit], color);
            break;
        }
        case ColliderShape::Sphere:
            painter.Arc(g.position, x, y, g.radius, 0.0f, twoPi, color, 32);
            painter.Arc(g.position, y, z, g.radius, 0.0f, twoPi, color, 32);
            painter.Arc(g.position, z, x, g.radius, 0.0f, twoPi, color, 32);
            break;
        case ColliderShape::Capsule:
        case ColliderShape::Cylinder:
        {
            const glm::vec3 top = g.position + y * g.halfHeight, bottom = g.position - y * g.halfHeight;
            painter.Arc(top, x, z, g.radius, 0.0f, twoPi, color, 32);
            painter.Arc(bottom, x, z, g.radius, 0.0f, twoPi, color, 32);
            for (const glm::vec3& side : { x, -x, z, -z })
                painter.Line(top + side * g.radius, bottom + side * g.radius, color);
            if (g.shape == ColliderShape::Capsule)
            {
                // The rounded ends, in the two planes through the axis.
                painter.Arc(top, x, y, g.radius, 0.0f, pi, color);
                painter.Arc(top, z, y, g.radius, 0.0f, pi, color);
                painter.Arc(bottom, x, y, g.radius, pi, twoPi, color);
                painter.Arc(bottom, z, y, g.radius, pi, twoPi, color);
            }
            break;
        }
        }
    }
}

namespace PhysicsEditor
{
    bool FitCollidersToMesh(Entity entity)
    {
        const MeshComponent* mesh = entity.TryGet<MeshComponent>();
        if (!mesh || !mesh->mesh.Get() || !mesh->mesh.Get()->hasBounds)
            return false;
        const glm::vec3 min = mesh->mesh.Get()->boundsMin, max = mesh->mesh.Get()->boundsMax;
        glm::vec3 size = max - min, center = (min + max) * 0.5f;
        // A flat mesh (a ground plane) gets a slab below its surface.
        for (int axis = 0; axis < 3; ++axis)
        {
            if (size[axis] < 1e-3f)
            {
                size[axis] = 0.1f;
                center[axis] -= 0.05f;
            }
        }
        if (BoxCollider* box = entity.Get<BoxCollider>())
        {
            box->size = size;
            box->center = center;
        }
        if (SphereCollider* sphere = entity.Get<SphereCollider>())
        {
            sphere->radius = 0.5f * std::max({ size.x, size.y, size.z });
            sphere->center = center;
        }
        if (CapsuleCollider* capsule = entity.Get<CapsuleCollider>())
        {
            capsule->radius = 0.5f * std::max(size.x, size.z);
            capsule->height = std::max(size.y, 2.0f * capsule->radius);
            capsule->center = center;
        }
        if (CylinderCollider* cylinder = entity.Get<CylinderCollider>())
        {
            cylinder->radius = 0.5f * std::max(size.x, size.z);
            cylinder->height = size.y;
            cylinder->center = center;
        }
        return true;
    }

    void RegisterInspector(InspectorWindow& inspector, UndoStack& undo, const PhysicsWorld& world)
    {
        inspector.SetComponentDrawer("RigidBody", [&inspector, &undo, &world](Entity& entity) {
            ImGui::PushID("RigidBody");
            if (Section<RigidBody>(inspector, undo, entity, "Rigid Body"))
            {
                EditComponent<RigidBody>(entity, undo, "Edit Rigid Body", [&](RigidBody& b) {
                    bool changed = false;
                    int motion = static_cast<int>(b.motion);
                    if (ImGui::Combo("Motion", &motion, kMotionNames, 3))
                    {
                        b.motion = static_cast<MotionType>(motion);
                        changed = true;
                    }
                    if (b.motion == MotionType::Dynamic)
                    {
                        if (ImGui::DragFloat("Mass", &b.mass, 0.1f, 0.0f, 1e6f, b.mass > 0.0f ? "%.2f kg" : "from density"))
                        {
                            b.mass = std::max(b.mass, 0.0f);
                            changed = true;
                        }
                        changed |= ImGui::SliderFloat("Linear Damping", &b.linearDamping, 0.0f, 5.0f, "%.2f");
                        changed |= ImGui::SliderFloat("Angular Damping", &b.angularDamping, 0.0f, 5.0f, "%.2f");
                        changed |= ImGui::DragFloat("Gravity Factor", &b.gravityFactor, 0.01f, -10.0f, 10.0f, "%.2f");
                        changed |= ImGui::Checkbox("Continuous", &b.continuous);
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Sweeps it between steps so it doesn't pass through thin bodies when fast");
                        ImGui::SameLine();
                        changed |= ImGui::Checkbox("Can Sleep", &b.allowSleeping);

                        const char* axes[] = { "X", "Y", "Z" };
                        ImGui::TextDisabled("Lock Position");
                        for (int i = 0; i < 3; ++i)
                        {
                            ImGui::SameLine();
                            bool locked = (b.lockedAxes >> i) & 1;
                            ImGui::PushID(i);
                            if (ImGui::Checkbox(axes[i], &locked))
                            {
                                b.lockedAxes = static_cast<uint8_t>(locked ? b.lockedAxes | (1 << i) : b.lockedAxes & ~(1 << i));
                                changed = true;
                            }
                            ImGui::PopID();
                        }
                        ImGui::TextDisabled("Lock Rotation");
                        for (int i = 0; i < 3; ++i)
                        {
                            ImGui::SameLine();
                            bool locked = (b.lockedAxes >> (i + 3)) & 1;
                            ImGui::PushID(i + 3);
                            if (ImGui::Checkbox(axes[i], &locked))
                            {
                                b.lockedAxes = static_cast<uint8_t>(locked ? b.lockedAxes | (1 << (i + 3)) : b.lockedAxes & ~(1 << (i + 3)));
                                changed = true;
                            }
                            ImGui::PopID();
                        }
                        changed |= DragVec3("Start Velocity", b.startVelocity, 0.05f);
                        changed |= DragVec3("Start Spin", b.startAngularVelocity, 0.05f);
                    }

                    const std::vector<std::string>& layers = world.Settings().layers;
                    const char* layer = b.layer < layers.size() ? layers[b.layer].c_str() : "?";
                    if (ImGui::BeginCombo("Layer", layer))
                    {
                        for (uint32_t i = 0; i < layers.size(); ++i)
                        {
                            if (ImGui::Selectable(layers[i].c_str(), i == b.layer))
                            {
                                b.layer = i;
                                changed = true;
                            }
                        }
                        ImGui::EndCombo();
                    }
                    return changed;
                });
                DrawLive(world, entity);
            }
            ImGui::PopID();
        });

        inspector.SetComponentDrawer("BoxCollider", [&inspector, &undo](Entity& entity) {
            ImGui::PushID("BoxCollider");
            if (Section<BoxCollider>(inspector, undo, entity, "Box Collider"))
            {
                EditComponent<BoxCollider>(entity, undo, "Edit Box Collider", [&](BoxCollider& c) {
                    bool changed = false;
                    if (DragVec3("Size", c.size))
                    {
                        c.size = glm::max(c.size, glm::vec3(1e-3f));
                        changed = true;
                    }
                    changed |= DrawColliderCommon(c);
                    if (FitButton(entity))
                        changed |= FitCollidersToMesh(entity);
                    return changed;
                });
            }
            ImGui::PopID();
        });

        inspector.SetComponentDrawer("SphereCollider", [&inspector, &undo](Entity& entity) {
            ImGui::PushID("SphereCollider");
            if (Section<SphereCollider>(inspector, undo, entity, "Sphere Collider"))
            {
                EditComponent<SphereCollider>(entity, undo, "Edit Sphere Collider", [&](SphereCollider& c) {
                    bool changed = false;
                    if (ImGui::DragFloat("Radius", &c.radius, 0.01f, 1e-3f, 1e4f, "%.3f"))
                    {
                        c.radius = std::max(c.radius, 1e-3f);
                        changed = true;
                    }
                    changed |= DrawColliderCommon(c);
                    if (FitButton(entity))
                        changed |= FitCollidersToMesh(entity);
                    return changed;
                });
            }
            ImGui::PopID();
        });

        auto roundShape = [&inspector, &undo](auto tag, const char* id, const char* title, const char* step) {
            using T = typename decltype(tag)::type;
            inspector.SetComponentDrawer(id, [&inspector, &undo, id, title, step](Entity& entity) {
                ImGui::PushID(id);
                if (Section<T>(inspector, undo, entity, title))
                {
                    EditComponent<T>(entity, undo, step, [&](T& c) {
                        bool changed = false;
                        if (ImGui::DragFloat("Radius", &c.radius, 0.01f, 1e-3f, 1e4f, "%.3f"))
                        {
                            c.radius = std::max(c.radius, 1e-3f);
                            changed = true;
                        }
                        if (ImGui::DragFloat("Height", &c.height, 0.01f, 1e-3f, 1e4f, "%.3f"))
                        {
                            c.height = std::max(c.height, 1e-3f);
                            changed = true;
                        }
                        changed |= DrawColliderCommon(c);
                        if (FitButton(entity))
                            changed |= FitCollidersToMesh(entity);
                        return changed;
                    });
                }
                ImGui::PopID();
            });
        };
        struct CapsuleTag { using type = CapsuleCollider; };
        struct CylinderTag { using type = CylinderCollider; };
        roundShape(CapsuleTag{}, "CapsuleCollider", "Capsule Collider", "Edit Capsule Collider");
        roundShape(CylinderTag{}, "CylinderCollider", "Cylinder Collider", "Edit Cylinder Collider");

        inspector.AddComponentOption({ "Physics", "Rigid Body",
            [](Entity e) { return !e.TryGet<RigidBody>(); },
            [&undo](Entity e) { AddWithUndo(undo, e, "Rigid Body", RigidBody{}); } });
        inspector.AddComponentOption({ "Physics", "Box Collider",
            [](Entity e) { return !e.TryGet<BoxCollider>(); },
            [&undo](Entity e) { AddWithUndo(undo, e, "Box Collider", BoxCollider{}); } });
        inspector.AddComponentOption({ "Physics", "Sphere Collider",
            [](Entity e) { return !e.TryGet<SphereCollider>(); },
            [&undo](Entity e) { AddWithUndo(undo, e, "Sphere Collider", SphereCollider{}); } });
        inspector.AddComponentOption({ "Physics", "Capsule Collider",
            [](Entity e) { return !e.TryGet<CapsuleCollider>(); },
            [&undo](Entity e) { AddWithUndo(undo, e, "Capsule Collider", CapsuleCollider{}); } });
        inspector.AddComponentOption({ "Physics", "Cylinder Collider",
            [](Entity e) { return !e.TryGet<CylinderCollider>(); },
            [&undo](Entity e) { AddWithUndo(undo, e, "Cylinder Collider", CylinderCollider{}); } });
    }

    void DrawColliders(const Scene& scene, Entity selected, bool all, const glm::mat4& viewProj, glm::vec2 imageMin, glm::vec2 imageSize)
    {
        const Painter painter{ ImGui::GetWindowDrawList(), viewProj, imageMin, imageSize };
        auto draw = [&](Entity entity) {
            const WorldXform* world = entity.TryGet<WorldXform>();
            if (!world || !HasCollider(entity))
                return;
            ForEachCollider(entity, glm::make_mat4(world->model), [&](const ColliderGeometry& g) { DrawGeometry(painter, g); });
        };
        if (all)
        {
            scene.ForEachRoot([&](Entity root) {
                draw(root);
                scene.ForEachDescendant(root, draw);
            });
        }
        else if (selected.IsValid())
        {
            draw(selected);
            scene.ForEachDescendant(selected, draw);
        }
    }
}

PhysicsWindow::PhysicsWindow(GUI& gui, PhysicsWorld& world, std::filesystem::path settingsFile)
    : EditorWindow("Physics", gui, false), m_World(world), m_File(std::move(settingsFile))
{
}

void PhysicsWindow::Draw()
{
    if (!m_open)
        return;
    ImGui::SetNextWindowSize(ImVec2(420.0f, 480.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(m_name.c_str(), &m_open))
    {
        PhysicsSettings settings = m_World.Settings();
        bool changed = false;

        ImGui::Checkbox("Show all colliders", &showAllColliders);
        ImGui::TextDisabled("%zu bodies, %zu moving", m_World.BodyCount(), m_World.ActiveBodyCount());

        ImGui::SeparatorText("Simulation");
        changed |= ImGui::DragFloat3("Gravity", glm::value_ptr(settings.gravity), 0.05f);
        int rate = static_cast<int>(std::round(1.0f / settings.fixedTimestep));
        if (ImGui::SliderInt("Steps per second", &rate, 10, 240))
        {
            settings.fixedTimestep = 1.0f / static_cast<float>(rate);
            changed = true;
        }
        changed |= ImGui::SliderInt("Max steps per frame", &settings.maxStepsPerFrame, 1, 16);
        changed |= ImGui::SliderInt("Collision steps", &settings.collisionSteps, 1, 8);

        ImGui::SeparatorText("Layers");
        ImGui::TextDisabled("Tick the pairs that collide");
        const int count = static_cast<int>(settings.layers.size());
        for (int i = 0; i < count; ++i)
        {
            ImGui::PushID(i);
            char name[64] = {};
            std::snprintf(name, sizeof(name), "%s", settings.layers[i].c_str());
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
            if (ImGui::InputText("##name", name, sizeof(name)) && name[0])
            {
                settings.layers[i] = name;
                changed = true;
            }
            for (int j = 0; j <= i; ++j)
            {
                ImGui::SameLine();
                bool collides = settings.Collides(i, j);
                ImGui::PushID(j);
                if (ImGui::Checkbox("##pair", &collides))
                {
                    settings.SetCollides(i, j, collides);
                    changed = true;
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s with %s", settings.layers[i].c_str(), settings.layers[j].c_str());
                ImGui::PopID();
            }
            ImGui::PopID();
        }
        if (count < PhysicsSettings::kMaxLayers && ImGui::SmallButton("+ Layer"))
        {
            settings.AddLayer("Layer " + std::to_string(count));
            changed = true;
        }

        if (changed)
            m_World.SetSettings(settings);

        ImGui::Separator();
        if (ImGui::Button("Save"))
        {
            std::string error;
            m_Status = settings.Save(m_File, error) ? "Saved to " + m_File.filename().string() : error;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload"))
        {
            PhysicsSettings loaded;
            std::string error;
            if (loaded.Load(m_File, error))
            {
                m_World.SetSettings(loaded);
                m_Status = "Reloaded";
            }
            else
                m_Status = error;
        }
        if (!m_Status.empty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", m_Status.c_str());
        }
    }
    ImGui::End();
}
