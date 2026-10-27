--!strict
-- LaunchPad: throws whatever comes onto it up into the air (and on along the pad's forward). The
-- entity needs a trigger collider: what enters it is thrown.
local LaunchPad = {} :: Script

LaunchPad.properties = {
    up = 11,      -- m/s upward
    forward = 4,  -- m/s added along the pad's +Z
}

function LaunchPad.Start(self: Script)
    self.launches = 0
end

function LaunchPad.OnTriggerEnter(self: Script, other: Entity)
    local velocity = physics.GetVelocity(other)
    if not velocity then
        return -- not something that moves (static, kinematic or soft)
    end
    local level = vec3(velocity.x, 0, velocity.z)
    physics.SetVelocity(other, level + self.entity.forward * self.forward + vec3(0, self.up, 0))
    self.launches += 1
end

return LaunchPad
