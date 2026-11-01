---
title: Your First Script
description: Write, attach and run a script step by step: a floating, spinning pickup that you can tune in the Inspector and collect with the car.
---

# Your First Script

In this walkthrough you write a **pickup**: a coin that floats and spins until something drives through it. You'll attach a script, use properties, react to a trigger and see live reloading. It takes about 15 minutes in the Sandbox's Playground scene.

## 1. Make the coin

1. Open the Playground (it opens when the editor starts; otherwise File > Open Scene > *Playground.scene*).
2. **Create > Cylinder**. In the Inspector, rename it **Coin**, set its **Scale** to `1.2, 0.15, 1.2` and its **Rotation** to `90, 0, 0` so it stands on its edge.
3. Move it in front of the car with the gizmo (press **1**), about 1.5 m above the ground.
4. **Add Component > Physics > Sphere Collider**, then tick **trigger** in its section. A trigger reports what passes through it instead of blocking it.

## 2. Create the script

1. **Add Component > Scripting > Scripts**.
2. **Add Script**, type `Pickup`, click **Create**. The file `Scripts/Pickup.luau` is created and attached.
3. Click **Edit** to open it in the Script Editor.

Replace the template with:

```luau title="Pickup.luau"
--!strict
-- Pickup: floats and spins; whatever passes through it collects it.
local Pickup = {} :: Script

Pickup.properties = {
    spinSpeed = 180,    -- degrees per second
    bobHeight = 0.2,    -- metres up and down
    value = 10,         -- points it's worth
}

function Pickup.Start(self: Script)
    self.base = self.entity.position
    self.time = 0
end

function Pickup.Update(self: Script, dt: number)
    self.time += dt
    self.entity.position = self.base + vec3(0, math.sin(self.time * 3) * self.bobHeight, 0)
    self.entity:Rotate(vec3(0, 1, 0), self.spinSpeed * dt)
end

function Pickup.OnTriggerEnter(self: Script, other: Entity)
    print(`{other.name} collected {self.entity.name} for {self.value} points`)
    self.entity:Destroy()
end

return Pickup
```

Press **Ctrl+S** to save.

What it does:

- `Start` remembers where the coin was placed (`self.base`) and starts a clock of its own. Anything you put in `self` stays for the next frame, separately for every coin.
- `Update` runs every frame: it sets the position from the base plus a sine wave, and turns the coin about its own Y axis. `dt` is the frame's duration in seconds, so the speeds are per second however fast the game runs.
- `OnTriggerEnter` runs when a body enters the coin's trigger collider. It logs a message and destroys the coin.

> [!NOTE]
> The cylinder was rotated 90 degrees about X, so its own Y axis now lies flat. `Rotate` turns about the entity's **own** axes, so the coin spins like a coin rolling on its edge. Try `vec3(0, 0, 1)` to see the difference.

## 3. Play

Press **Play** (Ctrl+P) and watch the Game window: the coin bobs and spins. Drive the car through it with **W A S D**: the log says *… collected Coin for 10 points*, with the car's name, and the coin is gone. Press **Stop**: the coin is back, because stopping restores the scene.

## 4. Tune it in the Inspector

The **Scripts** section of the coin shows its properties: `spinSpeed`, `bobHeight`, `value`. Change `spinSpeed` to 720 **while the game is running**: the coin spins faster at once.

Values you set on an entity are drawn in the accent colour, to show they differ from the script's default. Right-click a value and choose **Reset to default** to undo it. Each coin can have its own values: duplicate the coin a few times and give each a different `value`.

## 5. Change the code while playing

With the game running, change the `Update` function to tilt the coin as it spins:

```luau
function Pickup.Update(self: Script, dt: number)
    self.time += dt
    self.entity.position = self.base + vec3(0, math.sin(self.time * 3) * self.bobHeight, 0)
    self.entity:Rotate(vec3(0, 1, 0), self.spinSpeed * dt)
    self.entity:Rotate(vec3(1, 0, 0), math.sin(self.time) * 20 * dt)
end
```

Press Ctrl+S. The running coin picks up the new code immediately, keeping its `self` (its base and its clock).

## 6. Make a mistake on purpose

Change `self.entity:Rotate` to `self.entity:Rotat` and save. The Script Editor underlines it before you even play: *Key 'Rotat' not found in external type 'Entity'*. Play anyway: the script stops with the error, shown in red in the Inspector next to the script and in the log with its line. The rest of the game keeps running. Fix it and save, and the script starts again.

## What's next

- [Script Lifecycle](lifecycle.md): every function the engine calls, and when.
- [Properties and State](properties.md): properties, `self`, and how scripts talk to each other.
- [Cookbook](cookbook.md): pickups that count a score, doors, projectiles, cameras and more.
