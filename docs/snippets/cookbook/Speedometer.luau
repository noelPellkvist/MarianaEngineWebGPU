--!strict
-- Speedometer: put it on a vehicle (with a Vehicle component). Prints its speed, gear and RPM once a
-- second, and warns when a wheel loses grip.
local Speedometer = {} :: Script

function Speedometer.Start(self: Script)
    self.timer = 0
end

function Speedometer.Update(self: Script, dt: number)
    self.timer += dt
    if self.timer < 1 then
        return
    end
    self.timer = 0
    local info = physics.GetVehicleInfo(self.entity)
    if not info then
        return   -- not a vehicle (or the game isn't running)
    end
    local kmh = info.speed * 3.6
    print(`{string.format("%.0f", kmh)} km/h, gear {info.gear}, {string.format("%.0f", info.rpm)} rpm`)
    for i, wheel in info.wheels do
        if wheel.contact and math.abs(wheel.lateralSlip) > 15 then
            print(`wheel {i} is sliding`)
        end
    end
end

return Speedometer
