#include "WorldImpl.hpp"

#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Physics/Constraints/ContactConstraintManager.h>
#include <Jolt/Renderer/DebugRendererSimple.h>
#endif

#include <mutex>

using namespace PhysicsInternal;

#ifdef JPH_DEBUG_RENDERER
namespace
{
    // Jolt allows one debug renderer; it collects lines into whatever target is set (from Jolt's
    // worker threads while it draws contacts during a step).
    class LineCollector final : public JPH::DebugRendererSimple
    {
    public:
        void SetTarget(std::vector<DebugLine>* lines, size_t maxLines)
        {
            std::lock_guard lock(m_Mutex);
            m_Lines = lines;
            m_Max = maxLines;
        }

        void DrawLine(JPH::RVec3Arg from, JPH::RVec3Arg to, JPH::ColorArg color) override
        {
            std::lock_guard lock(m_Mutex);
            if (!m_Lines || m_Lines->size() >= m_Max)
                return;
            m_Lines->push_back(DebugLine{ ToGlm(JPH::Vec3(from)), ToGlm(JPH::Vec3(to)), color.GetUInt32() });
        }

        void DrawText3D(JPH::RVec3Arg, const std::string_view&, JPH::ColorArg, float) override {}

    private:
        std::mutex m_Mutex;
        std::vector<DebugLine>* m_Lines = nullptr;
        size_t m_Max = 0;
    };

    LineCollector* g_Lines = nullptr;
}
#endif

namespace PhysicsInternal
{
    void AcquireDebugRenderer()
    {
#ifdef JPH_DEBUG_RENDERER
        if (!g_Lines)
            g_Lines = new LineCollector();
#endif
    }

    void ReleaseDebugRenderer()
    {
#ifdef JPH_DEBUG_RENDERER
        delete g_Lines;
        g_Lines = nullptr;
#endif
    }

    void BeginStepDrawing(std::vector<DebugLine>* lines, size_t maxLines)
    {
#ifdef JPH_DEBUG_RENDERER
        if (!g_Lines)
            return;
        g_Lines->SetTarget(lines, maxLines);
        JPH::ContactConstraintManager::sDrawContactManifolds = true;
#else
        (void)lines;
        (void)maxLines;
#endif
    }

    void EndStepDrawing()
    {
#ifdef JPH_DEBUG_RENDERER
        JPH::ContactConstraintManager::sDrawContactManifolds = false;
        if (g_Lines)
            g_Lines->SetTarget(nullptr, 0);
#endif
    }
}

void PhysicsWorld::DebugDraw(const PhysicsDebugDraw& settings, std::vector<DebugLine>& lines)
{
    // Contacts are drawn by Jolt while it steps: from now on, and those of the last step now.
    m_Impl->drawContacts = settings.contacts;
    m_Impl->maxContactLines = settings.maxLines / 4;
    if (settings.contacts)
        lines.insert(lines.end(), m_Impl->contactLines.begin(), m_Impl->contactLines.end());
    else
        m_Impl->contactLines.clear();

#ifdef JPH_DEBUG_RENDERER
    if (!g_Lines || lines.size() >= settings.maxLines)
        return;
    JPH::BodyManager::DrawSettings draw;
    draw.mDrawShape = settings.shapes;
    draw.mDrawShapeWireframe = true;
    draw.mDrawShapeColor = settings.sleepColors ? JPH::BodyManager::EShapeColor::SleepColor : JPH::BodyManager::EShapeColor::MotionTypeColor;
    draw.mDrawBoundingBox = settings.boundingBoxes;
    draw.mDrawCenterOfMassTransform = settings.centerOfMass;
    draw.mDrawVelocity = settings.velocities;
    g_Lines->SetCameraPos(JPH::RVec3(ToJolt(settings.cameraPosition)));
    g_Lines->SetTarget(&lines, settings.maxLines);
    m_Impl->system->DrawBodies(draw, g_Lines);
    g_Lines->SetTarget(nullptr, 0);
#endif
}
