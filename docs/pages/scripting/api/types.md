---
title: Types
description: The types of the Mariana script API for Luau's type checker: Script, Contact, RaycastHit, QueryOptions, BodyInfo, VehicleInfo, WheelInfo, CharacterInfo, ComponentName and the component types.
---

# Types

Luau checks your scripts' types as you write them (see [Type checking](../script-editor.md#type-checking)). These are the types the API uses. Each is a plain table you can read fields from; you only write their names in **type annotations** (`local hit: RaycastHit? = ...`), which are optional.

## Script {#script}

```luau
export type Script = { entity: Entity, properties: { [string]: any }?, [string]: any }
```

What a script's module table and its `self` are. Write every script as:

```luau
local Mover = {} :: Script

function Mover.Update(self: Script, dt: number)
    self.entity:Translate(vec3(0, 0, dt))
end

return Mover
```

- `self.entity` is typed: the checker knows every [Entity](entity.md) member.
- Anything else on `self` (your properties, your own state) is `any`, so it's never an error to read or write it, and the checker doesn't check what you do with it. Give such values a type where it matters: `local speed: number = self.speed`.

> [!IMPORTANT]
> Write `function Mover.Update(self: Script, dt: number)`, not `function Mover:Update(dt)`. Both run the same, but Luau's checker can't give `self` a type in the `:` form, so the body of such a function isn't checked at all.

## Contact {#contact}

```luau
export type Contact = { point: vector, normal: vector, speed: number }
```

What `OnContactBegin(self, other, contact)` gets:

| Field | |
|---|---|
| `point` | Where they touch, in the world |
| `normal` | The direction from this entity toward `other` |
| `speed` | How fast they came together along the normal, m/s (how hard the hit was) |

```luau
function Crate.OnContactBegin(self: Script, other: Entity, contact: Contact)
    if contact.speed > 8 then
        self.entity:Destroy()      -- smashed
    end
end
```

## RaycastHit {#raycasthit}

```luau
export type RaycastHit = { entity: Entity, point: vector, normal: vector, distance: number }
```

What [`physics.Raycast`](physics.md#raycast), `RaycastAll` and `SphereCast` return: the body's entity, the point hit, the surface normal there, and the distance from the start.

## QueryOptions {#queryoptions}

```luau
export type QueryOptions = { ignore: Entity?, triggers: boolean?, layers: number? }
```

The optional last argument of every physics query. See [Query options](physics.md#query-options).

## BodyInfo {#bodyinfo}

```luau
export type BodyInfo = {
    motion: "Static" | "Kinematic" | "Dynamic",
    sleeping: boolean,
    sensor: boolean,
    mass: number,
    linearVelocity: vector,
    angularVelocity: vector,
}
```

From [`physics.GetBodyInfo`](physics.md#getbodyinfo). `sensor` is true for a trigger's body; `angularVelocity` is in radians per second.

## VehicleInfo and WheelInfo {#vehicleinfo}

```luau
export type VehicleInfo = { speed: number, rpm: number, gear: number, wheels: { WheelInfo } }
export type WheelInfo = {
    contact: boolean,
    suspension: number,
    steerAngle: number,
    rpm: number,
    longitudinalSlip: number,
    lateralSlip: number,
}
```

From [`physics.GetVehicleInfo`](physics.md#getvehicleinfo). Wheels are in the order their entities were made.

## CharacterInfo {#characterinfo}

```luau
export type CharacterInfo = {
    grounded: boolean,
    onSteepGround: boolean,
    velocity: vector,
    groundNormal: vector,
    ground: Entity?,
}
```

From [`physics.GetCharacterInfo`](physics.md#getcharacterinfo).

## Engine types

| Type | |
|---|---|
| `Entity` | See [Entity](entity.md) |
| `vector` | Luau's built-in 3D vector, made with [`vec3`](vec3.md) |
| `quat` | See [quat](quat.md) |
| `Time` | The type of the [`time`](time.md) global |
| `Component` | The base of every component type |
| `AnimatorMethods` | The [Animator's methods](animator.md), which the `Animator` type has besides its fields |

## Component types {#component-types}

Every component the engine (and your project's libraries) registers has a type of its own with its fields, named like the component: `RigidBody`, `BoxCollider`, `Vehicle`, `CharacterController`, `Animator`, …. Enum fields are unions of their names, so the checker catches a misspelt one:

```luau
local body = self.entity:Get("RigidBody")      -- RigidBody?
if body then
    body.motion = "Dynamic"                    -- "Static" | "Kinematic" | "Dynamic"
    body.mass = 12
end
```

`ComponentName` is the union of every component's name, so `entity:Get("RigidBdy")` is an error too. The fields of each are in the [Component Reference](component-reference.md).

## Narrowing `nil`

Functions that look something up return an optional type (`Entity?`, `RigidBody?`, `RaycastHit?`). In strict mode the checker makes you deal with `nil` before using it:

```luau
--!strict
local target = scene.Find("Target")     -- Entity?
if target then
    self.entity:LookAt(target)          -- Entity here
end

local hit = physics.Raycast(origin, direction)
if not hit then
    return
end
print(hit.distance)                     -- RaycastHit from here on
```

## Where the definitions are

The editor writes them all, including your project's components, to `<project>/.mariana/types/Mariana.d.luau`, which [VS Code's Luau extension](../script-editor.md#vs-code) reads. Open it to see the exact type of anything.
