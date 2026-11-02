---
title: Entity
description: The Entity type in Mariana scripts: transform properties, hierarchy, components, scripts and methods.
---

# Entity

An `Entity` is something in the scene. Scripts get entities from `self.entity`, from [`scene`](scene.md) functions, from events (`other`) and from other entities (`parent`, `GetChildren`). The same entity is always the same value: compare with `==` and use entities as table keys.

```luau
local e = self.entity
e.position = vec3(0, 2, 0)
e:Rotate(vec3(0, 1, 0), 90)
local body = e:Get("RigidBody")
```

## Properties

| Property | Type | Read / write | |
|---|---|---|---|
| `id` | number | read | A unique number for the entity in this scene |
| `name` | string | read / write | Its name |
| `parent` | `Entity?` | read / write | The entity above it, or `nil` for a root. Setting it keeps the local transform as it is (so the entity moves with its new parent); `nil` makes it a root |
| `position` | vector | read / write | Position relative to the parent (the Inspector's Position) |
| `rotation` | [`quat`](quat.md) | read / write | Rotation relative to the parent |
| `scale` | vector | read / write | Scale relative to the parent. Accepts a number for a uniform scale |
| `worldPosition` | vector | read / write | Position in the world |
| `worldRotation` | quat | read / write | Rotation in the world |
| `forward` | vector | read | Its +Z axis in the world (where it looks) |
| `right` | vector | read | Its +X axis in the world |
| `up` | vector | read | Its +Y axis in the world |

Transforms are always current: set `position` and the next read of `worldPosition` (or of a child's) already reflects it.

```luau
-- World and local
local hand = self.entity:FindChild("RightHand")
if hand then
    print(hand.position, hand.worldPosition)
end

-- Moving relative to where it faces
self.entity.worldPosition += self.entity.forward * speed * dt

-- Uniform scale
self.entity.scale = 2
```

> [!NOTE]
> Setting the transform of an entity with a **dynamic** rigid body teleports its body there. To move bodies naturally, push them with forces or set their velocity through [`physics`](physics.md). Entities moved by physics, animation or a character controller are overwritten by them every frame.

## Methods

### `entity:IsValid() -> boolean` {#isvalid}

`true` while the entity exists. Entities you hold on to can be destroyed by other scripts, by gameplay, or by `Destroy`; check before use:

```luau
if self.target and self.target:IsValid() then
    self.entity:LookAt(self.target)
end
```

### `entity:Destroy()` {#destroy}

Destroys the entity and everything below it at the end of the current script phase (after all `Update`s, or after all events and `LateUpdate`s). Scripts on them get `OnDestroy` first. Destroying an entity that's already gone does nothing.

### `entity:Get(component) -> Component?` {#get}

The entity's component by name, or `nil` if it doesn't have one. The result is typed by name (`Get("RigidBody")` is a `RigidBody?`). See [Components](components.md).

```luau
local vehicle = self.entity:Get("Vehicle")
if vehicle then
    vehicle.throttle = 1
end
```

### `entity:Add(component, fields?) -> Component` {#add}

Adds the component with its defaults and the fields you give, or, if the entity already has it, sets those fields. Returns the component. An unknown field name is an error.

```luau
local body = self.entity:Add("RigidBody", { motion = "Dynamic", mass = 5 })
self.entity:Add("ShadowCasterTag")      -- tags take no fields
```

### `entity:Has(component) -> boolean` {#has}

Whether the entity has the component (or tag).

### `entity:Remove(component)` {#remove}

Removes the component if the entity has it.

### `entity:GetScript(name) -> any` {#getscript}

Another script's instance on this entity (its `self`), by the script's file name without `.luau`, or `nil`. Read its fields and call its functions:

```luau
local health = other:GetScript("Health")
if health then
    health:Damage(10)      -- calls Health.Damage(health, 10)
end
```

### `entity:GetChildren() -> { Entity }` {#getchildren}

The entities directly below it, in order.

### `entity:FindChild(name) -> Entity?` {#findchild}

The first entity anywhere below it with that name (depth first), or `nil`.

```luau
local wheel = car:FindChild("Wheel_FL")
```

### `entity:Rotate(axis, degrees)` {#rotate}

Turns it about one of its **own** axes (`vec3(0, 1, 0)` is its own up).

```luau
self.entity:Rotate(vec3(0, 1, 0), 90 * dt)   -- spin at 90 degrees per second
```

To turn about a world axis instead: `entity.rotation = quat.angleAxis(degrees, axis) * entity.rotation` (for a root entity).

### `entity:Translate(offset)` {#translate}

Moves it along its **own** axes: `Translate(vec3(0, 0, 1))` moves it 1 m forward.

### `entity:LookAt(target, up?)` {#lookat}

Turns it so its +Z points at `target`, a point (vector) or an entity (its world position), with +Y toward `up` (default `vec3(0, 1, 0)`). Nothing happens when the target is where the entity is.

```luau
camera:LookAt(player)
turret:LookAt(target.worldPosition + vec3(0, 1, 0))
```

### Meshes and materials {#meshes-and-materials}

| Method | |
|---|---|
| [`entity:SetMesh(mesh, materials?)`](meshes.md#setmesh) | Draws a mesh (one a script made with `mesh.new`) |
| [`entity:GetMesh() -> Mesh?`](meshes.md#getmesh) | The mesh it draws |
| [`entity:GetMaterial(slot?) -> Material?`](meshes.md#getmaterial) | The material in a slot (from 1), the entity's own copy to change |
| [`entity:SetMaterial(material, slot?)`](meshes.md#setmaterial) | Puts a material in a slot |

```luau
local m = self.entity:GetMaterial()
if m then
    m.color = vec3(1, 0, 0)    -- this entity turns red, nothing else does
end
```

## Printing

`print(entity)` shows `Entity(42 'Crate')`, or `Entity(destroyed)` for one that's gone.

## Entities and scripts

- `self.entity` is the entity a script instance runs on.
- An entity's scripts run in the order they're listed in its Scripts component; entities run in creation order.
- When an entity is destroyed, every script on it and below it gets `OnDestroy`.
