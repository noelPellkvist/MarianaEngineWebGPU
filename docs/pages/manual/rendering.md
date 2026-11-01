---
title: Lighting, Cameras and Rendering
nav: Rendering
description: Cameras, the sun and the sky, shadows, exposure and the post effects of a render pipeline.
---

# Lighting, Cameras and Rendering

The renderer draws physically based materials lit by a **sun** and the **sky**, with cascaded shadows, ambient occlusion, bloom and tonemapping. Most of it works without any setup; this page shows what you can tune.

![The Render Pipeline window next to the Scene view](images/render-pipeline.jpg "Window > Render Pipeline: exposure, shadows, the sky and every post effect with its settings.")

## Cameras

A **Camera** component makes an entity a camera. It looks along its **+Z** axis with **+Y** up.

| Field | Default | |
|---|---|---|
| **Field of view** | 60 | Vertical, in degrees |
| **Near** | 0.1 | Closest distance drawn, in metres |
| **Far** | 500 | Farthest distance drawn |

The game (the Game window and built games) looks through the **first camera** in the scene. **Create > Camera** puts one where the editor camera is, looking the same way: frame the shot in the Scene view first, then create it.

To move a camera from code, set its entity's `position` and `rotation` (or `worldPosition` and `worldRotation`), or use `entity:LookAt(target)`. The Sandbox's [`ChaseCamera.luau`](../scripting/api/sandbox-scripts.md#chasecamera) is a complete follow camera to copy.

Keep the near plane as far out as you can (0.1 to 0.3 for first-person, 0.5 or more for a top-down view) and the far plane as close as the scene allows: it keeps depth precise and shadows sharp.

## The sun and the sky

**Window > Lighting** edits the scene's light. These settings are saved with the scene.

| Setting | |
|---|---|
| **Sun azimuth** | Where the sun is around the horizon, in degrees (0 is +Z, 90 is +X) |
| **Sun elevation** | How high it is above the horizon |
| **Sun colour** and **intensity** | Direct light (the only light that casts shadows) |
| **Sky diffuse** | How much light the sky gives everything from all around |
| **Sky reflections** | How strongly glossy surfaces reflect the sky |
| **Sky rotation** | Turns the sky (its picture, its light and its reflections) around the vertical axis |

The sky is an HDR cube map. Its light is worked out when the engine starts: bright parts of the sky light things from their direction, and shiny surfaces reflect it, blurrier the rougher they are.

## Shadows

The sun casts **cascaded shadow maps**: four shadow maps, the nearest covering a small area around the camera in detail, the farthest a large area coarsely. Shadows fade out at the shadow distance. In the Render Pipeline window:

| Setting | Default | |
|---|---|---|
| **Shadows** | On | Turn sun shadows off entirely |
| **Shadow distance** | 80 m | How far from the camera shadows reach |
| **Shadow map size** | 2048 | The resolution of each of the four maps |

A shorter shadow distance gives sharper shadows. Turn **Cast Shadows** off in a Mesh Renderer for objects that don't need to cast them (the ground, distant scenery): it saves work.

Only opaque and masked materials cast shadows; blended (see-through) ones, lines and points don't.

## Exposure and tonemapping

The scene is rendered in HDR: light values aren't limited to 0..1. The **exposure** scales them (the Default pipeline uses 1.5) before the **Tonemap** effect compresses them into what a screen can show, without washing out highlights. Raise exposure for dim scenes, lower it for bright ones.

## Post effects

A **render pipeline** is a list of effects, each with its settings. The Default pipeline:

| Effect | Stage | Settings | |
|---|---|---|---|
| **Ambient Occlusion (GTAO)** | Before lighting | `radius` (0.6 m), `power` (1.5) | Darkens creases and contact areas, from the depth buffer |
| **Bloom** | Post process | `intensity` (1), `threshold` (1), `knee` (0.5), `radius` (1) | Glow around bright parts: emissive materials, the sun's highlights |
| **Tonemap** | Final | | HDR to the screen, with exposure |

In **Window > Render Pipeline**:

- Toggle each effect, change its settings (sliders built from the effect's own ranges), reorder or remove it.
- **Add effect** lists every `.effect` file in the project and the engine.
- **Sky** turns the sky background on or off (off gives a transparent background).
- The pipeline file at the top: switch pipelines, **Save** (a `*` shows unsaved changes), **Reload**, or **Save As** a new `.pipeline` in the project's `Pipelines` folder.

The scene remembers which pipeline it uses; Ctrl+S saves a changed pipeline along with the scene. A built game uses the scene's pipeline unless its build profile names another.

You can write your own effects in WGSL: see [Custom Post Effects](../reference/custom-effects.md).

## Performance

- Fewer, bigger meshes draw faster than many small ones. The renderer batches identical meshes with the same material into one draw, so reusing meshes and materials helps.
- Shadows are only redrawn when something in them moves or the sun turns.
- Watch the Profiler's GPU tab to see what each pass costs: see [Profiling](profiler.md).
