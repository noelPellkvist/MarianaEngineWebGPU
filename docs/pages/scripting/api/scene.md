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

An instance of a model (`.glb` / `.gltf`) or [prefab](#prefab-files) (`.prefab`) file, by its resource path (from the project folder, starting with `/`). Returns the model's root, placed at `position` with `rotation`. An animated model comes with its Animator playing its first clip.

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

### `scene.Clone(entity, position?, rotation?) -> Entity` {#clone}

A copy of the entity and everything below it: components, colliders, bodies, animation, and **scripts** (the copies' instances start with the next frame's scripts, with the property values the original's slots have, not its running state). The copy goes under the same parent, at a world `position` and `rotation` when given.

```luau
local enemy = scene.Clone(scene.Find("Enemy Template") :: Entity, vec3(4, 0, 10))
enemy.active = true
```

**Templates.** Build what you'll spawn in the editor, scripts and all, untick its **Active** box so it takes no part in the game, and clone it: the copies are inactive too until you set `active = true`. See the [spawner recipe](../cookbook.md#spawn-copies-of-a-template).

### `scene.Load(path)` {#load}

Switches to another scene: once this frame's scripts have run, the scene is replaced by the file's (with its lighting and pipeline), and its scripts start. Every running script ends first (`OnDestroy`), and its tasks with it. A path to a scene that isn't there is an error.

```luau
function Goal.OnTriggerEnter(self: Script, other: Entity)
    if other.name == "Player" then
        scene.Load("/Scenes/Level2.scene")
    end
end
```

Nothing carries over from one scene to the next but the [save data](save.md) (and `time.time`, which keeps counting): keep what the next scene needs there. A build takes the scenes its [profile](../../building/build-profiles.md) lists, so add every scene the game loads to it.

In the editor the loaded scene plays until you press Stop, which brings back the scene you were editing.

### `scene.Reload()` {#reload}

Starts the scene again, as it was when the game started (or as its file has it, after a `scene.Load`): a restart after game over. Like `Load`, it happens once this frame's scripts have run.

### `scene.Broadcast(name, ...) -> number` {#broadcast}

Calls the function `name` on every running script that has one (with its `self` and the arguments), in entity order, and returns how many did. Scripts on switched-off entities, disabled ones and ones that have stopped on an error don't hear it; scripts without such a function are skipped.

```luau
scene.Broadcast("OnRoundOver", winner)
-- any script can listen:
function Crowd.OnRoundOver(self: Script, winner: string)
    print(`{winner} wins!`)
end
```

To call one entity's scripts, use [`entity:Send`](entity.md#send).

## Prefab files

A **prefab** is an entity saved to a file, with everything below it: right-click it in the Hierarchy and choose **Save as Prefab**, and it is written to the project's `Prefabs/` folder. Spawn it like a model:

```luau
local crate = scene.Spawn("/Prefabs/Crate.prefab", vec3(0, 3, 0))
```

Its scripts come with it and start with the next frame's scripts. Builds always take the `Prefabs` folder (and what the prefabs use). A prefab file isn't linked to copies already in scenes: saving it again changes what is spawned from then on.

## Putting scripts on entities

### `entity:AddScript(path, values?)`

Puts a script on any entity, with values for its properties. See [Entity](entity.md#addscript).

```luau
local mine = scene.SpawnPrimitive("sphere", vec3(0, 0.5, 5), vec3(1, 0.2, 0.2))
mine:AddScript("/Scripts/Mine.luau", { radius = 3, damage = 50 })
```

For many similar things, one **manager** script that spawns them, keeps them in a table and moves them all in its `Update` is still the fastest way: this is how the [Snake tutorial](../../getting-started/first-game.md) works.

## Entities made while playing

Everything a script makes while playing is gone when you press Stop: the scene goes back to how it was when you pressed Play. (To keep changes, see [Keeping play-mode changes](../../manual/play-mode.md).)
