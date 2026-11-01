--!strict
-- RandomSpawner: drops a crate somewhere in an area around its entity every few seconds, keeping at
-- most `maxCount` (the oldest goes first).
local RandomSpawner = {} :: Script

RandomSpawner.properties = {
    area = vec3(10, 0, 10),   -- width and depth of the area
    height = 8,               -- dropped from this high
    interval = 1.5,
    maxCount = 20,
}

function RandomSpawner.Start(self: Script)
    math.randomseed(os.clock() * 1000)
    self.timer = 0
    self.spawned = {}
end

local function spawnCrate(self: Script)
    local x = (math.random() - 0.5) * self.area.x
    local z = (math.random() - 0.5) * self.area.z
    local position = self.entity.worldPosition + vec3(x, self.height, z)
    local tint = 0.6 + math.random() * 0.4
    local crate = scene.SpawnPrimitive("cube", position, vec3(0.8 * tint, 0.55 * tint, 0.3 * tint),
        quat.euler(0, math.random(0, 359), 0))
    crate.name = "Crate"
    crate.scale = vec3(0.8, 0.8, 0.8)
    crate:Add("BoxCollider")
    crate:Add("RigidBody", { motion = "Dynamic", mass = 15 })
    table.insert(self.spawned, crate)

    if #self.spawned > self.maxCount then
        local oldest = table.remove(self.spawned, 1) :: Entity
        if oldest:IsValid() then
            oldest:Destroy()
        end
    end
end

function RandomSpawner.Update(self: Script, dt: number)
    self.timer += dt
    if self.timer >= self.interval then
        self.timer -= self.interval
        spawnCrate(self)
    end
end

return RandomSpawner
