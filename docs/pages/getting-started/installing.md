---
title: Installing and Building
nav: Installing
description: Get the source, build the Mariana editor and player for Windows or the web, and run them.
---

# Installing and Building

The Mariana Engine is distributed as source. You build the **editor** (where you make games) and the **player** (which runs them) once. After that you work in the editor, and it builds your games for you.

## Requirements

| | Windows (native) | Web (browser) |
|---|---|---|
| Operating system | Windows 10 or 11, 64-bit | Any OS that runs the tools below |
| Compiler | Visual Studio 2022 with **Desktop development with C++** (MSVC, C++20) | [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) 4.0 or newer |
| Build system | [CMake](https://cmake.org/download/) 3.22 or newer | CMake 3.22+ and [Ninja](https://ninja-build.org/) |
| Other tools | [Git](https://git-scm.com/) and Python 3 (CMake uses them to fetch dependencies) | Git and Python 3 (the SDK brings its own Python) |
| GPU | A GPU with Direct3D 12 or Vulkan support | A browser with WebGPU: Chrome or Edge 113+, or a current Safari or Firefox with WebGPU enabled |

> [!NOTE]
> The first configure downloads the engine's dependencies (Google Dawn's third-party code, flecs, Luau, Jolt Physics and more), and the first native build compiles Dawn. Plan for 20–40 minutes and a few GB of disk space. Builds after that are incremental and take seconds to a few minutes.

## Get the source

```bash
git clone https://github.com/noelPellkvist/MarianaEngineWebGPU.git
cd MarianaEngineWebGPU
git submodule update --init
```

The submodules are Google Dawn (the WebGPU implementation) and glm (math). Everything else is fetched by CMake when you configure.

## Build for Windows

From a terminal in the repository's folder (a *Developer PowerShell for VS 2022* works, but any terminal with CMake on the `PATH` does):

```bash
cmake -B build
cmake --build build --config Release --target MarianaEditor MarianaPlayer
```

| Output | What it is |
|---|---|
| `build/bin/Release/MarianaEditor.exe` | The editor |
| `build/bin/Release/MarianaPlayer.exe` | The player: runs the project without the editor |
| `build/bin/Release/MarianaExport.exe` | The command-line exporter (build it with `--target MarianaExportTool`) |

Use `--config Debug` for a build with more checks and full debug information (slower to run). In Visual Studio Code the repository has tasks named **build Debug** and **build Player Debug**.

Run the editor:

```bash
./build/bin/Release/MarianaEditor.exe
```

The first time, it opens the sample project, **Sandbox**, in its *Playground* scene (after that, the project and scene you had open last; see [Projects](projects.md#creating-and-opening-projects)): a car on a test track, a water pool, jellies, a flag and three dancers in the car's way. Press **Ctrl+P** to play and drive with **W A S D**.

> [!TIP]
> Keep the `build` folder. File > Build uses it to build the player for your games, so a game build after the first one only takes seconds.

## Build for the web

The editor and the player also run in the browser, compiled to WebAssembly with Emscripten.

```bash
# Activate the Emscripten SDK in this terminal first:
#   Windows:  emsdk_env.bat     macOS/Linux:  source ./emsdk_env.sh
emcmake cmake -B build-web -G Ninja
cmake --build build-web
```

| Output | What it is |
|---|---|
| `build-web/bin/index.html` (+ `.js`, `.wasm`, `.data`) | The editor in the browser, with the Sandbox project packaged in |
| `build-web/player/index.html` (+ `.js`, `.wasm`, `.data`) | The player with the Sandbox's Playground: a standalone site |

WebGPU only runs in a *secure context*: serve the folder over `https`, or from `localhost`:

```bash
emrun --no_browser --port 8000 build-web/bin
# then open http://localhost:8000
```

Any static file server works (`python -m http.server 8000 --directory build-web/bin`, for example). Opening `index.html` straight from disk does not, because browsers don't allow WebGPU and file loading there.

> [!NOTE]
> The web editor is good for trying things and for showing the engine. Files you save in it live in the page's memory: saving a scene also downloads it, so you can keep it. For real work use the desktop editor, which also builds games (File > Build needs it).

## Check your build

The engine has console test programs that need no window or GPU. They are a quick way to see that a build works:

```bash
cmake --build build --config Release --target MarianaEngineTest MarianaPhysicsTest MarianaScriptingTest MarianaExportTest
./build/bin/Release/MarianaEngineTest.exe
./build/bin/Release/MarianaPhysicsTest.exe
./build/bin/Release/MarianaScriptingTest.exe
./build/bin/Release/MarianaExportTest.exe
```

Each prints a line per scenario and ends with a count, `... 0 failed`.

## Next steps

- [Take the editor tour](editor-tour.md) to find your way around.
- [Make your first game](first-game.md), a 3D Snake, in about an hour.
- Read how [projects and resource paths](projects.md) work before you add your own files.
