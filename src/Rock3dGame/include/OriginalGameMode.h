#pragma once

#include "OriginalWorld.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace r3d::game::originalrace::source
{

struct GameModeRaceAdvance
{
    std::optional<std::int32_t> countdownStage;
    bool raceStarted = false;
    bool finishTimeEnded = false;
};

class GameModeRaceState
{
public:
    static constexpr std::int32_t goRaceWait = 0;
    static constexpr std::int32_t goRace1 = 1;
    static constexpr std::int32_t goRace2 = 2;
    static constexpr std::int32_t goRace3 = 3;
    static constexpr std::int32_t goRace = 4;

    void Reset(bool immediateRaceStart = false) noexcept;
    void Pause(bool paused) noexcept;
    GameModeRaceAdvance OnFrame(float seconds) noexcept;
    std::optional<GameModeRaceAdvance> SynchronizeCountdown(
        std::int32_t stage) noexcept;

    void RunFinishTimer() noexcept;
    void CancelFinishTimer() noexcept;
    void FinishImmediately() noexcept;

    bool IsPaused() const noexcept;
    bool IsRaceGo() const noexcept;
    std::int32_t CountdownStage() const noexcept;
    float CountdownSeconds() const noexcept;
    bool IsFinishTimerRunning() const noexcept;
    bool IsFinishPresentationReady() const noexcept;
    float FinishSecondsRemaining() const noexcept;

private:
    std::int32_t countdownStage_ = goRaceWait;
    float goRaceTime_ = 0.0F;
    float countdownSeconds_ = 4.0F;
    float finishTime_ = -1.0F;
    bool externalCountdown_ = false;
    bool paused_ = false;
    bool finishTimeEnded_ = false;
};

struct GameModeEventData
{
    // Original GameMode::cUndefPlayerId (0xFF000000) without relying on an
    // implementation-defined unsigned-to-signed conversion.
    std::int32_t playerId = -16777216;
    bool success = false;
};

class GameModeUser
{
public:
    virtual ~GameModeUser() = default;
    virtual void OnProcessEvent(
        std::uint32_t eventId, const GameModeEventData* data)
    {
        static_cast<void>(eventId);
        static_cast<void>(data);
    }
};

enum class GameModeCommand : std::uint8_t
{
    ShowRaceInfo,
    DoStartRace,
    StopCommentator,
    StopGameMusic,
    ClearSemaphore,
    ExitRaceBackend,
    SaveGame,
    ShowFinish,
    MuteEffects,
    UnmuteEffects,
    FadeOutMusic,
    ResumeMenuMusic,
};

// Source owner for GameMode's outer match/race admission, two-frame loading
// gate, pause commands, ordered users and finish-close transition. Race-local
// countdown/finish clocks remain a composed GameModeRaceState used by the
// active OriginalRaceSession.
class GameModeState final : public GameModeFrameEvent
{
public:
    void StartMatch() noexcept;
    void ExitMatch() noexcept;
    bool IsMatchStarted() const noexcept;

    bool StartRace();
    void CancelRaceStart() noexcept;
    bool ExitRace(bool saveGame);
    void ExitRaceGoFinish();
    void OnFinishFrameClose();

    void OnLoadingFramePresented() noexcept;
    std::size_t LoadingPresentedFrames() const noexcept;
    bool IsRaceLoading() const noexcept;
    bool IsRaceActive() const noexcept;

    void Pause(bool paused);
    bool IsPaused() const noexcept;

    void RegUser(GameModeUser* user);
    void UnregUser(GameModeUser* user) noexcept;
    void SendEvent(
        std::uint32_t eventId,
        const GameModeEventData* data = nullptr) const;

    std::vector<GameModeCommand> TakeCommands();
    void OnGameFrame(
        float deltaTime, float physicsAlpha, bool worldPaused) override;

private:
    std::vector<GameModeUser*> users_;
    std::vector<GameModeCommand> commands_;
    std::int32_t startRaceFrame_ = -1;
    std::size_t loadingPresentedFrames_ = 0U;
    bool matchStarted_ = false;
    bool raceLoading_ = false;
    bool raceActive_ = false;
    bool paused_ = false;
};

} // namespace r3d::game::originalrace::source
