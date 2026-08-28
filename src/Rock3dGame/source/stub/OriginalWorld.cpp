#include "OriginalWorld.h"

namespace r3d::game::originalrace::source
{

void WorldEvent::Remove() noexcept
{
    removed_ = true;
}

bool WorldEvent::Removed() const noexcept
{
    return removed_;
}

void WorldHost::OnLogicProgress(float deltaTime)
{
    static_cast<void>(deltaTime);
}

void WorldHost::OnEnvironmentFrame(float deltaTime)
{
    static_cast<void>(deltaTime);
}

void WorldHost::OnNetworkFrame(float deltaTime)
{
    static_cast<void>(deltaTime);
}

void WorldHost::OnControlFrame(float deltaTime)
{
    static_cast<void>(deltaTime);
}

WorldEventPump::WorldEventPump(WorldHost* host) noexcept
    : host_(host)
{
}

void WorldEventPump::SetHost(WorldHost* host) noexcept
{
    host_ = host;
}

void WorldEventPump::SetGameMode(GameModeFrameEvent* gameMode) noexcept
{
    gameMode_ = gameMode;
}

void WorldEventPump::RegFixedStepEvent(FixedStepEvent* event)
{
    Insert(fixedStepEvents_, event);
}

void WorldEventPump::UnregFixedStepEvent(FixedStepEvent* event) noexcept
{
    Erase(fixedStepEvents_, event);
}

void WorldEventPump::RegProgressEvent(ProgressEvent* event)
{
    Insert(progressEvents_, event);
}

void WorldEventPump::UnregProgressEvent(ProgressEvent* event) noexcept
{
    Erase(progressEvents_, event);
}

void WorldEventPump::RegLateProgressEvent(LateProgressEvent* event)
{
    Insert(lateProgressEvents_, event);
}

void WorldEventPump::UnregLateProgressEvent(LateProgressEvent* event) noexcept
{
    Erase(lateProgressEvents_, event);
}

void WorldEventPump::RegFrameEvent(FrameEvent* event)
{
    Insert(frameEvents_, event);
}

void WorldEventPump::UnregFrameEvent(FrameEvent* event) noexcept
{
    Erase(frameEvents_, event);
}

void WorldEventPump::Progress(float deltaTime)
{
    if (host_ != nullptr)
        host_->OnLogicProgress(deltaTime);

    for (auto entry = progressEvents_.begin();
         entry != progressEvents_.end();)
    {
        auto* event = *entry;
        event->OnProgress(deltaTime);
        if (event->Removed())
            entry = progressEvents_.erase(entry);
        else
            ++entry;
    }
}

void WorldEventPump::FixedStep(float deltaTime)
{
    for (auto* event : fixedStepEvents_)
        event->OnFixedStep(deltaTime);
}

void WorldEventPump::LateProgress(float deltaTime, bool physicsStep)
{
    for (auto* event : lateProgressEvents_)
        event->OnLateProgress(deltaTime, physicsStep);
}

void WorldEventPump::FrameStep(float deltaTime, float physicsAlpha)
{
    if (!paused_)
    {
        for (auto* event : frameEvents_)
            event->OnFrame(deltaTime, physicsAlpha);
        if (host_ != nullptr)
            host_->OnEnvironmentFrame(deltaTime);
    }

    if (host_ != nullptr)
        host_->OnNetworkFrame(deltaTime);
    if (!paused_ && host_ != nullptr)
        host_->OnControlFrame(deltaTime);
    if (gameMode_ != nullptr)
        gameMode_->OnGameFrame(deltaTime, physicsAlpha, paused_);
}

bool WorldEventPump::DispatchFixedStepEvent(
    FixedStepEvent* event, float deltaTime)
{
    if (!HasFixedStepEvent(event) || event->Removed())
        return false;
    event->OnFixedStep(deltaTime);
    return true;
}

bool WorldEventPump::DispatchFrameEvent(
    FrameEvent* event, float deltaTime, float physicsAlpha)
{
    if (paused_ || !HasFrameEvent(event) || event->Removed())
        return false;
    event->OnFrame(deltaTime, physicsAlpha);
    return true;
}

bool WorldEventPump::HasFixedStepEvent(
    const FixedStepEvent* event) const noexcept
{
    return std::find(fixedStepEvents_.begin(), fixedStepEvents_.end(), event) !=
           fixedStepEvents_.end();
}

bool WorldEventPump::HasLateProgressEvent(
    const LateProgressEvent* event) const noexcept
{
    return std::find(
               lateProgressEvents_.begin(), lateProgressEvents_.end(), event) !=
           lateProgressEvents_.end();
}

bool WorldEventPump::HasFrameEvent(const FrameEvent* event) const noexcept
{
    return std::find(frameEvents_.begin(), frameEvents_.end(), event) !=
           frameEvents_.end();
}

std::size_t WorldEventPump::FixedStepEventCount() const noexcept
{
    return fixedStepEvents_.size();
}

std::size_t WorldEventPump::LateProgressEventCount() const noexcept
{
    return lateProgressEvents_.size();
}

std::size_t WorldEventPump::FrameEventCount() const noexcept
{
    return frameEvents_.size();
}

void WorldEventPump::Pause(bool paused) noexcept
{
    paused_ = paused;
}

bool WorldEventPump::IsPaused() const noexcept
{
    return paused_;
}

void WorldEventPump::ResetInput(bool reset) noexcept
{
    inputWasReset_ = reset;
}

bool WorldEventPump::InputWasReset() const noexcept
{
    return inputWasReset_;
}

} // namespace r3d::game::originalrace::source
