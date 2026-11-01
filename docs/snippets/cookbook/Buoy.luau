--!strict
-- Buoy: reports when its entity goes into and out of water, and how deep it is. Works with any
-- entity; give it a dynamic Rigid Body to have it float in a Water Volume.
local Buoy = {} :: Script

function Buoy.Start(self: Script)
    self.wet = false
end

function Buoy.Update(self: Script, dt: number)
    local position = self.entity.worldPosition
    local surface = physics.WaterAt(position)
    local wet = surface ~= nil and position.y < surface.y
    if wet ~= self.wet then
        self.wet = wet
        print(if wet then `{self.entity.name} splashed in` else `{self.entity.name} is out of the water`)
    end
    if wet and surface then
        self.depth = surface.y - position.y   -- metres below the surface
    end
end

return Buoy
