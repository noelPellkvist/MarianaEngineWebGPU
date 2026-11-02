---
title: Projects and Resource Paths
nav: Projects
description: How a Mariana project is organised, how files are found with resource paths like /Models/Car.glb, and how to override engine files.
---

# Projects and Resource Paths

A **project** is a folder with your game's content: scenes, models, textures, materials, scripts, animation graphs and settings. The engine finds every file in it by a **resource path**, a path that starts at the project's folder, such as `/Models/Car.glb`.

## The project folder

The engine comes with one project, **Sandbox**, in `projects/Sandbox/`; make your own with **File > New Project** (see [below](#creating-and-opening-projects)). A project's folders are up to you, but these names are what the editor and the engine look for:

| Folder or file | What goes there |
|---|---|
| `Scenes/` | Scene files (`.scene`). File > Open Scene lists this folder, and Save As writes into it. |
| `Scripts/` | Luau scripts (`.luau`). New scripts are created here, and games always take the whole folder. |
| `Models/` | glTF models (`.gltf` with their buffers and images, or `.glb`). |
| `Textures/` | Images (`.png`, `.jpg`). |
| `Materials/` | Material files (`.material`). Save As in the material editor writes here. |
| `Animations/` | Animation graphs (`.animgraph`). New graphs from the Animator are created here. |
| `Pipelines/` | Render pipelines of your own (`.pipeline`). |
| `Effects/`, `Shaders/` | Post effects and their shaders of your own (see [Custom Post Effects](../reference/custom-effects.md)). |
| `Physics.json` | Gravity, the physics step and collision layers (the Physics window saves it). |
| `Player.json` | How the player runs the project on its own (see [Player.json](../building/player-config.md)). |
| `BuildProfiles.json` | Your build profiles (see [Build Profiles](../building/build-profiles.md)). |
| `Builds/` | Where games are built to. Not part of the project: keep it out of version control. |
| `.mariana/` | The editor's caches (thumbnails, script types, the editor camera of each scene, build tool paths). Safe to delete; keep it out of version control. |

## Resource paths

Anywhere the engine or a script names a file, it uses a resource path:

```luau
scene.Spawn("/Models/Rumba.glb", vec3(0, 0, 4))
```

A resource path starts with `/` and uses forward slashes. It is looked up in a list of **roots**, in order:

1. The project's folder.
2. The editor's own resources (in the editor only: its font, icon and thumbnail pipeline).
3. The engine's resources: `engine/resources/`, with the default render pipeline, the built-in post effects, the shaders and the default sky.

The first root that has the file wins. That makes **overriding** engine files easy: to change the default pipeline for your project, save a file at `/Pipelines/Default.pipeline` in the project, and it is used instead of the engine's.

> [!WARNING]
> Paths are case-insensitive on Windows but not on the web or on other systems. Write them exactly as the files are named (`/Models/Rumba.glb`, not `/models/rumba.glb`) so your game works everywhere.

## Paths inside files

Files that refer to other files use resource paths too, or paths relative to themselves:

- **Scenes** refer to everything by resource path: `/Models/CarConcept/CarConcept.gltf#mesh/3` is the fourth mesh in that model, `/Materials/Floor.material` a material file, and `builtin://mesh/Cube` a built-in mesh. Each scene also lists the files it uses under `dependencies`, which is how builds know what to package.
- **Material** files name their images relative to the `.material` file (`albedo.png`), or by resource path when the path starts with `/`.
- **Animation graphs** name the models their clips come from relative to the `.animgraph` file, or by resource path.

## Creating and opening projects

**File > New Project...** makes a project: give it a name and a location (the folder it goes in), and press **Create**. The new folder gets:

- the folders `Scenes`, `Models`, `Textures`, `Materials` and `Scripts`,
- the Sandbox's `VehicleDriver.luau`, `CharacterDriver.luau` and `ChaseCamera.luau` in `Scripts`, which the editor's ready-made vehicles and characters use (Create > Vehicle, Create > Physics > Character attach them by resource path),
- a `.gitignore` that keeps `.mariana/` and `Builds/` out of version control.

The editor then starts again in a new window on the new project and closes the old one (after asking to save anything unsaved). The project starts with an empty scene: save it with **Ctrl+S** to give it a name in `Scenes/`.

**File > Open Project...** opens another project: type its folder, or pick one of the **recent projects**. Any folder can be a project; the editor makes its `.mariana/` cache folder in it when it opens.

When the editor starts, it opens:

1. the project given on the command line, if there is one: `MarianaEditor.exe --project "C:/Games/My Game"`,
2. else the project you had open last,
3. else the Sandbox.

In a project, it opens the scene you last had open there; the first time, the project's `Playground.scene`, else its first scene, else an empty one.

The list of recent projects is kept for your user account (`%APPDATA%/Mariana/projects.json` on Windows), not in any project.

> [!NOTE]
> The web editor has the Sandbox built into its page, so it can't create or open other projects: use the desktop editor for your own games.

### Working in the Sandbox

You can also make a game inside the Sandbox: add your scenes, scripts and models next to the samples, and point a [build profile](../building/build-profiles.md) at your start scene. Builds only take the files your scenes use, so the samples don't end up in your game.

## Version control

Commit everything in the project except `Builds/` and `.mariana/`. Scene, material, graph and pipeline files are JSON, so they diff and merge well.
