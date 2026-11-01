---
title: Scripting Overview
description: How gameplay code works in the Mariana Engine: Luau scripts on entities, their lifecycle, their state, and what they can reach.
---

# Scripting Overview

Gameplay in the Mariana Engine is written in **[Luau](https://luau.org)**, the fast, typed language derived from Lua 5.1 that powers Roblox. Scripts are small files that you attach to entities; the engine calls their functions as the game runs. They can move things, read the keyboard and mouse, spawn and destroy entities, change any component, drive vehicles and characters, query and push physics, and control animation.

Even the engine's sample controls are scripts: driving the car, walking the character and the chase camera are Luau files in the Sandbox project that you can read, copy and change.

## A script at a glance

```luau title="Spin.luau"
--!strict
local Spin = {} :: Script

-- Values you can set per entity in the Inspector, with their defaults.
Spin.properties = {
    speed = 90,              -- degrees per second
    axis = vec3(0, 1, 0),
}

-- Once, when the game starts (or the entity appears).
function Spin.Start(self: Script)
    print(`{self.entity.name} starts spinning`)
end

-- Every frame, with the seconds since the last one.
function Spin.Update(self: Script, dt: number)
    self.entity:Rotate(self.axis, self.speed * dt)
end

return Spin
```

1. A script **returns a table**. Its functions with known names (`Start`, `Update`, `OnTriggerEnter`, …) are called by the engine.
2. Each entity with the script gets its own **`self`**: a table that holds that entity's state. `self.entity` is the entity.
3. **`properties`** lists values the Inspector edits per entity. They're put into `self` before `Start`, so the code reads `self.speed`.
4. Everything else comes from a handful of **globals**: `scene`, `input`, `time`, `physics`, `vec3`, `quat`, and Luau's standard library.

## Attaching scripts

A script runs on every entity that has it in its **Scripts** component:

1. Select the entity, **Add Component > Scripting > Scripts**.
2. **Add Script**: type a name and **Create** to make a new file in `Scripts/` from a template, or pick one of the project's scripts from the list. You can also drag a `.luau` file from the Assets window onto the section.

An entity can have several scripts, and a script can be on many entities. Each pairing is an independent instance with its own `self`. The Inspector shows, per script: an **enabled** box, **Edit** (opens the Script Editor), **Remove**, any error, and its properties.

## When scripts run

Scripts run while the game plays (Play in the editor, or always in a built game). Each frame:

| Order | What happens |
|---|---|
| 1 | Input is read |
| 2 | Scripts: `Start` for scripts that are new this frame, then **`Update(dt)`** for every script |
| 3 | Animation updates, then **physics** steps |
| 4 | Scripts: physics events (`OnContactBegin/End`, `OnTriggerEnter/Exit`), then animation events (`OnAnimationEvent`), then **`LateUpdate(dt)`** |
| 5 | Transforms are worked out and the frame is drawn |

Entities run in the order they were created, and an entity's scripts in the order they were added. See [Script Lifecycle](lifecycle.md) for every function.

## What scripts can reach

| Global | For | Reference |
|---|---|---|
| `scene` | Finding, creating, spawning and destroying entities | [`scene`](api/scene.md) |
| `input` | Keyboard and mouse | [`input`](api/input.md) |
| `time` | Game time, frame count, slow motion | [`time`](api/time.md) |
| `physics` | Raycasts, overlaps, forces, velocities, vehicles, characters, ragdolls, water | [`physics`](api/physics.md) |
| `vec3`, `vector` | 3D vectors | [`vec3` and `vector`](api/vec3.md) |
| `quat` | Rotations | [`quat`](api/quat.md) |
| `print`, `warn` | Messages to the log | [Debugging](debugging.md) |

Entities are [`Entity`](api/entity.md) values with properties (`position`, `rotation`, …) and methods (`Rotate`, `LookAt`, `Get`, …). Every component on an entity can be read and changed by name, field by field: `entity:Get("RigidBody").mass = 20`. See [Components](api/components.md) and the [Component Reference](api/component-reference.md).

## Live editing

- Save a script (**Ctrl+S** in the Script Editor, or in any editor you like) while the game runs and it **reloads at once**. Every instance keeps its `self`, so you keep playing from where you were with the new code.
- Change a property in the Inspector while playing and the running instance gets the new value.
- A reload with a syntax error keeps the old code running until the error is fixed.

## Errors and safety

- A script that raises an error is **stopped**: the error and its line are shown in the Inspector and the log. The rest of the game keeps running. It starts again when you change the script.
- A call that runs longer than 2 seconds (an endless loop) is stopped the same way, so a mistake never freezes the editor.
- Scripts are **sandboxed**: they can't read or write files, open network connections or reach anything outside the game. Their globals are read-only, so one script can't break another by accident.

## Types and tooling

Scripts are checked by Luau's type checker against the engine's API as you type, in the editor's [Script Editor](script-editor.md) and in VS Code. A misspelt method, a wrong argument or a component field that doesn't exist is underlined before you even run the game. Start a script with `--!strict` for full checking.

## Performance

Natively, scripts are compiled to machine code (Luau's native code generator); on the web they run in Luau's fast interpreter. Every script function is timed by the [Profiler](../manual/profiler.md), named like `Spin.Update`, so you can see what each costs.

## Where to go next

- [Your First Script](first-script.md): a hands-on walkthrough.
- [Luau for Game Scripting](luau.md): the language, quickly.
- [Cookbook](cookbook.md): ready-made answers to common tasks.
- [API Reference](api/index.md): everything, in detail.
