---
title: Properties, State and Talking Between Scripts
nav: Properties and state
description: Inspector properties, per-instance state in self, state shared by every instance, and how scripts find and call each other.
---

# Properties, State and Talking Between Scripts

## Properties

A script's `properties` table lists the values designers tune per entity in the Inspector, with their default values:

```luau
Enemy.properties = {
    health = 100,                   -- number
    aggressive = true,              -- boolean
    targetName = "Player",          -- string
    patrolOffset = vec3(0, 0, 10),  -- vector
}
```

| Type | Inspector shows | Example |
|---|---|---|
| number | A number field (drag or type) | `speed = 4.5` |
| boolean | A tick box | `loop = true` |
| string | A text field | `target = "Car"` |
| vector | Three number fields | `offset = vec3(0, 2, -6)` |

Other types (tables, entities, functions) can't be properties; keep them in `self` instead.

How they work:

- Before `Start`, every property is copied into `self`: read it as `self.health`. The default is used unless the entity has its own value.
- Values set on an entity are saved with the scene and drawn in the accent colour. Right-click one and choose **Reset to default** to go back to the script's value.
- Change a value while the game runs and the running instance's `self` gets it immediately (overwriting what the script may have stored there).
- Properties are listed alphabetically in the Inspector. Add a comment after each to remember what it's for; the comments are only in the code.

## Per-instance state: `self`

`self` belongs to one script on one entity. Anything you store there stays until the instance ends:

```luau
function Enemy.Start(self: Script)
    self.hits = 0
    self.home = self.entity.position
end

function Enemy.OnContactBegin(self: Script, other: Entity, contact: Contact)
    self.hits += 1
end
```

`self.entity` is the entity. `self.properties` is the script's defaults table (the same for every instance), so read the actual values from `self`.

## Shared state: module locals

Code at the top level of a script runs **once per script file**, not once per entity. Locals declared there are shared by every instance of that script:

```luau
local Coin = {} :: Script

local collected = 0          -- shared by every coin

function Coin.OnTriggerEnter(self: Script, other: Entity)
    collected += 1
    print(`{collected} coins collected`)
    self.entity:Destroy()
end

return Coin
```

Use this for constants (colours, key maps, tuning tables) and for state that's naturally shared, like a counter of every coin. Module locals start fresh every time the game starts, and also when the script is reloaded while playing (the file runs again); `self` is what survives a reload, so keep anything that must last in `self`.

## Finding other entities

```luau
local player = scene.Find("Player")            -- by name: the first created with that name
local target = self.entity.parent              -- the parent
local wheel = self.entity:FindChild("Wheel_FL") -- anywhere below this entity, by name

-- Every entity with a component:
scene.Each("Vehicle", function(car)
    print(car.name)
end)
```

Finding by name walks the scene, so don't do it every frame: find once (in `Start`, or lazily the first time) and keep the entity in `self`. Check `IsValid()` before using an entity that could have been destroyed:

```luau
function Follow.Update(self: Script, dt: number)
    if not (self.target and self.target:IsValid()) then
        self.target = scene.Find(self.targetName)
        if not self.target then
            return
        end
    end
    -- use self.target
end
```

Entities are the same value each time you get them, so they work as table keys and compare with `==`:

```luau
self.seen[other] = true
if other == self.target then ... end
```

## Calling another script

`entity:GetScript(name)` returns another script instance's `self` on that entity, by the script's file name without `.luau` (`nil` if the entity doesn't have it). Through it you can read its state and call its functions:

```luau title="Health.luau"
local Health = {} :: Script
Health.properties = { maxHealth = 100 }

function Health.Start(self: Script)
    self.current = self.maxHealth
end

-- Not called by the engine: other scripts call it.
function Health.Damage(self: Script, amount: number)
    self.current -= amount
    if self.current <= 0 then
        self.entity:Destroy()
    end
end

return Health
```

```luau title="Spikes.luau"
local Spikes = {} :: Script

function Spikes.OnTriggerEnter(self: Script, other: Entity)
    local health = other:GetScript("Health")
    if health then
        health:Damage(25)
    end
end

return Spikes
```

`health:Damage(25)` passes `health` as `self`: with `GetScript` you get the instance, and its functions work through it as through a method call. This is how scripts expose an API to each other.

> [!TIP]
> Calling a function the engine doesn't know (like `Damage`) is plain Luau: any function in the script's table can be called this way.

## Talking to components

Components are reached by name and changed field by field:

```luau
local body = self.entity:Get("RigidBody")
if body then
    body.gravityFactor = 0       -- float in the air
end
```

See [Components](api/components.md).

## Game managers

For game-wide state (score, lives, the current level), the usual pattern is one entity with a manager script, found by name:

```luau title="Game.luau"
local Game = {} :: Script

function Game.Start(self: Script)
    self.score = 0
end

function Game.AddScore(self: Script, points: number)
    self.score += points
    print(`Score: {self.score}`)
end

return Game
```

```luau
-- anywhere else:
local game = scene.Find("Game")
if game then
    game:GetScript("Game"):AddScore(10)
end
```
