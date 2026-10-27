-- Respawn: press R to put the vehicle (or anything with a body) back where it started, at rest.
local Respawn = {}

Respawn.properties = {
    key = "R",
    lift = 0.3, -- metres above the start, so it drops onto its wheels
}

function Respawn:Start()
    self.start = self.entity.worldPosition
    self.rotation = self.entity.worldRotation
end

function Respawn:Update(dt: number)
    if input.GetKeyDown(self.key) then
        physics.Teleport(self.entity, self.start + vec3(0, self.lift, 0), self.rotation)
        physics.SetVelocity(self.entity, vec3(0, 0, 0), vec3(0, 0, 0))
    end
end

return Respawn
