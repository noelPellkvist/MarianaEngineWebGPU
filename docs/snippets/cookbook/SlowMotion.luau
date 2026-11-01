--!strict
-- SlowMotion: hold Tab for slow motion; the change eases in and out over a fraction of a second.
-- Everything on game time slows down: scripts' dt, physics, animation.
local SlowMotion = {} :: Script

SlowMotion.properties = {
    slowScale = 0.25,
    key = "tab",
}

function SlowMotion.Update(self: Script, dt: number)
    local goal = if input.GetKey(self.key) then self.slowScale else 1
    -- dt is already slowed down, so ease with real seconds: dt / time.scale.
    local realDt = if time.scale > 0 then dt / time.scale else dt
    time.scale += (goal - time.scale) * math.min(1, realDt * 8)
end

function SlowMotion.OnDestroy(self: Script)
    time.scale = 1
end

return SlowMotion
