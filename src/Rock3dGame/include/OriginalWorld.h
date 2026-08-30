#pragma once

#include <algorithm>
#include <deque>
#include <vector>

namespace r3d::game::originalrace::source
{

class WorldEvent
{
public:
    virtual ~WorldEvent() = default;

    void Remove() noexcept;
    bool Removed() const noexcept;

private:
    bool removed_ = false;
};

class FixedStepEvent : public virtual WorldEvent
{
public:
    virtual void OnFixedStep(float deltaTime) = 0;
};

class ProgressEvent : public virtual WorldEvent
{
public:
    virtual void OnProgress(float deltaTime) = 0;
};

class LateProgressEvent : public virtual WorldEvent
{
public:
    virtual void OnLateProgress(float deltaTime, bool physicsStep) = 0;
};

class FrameEvent : public virtual WorldEvent
{
public:
    virtual void OnFrame(float deltaTime, float physicsAlpha) = 0;
};

class GameModeFrameEvent
{
public:
    virtual ~GameModeFrameEvent() = default;
    virtual void OnGameFrame(
        float deltaTime, float physicsAlpha, bool worldPaused) = 0;
};

class WorldHost
{
public:
    virtual ~WorldHost() = default;
    virtual void OnLogicProgress(float deltaTime);
    virtual void OnEnvironmentFrame(float deltaTime);
    virtual void OnNetworkFrame(float deltaTime);
    virtual void OnControlFrame(float deltaTime);
};

// Timing policy from World::MainProgress. Platform clocks supply raw frame
// seconds; this owner keeps the source 15-frame synchronized average,
// 7/60 clamp, 1/60 accumulator and render interpolation alpha. Physics and
// rendering backends consume the resulting plan without owning game timing.
class WorldFrameClock
{
public:
    static constexpr float fixedStep = 1.0F / 60.0F;
    static constexpr std::size_t maximumFixedSteps = 7U;
    static constexpr std::size_t synchronizedFrameCount = 15U;

    struct Plan
    {
        float deltaTime = 0.0F;
        float physicsAlpha = -1.0F;
        std::size_t fixedSteps = 0U;
        bool lateProgressWithoutPhysics = true;
    };

    void Reset() noexcept;
    float SmoothDelta(
        double rawDeltaTime, bool synchronized = true) noexcept;
    Plan Schedule(float deltaTime, bool raceStarted) noexcept;
    float GetAccumulator() const noexcept;
    std::size_t GetSynchronizedFrameCount() const noexcept;

private:
    std::deque<double> frameDeltas_;
    double accumulator_ = 0.0;
};

// Backend-neutral transcription of World.cpp's four ordered event lists and
// FrameStep boundary. Native physics/render/network/control work is exposed
// only through WorldHost; gameplay listeners retain source order and pause
// semantics.
class WorldEventPump
{
public:
    explicit WorldEventPump(WorldHost* host = nullptr) noexcept;

    void SetHost(WorldHost* host) noexcept;
    void SetGameMode(GameModeFrameEvent* gameMode) noexcept;

    void RegFixedStepEvent(FixedStepEvent* event);
    void UnregFixedStepEvent(FixedStepEvent* event) noexcept;
    void RegProgressEvent(ProgressEvent* event);
    void UnregProgressEvent(ProgressEvent* event) noexcept;
    void RegLateProgressEvent(LateProgressEvent* event);
    void UnregLateProgressEvent(LateProgressEvent* event) noexcept;
    void RegFrameEvent(FrameEvent* event);
    void UnregFrameEvent(FrameEvent* event) noexcept;

    void Progress(float deltaTime);
    void FixedStep(float deltaTime);
    void LateProgress(float deltaTime, bool physicsStep);
    void FrameStep(float deltaTime, float physicsAlpha);

    // Jolt exposes one callback per vehicle inside a world substep.  Calling
    // FixedStep there would advance every registered GameCar once per car.
    // These address-specific adapters preserve the World registration gate
    // while dispatching only the source object represented by that callback.
    bool DispatchFixedStepEvent(
        FixedStepEvent* event, float deltaTime);
    bool DispatchFrameEvent(
        FrameEvent* event, float deltaTime, float physicsAlpha);
    bool HasFixedStepEvent(const FixedStepEvent* event) const noexcept;
    bool HasLateProgressEvent(
        const LateProgressEvent* event) const noexcept;
    bool HasFrameEvent(const FrameEvent* event) const noexcept;
    std::size_t FixedStepEventCount() const noexcept;
    std::size_t LateProgressEventCount() const noexcept;
    std::size_t FrameEventCount() const noexcept;

    void Pause(bool paused) noexcept;
    bool IsPaused() const noexcept;
    void ResetInput(bool reset) noexcept;
    bool InputWasReset() const noexcept;

private:
    template <typename Event>
    static void Insert(std::vector<Event*>& events, Event* event)
    {
        if (event == nullptr ||
            std::find(events.begin(), events.end(), event) != events.end())
            return;
        events.push_back(event);
    }

    template <typename Event>
    static void Erase(std::vector<Event*>& events, Event* event) noexcept
    {
        const auto found = std::find(events.begin(), events.end(), event);
        if (found != events.end())
            events.erase(found);
    }

    WorldHost* host_ = nullptr;
    GameModeFrameEvent* gameMode_ = nullptr;
    std::vector<FixedStepEvent*> fixedStepEvents_;
    std::vector<ProgressEvent*> progressEvents_;
    std::vector<LateProgressEvent*> lateProgressEvents_;
    std::vector<FrameEvent*> frameEvents_;
    bool paused_ = false;
    bool inputWasReset_ = false;
};

} // namespace r3d::game::originalrace::source
