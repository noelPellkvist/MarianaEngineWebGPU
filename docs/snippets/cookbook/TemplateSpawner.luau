--!strict
-- TemplateSpawner: makes copies of a template entity (with its scripts, colliders and everything
-- below it) at random places around this entity. Build the template in the editor, untick its
-- Active box so it stays out of the game, and type its name in `template`.
local Spawner = {} :: Script

Spawner.properties = {
    template = "Enemy Template",
    interval = 2,  -- seconds between copies
    radius = 8,    -- how far from this entity they appear
    maxAlive = 10, -- copies at once
}

function Spawner.Start(self: Script)
    self.source = scene.Find(self.template)
    self.timer = 0
    self.alive = {}
    if not self.source then
        warn(`No entity named {self.template}`)
    end
end

function Spawner.Update(self: Script, dt: number)
    local source: Entity? = self.source
    if not source then
        return
    end
    -- Forget the copies that were destroyed (by gameplay, or their own scripts).
    local alive: { Entity } = self.alive
    for i = #alive, 1, -1 do
        if not alive[i]:IsValid() then
            table.remove(alive, i)
        end
    end

    self.timer += dt
    if self.timer < self.interval or #alive >= self.maxAlive then
        return
    end
    self.timer = 0
    local angle = math.random() * math.pi * 2
    local radius: number = self.radius
    local at = self.entity.worldPosition + vec3(math.cos(angle) * radius, 0, math.sin(angle) * radius)
    local copy = scene.Clone(source, at, quat.euler(0, math.random(0, 359), 0))
    copy.active = true -- copies of an inactive template are inactive too, until switched on
    table.insert(alive, copy)
end

return Spawner
