--!strict
-- ChaseCamera: keeps its entity (a camera) behind and above a target, following it smoothly: the
-- entity named `target`, or else the first one (by id) driven by a VehicleDriver or CharacterDriver.
local ChaseCamera = {} :: Script

ChaseCamera.properties = {
    target = "",     -- an entity's name; empty = the driven one
    distance = 7,    -- behind the target (m)
    height = 2.5,    -- above it
    lookHeight = 1,  -- looks at this far above the target
    stiffness = 5,   -- how quickly it catches up (1/s)
}

local function findTarget(name: string): Entity?
    if name ~= "" then
        return scene.Find(name)
    end
    local found: Entity? = nil
    scene.Each("Scripts", function(e)
        if not found and (e:GetScript("VehicleDriver") or e:GetScript("CharacterDriver")) then
            found = e
        end
    end)
    return found
end

function ChaseCamera.LateUpdate(self: Script, dt: number)
    if not (self.followed and self.followed:IsValid()) then
        self.followed = findTarget(self.target)
    end
    local target = self.followed
    if not target or target == self.entity then
        return
    end

    -- Behind the target along its forward, kept level so the camera doesn't pitch with the body.
    local base = target.worldPosition
    local forward = target.forward
    forward = vec3(forward.x, 0, forward.z)
    forward = if vector.magnitude(forward) > 1e-4 then vector.normalize(forward) else vec3(0, 0, 1)
    local wantedPosition = base - forward * self.distance + vec3(0, self.height, 0)
    local wantedLook = base + vec3(0, self.lookHeight, 0)

    -- On from where the camera is now; far off (the first time, or after a jump) it is there at once.
    local reach = math.max(self.distance, 1)
    local position = self.entity.worldPosition
    local look = position + self.entity.forward * reach
    if vector.magnitude(position - wantedPosition) > 4 * reach then
        position, look = wantedPosition, wantedLook
    else
        local stiffness = math.max(self.stiffness, 0)
        position += (wantedPosition - position) * (1 - math.exp(-stiffness * dt))
        -- The look point follows faster, so the target stays near the middle of the view.
        look += (wantedLook - look) * (1 - math.exp(-stiffness * 3 * dt))
    end
    local direction = look - position
    if vector.magnitude(direction) < 1e-4 then
        return
    end
    self.entity.worldPosition = position
    self.entity.worldRotation = quat.lookRotation(direction)
end

return ChaseCamera
