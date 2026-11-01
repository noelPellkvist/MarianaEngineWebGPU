--!strict
-- Coin: a pickup. Needs a trigger collider on its entity. Spins, and when something passes through
-- it adds its value to the game manager (an entity named "Game" with GameManager.luau).
local Coin = {} :: Script

Coin.properties = {
    value = 10,
    spinSpeed = 180,
}

function Coin.Update(self: Script, dt: number)
    self.entity:Rotate(vec3(0, 1, 0), self.spinSpeed * dt)
end

function Coin.OnTriggerEnter(self: Script, other: Entity)
    if self.collected then
        return   -- two bodies can enter on the same frame
    end
    self.collected = true
    local game = scene.Find("Game")
    local manager = if game then game:GetScript("GameManager") else nil
    if manager then
        manager:AddScore(self.value)
    end
    self.entity:Destroy()
end

return Coin
