---
title: Keyboard Shortcuts
description: Every keyboard shortcut and mouse control in the Mariana editor.
---

# Keyboard Shortcuts

## Files and building

| Shortcut | |
|---|---|
| Ctrl+N | New scene |
| Ctrl+O | Open scene |
| Ctrl+S | Save the scene and every changed asset (in the Script Editor or Animation Graph window: save that file) |
| Ctrl+Shift+S | Save scene as |
| Ctrl+B | Build the active build profile |
| Ctrl+Shift+B | Build and run |

## Editing

| Shortcut | |
|---|---|
| Ctrl+Z | Undo |
| Ctrl+Y, Ctrl+Shift+Z | Redo |
| Delete | Delete the selected entity (and everything below it) |
| 1 | Gizmo: move |
| 2 | Gizmo: rotate |
| 3 | Gizmo: scale |
| F11 | Fullscreen |

A text field you are typing in keeps its own Ctrl+Z, Delete and number keys.

## Play mode

| Shortcut | |
|---|---|
| Ctrl+P | Play / Stop |
| Ctrl+Shift+P | Pause / Resume |
| Ctrl+Alt+P | Step one frame (1/60 s) |

On the web, Ctrl+P is the browser's Print: use the buttons in the menu bar.

## Scene view camera

These work while the mouse is over the Scene view.

| Control | |
|---|---|
| Right mouse button + drag | Look around |
| Right mouse button + W A S D | Fly forward, left, back, right |
| Right mouse button + Space / Shift | Fly up / down |
| Middle mouse button + drag | Pan |
| Mouse wheel | Move forward / back |
| Left click | Select what's under the mouse |

## Script Editor

| Shortcut | |
|---|---|
| Ctrl+S | Save (a running game reloads the script) |
| Ctrl+Space | Show completions |
| Up / Down | Choose a completion |
| Enter, Tab | Take the completion |
| Escape | Close the completions |

Completions also open by themselves after `.`, `:` and `"`.

## Animation Graph window

| Control | |
|---|---|
| Ctrl+S | Save the graph |
| Middle or right mouse button + drag | Pan |
| Mouse wheel | Zoom |
| Right click | Add a node or state; a node's or state's menu |
| Double-click a state machine | Open it |
| Double-click a state | Go to its node |
| Backspace | Back up one level |
| Delete | Delete the selected node, state or transition |
| Escape | Cancel making a transition |

## Profiler

| Control | |
|---|---|
| Left / Right | The previous / next frame (while paused) |
| Click the frame graph | Select that frame (and pause) |
| Mouse wheel on the timeline | Zoom |
| Drag the timeline | Pan |
| Double-click the timeline | Show the whole frame |

## In your game

The keys your scripts read are up to you: see [input](../scripting/api/input.md). The Sandbox's scripts use:

| Keys | |
|---|---|
| W A S D / arrows | Drive or walk |
| Space | Hand brake / jump |
| Shift | Run |
| R | Respawn the car |
