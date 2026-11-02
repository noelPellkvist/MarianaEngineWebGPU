--!strict
-- Click to push: a ray from the camera through the mouse; what it points at is marked, and a click
-- shoves a dynamic body away from the camera.
local ClickToPush = {} :: Script

ClickToPush.properties = {
    strength = 6,    -- metres per second given to what is clicked
}

function ClickToPush.Update(self: Script, dt: number)
    local origin, direction = screen.Ray()
    if not origin or not direction then
        return -- nothing shows the game yet (no camera)
    end
    local hit = physics.Raycast(origin, direction, 200)
    if not hit then
        return
    end

    -- Where the mouse points, drawn this frame (gizmos show in the editor's views).
    gizmos.Sphere(hit.point, 0.15, vec3(1, 0.9, 0.2))
    gizmos.Line(hit.point, hit.point + hit.normal * 0.6, vec3(1, 0.9, 0.2))

    if input.GetMouseButtonDown(0) then
        local info = physics.GetBodyInfo(hit.entity)
        if info and info.motion == "Dynamic" then
            physics.AddImpulseAtPoint(hit.entity, direction * self.strength * info.mass, hit.point)
            gizmos.Text(hit.point + vec3(0, 0.5, 0), hit.entity.name, vec3(1, 1, 1), 1)
        end
    end
end

return ClickToPush
