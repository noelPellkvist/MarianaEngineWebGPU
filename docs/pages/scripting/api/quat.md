---
title: quat
description: Rotations in Mariana scripts: making quaternions from angles, axes and directions, combining them and turning vectors.
---

# quat

Rotations are **quaternions** (`quat`). You rarely need to know how they work: make them from angles or directions, combine them by multiplying, and turn vectors with them. All angles are in **degrees**.

```luau
self.entity.rotation = quat.euler(0, 90, 0)                 -- turned 90 degrees about Y
self.entity.rotation = quat.lookRotation(target - here)     -- facing a direction
local turned = quat.angleAxis(45, vec3(0, 1, 0)) * self.entity.rotation   -- 45 more about the world's Y
```

## Making rotations

### `quat.identity`

No rotation.

### `quat.euler(x, y, z)`
### `quat.euler(angles)` {#quat-euler-vector}

A rotation from angles in degrees about X, Y and Z (three numbers or one vector), the same angles the Inspector shows.

```luau
local tilted = quat.euler(30, 0, 0)        -- pitched 30 degrees
local facingLeft = quat.euler(vec3(0, -90, 0))
```

### `quat.angleAxis(degrees, axis)`

A rotation of `degrees` about `axis` (any length; a zero axis gives no rotation).

```luau
local spin = quat.angleAxis(90 * dt, vec3(0, 1, 0))
self.entity.rotation = spin * self.entity.rotation
```

### `quat.lookRotation(forward, up?)`

The rotation that turns **+Z** to point along `forward`, with **+Y** toward `up` (default `vec3(0, 1, 0)`). Use it to make an entity face a direction:

```luau
self.entity.rotation = quat.lookRotation(velocity)
```

A zero `forward` gives no rotation. Looking straight up or down (along `up`) picks another up so the result is defined.

### `quat.fromToRotation(from, to)`

The shortest rotation that turns direction `from` into direction `to`.

```luau
-- Tilt to match the ground's slope:
local tilt = quat.fromToRotation(vec3(0, 1, 0), groundNormal)
```

### `quat.slerp(a, b, t)`

The rotation `t` of the way from `a` to `b` (smoothly, along the shortest arc).

```luau
self.entity.rotation = quat.slerp(self.entity.rotation, goal, 1 - math.exp(-8 * dt))
```

### `quat.new(x, y, z, w)`

A quaternion from its four components (normalized). You'll rarely need it.

## Using rotations

| Expression | Result |
|---|---|
| `a * b` | `b` then `a`: the combined rotation |
| `q * v` | The vector `v` turned by `q` |
| `q:Inverse()` | The opposite rotation |
| `q:ToEuler()` | Angles in degrees about X, Y and Z, as a vector |
| `quat.angle(a, b)` | The angle between two rotations, in degrees |
| `q.x`, `q.y`, `q.z`, `q.w` | The components |
| `a == b` | Exactly equal |

### Order of multiplication

`a * b` applies `b` first, then `a`:

- `quat.angleAxis(10, vec3(0, 1, 0)) * rotation` turns **about the world's** Y.
- `rotation * quat.angleAxis(10, vec3(0, 1, 0))` turns **about the entity's own** Y (the same as `entity:Rotate(vec3(0, 1, 0), 10)`).

### Directions from a rotation

```luau
local forward = rotation * vec3(0, 0, 1)
local right = rotation * vec3(1, 0, 0)
local up = rotation * vec3(0, 1, 0)
```

For an entity, `entity.forward`, `entity.right` and `entity.up` give these directly, in world space.

## Recipes

### Turn smoothly toward a target

```luau
local direction = target.worldPosition - self.entity.worldPosition
direction = vec3(direction.x, 0, direction.z)        -- only turn about Y
if vector.magnitude(direction) > 0.01 then
    local goal = quat.lookRotation(direction)
    self.entity.worldRotation = quat.slerp(self.entity.worldRotation, goal, 1 - math.exp(-5 * dt))
end
```

### Yaw (heading) of an entity, in degrees

```luau
local f = self.entity.forward
local yaw = math.deg(math.atan2(f.x, f.z))   -- 0 = facing +Z, 90 = facing +X
```

### Limit how far something turns

```luau
if quat.angle(current, goal) > 45 then
    goal = quat.slerp(current, goal, 45 / quat.angle(current, goal))
end
```
