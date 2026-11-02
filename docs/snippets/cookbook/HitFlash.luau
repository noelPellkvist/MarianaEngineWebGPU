--!strict
-- HitFlash: the entity glows red when something hits it hard, then fades back to its own colour.
-- Put it on an entity with a mesh and a rigid body (or a collider).
local HitFlash = {} :: Script

HitFlash.properties = {
    flashColor = vec3(1, 0.1, 0.1),
    fade = 4,       -- how quickly the flash fades (per second)
    minSpeed = 2,   -- hits slower than this (m/s) don't flash
}

function HitFlash.Start(self: Script)
    -- The entity's own copy of its material: changing it changes this entity only.
    self.material = self.entity:GetMaterial()
    if self.material then
        self.original = self.material.color
    end
    self.flash = 0
end

function HitFlash.OnContactBegin(self: Script, other: Entity, contact: Contact)
    if contact.speed >= self.minSpeed then
        self.flash = 1
    end
end

function HitFlash.Update(self: Script, dt: number)
    local m: Material? = self.material
    if not m or self.flash <= 0 then
        return
    end
    self.flash = math.max(0, self.flash - dt * self.fade)
    m.color = vector.lerp(self.original, self.flashColor, self.flash)
    m.emissive = self.flashColor * (self.flash * 2)
end

return HitFlash
