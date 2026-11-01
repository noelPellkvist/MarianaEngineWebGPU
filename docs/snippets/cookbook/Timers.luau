--!strict
-- Timers: something every few seconds, something once after a delay, and a cooldown on a key.
local Timers = {} :: Script

Timers.properties = {
    interval = 2,    -- seconds between ticks
    delay = 5,       -- seconds before the one-off
    cooldown = 1,    -- seconds between Space presses that count
}

function Timers.Start(self: Script)
    self.tickTimer = 0
    self.elapsed = 0
    self.fired = false
    self.readyAt = 0   -- game time when Space works again
end

function Timers.Update(self: Script, dt: number)
    -- Repeating: subtract the interval instead of resetting to 0, so it doesn't drift.
    self.tickTimer += dt
    while self.tickTimer >= self.interval do
        self.tickTimer -= self.interval
        print("tick")
    end

    -- Once, after a delay.
    self.elapsed += dt
    if not self.fired and self.elapsed >= self.delay then
        self.fired = true
        print(`{self.delay} seconds have passed`)
    end

    -- A cooldown, measured against the game clock.
    if input.GetKeyDown("space") then
        if time.time >= self.readyAt then
            self.readyAt = time.time + self.cooldown
            print("Ability used")
        else
            print(`Cooling down: {string.format("%.1f", self.readyAt - time.time)} s left`)
        end
    end
end

return Timers
