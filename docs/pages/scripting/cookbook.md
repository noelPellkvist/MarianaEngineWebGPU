---
title: Scripting Cookbook
nav: Cookbook
description: Complete, tested scripts for common gameplay tasks: movement, timers, shooting, pickups and scores, cameras, raycasts, triggers, explosions, platforms, doors, vehicles, animation, ragdolls, slow motion, spawning and game flow.
---

# Scripting Cookbook

Ready-to-use scripts for everyday gameplay tasks. Each is a complete file: create a script with the same name (Inspector > Scripts > **Add Script**), paste it in, and attach it as described. Every script here is type-checked in strict mode and run by the engine's test suite, so they work as printed.

> [!TIP]
> All of them have `properties`, so after attaching one, tune it in the Inspector without touching the code.

## Movement and input

### Move with the keyboard

Moves its entity along its own axes with **W A S D** or the arrows; **Q** and **E** turn it. Works on any entity without a dynamic rigid body (for physics bodies, set velocities or forces instead, see below).

```luau title="KeyboardMover.luau"
@include docs/snippets/cookbook/KeyboardMover.luau
```

- `Translate` moves along the entity's **own** axes (forward is its +Z); to move along the world's axes, add to `self.entity.position` instead.
- For a walking character with gravity, slopes and stairs, use a [Character Controller](../manual/gameplay-physics.md#characters) and the Sandbox's [CharacterDriver](api/sandbox-scripts.md#characterdriver).

### Timers and cooldowns

There's no `wait()` in scripts: a function must return so the frame can go on. Keep timers in `self` and count them down in `Update`.

```luau title="Timers.luau"
@include docs/snippets/cookbook/Timers.luau
```

- Repeating timers subtract the interval instead of resetting to zero, so they don't drift.
- Cooldowns compare against `time.time`, the game's clock since Play (it stops while paused and follows `time.scale`).

## Spawning

### Shoot projectiles

**Space** fires physics balls along the entity's forward; old balls are cleaned up.

```luau title="Shooter.luau"
@include docs/snippets/cookbook/Shooter.luau
```

- `startVelocity` gives a new rigid body its speed the moment it's created. Its body is made at the next physics step, so `physics.SetVelocity` on the same frame wouldn't find it yet.
- `continuous = true` stops fast, small bodies from tunnelling through thin walls.
- Keep the entities you spawn in a list so you can clean them up; iterate backwards when removing.

### Spawn things at random

Drops crates at random places in an area, keeping at most a set number.

```luau title="RandomSpawner.luau"
@include docs/snippets/cookbook/RandomSpawner.luau
```

`scene.SpawnPrimitive(shape, position, color, rotation)` makes a cube, sphere, plane, cylinder or capsule with a material of its own. To spawn a model, use `scene.Spawn("/Models/Thing.glb", position, rotation)`.

## Scores and game flow

### A game manager and pickups

A **GameManager** keeps the score and lives; **Coins** report to it. Create an empty entity named **Game** with `GameManager.luau`, and give each coin a trigger collider and `Coin.luau`.

```luau title="GameManager.luau"
@include docs/snippets/cookbook/GameManager.luau
```

```luau title="Coin.luau"
@include docs/snippets/cookbook/Coin.luau
```

- `GetScript("GameManager")` returns the manager's `self`; calling `manager:AddScore(10)` runs its function with that `self`. See [Calling another script](properties.md#calling-another-script).
- The `collected` flag stops a coin from counting twice when two bodies touch it on the same frame (it's only destroyed at the end of the frame).

### Rounds: a game flow state machine

Countdown, play, results, repeat: a small state machine in plain Lua.

```luau title="RoundTimer.luau"
@include docs/snippets/cookbook/RoundTimer.luau
```

## Cameras

### Smooth follow camera

Put it on a camera. It follows a target by name from a fixed offset and keeps looking at it.

```luau title="SmoothFollow.luau"
@include docs/snippets/cookbook/SmoothFollow.luau
```

- Cameras move in **`LateUpdate`**, after physics has moved the target this frame. In `Update` the camera would lag a frame behind and jitter.
- `1 - math.exp(-stiffness * dt)` makes the easing feel the same at any frame rate.
- For a camera that stays behind a vehicle, see the Sandbox's [ChaseCamera](api/sandbox-scripts.md#chasecamera).

### Orbit camera with the mouse

Drag with the left mouse button to orbit around a target, scroll to zoom.

```luau title="OrbitCamera.luau"
@include docs/snippets/cookbook/OrbitCamera.luau
```

`input.GetMouseDelta()` is how far the mouse moved this frame, in pixels; `input.GetScroll()` the wheel. Both are vectors (`x`, `y`, `0`).

## Physics

### Raycasts: a laser pointer

Casts a ray along the entity's forward, puts a marker where it hits, and pushes what it hits when you click.

```luau title="Laser.luau"
@include docs/snippets/cookbook/Laser.luau
```

- `physics.Raycast` returns the nearest hit (`entity`, `point`, `normal`, `distance`) or `nil`. Pass `{ ignore = self.entity }` so a ray from inside a collider doesn't hit its own entity.
- Raycasts see the physics world as the last physics step left it, and only bodies (entities with colliders).
- The hit entity is the one with the **body**: for a collider on a child, the rigid body's entity above it.

### Kill zones and respawning

A trigger that sends whatever falls in back to a spawn point. Make a big box collider with *trigger* on below your level, and an empty entity named **Spawn Point**.

```luau title="KillZone.luau"
@include docs/snippets/cookbook/KillZone.luau
```

`physics.Teleport` moves the entity and its physics body together, so it doesn't sweep through things on the way. Setting `position` on a dynamic body works too (the body follows), but teleporting is clearer.

### Explosions

Throws every dynamic body within a radius away from the centre, harder the closer it is. Ragdolls go limp and are thrown too.

```luau title="Explosion.luau"
@include docs/snippets/cookbook/Explosion.luau
```

- `physics.OverlapSphere` returns every entity whose body touches the sphere.
- An **impulse** is an instant kick (newton-seconds: an impulse of 10 on a 2 kg body adds 5 m/s). A **force** (`AddForce`) pushes over time; call it every frame for a steady push.
- A ragdoll's bodies are its **bones**; the Ragdoll component is on the model's root. Walk up `parent` to find it.

### Moving platforms

A platform that moves back and forth. Give it a **Kinematic** rigid body and a collider: kinematic bodies follow their entity and carry characters and crates.

```luau title="MovingPlatform.luau"
@include docs/snippets/cookbook/MovingPlatform.luau
```

### Automatic doors

A sliding door that opens while anything is in a trigger in front of it.

```luau title="AutoDoor.luau"
@include docs/snippets/cookbook/AutoDoor.luau
```

`next(t) ~= nil` is the Lua way to ask whether a table has any entries. The trigger's script keeps a set of what's inside, so the door stays open while *anyone* is there.

For a door that swings on hinges and can be pushed, use a [Hinge joint](../manual/physics.md#joints) instead (Create > Physics > Door).

### Floating and water

Reports when its entity goes in and out of water. With a dynamic rigid body, the entity floats in any Water Volume by itself; this is for gameplay that reacts to it.

```luau title="Buoy.luau"
@include docs/snippets/cookbook/Buoy.luau
```

### Knocking down ragdolls

**F** shoves every ragdoll near the entity: they go limp, fly away, and get back up a few seconds later.

```luau title="Knockout.luau"
@include docs/snippets/cookbook/Knockout.luau
```

Ragdolls can also be knocked out by hits on their own: set the Ragdoll's `limpOnImpact` and `knockback` (the Playground's dancers are set up this way).

## Vehicles and characters

### Read a vehicle's speed

Put it on a vehicle (an entity with a Vehicle component).

```luau title="Speedometer.luau"
@include docs/snippets/cookbook/Speedometer.luau
```

To drive a vehicle from code, set its Vehicle component's `throttle`, `steering`, `brake` and `handBrake` every frame: see [Vehicles](../manual/gameplay-physics.md#driving-from-code) and the Sandbox's [VehicleDriver](api/sandbox-scripts.md#vehicledriver).

### Animate a character from its movement

Feeds a character controller's speed and ground state into its animation graph's parameters.

```luau title="AnimatedWalker.luau"
@include docs/snippets/cookbook/AnimatedWalker.luau
```

The graph needs a float parameter `speed` driving a **Blend 1D** (idle at 0, walk at about 1.5, run at about 4), a bool `grounded`, and a trigger `jump` used by a transition into a jump state. See [Animation](../manual/animation.md).

## Time

### Slow motion

Hold **Tab** for slow motion, eased in and out.

```luau title="SlowMotion.luau"
@include docs/snippets/cookbook/SlowMotion.luau
```

`time.scale` slows everything on game time: scripts' `dt`, physics and animation. Set it back to 1 when you're done; this script does that in `OnDestroy` too, so stopping the game never leaves it slow.

## More examples

The Sandbox project's own scripts are complete examples too: driving vehicles, walking characters, chase cameras, respawning, launch pads, hovering markers, heads that watch the car, and the Snake game. See [Sandbox Scripts](api/sandbox-scripts.md).
