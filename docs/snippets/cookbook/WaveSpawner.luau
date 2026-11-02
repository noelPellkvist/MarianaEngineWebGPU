--!strict
-- Waves: one task counts down, spawns a wave of crates one by one, rests, and does it again.
local WaveSpawner = {} :: Script

WaveSpawner.properties = {
    waves = 3,       -- how many waves
    perWave = 4,     -- crates in the first wave (one more each wave)
    gap = 0.3,       -- seconds between crates
    rest = 2,        -- seconds between waves
}

function WaveSpawner.Start(self: Script)
    task.spawn(function()
        for wave = 1, self.waves do
            for count = 3, 1, -1 do
                print(`Wave {wave} in {count}...`)
                task.wait(1)
            end
            for _ = 1, self.perWave + wave - 1 do
                local spot = self.entity.worldPosition + vec3(math.random(-4, 4), 6, math.random(-4, 4))
                local crate = scene.SpawnPrimitive("cube", spot, vec3(0.8, 0.4, 0.2))
                crate:Add("BoxCollider", { size = vec3(1, 1, 1) })
                crate:Add("RigidBody", { motion = "Dynamic", mass = 10 })
                task.wait(self.gap)
            end
            task.wait(self.rest)
        end
        -- Every script with an OnWavesDone function hears it.
        scene.Broadcast("OnWavesDone")
    end)
end

return WaveSpawner
