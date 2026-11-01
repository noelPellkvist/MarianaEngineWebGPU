--!strict
-- AutoDoor: a sliding door that opens while something is in its trigger and closes when it's clear.
-- Put it on the trigger entity (a box collider with trigger on, in front of the door) and name the
-- door entity in `door`.
local AutoDoor = {} :: Script

AutoDoor.properties = {
    door = "Door",
    slide = vec3(0, 3, 0),   -- how far the door moves to open
    speed = 2,               -- openings per second
}

function AutoDoor.Start(self: Script)
    self.inside = {}   -- what is in the trigger now
    self.amount = 0    -- 0 closed .. 1 open
    local door = scene.Find(self.door)
    self.doorEntity = door
    self.closedAt = if door then door.position else vec3(0, 0, 0)
end

function AutoDoor.OnTriggerEnter(self: Script, other: Entity)
    self.inside[other] = true
end

function AutoDoor.OnTriggerExit(self: Script, other: Entity)
    self.inside[other] = nil
end

function AutoDoor.Update(self: Script, dt: number)
    local door = self.doorEntity
    if not (door and door:IsValid()) then
        return
    end
    local anyone = next(self.inside) ~= nil
    local goal = if anyone then 1 else 0
    if self.amount < goal then
        self.amount = math.min(goal, self.amount + self.speed * dt)
    elseif self.amount > goal then
        self.amount = math.max(goal, self.amount - self.speed * dt)
    end
    door.position = self.closedAt + self.slide * self.amount
end

return AutoDoor
