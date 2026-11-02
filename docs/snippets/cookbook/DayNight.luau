--!strict
-- Day and night: the sun goes round, the light dims and warms toward dusk, and bloom rises at night.
local DayNight = {} :: Script

DayNight.properties = {
    dayLength = 60,  -- seconds for a whole day
}

function DayNight.Start(self: Script)
    self.hasBloom = table.find(effects.Names(), "Bloom") ~= nil
end

function DayNight.Update(self: Script, dt: number)
    local t = (time.time / self.dayLength) % 1  -- 0 = sunrise, 0.25 = noon, 0.75 = midnight
    local height = math.sin(t * 2 * math.pi)   -- 1 at noon, -1 at midnight
    local day = math.clamp(height * 2 + 0.5, 0, 1)

    lighting.sunAzimuth = 90 + t * 360
    lighting.sunElevation = math.max(height * 70, -5)
    lighting.sunIntensity = day
    lighting.sunColor = vec3(1, 0.65 + 0.35 * day, 0.45 + 0.55 * day)
    lighting.skyIntensity = 0.1 + 0.9 * day

    if self.hasBloom then
        effects.Set("Bloom", "intensity", 1 + 1.5 * (1 - day))
    end
end

return DayNight
