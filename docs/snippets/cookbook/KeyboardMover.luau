--!strict
-- KeyboardMover: moves its entity with W A S D (or the arrows) on the ground plane, Q and E turn it.
local KeyboardMover = {} :: Script

KeyboardMover.properties = {
    speed = 5,        -- metres per second
    turnSpeed = 120,  -- degrees per second
}

local function axis(negative: string, positive: string): number
    return (if input.GetKey(positive) then 1 else 0) - (if input.GetKey(negative) then 1 else 0)
end

function KeyboardMover.Update(self: Script, dt: number)
    local forward = axis("s", "w") + axis("down", "up")
    local sideways = axis("a", "d") + axis("left", "right")
    local turn = axis("q", "e")

    -- Translate moves along the entity's own axes: +Z forward, +X right.
    local move = vec3(sideways, 0, forward)
    if vector.magnitude(move) > 0 then
        self.entity:Translate(vector.normalize(move) * self.speed * dt)
    end
    if turn ~= 0 then
        self.entity:Rotate(vec3(0, 1, 0), turn * self.turnSpeed * dt)
    end
end

return KeyboardMover
