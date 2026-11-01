--!strict
-- OrbitCamera: put it on a camera. Hold the left mouse button and drag to orbit around a target,
-- scroll to zoom.
local OrbitCamera = {} :: Script

OrbitCamera.properties = {
    target = "Player",
    distance = 8,
    minDistance = 2,
    maxDistance = 30,
    sensitivity = 0.25,   -- degrees per pixel
}

function OrbitCamera.Start(self: Script)
    self.yaw = 0
    self.pitch = 20
end

function OrbitCamera.LateUpdate(self: Script, dt: number)
    local target = scene.Find(self.target)
    local center = if target then target.worldPosition + vec3(0, 1, 0) else vec3(0, 0, 0)

    if input.GetMouseButton(0) then
        local delta = input.GetMouseDelta()
        self.yaw += delta.x * self.sensitivity
        self.pitch = math.clamp(self.pitch + delta.y * self.sensitivity, -10, 80)
    end
    local scroll = input.GetScroll()
    self.distance = math.clamp(self.distance - scroll.y, self.minDistance, self.maxDistance)

    -- Behind the target (-Z), turned by yaw about Y and pitch about X.
    local rotation = quat.euler(self.pitch, self.yaw, 0)
    self.entity.worldPosition = center + rotation * vec3(0, 0, -self.distance)
    self.entity:LookAt(center)
end

return OrbitCamera
