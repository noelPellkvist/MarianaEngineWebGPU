--!strict
-- Ripples: a dynamic mesh whose vertices move every frame, rings of waves spreading from the middle.
-- Put it on an empty entity.
local Ripples = {} :: Script

Ripples.properties = {
    size = 10,    -- metres across
    cells = 30,   -- grid cells along each side
    speed = 2,    -- how fast the rings move out
    waveHeight = 0.2,
}

function Ripples.Start(self: Script)
    local cells: number = self.cells
    local size: number = self.size
    local positions, indices = {}, {}
    for row = 0, cells do
        for col = 0, cells do
            table.insert(positions, vec3((col / cells - 0.5) * size, 0, (row / cells - 0.5) * size))
        end
    end
    local across = cells + 1
    for row = 0, cells - 1 do
        for col = 0, cells - 1 do
            local a = row * across + col + 1
            for _, i in { a, a + across, a + 1, a + 1, a + across, a + across + 1 } do
                table.insert(indices, i)
            end
        end
    end

    self.flat = table.clone(positions) -- where each vertex rests
    self.positions = positions        -- reused every frame
    -- dynamic: Update can move its vertices (as many as it was made with)
    self.surface = mesh.new{ positions = positions, indices = indices, smooth = true, dynamic = true }
    self.entity:SetMesh(self.surface)
    local water = self.entity:GetMaterial()
    if water then
        water.color = vec3(0.1, 0.35, 0.5)
        water.roughness = 0.1
    end
end

function Ripples.Update(self: Script, dt: number)
    local t = time.time * (self.speed :: number)
    local waveHeight: number = self.waveHeight
    local flat: { vector } = self.flat
    local positions: { vector } = self.positions
    for i, p in flat do
        local distance = vector.magnitude(vec3(p.x, 0, p.z))
        positions[i] = vec3(p.x, math.sin(distance * 1.5 - t) * waveHeight, p.z)
    end
    -- Smooth normals are worked out again from the moved positions.
    self.surface:Update{ positions = positions }
end

return Ripples
