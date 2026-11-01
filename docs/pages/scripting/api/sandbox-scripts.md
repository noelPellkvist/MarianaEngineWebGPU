---
title: Sandbox Scripts
description: The ready-made scripts in the Sandbox project: VehicleDriver, CharacterDriver, ChaseCamera, Respawn, LaunchPad, Hover, WatchCar and Snake, with their properties.
---

# Sandbox Scripts

The Sandbox project comes with scripts you can use as they are, copy into your own project, or read as examples. They're in `projects/Sandbox/Scripts/`. The engine itself has no built-in game scripts: driving a car or walking a character is done by these, so you can change how your game controls by editing them.

The editor's **Create** menu puts some of them on what it makes: a vehicle gets VehicleDriver, a character CharacterDriver, and either gets a camera with ChaseCamera if the scene has none.

> [!NOTE]
> The Create menu finds these scripts by their resource paths (`/Scripts/VehicleDriver.luau` and so on). In a project of your own, copy the ones you want into its `Scripts/` folder with the same names, or the vehicle you create stays undriven (the log says which file is missing).

| Script | Goes on | Does |
|---|---|---|
| [VehicleDriver](#vehicledriver) | A vehicle | Drives it with the keyboard |
| [CharacterDriver](#characterdriver) | A character controller | Walks it with the keyboard, relative to the camera |
| [ChaseCamera](#chasecamera) | A camera | Follows the driven vehicle or character |
| [Respawn](#respawn) | Anything with a body | R puts it back at the start |
| [LaunchPad](#launchpad) | A trigger | Throws what enters it into the air |
| [Hover](#hover) | Anything | Bobs and turns |
| [WatchCar](#watchcar) | A dancer | Turns its head to watch the car |
| [Snake](#snake) | An empty entity | The whole Snake game |

## VehicleDriver {#vehicledriver}

Drives the entity's `Vehicle` (car, motorcycle or tank).

| Key | |
|---|---|
| W / Up | Throttle |
| S / Down | Brake; once (nearly) stopped, reverse |
| A D / Left Right | Steer |
| Space | Hand brake |

| Property | Default | |
|---|---|---|
| `steerSpeed` | 3 | How fast the steering follows the keys: full lock in 1 / `steerSpeed` seconds |
| `steerFade` | 0 | The speed (m/s) at which the keys give half the lock, less the faster it goes, so it doesn't spin out at speed. 0 = always full lock. The concept car uses 25 |

It writes the component's `throttle`, `steering`, `brake` and `handBrake` every frame: the pattern for any vehicle control (AI drivers, a gamepad mapping). See [Vehicles](../../manual/gameplay-physics.md#driving-from-code).

## CharacterDriver {#characterdriver}

Walks the entity's `CharacterController`.

| Key | |
|---|---|
| W A S D / arrows | Walk, relative to where the camera looks (the scene's first camera; else along +Z) |
| Shift | Run |
| Space | Jump |

No properties: the walk speed, run factor, jump speed and turning are the Character Controller's settings. It sets `move` (a direction, at most 1 long), `run` and `jump`.

## ChaseCamera {#chasecamera}

Keeps its entity (a camera) behind and above a target and follows it smoothly. It runs in `LateUpdate`, after physics has moved the target, so it never lags a frame.

| Property | Default | |
|---|---|---|
| `target` | `""` | The name of the entity to follow. Empty = the first one driven by a VehicleDriver or CharacterDriver |
| `distance` | 7 | Metres behind the target |
| `height` | 2.5 | Metres above it |
| `lookHeight` | 1 | It looks at this far above the target's origin |
| `stiffness` | 5 | How quickly it catches up (per second); higher is tighter |

## Respawn {#respawn}

Press a key to put the entity back where it was when the game started, at rest: for when the car lands on its roof.

| Property | Default | |
|---|---|---|
| `key` | `"R"` | The key, by [name](input.md#key-names) |
| `lift` | 0.3 | Metres above the start, so it drops onto its wheels |

## LaunchPad {#launchpad}

On an entity with a **trigger** collider: whatever enters the trigger is thrown up into the air and on along the pad's +Z (its forward arrow in the gizmo). The Sandbox's is at (-10, 0, 20).

| Property | Default | |
|---|---|---|
| `up` | 11 | m/s upward |
| `forward` | 4 | m/s added along the pad's +Z |

It keeps the body's speed along the ground, adds `forward` along the pad's +Z and sets its upward speed to `up`. Only dynamic bodies (the car, crates) are thrown; other things pass through.

## Hover {#hover}

Bobs up and down around where it was put and turns about the world's up axis: for pickups and markers.

| Property | Default | |
|---|---|---|
| `height` | 0.25 | Metres up and down |
| `bobSpeed` | 2 | Radians per second (one bob every π seconds at 2) |
| `turnSpeed` | 90 | Degrees per second |

## WatchCar {#watchcar}

On a model whose Animator plays the Sandbox's `Dancer.animgraph` (its dance goes through a **Look At** node of the head, neck and chest): the head turns to watch the car as it comes near, and back to dancing as it goes. It sets the graph's `lookTarget` (a Vector parameter) to the car and eases `lookWeight` toward 1 inside `range`.

| Property | Default | |
|---|---|---|
| `target` | `""` | The name of the entity to watch. Empty = the first one driven by a VehicleDriver |
| `range` | 25 | It starts watching closer than this (m) |
| `eyeHeight` | 1 | Looks at this far above the car's origin (m) |
| `attention` | 3 | How quickly it turns to watch and away (per second) |

Use it as the model for any IK target from a script: see [IK targets](animator.md#ik-targets).

## Snake {#snake}

The finished game of the [Your First Game](../../getting-started/first-game.md) tutorial: put it on an empty entity at the origin of a scene with a camera looking down at it. Arrow keys or W A S D steer; red apples make the snake longer and faster; hitting a wall or itself ends the game, the snake flies apart, and a new game starts 2.5 s later.

| Property | Default | |
|---|---|---|
| `size` | 15 | The board is `size` × `size` cells of 1 m |
| `stepTime` | 0.16 | Seconds per step at the start |
| `speedUp` | 0.96 | Each apple makes a step take this share of the time it took before |
| `startLength` | 3 | Segments at the start |

The Sandbox's `Scenes/Snake.scene` is set up with it.
