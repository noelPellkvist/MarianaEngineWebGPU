---
title: Player.json
description: Player.json, the file a Mariana game reads at start-up: window, start scene, pipeline, camera, and things to spawn.
---

# Player.json

A built game starts by reading `Player.json` from its files. The build writes it from your profile (window, start scene, pipeline, follow camera, logging), on top of a `Player.json` in your project folder if you have one, so you can add the settings below that the Build window doesn't show.

```json title="Player.json"
{
  "window": { "title": "Rally", "width": 1600, "height": 900, "icon": "/Textures/icon.png" },
  "scene": "/Scenes/Track.scene",
  "pipeline": "",
  "camera": { "follow": true, "distance": 8, "height": 3 },
  "profile": false
}
```

Everything is optional. Paths are resource paths: from the project folder, starting with `/`.

## Settings

| Setting | Default | |
|---|---|---|
| `window.title` | `"Mariana Player"` | The window's title |
| `window.width`, `window.height` | 1280, 720 | Its size in pixels |
| `window.icon` | the engine's | A PNG |
| `scene` | none | The scene to load, with its lighting and settings |
| `pipeline` | the scene's | A render pipeline file. Empty = the one the scene was saved with, else `/Pipelines/Default.pipeline` |
| `camera` | | The view when the scene has **no camera** (a scene with a camera entity always shows its first one) |
| `camera.position`, `camera.target` | [0, 2, -5], [0, 1, 0] | A fixed view: where it is and what it looks at |
| `camera.orbitSpeed` | 0 | Degrees per second to circle the target (a turntable for showing a model) |
| `camera.follow` | false | Chase what the keys drive (a vehicle or character), with the Sandbox's ChaseCamera |
| `camera.distance`, `camera.height` | 7, 2.5 | For `follow`: metres behind and above |
| `profile` | false | Log frame times every 5 seconds |
| `spawn` | none | Things to add to the scene at start-up (below) |

## Spawning things

`spawn` lists things to add on top of the scene (or instead of one): models, primitives and ready-made presets. It's handy for quick test games without making a scene.

```json
{
  "spawn": [
    { "preset": "playground" },
    { "preset": "car", "position": [0, 0, 5] },
    { "prefab": "/Models/Rumba.glb", "position": [3, 0, 12], "faceCamera": true, "ragdoll": "powered" },
    { "primitive": "sphere", "color": [1, 0.3, 0.2], "position": [0, 5, 8],
      "physics": { "body": "dynamic", "mass": 2, "bounciness": 0.6 } }
  ],
  "camera": { "follow": true }
}
```

| Field | | |
|---|---|---|
| `prefab` | path | A model (`.glb`, `.gltf`) |
| `primitive` | `cube`, `sphere`, `plane`, `cylinder`, `capsule` | A built-in shape, tinted by `color` ([r, g, b], 0–1) |
| `preset` | see below | A ready-made setup |
| `position` | [x, y, z] | Metres |
| `rotation` | [x, y, z] | Degrees |
| `scale` | number or [x, y, z] | |
| `faceCamera` | boolean | Turn it so its front faces the camera's starting position |
| `physics` | object | Make it a physics body (below) |
| `vehicle` | `"car"` | Rig a car model as a drivable car: its entities named `Wheel...` become its wheels |
| `ragdoll` | `"powered"`, `"animated"`, `"limp"`, or an object | Give an animated model a ragdoll; as an object: `mode`, `mass` (80), `limpOnImpact` (4 m/s), `knockback`, `knockbackLift` (0.5) |

### Presets

| Preset | |
|---|---|
| `car`, `motorcycle`, `tank` | A vehicle driven with W A S D and Space |
| `playground` | A 200 m ground with jumps, ramps, logs, a banked turn and a wall of crates |
| `character` | A character walked with W A S D, Shift and Space |
| `course` | Stairs, a step, slopes and crates to walk a character over |
| `chain`, `door`, `spinner`, `wrecking ball` | Joints at work |
| `pool` | A water pool with floating crates, a raft and a ramp to drive in |
| `ragdoll` | The dancing Rumba, knocked limp by a hit |
| `flag`, `drape`, `jelly`, `balloon` | Soft bodies |

### `physics`

| Field | Default | |
|---|---|---|
| `body` | `"static"` | `"static"`, `"kinematic"` or `"dynamic"` |
| `collider` | the primitive's shape; else `"mesh"` (static) or `"convex"` (moving) | `box`, `sphere`, `capsule`, `cylinder` (fitted to the model), `mesh` or `convex` (one on each mesh), or `none` |
| `mass` | 1 | kg; 0 = from the colliders' volume |
| `friction` | 0.5 | |
| `bounciness` | 0 | 0 to 1 |
| `velocity` | [0, 0, 0] | m/s at the start |

## Where the game finds its files

- **Windows:** `data/game/` next to the `.exe` is the game's folder, `data/engine/` the engine's.
- **Web:** `game.data` is mounted as the game's folder.

A file in the game's folder overrides the engine's at the same path, so a project can replace an engine shader or effect by putting its own at the same resource path.
