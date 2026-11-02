---
title: Scripting API Reference
nav: Overview
description: Every global, type and function available to Mariana Luau scripts.
---

# Scripting API Reference

This is the complete reference for what scripts can use. For how scripts work, start with the [Scripting Overview](../overview.md).

## Globals

| Global | What it is |
|---|---|
| [`vec3(x, y, z)`](vec3.md) | Makes a vector. Luau's `vector` library works on it |
| [`quat`](quat.md) | Rotations: `quat.euler`, `quat.angleAxis`, `quat.lookRotation`, … |
| [`scene`](scene.md) | Finding, creating, spawning and destroying entities |
| [`input`](input.md) | Keyboard and mouse, the cursor |
| [`screen`](screen.md) | The game view: rays through the mouse (picking), points of the world on screen |
| [`time`](time.md) | Frame time, game time, frame count, time scale |
| [`task`](task.md) | Functions that wait: `task.spawn`, `task.delay`, `task.wait` |
| [`physics`](physics.md) | Raycasts, overlaps, forces, velocities, teleporting, vehicle, character and ragdoll control, water |
| [`lighting`](lighting.md#lighting) | The sun, the sky and exposure |
| [`effects`](lighting.md#effects) | The render pipeline's effects: parameters, on and off |
| [`gizmos`](gizmos.md) | Debug drawing: lines, spheres, boxes, text |
| [`save`](save.md) | Values kept between runs of the game |
| [`mesh`](meshes.md#mesh-new) | Builds meshes at run time: `mesh.new{ positions = ..., indices = ... }` |
| [`material`](meshes.md#material-new) | New materials: `material.new{ color = ..., metallic = ... }` |
| [`texture`](meshes.md#texture-load) | Loads images for materials: `texture.load("/Textures/Grass.png")` |
| `print(...)`, `warn(...)` | Log messages, with the script's name and line |
| [`require(path)`](../modules.md) | Loads a library script and returns what it returned |

Plus Luau's standard libraries: `math`, `string`, `table`, `vector`, `bit32`, `utf8`, `buffer`, `coroutine` and `os.clock`/`os.time`/`os.date`. See [Luau for Game Scripting](../luau.md#the-standard-library).

## Types

| Type | What it is |
|---|---|
| [`Entity`](entity.md) | Something in the scene: its transform, hierarchy, components and methods |
| [`Component`](components.md) | One component of an entity, read and written by field name |
| [`Animator`](animator.md) | The Animator component, with methods to drive animation |
| [Component types](component-reference.md) | `RigidBody`, `BoxCollider`, `Vehicle`, `CharacterController`, … with their fields |
| `vector` | A 3D vector (made with `vec3`) |
| [`quat`](quat.md) | A rotation |
| [`Mesh`, `Material`, `Texture`](meshes.md) | A mesh to draw, how a surface looks, an image |
| [`Script`](types.md#script) | A script's table and its `self` |
| [`Contact`](types.md#contact) | A physics contact: point, normal, speed |
| [`RaycastHit`](types.md#raycasthit) | What a ray or shape cast hit |
| [`QueryOptions`](types.md#queryoptions) | Options for physics queries |
| [`BodyInfo`, `VehicleInfo`, `WheelInfo`, `CharacterInfo`](types.md) | State returned by `physics.Get…Info` |
| `ComponentName` | Any component's name, as a string |

## Script functions

The engine calls these on your script when they exist. See [Script Lifecycle](../lifecycle.md).

| Function | When |
|---|---|
| `Start(self)` | Once, before the first Update |
| `Update(self, dt)` | Every frame |
| `LateUpdate(self, dt)` | Every frame, after physics and animation |
| `FixedUpdate(self, dt)` | Before every physics step (`dt` = the step, 1/60 s) |
| `OnContactBegin(self, other, contact)`, `OnContactEnd(self, other)` | Bodies touching |
| `OnTriggerEnter(self, other)`, `OnTriggerExit(self, other)` | Triggers |
| `OnAnimationEvent(self, name, clip)` | Animation events |
| `OnDestroy(self)` | The instance ends |

Any other function can be called by name from other scripts with [`entity:Send`](entity.md#send) and [`scene.Broadcast`](scene.md#broadcast).

## Conventions

| | |
|---|---|
| **Units** | Metres, kilograms, seconds. Speeds in m/s, forces in newtons, impulses in newton-seconds |
| **Angles** | Degrees everywhere in the script API (rotations, `Rotate`, `quat.euler`, `angleAxis`, character turn speeds), except where a field says otherwise (angular velocities are in radians per second) |
| **Axes** | +X right, +Y up, +Z forward. Entities look along their +Z |
| **Methods and properties** | Methods use a colon: `entity:Rotate(axis, 45)`. Properties and fields use a dot: `entity.position`, `body.mass` |
| **Names** | Components, keys and enum values are strings: `"RigidBody"`, `"space"`, `"Dynamic"` |
| **Errors** | Wrong arguments raise errors with a message saying what was expected; functions that look things up (`scene.Find`, `entity:Get`, `physics.Raycast`) return `nil` when there's nothing |
| **Clip indices** | Animator clips by index count from **0** (`animator:Play(0)`); use names where you can |

## Quick reference

```luau
-- Entities
local e = scene.Find("Player")                 -- Entity?
local box = scene.SpawnPrimitive("cube", vec3(0, 5, 0), vec3(1, 0, 0))
local model = scene.Spawn("/Models/Rumba.glb", vec3(2, 0, 0))
local empty = scene.Create("Group")
local copy = scene.Clone(template, vec3(0, 0, 5));  copy.active = true
local crate = scene.Spawn("/Prefabs/Crate.prefab", vec3(0, 3, 0))
e.position = vec3(0, 1, 0);  e.rotation = quat.euler(0, 90, 0);  e.scale = vec3(2, 2, 2)
e.worldPosition, e.worldRotation, e.forward, e.right, e.up, e.parent, e.name
e:Translate(vec3(0, 0, 1));  e:Rotate(vec3(0, 1, 0), 45);  e:LookAt(target)
e:GetChildren();  e:FindChild("Wheel");  e:GetScript("Health");  e:IsValid();  e:Destroy()
e.active = false;  e.visible = false;  e.activeInHierarchy
e:AddScript("/Scripts/Health.luau", { max = 100 });  e:RemoveScript("Health");  e:SetScriptEnabled("Health", false)
local Grid = require("./Lib/Grid")

-- Components
local body = e:Get("RigidBody")                -- RigidBody?
e:Add("BoxCollider", { size = vec3(1, 2, 1) })
e:Has("Vehicle");  e:Remove("RigidBody")
scene.Each("Vehicle", function(car) ... end)

-- Input, the view and time
input.GetKey("w");  input.GetKeyDown("space");  input.GetKeyUp("e")
input.GetMouseButton(0);  input.GetMouseDelta();  input.GetMousePosition();  input.GetScroll()
input.SetCursorLocked(true)
local origin, direction = screen.Ray()         -- through the mouse (nil without a camera)
time.delta;  time.time;  time.frame;  time.scale = 0.5
task.spawn(function() task.wait(2) print("2 s later") end);  task.delay(1, fn)

-- Messages and scenes
other:Send("Damage", 10);  scene.Broadcast("OnRoundOver")
scene.Load("/Scenes/Level2.scene");  scene.Reload()

-- Light, effects, debug drawing, save data
lighting.sunElevation = 10;  lighting.exposure = 2;  effects.Set("Bloom", "intensity", 2)
gizmos.Line(a, b, vec3(1, 0, 0));  gizmos.Sphere(p, 1);  gizmos.Text(p, "here")
save.Set("best", 120);  local best = save.Get("best", 0)

-- Physics
local hit = physics.Raycast(origin, direction, 100, { ignore = e })
physics.OverlapSphere(center, radius);  physics.SphereCast(origin, radius, direction)
physics.AddForce(e, f);  physics.AddImpulse(e, i);  physics.SetVelocity(e, v)
physics.Teleport(e, position, rotation);  physics.GetVelocity(e)
physics.GetVehicleInfo(e);  physics.GetCharacterInfo(e);  physics.SetRagdollMode(e, "Limp")

-- Meshes and materials
local tri = mesh.new{ positions = { vec3(0, 0, 0), vec3(0, 1, 0), vec3(1, 0, 0) } }
e:SetMesh(tri)
local m = e:GetMaterial();  m.color = vec3(1, 0, 0);  m.roughness = 0.2
e:SetMaterial(material.new{ color = vec3(0, 0, 1), metallic = 1 })

-- Animation
local anim = e:Get("Animator")
anim:SetFloat("speed", 3);  anim:SetBool("grounded", true);  anim:SetTrigger("jump")
anim:SetVector("lookTarget", point);  anim:CrossFade("Run", 0.2);  anim:CrossFadeState("Jump")
```
