---
title: vec3 and vector
description: Making and using 3D vectors in Mariana scripts: arithmetic, the vector library, and common recipes.
---

# vec3 and vector

Positions, directions, velocities, scales and colours are **vectors**: Luau's built-in `vector` type, three numbers `x`, `y`, `z`. Vectors are values (like numbers): copying one copies it, and they compare by value.

## Making vectors

### `vec3(x?, y?, z?)`

```luau
local up = vec3(0, 1, 0)
local zero = vec3()           -- (0, 0, 0): missing parts are 0
local flat = vec3(3, 0, 4)
```

Returns a `vector`. Scripts compile `vec3(...)` straight into a vector, so it's as fast as writing the numbers.

## Reading components

```luau
local p = self.entity.position
print(p.x, p.y, p.z)
```

Vectors can't be changed in place (`p.y = 2` is an error); make a new one: `p = vec3(p.x, 2, p.z)`.

## Arithmetic

| Expression | Result |
|---|---|
| `a + b`, `a - b` | Component-wise sum and difference |
| `a * b`, `a / b` | Component-wise product and quotient |
| `v * n`, `n * v`, `v / n` | Scaled by a number |
| `-v` | Negated |
| `a == b`, `a ~= b` | All components equal / not |
| `q * v` | `v` rotated by the quaternion `q` (see [`quat`](quat.md)) |

```luau
local velocity = direction * speed
self.entity.position += velocity * dt
local midpoint = (a + b) / 2
```

## The vector library

Luau's `vector` library works on vectors:

| Function | Returns |
|---|---|
| `vector.magnitude(v)` | The length |
| `vector.normalize(v)` | `v` with length 1 (the zero vector stays zero) |
| `vector.dot(a, b)` | The dot product: `|a| |b| cos(angle)` |
| `vector.cross(a, b)` | The cross product: perpendicular to both |
| `vector.angle(a, b, axis?)` | The angle between them, in **radians** (signed about `axis` when given) |
| `vector.lerp(a, b, t)` | The point `t` of the way from `a` to `b` |
| `vector.min(a, b, ...)`, `vector.max(a, b, ...)` | Component-wise minimum, maximum |
| `vector.clamp(v, min, max)` | Each component clamped between `min`'s and `max`'s |
| `vector.floor(v)`, `vector.ceil(v)`, `vector.abs(v)`, `vector.sign(v)` | Component-wise |
| `vector.create(x, y, z)` | The same as `vec3` |
| `vector.zero`, `vector.one` | `(0, 0, 0)`, `(1, 1, 1)` |

## Recipes

### Distance between two things

```luau
local distance = vector.magnitude(target.worldPosition - self.entity.worldPosition)
```

### Direction toward something

```luau
local toTarget = target.worldPosition - self.entity.worldPosition
local direction = vector.normalize(toTarget)
```

### Is it in front or behind?

```luau
local inFront = vector.dot(self.entity.forward, toTarget) > 0
```

### Flatten onto the ground

```luau
local flat = vec3(v.x, 0, v.z)
```

### Move toward a point at a fixed speed

```luau
local function moveTowards(from: vector, to: vector, maxStep: number): vector
    local offset = to - from
    local distance = vector.magnitude(offset)
    if distance <= maxStep then
        return to
    end
    return from + offset / distance * maxStep
end

self.entity.position = moveTowards(self.entity.position, goal, self.speed * dt)
```

### Smooth follow, independent of frame rate

```luau
local t = 1 - math.exp(-stiffness * dt)
self.entity.position = vector.lerp(self.entity.position, goal, t)
```

### Angle between directions, in degrees

```luau
local degrees = math.deg(vector.angle(a, b))
```

## Colours

Colours are vectors too: red, green and blue from 0 to 1 (`vec3(1, 0.5, 0)` is orange), as used by [`scene.SpawnPrimitive`](scene.md#spawnprimitive).
