---
title: save
description: Keeping data between runs of a Mariana game: high scores, settings, progress, with the save global.
---

# save

The `save` global keeps values between runs of the game: high scores, unlocked levels, settings. Values are kept by name (a key) and written when they change.

```luau
local best = save.Get("best", 0)     -- 0 the first time
if score > best then
    save.Set("best", score)
end
```

| Function | |
|---|---|
| `save.Set(key, value)` | Keeps a value under a key (`nil` removes it) |
| `save.Get(key, default?) -> any` | The value, or `default` (`nil`) when there's none |
| `save.Has(key) -> boolean` | Whether there is a value |
| `save.Delete(key)` | Removes a value |
| `save.Keys() -> { string }` | Every key |
| `save.Clear()` | Removes everything |
| `save.Flush()` | Writes now (it's done after every frame that changed something anyway) |

## What it keeps {#values}

Numbers, booleans, strings, vectors, and tables of them (lists, and tables with string keys, nested):

```luau
save.Set("player", {
    name = "Ada",
    level = 3,
    checkpoint = vec3(12, 0, 40),
    keys = { "red", "blue" },
})
local player = save.Get("player")
print(player.name, player.checkpoint.x, #player.keys)
```

Entities, components, functions and the like can't be kept (they don't exist in the next run): keep what you need to make them again (a name, a position, a count). A table comes back as a new table.

## Where it's kept {#where}

| | |
|---|---|
| **A built game on Windows** | `%APPDATA%/<game's title>/save.json` (the window title from its [build profile](../../building/build-profiles.md)) |
| **A web build** | The browser's local storage for the page, under the game's title |
| **Playing in the editor** | `<project>/.mariana/save.json`: delete it to start afresh |

It's a JSON file, written after any frame a script changed it and when the game stops. The editor's copy is separate from a built game's, so testing doesn't touch a player's progress.
