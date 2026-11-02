---
title: Meshes, Materials and Textures
nav: Meshes and materials
description: Building meshes at run time from Luau, updating them every frame, changing materials and colours, and loading textures.
---

# Meshes, Materials and Textures

Scripts can build their own meshes (terrain, ropes, trails, procedural buildings, anything made of triangles), change how surfaces look, and load textures.

```luau
-- A triangle, drawn by this entity
local tri = mesh.new{
    positions = { vec3(0, 0, 0), vec3(0, 1, 0), vec3(1, 0, 0) },
}
self.entity:SetMesh(tri)

-- Make it gold
local gold = self.entity:GetMaterial()
if gold then
    gold.color = vec3(1, 0.8, 0.2)
    gold.metallic = 1
    gold.roughness = 0.3
end
```

## Meshes

### `mesh.new(data) -> Mesh` {#mesh-new}

Builds a mesh from a table:

| Field | | |
|---|---|---|
| `positions` | required | Each vertex's position: a table of vectors, or a `buffer` of 32-bit floats (x, y, z per vertex) |
| `normals` | optional | Each vertex's normal, the same way. Without them, triangles get flat normals (each face its own), or smooth ones with `smooth = true` |
| `uvs` | optional | Each vertex's texture coordinates (vectors' x and y; buffers: 2 floats per vertex) |
| `colors` | optional | Each vertex's colour, red, green, blue from 0 to 1 (multiplies the material's colour) |
| `indices` | optional | Which vertices make each triangle (three each), line (two) or point. In a table they count from **1**; in a `buffer` of 32-bit integers from **0**. Without indices, the positions are used in order |
| `submeshes` | optional | Instead of `indices`: a list of index lists, each drawn with its own material slot (the first with slot 1) |
| `topology` | `"triangles"` | `"triangles"`, `"lines"` or `"points"` |
| `smooth` | `false` | Without `normals`: smooth normals (shared at each vertex) instead of flat ones |
| `dynamic` | `false` | The mesh can be changed later with `Update` |

Optional attributes need one value per position. Mistakes are errors with what was wrong: an index past the last vertex, a field that doesn't exist, three positions and two colours.

**Which side is the front.** A triangle's front is the side where its corners go round **anticlockwise**: the side `cross(b - a, c - a)` points to. Only the front is drawn, unless the material is `doubleSided`.

```luau title="A quad facing up, two triangles sharing an edge"
local quad = mesh.new{
    positions = { vec3(0, 0, 0), vec3(1, 0, 0), vec3(1, 0, 1), vec3(0, 0, 1) },
    uvs = { vec3(0, 0, 0), vec3(1, 0, 0), vec3(1, 1, 0), vec3(0, 1, 0) },
    indices = { 1, 3, 2,  1, 4, 3 },
    smooth = true,
}
```

### Buffers for big meshes

Tables are easy to build; for meshes with many thousands of vertices, Luau's [`buffer`](../luau.md#the-standard-library) is quicker to fill and to read. Positions, normals and colours take 3 floats per vertex, uvs 2, and indices are 32-bit unsigned integers counting from 0:

```luau
local positions = buffer.create(3 * 3 * 4)        -- 3 vertices x 3 floats x 4 bytes
local corners = { 0, 0, 0,  0, 1, 0,  1, 0, 0 }
for i, v in corners do
    buffer.writef32(positions, (i - 1) * 4, v)
end
local tri = mesh.new{ positions = positions }
```

### Mesh fields

| Field | | |
|---|---|---|
| `vertexCount` | number | How many vertices it was made with |
| `boundsMin`, `boundsMax` | vector | The corners of the box around it, in its own space |
| `dynamic` | boolean | Whether `Update` can change it |

### `mesh:Update(data)` {#update}

Changes a **dynamic** mesh's vertices: give any of `positions`, `normals`, `uvs` and `colors`, as many as it was made with. What you leave out stays as it was, except that normals the mesh works out itself (flat or smooth) follow the new positions. The triangles stay the same: for another shape, make a new mesh.

```luau
self.surface:Update{ positions = positions }   -- every frame: a waving surface
```

See the [moving surface recipe](../cookbook.md#a-moving-surface).

## Putting meshes on entities

### `entity:SetMesh(mesh, materials?)` {#setmesh}

Draws the mesh on the entity: gives it a Mesh Renderer (casting shadows) if it has none, or replaces the mesh it draws. `materials` is a list for its slots; without it, the entity keeps its materials (or uses the default white one).

```luau
local e = scene.Create("Rock")
e:SetMesh(rockMesh, { stone })
```

### `entity:GetMesh() -> Mesh?` {#getmesh}

The mesh the entity draws: one a script made, or a model's (whose size and bounds you can read).

### Colliders

A [Mesh Collider](component-reference.md#colliders) uses the entity's mesh, so a generated mesh can be solid:

```luau
self.entity:SetMesh(ground)
self.entity:Add("MeshCollider")      -- a static body in the mesh's shape
```

On a dynamic rigid body, add it with `convex = true`. A collider keeps the shape the mesh had when the collider was made: updating a dynamic mesh doesn't move its collider.

## Materials

A **material** says how a surface looks. Each slot of an entity's Mesh Renderer has one.

### `entity:GetMaterial(slot?) -> Material?` {#getmaterial}

The material in a slot (from 1; default 1), or `nil` without a Mesh Renderer. The **first time** a script asks for an entity's material, the entity gets a copy of its own: changing it changes this entity only, never another entity or a model that shares the material, and nothing your game does to it is saved in the scene. Later calls return the same copy.

```luau
local m = self.entity:GetMaterial()
if m then
    m.color = vec3(0.2, 0.6, 1)
end
```

### `entity:SetMaterial(material, slot?)` {#setmaterial}

Puts a material in a slot (default 1). Several entities can share one material: change it once and they all change.

### `material.new(fields?) -> Material` {#material-new}

A new material (white, fully rough, no textures), with any fields you give:

```luau
local glass = material.new{ color = vec3(0.8, 0.9, 1), alpha = 0.3, alphaMode = "Blend", roughness = 0.05 }
```

### `material:Clone() -> Material` {#clone}

A new material with the same settings and textures.

### Material fields

| Field | Type | Default | |
|---|---|---|---|
| `color` | vector | 1, 1, 1 | Base colour (red, green, blue, 0–1) |
| `alpha` | number | 1 | Opacity, used by the `Blend` and `Mask` modes |
| `metallic` | number | 0 | 0 = paint, plastic, wood; 1 = metal |
| `roughness` | number | 1 | 0 = mirror-smooth, 1 = matte |
| `emissive` | vector | 0, 0, 0 | Light it gives off; above 1 glows (with bloom) |
| `normalStrength` | number | 1 | How strongly the normal map bends the light |
| `occlusionStrength` | number | 1 | How strongly the occlusion map darkens |
| `clearcoat`, `clearcoatRoughness` | number | 0, 0 | A varnish layer (car paint) |
| `doubleSided` | boolean | false | Draw the back of triangles too |
| `alphaMode` | string | `"Opaque"` | `"Opaque"`, `"Mask"` (cut out where alpha is under `alphaCutoff`) or `"Blend"` (see-through) |
| `alphaCutoff` | number | | Setting it makes the material `"Mask"` with that cutoff |
| `tiling` | vector | 1, 1 | How many times the textures repeat across the uvs (x, y) |
| `offset` | vector | 0, 0 | Moves the textures across the uvs |
| `baseColorMap`, `normalMap`, `metallicRoughnessMap`, `occlusionMap`, `emissiveMap` | `Texture?` | none | The textures, or `nil` to take one off |
| `name` | string | | Read only |

A field that doesn't exist is an error.

## Textures

### `texture.load(path) -> Texture` {#texture-load}

An image file by resource path (PNG or JPEG). Put it in a material's texture fields:

```luau
local grass = texture.load("/Textures/Grass.png")
local m = self.entity:GetMaterial()
if m then
    m.baseColorMap = grass
    m.tiling = vec3(8, 8, 0)
end
```

The image is loaded the first time it is put in a material, in the colour space its slot needs (colour maps as colours, normal and roughness maps as data), and shared after that. A Texture has one field, `path`.

> [!IMPORTANT]
> A [build](../../building/build-profiles.md) packages the files your scenes use. A texture or model that only a script loads isn't one of them: add its folder to the build (turn on **Whole project**), or use it in a scene too.

## Play mode and saving

Meshes and materials made by scripts exist while the game runs. When you press **Stop**, the scene goes back to how it was, and so do the entities' meshes and materials. Scene files can't save a generated mesh: build it in `Start` every time the game runs.
