---
title: screen
description: The game view in Mariana scripts: rays from the camera through the mouse for picking, points of the world on the screen, and locking or hiding the cursor.
---

# screen

The `screen` global is the game's view: what the player sees, through the scene's first camera. Use it to find what the mouse points at, or where something in the world is on the screen.

```luau
-- What's under the mouse?
local origin, direction = screen.Ray()
if origin and direction then
    local hit = physics.Raycast(origin, direction, 100)
    if hit and input.GetMouseButtonDown(0) then
        print(`Clicked {hit.entity.name}`)
    end
end
```

| Member | Type | |
|---|---|---|
| `screen.width`, `screen.height` | number | The view's size in pixels |
| `screen.Ray(point?)` | `(vector?, vector?)` | The ray from the camera through a point of the view: its origin (the camera) and direction (length 1) |
| `screen.WorldToScreen(point)` | `vector?` | Where a point of the world is in the view: `x`, `y` in pixels, `z` its distance from the camera |

### `screen.Ray(point?) -> (vector?, vector?)` {#ray}

`point` is in the view's pixels from its **top-left** corner (`x` right, `y` down; `z` is ignored). Without it, the ray goes through the mouse. Pass the result to [`physics.Raycast`](physics.md) to find what's there.

Both are `nil` while nothing shows the game: there's no camera entity in the scene, or (in the editor) the Game window hasn't been shown yet. Check before using them, as above.

### `screen.WorldToScreen(point) -> vector?` {#worldtoscreen}

`nil` when the point is behind the camera. A point off the sides of the view gives coordinates outside `0..width` and `0..height`.

```luau
-- Is the enemy on screen?
local spot = screen.WorldToScreen(enemy.worldPosition)
local visible = spot ~= nil and spot.x >= 0 and spot.x <= screen.width and spot.y >= 0 and spot.y <= screen.height
```

## The mouse in the view

[`input.GetMousePosition()`](input.md#mouse) is in the same pixels: from the game view's top-left corner. In the editor that's the Game window's image, so a click in the middle of the Game window is `(screen.width / 2, screen.height / 2)` however the editor is laid out; in a built game it's the window.

## Locking and hiding the cursor {#cursor}

| Function | |
|---|---|
| `input.SetCursorLocked(on)` | Hides the cursor and holds it in place; [`input.GetMouseDelta()`](input.md#mouse) still reports how the mouse moves. For mouse look (first-person cameras) |
| `input.SetCursorHidden(on)` | Hides the cursor but leaves it free (a game drawing its own pointer) |

```luau
function Look.Start(self: Script)
    input.SetCursorLocked(true)
end

function Look.Update(self: Script, dt: number)
    local delta = input.GetMouseDelta()
    self.yaw += delta.x * 0.15
    self.pitch = math.clamp(self.pitch - delta.y * 0.15, -85, 85)
    self.entity.rotation = quat.euler(self.pitch, self.yaw, 0)
end
```

In the editor the cursor is the game's only while it plays and has the keys: **Escape** gives it back (call `SetCursorLocked(true)` again, on a click for instance, to take it), and Pause and Stop release it. In a built game it stays as the game sets it; on the web the browser releases a locked pointer on Escape.

## In the editor

The Game window is the view: its image's size is `screen.width` and `screen.height`, and its camera is the scene's first camera entity. Picking works while you play there. The Scene window doesn't count (its camera is the editor's, not the game's).
