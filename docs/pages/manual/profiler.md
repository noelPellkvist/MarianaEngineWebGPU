---
title: Profiling
description: Find out where frame time goes with the Profiler: CPU scopes, GPU passes, counters and traces for Perfetto.
---

# Profiling

When a game slows down, the **Profiler** tells you why. It's always recording: every frame's CPU work, split into named scopes (scripts, physics, animation, rendering, UI), the GPU time of every render pass, and counters like draw calls and active bodies.

![The Profiler window: the frame graph, the timeline of one frame and the scope tree](images/profiler.jpg "Window > Profiler while the Playground plays.")

## Opening it

**Window > Profiler**, or the **Profiler…** button in the **Stats** window. Stats itself shows the frame rate and the main counters at a glance.

## The frame graph

The top of the window shows the last 600 frames as bars: the CPU's work, with the time spent waiting for the display drawn grey on top, and the GPU time as a line. Guides mark 16.7 ms (60 fps) and 33.3 ms (30 fps).

- **Hover** a bar for its numbers.
- **Click** a frame to select it and pause recording; **Left** and **Right** step through frames.
- **Pause**, **Resume** and **Clear** are in the toolbar.

## The timeline

Below the graph is the selected frame as a timeline: a row per thread and nesting depth, each scope a coloured block (physics, animation, scripts, rendering, UI and waiting each have their colour). GPU passes have a row of their own.

- **Wheel** zooms around the mouse, **drag** pans, **double-click** shows the whole frame.
- Hover a block for its name and times.

## Scopes, GPU and counters

| Tab | Shows |
|---|---|
| **Scopes** | A tree of every scope: how many calls, total time, *self* time (without what it called) and the worst frame. Averaged over the last 120 frames, or for the selected frame |
| **GPU** | Each render pass: this frame, the average, the worst |
| **Counters** | Draws, instances, triangles, shadow cascades drawn, transforms and materials uploaded, entities, animators, bodies and active bodies, script instances; with a plot of the last 120 frames |

Every script function call is a scope of its own, named like `Snake.Update`, so a slow script stands out at once.

## Traces

**Save trace** writes every kept frame to `.mariana/profiles/profile-<time>.json` in the project (and downloads it in the web editor). Open it in [Perfetto](https://ui.perfetto.dev) or `chrome://tracing` to explore it in depth: threads, frames, GPU passes and counters on their own tracks.

## Profiling a built game

Turn on **Log frame times** in a [build profile](../building/build-profiles.md) (or `"profile": true` in [Player.json](../building/player-config.md)). Every 5 seconds the game logs its average CPU and GPU time and the 8 scopes with the most self time: to the console natively, to the browser's console on the web.

## Reading the numbers

- The **display wait** (grey) isn't work: the frame is done and waits for the screen's refresh. A game at 60 fps on a 60 Hz screen spends most of each frame waiting.
- In the browser, times are rounded to 0.1 ms by the browser itself, so single scopes read in 0.1 ms steps; averages are still accurate.
- GPU times need the GPU's timestamp queries. Browsers round them too unless WebGPU developer features are enabled in `chrome://flags`. The GPU tab says how passes are being timed.
