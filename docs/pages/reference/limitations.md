---
title: Current Limitations
description: What the Mariana engine doesn't do yet, so you can plan around it.
---

# Current Limitations

Mariana is young. This page lists what it doesn't do yet, so you know before you start a project what you'd need to work around.

## Platforms

- The editor runs on **Windows** and in the **browser**. Games build for **Windows** and the **web**.
- macOS, Linux, mobile and consoles aren't supported yet.

## Not in the engine yet

| | |
|---|---|
| **Audio** | There is no sound or music playback yet |
| **Game UI** | No text, buttons or HUD for games: scripts can only `print` to the log |
| **Gamepads** | Keyboard and mouse only |
| **Lights** | One sun and the sky light. No point or spot lights yet; glowing (emissive) materials don't light their surroundings |
| **Saving game data** | Scripts can't write files (high scores, save games) |
| **Multiplayer** | Not yet available to games (a networking library is in development) |
| **Scene loading from scripts** | A game runs one scene; scripts can't switch scenes yet |

## Scripting

- Scripts can't put scripts on entities they create, or copy an existing entity. Have one script drive what it spawns (see [Spawning things that have scripts](../scripting/api/scene.md#spawning-things-that-have-scripts)).
- Fields that are assets (meshes, materials, textures, animation clips and graphs) can't be read or set from scripts, so a script can't change a material's colour after spawning. A primitive's colour is chosen when it's spawned.
- There is no `require`: scripts reach each other through [`entity:GetScript`](../scripting/api/entity.md#getscript).
- There is no `wait` or yielding: spread work over frames with timers in `self`.

## Assets

- Models: **glTF 2.0** only (`.glb`, `.gltf`). Convert other formats in Blender.
- Images: PNG and JPEG (TGA and BMP load too), through glTF files or as material textures.
- glTF features not rendered: morph targets (shape keys), iridescence, material variants, clearcoat textures, sheen. Transmissive (glass) materials are drawn see-through but don't refract.
- Animations play on any model with the same bone names and proportions, but aren't retargeted between different proportions.

## Rendering

- See-through (Blend) surfaces cast no shadows, don't take part in ambient occlusion, can't be clicked to select in the Scene view (use the Hierarchy) and are sorted per object, not per triangle.
- Shadows reach a set distance from the camera (80 m by default; the Render Pipeline window changes it).

## Physics

- Resting bodies sink up to 5 mm into what they stand on (a dropped 1 m ball rests at 0.495 m).
- No height fields for terrain yet: use a mesh collider.
- Soft bodies don't float and make no contact events.

## Editor

- One entity is selected at a time; there is no copy, paste or duplicate for entities yet.
- Your own prefabs: a "prefab" is a model file. Groups of entities can't be saved as reusable prefabs yet.
- Open Recent remembers scenes for the current session only (a project does reopen in the scene you had open last).
- On the web, the editor's files live in the page: saving also downloads the file, and everything is gone when the page reloads.
- Undoing an edit back to the saved state still marks a `.material` or `.animgraph` file as modified.
- A scene can make about 131,000 entities with transforms in one run: those destroyed while playing don't give their slot back until the scene starts again (Stop, or opening it). Reuse projectiles and effects (move them back into a pool) instead of creating new ones for hours on end.
