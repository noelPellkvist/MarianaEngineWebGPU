---
title: Animator
description: Driving animation from Mariana scripts: graph parameters, triggers, IK targets, playing and cross-fading clips, state machines, root motion and animation events.
---

# Animator

The `Animator` component plays a model's animation. Get it from the model's root (the entity you spawned or dropped in) and drive it with its methods:

```luau
local animator = self.entity:Get("Animator")
if animator then
    animator:SetFloat("speed", 3.5)       -- a graph parameter
    animator:SetTrigger("jump")           -- fires a transition once
end
```

An Animator either runs an **animation graph** (blend trees, state machines and IK, made in the [Animation Graph window](../../manual/animation.md#the-animation-graph-window)) or, without one, plays its **clips** directly. Graph parameters are how scripts talk to a graph; `Play` and `CrossFade` are for Animators without one.

> [!TIP]
> Keep the Animator in `self` in `Start` instead of calling `Get("Animator")` every frame. The component value stays valid as long as the entity has the component.

## Graph parameters

A graph's parameters are its inputs: the Inspector lists them under the Animator, and the graph's conditions, blends and IK nodes read them. Set them every frame, or when something changes.

| Method | |
|---|---|
| `animator:SetFloat(name, value)` | Sets a Float parameter (a blend's position, a speed, a weight) |
| `animator:GetFloat(name) -> number` | Its value now |
| `animator:SetBool(name, on)` | Sets a Bool parameter (`grounded`, `crouching`) |
| `animator:GetBool(name) -> boolean` | |
| `animator:SetTrigger(name)` | Sets a Trigger. It stays set until a transition that checks it starts, which resets it |
| `animator:ResetTrigger(name)` | Clears a trigger nothing used |
| `animator:SetVector(name, point)` | Sets a Vector parameter: a **world position**, for IK targets (where a hand reaches, what a head looks at) |
| `animator:GetVector(name) -> vector` | |

A name the graph doesn't have is ignored (and reads as 0, `false` or a zero vector), so a script can drive several models whose graphs share only some parameters. Parameter values keep while the graph is edited in the editor.

```luau title="Blend walk and run from a character's speed"
local Locomotion = {} :: Script

function Locomotion.Start(self: Script)
    self.animator = self.entity:Get("Animator")
end

function Locomotion.Update(self: Script, dt: number)
    local info = physics.GetCharacterInfo(self.entity)
    if self.animator and info then
        local v = info.velocity
        self.animator:SetFloat("speed", vector.magnitude(vec3(v.x, 0, v.z)))
        self.animator:SetBool("grounded", info.grounded)
    end
end

return Locomotion
```

### Triggers

A trigger is a one-shot: set it, and the first transition whose conditions include it starts and clears it. If no transition can use it right now (the state machine is somewhere else), it waits until one can; call `ResetTrigger` to cancel it.

```luau
if input.GetKeyDown("space") then
    animator:SetTrigger("jump")
end
```

### IK targets

Two Bone IK and Look At nodes take their targets from Vector parameters, in world coordinates. Set them every frame from whatever they should follow, and fade the node in with a Float weight parameter:

```luau
-- Aim the head at the player (the graph has a Look At node using "lookTarget" and "lookWeight")
local player = scene.Find("Player")
if player then
    animator:SetVector("lookTarget", player.worldPosition + vec3(0, 1.6, 0))
    animator:SetFloat("lookWeight", 1)
end
```

The Sandbox's [WatchCar.luau](sandbox-scripts.md#watchcar) does exactly this, easing the weight in as the car comes near.

## State machines

State machines move by their transitions, which your parameters drive. To jump straight to a state instead:

### `animator:CrossFadeState(state, seconds?) -> boolean` {#crossfadestate}

Moves the first state machine with a state of that name to it, cross-fading over `seconds` (default 0.25; 0 = at once), whatever its transitions say. Returns `false` if no state machine in the graph has that state.

```luau
animator:CrossFadeState("Celebrate", 0.3)
```

Called before the Animator has run its first update (in `Start`), it moves there as soon as the graph is ready.

The `states` field reads the state each state machine is in, where it isn't its entry state, and setting it moves them there at once:

```luau
print(animator.states[1])     -- "Run", or nil while in the entry state
```

## Playing clips without a graph

An Animator without a graph plays its clips (the ones listed under it in the Inspector; a model's own animations are there when it loads). Clips are given by **name** or by **index from 0**.

### `animator:Play(clip, loop?)` {#play}

Plays `clip` from its start, at once. `loop` defaults to `true`.

### `animator:CrossFade(clip, seconds?, loop?)` {#crossfade}

Fades from what plays now to `clip` over `seconds` (default 0.25). The clips playing keep playing under the new one until it is fully in.

```luau
local wanted = if moving then "Walk" else "Idle"
if wanted ~= self.clip then          -- only when it changes
    self.clip = wanted
    animator:CrossFade(wanted, 0.25)
end
```

> [!WARNING]
> Every `CrossFade` call starts the clip again from its beginning, fading it in. Call it when what should play **changes**, as above, not every frame. With 0 seconds (or nothing playing yet) it is the same as `Play`.

A clip name the Animator doesn't have is an error (`the Animator has no clip 'Wlak'`). With a graph, the graph decides what plays: use parameters or `CrossFadeState` instead.

## Fields

The Animator's fields are read and written like any component's (see [Components](components.md)):

| Field | Type | |
|---|---|---|
| `speed` | number | Playback speed of everything it plays (1 = as authored; 0 holds the pose) |
| `playing` | boolean | `false` pauses it |
| `rootMotion` | boolean | The clips move the entity (or its character controller) instead of the hips. See below |
| `rootBone` | string | The bone root motion comes from; empty = found automatically |
| `clip` | number | Without a graph: the clip playing (index from 0; -1 = none). Setting it switches to that clip at the current `time`; `Play` starts one from its beginning |
| `loop` | boolean | Whether that clip loops |
| `time` | number | Its time in seconds. Setting it jumps there |
| `states` | table of strings | See [State machines](#state-machines) |

```luau
animator.speed = 0.5            -- slow motion for this model only
animator.time = 0               -- back to the start
```

The skeleton, the clip list and the graph are assets: choose them in the Inspector.

## Root motion

With `rootMotion` on, walking and turning come from the animation itself: the clips move the entity, and the hips stay over it. On an entity with a [character controller](../../manual/gameplay-physics.md#characters) the motion walks the character instead (so walls, stairs and gravity still apply).

### `animator:GetRootVelocity() -> (vector, number)` {#getrootvelocity}

The root motion of the last update: the velocity in metres per second in the model's own axes (+Z forward), and the turn rate in degrees per second.

```luau
local velocity, turnRate = animator:GetRootVelocity()
print(string.format("walking %.2f m/s, turning %.0f deg/s", velocity.z, turnRate))
```

## Animation events

Clips can carry named markers (add them under the Animator's Events in the Inspector, or in the graph window's clips). When a clip that makes up at least a quarter of the pose passes one, every script on the **same entity** as the Animator gets `OnAnimationEvent`:

```luau title="Footsteps.luau"
local Footsteps = {} :: Script

function Footsteps.OnAnimationEvent(self: Script, name: string, clip: string)
    if name == "Footstep" then
        print("step in", clip, "at", time.time)
    end
end

return Footsteps
```

Events fire after physics, before `LateUpdate`, once per name per frame (the heavier clip wins when two fire the same name). See [Animation events](../../manual/animation.md#animation-events).

## Ragdolls

A model with a `Ragdoll` component can go limp and be pushed around: see [`physics.SetRagdollMode`](physics.md#setragdollmode) and [`physics.AddRagdollImpulse`](physics.md#addragdollimpulse).
