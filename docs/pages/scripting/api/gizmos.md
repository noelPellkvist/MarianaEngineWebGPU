---
title: gizmos
description: Debug drawing from Mariana scripts: lines, spheres, boxes and text over the game in the editor's views.
---

# gizmos

The `gizmos` global draws shapes over the game, to see what a script is doing: a ray, a range, a target, a value. They show in the editor's **Scene** and **Game** windows while the game plays (over everything, not hidden by what's in front). Built games don't draw them, so they can stay in a script.

```luau
function Turret.Update(self: Script, dt: number)
    gizmos.Sphere(self.entity.worldPosition, self.range, vec3(1, 0.5, 0))   -- its range
    if self.target then
        gizmos.Line(self.entity.worldPosition, self.target.worldPosition, vec3(1, 0, 0))
    end
end
```

| Function | Draws |
|---|---|
| `gizmos.Line(from, to, color?, seconds?)` | A line |
| `gizmos.Sphere(center, radius, color?, seconds?)` | A sphere, as three circles |
| `gizmos.Box(center, size, rotation?, color?, seconds?)` | A box: `size` is its whole width, height and depth, turned by `rotation` (a [quat](quat.md)) |
| `gizmos.Text(position, text, color?, seconds?)` | Text at a point of the world |

- **`color`** is a vector of red, green, blue from 0 to 1 (`vec3(1, 0, 0)` is red); white when left out.
- **`seconds`**: how long it stays, in game time. Left out, a shape lasts the frame it was drawn in, so draw it every `Update` for as long as it should show. With seconds it stays that long (an impact point, a path taken).
- Drawing while paused keeps the last frame's shapes on screen until the game moves on.

```luau
-- Mark where a ray hit for two seconds
local hit = physics.Raycast(origin, direction, 50)
if hit then
    gizmos.Sphere(hit.point, 0.1, vec3(1, 1, 0), 2)
    gizmos.Line(hit.point, hit.point + hit.normal, vec3(0, 1, 1), 2)
end
```

For physics' own view (every collider, contacts, velocities), use the [Physics window's debug view](../../manual/physics.md) instead.
