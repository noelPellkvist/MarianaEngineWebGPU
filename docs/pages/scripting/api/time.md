---
title: time
description: Game time in Mariana scripts: the frame's delta, the time since Play, the frame count and the time scale for slow motion.
---

# time

The `time` global is the game's clock. It runs only while the game plays.

| Field | Type | |
|---|---|---|
| `time.delta` | number | Seconds this frame took in game time, the same as `Update`'s `dt` |
| `time.time` | number | Seconds of game time since Play |
| `time.frame` | number | Frames since Play |
| `time.scale` | number | How fast game time runs: 1 = normal, 0.5 = half speed, 0 = frozen. Read **and write** |

```luau
-- Something that happens every 2 seconds
if time.time - self.lastPulse >= 2 then
    self.lastPulse = time.time
    pulse()
end
```

## Game time and real time

Everything in the game runs on game time: scripts' `dt`, animation and physics.

- **Paused** in the editor, game time stands still (`dt` is 0). **Step** advances exactly one frame of 1/60 s.
- A frame that takes longer than **0.25 s** (a hitch, a breakpoint, a slow load) counts as 0.25 s, so things don't jump through walls after a stall.
- `time.scale` multiplies it. At 0.5, `dt` is half the frame's real length, physics simulates half as much per frame and animations play at half speed.

## Slow motion

```luau title="Bullet time while Shift is held"
local SlowMotion = {} :: Script

function SlowMotion.Update(self: Script, dt: number)
    local target = if input.GetKey("shift") then 0.25 else 1
    -- Ease toward it in real time (dt shrinks with the scale, so divide it back out)
    local real = if time.scale > 0 then dt / time.scale else 0
    time.scale += (target - time.scale) * math.min(1, real * 8)
end

return SlowMotion
```

`time.scale` can't be negative (time doesn't run backwards). Setting it to 0 freezes the game, including your own `Update`'s `dt`, so a script that should bring time back must use something other than `dt` (input, or `os.clock()`).

> [!NOTE]
> The scale stays as you set it until something sets it again. Stop in the editor puts it back to 1.

## Measuring code

`os.clock()` is a precise real-time clock (seconds, for measuring how long code takes). Prefer the [Profiler](../../manual/profiler.md) for finding what is slow: every script function shows there under its own name.

```luau
local start = os.clock()
rebuildLevel()
print(string.format("rebuilt in %.2f ms", (os.clock() - start) * 1000))
```
