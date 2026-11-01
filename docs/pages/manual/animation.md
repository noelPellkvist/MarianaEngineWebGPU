---
title: Animation
description: Play clips, blend and layer them, build state machines in the Animation Graph editor, and use IK, root motion and animation events.
---

# Animation

Animated models (skinned characters, moving machinery, anything with animations in its glTF file) are played by an **Animator** component on the model's root. An Animator either plays **clips** directly, with cross-fades between them, or runs an **animation graph**: blend trees, layers, state machines and IK, driven by **parameters** your scripts set.

![The Animation Graph window with the dancer's graph](images/animation-graph.jpg "Window > Animation Graph on a dancer while playing: the selection and the parameters (with their live values) on the left, the nodes on the canvas, the Animator in the Inspector.")

## The Animator

Drop an animated model into the scene and its root gets an Animator, playing the file's first animation in a loop. Select the root to see it in the Inspector:

| Setting | |
|---|---|
| **Graph** | The animation graph it runs, if any. Pick one, drop an `.animgraph` here, **Edit graph** to open it, or **New graph** to make one with a state for each clip |
| **Playing**, **Loop**, **Speed**, **Time** | Playback. While the game is stopped, dragging *Time* poses the model |
| **Root Motion** | Let the clips move the model itself (see [Root motion](#root-motion)) |
| **Clips** | The clips it knows, each with a name (what graphs and scripts call it), **Play**, **Fade** and a cross-fade time. Drop a `.glb` here to add its animations |
| **Events** | Per clip: named moments that call scripts (see [Animation events](#animation-events)) |
| **Parameters** | The graph's parameters, live: change them while playing to try the graph |
| **Nodes** | The graph's nodes with their current weights and the state each state machine is in, with a preview of each state |

### Clips without a graph

Without a graph, the Animator plays one clip at a time:

- **Play** switches at once.
- **Fade** cross-fades to it over the cross-fade time: the new clip fades in while the old one keeps playing underneath until it's gone.

From a script:

```luau
local animator = self.entity:Get("Animator")
animator:CrossFade("Run", 0.25)       -- by name, a quarter of a second
animator:Play("Idle")                  -- at once
animator.speed = 1.5
```

## Animation graphs

An **animation graph** (`.animgraph`) describes how clips mix. It's an asset: several characters can share one graph, each with its own parameter values.

### Parameters

Parameters are the graph's inputs, set by scripts (or in the Inspector to try things out):

| Type | Used for | From a script |
|---|---|---|
| **Float** | Blend amounts, speeds, conditions (`speed > 0.1`) | `animator:SetFloat("speed", 3.2)` |
| **Bool** | On/off conditions (`grounded`) | `animator:SetBool("grounded", true)` |
| **Trigger** | One-shot conditions (`jump`): reset by the transition that uses it | `animator:SetTrigger("jump")` |
| **Vector** | World positions for IK targets (where to look, where to put a hand) | `animator:SetVector("lookTarget", target)` |

### Nodes

| Node | What it does |
|---|---|
| **Clip** | Plays a clip by name, with its own speed and looping. Clips in the same **sync group** stay in step (a walk and a run keep their feet together while blending) |
| **Blend** | Mixes two inputs by a weight: a constant or a float parameter (0 = the first, 1 = the second) |
| **Blend 1D** | Mixes inputs placed along one float parameter: idle at 0, walk at 1.5, run at 4 m/s |
| **Blend 2D** | Mixes inputs placed on two float parameters: strafing in eight directions |
| **Additive** | Adds a clip's movement on top of another (a breathing or leaning layer), relative to its first frame or the rest pose |
| **Layer** | Puts a second input over the first, on chosen bones only (the **mask**: an upper body waving while the legs walk) |
| **State Machine** | States (each one an input node) and **transitions** between them |
| **Two Bone IK** | Bends a limb (upper arm, forearm, hand; thigh, shin, foot) so its end reaches a Vector target, with an optional pole for the elbow or knee |
| **Look At** | Turns a bone (the head) and some above it (neck, spine) toward a Vector target, within an angle limit |

### State machines

A state machine is in one **state** at a time; **transitions** move it to another with a cross-fade.

| Transition setting | |
|---|---|
| **From**, **To** | The states. *Any State* transitions can start from every state |
| **Duration** | The cross-fade, in seconds |
| **Exit time** | Only leave once the current state has played this far (1 = one full loop); off = any time |
| **Interruptible** | Whether another transition may start while this one runs |
| **Conditions** | All must hold: a parameter compared with a value (*if*, *if not*, *greater*, *less*, *equals*, *not equals*) |

Transitions are tried in order; the first whose conditions hold starts. A trigger used by a transition is reset when it starts, so `SetTrigger("jump")` causes exactly one jump.

Scripts can also jump straight to a state:

```luau
animator:CrossFadeState("Dance", 0.3)
```

### The Animation Graph window

Open it from **Window > Animation Graph**, by double-clicking an `.animgraph` in the Assets window, or with **Edit graph** on an Animator.

- **Pan** with the middle or right mouse button, **zoom** with the wheel.
- **Right-click** empty space to add a node; right-click a node for its menu (open, make output, duplicate, delete).
- **Drag** from a node's output pin to another's input to connect them; drag a link off an input to move or remove it.
- **Double-click** a state machine to open it: states are boxes (the entry state is marked), transitions are arrows. Right-click a state to start a transition (then click its target) or make it the entry; right-click empty space to add a state. **Backspace** goes back up.
- The **side panel** edits the selected node, state or transition, the graph's **parameters** and the clips the graph brings with it.
- While an entity using the graph is selected, the graph is **live**: node weights, the flow along links and the current state with its progress are drawn as the game runs.
- **Arrange** lays the graph out automatically. **Save** (Ctrl+S) writes the file; Ctrl+S in the scene saves changed graphs too.

Every edit is an undo step.

## Inverse kinematics (IK)

IK nodes adjust the animated pose so a body part reaches a point in the world:

- **Two Bone IK** for limbs: a hand on a door handle, a foot on a step. The target is a Vector parameter; the optional pole (another Vector) says which way the elbow or knee points. Out of reach, the limb points straight at the target.
- **Look At** for heads: the head (and a chain of bones above it, sharing the turn) faces the target, never turning more than the angle limit from where the animation has it.

Both have a **weight** (a constant or a float parameter) to blend them in and out. The Sandbox's dancers use a Look At on head, neck and chest, aimed at the car by `WatchCar.luau`:

```luau
local animator = self.entity:Get("Animator")
animator:SetVector("lookTarget", car.worldPosition + vec3(0, 1, 0))
animator:SetFloat("lookWeight", 1)
```

## Root motion

Some animations move the character: a walk cycle that travels forward, a turn on the spot. With **Root Motion** on, the Animator takes that travel out of the hips and moves the **entity** instead, so the character walks exactly as far as its feet do. Root motion works through blends and state machines.

On a [character controller](gameplay-physics.md#characters), root motion becomes the character's walk, with gravity, stairs and walls applying as usual.

From a script, `animator:GetRootVelocity()` tells you how fast the animation is moving the character (metres per second, in the model's own axes) and turning it (degrees per second).

## Animation events

**Events** are named moments in a clip: `Footstep` at 0.4 s of *Walk*, `Swing` at 0.2 s of *Attack*. When a playing clip passes one, every script on the entity gets a call:

```luau
function Character.OnAnimationEvent(self: Script, name: string, clip: string)
    if name == "Footstep" then
        print(`step during {clip}`)
    end
end
```

Add events in the Animator's **Events** section (per clip: **Add** puts one at the current time; set its name and time) or for a graph's own clips in the Animation Graph window. A clip fires its events only while it makes up at least a quarter of the pose, so a clip faded in at 10% stays quiet; each event name fires at most once per frame. Events are saved with the scene and the graph.

## Ragdolls

An animated character can become a ragdoll: physics bodies on its bones that follow the animation or fall limp when hit. See [Ragdolls](gameplay-physics.md#ragdolls).
