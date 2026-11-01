---
title: scene
description: The scene library in Mariana scripts: finding entities by name or component, creating empty entities, spawning models and primitives, destroying entities.
---

# scene

The `scene` global finds, makes and destroys entities in the running scene.

```luau
local player = scene.Find("Player")
local crate = scene.SpawnPrimitive("cube", vec3(0, 4, 0), vec3(0.8, 0.5, 0.2))
local dancer = scene.Spawn("/Models/Rumba.glb", vec3(3, 0, 0), quat.euler(0, 180, 0))
```

## Functions

### `scene.Find(name) -> Entity?` {#find}

The entity with that name, anywhere in the scene, or `nil`. When several have the name, the one made first. Names are matched exactly (case matters).

```luau
local door = scene.Find("Front Door")
if door then
    door:Get("Joint")
end
```

> [!TIP]
> `Find` looks through the scene's names, so it costs more than holding on to an entity. Call it in `Start` and keep the result in `self`. To find something below one entity, use [`entity:FindChild`](entity.md#findchild).

### `scene.Create(name?, parent?) -> Entity` {#create}

A new empty entity (a transform and a name, nothing else) at the origin of its parent, or of the world. `name` defaults to `"Entity"`.

```luau
local group = scene.Create("Debris")
local marker = scene.Create("Marker", group)
marker.position = vec3(0, 2, 0)
```

Use empty entities to group things (destroying the group destroys everything below it), as pivots (a child turns around its parent) and as markers.

### `scene.Spawn(path, position?, rotation?) -> Entity` {#spawn}

An instance of a model (`.glb` / `.gltf`) or prefab file, by its resource path (from the project folder, starting with `/`). Returns the model's root, placed at `position` with `rotation`. An animated model comes with its Animator playing its first clip.

```luau
local tree = scene.Spawn("/Models/Tree.glb", vec3(math.random(-20, 20), 0, math.random(-20, 20)))
tree.scale = 1 + math.random() * 0.5
```

The first spawn of a file loads it (which takes a moment for a big model); later spawns of the same file reuse what's loaded and are quick. A path that doesn't exist is an error (`no file /Models/Tre.glb`).

> [!IMPORTANT]
> When you [build a game](../../building/build-profiles.md), only the files your scenes use are packaged. A model that only a script spawns isn't in any scene, so add its folder to the build (or turn on **Include whole project** in the profile), or place one copy of it somewhere in a scene.

### `scene.SpawnPrimitive(shape, position?, color?, rotation?) -> Entity` {#spawnprimitive}

A built-in shape with a material of its own (so changing its colour changes only this one):

| `shape` | Size | Named |
|---|---|---|
| `"cube"` | 1 m | `Cube` |
| `"sphere"` | 1 m across | `Sphere` |
| `"plane"` | 10 m square, facing up | `Plane` |
| `"cylinder"` | 1 m across, 1 m tall | `Cylinder` |
| `"capsule"` | 1 m across, 2 m tall (tip to tip) | `Capsule` |

`color` is red, green and blue from 0 to 1 (default white). The primitive casts shadows. It has **no collider**: add one to make it solid, and a rigid body to make it fall.

```luau title="A crate that falls"
local crate = scene.SpawnPrimitive("cube", vec3(0, 6, 0), vec3(0.6, 0.4, 0.2))
crate:Add("BoxCollider")                              -- 1 m, like the cube
crate:Add("RigidBody", { motion = "Dynamic", mass = 20 })
```

### `scene.Each(component, fn)` {#each}

Calls `fn(entity)` for every entity that has the component, in the order they were made.

```luau
-- Wake up every body (after changing gravity, say)
scene.Each("RigidBody", function(entity)
    physics.WakeUp(entity)
end)

-- Count the coins left
local coins = 0
scene.Each("Scripts", function(entity)
    if entity:GetScript("Coin") then
        coins += 1
    end
end)
```

`fn` may change the scene (destroy, create, add components): the list is taken first, and entities destroyed on the way are skipped. Tags (components without fields, like `ShadowCasterTag`) can't be listed this way.

### `scene.Destroy(entity)` {#destroy}

The same as [`entity:Destroy()`](entity.md#destroy): the entity and everything below it are destroyed at the end of the current phase, after their scripts' `OnDestroy`. `nil` or an entity that's already gone is ignored.

## Spawning things that have scripts

Scripts can't put scripts on the entities they make (a script is an asset, chosen in the editor). Two ways to give spawned things behaviour:

- **A manager drives them.** One script spawns the entities, keeps them in a table and moves them all in its `Update`. This is how the [Snake tutorial](../../getting-started/first-game.md) works, and it is the fastest way to run many similar things.
- **Physics does the work.** Give them colliders, bodies, joints or water, and let the simulation move them. Contacts with them still reach the scripts on the things they touch (`OnContactBegin`'s `other`).

```luau title="A manager that moves what it spawns"
local Spinners = {} :: Script
Spinners.properties = { count = 8 }

function Spinners.Start(self: Script)
    self.items = {}
    for i = 1, self.count do
        local angle = i / self.count * math.pi * 2
        local item = scene.SpawnPrimitive("cube", vec3(math.cos(angle) * 4, 1, math.sin(angle) * 4))
        item.parent = self.entity
        table.insert(self.items, item)
    end
end

function Spinners.Update(self: Script, dt: number)
    for _, item in self.items do
        item:Rotate(vec3(0, 1, 0), 180 * dt)
    end
end

return Spinners
```

## Entities made while playing

Everything a script makes while playing is gone when you press Stop: the scene goes back to how it was when you pressed Play. (To keep changes, see [Keeping play-mode changes](../../manual/play-mode.md).)
