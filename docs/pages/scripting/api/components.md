---
title: Components
description: Reading, changing, adding and removing any component from Mariana scripts, by name and field by field.
---

# Components

Every component the engine saves in scene files can be reached from scripts by its name, field by field, with the same names the scene files use. That covers physics (rigid bodies, colliders, joints, vehicles, characters, water, ragdolls, soft bodies), cameras, animators and transforms.

```luau
local body = self.entity:Get("RigidBody")      -- RigidBody?
if body then
    print(body.mass, body.motion)              -- 1   Dynamic
    body.mass = 20
    body.motion = "Kinematic"
end
```

## Getting components

| Call | |
|---|---|
| `entity:Get(name)` | The component, or `nil` |
| `entity:Has(name)` | Whether it's there |
| `entity:Add(name, fields?)` | Adds it (or sets fields on the existing one) and returns it |
| `entity:Remove(name)` | Removes it |
| `scene.Each(name, fn)` | Calls `fn(entity)` for every entity with it, in creation order |

Names are the components' names in the [Component Reference](component-reference.md): `"RigidBody"`, `"BoxCollider"`, `"CharacterController"`, `"Animator"`, … A name that doesn't exist is an error.

## Fields

A component value reads and writes the entity's live component:

- **Reading** a field returns its current value.
- **Writing** a field changes the component at once (physics picks up the change on its next step; a changed collider or body setting rebuilds the body).
- A field that doesn't exist is an **error**: `RigidBody has no field 'masss'`.
- Each read goes to the entity, so keep the component, not its values, if they change:

```luau
local body = self.entity:Get("RigidBody")   -- keep this...
print(body.mass)                           -- ...and read fresh values from it
```

### Field types

| In the component | In Luau | Example |
|---|---|---|
| Number | number | `body.mass = 20` |
| Whole number | number (rounded down; some can't be negative) | `soft.iterations = 8` |
| Boolean | boolean | `body.continuous = true` |
| Text | string | `joint.connected = "Door Frame"` |
| Two or three numbers | vector | `box.size = vec3(1, 2, 1)` |
| Rotation | `quat` | `box.rotation = quat.euler(0, 45, 0)` |
| Choice (enum) | string, by name | `body.motion = "Dynamic"` |
| List of numbers | table of numbers | `vehicle.gearRatios = { 3.1, 2.0, 1.4, 1.0 }` |

Enum fields read as their names and accept names (or their index as a number). The names of each are in the [Component Reference](component-reference.md); a wrong name is an error.

## Adding with fields

```luau
local crate = scene.SpawnPrimitive("cube", vec3(0, 3, 0))
crate:Add("BoxCollider", { friction = 0.8 })
crate:Add("RigidBody", {
    motion = "Dynamic",
    mass = 25,
    startVelocity = vec3(0, 0, 5),
})
```

Fields you leave out keep their defaults. Giving a field that doesn't exist is an error (`some fields given for RigidBody aren't among its fields`), so typos never pass silently. Calling `Add` on an entity that already has the component sets the fields you give and leaves the others.

## Fields only code sets

A few fields aren't saved in scene files: they're inputs your scripts set every frame.

| Component | Fields |
|---|---|
| `Vehicle` | `throttle`, `steering`, `brake`, `handBrake` |
| `CharacterController` | `move`, `run`, `jump` |

## What scripts can't reach yet

Fields that refer to **assets** (a Mesh Renderer's mesh and materials, an Animator's skeleton, clips and graph, a Mesh Collider's mesh, a Scripts component's scripts) and **lists of objects** (an Animator's clips and events) aren't available from scripts. Set them in the editor. Choose a primitive's colour when you spawn it (`scene.SpawnPrimitive(shape, position, color)`).

## The Animator

The `Animator` component also has **methods** for driving animation (`SetFloat`, `SetTrigger`, `CrossFade`, …). See [Animator](animator.md).

## Typed access

`entity:Get("RigidBody")` is typed as `RigidBody?` (and `Add` as `RigidBody`), so the [type checker](../script-editor.md#type-checking) knows every field, its type and each enum's names:

```luau
local body = self.entity:Get("RigidBody")
if body then
    body.motion = "Floating"   -- underlined: not one of "Static" | "Kinematic" | "Dynamic"
end
```
