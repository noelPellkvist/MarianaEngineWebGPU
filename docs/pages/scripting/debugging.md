---
title: Debugging and Performance
description: Find problems in scripts with print, errors and the Profiler, understand common error messages, and keep scripts fast.
---

# Debugging and Performance

## Printing

`print(...)` and `warn(...)` write to the log, with the script's name and line: `[Pickup:17] Car collected Coin`. In the editor the log appears in the terminal it was started from (and in the browser's console for the web editor); in a built game, in its console or the browser's.

```luau
print("speed", speed)                          -- several values, separated by tabs
print(`position {self.entity.position}`)        -- vectors print as x, y, z
print(entity)                                   -- Entity(42 'Crate')
warn("no target found")                         -- shows as a warning
```

Print sparingly in `Update`: 60 lines a second hide everything else. Print when something changes instead.

## Errors

When a script function raises an error:

- that **instance** stops (no more calls to it) until the script file changes;
- the error, with the line and a traceback, appears in **red in the Inspector** under the script, and in the log;
- every other script keeps running.

Fix the code and save: the instance starts again.

Raise your own errors with `error("message")`, or check assumptions with `assert(condition, "message")`. To try something that might fail without stopping the script, use `pcall`:

```luau
local ok, message = pcall(function()
    body.notAField = 3
end)
if not ok then
    warn(message)
end
```

### Common error messages

| Message | Meaning |
|---|---|
| `attempt to index nil with 'position'` | The value before `.position` is `nil`: an entity that wasn't found (`scene.Find` returned nil), a component the entity doesn't have (`Get` returned nil) |
| `RigidBody has no field 'masss'` | A component field that doesn't exist (check the [Component Reference](api/component-reference.md)) |
| `there is no component named 'RigidBdy'` | A component name that doesn't exist |
| `mass expects a number, got string` | A field given the wrong type |
| `some fields given for BoxCollider aren't among its fields` | `Add("BoxCollider", { ... })` with a misspelt field |
| `the entity no longer has its RigidBody` | A component value kept after the component (or the entity) was removed |
| `there is no key named 'arrowup'` | A key name `input` doesn't know: see [key names](api/input.md#key-names) |
| `Entity has no member 'Positon'` | A misspelt entity property |
| `can't set 'forward' on an Entity` | A read-only property |
| `an entity can't go below itself` | Setting `parent` to the entity itself or one of its children |
| `the Animator has no clip 'Run'` | A clip name the Animator doesn't have (names are case-sensitive) |
| `the script ran too long (an endless loop?)` | A call ran longer than 2 seconds: usually a `while` loop that never ends |

Most of these are caught by the [type checker](script-editor.md#type-checking) before you play. Keep `--!strict` at the top of your scripts.

## Inspecting state while playing

- **Select** the entity in the Scene view or Hierarchy: the Inspector shows its components live (positions, velocities, a vehicle's gear, an Animator's state) and the script's properties.
- **Pause** (Ctrl+Shift+P) and **Step** (Ctrl+Alt+P) to go through the game frame by frame.
- The **Physics** window's debug view draws bodies, contacts and velocities.
- `tostring` and string interpolation print tables' addresses, not their contents. Print the fields you want.

## Performance

Scripts are fast (natively, Luau compiles them to machine code), but they run every frame, for every entity that has them. A few habits keep them cheap:

| Do | Instead of |
|---|---|
| Find entities once (in `Start`, or the first time) and keep them in `self` | `scene.Find` or `scene.Each` every frame |
| Keep a component in `self` if you change it often (check it's still there with `IsValid` on the entity) | `entity:Get(...)` several times a frame |
| React to events (`OnTriggerEnter`, `OnContactBegin`) | Checking distances to everything every frame |
| Use timers to do heavy work every few frames | Doing it every frame |
| Spawn once and reuse (move, hide by moving away) | Spawning and destroying many entities every second |
| Keep work out of `Update` when nothing changes | Setting the same position every frame |

Measure with the [Profiler](../manual/profiler.md): every script function is a scope of its own (`Coin.Update`, `Shooter.Update`, …), and the *Script instances* counter shows how many are running. If a script's scope is large, it's that script.
