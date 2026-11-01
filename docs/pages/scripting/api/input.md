---
title: input
description: Keyboard and mouse input in Mariana scripts, and the full list of key names.
---

# input

The `input` global reads the keyboard and mouse. Keys are given **by name**, as strings.

```luau
if input.GetKey("w") then                -- held
    self.entity:Translate(vec3(0, 0, 5 * dt))
end
if input.GetKeyDown("space") then        -- pressed this frame
    jump()
end
```

## Keyboard

| Function | `true` when |
|---|---|
| `input.GetKey(name)` | The key is held down |
| `input.GetKeyDown(name)` | The key went down this frame |
| `input.GetKeyUp(name)` | The key went up this frame |

`GetKeyDown` and `GetKeyUp` are true for exactly one frame per press, so use them for actions (jump, shoot, open a menu) and `GetKey` for things that last while the key is held (move, accelerate).

Key names don't care about case (`"W"` and `"w"` are the same key). A name that isn't a key is an error: `there is no key named 'spacebar'`.

### Key names {#key-names}

| Keys | Names |
|---|---|
| Letters | `"a"` … `"z"` |
| Digits (the row above the letters) | `"0"` … `"9"` |
| Function keys | `"f1"` … `"f12"` |
| Arrows | `"up"`, `"down"`, `"left"`, `"right"` |
| Space, Enter, Escape, Tab, Backspace | `"space"`, `"enter"` (or `"return"`; the keypad's Enter counts too), `"escape"`, `"tab"`, `"backspace"` |
| Editing | `"insert"`, `"delete"`, `"home"`, `"end"`, `"pageup"`, `"pagedown"` |
| Shift | `"shift"` (either), `"leftshift"`, `"rightshift"` |
| Control | `"ctrl"` or `"control"` (either), `"leftcontrol"`, `"rightcontrol"` |
| Alt | `"alt"` (either), `"leftalt"`, `"rightalt"` |
| Punctuation | `"minus"`, `"equal"`, `"comma"`, `"period"`, `"slash"`, `"backslash"`, `"semicolon"`, `"apostrophe"`, `"grave"` (the key left of 1), `"leftbracket"`, `"rightbracket"` |

## Mouse

| Function | Returns |
|---|---|
| `input.GetMouseButton(button?)` | `true` while the button is held |
| `input.GetMouseButtonDown(button?)` | `true` the frame it went down |
| `input.GetMouseButtonUp(button?)` | `true` the frame it went up |
| `input.GetMousePosition()` | The pointer's position in pixels from the window's top-left corner, as a vector (`x`, `y`; `z` is 0) |
| `input.GetMouseDelta()` | How far the pointer moved since the last frame, in pixels |
| `input.GetScroll()` | The wheel this frame: `y` up/down (positive = away from you), `x` sideways |

Buttons are numbers: **0** left (the default), **1** right, **2** middle, 3 and 4 the side buttons.

```luau title="Orbit with the mouse, zoom with the wheel"
local delta = input.GetMouseDelta()
if input.GetMouseButton(0) then
    self.yaw += delta.x * 0.2
    self.pitch = math.clamp(self.pitch + delta.y * 0.2, -80, 80)
end
self.distance = math.clamp(self.distance - input.GetScroll().y, 2, 30)
```

See the full [orbit camera recipe](../cookbook.md#orbit-camera-with-the-mouse).

## In the editor

While you play in the editor, the game gets the keyboard and mouse **except**:

- while a text field has the keyboard (typing in the Inspector or the Script Editor),
- while the right mouse button is held over the Scene view (flying the editor camera).

Then every key and button reads as not pressed, so your game doesn't react to what you type. Click the Game view (or empty space) to give the game the keys back.

The mouse position is relative to the editor's whole window, not the Game view; in a built game it is relative to the game's window.

> [!NOTE]
> Input is read once per frame, before `Update`. A press shorter than one frame still counts in `GetKeyDown`.
