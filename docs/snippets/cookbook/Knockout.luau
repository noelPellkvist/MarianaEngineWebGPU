--!strict
-- Knockout: press F to shove every ragdoll near this entity: they go limp and fly away from it, and
-- get back up (Powered) a few seconds later. Ragdolls' bodies are their bones, so a hit finds a
-- bone; the Ragdoll component is on the model's root above it.
local Knockout = {} :: Script

Knockout.properties = {
    reach = 3,        -- metres
    shove = 60,       -- N s
    getUpAfter = 4,   -- seconds
}

-- The entity with the Ragdoll at or above `entity`, if any.
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

function Knockout.Start(self: Script)
    self.down = {}   -- ragdoll root -> time it went down
end

function Knockout.Update(self: Script, dt: number)
    if input.GetKeyDown("f") then
        local center = self.entity.worldPosition
        for _, hit in physics.OverlapSphere(center, self.reach, { ignore = self.entity }) do
            local root = ragdollRoot(hit)
            if root and not self.down[root] then
                self.down[root] = time.time
                local away = root.worldPosition - center
                away = if vector.magnitude(away) > 0.01 then vector.normalize(away) else self.entity.forward
                physics.SetRagdollMode(root, "Limp")
                physics.AddRagdollImpulse(root, (away + vec3(0, 0.7, 0)) * self.shove)
                print(`{root.name} goes down`)
            end
        end
    end

    for root, since in self.down do
        if time.time - since > self.getUpAfter then
            self.down[root] = nil
            if root:IsValid() then
                physics.SetRagdollMode(root, "Powered")
            end
        end
    end
end

return Knockout
