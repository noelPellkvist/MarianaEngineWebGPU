#pragma once
#include <cstdint>

// The game's time, apart from real time: it only runs while the game plays (the editor's Play
// button; a player plays from the start), and can be paused, stepped a frame at a time and scaled.
// Game systems (animation, later physics and scripts) step with DeltaTime(); an app's own things
// (the editor camera, the UI) keep the real frame time.
class GameClock
{
public:
    // Stopped: not in play mode (the editor editing). Playing and Paused are play mode.
    enum class State { Stopped, Playing, Paused };

    // Enters play mode (with the time at 0), or resumes from a pause.
    void Play();
    void Pause();
    // Leaves play mode; the time goes back to 0 and the time scale to 1.
    void Stop();
    // Advances one frame (kStep) on the next tick, pausing first if the game is running.
    void Step();

    // Once per frame from the main loop, with the real time the frame took.
    void Tick(float realDelta);

    State GetState() const { return m_State; }
    // In play mode, paused or not.
    bool IsPlaying() const { return m_State != State::Stopped; }
    bool IsPaused() const { return m_State == State::Paused; }

    // This frame's game time step: 0 unless the game runs (or steps).
    float DeltaTime() const { return m_Delta; }
    // Game seconds and frames since Play.
    double Time() const { return m_Time; }
    uint64_t Frame() const { return m_Frame; }

    // Game seconds per real second (the game sets it while playing; Stop puts it back).
    float timeScale = 1.0f;

    static constexpr float kStep = 1.0f / 60.0f;
    // A longer frame (loading, a breakpoint) only moves the game this far.
    static constexpr float kMaxDelta = 0.25f;

private:
    State m_State = State::Stopped;
    float m_Delta = 0.0f;
    double m_Time = 0.0;
    uint64_t m_Frame = 0;
    bool m_StepPending = false;
};
