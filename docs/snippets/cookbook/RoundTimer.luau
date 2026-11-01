--!strict
-- RoundTimer: a simple game flow as a state machine in Lua: a countdown, a timed round, a results
-- pause, then again. Other scripts can read `state` through GetScript("RoundTimer").
local RoundTimer = {} :: Script

RoundTimer.properties = {
    countdown = 3,
    roundLength = 60,
    resultsTime = 5,
}

local function enter(self: Script, state: string)
    self.state = state
    self.stateTime = 0
    print(`-> {state}`)
end

function RoundTimer.Start(self: Script)
    enter(self, "countdown")
end

function RoundTimer.Update(self: Script, dt: number)
    self.stateTime += dt
    if self.state == "countdown" then
        local left = math.ceil(self.countdown - self.stateTime)
        if left ~= self.lastShown and left > 0 then
            self.lastShown = left
            print(`{left}...`)
        end
        if self.stateTime >= self.countdown then
            enter(self, "playing")
        end
    elseif self.state == "playing" then
        if self.stateTime >= self.roundLength then
            enter(self, "results")
        end
    elseif self.state == "results" then
        if self.stateTime >= self.resultsTime then
            self.lastShown = nil
            enter(self, "countdown")
        end
    end
end

return RoundTimer
