--!strict
-- VehicleDriver: drives the entity's Vehicle from the keyboard. W / Up is the throttle, S / Down
-- brakes until the vehicle has (nearly) stopped and then backs up, A D / Left Right steer, Space is
-- the hand brake.
local VehicleDriver = {} :: Script

VehicleDriver.properties = {
    steerSpeed = 3, -- how fast the steering follows the keys (full lock in 1 / steerSpeed s)
    steerFade = 0,  -- m/s at which the keys give half the lock (less the faster it goes); 0 = full lock
}

-- 1, -1 or 0 from two pairs of keys.
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

function VehicleDriver.Update(self: Script, dt: number)
    local vehicle = self.entity:Get("Vehicle")
    if not vehicle then
        return
    end
    local wanted = axis("w", "up", "s", "down")
    local steer = axis("d", "right", "a", "left")
    local handBrake = input.GetKey("space")
    local info = physics.GetVehicleInfo(self.entity)
    local speed = if info then info.speed else 0

    -- Going the other way: brake until it has (nearly) stopped, then drive.
    if wanted * speed < 0 and math.abs(speed) > 0.5 then
        vehicle.throttle = 0
        vehicle.brake = 1
    else
        vehicle.throttle = if handBrake then 0 else wanted
        vehicle.brake = 0
    end
    vehicle.handBrake = if handBrake then 1 else 0

    -- The steering follows the keys at steerSpeed and comes back twice as fast; with steerFade fast
    -- vehicles get less of the lock.
    local lock = if self.steerFade > 0 then 1 / (1 + math.abs(speed) / self.steerFade) else 1
    local rate = math.max(self.steerSpeed, 0.1) * dt * (if steer == 0 then 2 else 1)
    vehicle.steering += math.clamp(steer * lock - vehicle.steering, -rate, rate)
end

return VehicleDriver
