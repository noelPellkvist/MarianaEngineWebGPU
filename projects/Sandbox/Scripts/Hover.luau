--!strict
-- Hover: bobs up and down around where it was put, turning about the world's up axis.
local Hover = {} :: Script

Hover.properties = {
    height = 0.25,   -- metres up and down
    bobSpeed = 2,    -- radians per second
    turnSpeed = 90,  -- degrees per second
}

function Hover.Start(self: Script)
    self.base = self.entity.position
    self.baseRotation = self.entity.rotation
    self.time = 0
end

function Hover.Update(self: Script, dt: number)
    self.time += dt
    self.entity.position = self.base + vec3(0, math.sin(self.time * self.bobSpeed) * self.height, 0)
    self.entity.rotation = quat.angleAxis(self.time * self.turnSpeed, vec3(0, 1, 0)) * self.baseRotation
end

return Hover
