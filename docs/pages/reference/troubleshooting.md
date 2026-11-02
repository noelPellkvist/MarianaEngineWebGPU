---
title: Troubleshooting
description: Solutions to common problems with the Mariana editor, scripts, imported assets, physics and builds.
---

# Troubleshooting

The first place to look is always the **log** (in the terminal the editor runs from, or the browser's console on the web): warnings and errors say what went wrong and usually which file or entity.

## Editor and graphics

**The window is black, or the editor doesn't start.**
Mariana needs a graphics card with Direct3D 12 drivers on Windows, or a browser with WebGPU. Update your graphics drivers.

**The web editor or a web build shows a black page.**
- Opened from disk (`file://`)? Browsers only run WebGPU on pages served over `http://localhost` or `https://`. Serve the folder ([Web Builds](../building/web.md#testing-it-locally)).
- Does the browser have WebGPU? Current Chrome and Edge do. Check the browser's developer console for the reason.

**A window I closed is gone.**
Open it again from the **Window** menu.

## Scripts

**My script doesn't run.**
- Scripts only run while **playing** (Ctrl+P), and only on **active** entities: check the entity's (and its parents') **Active** box.
- Is the script **enabled** on the entity (the checkbox in its Scripts section)?
- Did it fail? A script that raised an error stops for that entity until you change it: the Inspector shows the error under the script, and the log has the full message with its line.
- Does the file **return** the script's table on its last line (`return MyScript`)?

**`attempt to index nil with 'position'` (or similar).**
Something you looked up wasn't there: `scene.Find` didn't find the name, `entity:Get` the component, or an entity was destroyed. Check for `nil` before using it (see [Narrowing nil](../scripting/api/types.md#narrowing-nil)).

**`the script ran too long (an endless loop?)`.**
One call ran for more than 2 seconds, usually a `while` loop whose condition never changes. A loop that should go on over time must wait in it: run it as a [task](../scripting/api/task.md) with `task.wait()` in the loop, or keep state in `self` and do a bit every `Update` (see [Timers](../scripting/cookbook.md#timers-and-cooldowns)).

**Keys do nothing while playing in the editor.**
The game doesn't get the keyboard while a text field is focused (click the Game view or empty space) or while the right mouse button flies the Scene camera. See [input](../scripting/api/input.md#in-the-editor).

**My properties went back to their defaults.**
Values you set in the Inspector **while playing** reset when you press Stop, like everything else. Set them while stopped, or keep them with [Keep changes after Stop](../manual/play-mode.md#keeping-changes).

**Changes to the script file don't show up.**
The editor reloads a script when its file changes (and the running game picks it up). If the new version doesn't compile, the old one keeps running and the error is shown: fix it and save again.

**`some fields given for RigidBody aren't among its fields`.**
A field name passed to `Add` is misspelt. The [Component Reference](../scripting/api/component-reference.md) lists every field; the type checker underlines unknown ones.

## Models and materials

**My model doesn't import.**
Mariana imports **glTF 2.0** (`.glb` or `.gltf`). Export other formats (FBX, OBJ, Blender files) from Blender with **File > Export > glTF 2.0**.

**It's huge or tiny.**
Mariana works in metres. Export with the model's real size (in Blender: apply the scale, *Ctrl+A > Scale*, before exporting).

**It faces the wrong way.**
Models face **+Z** with **+Y** up, as glTF defines. Blender's glTF exporter converts automatically when *+Y Up* is on (the default). Turn the entity, or the model in your modelling tool.

**It looks black or very dark.**
- A **metallic** material without much to reflect looks dark: lower *Metallic* in the material editor.
- The textures weren't found: the log names the image files it couldn't load. For a `.gltf`, keep its `.bin` and images next to it.

**My colour changes to a model's material are lost.**
They're kept with the scene: save it (Ctrl+S). To share them between scenes, **Extract materials** into `.material` files.

## Physics

**Things fall through the floor.**
- The floor needs a **collider** (a Plane primitive has none until you add one; Create > Physics > Ground has one).
- A small, fast body can pass through thin walls between steps: turn on **Continuous** in its Rigid Body.

**My object doesn't move.**
- It needs a **Rigid Body** with Motion **Dynamic** and a collider.
- Moving it from a script by setting `position` every frame fights the simulation. Push it with [forces or velocities](../scripting/api/physics.md#forces-and-velocities), or make it **Kinematic** to move it yourself.

**`OnContactBegin` / `OnTriggerEnter` never runs.**
- The script must be on the entity with the **body** (the Rigid Body; for a static collider, the collider's entity), not on a child collider.
- Triggers need **Is Trigger** on the collider, and the other thing needs a collider of its own.
- Two static colliders never touch each other.

**The car doesn't drive / the character doesn't walk.**
They're driven by the Sandbox's [VehicleDriver](../scripting/api/sandbox-scripts.md#vehicledriver) and [CharacterDriver](../scripting/api/sandbox-scripts.md#characterdriver) scripts: check the entity has the script, and that your project has the file in `Scripts/`.

**Ragdoll hits report a bone, not the character.**
That's by design: each bone is a body. Walk up `parent` to the entity with the `Ragdoll` (see [Knocking down ragdolls](../scripting/cookbook.md#knocking-down-ragdolls)).

## Building

**The Build window says a tool is missing.**
Each missing tool is named under **Build tools**. Windows builds need the engine's native build folder (where you built the editor); web builds also need the Emscripten SDK (set `EMSDK`, or pick its folder) and Ninja.

**The first web build takes forever.**
It compiles the whole engine for the web once (several minutes). Later builds only rebuild what changed.

**The built game is missing a model / script / texture.**
A build packages what your scenes use. Files that only scripts use (a model `scene.Spawn` loads) aren't in any scene: place one in a scene, or turn on **Whole project** in the profile. `MarianaExport --list` shows what a profile takes.

**The build failed with a compiler error.**
The log has the compiler's output. If you didn't change the engine's code, make sure the editor itself builds from a terminal (`cmake --build build --config Release`), and that no other build is running in the same folder.

**My changes aren't in the built game.**
A build takes the files as saved. Save the scene (and the Script Editor's tabs) first.

## Performance

**The game is slow.**
Open the [Profiler](../manual/profiler.md) (Window > Profiler) while playing: it shows per frame what took the time, down to each script function. Common causes:
- Very big scenes with many separate objects: the Stats window's draw count tells.
- Large shadow distances (the Render Pipeline window's *Shadow distance*).
- Scripts doing a lot of work for many entities every frame: move work to `Start`, or spread it out.
- A **Debug** build: build Release.
