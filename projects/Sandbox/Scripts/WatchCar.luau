--!strict
-- WatchCar: on a dancer whose Animator plays Dancer.animgraph, the head (with the neck and chest)
-- turns to watch the car as it comes closer: the graph's Look At node aims at its `lookTarget` and
-- blends in by `lookWeight`. The car is the entity named `target`, or else the first one (by id)
-- driven by a VehicleDriver.
local WatchCar = {} :: Script

WatchCar.properties = {
    target = "",    -- an entity's name; empty = the driven car
    range = 25,     -- it starts watching closer than this (m)
    eyeHeight = 1,  -- looks at this far above the car's origin (m)
    attention = 3,  -- how quickly it turns to watch and away (1/s)
}

local function findCar(name: string): Entity?
    if name ~= "" then
        return scene.Find(name)
    end
    local found: Entity? = nil
    scene.Each("Scripts", function(e)
        if not found and e:GetScript("VehicleDriver") then
            found = e
        end
    end)
    return found
end

function WatchCar.Update(self: Script, dt: number)
    local animator = self.entity:Get("Animator")
    if not animator then
        return
    end
    if not (self.car and self.car:IsValid()) then
        self.car = findCar(self.target)
    end
    local car = self.car
    local watching = 0
    if car then
        local at = car.worldPosition + vec3(0, self.eyeHeight, 0)
        animator:SetVector("lookTarget", at)
        if vector.magnitude(at - self.entity.worldPosition) < self.range then
            watching = 1
        end
    end
    -- Eases its attention on and off rather than snapping.
    local weight = animator:GetFloat("lookWeight")
    weight += (watching - weight) * (1 - math.exp(-math.max(self.attention, 0) * dt))
    animator:SetFloat("lookWeight", weight)
end

return WatchCar
