---
title: Building from the Command Line
description: MarianaExport, the command line tool that builds a Mariana project's build profiles, for scripts and continuous integration.
---

# Building from the Command Line

`MarianaExport` builds a project's [build profiles](build-profiles.md) without the editor: the same build, step for step, as the Build window's. Use it in scripts, batch files and continuous integration.

Build it with the `MarianaExportTool` target (`cmake --build build --config Release --target MarianaExportTool`); it lands next to the editor as `MarianaExport.exe`:

```bash
build/bin/Release/MarianaExport.exe --project projects/Sandbox --profile Web
```

## Options

| Option | |
|---|---|
| `--project <folder>` | The project to build (required) |
| `--profile <name>` | The profile, by name. Without it, the project's active profile |
| `--list` | Builds nothing: prints every file the game would take, warnings for files its scenes refer to that can't be found, and the `Player.json` it would write |
| `--native-build <folder>` | The engine's native build folder to compile the player in (default: the one the tool was built in) |
| `--web-build <folder>` | The web build folder (default: `build-web-release`, or `build-web` for a Debug profile, next to the engine's source) |
| `--emsdk <folder>` | The Emscripten SDK (default: the `EMSDK` environment variable) |

Defaults also come from the project's `.mariana/build-tools.json` (the paths the Build window saved on this computer); options on the command line win.

## Output and exit code

The log goes to the console as the build runs: each step, the compilers' output, then what was written. Errors go to stderr. The exit code is **0** when the build succeeded, **1** when it failed (the log says why) and **2** for a wrong command line.

```text
> MarianaExport.exe --project projects/Sandbox --list
Animations/Dancer.animgraph
Models/CarConcept
Models/Rumba.glb
Scenes/Playground.scene
Scripts
Player.json:
{
  "window": { "title": "Mariana Playground", "width": 1280, "height": 720 },
  "scene": "/Scenes/Playground.scene",
  "camera": { "position": [-5.5, 3.2, -8.0], "target": [0, 0.6, 3.0], "follow": true },
  "profile": false
}
```

Folders (a `.gltf`'s, the `Scripts` folder) are listed once and packaged whole. Paths are relative to the project.

## In continuous integration

```bash title="build-game.sh"
set -e
cmake -B build
cmake --build build --config Release --target MarianaExportTool
build/bin/Release/MarianaExport.exe --project MyGame --profile Windows
build/bin/Release/MarianaExport.exe --project MyGame --profile Web --emsdk "$EMSDK"
```

The Windows output lands in `MyGame/Builds/Windows`, the web one in `MyGame/Builds/Web` (or each profile's folder): archive them as the job's results.

> [!NOTE]
> Building for Windows runs on Windows. The web build needs the Emscripten SDK and Ninja on the machine.
