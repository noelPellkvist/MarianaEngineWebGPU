---
title: Component Reference
description: Every component scripts can reach, with each field's type, default value and meaning.
---

# Component Reference

Every component below can be used from scripts with `entity:Get(name)`, `entity:Add(name, fields)`, `entity:Has(name)` and `entity:Remove(name)`, and its fields read and written by name. See [Components](components.md) for how fields work.

Types: `number`, `boolean`, `string`, `vector` (a [vec3](vec3.md); two-number fields use x and y), [`quat`](quat.md), or a string choosing from the listed names.

## Transform and rendering

### `LocalTRS`

The transform (position, rotation, scale relative to the parent). Prefer the [Entity](entity.md) properties `position`, `rotation` and `scale`, which do the same with nicer types.

| Field | Type | Default | |
|---|---|---|---|
| `position` | vector | 0, 0, 0 | |
| `rotation` | table of 4 numbers | 0, 0, 0, 1 | x, y, z, w |
| `scale` | vector | 1, 1, 1 | |

### `CameraComponent`

Makes the entity a camera, looking along its +Z. The game shows the scene through the first camera.

| Field | Type | Default | |
|---|---|---|---|
| `fov` | number | 60 | Vertical field of view, degrees |
| `near` | number | 0.1 | Near plane, metres |
| `far` | number | 500 | Far plane, metres |

### `MeshComponent`

Draws a mesh with its materials (the Inspector's *Mesh Renderer*). Its mesh and materials are assets, which scripts can't change yet; `Has("MeshComponent")` tells you whether an entity draws something.

### `ShadowCasterTag`

A tag (no fields): the mesh casts shadows. `entity:Add("ShadowCasterTag")` / `entity:Remove("ShadowCasterTag")` turns its shadow on and off.

## Animation

### `Animator`

Plays a model's clips or an animation graph. It also has [methods](animator.md).

| Field | Type | Default | |
|---|---|---|---|
| `speed` | number | 1 | Playback speed |
| `playing` | boolean | true | |
| `rootMotion` | boolean | false | Clips move the entity (or its character) instead of the hips |
| `rootBone` | string | "" | The root motion bone; empty = found automatically (the hips) |
| `clip` | number | | The clip playing, by index from 0 (without a graph) |
| `loop` | boolean | | Whether that clip loops |
| `time` | number | | Its time in seconds |
| `states` | table of strings | | Each state machine's state, where it isn't the entry state. Setting it moves them there |

### `AnimationTargetEntity`

Marks a model's node (bone) entity for its Animator.

| Field | Type | |
|---|---|---|
| `node` | number | The node's index in its glTF file |

### `SkinComponent`

A skinned mesh's skin (an asset; no fields scripts can change).

## Physics bodies

### `RigidBody`

| Field | Type | Default | |
|---|---|---|---|
| `motion` | `"Static"`, `"Kinematic"`, `"Dynamic"` | `"Dynamic"` | How it moves |
| `mass` | number | 1 | kg; 0 = the colliders' volume times their density |
| `linearDamping` | number | 0.05 | |
| `angularDamping` | number | 0.05 | |
| `gravityFactor` | number | 1 | |
| `continuous` | boolean | false | Sweep fast bodies so they don't tunnel |
| `allowSleeping` | boolean | true | |
| `lockedAxes` | number | 0 | Bits: 1, 2, 4 = position X, Y, Z; 8, 16, 32 = rotation X, Y, Z |
| `layer` | number | 0 | Collision layer (0–31) |
| `startVelocity` | vector | 0, 0, 0 | m/s, world, when the body is made |
| `startAngularVelocity` | vector | 0, 0, 0 | rad/s, world |
| `centerOfMass` | vector | 0, 0, 0 | Offset of the centre of mass |
| `interpolate` | boolean | true | Drawn smoothly between physics steps |
| `buoyancy` | number | 1 | How much water holds it up; 0 = not at all |

```luau
-- Lock a body to the XY plane (a 2D game):
body.lockedAxes = 4 + 8 + 16   -- position Z, rotation X and Y
```

### Colliders

Every collider has these fields:

| Field | Type | Default | |
|---|---|---|---|
| `center` | vector | 0, 0, 0 | Offset on the entity |
| `rotation` | quat | identity | |
| `friction` | number | 0.5 | |
| `restitution` | number | 0 | Bounciness, 0–1 |
| `density` | number | 1000 | kg/m³, for bodies with mass 0 |
| `trigger` | boolean | false | Reports what enters instead of colliding |

And by shape:

| Component | Field | Type | Default | |
|---|---|---|---|---|
| `BoxCollider` | `size` | vector | 1, 1, 1 | Full size |
| `SphereCollider` | `radius` | number | 0.5 | |
| `CapsuleCollider` | `radius`, `height` | number | 0.5, 2 | Along local Y; height from tip to tip |
| `CylinderCollider` | `radius`, `height` | number | 0.5, 1 | Along local Y |
| `MeshCollider` | `convex` | boolean | false | Use the mesh's convex hull (its mesh is set in the editor) |

All collider sizes are scaled by the entity's scale.

### `Joint`

| Field | Type | Default | |
|---|---|---|---|
| `type` | `"Fixed"`, `"Point"`, `"Distance"`, `"Hinge"`, `"Slider"`, `"Cone"`, `"SwingTwist"`, `"SixDOF"` | `"Hinge"` | |
| `connected` | string | "" | The other body's entity, by name; empty = the world |
| `toParent` | boolean | false | Connect to the body above instead (chains) |
| `collideConnected` | boolean | false | Let the two bodies still collide |
| `limited` | boolean | false | Hinge and slider limits on |
| `minLimit`, `maxLimit` | number | -45, 45 | Degrees (hinge) or metres (slider) |
| `connectedAnchor` | vector | 0, 0, 0 | Distance joints: the other end, on the connected entity (or in the world) |
| `minDistance`, `maxDistance` | number | -1, -1 | Distance joints; below 0 = the distance at the start |
| `springFrequency`, `springDamping` | number | 0, 0.5 | A soft limit (Hz; 0 = hard) |
| `coneAngle` | number | 45 | Cone and swing twist half angle, degrees |
| `twistMin`, `twistMax` | number | -45, 45 | Swing twist range, degrees |
| `lockedAxes` | number | 63 | Six DOF: held axes (bits as RigidBody's, in the joint's axes) |
| `motor` | `"Off"`, `"Velocity"`, `"Position"` | `"Off"` | Hinge and slider motor |
| `motorSpeed` | number | 90 | deg/s or m/s |
| `motorTarget` | number | 0 | deg or m |
| `motorStrength` | number | 1000 | N m or N |
| `motorFrequency` | number | 2 | Position motor: how fast it gets there (Hz) |
| `friction` | number | 0 | When not driven (N m or N) |
| `breakForce`, `breakTorque` | number | 0, 0 | Breaks above these (N, N m); 0 = never |

```luau
-- Open a motorised door to 90 degrees:
local hinge = door:Get("Joint")
if hinge then
    hinge.motor = "Position"
    hinge.motorTarget = 90
end
```

## Vehicles

### `Vehicle`

On the chassis (an entity with a dynamic RigidBody).

| Field | Type | Default | |
|---|---|---|---|
| `type` | `"Wheeled"`, `"Motorcycle"`, `"Tracked"` | `"Wheeled"` | |
| `collision` | `"Ray"`, `"Sphere"`, `"Cylinder"` | `"Cylinder"` | How wheels test the ground |
| `drive` | `"Front"`, `"Rear"`, `"All"` | `"Rear"` | Driven wheels (wheeled) |
| `engineTorque` | number | 500 | N m |
| `minRPM`, `maxRPM` | number | 1000, 6000 | |
| `automatic` | boolean | true | |
| `shiftUpRPM`, `shiftDownRPM` | number | 4000, 2000 | |
| `clutchStrength` | number | 10 | |
| `gearRatios` | table of numbers | 2.66, 1.78, 1.3, 1.0, 0.74 | |
| `reverseRatio` | number | -2.9 | |
| `differentialRatio` | number | 3.42 | |
| `limitedSlipRatio` | number | 1.4 | 0 = open differential |
| `antiRollStiffness` | number | 1000 | N/m per axle; 0 = none |
| `maxPitchRollAngle` | number | 60 | Degrees it may tip before it's held up |
| `maxLeanAngle` | number | 45 | Motorcycles: lean into turns, degrees |
| `throttle` | number | 0 | **Input**, -1 to 1 |
| `steering` | number | 0 | **Input**, -1 (left) to 1 (right) |
| `brake` | number | 0 | **Input**, 0 to 1 |
| `handBrake` | number | 0 | **Input**, 0 to 1 |

### `VehicleWheel`

On each wheel entity below the chassis.

| Field | Type | Default | |
|---|---|---|---|
| `radius`, `width` | number | 0.35, 0.25 | Metres |
| `suspensionMin`, `suspensionMax` | number | 0.1, 0.3 | Travel, metres |
| `suspensionFrequency` | number | 1.5 | Spring, Hz |
| `suspensionDamping` | number | 0.5 | |
| `maxSteerAngle` | number | 0 | Degrees; 0 = doesn't steer |
| `casterAngle` | number | 0 | Degrees the steering axis leans back |
| `maxBrakeTorque` | number | 1500 | N m |
| `maxHandBrakeTorque` | number | 0 | N m |
| `grip` | number | 1 | Scales the tyre's friction |

## Characters

### `CharacterController`

| Field | Type | Default | |
|---|---|---|---|
| `radius` | number | 0.3 | Capsule radius |
| `height` | number | 1.8 | Feet to the top of the head |
| `maxSlope` | number | 50 | Degrees it can walk up |
| `stepHeight` | number | 0.35 | Steps it climbs |
| `stickDown` | number | 0.5 | Follows the ground down this far |
| `mass` | number | 70 | kg, for pushing and being pushed |
| `strength` | number | 200 | N it pushes with |
| `speed` | number | 4 | Walking speed, m/s |
| `runFactor` | number | 1.8 | Times faster when running |
| `jumpSpeed` | number | 5 | m/s up |
| `gravityFactor` | number | 1 | |
| `airControl` | number | 0.3 | 0 = keeps its velocity in the air, 1 = steers as on the ground |
| `turnToMove` | boolean | true | Turns to face where it walks |
| `turnSpeed` | number | 720 | Degrees per second |
| `layer` | number | 0 | |
| `move` | vector | 0, 0, 0 | **Input**: direction to walk, world space, length ≤ 1 |
| `run` | boolean | false | **Input** |
| `jump` | boolean | false | **Input**: jumps at the next step |

## Water

### `WaterVolume`

| Field | Type | Default | |
|---|---|---|---|
| `size` | vector | 10, 2, 10 | The box, centred on the entity; its top is the surface |
| `density` | number | 1000 | kg/m³ |
| `linearDrag`, `angularDrag` | number | 0.5, 0.05 | |
| `damping` | number | 1 | Calms what floats |
| `flow` | vector | 0, 0, 0 | Current, m/s, world |
| `waveHeight` | number | 0 | Metres |
| `waveLength` | number | 6 | Metres |
| `waveSpeed` | number | 1.5 | m/s |
| `rippleSpeed` | vector | 0.04, 0.025 | Normal map scroll per second (x, y) |

## Ragdolls

### `Ragdoll`

On an animated model's root (the entity with the Animator).

| Field | Type | Default | |
|---|---|---|---|
| `mode` | `"Animated"`, `"Powered"`, `"Limp"` | `"Powered"` | |
| `mass` | number | 80 | kg, shared by the bones |
| `strength` | number | 600 | N m per joint motor (Powered) |
| `limpOnImpact` | number | 4 | m/s; a harder hit knocks it limp; 0 = never |
| `knockback` | number | 0 | How hard a hit throws it (times the hitter's speed) |
| `knockbackLift` | number | 0.5 | Upward share of the throw |
| `friction` | number | 0.6 | |
| `jointLimitScale` | number | 1 | Widens (> 1) or narrows every joint's limits |
| `layer` | number | 0 | |

## Soft bodies

### `SoftBody`

On an entity with a mesh.

| Field | Type | Default | |
|---|---|---|---|
| `mass` | number | 1 | kg |
| `stiffness` | number | 0.9 | 0 = rubber band, 1 = doesn't stretch |
| `bendStiffness` | number | 0 | 0 = folds freely, 1 = keeps its shape |
| `pressure` | number | 0 | Closed meshes: 10 = a soft ball, 50 = firm |
| `friction` | number | 0.5 | |
| `restitution` | number | 0 | |
| `damping` | number | 0.1 | 1/s |
| `gravityFactor` | number | 1 | |
| `iterations` | number | 5 | Solver sub-steps per physics step |
| `thickness` | number | 0.02 | Metres it keeps off what it touches |
| `pin` | `"None"`, `"Top"`, `"Bottom"`, `"Left"`, `"Right"`, `"Front"`, `"Back"`, `"Corners"`, `"TopCorners"` | `"None"` | The side held still |
| `wind` | vector | 0, 0, 0 | m/s, world |
| `gusts` | number | 0.5 | 0 = steady, 1 = varies a lot |
| `layer` | number | 0 | |

## Scripts

### `Scripts`

The scripts on the entity. Its scripts are assets, set in the editor; from code use `entity:GetScript(name)` to reach one.
