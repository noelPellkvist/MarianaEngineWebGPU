#include <Mariana/Scene/Transform.hpp>

#include <glm/gtc/quaternion.hpp>

glm::mat4 LocalMatrix(const LocalTRS& t)
{
    glm::mat4 m = glm::mat4_cast(glm::quat(t.rot_quat[3], t.rot_quat[0], t.rot_quat[1], t.rot_quat[2]));
    m[0] *= t.scl[0];
    m[1] *= t.scl[1];
    m[2] *= t.scl[2];
    m[3] = glm::vec4(t.pos[0], t.pos[1], t.pos[2], 1.0f);
    return m;
}

glm::mat4 WorldMatrix(const Scene& scene, Entity entity)
{
    glm::mat4 m(1.0f);
    for (Entity e = entity; e.IsValid(); e = scene.Parent(e))
        if (const LocalTRS* t = e.TryGet<LocalTRS>())
            m = LocalMatrix(*t) * m;
    return m;
}
