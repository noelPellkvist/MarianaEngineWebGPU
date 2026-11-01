---
title: Custom Effects
description: Writing your own post-processing effects for Mariana in WGSL: effect files, passes, parameters, textures and the scene textures you can read.
---

# Custom Effects

Post effects (ambient occlusion, bloom, tonemapping) are not built into the renderer: each is an **effect file** that names its shaders, parameters and passes. You can write your own the same way, and they show up in the Render Pipeline window's **Add effect** list next to the engine's.

An effect is two kinds of file in your project:

| File | Where | What |
|---|---|---|
| `Vignette.effect` | `<project>/Effects/` | JSON: the effect's name, stage, parameters, textures and passes |
| `vignette.wgsl` | anywhere, e.g. `<project>/Shaders/Effects/` | A WGSL fragment shader per pass |

## A first effect: vignette

```json title="Effects/Vignette.effect"
{
  "name": "Vignette",
  "stage": "PostProcess",
  "parameters": {
    "strength": { "type": "float", "default": 0.5, "min": 0, "max": 1 },
    "radius": { "type": "float", "default": 0.7, "min": 0.1, "max": 1.5 },
    "tint": { "type": "color3", "default": [0, 0, 0] }
  },
  "passes": [
    {
      "name": "Vignette",
      "shader": "/Shaders/Effects/vignette.wgsl",
      "inputs": { "scene": "Input" },
      "output": "Output"
    }
  ]
}
```

```wgsl title="Shaders/Effects/vignette.wgsl"
// Darkens (or tints) the edges of the image. Input: scene.
@fragment
fn fragmentMain(input: FullscreenInput) -> @location(0) vec4f {
    let color = textureSampleLevel(scene, linearClamp, input.uv, 0.0);
    // -1..1 across the screen, corrected for its aspect so the vignette is round
    let aspect = passInfo.outputSize.x * passInfo.outputSize.w;
    let p = (input.uv * 2.0 - 1.0) * vec2f(aspect, 1.0);
    let edge = smoothstep(params.radius, params.radius + 0.6, length(p));
    return vec4f(mix(color.rgb, params.tint, edge * params.strength), color.a);
}
```

Then open **Window > Render Pipeline**, choose **Add effect > Vignette**, and move it up so it runs **before** Tonemap (post effects work on the HDR image; the tonemap turns it into what the screen shows). Its parameters appear with sliders from their `min` and `max`. **Save** writes it into the pipeline file, so the scene and your built game use it.

> [!TIP]
> After editing the shader, press **Save**, then **Reload** in the Render Pipeline window: the pipeline and every effect in it are loaded again from their files (Reload goes back to what the pipeline file says, so save tuning first). A shader that doesn't compile logs the WGSL compiler's message, with the line.

## The effect file

| Key | |
|---|---|
| `name` | Shown in the Render Pipeline window (default: the file's name) |
| `stage` | `"PostProcess"` (default): runs on the lit HDR image, in pipeline order. `"BeforeLighting"`: runs before the scene is lit, to make textures the lighting reads (ambient occlusion writes `AmbientOcclusion`) |
| `final` | `true` for an effect that writes the screen (a tonemapper): it always runs last. Without an enabled final effect, the engine's `Present` effect shows the image with exposure but no tonemapping |
| `parameters` | Values the user can tune (below) |
| `textures` | The effect's own intermediate textures (below) |
| `passes` | One or more passes, run in order |

### Parameters

Each parameter has a `type`, a `default`, and for numbers a `min` and `max` (the slider's range):

| `type` | In WGSL | Default given as |
|---|---|---|
| `float` | `f32` | a number |
| `int` | `i32` | a number |
| `bool` | `u32` (0 or 1) | `true` / `false` |
| `vec2`, `vec3`, `vec4` | `vec2f`, `vec3f`, `vec4f` | an array |
| `color3`, `color4` | `vec3f`, `vec4f` (shown as a colour picker) | an array, 0–1 |

They reach the shader as the uniform `params`, with the names you gave: `params.strength`.

### Textures

An effect with several passes passes its intermediate results through textures of its own:

```json
"textures": {
  "half": { "format": "rgba16float", "scale": 0.5 },
  "lut": { "format": "rgba8unorm", "width": 32, "height": 32 }
}
```

`scale` is a fraction of the screen's size (they're resized with it); `width` and `height` make a fixed size instead. Formats: `r8unorm`, `r16float`, `r32float`, `rg16float`, `rgba8unorm`, `rgba16float` (default), `rgba32float`, `rgb10a2unorm`.

### Passes

| Key | |
|---|---|
| `name` | For the profiler and error messages |
| `shader` | The WGSL file (a resource path) |
| `inputs` | Shader name → texture: each becomes a texture the shader can read under that name |
| `output` | The texture the pass writes |
| `constants` | Up to four numbers the pass's shader reads as `passInfo.constants` (so one shader can serve several passes, like each step of a blur) |

Texture names in `inputs` and `output` are:

| Name | |
|---|---|
| `Input` | The image so far (post effects): the lit scene, or the previous effect's result |
| `Output` | Where a post effect's result goes (the next effect's `Input`, or the screen for a final effect) |
| Your `textures` | By the names you gave them |
| `SceneColor` | The lit scene in HDR (`rgba16float`), before any post effect |
| `SceneDepth` | The depth buffer (read as a `texture_depth_2d`) |
| `SceneNormals` | View-space normals, stored as `normal * 0.5 + 0.5` (`rgb10a2unorm`; decode with `xyz * 2 - 1`) |
| `AmbientOcclusion` | What a BeforeLighting effect wrote for the lighting (`r8unorm`, 1 = unoccluded) |

## The shader

Each pass's shader defines only its fragment entry point:

```wgsl
@fragment
fn fragmentMain(input: FullscreenInput) -> @location(0) vec4f { ... }
```

The engine adds, before your code:

| Name | Type | |
|---|---|---|
| `input.uv` | `vec2f` | 0..1 across the output, (0, 0) at the top left |
| `input.position` | `vec4f` | The pixel's position (`.xy` in pixels) |
| `params` | your parameters | |
| `passInfo.outputSize` | `vec4f` | `xy` the output's size in pixels, `zw` one over it |
| `passInfo.constants` | `vec4f` | The pass's `constants` |
| `camera` | struct | `projection`, `view`, `viewProj`, `invView`, `invProj`, `invViewProj` (`mat4x4f`), `position` (`vec3f`) and `exposure` (`f32`) of the camera being drawn |
| `linearClamp` | `sampler` | Linear filtering, clamped at the edges |
| one variable per input | `texture_2d<f32>` (or `texture_depth_2d`) | Named as in `inputs` |

Read inputs with `textureSampleLevel(name, linearClamp, uv, 0.0)` (filtered) or `textureLoad(name, vec2i(input.position.xy), 0)` (exact pixels; a depth texture returns an `f32`).

> [!IMPORTANT]
> Post effects run on **linear HDR** colour: values above 1 are normal (the sun, emissive surfaces). Don't clamp or gamma-correct in your effect; the final tonemap does that. Exposure is the camera's (`camera.exposure`), applied by the tonemap.

## Overriding the engine's effects

The engine's effects live at `/Effects/GTAO.effect`, `/Effects/Bloom.effect`, `/Effects/Tonemap.effect` and `/Effects/Present.effect`, with their shaders in `/Shaders/Effects/`. A file in your project at the same resource path is used instead of the engine's: copy `Tonemap.effect` and its shader into your project to grade your game's colours your own way, and every pipeline that uses it picks yours.
