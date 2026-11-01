---
title: Materials
description: Edit physically based materials in the Inspector, use texture maps, alpha and clearcoat, and save materials as files.
---

# Materials

Materials decide how surfaces look. The engine uses **physically based rendering** (PBR) with the *metallic-roughness* model, the same as glTF, Unreal and Unity: you describe what a surface *is* (metal or not, rough or smooth, its colour), and the lighting does the rest.

![The material editor in the Inspector, with the car paint selected](images/material-editor.jpg "Select an entity and its materials appear at the bottom of the Inspector.")

## Editing a material

Select an entity with a mesh. Its materials are listed at the bottom of the Inspector (for a model's root, every material used below it), each with the full editor:

| Setting | |
|---|---|
| **Base Color** | The surface colour (and alpha). Multiplied with the base colour map |
| **Metallic** | 0 for most things (plastic, wood, stone, skin), 1 for bare metal. Rarely in between |
| **Roughness** | 0 is a mirror, 1 is chalk |
| **Normal strength** | How strongly the normal map bends the surface |
| **Occlusion strength** | How much the occlusion map darkens creases |
| **Emissive** | Light the surface gives off. Values above 1 glow and bloom (it's HDR) |
| **Clearcoat**, **Clearcoat roughness** | A clear varnish layer on top: car paint, lacquered wood, a wet look |
| **Alpha mode** | *Opaque*; *Mask* (cut out where alpha is under the *cutoff*: leaves, fences); *Blend* (see-through by alpha: glass, water) |
| **Double sided** | Draw back faces too (leaves, cloth, thin sheets) |

Under the settings is one row per **texture slot**: base colour, normal, metallic-roughness, occlusion, emissive. Each shows a thumbnail (hover to enlarge, click to open it in the texture viewer), its size, format and mip levels.

- **Drop an image** from the Assets window on a slot to use it. Colour slots read it as sRGB, the others as linear data.
- **Clear** empties the slot.
- **Sampling** opens the slot's options: which UV set it uses, how it wraps and filters, and its **texture transform** (tiling, offset, rotation), for repeating floors and walls.

Every edit is an undo step, and shows at once in the Scene and Game views.

## Where a material lives

| Kind | Where it's stored | How to tell |
|---|---|---|
| A primitive's own material, or a *Make unique* copy | Inside the scene file | "Not saved" in its file row; saved with the scene |
| A model's material | Inside the model file | "From Car.glb" in its file row; the scene remembers your edits |
| A `.material` file | Its own file in the project | The file name, with "(unsaved)" while it has edits; **Save** writes it |

Edits change the material itself, so **every** mesh that uses it changes. *Used by N meshes* tells you how many. To change one object only:

- **Make unique** gives the selected entity (or model) its own copy, stored in the scene.

To turn any material into a file you can share between scenes:

- **Save As** writes it to the project's `Materials` folder as a `.material` file.
- **Extract materials** (for a model's materials) writes every material of the model next to it and makes the model use those files from then on.

Ctrl+S saves every changed `.material` file along with the scene.

## The material asset view

Click a material's **Open** button to show it on its own in the Inspector: the editor, **Duplicate**, and every entity that uses it (click one to select it). Click a texture thumbnail for the texture viewer: zoom, each mip level, the red, green, blue and alpha channels, and which materials use it.

## Tips for good materials

- Start from real-world values: most non-metals have a base colour between 0.05 and 0.9 in brightness; pure black and pure white don't exist outside light sources.
- Metal is either metal or not. For painted metal, use metallic 0 (it's the paint you see).
- Use **Mask** rather than **Blend** for foliage and fences: masked materials cast shadows and sort correctly; blended ones don't cast shadows and are sorted per object.
- Glowing parts need emissive above 1 to bloom, and an *emissive* map to limit the glow to where it should be.

## Materials from scripts

Scripts choose colours when they create primitives (`scene.SpawnPrimitive("cube", position, vec3(1, 0, 0))`), but can't edit a material's settings yet. For objects whose colour must change at runtime, spawn a replacement in the new colour, or keep a few variants in the scene and switch which one is shown.
