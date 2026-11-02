--!strict
-- A hovering body: a spring pushes it up toward a height above the ground, on every physics step
-- (FixedUpdate), so it behaves the same at any frame rate.
local HoverSpring = {} :: Script

HoverSpring.properties = {
    height = 2,      -- metres above the ground
    stiffness = 30,  -- push per metre too low, per kilogram
    damping = 6,     -- how quickly it settles
}

function HoverSpring.Start(self: Script)
    if not self.entity:Has("RigidBody") then
        self.entity:Add("SphereCollider", { radius = 0.5 })
        self.entity:Add("RigidBody", { motion = "Dynamic", mass = 2 })
    end
end

function HoverSpring.FixedUpdate(self: Script, dt: number)
    local info = physics.GetBodyInfo(self.entity)
    if not info then
        return
    end
    local below = physics.Raycast(self.entity.worldPosition, vec3(0, -1, 0), 20, { ignore = self.entity })
    local gap = if below then below.distance else 20
    local push = 9.81 + (self.height - gap) * self.stiffness - info.linearVelocity.y * self.damping
    physics.AddForce(self.entity, vec3(0, math.max(push, 0) * info.mass, 0))
end

return HoverSpring
