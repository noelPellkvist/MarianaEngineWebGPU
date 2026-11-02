--!strict
-- A score with a best kept between runs of the game (save), and everyone told when it's beaten
-- (Broadcast). Other scripts add points with scene.Find("Game"):Send("AddPoints", 10).
local HighScore = {} :: Script

function HighScore.Start(self: Script)
    self.score = 0
    self.best = save.Get("best", 0)
    print(`Best so far: {self.best}`)
end

function HighScore.AddPoints(self: Script, points: number)
    self.score += points
    if self.score > self.best then
        self.best = self.score
        save.Set("best", self.best)            -- written after this frame
        scene.Broadcast("OnNewBest", self.best) -- any script with OnNewBest(self, best)
    end
end

function HighScore.Update(self: Script, dt: number)
    -- Space scores here; a pickup would Send it.
    if input.GetKeyDown("space") then
        self.entity:Send("AddPoints", 10)
    end
end

return HighScore
