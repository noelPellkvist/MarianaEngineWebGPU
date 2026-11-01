---
title: The Editor at a Glance
nav: Editor tour
description: The main windows of the Mariana editor, how to move around a scene, and the shortcuts you'll use every day.
---

# The Editor at a Glance

This page is a ten-minute tour of the editor: what each window is for, how to move around a scene and how to try your game. Every window is covered in depth in [Using the Editor](../manual/editor.md).

![The editor with the Playground scene: Hierarchy on the left, the Scene view in the middle, the Inspector on the right and the Assets browser below](images/editor-overview.jpg "The editor with the Sandbox's Playground scene open.")

## The main windows

| Window | What it is for |
|---|---|
| **Scene** | The 3D view you edit in. Click to select, drag the gizmo to move, rotate and scale, fly around with the mouse and keyboard. |
| **Game** | What the game's camera sees: the first entity with a Camera component. This is what players will see. |
| **Hierarchy** | Every entity in the scene, as a tree of parents and children. Click to select, drop a model here to add it. |
| **Inspector** | The selected entity: its name, its components and their settings, and its materials. **Add Component** adds more. |
| **Assets** | The project's folders and files, with thumbnails. Drag models into the scene, double-click scripts, scenes and graphs to open them. |
| **Script Editor** | Edit Luau scripts with colours, live error checking and autocomplete. |

More windows live in the **Window** menu: **Stats** and the **Profiler** (performance), **Lighting** (the sun and the sky), **Render Pipeline** (post effects), **Animation Graph**, **Physics** (gravity, layers, debug drawing), **Loaded Assets**, and the **Build** window (File > Build). Every window can be docked, tabbed and moved by its title bar; the layout is kept between sessions.

## The menu bar

| Menu | |
|---|---|
| **File** | New, Open, Open Recent, Save and Save As for scenes; **Build** (Build Settings, Build, Build and Run); Exit |
| **Edit** | Undo and Redo (with the name of the step) |
| **Create** | Empty entities, primitives (cube, sphere, plane, cylinder, capsule), cameras, physics objects, vehicles, characters, water, cloth and more |
| **Window** | Open and close the editor's windows |

In the middle of the menu bar are the **Play**, **Pause** and **Step** buttons, and while the game runs, its clock. On the right is the scene's file name, with a `*` when something isn't saved yet: hover it to see what.

## Moving around the Scene view

The Scene view's camera works like Unity's. The mouse must be over the Scene view for these to start.

| Do this | To |
|---|---|
| Hold **right mouse button** and move the mouse | Look around |
| Hold right mouse button + **W A S D** | Fly forward, left, back, right |
| Hold right mouse button + **Space** / **Left Shift** | Fly up / down |
| Hold **middle mouse button** and drag | Pan |
| **Mouse wheel** | Move forward and back (dolly) |

## Selecting and moving things

- **Click** an entity in the Scene view, or its name in the Hierarchy, to select it.
- The **gizmo** appears on the selection. Press **1** to move, **2** to rotate, **3** to scale, then drag its handles.
- The **Inspector** shows the exact values: type them, or drag over a number to change it.
- **Delete** removes the selected entity (and everything below it). **Ctrl+Z** undoes it.

## Playing your game

Press **Play** (or **Ctrl+P**). Scripts start, physics and animation run, and the keyboard goes to your game whenever the mouse isn't flying the editor camera and you aren't typing in a field. Press it again (it's now a **Stop** button) to stop.

- **Pause** (**Ctrl+Shift+P**) freezes the game; **Step** (**Ctrl+Alt+P**) runs one frame while paused.
- Stopping puts the scene back exactly as it was before you pressed Play: experiment freely. To keep what happened to an entity, tick **Keep changes after Stop** in its Inspector while playing. See [Play Mode](../manual/play-mode.md).

> [!TIP]
> Open the **Game** window beside the **Scene** window (drag its tab) so you can watch the game while you look around the scene.

## Saving

**Ctrl+S** saves the scene into the project's `Scenes` folder, together with every material, animation graph and render pipeline you changed. The first time it asks for a name. See [Scenes](../manual/scenes.md) for everything a scene keeps.

## Next steps

- Make your first game: [Tutorial: 3D Snake](first-game.md).
- Learn where your files go: [Projects and Resource Paths](projects.md).
