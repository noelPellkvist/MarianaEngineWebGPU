--!strict
-- GameManager: keeps the score and the lives. Put it on an entity named "Game"; other scripts find it
-- with scene.Find("Game") and call its functions.
local GameManager = {} :: Script

GameManager.properties = {
    lives = 3,
    winScore = 100,
}

function GameManager.Start(self: Script)
    self.score = 0
    self.state = "playing"   -- "playing", "won" or "lost"
end

function GameManager.AddScore(self: Script, points: number)
    if self.state ~= "playing" then
        return
    end
    self.score += points
    print(`Score: {self.score}`)
    if self.score >= self.winScore then
        self.state = "won"
        print("You win!")
    end
end

function GameManager.LoseLife(self: Script)
    if self.state ~= "playing" then
        return
    end
    self.lives -= 1
    print(`Lives left: {self.lives}`)
    if self.lives <= 0 then
        self.state = "lost"
        print("Game over")
    end
end

return GameManager
