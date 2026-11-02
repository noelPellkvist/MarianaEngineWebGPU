--!strict
-- ProceduralTerrain: builds hilly ground from a height function as one mesh with smooth normals,
-- coloured by height, and makes it solid with a mesh collider. Put it on an empty entity.
local Terrain = {} :: Script

Terrain.properties = {
    size = 40,   -- metres across
    cells = 40,  -- grid cells along each side
    height = 3,  -- how tall the hills are
}

local LOW = vec3(0.25, 0.45, 0.2)
local HIGH = vec3(0.6, 0.55, 0.45)

local function heightAt(x: number, z: number, height: number): number
    return (math.sin(x * 0.25) + math.cos(z * 0.2) + math.sin((x + z) * 0.1)) * height / 3
end

function Terrain.Start(self: Script)
    local cells: number = self.cells
    local size: number = self.size
    local height: number = self.height
    local positions, colors, uvs, indices = {}, {}, {}, {}

    -- A grid of (cells + 1) x (cells + 1) vertices.
    for row = 0, cells do
        for col = 0, cells do
            local x = (col / cells - 0.5) * size
            local z = (row / cells - 0.5) * size
            local y = heightAt(x, z, height)
            table.insert(positions, vec3(x, y, z))
            table.insert(uvs, vec3(col / cells, row / cells, 0))
            table.insert(colors, vector.lerp(LOW, HIGH, math.clamp(y / height * 0.5 + 0.5, 0, 1)))
        end
    end

    -- Two triangles per cell. Indices count from 1; the front is the side the triangle's corners
    -- go round anticlockwise, here the top.
    local across = cells + 1
    for row = 0, cells - 1 do
        for col = 0, cells - 1 do
            local a = row * across + col + 1
            local b = a + 1
            local c = a + across
            local d = c + 1
            for _, i in { a, c, b, b, c, d } do
                table.insert(indices, i)
            end
        end
    end

    local ground = mesh.new{ positions = positions, colors = colors, uvs = uvs, indices = indices, smooth = true }
    self.entity:SetMesh(ground)
    local surface = self.entity:GetMaterial()
    if surface then
        surface.roughness = 0.9
    end
    self.entity:Add("MeshCollider") -- uses the entity's mesh: things land on the hills
end

return Terrain
