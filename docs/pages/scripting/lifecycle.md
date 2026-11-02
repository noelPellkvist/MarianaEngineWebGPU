---
title: Script Lifecycle and Events
nav: Lifecycle and events
description: Every function the engine calls on a script, when it is called, in what order, and with which arguments.
---

# Script Lifecycle and Events

A script is a table of functions. The engine calls the ones with these names, on each entity that has the script, passing that instance's `self` first. Leave out any you don't need.

| Function | Called | Arguments |
|---|---|---|
| `Start(self)` | Once, before the instance's first `Update` | |
| `Update(self, dt)` | Every frame the game moves | `dt`: seconds since the last frame (game time) |
| `LateUpdate(self, dt)` | Every frame, after physics and animation | `dt` |
| `FixedUpdate(self, dt)` | Before every physics step | `dt`: the step's length (1/60 s by default) |
| `OnContactBegin(self, other, contact)` | A body of this entity began touching another | `other`: the [Entity](api/entity.md); `contact`: a [Contact](api/types.md#contact) |
| `OnContactEnd(self, other)` | They stopped touching | `other` |
| `OnTriggerEnter(self, other)` | A trigger and a body met (on both sides) | `other` |
| `OnTriggerExit(self, other)` | They parted | `other` |
| `OnAnimationEvent(self, name, clip)` | The entity's Animator passed an event | `name`, `clip`: strings |
| `OnDestroy(self)` | The instance ends | |

## The frame, step by step

```text
Frame N
  ├─ input is read
  ├─ scripts: Start (new instances, in entity order), then Update (everyone)
  │     then tasks whose wait is over (task.wait)
  │     destroys asked for during these calls happen here
  ├─ animation: Animators advance and pose their models
  ├─ physics: fixed steps (bodies, vehicles, characters, water, ragdolls, soft bodies),
  │     each after FixedUpdate (everyone)
  ├─ scripts: physics events (contacts, triggers) for this frame's steps
  │           animation events for this frame
  │           LateUpdate (everyone)
  ├─ transforms are worked out
  └─ the frame is drawn
```

Within each phase, entities go in the order they were created (their id), and an entity's scripts in the order they're listed in its Scripts component.

## Start

`Start` runs once per instance, on the first frame it exists, before any `Update` of that frame. Every instance that's new on a frame gets its `Start` before **any** instance gets its `Update`, so in `Start` you can rely on other scripts' `self` existing, but not on their own `Start` having run (that depends on order). Use `Start` to remember initial values and set up state:

```luau
function Door.Start(self: Script)
    self.closed = self.entity.rotation
    self.open = false
end
```

Properties are already in `self` when `Start` runs.

Instances start:

- when the game starts (Play, or a built game launching),
- when a script is added to an entity in the Inspector while playing,
- again after an error, once the script file is changed.

> [!NOTE]
> Entities a script makes get scripts with [`entity:AddScript`](api/entity.md#addscript), or come with them when cloned from a template or spawned from a prefab; their `Start` runs on the next frame.

## Update and dt

`Update` runs every frame the game moves: not while paused (but on each **Step**). `dt` is the frame's game time in seconds: typically 1/60 to 1/144, capped at 0.25 s for a frame that took longer (after a hitch, the game slows down rather than jumping). With `time.scale` set to 0.5, `dt` is half as long.

Multiply speeds by `dt` so they're per second, whatever the frame rate:

```luau
self.entity.position += self.entity.forward * self.speed * dt
```

## LateUpdate

`LateUpdate` runs after physics and animation have moved everything this frame. It's the place for **cameras** that follow something (so they see where it ended up, not where it was) and for anything that reacts to the frame's final positions.

## FixedUpdate

Physics runs in fixed steps of 1/60 s (the Physics settings' steps per second), as many as the frame needs: none on some frames at 144 fps, two or more at 30 fps. `FixedUpdate` runs before **each** step, with that step's length as `dt`, so a force added there acts on exactly that step:

```luau
function Rocket.FixedUpdate(self: Script, dt: number)
    physics.AddForce(self.entity, self.entity.up * self.thrust)
end
```

Use it for forces that depend on the body's state as it is step by step (springs, hovering, steering by velocity): the result doesn't change with the frame rate. A force added in `Update` acts through every step of that frame's physics, which is fine for steady pushes. Reading input belongs in `Update` (a key pressed this frame is seen once there, however many steps there are). See [Hovering](cookbook.md#hovering).

## Messages

Scripts call each other's functions by name with [`entity:Send(name, ...)`](api/entity.md#send) (the scripts on one entity) and [`scene.Broadcast(name, ...)`](api/scene.md#broadcast) (every script): any function of a script can receive them, called at once with that script's `self`.

## Physics events

These are called after the frame's physics steps, for everything that happened in them, before `LateUpdate`.

```luau
function Crate.OnContactBegin(self: Script, other: Entity, contact: Contact)
    if contact.speed > 5 then
        print(`{self.entity.name} hit {other.name} at {contact.speed} m/s`)
    end
end
```

- **Contacts** are between solid bodies. Both entities' scripts get the event, each with the other as `other` and the normal pointing from itself towards the other. `contact` has `point` (world position), `normal` and `speed` (how fast they met). A pair of bodies gets one Begin however many parts touch, and one End when they all part.
- **Triggers** are colliders with *trigger* on. Both the trigger's scripts and the body's get `OnTriggerEnter` / `OnTriggerExit`. Triggers notice dynamic, kinematic and static bodies and characters; a body that falls asleep inside a trigger doesn't "leave" it.
- Events come from the entity that **has the body**: a collider on a child belongs to the rigid body above it, so the event goes to the rigid body's entity.

## Animation events

`OnAnimationEvent(self, name, clip)` is called when the entity's Animator passes a named event in a clip, after physics, before `LateUpdate`. See [Animation events](../manual/animation.md#animation-events).

```luau
function Footsteps.OnAnimationEvent(self: Script, name: string, clip: string)
    if name == "Footstep" then
        -- play a sound, spawn dust, shake the camera...
    end
end
```

## OnDestroy

`OnDestroy` runs when an instance ends:

- its entity (or an entity above it) is destroyed,
- the script is removed from the entity, or the Scripts component is removed,
- the game stops (Stop in the editor; the game closing).

Use it to clean up what the instance made that isn't below its entity:

```luau
function Turret.OnDestroy(self: Script)
    if self.laser and self.laser:IsValid() then
        self.laser:Destroy()
    end
end
```

## Destroying entities

`entity:Destroy()` and `scene.Destroy(entity)` don't destroy at once: the entity is destroyed at the end of the current script phase (after every `Start`/`Update`, or after every event/`LateUpdate`), with `OnDestroy` called for every script on it and below it. Until then the entity still exists, so code that runs later in the same phase isn't surprised by it disappearing. Use `entity:IsValid()` to check whether an entity you hold is still alive.

## Errors

If any of these functions raises an error, that **instance** stops: no more calls to it until the script file changes. The error, with a traceback, is shown in the Inspector under the script and in the log. Other instances of the same script, and every other script, keep running. See [Debugging](debugging.md).

## Reloading

Saving a script while the game runs reloads it: the next calls use the new functions, and each instance keeps its `self`. Properties the new version adds get their defaults; instances that had stopped with an error start again. `Start` isn't called again for instances that already started.
