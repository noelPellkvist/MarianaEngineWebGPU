---
title: task
description: Functions that wait in Mariana scripts: task.spawn, task.delay, task.wait and task.cancel, on game time.
---

# task

A script function has to return so the frame can go on, so it can't wait. A **task** can: it is a function that runs a bit, waits (`task.wait`), and goes on from there later, while the game keeps running.

```luau
function Door.Open(self: Script)
    task.spawn(function()
        self.entity:Get("Joint").motorTarget = 90
        task.wait(3)                          -- three seconds later...
        self.entity:Get("Joint").motorTarget = 0
    end)
end
```

| Function | |
|---|---|
| `task.spawn(fn, ...) -> thread` | Runs `fn(...)` now, until it waits or ends |
| `task.delay(seconds, fn, ...) -> thread` | Runs `fn(...)` after `seconds` of game time |
| `task.wait(seconds?) -> number` | Inside a task only: goes on after `seconds` (none: the next frame). Returns how long it waited |
| `task.cancel(task)` | Stops a task (it never goes on) |

## How tasks run {#how-tasks-run}

- **Game time.** Waits count game time: they stand still while the game is paused and follow [`time.scale`](time.md). A wait ends on the first frame at or past its time, so `task.wait(0.5)` at 60 fps goes on 30 frames later.
- **Once a frame, after `Update`.** Tasks whose wait is over go on after every script's `Update` (in the order they were started). A task goes on at most once a frame, so a loop with `task.wait()` does one round per frame.
- **They belong to a script.** A task started by a script's function (its `Start`, `Update`, an event...) belongs to that script on that entity: it waits while the entity is switched off or the script disabled, and ends with the script (its entity destroyed, `RemoveScript`, an error in the script, Stop).
- **Errors** in a task are logged with its script's name and end the task (not the script).
- **The 2 s limit** applies to each run of a task, as to any call: a task that loops forever must wait in its loop.
- `task.wait` outside a task is an error: `Update`, `Start` and the events can't wait (start a task from them instead).

## Repeating {#repeating}

A loop with a wait in it repeats something without a timer:

```luau
function Beacon.Start(self: Script)
    task.spawn(function()
        while true do
            self.entity.visible = not self.entity.visible
            task.wait(0.5)
        end
    end)
end
```

It stops with its script (its entity destroyed, Stop). To stop it earlier, keep what `task.spawn` returned and cancel it:

```luau
self.blinking = task.spawn(blink)
-- later
task.cancel(self.blinking)
```

## Sequences {#sequences}

Tasks read in order, which suits anything scripted in steps: a countdown, waves of enemies, a cutscene. See [Waves of enemies](../cookbook.md#waves-with-tasks):

```luau
task.spawn(function()
    for count = 3, 1, -1 do
        print(count)
        task.wait(1)
    end
    print("Go!")
end)
```

## Tasks and timers {#tasks-and-timers}

Counting a timer down in `Update` (see [Timers and cooldowns](../cookbook.md#timers-and-cooldowns)) still works and is the better fit for something checked every frame anyway (a cooldown, a speed-up). Tasks are clearer for one-offs and sequences.

Tasks are Luau coroutines run by the engine; Luau's own `coroutine` library works too, but nothing resumes those for you.
