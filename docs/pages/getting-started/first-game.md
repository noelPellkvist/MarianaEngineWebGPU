---
title: "Tutorial: Your First Game (3D Snake)"
nav: "Tutorial: 3D Snake"
description: Make a complete 3D Snake game in the Mariana editor with one Luau script, then build it for Windows and the web.
---

# Tutorial: Your First Game (3D Snake)

In this tutorial you make a complete game from an empty scene: a 3D take on *Snake*. You steer with the arrow keys, eat apples to grow, and when you crash, the snake flies apart as physics bodies before a new round starts. Along the way you'll use the parts of the engine you need for any game: entities, a camera, a script with properties, input, spawning, timing, physics and builds.

![The finished game: a green snake on a 15 by 15 board, an apple ahead of it](images/snake-playing.jpg "The finished game. Arrow keys or W A S D steer; every apple makes the snake longer and faster.")

**Time:** about an hour. **You need:** the editor built and running ([Installing](installing.md)) and the [editor tour](editor-tour.md) behind you. No Lua experience is needed: the code is explained as we go, and [Luau for Game Scripting](../scripting/luau.md) covers the language in more depth.

> [!NOTE] The finished game is already in the Sandbox
> The Sandbox project ships with the result: the scene `Scenes/Snake.scene` and the script `Scripts/Snake.luau`. Open it any time to compare, or to play it first.

## How the game works

Before writing code, it helps to know the plan:

- The board is a grid of **cells**, 15 by 15, each 1 m across. A cell is written as a vector with whole numbers in `x` and `z`: `vec3(7, 0, 7)` is the middle.
- The snake is a list of cells, **head first**, and an entity (a cube) for each.
- Every *step* (a fraction of a second), the head moves one cell in the current direction and every other segment takes the cell in front of it.
- Eating an apple makes the snake one segment longer and steps a little shorter.
- Leaving the board or running into yourself ends the round.

Everything happens in **one script** on **one entity**. The script builds the board, spawns the snake and the apple, and runs the game.

## Step 1: A new scene

1. Choose **File > New Scene** (Ctrl+N). If the editor asks about unsaved changes, choose **Don't Save** (or **Save** if you changed the Playground and want to keep it).
2. The scene is empty: no entities, just the sky.

## Step 2: A camera

The game needs a camera to look through: the **Game** window and the built game show what the first camera in the scene sees.

1. Choose **Create > Camera**. A camera entity appears where the editor camera is, and is selected.
2. In the **Inspector**, set its **Transform**:

| Field | X | Y | Z |
|---|---|---|---|
| Position | 0 | 17 | -11 |
| Rotation | 57 | 0 | 0 |

The camera now hangs above and behind the middle of the world, looking down at it at 57 degrees. Cameras look along their own **+Z** axis; rotating 57 degrees about X tips that view downward.

## Step 3: The game entity

The script needs an entity to live on. It doesn't need a mesh: it only holds the script and marks the middle of the board.

1. Choose **Create > Empty Entity**. An entity named *Entity* appears at the world's origin, selected.
2. In the Inspector, rename it **Game** (the **Name** field at the top) and make sure its **Position** is `0, 0, 0`.

## Step 4: Create the script

1. With **Game** selected, click **Add Component** at the bottom of the Inspector and choose **Scripting > Scripts**.
2. In the new **Scripts** section, click **Add Script**, type **MySnake** in the field and click **Create**. The editor writes `Scripts/MySnake.luau` from a template and puts it on the entity.
3. Click the script's **Edit** button. The [Script Editor](../scripting/script-editor.md) opens with the template:

```luau
--!strict
-- MySnake: runs on every entity that has it (scripting/API.md lists what scripts can do).
local MySnake = {} :: Script

-- Values the inspector edits per entity (numbers, booleans, strings, vectors), with their defaults.
MySnake.properties = {
    speed = 1,
}

function MySnake.Start(self: Script)
end

function MySnake.Update(self: Script, dt: number)
end

return MySnake
```

Every script looks like this: it **returns a table** of functions the engine calls. `Start` runs once when the game starts, `Update` every frame. `self` is this script's own data on this entity, and `self.entity` is the entity. `--!strict` turns on full type checking, so the editor catches mistakes as you type.

> [!TIP]
> You'll replace the whole file in a moment. The name of the table (`MySnake`, or `Snake` in the listings below) doesn't have to match the file's name: what counts is that the file returns it.

## Part 1: The board and the snake

Replace the script's text with the code below, a piece at a time, or all at once from the checkpoint at the end of this part.

### Properties and colours

```luau
local Snake = {} :: Script

Snake.properties = {
    size = 15,          -- the board is size x size cells of 1 m
    stepTime = 0.16,    -- seconds per step at the start
    speedUp = 0.96,     -- each apple makes a step take this much of the time before
    startLength = 3,    -- segments at the start
}

local HEAD_COLOR = vec3(0.35, 0.95, 0.35)
local BODY_COLOR = vec3(0.15, 0.6, 0.2)
local WALL_COLOR = vec3(0.25, 0.27, 0.32)
local FLOOR_COLOR = vec3(0.75, 0.78, 0.72)
```

`properties` are values you can change per entity in the Inspector without touching the code; the engine copies them into `self` before `Start`, so the script reads `self.size`, `self.stepTime` and so on. Colours are `vec3`s of red, green and blue from 0 to 1.

### From cells to the world

```luau
-- Where a cell (whole numbers x and z) is in the world: the board is centred on the entity.
local function cellToWorld(self: Script, cell: vector): vector
    local half = (self.size - 1) / 2
    return self.entity.worldPosition + vec3(cell.x - half, 0.45, cell.z - half)
end
```

Cell `(0, 0)` is the board's corner. Subtracting half the board puts cell `(7, 7)` on the entity, so moving the **Game** entity moves the whole board. The `0.45` lifts the cubes so they sit on the floor.

### Building the board

```luau
local function buildBoard(self: Script)
    local center = self.entity.worldPosition
    local floor = scene.SpawnPrimitive("plane", center, FLOOR_COLOR)
    floor.name = "Floor"
    floor.scale = vec3(self.size / 10, 1, self.size / 10) -- the plane is 10 m across

    local half = self.size / 2 + 0.5
    local walls = {
        { vec3(0, 0.5, half), vec3(self.size + 2, 1, 1) },
        { vec3(0, 0.5, -half), vec3(self.size + 2, 1, 1) },
        { vec3(half, 0.5, 0), vec3(1, 1, self.size) },
        { vec3(-half, 0.5, 0), vec3(1, 1, self.size) },
    }
    for _, wall in walls do
        local block = scene.SpawnPrimitive("cube", center + wall[1], WALL_COLOR)
        block.name = "Wall"
        block.scale = wall[2]
    end
end
```

[`scene.SpawnPrimitive`](../scripting/api/scene.md) makes a new entity with a built-in mesh (a 1 m cube, a 1 m sphere, a 10 m plane, a cylinder or a capsule) and a material of its own in the colour you give. The walls are cubes stretched with `scale`: two long ones along X, two along Z.

### The snake

```luau
local function addSegment(self: Script, cell: vector, color: vector): Entity
    local segment = scene.SpawnPrimitive("cube", cellToWorld(self, cell), color)
    segment.name = "Segment"
    segment.scale = vec3(0.85, 0.85, 0.85)
    table.insert(self.segments, segment)
    return segment
end

-- A new game: the snake in the middle of the board, its head on the right.
local function newGame(self: Script)
    self.cells = {}
    self.segments = {}
    local middle = math.floor(self.size / 2)
    for i = 0, self.startLength - 1 do
        local cell = vec3(middle - i, 0, middle)
        table.insert(self.cells, cell)
        addSegment(self, cell, if i == 0 then HEAD_COLOR else BODY_COLOR)
    end
end

function Snake.Start(self: Script)
    buildBoard(self)
    newGame(self)
end

return Snake
```

`self.cells` is the list of cells (head first) and `self.segments` the matching cubes. Anything you store in `self` stays there for the next frame; it's this entity's game state.

Press **Ctrl+S** in the Script Editor to save, then **Play** (Ctrl+P) and look at the **Game** window: a grey-walled board with a three-segment snake in the middle. Press **Ctrl+P** again to stop. Everything the script spawned disappears, because stopping puts the scene back as it was.

![The board and the snake after part 1, seen through the game camera](images/snake-board.jpg "After part 1: the board, its walls and a snake that doesn't move yet.")

<details class="checkpoint"><summary>Checkpoint: the whole script after part 1 <span>(click to show)</span></summary>

```luau title="MySnake.luau"
@include docs/snippets/snake/part1.luau
```
</details>

## Part 2: Moving and steering

A snake that moves needs a direction, a clock and a step.

### Directions from the keyboard

```luau
-- The arrow keys and W A S D as directions on the board (+Z is "up" on the screen).
local KEYS: { [string]: vector } = {
    up = vec3(0, 0, 1), w = vec3(0, 0, 1),
    down = vec3(0, 0, -1), s = vec3(0, 0, -1),
    left = vec3(-1, 0, 0), a = vec3(-1, 0, 0),
    right = vec3(1, 0, 0), d = vec3(1, 0, 0),
}

local function sameCell(a: vector, b: vector): boolean
    return a.x == b.x and a.z == b.z
end

local function isInside(self: Script, cell: vector): boolean
    return cell.x >= 0 and cell.z >= 0 and cell.x < self.size and cell.z < self.size
end
```

The camera looks along +Z, so +Z is "up" on the screen and +X is "right". Keys are named as strings: see [`input`](../scripting/api/input.md) for every name.

At the end of `newGame`, the game also needs a heading, a queue of turns and a clock:

```luau
    self.direction = vec3(1, 0, 0)
    self.queued = {}
    self.timer = 0
    self.currentStep = self.stepTime
    self.over = false
```

### One step

```luau
-- One step: the head moves on a cell and every other segment takes the cell in front of it.
local function step(self: Script)
    if #self.queued > 0 then
        self.direction = table.remove(self.queued, 1) :: vector
    end
    local head = self.cells[1] + self.direction

    -- Walls and its own body (the tail moves away this step, so it doesn't count).
    if not isInside(self, head) then
        self.over = true
        print("Game over: you hit the wall")
        return
    end
    for i = 1, #self.cells - 1 do
        if sameCell(self.cells[i], head) then
            self.over = true
            print("Game over: you bit yourself")
            return
        end
    end

    table.insert(self.cells, 1, head)
    table.remove(self.cells)
    for i, segment in self.segments do
        segment.position = cellToWorld(self, self.cells[i])
    end
    self.segments[1].rotation = quat.lookRotation(self.direction)
end
```

Moving a snake is simpler than it looks: insert the new head cell at the front of the list and remove the last one. Every cube then moves to its cell. `quat.lookRotation` turns the head to face where it's going. `print` writes to the editor's log, with the script's name and line.

### The game loop

```luau
function Snake.Update(self: Script, dt: number)
    if self.over then
        return
    end

    -- Turns are queued (two at most), so quick key presses all count; reversing isn't allowed.
    for key, direction in KEYS do
        if input.GetKeyDown(key) and #self.queued < 2 then
            local heading = if #self.queued > 0 then self.queued[#self.queued] else self.direction
            if not sameCell(direction, heading) and not sameCell(direction, -heading) then
                table.insert(self.queued, direction)
            end
        end
    end

    self.timer += dt
    while self.timer >= self.currentStep and not self.over do
        self.timer -= self.currentStep
        step(self)
    end
end
```

`Update` runs every frame with `dt`, the seconds since the last frame. `input.GetKeyDown` is true only on the frame a key goes down, so holding a key doesn't queue turns over and over. The turn queue makes fast double turns (up, then left) work even when both presses land between two steps. The timer adds up `dt` and takes a step whenever a step's worth has passed, so the snake moves at the same speed at any frame rate.

Save and play. The snake glides right, cell by cell; steer it with the arrow keys or W A S D. When it leaves the board it stops, and the log says *Game over*.

> [!TIP] Tune it live
> While the game runs, select **Game** and change `stepTime` in its Scripts section in the Inspector, or edit the script and press Ctrl+S: the running game picks the change up at once and keeps its state.

<details class="checkpoint"><summary>Checkpoint: the whole script after part 2 <span>(click to show)</span></summary>

```luau title="MySnake.luau"
@include docs/snippets/snake/part2.luau
```
</details>

## Part 3: Apples

### Placing an apple

```luau
local APPLE_COLOR = vec3(0.9, 0.12, 0.1)

-- A free cell for the apple (not under the snake).
local function freeCell(self: Script): vector
    while true do
        local cell = vec3(math.random(0, self.size - 1), 0, math.random(0, self.size - 1))
        local taken = false
        for _, c in self.cells do
            if sameCell(c, cell) then
                taken = true
                break
            end
        end
        if not taken then
            return cell
        end
    end
end

local function placeApple(self: Script)
    self.appleCell = freeCell(self)
    if not (self.apple and self.apple:IsValid()) then
        self.apple = scene.SpawnPrimitive("sphere", nil, APPLE_COLOR)
        self.apple.name = "Apple"
        self.apple.scale = vec3(0.7, 0.7, 0.7)
    end
    self.apple.position = cellToWorld(self, self.appleCell)
end
```

The apple is spawned once and then just moved. `IsValid()` tells you whether an entity still exists. At the end of `newGame`, set `self.score = 0` and call `placeApple(self)`. In `Start`, seed the random numbers so every game is different:

```luau
function Snake.Start(self: Script)
    math.randomseed(os.clock() * 1000)
    buildBoard(self)
    newGame(self)
end
```

### Eating and growing

In `step`, work out whether the head lands on the apple. If it does, the tail stays where it is this step, and a new segment appears there:

```luau
    local head = self.cells[1] + self.direction
    local eats = sameCell(head, self.appleCell)

    -- Walls and its own body (the tail moves away this step, unless the snake grows).
    local last = if eats then #self.cells else #self.cells - 1
    -- ... the wall check as before, then `for i = 1, last do` for the body check ...

    table.insert(self.cells, 1, head)
    if eats then
        -- The new segment goes where the tail is; the tail stays this step.
        addSegment(self, self.cells[#self.cells], BODY_COLOR)
        self.score += 1
        self.currentStep *= self.speedUp
        print(`Score {self.score}`)
        placeApple(self)
    else
        table.remove(self.cells)
    end
```

Strings in backticks are *interpolated*: `{self.score}` is replaced by the value. Every apple multiplies the step time by `speedUp` (0.96), so the game slowly gets faster.

<details class="checkpoint"><summary>Checkpoint: the whole script after part 3 <span>(click to show)</span></summary>

```luau title="MySnake.luau"
@include docs/snippets/snake/part3.luau
```
</details>

## Part 4: Crashing with physics

Stopping dead is a dull ending. Instead, every segment turns into a physics body and flies apart. Physics needs something to land on and bounce off, so first give the board colliders. In `buildBoard`, add an invisible slab under the floor, and a collider on each wall:

```luau
    -- An invisible slab under the floor, so what flies apart at the end lands on something.
    local ground = scene.Create("Ground", self.entity)
    ground.worldPosition = center - vec3(0, 0.5, 0)
    ground:Add("BoxCollider", { size = vec3(self.size + 2, 1, self.size + 2) })
```

```luau
        block:Add("BoxCollider") -- 1 m, scaled with the entity
```

`scene.Create` makes an empty entity (here a child of the Game entity). `Add` puts a [component](../scripting/api/components.md) on an entity, with any fields you want to set. A collider without a rigid body is a *static* body: it never moves, but things collide with it.

Then a `gameOver` function that does the flying:

```luau
-- The game is lost: every segment becomes a physics body and flies apart.
local function gameOver(self: Script)
    self.over = true
    self.overTime = 0
    self.best = math.max(self.best or 0, self.score)
    print(`Game over! Score {self.score}, best {self.best}`)
    for _, segment in self.segments do
        local away = vec3(math.random() - 0.5, 0, math.random() - 0.5) * 8
        segment:Add("BoxCollider")
        segment:Add("RigidBody", {
            motion = "Dynamic",
            mass = 1,
            startVelocity = away + vec3(0, 6 + math.random() * 4, 0),
            startAngularVelocity = vec3(math.random() * 10, math.random() * 10, math.random() * 10),
        })
    end
end
```

A **Dynamic** rigid body is moved by physics: gravity, collisions and the start velocities you give it. In `step`, call `gameOver(self)` instead of setting `self.over` in both crash checks. Finally, at the top of `Update`, start a new round once the pieces have flown for a while:

```luau
    if self.over then
        -- Two and a half seconds of flying pieces, then a new game.
        self.overTime += dt
        if self.overTime > 2.5 then
            for _, segment in self.segments do
                segment:Destroy()
            end
            newGame(self)
        end
        return
    end
```

`Destroy` removes an entity (and its children) at the end of the frame.

![The snake flying apart after hitting the wall](images/snake-crash.jpg "Part 4: the segments become rigid bodies and scatter across the board.")

<details class="checkpoint"><summary>Checkpoint: the whole script after part 4 <span>(click to show)</span></summary>

```luau title="MySnake.luau"
@include docs/snippets/snake/part4.luau
```
</details>

## Part 5: Polish

Three touches make the game feel finished. They're all in the final script below:

- **Smooth movement.** Instead of jumping from cell to cell, each segment glides between its old cell and its new one during a step. `step` keeps where each segment started in `self.from`, and `Update` sets every segment's position to `vector.lerp(from, to, t)`, where `t` is how far the current step has got.
- **Eyes.** Two small black spheres are made children of the head (`eye.parent = head`), so they move and turn with it. A child's `position` is relative to its parent.
- **A lively apple.** The apple bobs with `math.sin(time.time * 4)` and spins with `Rotate`.

### The complete script

This is `Scripts/Snake.luau` from the Sandbox, the game you saw at the top:

```luau title="Snake.luau"
@include projects/Sandbox/Scripts/Snake.luau
```

## Save the scene

Stop the game, then press **Ctrl+S**. The scene has no file yet, so the editor asks for a name: type **MySnake** and press Enter. It's saved to `Scenes/MySnake.scene` with everything in it: the camera, the Game entity with its script and property values, the lighting and the render pipeline.

## Build your game

Your game is ready to leave the editor.

1. Choose **File > Build > Build Settings…**.
2. Select the **Windows** profile (or click **+ Windows** to make a new one).
3. Set **Game name** to `Snake`, **Window title** to `Snake`, and **Start scene** to `/Scenes/MySnake.scene`.
4. Click **Build and Run**.

The Build window shows each step and the build log. The first build compiles the player (a few minutes); later builds take seconds. When it's done, your game starts in its own window. The game is in `Builds/Windows/` inside the project: `Snake.exe` and its `data` folder. Zip that folder and send it to a friend.

For the browser, use the **Web** profile instead: it makes a folder with `index.html` that you can upload to any web host. See [Web Builds](../building/web.md).

![The Build window with the Windows and Web profiles](images/build-window.jpg "File > Build > Build Settings: profiles on the left, the selected profile's settings on the right, the log below.")

## Going further

Some ideas to take it further, with pointers to what you'll need:

- **Show the score.** There's no text UI yet, but you can spawn a small gold cube on the wall for every apple. Keep them in a list and destroy them in `newGame`.
- **Obstacles.** Spawn a few cubes on random free cells at the start of each round and treat them like walls in `step`.
- **A second snake.** Put two Game entities in the scene, give one the property `startLength = 5` and change its keys. Each entity runs the script with its own `self`.
- **A chase camera.** The Sandbox's [`ChaseCamera.luau`](../scripting/api/sandbox-scripts.md) follows a target by name: try following the snake's head.
- **More physics.** Make the apple a dynamic body that drops onto the board when it appears: `self.apple:Add("SphereCollider")` and `self.apple:Add("RigidBody", { motion = "Dynamic" })`.

## What you learned

| Concept | Where to read more |
|---|---|
| Scripts, `self`, `Start` and `Update` | [Script Lifecycle](../scripting/lifecycle.md) |
| Properties edited in the Inspector | [Properties and State](../scripting/properties.md) |
| Spawning, finding and destroying entities | [`scene`](../scripting/api/scene.md), [`Entity`](../scripting/api/entity.md) |
| Keys and the mouse | [`input`](../scripting/api/input.md) |
| Vectors and rotations | [`vec3` and `vector`](../scripting/api/vec3.md), [`quat`](../scripting/api/quat.md) |
| Components from code, rigid bodies and colliders | [Components](../scripting/api/components.md), [Physics](../manual/physics.md) |
| Building for Windows and the web | [Build Profiles](../building/build-profiles.md) |
