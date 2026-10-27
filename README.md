<div align="center">

<img src="logo.png" alt="Mariana Engine Logo" width="140"/>

# Mariana Engine (WebGPU)

**A small, fast, no-nonsense game engine targeting native and the web via WebGPU (Google Dawn).**  
_If it runs in your browser and on your PC with the same codebase, that’s not magic—that’s just good engineering._

[![CI](https://github.com/noelpellkvist/MarianaEngineWebGPU/actions/workflows/pages.yml/badge.svg)](https://github.com/noelpellkvist/MarianaEngineWebGPU/actions/workflows/pages.yml)
[![GitHub Pages](https://img.shields.io/badge/Live%20Demo-gh%20pages-2ea44f)](https://noelpellkvist.github.io/MarianaEngineWebGPU/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](#license)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](#contributing)

</div>

---

## 🔗 Live Demo

- **Latest test build (GitHub Pages):** **https://noelpellkvist.github.io/MarianaEngineWebGPU/**  
  Deployed automatically from `2025` via GitHub Actions. If it loads, WebGPU’s working; if it flies, you’re welcome. 🛠️🚀

> Tip: If your browser doesn’t support WebGPU yet, try the latest Chrome/Edge, or enable WebGPU flags.

---

## ✨ Highlights

- **WebGPU-first renderer** powered by **Google Dawn** (same API across native & web)
- **Single codebase** for desktop and browser targets
- **Modern C++** (C++20) with **CMake** build system
- **Lean & hackable**: small surface area, clean architecture
- **CI/CD** with GitHub Actions + automatic **Pages** deploys

> TL;DR: write once, run anywhere *that doesn’t hate triangles*.

---

## 🧩 Features

**Rendering**
- PBR forward renderer on a render graph: cascaded shadow maps, sky lighting and reflections, HDR, ambient occlusion (GTAO), bloom, tonemapping
- Render pipelines and post effects as data files; several views of one scene share its GPU data
- glTF 2.0 models: skinning, animations, texture transforms, clearcoat, emissive strength, alpha masking and see-through blending

**Animation**
- Clips, cross-fades, 1D/2D blend spaces, additive layers, bone masks and state machines, saved as `.animgraph` files and edited in a node graph editor

**Physics** (Jolt Physics, native and web)
- Rigid bodies with box, sphere, capsule, cylinder, mesh and convex colliders; triggers, raycasts, shape casts, contact events, layers
- Vehicles: cars (any glTF car with `Wheel…` nodes becomes drivable), motorcycles and tanks
- A character controller that climbs stairs, walks slopes, pushes crates and swims
- Joints: hinges, sliders, chains, doors, motors, breakable joints
- Water: buoyancy, drag, waves and currents
- Ragdolls from animated skeletons: powered ragdolls keep dancing until something hits them
- Soft bodies: cloth, flags in the wind, jelly and balloons

**Scripting** (Luau, native and web)
- Game code in Luau: scripts on entities with Start/Update/contact callbacks, any saved component by name, input, game time and the physics API ([scripting/API.md](scripting/API.md))
- Properties edited per entity in the inspector, live reload while playing, errors shown where they happen; native code generation on desktop
- The game controls are scripts too: driving vehicles, walking characters and chase cameras come as readable `.luau` files a project can change
- A built-in Script Editor with syntax colours, type errors as you type and autocomplete from the engine's API types (also set up for VS Code's Luau extension)

**Editor** (Dear ImGui)
- Scene and game views, hierarchy, inspector, asset browser with rendered thumbnails, material editor, undo/redo, play/pause/step, scene files

**Player**
- Runs a project without the editor from its `Player.json`; its web build packages only what the project uses

**Try it:** the Sandbox project opens in a playground scene with a car, a pool, jellies, a flag and three dancers in the car's way. Press Play (Ctrl+P) in the editor, or open the player, and drive with W A S D (Space is the hand brake, R puts the car back at the start); the orange pad on the left throws the car into the air. All of that is Lua: the driving and the chase camera in `scripting/resources/Scripts/`, the respawn and the pad in `projects/Sandbox/Scripts/`.

---

## 📦 Getting Started

### 1) Clone the repository

```bash
# HTTPS
git clone https://github.com/noelpellkvist/MarianaEngineWebGPU.git
cd MarianaEngineWebGPU
git submodule update --init
```

### 2) Build Options

#### Option A — Native (Windows/macOS/Linux)

**Prerequisites**
- CMake ≥ 3.20
- A C++20 compiler (MSVC, Clang, or GCC)
- Platform SDK/Dev tools (e.g., Xcode on macOS, Build Tools on Windows)

**Configure & build**
```bash
cmake -B build 
cmake --build build -j4
```

**Run**
```bash
# The editor, and the player (runs the Sandbox project without the editor)
./build/bin/Debug/MarianaEditor
./build/bin/Debug/MarianaPlayer
```

#### Option B — Web (WASM via Emscripten)

**Prerequisites**
- [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) installed and activated (`emsdk_env`)
- CMake ≥ 3.20

**Configure & build**
```bash
# Activate emsdk in your shell first
# source /path/to/emsdk/emsdk_env.sh

emcmake cmake -B build-web
cmake --build build-web -j4
```

**Serve locally**
```bash
# Using emrun or any static server
emrun --no_browser --port 8000 build-web/bin
# then open http://localhost:8000 in your browser
```

The editor is built into `build-web/bin`, the player into `build-web/player` (a standalone site to upload as is). WebGPU needs a secure context: https, or localhost.

---



## ⚙️ Configuration & Flags

# TBA

---

## 🧪 Continuous Integration / Deployment

- **CI:** Builds and tests on push/PR via **GitHub Actions**.  
  Badge above points at `.github/workflows/pages.yml`.
- **CD:** Successful builds on `2025` deploy to **GitHub Pages** → **[Live Demo](https://noelpellkvist.github.io/MarianaEngineWebGPU/)**.

Example workflow names you might see:
- `pages.yml` — configure + build + test (web) + publish the web build to Pages

---

## 🧭 Roadmap

- [ ] Editor playground (scene graph, gizmos, hot reload)
- [ ] Asset pipeline & importers
- [ ] More samples (lighting, post, physics integration)
- [ ] Expanded platform matrix & CI coverage
- [ ] Documentation site

Have ideas? Pop an issue. Bold ones get bonus points.

---

## 🤝 Contributing

Contributions are welcome! Please:
1. Open an issue to discuss significant changes.
2. Keep PRs focused and reasonably sized.
3. Run the full build/test matrix if applicable.

> First-time contributors: check `good first issue` labels.

---

## 🐞 Troubleshooting

- **WebGPU not available:** Update Chrome/Edge; enable `chrome://flags/#enable-webgpu` as needed.
- **CI fails but local works:** Ensure you can build with a clean cache and the same CMake options CI uses.

---

## 📚 Acknowledgements

- **Google Dawn** — WebGPU implementation powering both native and web backends
- **WebGPU** — modern graphics API for the Web and native
- **Jolt Physics**, **Luau**, **flecs**, **Dear ImGui**, **ImGuizmo**, **tinygltf**, **glm**, **stb_image** and **nlohmann/json**
- **Car Concept** (`projects/Sandbox/Models/CarConcept/`) — © 2024 Darmstadt Graphics Group GmbH, created by Eric Chadwick, based on a public domain model by Unity Fan; from the Khronos glTF Sample Assets, [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) (Khronos logos and trademarks excluded)
- Everyone who likes triangles and frame times under 16ms

---

## 📄 License

This project is licensed under the **MIT License**. See [`LICENSE`](LICENSE) for details.

---

## 🗣️ Contact

- Author: **Noel Pellkvist**  
- Demo: **https://noelpellkvist.github.io/MarianaEngineWebGPU/**

