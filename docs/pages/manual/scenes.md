---
title: Scenes
description: Create, open and save scenes, what a scene file keeps, and how the editor tracks unsaved changes.
---

# Scenes

A **scene** is a level, a menu, a test room: a set of entities with their components, plus the scene's lighting and the render pipeline it is seen with. Scenes are saved as `.scene` files in the project's `Scenes` folder.

## Working with scene files

| Command | Shortcut | What it does |
|---|---|---|
| **File > New Scene** | Ctrl+N | An empty scene, not saved yet (*Untitled*) |
| **File > Open Scene…** | Ctrl+O | A list of every `.scene` in the project's `Scenes` folder |
| **File > Open Recent** | | The scenes you opened in this session |
| **File > Save Scene** | Ctrl+S | Saves the scene, and every changed asset with it (below). Asks for a name the first time |
| **File > Save Scene As…** | Ctrl+Shift+S | Saves under a new name in `Scenes/` |

You can also double-click a `.scene` file in the Assets window to open it.

Saving is off while the game is running: a scene is saved as you edited it, not as the game left it. Stop the game first (or use [Keep changes after Stop](play-mode.md#keeping-changes) for what you want to keep).

## What a scene keeps

Everything you see in the editor is saved:

- **Every entity**: its name, its parent, and every component with all its settings: transforms, meshes and their materials, cameras, rigid bodies and colliders, joints, vehicles and wheels, characters, water, ragdolls, soft bodies, and scripts with the property values you set on each entity.
- **Animators**: the model's skeleton, its clips with their names and [events](animation.md#animation-events), the animation graph, the speed, whether it plays, root motion, the clip playing with its time and loop, the graph's **parameter values** you changed from the defaults, and the **state** each state machine is in.
- **Materials made in the scene**: primitives' own materials and copies made with *Make unique* are stored inside the scene file.
- **Edits to a model's materials**: change the colour of a material that belongs to a `.glb` or `.gltf`, and the scene remembers the change and puts it back on the model when it loads. The model file itself isn't touched.
- **Lighting**: the sun and the sky settings from the Lighting window.
- **The render pipeline**: which `.pipeline` file the scene is seen with. Opening the scene loads it.

What isn't in the scene file: the selection, and the editor camera, which is kept per scene in the project's `.mariana` folder so it comes back when you reopen the scene.

## Saving everything at once

**Ctrl+S** saves the scene and, before it, every asset file you changed:

- `.material` files whose materials you edited,
- `.animgraph` files you edited in the Animation Graph window (including moving boxes),
- the render pipeline, if what runs differs from its `.pipeline` file.

The log lists every file written. Each of those windows also has its own Save button, for saving one thing alone.

## Unsaved changes

The scene's name on the right of the menu bar gets a `*` when anything isn't saved: an entity moved, a component setting changed, the lighting adjusted, a material or graph edited. Hover it to see what isn't saved. Undo back to how it was saved and the `*` goes away again for scene changes.

Before throwing changes away (New Scene, Open Scene, Open Recent, New Project, Open Project, Exit, or closing the window), the editor asks:

- **Save**: saves everything (and asks for a name if the scene has none), then goes on.
- **Don't Save**: goes on without saving.
- **Cancel**: stays where you are.

In the web editor the browser asks before you leave a page with unsaved changes.

## Prefabs and models

A model file (`.gltf` or `.glb`) works like a prefab: drag it into the scene as often as you like, and each copy is a tree of entities you can change independently. The scene refers to the model's meshes, materials and skeleton by path, so updating the model file updates every scene that uses it.

To reuse something you built in the editor (a model with colliders, materials and scripts, or a group of entities), save it as a **prefab file**: right-click it in the Hierarchy and choose **Save as Prefab**. It is written to `Prefabs/<name>.prefab` with everything below it, components, materials and scripts included. Drag the file from the Assets browser into the Hierarchy or the Scene view to place a copy. A copy isn't linked to its file: saving the prefab again changes what you place (or spawn) from then on, not the copies already in scenes.

From a script, spawn models with [`scene.Spawn`](../scripting/api/scene.md):

```luau
local dancer = scene.Spawn("/Models/Rumba.glb", vec3(0, 0, 5))
local crate = scene.Spawn("/Prefabs/Crate.prefab", vec3(0, 3, 0))
```

## The Playground

The Sandbox opens `Scenes/Playground.scene` on start: the vehicle test track, the concept car with a chase camera, three dancing ragdolls in its lane, a water pool, jellies, a flag and a launch pad. It's a good place to look at how things are set up: select anything and read its Inspector.

## Scene files on disk

`.scene` files are JSON. You don't need to edit them by hand, but they diff and merge well in version control. The format is described in [File Formats](../reference/file-formats.md#scene-files).
