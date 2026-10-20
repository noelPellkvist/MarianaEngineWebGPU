#include <Editor/Physics/PhysicsEditor.hpp>

#include <Editor/UndoStack.hpp>
#include <Editor/Windows/InspectorWindow.hpp>
#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Graphics/Mesh.hpp>
#include <Mariana/Physics/ColliderGeometry.hpp>
#include <Mariana/Physics/Controllers.hpp>
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
        if (MeshBounds(entity).IsEmpty())
            return false;
        return ImGui::SmallButton("Fit to mesh");
    }

    // Every entity at or below `entity` that has a mesh but no mesh collider.
    std::vector<Entity> WithoutMeshColliders(Entity entity)
    {
        std::vector<Entity> found;
        std::function<void(Entity)> visit = [&](Entity e) {
            if (e.TryGet<MeshComponent>() && !e.TryGet<MeshCollider>())
                found.push_back(e);
            e.ForEachChild(visit);
        };
        visit(entity);
        return found;
    }

    std::string MeshName(AssetManager2& assets, const AssetHandle<IMesh>& mesh)
    {
        const AssetMetadata* meta = mesh.IsValid() ? assets.GetMetadata(mesh.GetID()) : nullptr;
        return meta ? meta->name : std::string("(unnamed)");
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

    // A vehicle wheel: its tyre's circle and edges where it hangs, and the suspension travel up from it.
    void DrawWheel(const Painter& painter, const glm::mat4& world, const VehicleWheel& w)
    {
        const ImU32 color = IM_COL32(255, 200, 80, 230);
        const glm::vec3 center(world[3]);
        // The axle is the wheel's X; its up the Y.
        const glm::vec3 axle = glm::normalize(glm::vec3(world[0])), up = glm::normalize(glm::vec3(world[1])), forward = glm::normalize(glm::vec3(world[2]));
        const float twoPi = glm::two_pi<float>();
        for (float side : { -0.5f, 0.5f })
            painter.Arc(center + axle * (side * w.width), up, forward, w.radius, 0.0f, twoPi, color, 32);
        for (const glm::vec3& edge : { up, -up, forward, -forward })
            painter.Line(center + edge * w.radius - axle * (0.5f * w.width), center + edge * w.radius + axle * (0.5f * w.width), color);
        painter.Line(center, center + up * (w.suspensionMax - w.suspensionMin), IM_COL32(255, 120, 60, 230));
    }

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
        case ColliderShape::Mesh:
        case ColliderShape::ConvexHull:
        {
            // The mesh's bounds; the shape itself is drawn by the debug view while playing.
            if (g.meshBounds.IsEmpty())
                break;
            glm::vec3 corners[8];
            for (int i = 0; i < 8; ++i)
            {
                const glm::vec3 local((i & 1) ? g.meshBounds.max.x : g.meshBounds.min.x, (i & 2) ? g.meshBounds.max.y : g.meshBounds.min.y, (i & 4) ? g.meshBounds.max.z : g.meshBounds.min.z);
                corners[i] = g.position + g.rotation * (g.scale * local);
            }
            const ImU32 meshColor = g.collider->isTrigger ? color : (g.shape == ColliderShape::Mesh ? IM_COL32(110, 235, 130, 140) : IM_COL32(235, 200, 110, 200));
            for (int i = 0; i < 8; ++i)
                for (int bit : { 1, 2, 4 })
                    if (!(i & bit))
                        painter.Line(corners[i], corners[i | bit], meshColor);
            break;
        }
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
        return FitColliders(entity);
    }

    void RegisterInspector(InspectorWindow& inspector, UndoStack& undo, const PhysicsWorld& world, AssetManager2& assets)
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
                        changed |= DragVec3("Center of Mass", b.centerOfMass, 0.01f);
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Moves the center of mass (body space); lower makes vehicles steadier");
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

        inspector.SetComponentDrawer("MeshCollider", [&inspector, &undo, &assets](Entity& entity) {
            ImGui::PushID("MeshCollider");
            if (Section<MeshCollider>(inspector, undo, entity, "Mesh Collider"))
            {
                EditComponent<MeshCollider>(entity, undo, "Edit Mesh Collider", [&](MeshCollider& c) {
                    bool changed = false;
                    const MeshComponent* renderer = entity.TryGet<MeshComponent>();
                    const std::string current = c.mesh.IsValid() ? MeshName(assets, c.mesh)
                                              : renderer && renderer->mesh.IsValid() ? "Mesh Renderer's (" + MeshName(assets, renderer->mesh) + ")"
                                              : std::string("None");
                    if (ImGui::BeginCombo("Mesh", current.c_str()))
                    {
                        if (ImGui::Selectable("Mesh Renderer's", !c.mesh.IsValid()))
                        {
                            c.mesh = AssetHandle<IMesh>();
                            changed = true;
                        }
                        assets.ForEach<IMesh>([&](AssetID id, IMesh& mesh) {
                            if (!mesh.collision)
                                return;
                            const AssetMetadata* meta = assets.GetMetadata(id);
                            ImGui::PushID(&mesh);
                            if (ImGui::Selectable(meta ? meta->name.c_str() : "(unnamed)", c.mesh.IsValid() && c.mesh.GetID() == id))
                            {
                                c.mesh = assets.LoadAsset<IMesh>(id);
                                changed = true;
                            }
                            ImGui::PopID();
                        });
                        ImGui::EndCombo();
                    }
                    changed |= ImGui::Checkbox("Convex", &c.convex);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Collide as the convex hull of the mesh (the only way it can be dynamic)");
                    if (const IMesh* mesh = ColliderMesh(entity, c))
                    {
                        if (mesh->collision)
                            ImGui::TextDisabled("%zu triangles, %zu vertices", mesh->collision->triangles.size() / 3, mesh->collision->positions.size());
                        else
                            ImGui::TextDisabled("This mesh has no triangles to collide with");
                    }
                    const RigidBody* body = entity.TryGet<RigidBody>();
                    if (!c.convex && body && body->motion == MotionType::Dynamic)
                        ImGui::TextDisabled("Used as a convex hull on a dynamic body");
                    changed |= DrawColliderCommon(c);
                    return changed;
                });
            }
            ImGui::PopID();
        });

        inspector.SetComponentDrawer("Vehicle", [&inspector, &undo, &world](Entity& entity) {
            ImGui::PushID("Vehicle");
            if (Section<Vehicle>(inspector, undo, entity, "Vehicle"))
            {
                EditComponent<Vehicle>(entity, undo, "Edit Vehicle", [&](Vehicle& v) {
                    bool changed = false;
                    const char* types[] = { "Wheeled", "Motorcycle", "Tracked" };
                    int type = static_cast<int>(v.type);
                    if (ImGui::Combo("Type", &type, types, 3)) { v.type = static_cast<VehicleType>(type); changed = true; }
                    const char* collisions[] = { "Ray", "Sphere", "Cylinder" };
                    int collision = static_cast<int>(v.collision);
                    if (ImGui::Combo("Wheel Collision", &collision, collisions, 3)) { v.collision = static_cast<WheelCollision>(collision); changed = true; }
                    if (v.type == VehicleType::Wheeled)
                    {
                        const char* drives[] = { "Front", "Rear", "All" };
                        int drive = static_cast<int>(v.drive);
                        if (ImGui::Combo("Drive", &drive, drives, 3)) { v.drive = static_cast<VehicleDrive>(drive); changed = true; }
                    }
                    if (ImGui::TreeNode("Engine and Gearbox"))
                    {
                        changed |= ImGui::DragFloat("Torque", &v.engineTorque, 5.0f, 0.0f, 20000.0f, "%.0f Nm");
                        changed |= ImGui::DragFloat("Min RPM", &v.minRPM, 10.0f, 1.0f, 30000.0f, "%.0f");
                        changed |= ImGui::DragFloat("Max RPM", &v.maxRPM, 10.0f, 1.0f, 30000.0f, "%.0f");
                        changed |= ImGui::Checkbox("Automatic", &v.automatic);
                        if (v.automatic)
                        {
                            changed |= ImGui::DragFloat("Shift Up RPM", &v.shiftUpRPM, 10.0f, 0.0f, 30000.0f, "%.0f");
                            changed |= ImGui::DragFloat("Shift Down RPM", &v.shiftDownRPM, 10.0f, 0.0f, 30000.0f, "%.0f");
                        }
                        changed |= ImGui::DragFloat("Clutch", &v.clutchStrength, 0.1f, 0.0f, 100.0f, "%.1f");
                        for (size_t i = 0; i < v.gearRatios.size(); ++i)
                        {
                            ImGui::PushID(static_cast<int>(i));
                            changed |= ImGui::DragFloat(("Gear " + std::to_string(i + 1)).c_str(), &v.gearRatios[i], 0.01f, 0.05f, 20.0f, "%.2f");
                            ImGui::PopID();
                        }
                        if (ImGui::SmallButton("+ Gear")) { v.gearRatios.push_back(v.gearRatios.empty() ? 1.0f : v.gearRatios.back() * 0.8f); changed = true; }
                        ImGui::SameLine();
                        if (v.gearRatios.size() > 1 && ImGui::SmallButton("- Gear")) { v.gearRatios.pop_back(); changed = true; }
                        changed |= ImGui::DragFloat("Reverse", &v.reverseRatio, 0.01f, -20.0f, -0.05f, "%.2f");
                        changed |= ImGui::DragFloat("Final Drive", &v.differentialRatio, 0.01f, 0.1f, 50.0f, "%.2f");
                        ImGui::TreePop();
                    }
                    if (v.type != VehicleType::Tracked)
                        changed |= ImGui::DragFloat("Limited Slip", &v.limitedSlipRatio, 0.01f, 0.0f, 10.0f, v.limitedSlipRatio > 1.0f ? "%.2f" : "open");
                    if (v.type == VehicleType::Wheeled)
                        changed |= ImGui::DragFloat("Anti-roll", &v.antiRollStiffness, 10.0f, 0.0f, 100000.0f, "%.0f N/m");
                    if (v.type == VehicleType::Motorcycle)
                        changed |= ImGui::SliderFloat("Max Lean", &v.maxLeanAngle, 0.0f, 80.0f, "%.0f deg");
                    changed |= ImGui::SliderFloat("Max Tilt", &v.maxPitchRollAngle, 0.0f, 180.0f, "%.0f deg");
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("How far it may pitch or roll before it is held up (180 = never)");

                    ImGui::SeparatorText("Driver");
                    changed |= ImGui::SliderFloat("Throttle", &v.throttle, -1.0f, 1.0f, "%.2f");
                    changed |= ImGui::SliderFloat("Steering", &v.steering, -1.0f, 1.0f, "%.2f");
                    changed |= ImGui::SliderFloat("Brake", &v.brake, 0.0f, 1.0f, "%.2f");
                    changed |= ImGui::SliderFloat("Hand Brake", &v.handBrake, 0.0f, 1.0f, "%.2f");
                    if (!v.automatic)
                        changed |= ImGui::SliderInt("Gear", &v.gear, -1, static_cast<int>(v.gearRatios.size()));
                    return changed;
                });
                VehicleInfo info;
                if (world.GetVehicleInfo(entity, info))
                {
                    ImGui::TextDisabled("%.0f km/h, %.0f rpm, gear %d", info.speed * 3.6f, info.rpm, info.gear);
                    for (size_t i = 0; i < info.wheels.size(); ++i)
                    {
                        const VehicleInfo::Wheel& w = info.wheels[i];
                        ImGui::TextDisabled("  wheel %zu: %s, suspension %.0f%%, steer %.0f, slip %.2f / %.0f deg", i + 1, w.contact ? "on ground" : "in air",
                                            w.suspension * 100.0f, w.steerAngle, w.longitudinalSlip, w.lateralSlip);
                    }
                }
                else
                {
                    ImGui::TextDisabled("Drives while playing (needs a dynamic Rigid Body and Vehicle Wheels below)");
                }
            }
            ImGui::PopID();
        });

        inspector.SetComponentDrawer("VehicleWheel", [&inspector, &undo](Entity& entity) {
            ImGui::PushID("VehicleWheel");
            if (Section<VehicleWheel>(inspector, undo, entity, "Vehicle Wheel"))
            {
                EditComponent<VehicleWheel>(entity, undo, "Edit Vehicle Wheel", [&](VehicleWheel& w) {
                    bool changed = false;
                    changed |= ImGui::DragFloat("Radius", &w.radius, 0.005f, 0.01f, 5.0f, "%.3f m");
                    changed |= ImGui::DragFloat("Width", &w.width, 0.005f, 0.01f, 5.0f, "%.3f m");
                    changed |= ImGui::DragFloat("Suspension Min", &w.suspensionMin, 0.005f, 0.0f, 5.0f, "%.3f m");
                    changed |= ImGui::DragFloat("Suspension Max", &w.suspensionMax, 0.005f, 0.0f, 5.0f, "%.3f m");
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Travel from the attachment point; the wheel's place is where it hangs at the max");
                    changed |= ImGui::DragFloat("Spring", &w.suspensionFrequency, 0.01f, 0.01f, 20.0f, "%.2f Hz");
                    changed |= ImGui::DragFloat("Damping", &w.suspensionDamping, 0.01f, 0.0f, 5.0f, "%.2f");
                    changed |= ImGui::SliderFloat("Max Steer", &w.maxSteerAngle, 0.0f, 89.0f, "%.0f deg");
                    changed |= ImGui::SliderFloat("Caster", &w.casterAngle, -60.0f, 60.0f, "%.0f deg");
                    changed |= ImGui::DragFloat("Brake", &w.maxBrakeTorque, 10.0f, 0.0f, 100000.0f, "%.0f Nm");
                    changed |= ImGui::DragFloat("Hand Brake", &w.maxHandBrakeTorque, 10.0f, 0.0f, 100000.0f, "%.0f Nm");
                    changed |= ImGui::DragFloat("Grip", &w.grip, 0.01f, 0.0f, 10.0f, "%.2f");
                    return changed;
                });
            }
            ImGui::PopID();
        });

        inspector.SetComponentDrawer("PlayerControl", [&inspector, &undo](Entity& entity) {
            ImGui::PushID("PlayerControl");
            if (Section<PlayerControl>(inspector, undo, entity, "Player Control"))
            {
                EditComponent<PlayerControl>(entity, undo, "Edit Player Control", [&](PlayerControl& c) {
                    bool changed = ImGui::Checkbox("Enabled", &c.enabled);
                    changed |= ImGui::DragFloat("Steer Speed", &c.steerSpeed, 0.05f, 0.1f, 20.0f, "%.2f");
                    return changed;
                });
                ImGui::TextDisabled("W/S or Up/Down drive, A/D or Left/Right steer, Space hand brake");
            }
            ImGui::PopID();
        });

        inspector.SetComponentDrawer("FollowCamera", [&inspector, &undo](Entity& entity) {
            ImGui::PushID("FollowCamera");
            if (Section<FollowCamera>(inspector, undo, entity, "Follow Camera"))
            {
                EditComponent<FollowCamera>(entity, undo, "Edit Follow Camera", [&](FollowCamera& c) {
                    char target[128] = {};
                    std::snprintf(target, sizeof(target), "%s", c.target.c_str());
                    bool changed = false;
                    if (ImGui::InputTextWithHint("Target", "the player-controlled one", target, sizeof(target)))
                    {
                        c.target = target;
                        changed = true;
                    }
                    changed |= ImGui::DragFloat("Distance", &c.distance, 0.05f, 0.0f, 500.0f, "%.2f m");
                    changed |= ImGui::DragFloat("Height", &c.height, 0.05f, -100.0f, 100.0f, "%.2f m");
                    changed |= ImGui::DragFloat("Look Height", &c.lookHeight, 0.05f, -100.0f, 100.0f, "%.2f m");
                    changed |= ImGui::DragFloat("Stiffness", &c.stiffness, 0.05f, 0.0f, 100.0f, "%.2f");
                    return changed;
                });
            }
            ImGui::PopID();
        });

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
        inspector.AddComponentOption({ "Physics", "Mesh Collider",
            [](Entity e) { return !e.TryGet<MeshCollider>(); },
            [&undo](Entity e) { AddWithUndo(undo, e, "Mesh Collider", MeshCollider{}); } });
        inspector.AddComponentOption({ "Vehicles", "Vehicle",
            [](Entity e) { return !e.TryGet<Vehicle>(); },
            [&undo](Entity e) { AddWithUndo(undo, e, "Vehicle", Vehicle{}); } });
        inspector.AddComponentOption({ "Vehicles", "Vehicle Wheel",
            [](Entity e) { return !e.TryGet<VehicleWheel>(); },
            [&undo](Entity e) { AddWithUndo(undo, e, "Vehicle Wheel", VehicleWheel{}); } });
        inspector.AddComponentOption({ "Vehicles", "Player Control",
            [](Entity e) { return !e.TryGet<PlayerControl>(); },
            [&undo](Entity e) { AddWithUndo(undo, e, "Player Control", PlayerControl{}); } });
        inspector.AddComponentOption({ "Vehicles", "Follow Camera",
            [](Entity e) { return !e.TryGet<FollowCamera>(); },
            [&undo](Entity e) { AddWithUndo(undo, e, "Follow Camera", FollowCamera{}); } });
        // A model made solid at once (a level, say): a mesh collider on everything with a mesh.
        inspector.AddComponentOption({ "Physics", "Mesh Colliders on Children",
            [](Entity e) { return !WithoutMeshColliders(e).empty(); },
            [&undo](Entity e) {
                const std::vector<Entity> targets = WithoutMeshColliders(e);
                for (Entity target : targets)
                    target.Add<MeshCollider>(MeshCollider{});
                undo.Push("Add Mesh Colliders",
                    [targets]() mutable { for (Entity t : targets) if (t.IsValid()) t.Remove<MeshCollider>(); },
                    [targets]() mutable { for (Entity t : targets) if (t.IsValid() && !t.TryGet<MeshCollider>()) t.Add<MeshCollider>(MeshCollider{}); });
            } });
    }

    void DrawColliders(const Scene& scene, Entity selected, bool all, const glm::mat4& viewProj, glm::vec2 imageMin, glm::vec2 imageSize)
    {
        const Painter painter{ ImGui::GetWindowDrawList(), viewProj, imageMin, imageSize };
        auto draw = [&](Entity entity) {
            const WorldXform* world = entity.TryGet<WorldXform>();
            if (!world)
                return;
            if (const VehicleWheel* wheel = entity.TryGet<VehicleWheel>())
            {
                DrawWheel(painter, glm::make_mat4(world->model), *wheel);
                return;
            }
            if (HasCollider(entity))
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

    void DrawDebugLines(const std::vector<DebugLine>& lines, const glm::mat4& viewProj, glm::vec2 imageMin, glm::vec2 imageSize)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        constexpr float kNear = 1e-3f;
        for (const DebugLine& line : lines)
        {
            glm::vec4 a = viewProj * glm::vec4(line.from, 1.0f);
            glm::vec4 b = viewProj * glm::vec4(line.to, 1.0f);
            if (a.w < kNear && b.w < kNear)
                continue;
            if (a.w < kNear)
                a = glm::mix(a, b, (kNear - a.w) / (b.w - a.w));
            else if (b.w < kNear)
                b = glm::mix(b, a, (kNear - b.w) / (a.w - b.w));
            const glm::vec2 na = glm::vec2(a) / a.w, nb = glm::vec2(b) / b.w;
            // Lines wholly off one side of the image aren't drawn.
            if ((na.x < -1.0f && nb.x < -1.0f) || (na.x > 1.0f && nb.x > 1.0f) || (na.y < -1.0f && nb.y < -1.0f) || (na.y > 1.0f && nb.y > 1.0f))
                continue;
            const ImVec2 sa(imageMin.x + (na.x * 0.5f + 0.5f) * imageSize.x, imageMin.y + (0.5f - na.y * 0.5f) * imageSize.y);
            const ImVec2 sb(imageMin.x + (nb.x * 0.5f + 0.5f) * imageSize.x, imageMin.y + (0.5f - nb.y * 0.5f) * imageSize.y);
            drawList->AddLine(sa, sb, line.color | IM_COL32(0, 0, 0, 255), 1.0f);
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

        ImGui::SeparatorText("Debug view (while playing)");
        ImGui::Checkbox("Draw the simulation", &debugDraw);
        if (debugDraw)
        {
            PhysicsDebugDraw& d = debugDrawSettings;
            ImGui::Checkbox("Shapes", &d.shapes);
            ImGui::SameLine();
            ImGui::Checkbox("Awake / asleep colours", &d.sleepColors);
            ImGui::Checkbox("Contacts", &d.contacts);
            ImGui::SameLine();
            ImGui::Checkbox("Velocities", &d.velocities);
            ImGui::Checkbox("Centers of mass", &d.centerOfMass);
            ImGui::SameLine();
            ImGui::Checkbox("Bounding boxes", &d.boundingBoxes);
            ImGui::TextDisabled("%zu lines%s", debugLines, debugLines >= d.maxLines ? " (the most drawn)" : "");
        }

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
