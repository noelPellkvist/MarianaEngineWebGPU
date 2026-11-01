---
title: The Script Editor, Types and VS Code
nav: Script Editor and types
description: Edit scripts in the built-in Script Editor with live type checking and autocomplete, or in VS Code with the Luau Language Server.
---

# The Script Editor, Types and VS Code

## The Script Editor

The editor has a Luau editor built in: **Window > Script Editor**, or double-click a `.luau` file in the Assets window, or click **Edit** next to a script in the Inspector.

![The Script Editor with the Snake script open](images/script-editor.jpg "The Script Editor: syntax colours, line numbers, and problems underlined as you type.")

| Feature | |
|---|---|
| **Tabs** | One per open script, with a dot while it has unsaved changes |
| **Syntax colours** | Keywords, the engine's libraries, numbers, strings and comments |
| **Live checking** | A moment after you stop typing, problems are underlined in red; hover one for the message. The toolbar counts them and the list under the text names each (click to jump there) |
| **Autocomplete** | Opens after `.`, `:` and `"`, or with **Ctrl+Space**: an entity's methods after `:`, a component's fields after `.`, component names inside `Get("`, plus your own variables and the keywords |
| **Save** | **Ctrl+S** writes the file; a running game reloads it at once |
| **External changes** | A tab without unsaved typing follows the file when it changes on disk (you edited it elsewhere) |

In the autocomplete list, **Up** and **Down** choose, **Enter** or **Tab** inserts, **Esc** closes.

## Type checking

Scripts are checked by Luau's own type checker against the engine's API, so mistakes show before you play:

- a misspelt function, method or property (`self.entity:Rotat(...)`),
- a component that doesn't exist (`entity:Get("RigidBdy")`) or a field it doesn't have (`body.masss = 2`),
- an enum value that isn't one (`motion = "Floating"`),
- the wrong type of argument (`entity:Rotate(45, vec3(0, 1, 0))`).

The Inspector also shows the number of problems in each script and the first few, so you notice them without opening the script.

**Strict mode**: a script whose first line is `--!strict` is checked fully; without it, checking is looser (unknown things are allowed). New scripts start in strict mode, as do the Sandbox's.

For the checker to know what `self` is, scripts are written in this form:

```luau
--!strict
local Enemy = {} :: Script

function Enemy.Update(self: Script, dt: number)
    self.entity.position += vec3(0, 0, dt)   -- checked: self.entity is an Entity
end

return Enemy
```

`function Enemy:Update(dt)` (with a colon) runs exactly the same, but the checker can't type `self` there, so the function's body isn't checked. Prefer the dot form.

The types available in scripts: `Script`, `Entity`, `quat`, `vector`, `Component`, `Contact`, `RaycastHit`, `QueryOptions`, `BodyInfo`, `VehicleInfo`, `WheelInfo`, `CharacterInfo`, `ComponentName` and one per component (`RigidBody`, `Vehicle`, `Animator`, …). `entity:Get("RigidBody")` is a `RigidBody?`, so after an `if body then` the checker knows its fields.

> [!NOTE]
> Type problems never stop a script from running. They're there to help; a script with a type warning still runs exactly as written.

## VS Code

You can write scripts in Visual Studio Code with the same checking and autocomplete:

1. Install the **Luau Language Server** extension (by JohnnyMorganz).
2. Open the project's folder (for example `projects/Sandbox`) in VS Code.

The desktop editor writes the engine's types to `.mariana/types/Mariana.d.luau` in the project (and again whenever the components change), and, if the project has none, a `.vscode/settings.json` that points the extension at them:

```json title=".vscode/settings.json"
{
    "luau-lsp.platform.type": "standard",
    "luau-lsp.types.definitionFiles": [".mariana/types/Mariana.d.luau"]
}
```

Save in VS Code and a running game in the editor reloads the script, as with the built-in editor.

> [!TIP]
> The `Mariana.d.luau` file is also the most precise reference there is for the API: every global, method and component field with its type.

## Other editors

Any editor works: scripts are plain text files. The **Open** button next to a script in the Inspector opens it with the program your system uses for `.luau` files.
