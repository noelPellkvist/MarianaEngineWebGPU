--!strict
-- SmoothFollow: a camera (or anything) that follows a target by name from a fixed offset, easing
-- toward it, and keeps looking at it. Runs in LateUpdate, after the target has moved this frame.
local SmoothFollow = {} :: Script

SmoothFollow.properties = {
    target = "Player",
    offset = vec3(0, 6, -8),   -- from the target, in the world's axes
    stiffness = 4,             -- how quickly it catches up (per second)
}

function SmoothFollow.LateUpdate(self: Script, dt: number)
    if not (self.followed and self.followed:IsValid()) then
        self.followed = scene.Find(self.target)
        if not self.followed then
            return
        end
    end
    local target = self.followed :: Entity
    local wanted = target.worldPosition + self.offset
    -- Exponential easing: the same feel at any frame rate.
    local t = 1 - math.exp(-self.stiffness * dt)
    self.entity.worldPosition = vector.lerp(self.entity.worldPosition, wanted, t)
    self.entity:LookAt(target.worldPosition + vec3(0, 1, 0))
end

return SmoothFollow
