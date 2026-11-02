---
title: Luau for Game Scripting
nav: Luau language
description: The Luau language as you'll use it in Mariana scripts, for programmers coming from C#, C++, JavaScript or Python.
---

# Luau for Game Scripting

Luau is Lua 5.1 with types, compound assignment, string interpolation, if-expressions and a native vector type. If you know any C-like language or Python, you can read it at once; this page covers what you need for game scripts and the surprises worth knowing. The full language is described at [luau.org](https://luau.org).

## Values and variables

```luau
local speed = 4.5                 -- number (always a 64-bit float)
local name = "Player"             -- string
local alive = true                -- boolean
local target = nil                -- nothing
local offset = vec3(0, 2, -6)     -- vector (x, y, z)
local spin = quat.euler(0, 90, 0) -- quaternion (an engine type)
local inventory = { "sword", "key" }   -- table

speed += 1          -- also -=, *=, /=, //=, %=, ^=, ..=
local half = speed / 2
local cells = 17 // 4   -- 4: floor division
local remainder = 17 % 4
```

Always declare variables with `local`. Scripts' globals are read-only, so assigning to an undeclared name is an error.

## Truthiness and comparisons

- Only `nil` and `false` are false. **`0` and `""` are true.**
- Not-equal is `~=`, not `!=`. Logical operators are the words `and`, `or`, `not`.
- `a and b or c` was the old ternary; Luau has a real one:

```luau
local color = if health > 50 then GREEN elseif health > 20 then YELLOW else RED
```

## Strings

```luau
local message = "Score: " .. score          -- .. joins strings
local better = `Score: {score} of {best}`   -- backticks interpolate expressions
local padded = string.format("%05.1f", 3.14159)   -- "003.1"
print(name:upper(), #name, name:sub(1, 3))
```

## Control flow

```luau
if speed > 10 then
    brake()
elseif speed < 1 then
    accelerate()
else
    cruise()
end

for i = 1, 10 do          -- 1 to 10 inclusive
    print(i)
end
for i = 10, 1, -2 do      -- 10, 8, 6, 4, 2
end

while not done do
    done = step()
end

repeat
    attempts += 1
until attempts >= 3
```

`continue` skips to the next loop iteration; `break` leaves the loop.

## Tables

Tables are Luau's one data structure: arrays, dictionaries and objects.

```luau
-- Arrays start at 1
local path = { vec3(0, 0, 0), vec3(5, 0, 0), vec3(5, 0, 5) }
print(#path)                  -- 3
table.insert(path, vec3(0, 0, 5))      -- append
table.insert(path, 1, vec3(-5, 0, 0))  -- insert at the front
local last = table.remove(path)        -- remove and return the last
local first = table.remove(path, 1)    -- remove and return the first

-- Iterating (Luau iterates arrays in order, dictionaries in any order)
for i, point in path do
    print(i, point)
end

-- Dictionaries
local prices = { apple = 5, pear = 3 }
prices.banana = 2
prices["kiwi"] = 4
for fruit, price in prices do
    print(fruit, price)
end
prices.pear = nil             -- removes the entry

-- Useful functions
local copy = table.clone(path)
local index = table.find(path, someValue)       -- or nil
table.sort(scores, function(a, b) return a > b end)
print(table.concat({ "a", "b", "c" }, ", "))   -- "a, b, c"
```

Entities can be table keys: `seen[entity] = true`.

## Functions

```luau
local function clampSpeed(speed: number, limit: number): number
    return math.min(speed, limit)
end

-- Several return values
local function minMax(a: number, b: number): (number, number)
    return math.min(a, b), math.max(a, b)
end
local low, high = minMax(7, 3)

-- Functions are values: pass them around
scene.Each("RigidBody", function(e)
    physics.WakeUp(e)
end)
```

Local functions must be declared before the code that calls them (above it in the file).

### `.` and `:`

`object:Method(a)` is short for `object.Method(object, a)`: the colon passes the object itself first. Engine types use the colon for methods:

```luau
self.entity:Rotate(vec3(0, 1, 0), 45)      -- a method: colon
local p = self.entity.position             -- a property: dot
local body = self.entity:Get("RigidBody")  -- a method
body.mass = 20                             -- a field: dot
```

A script's own functions are declared with a dot and an explicit `self` (`function Enemy.Update(self: Script, dt: number)`), which lets the type checker check them. Calling them on another instance uses the colon: `other:GetScript("Health"):Damage(10)`.

## Types

Type annotations are optional, but they make the editor catch mistakes:

```luau
local speed: number = 4
local target: Entity? = nil          -- ? means "or nil"
local function distance(a: vector, b: vector): number
    return vector.magnitude(b - a)
end
type Waypoint = { position: vector, wait: number }
local route: { Waypoint } = {}
```

A script whose first line is `--!strict` is checked fully; without it, checking is looser. The engine's types are `Entity`, `quat`, `Script`, `Contact`, `RaycastHit` and one per component (`RigidBody`, `Vehicle`, …). Use `if x then` to narrow an optional (`Entity?`) to a value:

```luau
local car = scene.Find("Car")      -- Entity?
if car then
    car.position = vec3(0, 1, 0)   -- Entity here
end
```

Type errors never stop a script from running; they're warnings to help you.

## Vectors

`vec3(x, y, z)` makes a vector, Luau's built-in `vector` type: fast, and compared and printed by value.

```luau
local a = vec3(1, 2, 3)
local b = a + vec3(0, 1, 0)    -- + - * / with vectors and numbers
local c = a * 2
print(a.x, a.y, a.z)
print(vector.magnitude(a), vector.normalize(a), vector.dot(a, b), vector.cross(a, b))
```

See [`vec3` and `vector`](api/vec3.md).

## The standard library

Available in scripts:

| Library | Highlights |
|---|---|
| `math` | `abs`, `floor`, `ceil`, `round`, `min`, `max`, `clamp`, `sign`, `sqrt`, `sin`, `cos`, `tan`, `atan2`, `rad`, `deg`, `pi`, `huge`, `random`, `randomseed`, `noise` |
| `string` | `format`, `sub`, `upper`, `lower`, `rep`, `find`, `match`, `gmatch`, `gsub`, `split`, `byte`, `char` |
| `table` | `insert`, `remove`, `find`, `sort`, `concat`, `clone`, `clear`, `create`, `freeze` |
| `vector` | `magnitude`, `normalize`, `dot`, `cross`, `lerp`, `min`, `max`, `clamp`, `floor`, `ceil`, `abs`, `sign`, `angle` |
| `bit32`, `utf8`, `buffer`, `coroutine` | As in Luau |
| `os` | `clock` (seconds, for timing), `time`, `date` |
| `print`, `warn` | To the log, with the script's name and line |

Not available: files (`io`) and Roblox's APIs (`game`, `workspace`, `task`, `wait`, `Instance`). `require` loads library scripts: see [Sharing Code with require](modules.md).

> [!NOTE]
> There's no `wait()`: a script function must return so the frame can go on. For things that take time, keep a timer in `self` and check it in `Update`. See [Timers and cooldowns](cookbook.md#timers-and-cooldowns).

## Common pitfalls

| Pitfall | |
|---|---|
| Arrays start at **1** | `list[1]` is the first element, `list[#list]` the last |
| `0` is true | `if count then` is true for 0; write `if count > 0 then` |
| `#` counts arrays only | For a dictionary, count with a loop |
| Holes in arrays | `list[3] = nil` in the middle makes `#list` unreliable: use `table.remove` |
| `local` inside a block | A `local` declared in an `if` doesn't exist after its `end` |
| Number keys vs string keys | `t[1]` and `t["1"]` are different entries |
| Comparing entities | Use `==`; `scene.Find` returns the same value for the same entity |
