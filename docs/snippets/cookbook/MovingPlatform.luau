--!strict
-- MovingPlatform: moves back and forth between where it starts and `offset` away. Give the entity a
-- Kinematic Rigid Body and a collider: physics then carries characters and pushes crates with it.
local MovingPlatform = {} :: Script

MovingPlatform.properties = {
    offset = vec3(0, 0, 8),   -- the far end, from the start
    period = 6,               -- seconds there and back
}

function MovingPlatform.Start(self: Script)
    self.start = self.entity.position
    self.time = 0
end

function MovingPlatform.Update(self: Script, dt: number)
    self.time += dt
    -- 0 at the start, 1 at the far end, eased at both ends.
    local t = 0.5 - 0.5 * math.cos(self.time / self.period * 2 * math.pi)
    self.entity.position = self.start + self.offset * t
end

return MovingPlatform
