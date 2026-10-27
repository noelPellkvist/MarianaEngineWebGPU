-- CharacterDriver: walks the entity's CharacterController with the keyboard, relative to the view:
-- W A S D / arrows walk (forward is where the scene's first camera looks, else +Z), Shift runs,
-- Space jumps.
local CharacterDriver = {} :: Script

local function axis(plus: string, plusToo: string, minus: string, minusToo: string): number
    local value = 0
    if input.GetKey(plus) or input.GetKey(plusToo) then
        value += 1
    end
    if input.GetKey(minus) or input.GetKey(minusToo) then
        value -= 1
    end
    return value
end

-- Where the scene's first camera (by id) looks, level.
local function viewForward(): vector
    local camera = nil
    scene.Each("CameraComponent", function(e)
        if not camera then
            camera = e
        end
    end)
    local forward = if camera then camera.forward else vec3(0, 0, 1)
    forward = vec3(forward.x, 0, forward.z)
    return if vector.magnitude(forward) > 1e-4 then vector.normalize(forward) else vec3(0, 0, 1)
end

function CharacterDriver:Update(dt: number)
    local character = self.entity:Get("CharacterController")
    if not character then
        return
    end
    local forward = viewForward()
    local right = vec3(forward.z, 0, -forward.x)
    local move = forward * axis("w", "up", "s", "down") + right * axis("d", "right", "a", "left")
    if vector.magnitude(move) > 1 then
        move = vector.normalize(move)
    end
    character.move = move
    character.run = input.GetKey("shift")
    if input.GetKeyDown("space") then
        character.jump = true
    end
end

return CharacterDriver
