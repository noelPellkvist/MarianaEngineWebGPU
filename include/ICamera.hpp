#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> 
#include <glm/ext/matrix_clip_space.hpp>
#include <IInput.hpp>
#include <UniformLayout.hpp>

struct CameraInfo
        {
            glm::mat4 proj;
            glm::mat4 view;
            glm::mat4 viewProj;
            glm::mat4 invView;
            glm::mat4 invProj; 
            glm::mat4 invViewProj;
            glm::vec3 pos;
            float exposure;
        };

class ICamera
{
    public:
        

        ICamera(IInput& input) :
        _input(input),
        m_CameraBuffer(false,
                      m_CamInfo,
                      m_CamInfo.proj, 
                      m_CamInfo.view, 
                      m_CamInfo.viewProj, 
                      m_CamInfo.invView, 
                      m_CamInfo.invProj, 
                      m_CamInfo.invViewProj, 
                      m_CamInfo.pos, 
                      m_CamInfo.exposure) {}
        virtual ~ICamera() = default;

        virtual void OnUpdate(float deltaTime) {}

        void SetViewportSize(uint32_t x, uint32_t y) { aspect =  static_cast<float>(x) / static_cast<float>(y); }

        void UpdateBuffer()
        {
            _proj = glm::perspectiveLH_ZO(glm::radians(fov), aspect, nearClip, farClip);
            m_CamInfo.proj = _proj;
            m_CamInfo.view = _view;
            m_CamInfo.viewProj = m_CamInfo.proj * m_CamInfo.view;
            m_CamInfo.invView = glm::inverse(m_CamInfo.view);
            m_CamInfo.invProj = glm::inverse(m_CamInfo.proj);
            m_CamInfo.invViewProj = m_CamInfo.invView * m_CamInfo.invProj;
            m_CamInfo.pos = _pos;
            m_CamInfo.exposure = exposure;
            m_CameraBuffer.pack(m_CamInfo);
        }

        UniformLayout<CameraInfo>& GetBinding() { return m_CameraBuffer; }

        const glm::mat4& View() const { return _view; }
        const glm::mat4& Projection() const { return _proj; }
        glm::vec3 Position()   const { return _pos; }

    protected:
        IInput& _input;
        glm::vec3 _pos;
        glm::mat4 _view{1.0f};
        glm::mat4 _proj{1.0f};
        float fov = 60.0f;
        float nearClip = 0.1f, farClip = 100.0f;
        float exposure = 1.0;        
        float aspect = 16.0f / 9.0f;

    private:
        CameraInfo m_CamInfo;
        UniformLayout<CameraInfo> m_CameraBuffer;
};