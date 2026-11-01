--!strict
-- AnimatedWalker: feeds a character's movement to its animation graph. Put it on the entity with the
-- CharacterController; the model below it has an Animator whose graph has a float parameter "speed"
-- (a Blend 1D of idle, walk and run), a bool "grounded" and a trigger "jump".
local AnimatedWalker = {} :: Script

function AnimatedWalker.Start(self: Script)
    -- The Animator is on the model, somewhere below the character.
    local function find(entity: Entity): Entity?
        if entity:Has("Animator") then
            return entity
        end
        for _, child in entity:GetChildren() do
            local found = find(child)
            if found then
                return found
            end
        end
        return nil
    end
    self.model = find(self.entity)
end

function AnimatedWalker.Update(self: Script, dt: number)
    local model = self.model
    local info = physics.GetCharacterInfo(self.entity)
    if not (model and info) then
        return
    end
    local animator = model:Get("Animator")
    if not animator then
        return
    end
    local horizontal = vec3(info.velocity.x, 0, info.velocity.z)
    animator:SetFloat("speed", vector.magnitude(horizontal))
    animator:SetBool("grounded", info.grounded)
    if input.GetKeyDown("space") and info.grounded then
        animator:SetTrigger("jump")
    end
end

return AnimatedWalker
