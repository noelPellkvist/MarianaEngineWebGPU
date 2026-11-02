---
title: Build Profiles
description: Turning a Mariana project into a game for Windows or the web with build profiles and the Build window.
---

# Build Profiles

A **build profile** says how to turn your project into a game: for which platform, from which scenes, into which folder, with which window. A project can have as many as you like (a Windows release, a web demo, a debug build for testing), and one of them is **active**: the one **Build** builds.

![The Build window](../images/build-window.jpg)

## Building

| Menu | Shortcut | |
|---|---|---|
| **File > Build > Build Settings...** | | Opens the Build window: profiles, settings, tools, the log |
| **File > Build > Build** | Ctrl+B | Builds the active profile |
| **File > Build > Build and Run** | Ctrl+Shift+B | Builds it, then starts the game (Windows) or serves it and opens your browser (web) |

A build goes through its steps in the background while you keep working: it checks the profile and the tools, compiles the player (only what changed since the last build, so usually seconds), collects your game's files and lays out the output folder. The Build window shows the step, a progress bar and the log; errors are red. **Cancel** stops it.

> [!IMPORTANT]
> A build takes your files **as they are saved**. Save your scene (Ctrl+S) before building, or the game gets the version on disk.

When it's done, **Open output folder** shows the result: `<project>/Builds/<profile name>` unless the profile says otherwise.

> [!NOTE]
> Building runs CMake and the compilers on your computer, so it needs the desktop editor. The web editor can edit profiles but not build them.

## Profiles

The left side of the Build window lists the profiles. **+ Windows** and **+ Web** add one, **Duplicate** copies the selected one, **Delete** removes it and **Make active** makes it the one Build builds. A new project starts with a Windows and a Web profile that start in its first scene.

### Settings

| Setting | |
|---|---|
| **Name** | The profile's name (and its default output folder's) |
| **Platform** | **Windows**: a folder with an `.exe`. **Web**: a static website. See [Windows](windows.md) and [Web](web.md) |
| **Configuration** | **Release** (fast; what you ship) or **Debug** (slow, with checks; for finding engine problems) |
| **Folder** | Where the game goes, relative to the project (or a full path). Empty = `Builds/<name>` |
| **Game name** | The `.exe`'s name on Windows |
| **Window title** | The window's title (the browser tab's on the web). Empty = the game name. It also names where the game's [save data](../scripting/api/save.md) is kept, so changing it starts players afresh |
| **Window size** | The window's size when it opens, in pixels (on the web the page fills the browser) |
| **Icon** | A PNG for the window icon; empty = the engine's |
| **Start scene** | The scene the game opens. Choose from the project's `Scenes/` folder |
| **More scenes** | Other scenes to package with it: every scene a script switches to with [`scene.Load`](../scripting/api/scene.md#load) |
| **Pipeline** | The [render pipeline](../manual/rendering.md); empty = the one the scene was saved with |
| **Follow camera** | A scene without a camera gets one that chases what the keys drive (a vehicle or character with the Sandbox's driver scripts) |
| **Log frame times** | Every 5 seconds the game logs its average CPU and GPU times and its busiest parts (in the console on Windows, the browser's console on the web) |
| **Whole project** | Package every file of the project, not only what the scenes use |

Profiles are saved as you change them, in the project's `BuildProfiles.json`, so they can go into version control with the project. See [File Formats](../reference/file-formats.md#build-profiles).

## What goes into the game

Without **Whole project**, a build takes only what your scenes need:

- the start scene and the other scenes, and every file each one uses (models, materials and their textures, animation graphs and their clip files, the pipeline),
- each model's companions: a `.gltf`'s whole folder (its buffers and textures), a `.glb`'s extracted `.materials` folder,
- the project's `Physics.json`, its own pipelines and icon when it uses them,
- the whole `Scripts/` folder, and the `Prefabs/` folder with what the prefabs use.

So files that only scripts load (a model `scene.Spawn` spawns that no scene contains, a texture for `texture.load`) aren't included: put them in a scene, or turn on **Whole project**. Scenes the game switches to go in **More scenes**. To see exactly what a profile takes without building, run the [command line tool](command-line.md) with `--list`. Files a scene refers to that can't be found are listed as warnings.

**Whole project** takes everything except the editor's own folders (`.mariana`, `Builds`, `.vscode`, `.git`). The game is bigger, but nothing can be left out.

## Build tools

The **Build tools** part of the window shows what the build uses, found automatically on the computer that built the editor:

| Tool | Needed for | |
|---|---|---|
| **CMake** | Both | The one the engine was built with |
| **Native build folder** | Windows | The engine's build folder (`build/`), where the player is compiled |
| **Web build folder (Release / Debug)** | Web | `build-web-release` / `build-web` next to the engine's source; made on the first web build |
| **Emscripten SDK** | Web | The `EMSDK` environment variable, or a folder you choose |

Each shows whether it is ready or what's missing. Paths you change are saved in the project's `.mariana/build-tools.json`: they belong to this computer, not to the project, so they stay out of version control.

## Running the result

**Build and Run** starts what it built:

- **Windows:** the game's `.exe`, in its own window.
- **Web:** a small local web server at `http://localhost:6931/index.html`, opened in your browser. It keeps serving until you press **Stop serving** (or build again).

Next: the details of [Windows builds](windows.md) and [web builds](web.md), building from the [command line](command-line.md), and what the game reads at start-up, [Player.json](player-config.md).
