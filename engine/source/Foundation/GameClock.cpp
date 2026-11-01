#include <Mariana/Foundation/GameClock.hpp>

#include <algorithm>

void GameClock::Play()
{
    if (m_State == State::Stopped)
    {
        m_Time = 0.0;
        m_Frame = 0;
    }
    m_State = State::Playing;
}

void GameClock::Pause()
{
    if (m_State == State::Playing)
        m_State = State::Paused;
}

void GameClock::Stop()
{
    m_State = State::Stopped;
    m_Delta = 0.0f;
    m_Time = 0.0;
    m_Frame = 0;
    m_StepPending = false;
    timeScale = 1.0f;
}

void GameClock::Step()
{
    if (m_State == State::Stopped)
        return;
    m_State = State::Paused;
    m_StepPending = true;
}

void GameClock::Tick(float realDelta)
{
    m_Delta = 0.0f;
    bool advance = false;
    if (m_State == State::Playing)
    {
        m_Delta = std::clamp(realDelta, 0.0f, kMaxDelta) * timeScale;
        advance = true;
    }
    else if (m_State == State::Paused && m_StepPending)
    {
        m_Delta = kStep * timeScale;
        m_StepPending = false;
        advance = true;
    }
    if (!advance)
        return;
    m_Time += m_Delta;
    ++m_Frame;
}
