---
title: File Formats
description: The files a Mariana project is made of: scenes, materials, animation graphs, render pipelines, effects, physics settings, build profiles and scripts.
---

# File Formats

Everything a Mariana project keeps is a text file (JSON, or Luau for scripts), so it works with version control: changes show up as readable diffs and merge like code. You rarely need to edit them by hand (the editor writes them all), but it helps to know what they hold.

| Extension | What | Written by |
|---|---|---|
| `.scene` | A scene: entities, components, settings | Save Scene |
| `.material` | A material and its textures | The material editor's Save / Extract materials |
| `.animgraph` | An animation graph | The Animation Graph window |
| `.pipeline` | A render pipeline: exposure, shadows, post effects | The Render Pipeline window |
| `.effect` | A post effect: its passes and parameters | You (see [Custom Effects](custom-effects.md)) |
| `.luau` | A script | You |
| `Physics.json` | The project's physics settings and collision layers | The Physics window |
| `BuildProfiles.json` | The project's build profiles | The Build window |
| `Player.json` | What a built game starts with | The build (see [Player.json](../building/player-config.md)) |

Paths inside these files are [resource paths](../getting-started/projects.md#resource-paths) (from the project folder, starting with `/`), except where a file refers to something next to it (a material's images, a graph's clip files), which may be relative to the file.

In every format, **missing values keep their defaults**, so a hand-written file only needs what it changes.

## Scene files {#scene-files}

```json title="Scenes/Arena.scene"
{
  "version": 1,
  "dependencies": [ "/Models/Rumba.glb", "/Scripts/Spin.luau" ],
  "settings": {
    "lighting": { "sunAzimuth": 40, "sunElevation": 35, "sunColor": [1, 0.95, 0.9], "sunIntensity": 3,
                  "skyIntensity": 1, "reflectionIntensity": 1, "skyRotation": 0 },
    "pipeline": "/Pipelines/Default.pipeline"
  },
  "materials": [ { "name": "Floor Material", "baseColor": [0.3, 0.4, 0.3, 1], "roughness": 1 } ],
  "materialOverrides": [ { "material": "/Models/Rumba.glb#material/0", "baseColor": [1, 0.2, 0.2, 1] } ],
  "entities": [
    {
      "id": 158,
      "name": "Launch Pad",
      "components": {
        "LocalTRS": { "position": [-10, 0, 20], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1] },
        "BoxCollider": { "center": [0, 0.75, 0], "size": [3, 1.5, 3], "trigger": true },
        "Scripts": { "scripts": [ { "script": "/Scripts/LaunchPad.luau", "enabled": true, "values": [] } ] }
      }
    },
    { "id": 159, "name": "Marker", "parent": 158, "components": { "...": {} }, "tags": [ "ShadowCasterTag" ] }
  ]
}
```

| Key | |
|---|---|
| `version` | The format's version |
| `dependencies` | Every file the scene uses (what a build packages). Rewritten on every save |
| `settings` | The scene's lighting (sun, sky) and render pipeline |
| `materials` | Materials that exist only in this scene (a primitive's own, a "Make unique" copy), referred to from components as `#material/<index>` |
| `materialOverrides` | Changes the scene made to materials inside model files |
| `entities` | Every entity, parents before children: `id` (only to link `parent`s within the file), `name`, `parent`, `components` by name with their fields (see the [Component Reference](../scripting/api/component-reference.md)), and `tags` |

Asset references in components:

| Reference | |
|---|---|
| `/Models/Car.glb#mesh/3` | Something inside a file: its `mesh`, `material`, `image`, `skin` or `animation` by index, or its `#skeleton` |
| `/Materials/Steel.material` | A whole file |
| `/Textures/rock.png#srgb` | An image as a colour (`#srgb`) or data (`#linear`) texture |
| `builtin://mesh/cube` | A built-in asset |
| `#material/2` | One of the scene file's own `materials` |

Saving the same scene again gives the same file, byte for byte, so a diff shows only real changes.

## Materials {#materials}

```json title="Materials/Brick.material"
{
  "baseColor": [1, 1, 1, 1],
  "metallic": 0,
  "roughness": 0.8,
  "normalStrength": 1,
  "occlusionStrength": 1,
  "emissive": [0, 0, 0],
  "alphaMode": "Opaque",
  "alphaCutoff": 0.5,
  "doubleSided": false,
  "clearcoat": 0,
  "clearcoatRoughness": 0,
  "textures": {
    "baseColor": { "image": "brick_albedo.png", "scale": [4, 4] },
    "normal": { "image": "brick_normal.png", "scale": [4, 4] },
    "metallicRoughness": { "image": "/Textures/brick_orm.png", "uvSet": 0 }
  }
}
```

| Key | |
|---|---|
| `baseColor` | RGBA, 0–1, multiplied with the base colour texture |
| `metallic`, `roughness` | 0–1, multiplied with the metallic-roughness texture (blue = metal, green = roughness, as glTF) |
| `normalStrength`, `occlusionStrength` | How strongly the normal and occlusion maps apply |
| `emissive` | RGB; can go above 1 for glowing surfaces |
| `alphaMode` | `Opaque`, `Mask` (cut out below `alphaCutoff`) or `Blend` (see-through) |
| `doubleSided` | Draw both sides |
| `clearcoat`, `clearcoatRoughness` | A varnish layer on top (car paint) |
| `textures` | Per slot (`baseColor`, `normal`, `metallicRoughness`, `occlusion`, `emissive`): `image` (relative to the file, or a resource path), `uvSet` (0 or 1), `wrapU`/`wrapV` (`Repeat`, `Clamp`, `MirrorRepeat`), `magFilter`/`minFilter`/`mipFilter` (`Linear`, `Nearest`), `mipmaps`, and the texture transform `offset`, `rotation` (degrees), `scale` |

## Animation graphs {#animation-graphs}

```json title="Animations/Hero.animgraph"
{
  "parameters": [
    { "name": "speed", "type": "float", "default": 0 },
    { "name": "grounded", "type": "bool", "default": true },
    { "name": "jump", "type": "trigger" },
    { "name": "lookTarget", "type": "vector" }
  ],
  "clips": [ { "name": "Run", "file": "../Models/Run.glb", "animation": 0 } ],
  "nodes": [
    { "name": "Idle", "type": "clip", "clip": "Idle", "loop": true },
    { "name": "Run", "type": "clip", "clip": "Run", "loop": true },
    { "name": "Move", "type": "blend1d", "parameter": "speed",
      "inputs": [ { "node": "Idle", "threshold": 0 }, { "node": "Run", "threshold": 5 } ] },
    { "name": "Jump", "type": "clip", "clip": "Jump", "loop": false },
    { "name": "Body", "type": "stateMachine", "entry": "Move",
      "states": [ { "name": "Move", "node": "Move" }, { "name": "Jump", "node": "Jump" } ],
      "transitions": [
        { "from": "Move", "to": "Jump", "duration": 0.1, "conditions": [ { "parameter": "jump", "mode": "if" } ] },
        { "from": "Jump", "to": "Move", "duration": 0.2, "exitTime": 0.9 }
      ] },
    { "name": "Look", "type": "lookAt", "inputs": [ "Body" ], "bone": "Head", "chainLength": 3,
      "angleLimit": 70, "target": "lookTarget", "weight": 1 }
  ],
  "output": "Look"
}
```

- **Parameter types:** `float`, `bool`, `trigger`, `vector`.
- **Node types:** `clip`, `blend`, `blend1d`, `blend2d`, `additive`, `layer`, `stateMachine`, `twoBoneIK`, `lookAt`. Nodes refer to each other by name.
- **Clips:** clip nodes name a clip of the Animator's model, or one the graph brings in `clips` (a glTF file, relative to the graph or a resource path, and its animation by index or by `"clip"` name).
- **Transitions:** a transition without `from` goes from any state. `exitTime` is the normalized time of the state it leaves (1 = the end of its first loop). Condition modes: `if`, `ifNot`, `greater`, `less`, `equals`, `notEquals`.
- **Layout:** nodes and states may have a `position` (where the graph window draws them).

The [Animation](../manual/animation.md) page explains what each node does.

## Render pipelines {#pipelines}

```json title="Pipelines/Night.pipeline"
{
  "name": "Night",
  "exposure": 0.8,
  "scene": { "shadows": true, "shadowDistance": 60, "shadowMapSize": 2048, "sky": true },
  "effects": [
    { "effect": "/Effects/GTAO.effect", "enabled": true, "parameters": { "radius": 0.6, "power": 1.5 } },
    { "effect": "/Effects/Bloom.effect", "enabled": true, "parameters": { "intensity": 2, "threshold": 1 } },
    { "effect": "/Effects/Tonemap.effect", "enabled": true }
  ]
}
```

`effects` run in order within their stage. Parameters missing from the file take the effect's defaults; a parameter the effect doesn't have is ignored with a warning.

## Physics settings {#physics-json}

```json title="Physics.json"
{
  "gravity": [0, -9.81, 0],
  "fixedTimestep": 0.016667,
  "maxStepsPerFrame": 4,
  "collisionSteps": 1,
  "interpolation": true,
  "penetrationSlop": 0.005,
  "speculativeContactDistance": 0.02,
  "layers": [
    { "name": "Default", "collidesWith": [ "Default", "Debris" ] },
    { "name": "Debris", "collidesWith": [ "Default" ] }
  ]
}
```

The layers are the [collision layers](../manual/physics.md#collision-layers): up to 32, numbered from 0 in this order. Two static bodies never collide, whatever the layers say.

## Build profiles {#build-profiles}

```json title="BuildProfiles.json"
{
  "active": "Windows",
  "profiles": [
    {
      "name": "Windows",
      "platform": "Windows",
      "configuration": "Release",
      "output": "",
      "gameName": "Playground",
      "window": { "title": "Mariana Playground", "width": 1280, "height": 720, "icon": "" },
      "scene": "/Scenes/Playground.scene",
      "scenes": [],
      "pipeline": "",
      "followCamera": true,
      "logProfile": false,
      "wholeProject": false
    }
  ]
}
```

`platform` is `Windows` or `Web`, `configuration` `Release` or `Debug`. `output` is relative to the project (empty = `Builds/<name>`). See [Build Profiles](../building/build-profiles.md).

## Scripts

`.luau` files are [Luau](../scripting/luau.md) modules. A script that starts with `--!strict` is checked strictly; see [Type checking](../scripting/script-editor.md#type-checking).

## The `.mariana` folder

The editor keeps its own files in `<project>/.mariana/`: thumbnails, each scene's editor camera, saved profiler traces, the script type definitions and this computer's build tool paths. It can be deleted at any time (the editor makes it again) and shouldn't go into version control.
