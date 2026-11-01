---
title: Physics
description: Rigid bodies, colliders, triggers, collision layers and joints, powered by Jolt Physics.
---

# Physics

The engine simulates physics with [Jolt Physics](https://github.com/jrouwe/JoltPhysics), the engine behind *Horizon Forbidden West*. Physics runs while the game plays, in the editor and in built games, natively and on the web.

![Collider outlines and the Physics window's debug view while playing](images/physics-debug.jpg "Colliders are outlined on the selection; the Physics window can draw every body while the game runs.")

## Rigid bodies

A **Rigid Body** component makes an entity a physics body. Its **motion** decides how it moves:

| Motion | Moved by | Use it for |
|---|---|---|
| **Dynamic** | Physics: gravity, collisions, forces | Crates, balls, debris, anything that falls and tumbles |
| **Kinematic** | You: move the entity (from a script or an animation) and the body follows, pushing dynamic bodies out of the way | Moving platforms, doors you script, elevators |
| **Static** | Nothing: it stays where its entity is | Floors, walls, level geometry |

| Setting | Default | |
|---|---|---|
| **Mass** | 1 kg | 0 = worked out from the colliders' volume times their density |
| **Linear damping**, **Angular damping** | 0.05 | Air resistance for moving and turning |
| **Gravity factor** | 1 | 0 floats, 2 falls twice as fast |
| **Continuous** | Off | Sweeps fast bodies so they don't pass through thin ones (bullets, fast cars) |
| **Allow sleeping** | On | Bodies at rest stop simulating until something touches them (saves time) |
| **Locked axes** | None | Axes it can't move along or turn about (a 2D game locks Z movement and X and Y rotation) |
| **Layer** | 0 | Its collision layer (see [Layers](#collision-layers)) |
| **Start velocity**, **Start angular velocity** | 0 | Velocity it starts with when the game starts or the body is created (m/s; rad/s) |
| **Center of mass** | 0 | Moves the centre of mass (lower it for steadier vehicles) |
| **Interpolate** | On | Drawn smoothly between physics steps |
| **Buoyancy** | 1 | How strongly water holds it up (0 = sinks like a stone) |

Entities with colliders but no rigid body on or above them are **static** bodies.

**Create > Physics** has ready-made objects: dynamic cubes, spheres, capsules and cylinders, a static cube, a ground, and a box stack to knock over.

## Colliders

Colliders give a body its shape. Add them with **Add Component > Physics**:

| Collider | Shape | Main settings |
|---|---|---|
| **Box Collider** | A box | `size` (1 m) |
| **Sphere Collider** | A ball | `radius` (0.5 m) |
| **Capsule Collider** | A pill along local Y | `radius` (0.5 m), `height` (2 m, tip to tip) |
| **Cylinder Collider** | A cylinder along local Y | `radius` (0.5 m), `height` (1 m) |
| **Mesh Collider** | A mesh's triangles, or their convex hull | `convex`; the mesh (the entity's own by default) |

Every collider has a **center** and **rotation** (its place on the entity), **friction** (0.5), **restitution** (bounciness: 0 = none, 1 = keeps all its speed), **density** (1000 kg/m³, used when the body's mass is 0) and **trigger**. All are scaled with the entity.

Tips:

- When you add a collider from the Inspector, it's **fitted** to the entity's mesh (**Fit to mesh** does it again later). Drag the dots on the outline in the Scene view to resize a box, sphere, capsule or cylinder (Shift resizes both sides).
- A body's shape is its colliders **plus those of its children**, down to the next rigid body. Build complex shapes from several simple colliders on child entities.
- **Mesh colliders** are exact but slow, and only for things that don't move on their own (levels, static and kinematic bodies). On a dynamic body a mesh collider is used as its convex hull. For a whole level, **Add Component > Physics > Mesh Colliders on Children** puts one on every mesh below the selection at once.
- Prefer simple shapes: a capsule for characters, boxes for crates and walls, spheres for balls.

## Triggers

A collider with **trigger** on doesn't collide: it reports what enters and leaves it. Scripts on either side get `OnTriggerEnter(other)` and `OnTriggerExit(other)`. Triggers notice dynamic, kinematic and static bodies, and characters. A body resting in a trigger stays inside it even while it sleeps.

```luau
function Checkpoint.OnTriggerEnter(self: Script, other: Entity)
    if other.name == "Player" then
        print("Checkpoint!")
    end
end
```

## Contacts

When two bodies begin touching, scripts on both get `OnContactBegin(other, contact)`, with the contact's `point`, `normal` (pointing from this entity to the other) and the `speed` they hit at; `OnContactEnd(other)` when they part. See [Script Lifecycle](../scripting/lifecycle.md#physics-events).

## Collision layers

Every body is on a **layer** (0 to 31). The project's **layer matrix** says which layers collide. Set them up in **Window > Physics**: **+ Layer** adds a named layer, and the matrix's tick boxes say which pairs collide. **Save** writes them to the project's `Physics.json`.

Queries from scripts can be limited to layers too: `physics.Raycast(origin, direction, 100, { layers = 1 << 2 })` only hits layer 2.

## Joints

A **Joint** component ties the body at or above its entity to another body (or to the world):

| Type | Holds |
|---|---|
| **Fixed** | Both bodies together as one |
| **Point** | At one point, free to turn (a ball joint, a chain link) |
| **Distance** | Within a distance range (a rope, a spring) |
| **Hinge** | Turning about the joint's X axis only (a door, a wheel, a lever), with limits, a motor and friction |
| **Slider** | Sliding along the joint's X axis only (a piston, a drawer), with limits, a motor and friction |
| **Cone** | Turning within a cone (a shoulder) |
| **Swing Twist** | A cone plus a twist range (ragdoll joints) |
| **Six DOF** | Any axes locked, the rest free |

- The joint's **entity** places it: its position is the anchor, its X axis the hinge or slider axis. Put several joints on one body as child entities.
- **Connected** is the other body's entity, by name (empty = the world). **To parent** connects to the body above instead, for chains.
- **Motors** (Hinge, Slider) drive to a speed or a position with a strength.
- **Break force** and **break torque** make a joint snap when pulled too hard.

**Create > Physics** has joint examples: a chain, a door, a spinner and a wrecking ball.

## Simulation settings

**Window > Physics** (saved in `Physics.json`):

| Setting | Default | |
|---|---|---|
| **Gravity** | (0, -9.81, 0) | m/s² |
| **Steps per second** | 60 | The fixed physics rate |
| **Max steps per frame** | 4 | When a frame is slow, at most this many steps catch up |
| **Collision steps** | 1 | More = more accurate fast collisions, slower |

Physics runs at a fixed rate, independent of the frame rate, and bodies are drawn **interpolated** between steps so motion is smooth at any frame rate. The same scene always simulates the same way.

## Debugging

In **Window > Physics**, the debug view draws while the game runs: every body's shape (coloured by motion type, or awake and asleep), contacts, velocities, centres of mass and bounding boxes. **Show all colliders** outlines every collider in the Scene view, not just the selection's. While playing, the Inspector shows a body's live velocity, mass and whether it sleeps.

## From scripts

Scripts can cast rays, find what's in an area, push bodies and read their state: see [`physics`](../scripting/api/physics.md). They can also add and change rigid bodies and colliders like any component:

```luau
local box = scene.SpawnPrimitive("cube", vec3(0, 5, 0))
box:Add("BoxCollider")
box:Add("RigidBody", { motion = "Dynamic", mass = 20 })
```

## Known behaviour

- Resting bodies sink a few millimetres into what they lie on (up to 5 mm, Jolt's penetration slop): a 1 m ball rests at a height of 0.495.
- Two static bodies never collide with each other.
