# Mariana scripting (Luau)

Game code is written in [Luau](https://luau.org) (Lua 5.1 with types, `+=`, string interpolation
and a fast `vector` type). A script is a `.luau` (or `.lua`) file in the project; entities get
scripts through their **Scripts** component (inspector: Add Component > Scripting > Scripts, then
Add Script, or drop a file from Assets). Scripts run while the game plays (the editor's Play, the
player from the start), natively with Luau's code generator and on the web with its interpreter.

## A script

A script returns a table. Its functions run for each entity that has the script, each with a `self`
of its own; `self.entity` is the entity. `properties` names the values the inspector edits per
entity (numbers, booleans, strings and vectors), with their defaults; they are put into `self`
before `Start`.

```lua
--!strict
local Spin = {} :: Script

Spin.properties = {
    speed = 90,              -- degrees per second
    axis = vec3(0, 1, 0),
}

function Spin.Start(self: Script)
    self.turned = 0
end

function Spin.Update(self: Script, dt: number)
    self.entity:Rotate(self.axis, self.speed * dt)
    self.turned += self.speed * dt
end

return Spin
```

`:: Script` and `self: Script` give the type checker `self.entity` (see Types below);
`function Spin:Update(dt)` runs the same, but what it does with `self` isn't checked.

| Function | When |
|---|---|
| `Start()` | Once, the first frame the script runs (every new script starts before any updates) |
| `Update(dt)` | Every frame the game moves, before animation and physics (`dt` in game seconds) |
| `LateUpdate(dt)` | After physics and the follow cameras |
| `OnContactBegin(other, contact)` | A body of this entity began touching `other`; `contact` = `{ point, normal, speed }`, the normal pointing from this entity to `other` |
| `OnContactEnd(other)` | They stopped touching |
| `OnTriggerEnter(other)` / `OnTriggerExit(other)` | A trigger collider and a body met / parted (both sides get it) |
| `OnAnimationEvent(name, clip)` | The entity's Animator passed an event of one of its clips (`"Footstep"` at 0.4 s of `"Walk"`), after physics and before `LateUpdate`. Events are set on the clips in the inspector's Animator section (or the graph's clips in the Animation Graph window); a clip fires its events when it makes up at least a quarter of the pose, and an event name once per frame |
| `OnDestroy()` | The entity was destroyed, the script removed, or the game stopped |

Scripts on an entity run in their order, entities in id order. A script that raises an error is
stopped (the error is logged and shown in the inspector) until its file changes; a call that runs
for more than 2 s (an endless loop) is stopped the same way. Saving a script while the game runs
reloads it: running instances keep their `self` and take the new functions.

Globals are read-only (each script has globals of its own); the standard libraries are Luau's
(`math`, `string`, `table`, `vector`, `bit32`, `buffer`, `coroutine`, `utf8`, `os.clock` and
friends), plus `print` and `warn` (to the log, with the script and line).

## Types, errors and autocomplete

Scripts are checked against the API's types by Luau's type checker: what it finds shows in the
inspector next to the script and, as you type, in the editor's Script Editor (Window > Script Editor,
or double-click a script in Assets: syntax colours, problems underlined, autocomplete with Ctrl+Space
or after `.` `:` `"`, Ctrl+S saves and a running game takes the new code). A type problem doesn't stop
the script from running. A script is checked
loosely unless its first line is `--!strict`; the Sandbox's scripts are strict.
The types are `Script`, `Entity`, `quat`, `Contact`, `RaycastHit`, `QueryOptions`, `BodyInfo`,
`VehicleInfo`, `CharacterInfo` and one per component (`RigidBody`, `Vehicle`, ...), so
`entity:Get("RigidBody")` is a `RigidBody?` and a misspelt component, field or enum value is an
error.

In VS Code: install the "Luau Language Server" extension and open the project's folder. The editor
writes the types to `.mariana/types/Mariana.d.luau` (again whenever the components change) and, when
the project has none, a `.vscode/settings.json` pointing the extension at them; you get the same
errors and autocomplete there.

## Values

- `vec3(x, y, z)` makes a vector (Luau's own type: `v.x`, `+ - * /` with vectors and numbers,
  `vector.magnitude`, `vector.normalize`, `vector.dot`, `vector.cross`, `vector.lerp`, ...).
- `quat`: rotations. `quat.identity`, `quat.euler(x, y, z)` (degrees, or a vector),
  `quat.angleAxis(degrees, axis)`, `quat.lookRotation(forward, up?)` (+Z to `forward`),
  `quat.fromToRotation(a, b)`, `quat.slerp(a, b, t)`, `quat.angle(a, b)`, `quat.new(x, y, z, w)`;
  `q * q`, `q * v` (turns `v`), `q.x`/`y`/`z`/`w`, `q:Inverse()`, `q:ToEuler()` (degrees).

## Entity

| Member | |
|---|---|
| `id`, `name` | its id; its name (settable) |
| `parent` | settable (`nil` = a root); the local transform stays as it is |
| `position`, `rotation`, `scale` | local transform (settable; `scale` takes a number too) |
| `worldPosition`, `worldRotation` | settable |
| `forward`, `right`, `up` | its +Z, +X and +Y in the world |
| `IsValid()` | false once destroyed |
| `Destroy()` | destroys it and everything below it once the scripts of this frame have run |
| `Has(name)`, `Get(name)`, `Add(name, fields?)`, `Remove(name)` | components by name (below) |
| `GetScript(name)` | another script's `self` on this entity, by its file name (`"Health"`), or nil |
| `GetChildren()`, `FindChild(name)` | its children; the first entity below it with that name |
| `Rotate(axis, degrees)` | turns it about its own axis |
| `Translate(v)` | moves it along its own axes |
| `LookAt(target, up?)` | turns its +Z toward a point or an entity |

Entities are the same value while a script holds on to them, so they work as table keys.

## Components by name

Every component a scene file saves can be reached by its name, with the fields it is saved with
(enums by name): `RigidBody`, `BoxCollider`, `SphereCollider`, `CapsuleCollider`,
`CylinderCollider`, `MeshCollider`, `Joint`, `Vehicle`, `VehicleWheel`, `CharacterController`,
`WaterVolume`, `Ragdoll`, `SoftBody`, `CameraComponent`, `Animator`, `LocalTRS`, ... A vehicle's driver (`throttle`, `steering`, `brake`, `handBrake`) and a
character's walk (`move`, `run`, `jump`) are there too, although files don't keep them.

```lua
local body = self.entity:Add("RigidBody", { motion = "Dynamic", mass = 20 })
body.gravityFactor = 0.5
self.entity:Get("Vehicle").throttle = 1
if self.entity:Has("Ragdoll") then self.entity:Get("Ragdoll").mode = "Limp" end
```

A field that doesn't exist is an error (`pcall` it to try). The Animator also has methods:
`SetFloat(name, v)`, `SetBool`, `SetTrigger`, `ResetTrigger`, `GetFloat`, `GetBool`,
`SetVector(name, position)` and `GetVector(name)` (a graph's Vector parameters: world positions its
Two Bone IK and Look At nodes reach for or look at),
`Play(clip, loop?)`, `CrossFade(clip, seconds?, loop?)` (clips by name or index),
`CrossFadeState(state, seconds?)` and `GetRootVelocity()` (with `rootMotion = true` the clips' walk
and turn move the entity, or the character it is on, instead of the hips: this returns the walk in
m/s along the model's own axes and the turn in degrees per second).

## scene

| | |
|---|---|
| `scene.Find(name)` | the entity with that name (the smallest id when several have it), or nil |
| `scene.Create(name?, parent?)` | an empty entity |
| `scene.Spawn(path, position?, rotation?)` | an instance of a model (`"/Models/Rumba.glb"`) |
| `scene.SpawnPrimitive(shape, position?, color?, rotation?)` | `"cube"`, `"sphere"`, `"plane"`, `"cylinder"` or `"capsule"`, with a material of its own |
| `scene.Each(component, fn)` | `fn(entity)` for every entity with the component, in id order |
| `scene.Destroy(entity)` | as `entity:Destroy()` |

## input

Keys by name, case-insensitive: `"a"`..`"z"`, `"0"`..`"9"`, `"f1"`..`"f12"`, `"space"`,
`"enter"`, `"escape"`, `"tab"`, `"backspace"`, `"left"`, `"right"`, `"up"`, `"down"`, `"shift"`,
`"control"`, `"alt"` (either side; `"leftShift"` and so on for one), `"pageUp"`, `"home"`, ...

| | |
|---|---|
| `input.GetKey(name)` | held |
| `input.GetKeyDown(name)` / `input.GetKeyUp(name)` | pressed / released this frame |
| `input.GetMouseButton(i)`, `GetMouseButtonDown(i)`, `GetMouseButtonUp(i)` | 0 left, 1 right, 2 middle |
| `input.GetMousePosition()`, `GetMouseDelta()`, `GetScroll()` | vectors (x, y, 0) |

In the editor keys read as up while a text field has them or the right mouse button flies the camera.

## time

`time.delta` (this frame's game seconds), `time.time` (since Play), `time.frame`, `time.scale`
(settable: 0.5 is slow motion).

## physics

| | |
|---|---|
| `physics.Raycast(origin, direction, maxDistance?, options?)` | the nearest `{ entity, point, normal, distance }`, or nil |
| `physics.RaycastAll(...)` | every hit, nearest first |
| `physics.SphereCast(origin, radius, direction, maxDistance?, options?)` | as Raycast |
| `physics.OverlapSphere(center, radius, options?)`, `OverlapBox(center, halfExtents, rotation?, options?)` | the entities touched |
| `AddForce(e, f)`, `AddForceAtPoint(e, f, p)`, `AddTorque(e, t)` | through the next frame's steps |
| `AddImpulse(e, i)`, `AddImpulseAtPoint(e, i, p)`, `AddAngularImpulse(e, i)`, `AddRagdollImpulse(e, i)` | once |
| `SetVelocity(e, linear, angular?)`, `GetVelocity(e)` | `GetVelocity` returns linear, angular (nil if not a dynamic body) |
| `Teleport(e, position, rotation?)`, `WakeUp(e)`, `IsTouching(a, b)` | |
| `GetBodyInfo(e)`, `GetVehicleInfo(e)`, `GetCharacterInfo(e)` | tables (motion, mass, velocities; speed, rpm, gear, wheels; grounded, ground, ...) or nil |
| `SetRagdollMode(e, "Animated" \| "Powered" \| "Limp")`, `WaterAt(point)` | `WaterAt` returns the surface point and normal, or nil |

`options` = `{ ignore = entity, triggers = true, layers = mask }`.

## Meshes, materials and textures

| | |
|---|---|
| `mesh.new{ positions, normals?, uvs?, colors?, indices? \| submeshes?, topology?, smooth?, dynamic? }` | a Mesh: attributes as tables of vectors or buffers of floats; indices from 1 in tables, from 0 in buffers of u32; no normals = flat (or `smooth = true`) |
| `m:Update{ positions?, normals?, uvs?, colors? }` | a dynamic mesh's vertices again (as many) |
| `m.vertexCount`, `m.boundsMin`, `m.boundsMax`, `m.dynamic` | |
| `entity:SetMesh(m, materials?)`, `entity:GetMesh()` | draw a mesh (a Mesh Renderer, casting shadows) |
| `entity:GetMaterial(slot?)` | the material in a slot (from 1): the entity's own copy the first time, so only it changes |
| `entity:SetMaterial(material, slot?)`, `material.new(fields?)`, `mat:Clone()` | |
| material fields | `color`, `alpha`, `metallic`, `roughness`, `emissive`, `normalStrength`, `occlusionStrength`, `clearcoat`, `clearcoatRoughness`, `doubleSided`, `alphaMode` ("Opaque" / "Mask" / "Blend"), `alphaCutoff`, `tiling`, `offset`, `baseColorMap`, `normalMap`, `metallicRoughnessMap`, `occlusionMap`, `emissiveMap`, `name` (read) |
| `texture.load(path)` | an image by resource path, loaded in the colour space of the slot it is put in |

What scripts make or change this way lasts while the game runs: Stop puts the scene back.

## Control scripts (in the Sandbox)

Game controls are scripts too: the engine gives vehicles, characters and cameras, and these decide
how the keys drive them. They belong to the standard project, not the engine: they live in
`projects/Sandbox/Scripts/` and are found as `/Scripts/<name>.luau`. Another project copies them
into its own `Scripts/` folder (and changes them as it likes).

| Script | Goes on | What it does |
|---|---|---|
| `VehicleDriver.luau` | an entity with a `Vehicle` | W / Up throttle, S / Down brakes until it has (nearly) stopped and then backs up, A D / Left Right steer, Space the hand brake. `steerSpeed`, `steerFade` (less lock the faster it goes). |
| `CharacterDriver.luau` | an entity with a `CharacterController` | W A S D / arrows walk relative to the scene's first camera, Shift runs, Space jumps. |
| `ChaseCamera.luau` | a camera entity | Follows `target` (a name; empty = the first entity with a VehicleDriver or CharacterDriver) from `distance` behind and `height` above, catching up at `stiffness` per second. |

The editor's Create > Vehicle and Create > Physics > Character put the drivers on what they make and
add a chase camera; the player gives its presets the same. Both look for them at those paths in the
project, so a project without them gets vehicles nothing drives (and an error in the log).

## Examples

`projects/Sandbox/Scripts/` has `Respawn.luau` (R puts the car back at the start), `LaunchPad.luau`
(a trigger that throws what comes onto it into the air) and `Hover.luau` (bobbing and turning), all
used in the Sandbox's Playground scene.
