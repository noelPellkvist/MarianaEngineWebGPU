---
title: lighting and effects
nav: lighting and effects
description: Changing the sun, the sky, exposure and post effects (bloom, ambient occlusion, tonemapping) from Mariana scripts.
---

# lighting and effects

Scripts can change the scene's light and the render pipeline's effects while the game plays: a day-night cycle, lightning, a flash when hit, dimming for a menu. In the editor, what scripts change is put back at **Stop**.

## lighting {#lighting}

The `lighting` global is the scene's [Lighting](../../manual/rendering.md#the-sun-and-the-sky): read and write its fields.

| Field | Type | |
|---|---|---|
| `lighting.sunAzimuth` | number | Degrees around the vertical where the sun stands (0 = +Z, 90 = +X) |
| `lighting.sunElevation` | number | Degrees above the horizon |
| `lighting.sunColor` | vector | Its colour (red, green, blue, 0 to 1) |
| `lighting.sunIntensity` | number | Its strength (1 = normal) |
| `lighting.skyIntensity` | number | Light from the sky (1 = normal) |
| `lighting.reflectionIntensity` | number | The sky reflected by glossy surfaces |
| `lighting.skyRotation` | number | Degrees the sky (its light, reflections and background) is turned |
| `lighting.exposure` | number | The camera's exposure: higher is brighter |

```luau
-- Lightning: a bright flash that fades
lighting.skyIntensity = 4
task.spawn(function()
    for i = 1, 20 do
        lighting.skyIntensity = 1 + 3 * (1 - i / 20)
        task.wait()
    end
end)
```

Moving the sun moves its shadows, which are drawn again the frames it moves. See [Day and night](../cookbook.md#day-and-night).

## effects {#effects}

The `effects` global reaches the effects of the render pipeline the game runs with (GTAO, Bloom, Tonemap in the default one; see [Post effects](../../manual/rendering.md#post-effects)). Effects are named as in the Render Pipeline window, or by the start of the name or a word in it, ignoring case: `"bloom"`, `"Tonemap"`, `"GTAO"`.

| Function | |
|---|---|
| `effects.Get(effect, parameter) -> any` | A parameter's value: a number, a boolean, or a vector for colours and vectors |
| `effects.Set(effect, parameter, value)` | Changes a parameter |
| `effects.SetEnabled(effect, on)` | Switches an effect on or off |
| `effects.IsEnabled(effect) -> boolean` | Whether it's on |
| `effects.Names() -> { string }` | The effects in the pipeline, in order |

```luau
effects.Set("Bloom", "intensity", 2.5)    -- dreamy
effects.SetEnabled("GTAO", false)         -- a cheaper look on slow machines
```

Parameter names are the ones in the effect's file (and the Render Pipeline window): Bloom has `intensity` and `threshold`. An effect or parameter that isn't there is an error saying so; check `effects.Names()` first when a script should work with any pipeline.
