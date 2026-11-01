---
title: Vehicles, Characters, Water, Ragdolls and Soft Bodies
nav: Gameplay physics
description: The engine's ready-made gameplay physics: drivable vehicles, a character controller, buoyant water, ragdolls and cloth and jelly soft bodies.
---

# Vehicles, Characters, Water, Ragdolls and Soft Bodies

On top of rigid bodies and joints, the engine has complete gameplay systems you add as components. Each has a ready-made example in the **Create** menu: start from one and change it.

## Vehicles

A **Vehicle** component on a dynamic rigid body (the chassis) and a **Vehicle Wheel** component on each wheel entity below it make a drivable vehicle.

| Type | Steers by | Drive |
|---|---|---|
| **Wheeled** | Turning the front wheels | Front, rear or all wheels, with an engine, an automatic or manual gearbox, differentials and anti-roll bars |
| **Motorcycle** | The front wheel, leaning into turns | The rear wheel |
| **Tracked** | Slowing the inner track (turns on the spot when standing) | Both tracks |

**Create > Vehicle > Car, Motorcycle, Tank** make one from primitives, 8 m in front of the camera, with the [`VehicleDriver`](../scripting/api/sandbox-scripts.md#vehicledriver) script and a chase camera. **Create > Vehicle > Playground** adds a test track with jumps, ramps, logs and crates.

### Driving from code

A vehicle is driven by four inputs on its Vehicle component. Scripts set them every frame:

| Field | Range | |
|---|---|---|
| `throttle` | -1 to 1 | Forward and (negative) reverse |
| `steering` | -1 to 1 | Left (negative) to right |
| `brake` | 0 to 1 | |
| `handBrake` | 0 to 1 | |

```luau
local car = self.entity:Get("Vehicle")
car.throttle = if input.GetKey("w") then 1 else 0
car.steering = (if input.GetKey("d") then 1 else 0) - (if input.GetKey("a") then 1 else 0)
```

`physics.GetVehicleInfo(entity)` returns its speed (m/s), engine RPM, gear and each wheel's contact, suspension, steering angle and slip.

### Your own car model

Any car model can be made drivable when it:

- faces **+Z** and stands on its origin (as glTF models do),
- has each wheel as a **separate node named `Wheel…`** (`WheelFL`, `Wheel_RearLeft`, …) with its mesh.

The Sandbox's Playground does this with the *Concept Car*: the front axle steers, the rear wheels have the hand brake, the body collides as the convex hulls of its biggest parts, and the engine is a sports car's (700 Nm, six gears, 0–100 km/h in about 4.4 s). The player can rig a model the same way from [`Player.json`](../building/player-config.md) with `"vehicle": "car"`.

To build a vehicle by hand: give the chassis a dynamic Rigid Body (about 1500 kg for a car, centre of mass lowered by 0.25 m), colliders for the body (not the wheels) and a Vehicle; then give each wheel entity a Vehicle Wheel and place it where the wheel hangs with the suspension fully out. The Inspector shows every setting (engine torque and RPM, gear ratios, shift points, differential, anti-roll, wheel radius and width, suspension travel, frequency and damping, steering angle, brakes, grip) and, while driving, the live speed, RPM, gear and per-wheel state.

## Characters

A **Character Controller** makes a capsule-shaped character that walks, runs, jumps, climbs stairs, slides off steep slopes, pushes crates, rides moving platforms and swims. It isn't a rigid body: it moves exactly as told, and collides like a solid object.

| Setting | | Setting | |
|---|---|---|---|
| `radius`, `height` | The capsule (it stands on the entity's origin) | `speed`, `runFactor` | Walking speed (m/s), and how much faster running is |
| `maxSlope` | Steepest walkable slope (degrees) | `jumpSpeed` | Upward speed of a jump (m/s) |
| `stepHeight` | Tallest step it climbs | `gravityFactor`, `airControl` | Gravity, and how much it steers in the air |
| `stickDown` | How far it sticks to the ground going down | `turnToMove`, `turnSpeed` | Turn to face where it walks, how fast |
| `mass`, `strength` | For pushing bodies | `layer` | Its collision layer |

### Driving it from code

| Field | |
|---|---|
| `move` | The direction to walk, in world space, length up to 1 (`vec3(0, 0, 1)` = forward along +Z at full speed) |
| `run` | Run while true |
| `jump` | Jump (taken at the next physics step; set it on the frame the key goes down) |

```luau
local character = self.entity:Get("CharacterController")
character.move = vec3(x, 0, z)   -- from the keys, relative to the camera
character.run = input.GetKey("shift")
if input.GetKeyDown("space") then character.jump = true end
```

`physics.GetCharacterInfo(entity)` returns whether it's `grounded`, on steep ground, its velocity and what it stands on.

**Create > Physics > Character** makes one (a capsule with a visor on its front) with the [`CharacterDriver`](../scripting/api/sandbox-scripts.md#characterdriver) script and a chase camera. **Character Course** adds stairs, a step, slopes and crates to try it on.

An animated model below a character can drive it with [root motion](animation.md#root-motion): the animation's walk becomes the character's.

## Water

A **Water Volume** is a box of water: its top face is the surface. Dynamic bodies in it float or sink by their density, are slowed by drag, and drift with the flow; characters swim.

| Setting | | Setting | |
|---|---|---|---|
| `size` | The box (centred on the entity) | `flow` | Current (m/s) |
| `density` | 1000 (water) | `waveHeight`, `waveLength`, `waveSpeed` | Waves on the surface |
| `linearDrag`, `angularDrag` | Water resistance | `rippleSpeed` | How fast the surface's normal map scrolls |
| `damping` | Settles floating bodies | | |

A body floats when its density is lower than the water's: a 0.5 m box of 500 kg/m³ floats half under. Each rigid body's `buoyancy` scales how much water holds it up. `physics.WaterAt(point)` tells a script the surface height and normal at a point.

**Create > Physics > Water** makes a water volume with a see-through surface; **Water Pool** a pool with crates, a log, a raft, a steel ball and a ramp to drive into.

## Ragdolls

A **Ragdoll** on an animated character's root (the entity with the Animator) gives it physics bodies on its main bones (hips, spine, head, arms, legs), joined with realistic limits.

| Mode | |
|---|---|
| **Animated** | The bodies follow the animation exactly. They push things; nothing pushes them |
| **Powered** | The bodies chase the animation with motors: it dances like the animation, but can be knocked about |
| **Limp** | No animation: it falls and tumbles |

| Setting | |
|---|---|
| `mass`, `strength` | Total mass (80 kg), and how hard Powered motors pull |
| `limpOnImpact` | A hit faster than this (m/s) knocks it out: limp until its mode is set again (0 = never) |
| `knockback`, `knockbackLift` | How hard a hit throws it, and how much upward |
| `friction`, `jointLimitScale`, `layer` | |

From a script: `physics.SetRagdollMode(entity, "Limp")`, and `physics.AddRagdollImpulse(entity, impulse)` to hit it. **Create > Physics > Ragdoll (dancing)** makes a powered dancer; the Playground's dancers get thrown by the car.

The bones are picked by name (Mixamo's names and common ones), so ragdolls work with most humanoid models.

## Soft bodies

A **Soft Body** on an entity with a mesh turns the mesh into cloth or jelly: its vertices become particles held together by springs.

| Setting | |
|---|---|
| `mass` | The whole body's mass |
| `stiffness`, `bendStiffness` | 0 to 1: floppy to stiff, and how much it resists folding |
| `pressure` | Pumps up a closed mesh (balloons, jelly) |
| `pin` | Holds one side still: *Top*, *Left*, *Corners*, *TopCorners*, … (a flag on a pole, a curtain) |
| `wind`, `gusts` | Wind (m/s) and how much it varies |
| `friction`, `restitution`, `damping`, `gravityFactor`, `iterations`, `thickness`, `layer` | |

Soft bodies collide with rigid bodies and characters (vehicles' wheels pass through them). **Create > Physics > Soft Body** has a **Flag**, a **Cloth Drape**, a **Jelly Cube** and a **Balloon**.
