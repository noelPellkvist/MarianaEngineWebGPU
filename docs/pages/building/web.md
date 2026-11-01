---
title: Web Builds
description: Building a Mariana game for the web with WebGPU and Emscripten: the output, testing it locally, hosting it, and browser support.
---

# Web Builds

A Web profile builds your game as a **static website**: a handful of files you can put on any web host, itch.io or GitHub Pages. It runs in the browser with **WebGPU**, with nothing to install.

## What you get

```text title="Builds/Web"
index.html        the page
index.js          the player's code loader
index.wasm        the player (WebAssembly)
index.data        the engine's shaders, effects and default sky
game.data         your game's files
game.data.js      what loads game.data
```

| | Sandbox playground, Release |
|---|---|
| `index.wasm` | 8 MB |
| `index.data` | 3 MB |
| `game.data` | 14 MB |
| Total | 25 MB |

The page is plain: a black background, the game's canvas filling the window, and a loading percentage while the files download.

## What it needs to build

- The **Emscripten SDK** (the compiler for the web). Install it as in [Installing](../getting-started/installing.md#build-for-the-web) and set `EMSDK`, or choose its folder under **Build tools**.
- **Ninja**, which the web build uses; Visual Studio's CMake tools include it.

The first web build configures and compiles the whole engine for the web: that takes a while (several minutes). Later builds only rebuild what changed. Your game's files are packed separately (Emscripten's file packager), so changing a scene or a model doesn't recompile anything.

## Testing it locally

**Build and Run** serves the folder at `http://localhost:6931/index.html` and opens it in your browser. **Stop serving** in the Build window ends it.

Opening `index.html` straight from the disk (`file://`) **doesn't work**: browsers only allow WebGPU and the file downloads on a page served over `http://localhost` or `https://`. To serve a build yourself, any static server will do:

```bash
cd Builds/Web
python -m http.server 8000
```

Then open `http://localhost:8000`.

## Hosting

Upload the folder's files as they are, keeping them together. Requirements:

- **HTTPS.** WebGPU only runs on secure pages (`https://`, or `localhost` while testing). Every common host (GitHub Pages, itch.io, Netlify, Cloudflare Pages) serves HTTPS.
- Nothing else: no server code, no special headers.

> [!TIP]
> Hosts that compress files (gzip or Brotli) make the download much smaller: `.wasm` and `.data` files shrink a lot. GitHub Pages, Netlify and Cloudflare do this automatically.

### itch.io

Zip the folder's files (with `index.html` at the top of the zip, not inside a folder), upload the zip as an HTML game, and tick **This file will be played in the browser**. Set the embed size to your window size, or choose fullscreen.

### GitHub Pages

Put the files in a repository branch or folder that Pages publishes; the game is at `https://<you>.github.io/<repo>/`.

## Browsers

The game needs a browser with WebGPU:

| Browser | |
|---|---|
| Chrome, Edge (desktop) | Yes |
| Chrome (Android) | On recent devices |
| Firefox | In recent versions on Windows; elsewhere behind a setting |
| Safari | In recent versions |

In a browser without WebGPU the page stays black (its developer console says why). If your audience might not have it, say so on the game's page.

## Differences from Windows builds

- **Single-threaded.** The web player runs everything on one thread. Physics gives the same results, just with less headroom for very large scenes.
- **Timing precision.** Browsers round their clocks to 0.1 ms, so the profiler's single readings come in 0.1 ms steps (averages are still accurate).
- **Keyboard.** Some shortcuts belong to the browser (Ctrl+W, Ctrl+T, F5); a game can't use them.
- **Files.** The game's files are read-only in the browser; there's no saving to disk yet.
