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

struct GameModeStartupFrame
{
    float firstLogoAlpha = 0.0F;
    float secondLogoAlpha = 0.0F;
    bool loadFrame = false;
    bool startGame = false;
    bool active = false;
};

// Exact backend-neutral owner for GameMode::_startUpTime. GUI textures and
// drawing remain native, while logo timing, the -2 loading frame, the -3
// StartGame transition and Escape skip belong to the source state machine.
class GameModeStartupState
{
public:
    void Run(bool playIntro) noexcept;
    void SkipIntro() noexcept;
    GameModeStartupFrame OnFrame(float deltaTime) noexcept;

    bool IsActive() const noexcept;
    float ElapsedSeconds() const noexcept;

private:
    enum class Phase : std::uint8_t
    {
        Inactive,
        Logos,
        Loading,
        StartGame,
    };

    Phase phase_ = Phase::Inactive;
    float elapsedSeconds_ = 0.0F;
};

struct GameModeMovieFrame
{
    bool prepareWindow = false;
    bool enterVideoMode = false;
    bool openAndPlay = false;
    bool unload = false;
    bool exitVideoMode = false;
    bool videoStopped = false;
};

// GameMode::_movieTime is a frame counter, not a time duration. This owner
// preserves its window/video/music/input preparation and four-frame teardown
// while the macOS video backend executes the returned commands.
class GameModeMovieState
{
public:
    void Play() noexcept;
    void NotifyPlaybackEnded() noexcept;
    GameModeMovieFrame OnFrame() noexcept;

    bool IsPlaying() const noexcept;
    bool IsVideoMode() const noexcept;
    std::int32_t FrameState() const noexcept;

private:
    std::int32_t movieTime_ = -1;
    bool videoMode_ = false;
};

// Exact backend-neutral owner for GameMode::_fadeMusic and
// _fadeSpeedMusic. The historical FadeIn/FadeOut names are preserved even
// though they set target volumes 0 and 1 respectively in the shipped code.
class GameModeMusicFadeState
{
public:
    void FadeInMusic(
        float sourceVolume = -1.0F, float speed = 1.0F) noexcept;
    void FadeOutMusic(
        float sourceVolume = -1.0F, float speed = 1.0F) noexcept;
    bool OnFrame(float deltaTime, bool active = true) noexcept;

    float GetVolume() const noexcept;
    float GetTargetVolume() const noexcept;
    float GetSpeed() const noexcept;

private:
    void SetFade(float target, float sourceVolume, float speed) noexcept;

    float volume_ = 1.0F;
    float targetVolume_ = 1.0F;
    float speed_ = 0.0F;
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
    void FadeInMusic(
        float sourceVolume = -1.0F, float speed = 1.0F) noexcept;
    void FadeOutMusic(
        float sourceVolume = -1.0F, float speed = 1.0F) noexcept;
    bool OnMusicFrame(float deltaTime, bool active = true) noexcept;
    float GetMusicSourceVolume() const noexcept;
    const GameModeMusicFadeState& GetMusicFadeState() const noexcept;

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
    GameModeMusicFadeState musicFade_;
};

} // namespace r3d::game::originalrace::source
