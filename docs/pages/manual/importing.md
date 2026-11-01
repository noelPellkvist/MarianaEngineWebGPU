---
title: Importing Assets
nav: Importing assets
description: Bring models, animations and textures into a Mariana project. What glTF features are supported, and tips for Blender and Mixamo.
---

# Importing Assets

There's no import step: copy a file into the project's folder and the editor sees it. The Assets window shows it with a thumbnail, and you can drag it into the scene at once.

## Models: glTF 2.0

Models are **glTF 2.0** files, either `.glb` (one binary file) or `.gltf` (JSON with separate `.bin` buffers and images). glTF is the open standard every major 3D tool exports: Blender, Maya, 3ds Max, Cinema 4D, Substance, Sketchfab downloads and most asset stores.

| Feature | Supported |
|---|---|
| Node hierarchy | Yes: each node becomes an entity, in the same tree |
| Meshes | Triangles, lines and points, strips and fans; normals, tangents (generated when missing), two UV sets, vertex colours |
| Materials | Metallic-roughness PBR: base colour, metallic, roughness, normal, occlusion and emissive maps and factors |
| Alpha | Opaque, Mask (cut-out, with alpha to coverage) and Blend (see-through) |
| Double-sided materials | Yes |
| `KHR_materials_emissive_strength` | Yes (HDR emissive, for glowing parts that bloom) |
| `KHR_materials_clearcoat` | Factor and roughness (not the clearcoat textures) |
| `KHR_materials_transmission` | Approximated: transmissive materials are drawn see-through (blended) with their reflections, not refracted |
| `KHR_texture_transform` | Yes: tiling, offset, rotation, and its UV set |
| Skins | Yes: skinned meshes are animated on the GPU |
| Animations | Yes: translation, rotation and scale tracks, step, linear and cubic interpolation (morph targets aren't animated) |
| Cameras and lights in the file | Not imported: add cameras in the editor; lighting comes from the scene's sun and sky |

When a model is placed in a scene, its root entity gets an **Animator** if the file has animations (playing the first one, looping), and every mesh node gets a **Mesh Renderer** that casts shadows.

### Orientation and scale

The engine uses glTF's conventions, so exported files come in the right way round:

- **+Y is up, +Z is forward**, units are **metres**.
- From **Blender**, use the built-in glTF exporter with its defaults (*+Y Up* on). A model facing Blender's *-Y* (its front view) faces +Z in the engine.
- If a model comes in 100 times too big or small, fix the unit scale in your 3D tool (or scale its root entity in the editor).

### Making a car drivable

The ready-made car rig works on any car model that faces +Z, stands on its origin and has its wheels as separate nodes named `Wheel…` (`WheelFrontLeft`, `Wheel_RL`, …). See [Vehicles](gameplay-physics.md#vehicles).

## Animations

Animations come in glTF files too:

- **On the model**: the clips in a model's file are its Animator's clips, named as in the file.
- **From other files**: an animation-only `.glb` (a *clip library*) can be dropped on an Animator's clips list, or on an animation graph's clips. Clips are matched to the model's bones **by name**, so they play on any model with the same bone names and proportions. There's no retargeting between different skeletons yet.
- **Mixamo**: download characters and animations as FBX and convert them to glTF with Blender (import FBX, export glTF). Bone names keep their `mixamorig:` prefix, which the engine ignores when matching, so Mixamo animations play on any Mixamo character.

See [Animation](animation.md) for playing, blending and state machines.

## Textures

Images in **PNG** and **JPEG** load anywhere a texture can go: drop one on a material's texture slot in the Inspector. They're mip-mapped when loaded.

- **Colour** maps (base colour, emissive) are read as sRGB; **data** maps (normal, metallic-roughness, occlusion) as linear. The slot you drop an image on decides, so the same file can serve as either.
- Normal maps follow glTF's convention (OpenGL style, +Y up). A normal map from a DirectX-style tool needs its green channel flipped.
- Large textures cost memory and download size (on the web, everything a game uses is downloaded). 2048 x 2048 is plenty for most objects.

## Materials of imported models

A model's materials live inside its file. You can still edit them in the Inspector:

- Edits apply to the material **everywhere it's used**, and the scene **remembers** them when you save: the next time the scene loads, the changes are put back on the model.
- To make a model's materials your own files, click **Extract materials** in a material's section. Every material of the model is written to a `.material` file in a folder next to the model (`Car.materials/0_Paint.material`, …), and from then on the model uses those files: edit them, and every scene that uses the model sees the change.
- To change the material of **one** copy only, click **Make unique**: that copy gets its own material, stored in the scene.

See [Materials](materials.md).

## Updating assets

The editor watches the project folder. When you export a model again, its thumbnail updates; reopen the scene (or spawn it again) to load the new version. Scripts are different: saving a script while the game runs reloads it at once.

## What the engine ships with

- **Built-in meshes**: cube, sphere, plane, cylinder and capsule (Create menu, or `scene.SpawnPrimitive` from scripts).
- **The Sandbox's models**: the concept car (`Models/CarConcept/`), the *Rumba* dancer (`Models/Rumba.glb`), Sponza (`Models/Sponza/`), the Damaged Helmet, a tree, an avocado and a few test models. The car and Sponza come from the Khronos glTF Sample Assets (see the repository's README for credits and licences).
