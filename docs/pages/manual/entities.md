---
title: Entities and Components
description: Entities, components, transforms, parents and children, and the components the engine provides.
---

# Entities and Components

Everything in a scene is an **entity**: a car, a wall, a camera, an empty holder for a script. An entity is just a name, a place in the hierarchy and a set of **components**. Components give it its abilities: a Mesh Renderer draws it, a Rigid Body makes it fall, a Scripts component runs code on it.

## Creating entities

| From | What you get |
|---|---|
| **Create > Empty Entity** | An entity with only a transform, at the world's origin. A holder for scripts, or a parent to group others. |
| **Create > Cube / Sphere / Plane / Cylinder / Capsule** | A primitive with its own material, 5 m in front of the editor camera |
| **Create > Camera** | A camera where the editor camera is, looking the same way |
| **Create > Physics** | Physics-ready objects: dynamic and static shapes, a ground, a box stack, a character, joint setups, water, a ragdoll, soft bodies |
| **Create > Vehicle** | A drivable car, motorcycle or tank (with a chase camera), or a vehicle playground |
| Dragging a model from **Assets** | The model's tree of entities, with meshes, materials and, if it's animated, an Animator |
| A script | [`scene.Create`](../scripting/api/scene.md), `scene.Spawn` and `scene.SpawnPrimitive` |

**Delete** removes the selected entity and everything below it.

## The transform

Every entity has a transform (the **LocalTRS** component): a **position**, a **rotation** and a **scale**, relative to its parent. In the Inspector, rotation is shown as angles in degrees about X, Y and Z.

The engine's coordinate system:

| Axis | Direction |
|---|---|
| **+X** | Right |
| **+Y** | Up |
| **+Z** | Forward: cameras look along +Z, vehicles drive along +Z, models face +Z (as glTF models do) |

Units are metres, kilograms and seconds. Angles in the Inspector and in scripts are degrees.

## Parents and children

Entities form a tree. A child's transform is relative to its parent: move the parent and its children come with it. A model is a tree (the root, then its nodes), a vehicle's wheels are children of its body, the snake's eyes in the [tutorial](../getting-started/first-game.md) are children of its head.

From a script, set `entity.parent` (the local transform is kept as it is) and read `entity:GetChildren()` and `entity:FindChild(name)`.

## Components you can add

| Component | Section in the Inspector | What it does | Read more |
|---|---|---|---|
| LocalTRS | Transform | Position, rotation and scale | This page |
| MeshComponent | Mesh Renderer | Draws a mesh with its materials; *Cast Shadows* | [Materials](materials.md) |
| CameraComponent | Camera | Makes the entity a camera (field of view, near and far planes) | [Rendering](rendering.md#cameras) |
| Animator | Animator | Plays a model's clips or an animation graph | [Animation](animation.md) |
| RigidBody | Rigid Body | Makes the entity a physics body: static, kinematic or dynamic | [Physics](physics.md) |
| BoxCollider, SphereCollider, CapsuleCollider, CylinderCollider, MeshCollider | … Collider | The shape it collides with; triggers too | [Physics](physics.md#colliders) |
| Joint | Joint | Ties two bodies together: hinges, sliders, ropes, ball joints | [Physics](physics.md#joints) |
| Vehicle, VehicleWheel | Vehicle, Vehicle Wheel | A car, motorcycle or tank and its wheels | [Gameplay Physics](gameplay-physics.md#vehicles) |
| CharacterController | Character Controller | A walking, jumping, stair-climbing character | [Gameplay Physics](gameplay-physics.md#characters) |
| WaterVolume | Water Volume | Water things float in | [Gameplay Physics](gameplay-physics.md#water) |
| Ragdoll | Ragdoll | Turns an animated character into a ragdoll | [Gameplay Physics](gameplay-physics.md#ragdolls) |
| SoftBody | Soft Body | Cloth, jelly, balloons | [Gameplay Physics](gameplay-physics.md#soft-bodies) |
| Scripts | Scripts | Runs Luau scripts on the entity | [Scripting](../scripting/overview.md) |

Every component and its fields can also be read and changed from scripts by name: see the [Component Reference](../scripting/api/component-reference.md).

## Names

Names don't have to be unique, but scripts find entities by name (`scene.Find("Player")`), and with several of the same name they get the one created first. Give the entities your scripts look for clear, unique names.
