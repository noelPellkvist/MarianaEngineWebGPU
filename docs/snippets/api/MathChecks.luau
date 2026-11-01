--!strict
-- Uses every vector and quat function the API reference lists, so the test suite checks they exist
-- and behave (an error stops this script, and the test reports it).
local MathChecks = {} :: Script

function MathChecks.Start(self: Script)
    local a = vec3(1, 2, 3)
    local b = vector.create(4, 5, 6)
    local results = {
        vector.magnitude(a), vector.dot(a, b), vector.angle(a, b), vector.angle(a, b, vec3(0, 1, 0)),
    }
    local vectors = {
        vector.normalize(a), vector.cross(a, b), vector.lerp(a, b, 0.5), vector.min(a, b), vector.max(a, b, vector.one),
        vector.clamp(a, vector.zero, vector.one), vector.floor(a), vector.ceil(a), vector.abs(-a), vector.sign(a),
        a + b, a - b, a * b, a / b, a * 2, 2 * a, a / 2, -a,
    }
    assert(vec3() == vector.zero, "vec3() is zero")
    assert(math.abs(vector.magnitude(vector.lerp(vec3(0, 0, 0), vec3(0, 0, 10), 0.25)) - 2.5) < 1e-5, "lerp")

    local q = quat.euler(30, 45, 0)
    local q2 = quat.euler(vec3(0, 90, 0))
    local rotations = {
        quat.identity, quat.new(0, 0, 0, 1), quat.angleAxis(90, vec3(0, 1, 0)), quat.lookRotation(vec3(1, 0, 0)),
        quat.lookRotation(vec3(0, 1, 0), vec3(0, 1, 0)), quat.fromToRotation(vec3(0, 1, 0), vec3(1, 0, 0)),
        quat.slerp(q, q2, 0.5), q * q2, q:Inverse(),
    }
    local turned = quat.angleAxis(90, vec3(0, 1, 0)) * vec3(0, 0, 1)
    assert(math.abs(turned.x - 1) < 1e-5, "angleAxis(90, Y) turns +Z to +X")
    local euler = q:ToEuler()
    assert(math.abs(quat.angle(q, quat.euler(euler)) ) < 1e-3, "ToEuler round trip")
    assert(math.abs(quat.angle(quat.identity, q2) - 90) < 1e-3, "angle in degrees")
    self.ok = #results + #vectors + #rotations + q.x + q.y + q.z + q.w
    print("math checks passed")
end

return MathChecks
