--!strict
-- Shooter: Space fires a ball along the entity's forward. Balls are physics bodies that bounce
-- around and are cleaned up after a few seconds.
local Shooter = {} :: Script

Shooter.properties = {
    speed = 25,          -- m/s
    lifetime = 4,        -- seconds a ball lives
    fireInterval = 0.15, -- seconds between shots while Space is held
}

function Shooter.Start(self: Script)
    self.balls = {}      -- { entity = Entity, born = number }
    self.nextShot = 0
end

local function fire(self: Script)
    local muzzle = self.entity.worldPosition + self.entity.forward * 1.2 + vec3(0, 0.5, 0)
    local ball = scene.SpawnPrimitive("sphere", muzzle, vec3(1, 0.6, 0.1))
    ball.name = "Ball"
    ball.scale = vec3(0.3, 0.3, 0.3)
    ball:Add("SphereCollider", { restitution = 0.6 })   -- radius 0.5, scaled to 0.15
    ball:Add("RigidBody", {
        motion = "Dynamic",
        mass = 0.5,
        continuous = true,                                -- fast: don't pass through walls
        startVelocity = self.entity.forward * self.speed,
    })
    table.insert(self.balls, { entity = ball, born = time.time })
end

function Shooter.Update(self: Script, dt: number)
    if input.GetKey("space") and time.time >= self.nextShot then
        self.nextShot = time.time + self.fireInterval
        fire(self)
    end

    -- Destroy balls that are too old (going backwards so removing doesn't skip any).
    for i = #self.balls, 1, -1 do
        local shot = self.balls[i]
        if time.time - shot.born > self.lifetime or not shot.entity:IsValid() then
            if shot.entity:IsValid() then
                shot.entity:Destroy()
            end
            table.remove(self.balls, i)
        end
    end
end

return Shooter
