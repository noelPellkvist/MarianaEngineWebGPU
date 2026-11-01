--!strict
-- Laser: casts a ray along its entity's forward every frame and puts a small marker where it hits.
-- Click (left mouse button) to push whatever it points at.
local Laser = {} :: Script

Laser.properties = {
    range = 50,
    push = 15,   -- N s, the impulse a click gives
}

function Laser.Start(self: Script)
    self.marker = scene.SpawnPrimitive("sphere", nil, vec3(1, 0.1, 0.1))
    self.marker.name = "Laser Dot"
    self.marker.scale = vec3(0.15, 0.15, 0.15)
end

function Laser.Update(self: Script, dt: number)
    local origin = self.entity.worldPosition
    local direction = self.entity.forward
    -- Ignore the entity itself, in case it has a collider.
    local hit = physics.Raycast(origin, direction, self.range, { ignore = self.entity })
    if hit then
        self.marker.worldPosition = hit.point
        if input.GetMouseButtonDown(0) then
            print(`Hit {hit.entity.name} at {string.format("%.1f", hit.distance)} m`)
            physics.AddImpulseAtPoint(hit.entity, direction * self.push, hit.point)
        end
    else
        self.marker.worldPosition = origin + direction * self.range
    end
end

function Laser.OnDestroy(self: Script)
    if self.marker and self.marker:IsValid() then
        self.marker:Destroy()
    end
end

return Laser
