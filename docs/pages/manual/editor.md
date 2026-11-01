---
title: Editor Windows
nav: Editor windows
description: A reference for every window of the Mariana editor: Scene, Game, Hierarchy, Inspector, Assets and the tool windows.
---

# Editor Windows

The editor is made of dockable windows. Drag a window by its title bar to move it, onto another window to tab them together, or onto the edge markers to dock it beside. The layout is saved when you quit and restored when you start. Open or close any window from the **Window** menu.

## Scene

The **Scene** window is where you build. It shows the scene through the editor camera, which is yours alone and isn't part of the game.

- **Select** by clicking an object. Click empty sky to clear the selection.
- **Move, rotate and scale** with the gizmo on the selection: press **1** (move), **2** (rotate) or **3** (scale), then drag a handle or the gizmo's centre.
- **Navigate** with the mouse over the view: right mouse button to look (with **W A S D** to fly, **Space** up, **Left Shift** down), middle mouse button to pan, the wheel to dolly.
- Every **camera** in the scene is drawn as a small frustum: the one the Game window uses brighter, the selected one yellow.
- **Colliders** of the selection are outlined (green, triggers cyan); drag the dots on a box, sphere, capsule or cylinder collider to resize it. Vehicle wheels, joints, water volumes and character capsules are drawn too. While the game runs, the Physics window can draw every body.

The editor camera's place is saved per scene, in the project's `.mariana` folder, and comes back when you open the scene again.

## Game

The **Game** window shows the scene through the **first camera** in it (the first entity with a Camera component, by creation order), with the same render pipeline as the Scene window. This is what a built game shows. Without a camera it says *No camera in the scene*: add one with **Create > Camera**.

## Hierarchy

Every entity in the scene, as a tree: children are indented under their parents.

- Click a name to select it.
- Click the arrow to open or close an entity's children.
- Drop a model (a `.gltf` or `.glb` from the Assets window) on the Hierarchy to add it to the scene.
- Right-click an entity for its menu. While the game runs, **Keep changes after Stop** is there too, and kept entities say *keep* on their row.

## Inspector

The selected entity's settings.

- **Name** at the top: rename the entity.
- One **section per component**, in the order they were added. Each section has the component's settings; physics sections have a close button that removes the component.
- **Add Component** adds a component, grouped by area: *Rendering* (Camera), *Physics* (Rigid Body, colliders, Mesh Colliders on Children, Joint, Character Controller, Water Volume, Ragdoll, Soft Body), *Vehicles* (Vehicle, Vehicle Wheel) and *Scripting* (Scripts). Items are greyed out when they don't fit the entity.
- Below the components, a section per **material** the entity (or, for a model, anything below it) uses, with the full material editor. See [Materials](materials.md).
- While the game runs, sections show live state: a body's velocity, a vehicle's speed and gear, a character's ground, a joint's force.

The Inspector can also show an **asset** instead of an entity: a material, a texture (with a viewer that shows mip levels and channels), a mesh or an animation. Click a texture thumbnail or a material's **Open** button, or a row in the Loaded Assets window. **Back** returns to the entity.

Every change you make in the Inspector is a step you can **undo** (Ctrl+Z). Dragging a value is one step, however long you drag.

## Assets

A browser for the project's folder. Click folders to go in, the path bar to go back up.

- Models, images and `.material` files show **rendered thumbnails**. They are made in the background and kept in `.mariana/thumbnails`, so they appear at once the next time.
- **Drag** a model into the Scene view or the Hierarchy to place it; an image onto a material's texture slot; an `.animgraph` onto an Animator; a `.luau` onto a Scripts section.
- **Double-click** a scene to open it, a script to edit it in the Script Editor, an animation graph to open it in the Animation Graph window.
- **Right-click** in a folder to create a new **Script** or **Animation Graph** there.

The browser notices files you add, change or remove outside the editor (in Explorer, or from your 3D tool) and updates their thumbnails.

## Script Editor

An editor for Luau scripts with syntax colours, errors underlined as you type, and autocomplete. **Ctrl+S** saves, and a running game picks the new code up at once. See [The Script Editor](../scripting/script-editor.md).

## Animation Graph

A node editor for animation graphs: blend trees, layers, state machines with transitions and conditions, IK nodes, and the graph's parameters. See [Animation](animation.md).

## Render Pipeline

The post effects the views use and their settings: ambient occlusion, bloom, tonemapping and any you add, the camera's exposure, shadow settings and whether the sky is drawn. Save the result as a `.pipeline` file. See [Rendering](rendering.md).

## Lighting

The sun (direction, colour, intensity) and the sky (how much light and reflection it gives, and its rotation). These settings are saved with the scene.

## Physics

Simulation settings for the project (gravity, steps per second, collision layers and which collide), saved to `Physics.json`, and the physics debug view: draw every body's shape, sleeping bodies, contacts, velocities and centres of mass while the game runs. See [Physics](physics.md).

## Profiler and Stats

**Stats** shows the frame rate and the main counters. The **Profiler** records the last 600 frames: a graph of frame times, a timeline of every timed scope (scripts, physics, animation, rendering, UI) for the selected frame, GPU time per render pass, and counters. See [Profiling](profiler.md).

## Loaded Assets

Every asset in memory, with its type, its file, and how many things use it. **Unload Unused** frees what nothing uses. Click a row to inspect the asset.

## Build

Build profiles and building games: **File > Build > Build Settings…**. See [Build Profiles](../building/build-profiles.md).

## Undo and redo

**Ctrl+Z** undoes, **Ctrl+Y** or **Ctrl+Shift+Z** redoes; the Edit menu shows the name of the step. Recorded: moving, rotating and scaling (gizmo and Inspector), material edits, material slot changes, *Cast Shadows*, *Make unique*, every physics and script setting, collider handle drags, animation graph edits, animation event edits and *Keep play-mode changes*. A text field you're typing in keeps Ctrl+Z for itself.

## The desktop and web editors

The editor also runs in the browser. It's the same editor with these differences:

| | Desktop | Web |
|---|---|---|
| Saving | Writes files into the project | Writes into the page's memory and also downloads the file, since the page forgets on reload |
| Building games | Yes | No (needs CMake and Emscripten); profiles can be edited |
| Thumbnails | Every file, in the background | Only what you look at |
| Opening a script with the system's editor | Yes (the **Open** button) | No |
| Window layout | Restored from the last session | Starts at the defaults |
| Ctrl+P | Play | The browser's Print: use the Play button |
