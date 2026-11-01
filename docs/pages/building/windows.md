---
title: Windows Builds
description: Building a Mariana game for Windows: what the output folder holds, how to run and ship it, and what it needs.
---

# Windows Builds

A Windows profile builds a folder you can zip and hand to anyone with a Windows PC: no installer, no runtime to install.

## What you get

```text title="Builds/Windows"
Playground.exe          the game (the Mariana player, with everything linked in)
data/
  engine/               the engine's shaders, effects, pipelines and default sky
  game/                 your game's files and its Player.json
```

| | Sandbox playground, Release |
|---|---|
| `Playground.exe` | 14 MB |
| `data/engine` | 3 MB |
| `data/game` | 13 MB (the concept car, the dancers, scripts and the scene) |

The `.exe` finds `data/` next to itself, so the folder can go anywhere: move it, zip it, put it on a USB stick. Keep `data/` next to the `.exe`.

## How the build works

1. The profile and the tools are checked.
2. The player is compiled with CMake into the engine's build folder in the profile's configuration (Release or Debug). After the first time only what changed is rebuilt.
3. The output folder is laid out: the player as `<Game name>.exe`, the engine's resources and your game's files.
4. The game's `Player.json` is written from the profile (window, start scene, pipeline, follow camera, logging).
5. The result is checked: the `.exe` runs with `--check`, which loads nothing on screen but makes sure it finds `Player.json`, the shaders, the pipeline, the icon, the scene and every file the scene uses, then exits. A missing file fails the build with its name.

## Running it

Double-click the `.exe`, or use **Build and Run**. The game opens its window at the profile's size; **F11** toggles fullscreen. Closing the window ends the game.

Started from a terminal, the game prints its log there (what loaded, warnings, script `print`s). Started from Explorer it has no console window.

```bash
Builds/Windows/Playground.exe --check
```

`--check` prints whether each file the game needs is found, then exits without opening a window: useful after copying a build somewhere.

## What it needs

On the player's computer:

- Windows 10 or 11, 64-bit,
- a graphics card with Direct3D 12 (or Vulkan) drivers: anything from the last several years.

On yours (to build): the engine built with CMake and Visual Studio, as in [Installing](../getting-started/installing.md). The Build window's **Build tools** shows the native build folder it uses.

## Debug builds

A **Debug** profile builds the player with the compiler's checks and without optimisation. It runs much slower (physics and animation especially), so use it only to track down a crash in the engine, never to judge performance or to ship.

## Shipping checklist

- [ ] The scene is saved and plays as you want in the editor.
- [ ] The profile is **Release**, with your game's name, title, icon and window size.
- [ ] **Build and Run** works and the log shows no warnings about missing files.
- [ ] Zip the whole output folder (the `.exe` and `data/` together).
- [ ] Include the licences of any third-party models, textures or sounds you used.
