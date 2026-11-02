---
title: Play Mode
description: Play, pause and step your game in the editor, and keep the changes you want when you stop.
---

# Play Mode

Press **Play** to run the game inside the editor: scripts start, physics, animation and vehicles run, and the keyboard and mouse go to the game. Press **Stop** and the scene goes back to exactly how it was.

## The play controls

The buttons are in the middle of the menu bar.

| Button | Shortcut | |
|---|---|---|
| **Play / Stop** | Ctrl+P | Starts the game; while it runs, stops it |
| **Pause / Resume** | Ctrl+Shift+P | Freezes the game's clock; scripts, physics and animation hold still |
| **Step** | Ctrl+Alt+P | While paused, runs one frame (1/60 s) |

While the game runs, its clock (seconds since Play) is shown next to the buttons.

> [!NOTE]
> In the web editor, Ctrl+P is the browser's Print. Use the Play button there.

## What runs, and when

- **Game time** only moves while playing (and not while paused): scripts' `dt`, physics and animations all use it. While stopped, animations hold their pose; the Animator's time slider in the Inspector still poses the model.
- **Physics bodies** are made when you press Play and dropped when you stop, so every Play starts from the same state and runs the same way.
- **Scripts** start (their `Start` runs) on the first frame and end (`OnDestroy`) when you stop.
- **Input** goes to the game while the mouse isn't flying the editor camera (right mouse button) and you aren't typing in a text field. The mouse position scripts read is relative to the Game window's image, and [`screen.Ray`](../scripting/api/screen.md) looks through the scene's first camera there.
- **The cursor**: a game that locks or hides it (mouse look) gets it while it plays and has the keys. **Escape** gives it back, and so do Pause and Stop.
- **Scene switches**: a script's [`scene.Load`](../scripting/api/scene.md#load) replaces the scene until you stop; Stop brings back the one you were editing.
- **Gizmos** that scripts draw show over both the Scene and the Game window.
- **Save data** scripts keep with [`save`](../scripting/api/save.md) goes to the project's `.mariana/save.json`, apart from a built game's: delete it to start afresh.

Watch the game in the **Game** window. The **Scene** window still works while playing: fly around, select things and watch their components change live in the Inspector.

## Editing while playing

You can change anything while the game runs: move entities, change a rigid body's mass, edit a script's properties. Saving a script while playing reloads it into the running game, and every instance keeps its state.

When you stop, the **scene** goes back to how it was when you pressed Play, and so does the undo history, the **lighting** and the **render pipeline** (scripts may change them; so tune those while stopped). Other **asset** changes stay: edits to materials and animation graphs made while playing aren't undone by stopping.

## Keeping changes

Sometimes the game gets something right that you want to keep: a stack of crates that settled just so, a value you tuned while watching. While playing:

1. Select the entity.
2. In the Inspector, tick **Keep changes after Stop**. Tick **With children** too to keep everything below it (a vehicle's wheels, a model's bones).

You can also right-click the entity in the Hierarchy and choose **Keep changes after Stop**. Kept entities say *keep* in the Hierarchy.

When you stop, everything else goes back, and the kept entities get their components as the game left them: positions, rotations, every setting. The change is one undo step, *Keep play-mode changes of …*, so Ctrl+Z puts them back too.

> [!TIP]
> Entities created while playing (by scripts, or by you) can't be kept: they didn't exist before Play, so stopping removes them.

## Saving and playing

Saving is off while playing. A scene is saved as you edited it, so stop first, then press Ctrl+S.
