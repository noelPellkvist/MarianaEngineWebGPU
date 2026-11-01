--!strict
-- KillZone: a trigger (water, lava, the space below the level) that sends whatever falls in back to
-- a spawn point: the entity named by `spawnPoint`, else 5 m above where it fell in.
local KillZone = {} :: Script

KillZone.properties = {
    spawnPoint = "Spawn Point",
}

function KillZone.OnTriggerEnter(self: Script, other: Entity)
    local spawn = scene.Find(self.spawnPoint)
    local position = if spawn then spawn.worldPosition else other.worldPosition + vec3(0, 5, 0)
    local rotation = if spawn then spawn.worldRotation else other.worldRotation
    -- Teleport moves the entity and its physics body together; then stop it.
    physics.Teleport(other, position, rotation)
    physics.SetVelocity(other, vec3(0, 0, 0), vec3(0, 0, 0))
    print(`{other.name} respawned`)
end

return KillZone
