---
title: physics
description: The physics library in Mariana scripts: raycasts, shape casts and overlaps, forces, impulses and velocities, teleporting, contacts, vehicle, character and ragdoll state, and water.
---

# physics

The `physics` global asks the physics simulation questions (what does this ray hit? what is inside this sphere?) and pushes its bodies around.

```luau
-- Is there ground under me?
local hit = physics.Raycast(self.entity.worldPosition, vec3(0, -1, 0), 1.2, { ignore = self.entity })
if hit then
    print("standing on", hit.entity.name, hit.distance, "m below")
end

-- Kick a ball
physics.AddImpulse(ball, vec3(0, 3, 8))
```

Physics runs in **fixed steps** of 1/60 s after every script's `Update` (see [the frame](../lifecycle.md#the-frame-step-by-step)). Queries see the bodies **as the last step left them**: a body you create in `Update` exists from this frame's step on, so queries find it in `LateUpdate` or next frame.

## Which entity is a body

Results and events name the entity that **has the body**:

- the entity with the `RigidBody` (its colliders can be on it or on children below it: they are all one body),
- or, for colliders with no rigid body above them, the collider's own entity (a static body),
- a character's entity (the one with the `CharacterController`),
- for a ragdoll, the **bone** that was hit (its `Ragdoll` component is on the model's root above it).

Put scripts that handle contacts on the entity with the body, and pass that entity to the functions below.

## Queries

### `physics.Raycast(origin, direction, maxDistance?, options?) -> RaycastHit?` {#raycast}

The first body along a ray from `origin` in `direction` (any length; only its direction counts), up to `maxDistance` metres (default 1000). Returns a [`RaycastHit`](types.md#raycasthit) or `nil`.

| Field | |
|---|---|
| `entity` | The [body's entity](#which-entity-is-a-body) |
| `point` | Where the ray hit, in the world |
| `normal` | The surface's direction at that point (pointing out of it) |
| `distance` | Metres from `origin` |

```luau title="What is the camera looking at?"
local camera = self.entity
local hit = physics.Raycast(camera.worldPosition, camera.forward, 50)
if hit then
    print("looking at", hit.entity.name)
end
```

A ray that starts inside a solid body hits it at distance 0, so a ray cast from inside your own body needs `{ ignore = self.entity }` (see [options](#query-options)).

### `physics.RaycastAll(origin, direction, maxDistance?, options?) -> { RaycastHit }` {#raycastall}

Every body along the ray, nearest first, each once. An empty table when nothing is hit.

```luau
for _, hit in physics.RaycastAll(gun.worldPosition, gun.forward, 100) do
    print(hit.entity.name, hit.distance)      -- everything a piercing shot passes through
end
```

### `physics.SphereCast(origin, radius, direction, maxDistance?, options?) -> RaycastHit?` {#spherecast}

Sweeps a sphere of `radius` from `origin` along `direction` and returns the first body it would touch: a "thick" ray, for a ball rolling or a character fitting through a gap. Bodies the sphere already overlaps at `origin` are hit at distance 0.

```luau
-- Will a 0.4 m ball fit through?
local blocked = physics.SphereCast(start, 0.4, goal - start, vector.magnitude(goal - start))
```

### `physics.OverlapSphere(center, radius, options?) -> { Entity }` {#overlapsphere}

The bodies inside or touching a sphere, each once, in creation order.

```luau title="Everything within 5 m"
for _, entity in physics.OverlapSphere(self.entity.worldPosition, 5) do
    print(entity.name)
end
```

### `physics.OverlapBox(center, halfExtents, rotation?, options?) -> { Entity }` {#overlapbox}

The bodies in a box. `halfExtents` is half its size on each axis (`vec3(1, 1, 1)` is a 2 m box), turned by `rotation`.

```luau
local inside = physics.OverlapBox(zone.worldPosition, vec3(4, 2, 4), zone.worldRotation)
```

### Query options {#query-options}

Every query takes an optional table of options, a [`QueryOptions`](types.md#queryoptions):

| Option | Default | |
|---|---|---|
| `ignore` | none | One entity's body to leave out: usually your own (`{ ignore = self.entity }`) |
| `triggers` | `false` | Whether trigger colliders count |
| `layers` | every layer | A bit mask of the [collision layers](../../manual/physics.md#collision-layers) to include: layer *n* is `bit32.lshift(1, n)` |

```luau
-- Only layers 0 and 2, including triggers, not myself
local options = { ignore = self.entity, triggers = true, layers = bit32.bor(1, bit32.lshift(1, 2)) }
local hit = physics.Raycast(origin, direction, 30, options)
```

## Forces and velocities

Forces and impulses act on **dynamic** bodies (a `RigidBody` with `motion = "Dynamic"`) and return `false` (doing nothing) for anything else. Forces are in newtons, impulses in newton-seconds, points in world coordinates. Calls made before the body exists (in the `Update` that created it) are kept for its first step.

### `physics.AddForce(entity, force) -> boolean` {#addforce}

Pushes the body through every physics step of this frame. A steady push (a thruster, wind, a magnet) is a force **added every frame**.

```luau
-- A jetpack: 1.2 times its weight upward while Space is held
if input.GetKey("space") then
    local info = physics.GetBodyInfo(self.entity)
    if info then
        physics.AddForce(self.entity, vec3(0, info.mass * 9.81 * 1.2, 0))
    end
end
```

### `physics.AddForceAtPoint(entity, force, point) -> boolean` {#addforceatpoint}

A force at a point of the body: pushing off its centre also turns it.

### `physics.AddTorque(entity, torque) -> boolean` {#addtorque}

Turns the body about the axis `torque` points along (newton-metres), every step of this frame.

### `physics.AddImpulse(entity, impulse) -> boolean` {#addimpulse}

Changes the body's velocity at once by `impulse / mass`: a kick, a hit, an explosion, a jump. Call it once, not every frame.

```luau
physics.AddImpulse(ball, vec3(0, 0, 10))     -- a 2 kg ball now moves 5 m/s faster along +Z
```

### `physics.AddImpulseAtPoint(entity, impulse, point) -> boolean` {#addimpulseatpoint}

An impulse at a point: it also sets the body spinning.

### `physics.AddAngularImpulse(entity, impulse) -> boolean` {#addangularimpulse}

Sets the body spinning at once about the axis `impulse` points along.

### `physics.SetVelocity(entity, linear, angular?) -> boolean` {#setvelocity}

Sets the body's velocity (m/s) and, if given, its angular velocity (radians per second about each axis; otherwise it keeps spinning as it was).

```luau
physics.SetVelocity(self.entity, vec3(0, 0, 0), vec3(0, 0, 0))     -- stop dead
```

### `physics.GetVelocity(entity) -> (vector?, vector?)` {#getvelocity}

The body's velocity and angular velocity (rad/s), or `nil` when it has no body. Works for kinematic bodies too.

```luau
local velocity = physics.GetVelocity(car)
if velocity then
    print(string.format("%.0f km/h", vector.magnitude(velocity) * 3.6))
end
```

### `physics.Teleport(entity, position, rotation?) -> boolean` {#teleport}

Puts the entity and its body at a world position (and rotation, else it keeps its own) **at once**, so queries see it there straight away. Works for any body, and for entities without one. Velocity is kept: stop it with `SetVelocity` if it should arrive at rest.

```luau title="Respawn"
physics.Teleport(self.entity, self.spawnPoint, quat.identity)
physics.SetVelocity(self.entity, vec3(0, 0, 0), vec3(0, 0, 0))
```

> [!TIP]
> Setting `entity.position` on a dynamic body also moves its body (on the next step). `Teleport` is for when queries or contacts must see the new place this frame.

### `physics.WakeUp(entity) -> boolean` {#wakeup}

Bodies at rest fall asleep (they stop being simulated until something touches them). Forces and impulses wake them; call `WakeUp` after changing the world around a sleeping body (gravity, a removed floor) so it notices.

## Contacts

Touching bodies call the scripts on both of them: `OnContactBegin(self, other, contact)` and `OnContactEnd(self, other)`, or `OnTriggerEnter` / `OnTriggerExit` for triggers. See [Physics events](../lifecycle.md#physics-events). The `contact` is a [`Contact`](types.md#contact): `point`, `normal` (pointing from you toward `other`) and `speed` (how fast they came together, m/s).

### `physics.IsTouching(a, b) -> boolean` {#istouching}

Whether two bodies are touching now (as of the last step).

```luau
if physics.IsTouching(self.entity, scene.Find("Goal")) then
    win()
end
```

## Bodies, vehicles and characters

### `physics.GetBodyInfo(entity) -> BodyInfo?` {#getbodyinfo}

What the simulation has for the entity's body, or `nil`: `motion` (`"Static"`, `"Kinematic"` or `"Dynamic"`), `sleeping`, `sensor` (a trigger), `mass` (kg), `linearVelocity`, `angularVelocity`. See [`BodyInfo`](types.md#bodyinfo).

### `physics.GetVehicleInfo(entity) -> VehicleInfo?` {#getvehicleinfo}

A running vehicle's state (on its entity with the `Vehicle`), or `nil`:

| Field | |
|---|---|
| `speed` | m/s along its forward (negative backing up). `* 3.6` for km/h |
| `rpm` | Engine revolutions per minute |
| `gear` | -1 reverse, 0 neutral, 1 and up forward |
| `wheels` | Per wheel: `contact`, `suspension` (0 fully out .. 1 fully compressed), `steerAngle` (degrees), `rpm`, `longitudinalSlip`, `lateralSlip` (degrees) |

```luau title="A speedometer"
local info = physics.GetVehicleInfo(car)
if info then
    print(string.format("%3.0f km/h  gear %d  %4.0f rpm", info.speed * 3.6, info.gear, info.rpm))
end
```

Drive a vehicle by setting its `Vehicle` component's `throttle`, `steering`, `brake` and `handBrake` every frame: see [Vehicles](../../manual/gameplay-physics.md#driving-from-code) and the Sandbox's [VehicleDriver](sandbox-scripts.md#vehicledriver).

### `physics.GetCharacterInfo(entity) -> CharacterInfo?` {#getcharacterinfo}

A character controller's state, or `nil`:

| Field | |
|---|---|
| `grounded` | Standing on something it can walk on |
| `onSteepGround` | Touching ground too steep to stand on (it slides) |
| `velocity` | m/s, in the world |
| `groundNormal` | The ground's direction under it |
| `ground` | The entity it stands on, or `nil` |

Walk a character by setting its `CharacterController`'s `move`, `run` and `jump`: see [Characters](../../manual/gameplay-physics.md#driving-it-from-code) and [CharacterDriver](sandbox-scripts.md#characterdriver).

## Ragdolls

### `physics.SetRagdollMode(entity, mode)` {#setragdollmode}

Switches a ragdoll (on the model's root, the entity with the `Ragdoll`) between:

| Mode | |
|---|---|
| `"Animated"` | Its bodies follow the animation exactly; they push things, nothing pushes them |
| `"Powered"` | Its joints' motors pull it toward the animation, so it moves like the animation and can be knocked about |
| `"Limp"` | No drive: it falls like a rag doll |

Setting a mode also brings back one that was knocked out (limp after a hard hit).

### `physics.AddRagdollImpulse(entity, impulse) -> boolean` {#addragdollimpulse}

Pushes one bone (a bone entity, as contacts and overlaps report them), or the hips when given the model's root.

```luau title="A shove that knocks it over"
for _, bone in physics.OverlapSphere(self.entity.worldPosition + self.entity.forward, 1) do
    -- find the model's root above the bone
    local root: Entity? = bone
    while root and not root:Has("Ragdoll") do
        root = root.parent
    end
    if root then
        physics.SetRagdollMode(root, "Limp")
        physics.AddRagdollImpulse(bone, self.entity.forward * 60)
        break
    end
end
```

The whole recipe is in the cookbook: [Knocking down ragdolls](../cookbook.md#knocking-down-ragdolls).

## Water

### `physics.WaterAt(point) -> (vector?, vector?)` {#waterat}

When `point` is inside a [water volume](../../manual/gameplay-physics.md#water), the point on the surface straight above (or below) it, with waves, and the surface's normal. `nil` when it is in no water.

```luau title="Bob a buoy on the waves"
local surface, normal = physics.WaterAt(self.entity.worldPosition)
if surface and normal then
    self.entity.worldPosition = surface
    self.entity.worldRotation = quat.fromToRotation(vec3(0, 1, 0), normal)
end
```

Dynamic bodies float by themselves (their `RigidBody`'s `buoyancy`); `WaterAt` is for things that aren't bodies, effects and game rules ("is the player swimming?").
