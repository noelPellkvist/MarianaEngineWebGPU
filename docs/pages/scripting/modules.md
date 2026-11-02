---
title: Sharing Code with require
nav: Sharing code (require)
description: Library scripts in Mariana: sharing functions, constants and state between scripts with require.
---

# Sharing Code with require

A script on an entity runs for that entity. Code that several scripts need (maths helpers, a table of settings, a scoreboard everyone writes to) goes in a **library**: a `.luau` file that returns whatever it wants to share, loaded with `require`.

```luau title="Scripts/Lib/Grid.luau"
--!strict
-- Helpers for things on a grid of 1 m cells.
local Grid = {}

Grid.cellSize = 1

function Grid.toWorld(cell: vector): vector
    return vec3(cell.x * Grid.cellSize, 0, cell.z * Grid.cellSize)
end

function Grid.toCell(position: vector): vector
    return vec3(math.floor(position.x / Grid.cellSize + 0.5), 0, math.floor(position.z / Grid.cellSize + 0.5))
end

return Grid
```

```luau title="Scripts/Builder.luau"
--!strict
local Grid = require("./Lib/Grid")

local Builder = {} :: Script

function Builder.Start(self: Script)
    self.entity.position = Grid.toWorld(Grid.toCell(self.entity.position))   -- snapped to the grid
end

return Builder
```

## Paths

| `require(...)` | Finds |
|---|---|
| `"./Grid"` | `Grid.luau` in the same folder as the script that requires it |
| `"./Lib/Grid"` | In a folder below it |
| `"../Grid"` | In the folder above |
| `"/Scripts/Lib/Grid"` | By [resource path](../getting-started/projects.md#resource-paths), from anywhere |

The extension is optional (`.luau`, else `.lua`). A path that leads nowhere is an error naming the script it was required from.

## How libraries run

- A library runs **once** per game, the first time something requires it; every later `require` of it returns the same value. So a table a library returns is **shared**: change a field in one script and the others see it.

```luau title="Scripts/Lib/Score.luau"
--!strict
-- One scoreboard for the whole game: every script that requires it shares this table.
local Score = { points = 0 }

function Score.add(amount: number)
    Score.points += amount
    print("Score:", Score.points)
end

return Score
```

- A library can return anything (a table, a function, a number); a script **on an entity** must return its table of callbacks. A library isn't put on entities: it has no `self`, `Start` or `Update` of its own.
- Libraries can require other libraries. A library that ends up requiring itself (A requires B requires A) is an error, since neither could finish first.
- Everything resets when the game stops: the next Play runs every library again, from scratch.

## Changing a library while playing

Save a library and the game picks it up at once: the library runs again, and so does every script that required it (directly or through another library), keeping their instances' state as a [reload](lifecycle.md#reloading) does. Values a library kept in its own locals start again.

## Types

The [type checker](script-editor.md#type-checking) follows `require`: a library's functions and fields come with their types, so calling `Grid.toWorld("A1")` is underlined, and autocomplete lists what the library has. A library can also share types:

```luau
-- In Lib/Grid.luau
export type Cell = vector

-- In a script that requires it
local Grid = require("./Lib/Grid")
local start: Grid.Cell = vec3(0, 0, 0)
```

## In built games

Games always take the whole `Scripts` folder, so keep libraries in it (or below it).
