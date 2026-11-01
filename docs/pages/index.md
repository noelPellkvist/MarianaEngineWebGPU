---
title: Mariana Engine Documentation
nav: Home
description: Build 3D games with the Mariana Engine: the editor, Luau scripting, physics, animation and one-click builds for Windows and the web.
---

<div class="hero"><div class="hero-inner">
<div class="eyebrow">Mariana Engine · Documentation</div>
<h1>Make 3D games for <span>desktop and the web.</span></h1>
<p class="lead">A WebGPU engine with a full editor, Luau scripting, Jolt physics, a node-based animation system and builds for Windows and the browser from a single project.</p>
<div class="hero-actions"><a class="button primary" href="getting-started/first-game.html">Build your first game</a><a class="button" href="scripting/api/index.html">Scripting API</a><a class="button" href="getting-started/installing.html">Install</a></div>
</div>
<div class="hero-shot"><a href="images/editor-overview.jpg"><img src="images/editor-overview.jpg" alt="The Mariana editor with the Playground scene"></a></div>
</div>

## Start here

<div class="cards">
<a class="card" href="getting-started/installing.html"><div class="card-icon"><svg viewBox="0 0 24 24"><path d="M12 3v12m0 0-4-4m4 4 4-4M4 17v3h16v-3"/></svg></div><h3>Install the engine</h3><p>Build the editor and the player from source for Windows, or for the browser with Emscripten.</p></a>
<a class="card" href="getting-started/editor-tour.html"><div class="card-icon"><svg viewBox="0 0 24 24"><path d="M3 4h18v16H3zM3 9h18M9 9v11"/></svg></div><h3>Tour the editor</h3><p>The Scene and Game views, the Hierarchy, the Inspector, the Assets browser and the camera controls.</p></a>
<a class="card" href="getting-started/first-game.html"><div class="card-icon"><svg viewBox="0 0 24 24"><path d="M5 15c-1.5 1.5-2 5-2 5s3.5-.5 5-2m9-9a3 3 0 1 1-6 0 3 3 0 0 1 6 0Zm-1.5-6.5C17 3 20.5 3.5 20.5 3.5S21 7 19.5 11.5L13 18l-7-7 6.5-6.5Z"/></svg></div><h3>Tutorial: 3D Snake</h3><p>Make a complete game from an empty scene: a board, a snake you steer, apples, and physics when you crash.</p></a>
<a class="card" href="scripting/overview.html"><div class="card-icon"><svg viewBox="0 0 24 24"><path d="m8 6-6 6 6 6m8-12 6 6-6 6M14 4l-4 16"/></svg></div><h3>Scripting in Luau</h3><p>Write gameplay in Luau: scripts on entities, events, properties you tune in the Inspector, hot reload.</p></a>
<a class="card" href="scripting/api/index.html"><div class="card-icon"><svg viewBox="0 0 24 24"><path d="M4 4.5A2.5 2.5 0 0 1 6.5 2H20v17H6.5A2.5 2.5 0 0 0 4 21.5v-17Zm0 17A2.5 2.5 0 0 1 6.5 19H20v3H6.5"/></svg></div><h3>API reference</h3><p>Every global, type, method and component field scripts can use, with examples.</p></a>
<a class="card" href="building/build-profiles.html"><div class="card-icon"><svg viewBox="0 0 24 24"><path d="M14.7 6.3a4 4 0 0 0-5.4 5.4L3 18l3 3 6.3-6.3a4 4 0 0 0 5.4-5.4l-2.6 2.6-2.4-.6-.6-2.4 2.6-2.6Z"/></svg></div><h3>Build and publish</h3><p>Turn a project into a Windows game or a website with build profiles, from the editor or the command line.</p></a>
</div>

## What's in the engine

| Area | What you get |
|---|---|
| **Editor** | Scene and Game views, Hierarchy, Inspector, Assets browser with rendered thumbnails, material editor, undo and redo, play, pause and step, scene files that keep everything |
| **Rendering** | Physically based shading on WebGPU, cascaded shadows, sky lighting and reflections, HDR, ambient occlusion (GTAO), bloom, tonemapping, post effects as data files |
| **Scripting** | Luau (typed Lua) on entities: lifecycle and physics events, Inspector properties, hot reload, a Script Editor with type checking and autocomplete, VS Code support |
| **Physics** | Jolt Physics: rigid bodies, colliders, triggers, raycasts, joints and motors, cars, motorcycles, tanks, a character controller, water, ragdolls, cloth and soft bodies |
| **Animation** | glTF skinned models, clips and cross-fades, blend spaces, layers, state machines, Two Bone IK and Look At, root motion, animation events, a node graph editor |
| **Assets** | glTF 2.0 (`.gltf` and `.glb`), PNG and JPEG textures, `.material` files, animation graphs, render pipelines |
| **Builds** | Build profiles for Windows (an `.exe` and its data folder) and the web (a static site), Release or Debug, from the editor or the `MarianaExport` command line |
| **Tools** | A profiler with CPU scopes, GPU pass times and counters, exportable traces for Perfetto |

> [!TIP] New to the engine?
> Follow [Installing](getting-started/installing.md), take the [editor tour](getting-started/editor-tour.md), then make [3D Snake](getting-started/first-game.md). It takes about an hour and touches every part you need to start a game of your own.

## How this documentation is organised

- **Getting Started**: installation, the editor at a glance, and a complete first game.
- **Using the Editor**: scenes, entities and components, importing assets, materials, rendering, animation, physics and profiling.
- **Scripting**: how scripts work, the Luau language as you'll use it in games, the Script Editor, a cookbook of common tasks and debugging.
- **Scripting API Reference**: every function, type and component field available to scripts.
- **Building Games**: build profiles, Windows and web builds, the command line and `Player.json`.
- **Reference**: keyboard shortcuts, file formats, custom post effects, troubleshooting and current limitations.
