<div align="center">

<img src="logo.png" alt="Mariana Engine Logo" width="140"/>

# Mariana Engine (WebGPU)

**A small, fast, no-nonsense game engine targeting native and the web via WebGPU (Google Dawn).**  
_If it runs in your browser and on your PC with the same codebase, that’s not magic—that’s just good engineering._

[![CI](https://github.com/noelpellkvist/MarianaEngineWebGPU/actions/workflows/ci.yml/badge.svg)](https://github.com/noelpellkvist/MarianaEngineWebGPU/actions/workflows/ci.yml)
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
# Example (adjust to your actual sample/runner name):
./build/app
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
emrun --no_browser --port 8000 build-web/
# then open http://localhost:8000 in your browser
```

> Note: Exact target names/paths may differ.

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
- Everyone who likes triangles and frame times under 16ms

---

## 📄 License

This project is licensed under the **MIT License**. See [`LICENSE`](LICENSE) for details.

---

## 🗣️ Contact

- Author: **Noel Pellkvist**  
- Demo: **https://noelpellkvist.github.io/MarianaEngineWebGPU/**

