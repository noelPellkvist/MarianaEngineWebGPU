--!strict
-- Explosion: press X to blow up around this entity: every dynamic body within `radius` is thrown
-- outward and up, harder the closer it is. Ragdolls caught in it go limp and are thrown too.
local Explosion = {} :: Script

Explosion.properties = {
    radius = 8,
    strength = 40,   -- N s at the centre
    key = "x",
}

-- The entity with the Ragdoll at or above `entity` (a ragdoll's bodies are its bones), if any.
local function ragdollRoot(entity: Entity): Entity?
    local e: Entity? = entity
    while e do
        if e:Has("Ragdoll") then
            return e
        end
        e = e.parent
    end
    return nil
end

function Explosion.Update(self: Script, dt: number)
    if not input.GetKeyDown(self.key) then
        return
    end
    local center = self.entity.worldPosition
    for _, body in physics.OverlapSphere(center, self.radius, { ignore = self.entity }) do
        local offset = body.worldPosition - center
        local distance = vector.magnitude(offset)
        local falloff = 1 - math.clamp(distance / self.radius, 0, 1)
        local direction = if distance > 0.01 then offset / distance else vec3(0, 1, 0)
        local impulse = (direction + vec3(0, 0.6, 0)) * self.strength * falloff
        local ragdoll = ragdollRoot(body)
        if ragdoll then
            physics.SetRagdollMode(ragdoll, "Limp")
            physics.AddRagdollImpulse(body, impulse * 0.2)   -- per bone: a ragdoll has many
        else
            physics.AddImpulse(body, impulse)   -- false (and nothing) for bodies that aren't dynamic
        end
    end
end

return Explosion
