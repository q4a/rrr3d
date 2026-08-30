#include "OriginalGameMode.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace r3d::game::originalrace::source
{

void GameModeStartupState::Run(bool playIntro) noexcept
{
    elapsedSeconds_ = 0.0F;
    phase_ = playIntro ? Phase::Logos : Phase::Inactive;
}

void GameModeStartupState::SkipIntro() noexcept
{
    if (phase_ == Phase::Logos)
    {
        elapsedSeconds_ = 12.0F;
        phase_ = Phase::Loading;
    }
}

GameModeStartupFrame GameModeStartupState::OnFrame(
    float deltaTime) noexcept
{
    GameModeStartupFrame frame;
    switch (phase_)
    {
    case Phase::Inactive:
        return frame;
    case Phase::Loading:
        frame.active = true;
        frame.loadFrame = true;
        phase_ = Phase::StartGame;
        return frame;
    case Phase::StartGame:
        frame.startGame = true;
        phase_ = Phase::Inactive;
        return frame;
    case Phase::Logos:
        break;
    }

    constexpr float logoDelay = 1.0F;
    constexpr float logoFadeTime = 1.0F;
    constexpr float logoTime = 3.0F;
    constexpr float secondLogoDelay =
        logoFadeTime + logoTime + logoFadeTime +
        logoDelay + logoDelay;
    elapsedSeconds_ += std::max(deltaTime, 0.0F);
    frame.active = true;
    frame.firstLogoAlpha =
        std::clamp(
            (elapsedSeconds_ - logoDelay) / logoFadeTime,
            0.0F, 1.0F) -
        std::clamp(
            (elapsedSeconds_ -
             (logoDelay + logoFadeTime + logoTime)) /
                logoFadeTime,
            0.0F, 1.0F);
    frame.secondLogoAlpha =
        std::clamp(
            (elapsedSeconds_ - secondLogoDelay) / logoFadeTime,
            0.0F, 1.0F) -
        std::clamp(
            (elapsedSeconds_ -
             (secondLogoDelay + logoFadeTime + logoTime)) /
                logoFadeTime,
            0.0F, 1.0F);
    if (elapsedSeconds_ >= 12.0F)
        phase_ = Phase::Loading;
    return frame;
}

bool GameModeStartupState::IsActive() const noexcept
{
    return phase_ != Phase::Inactive;
}

float GameModeStartupState::ElapsedSeconds() const noexcept
{
    return elapsedSeconds_;
}

void GameModeRaceState::Reset(bool immediateRaceStart) noexcept
{
    countdownStage_ = immediateRaceStart ? goRace : goRaceWait;
    goRaceTime_ = immediateRaceStart ? -1.0F : 0.0F;
    countdownSeconds_ = immediateRaceStart ? 0.0F : 4.0F;
    finishTime_ = -1.0F;
    externalCountdown_ = false;
    paused_ = false;
    finishTimeEnded_ = false;
}

void GameModeRaceState::Pause(bool paused) noexcept
{
    paused_ = paused;
}

GameModeRaceAdvance GameModeRaceState::OnFrame(float seconds) noexcept
{
    GameModeRaceAdvance result;
    if (paused_)
        return result;

    seconds = std::max(seconds, 0.0F);
    if (!externalCountdown_ && goRaceTime_ >= 0.0F)
    {
        constexpr float goRaceLag = 1.0F;
        const auto previous = static_cast<std::int32_t>(
            std::floor(goRaceTime_ - goRaceLag));
        goRaceTime_ += seconds;
        countdownSeconds_ = std::max(4.0F - goRaceTime_, 0.0F);
        const auto current = static_cast<std::int32_t>(
            std::floor(goRaceTime_ - goRaceLag));
        if (previous != current)
        {
            countdownStage_ = std::clamp(
                goRace1 + current, goRace1, goRace);
            result.countdownStage = countdownStage_;
            if (countdownStage_ == goRace)
            {
                goRaceTime_ = -1.0F;
                countdownSeconds_ = 0.0F;
                result.raceStarted = true;
            }
        }
    }

    if (finishTime_ >= 0.0F && (finishTime_ += seconds) > 3.0F)
    {
        finishTime_ = -1.0F;
        finishTimeEnded_ = true;
        result.finishTimeEnded = true;
    }
    return result;
}

std::optional<GameModeRaceAdvance>
GameModeRaceState::SynchronizeCountdown(std::int32_t stage) noexcept
{
    if (stage < goRaceWait || stage > goRace)
        return std::nullopt;
    externalCountdown_ = true;
    goRaceTime_ = -1.0F;
    countdownStage_ = stage;
    countdownSeconds_ = static_cast<float>(
        stage <= goRace1 ? 3 : std::max(goRace - stage, 0));
    GameModeRaceAdvance result;
    result.countdownStage = stage;
    result.raceStarted = stage == goRace;
    return result;
}

void GameModeRaceState::RunFinishTimer() noexcept
{
    finishTime_ = 0.0F;
    finishTimeEnded_ = false;
}

void GameModeRaceState::CancelFinishTimer() noexcept
{
    finishTime_ = -1.0F;
    finishTimeEnded_ = false;
}

void GameModeRaceState::FinishImmediately() noexcept
{
    finishTime_ = -1.0F;
    finishTimeEnded_ = true;
}

bool GameModeRaceState::IsPaused() const noexcept
{
    return paused_;
}

bool GameModeRaceState::IsRaceGo() const noexcept
{
    return countdownStage_ == goRace;
}

std::int32_t GameModeRaceState::CountdownStage() const noexcept
{
    return countdownStage_;
}

float GameModeRaceState::CountdownSeconds() const noexcept
{
    return countdownSeconds_;
}

bool GameModeRaceState::IsFinishTimerRunning() const noexcept
{
    return finishTime_ >= 0.0F;
}

bool GameModeRaceState::IsFinishPresentationReady() const noexcept
{
    return finishTimeEnded_;
}

float GameModeRaceState::FinishSecondsRemaining() const noexcept
{
    if (finishTimeEnded_)
        return 0.0F;
    return finishTime_ < 0.0F ? -1.0F
                              : std::max(3.0F - finishTime_, 0.0F);
}

void GameModeState::StartMatch() noexcept
{
    matchStarted_ = true;
}

void GameModeState::ExitMatch() noexcept
{
    matchStarted_ = false;
    startRaceFrame_ = -1;
    loadingPresentedFrames_ = 0U;
    raceLoading_ = false;
    raceActive_ = false;
    paused_ = false;
}

bool GameModeState::IsMatchStarted() const noexcept
{
    return matchStarted_;
}

bool GameModeState::StartRace()
{
    if (raceLoading_ || raceActive_)
        return false;
    startRaceFrame_ = 0;
    loadingPresentedFrames_ = 0U;
    raceLoading_ = true;
    commands_.push_back(GameModeCommand::ShowRaceInfo);
    return true;
}

void GameModeState::CancelRaceStart() noexcept
{
    startRaceFrame_ = -1;
    loadingPresentedFrames_ = 0U;
    raceLoading_ = false;
}

bool GameModeState::ExitRace(bool saveGame)
{
    if (!raceActive_)
        return false;
    raceActive_ = false;
    startRaceFrame_ = -1;
    raceLoading_ = false;
    commands_.push_back(GameModeCommand::StopCommentator);
    commands_.push_back(GameModeCommand::StopGameMusic);
    commands_.push_back(GameModeCommand::ClearSemaphore);
    commands_.push_back(GameModeCommand::ExitRaceBackend);
    // Original GameMode always calls SaveGame(saveGame). The bool controls
    // profile persistence inside the backend; it does not suppress the call.
    static_cast<void>(saveGame);
    commands_.push_back(GameModeCommand::SaveGame);
    return true;
}

void GameModeState::ExitRaceGoFinish()
{
    commands_.push_back(GameModeCommand::ShowFinish);
}

void GameModeState::OnFinishFrameClose()
{
    commands_.push_back(GameModeCommand::StopCommentator);
    commands_.push_back(GameModeCommand::FadeOutMusic);
    commands_.push_back(GameModeCommand::ResumeMenuMusic);
}

void GameModeState::OnLoadingFramePresented() noexcept
{
    if (raceLoading_)
        ++loadingPresentedFrames_;
}

std::size_t GameModeState::LoadingPresentedFrames() const noexcept
{
    return loadingPresentedFrames_;
}

bool GameModeState::IsRaceLoading() const noexcept
{
    return raceLoading_;
}

bool GameModeState::IsRaceActive() const noexcept
{
    return raceActive_;
}

void GameModeState::Pause(bool paused)
{
    if (paused_ == paused)
        return;
    paused_ = paused;
    commands_.push_back(
        paused ? GameModeCommand::MuteEffects
               : GameModeCommand::UnmuteEffects);
}

bool GameModeState::IsPaused() const noexcept
{
    return paused_;
}

void GameModeState::RegUser(GameModeUser* user)
{
    if (user == nullptr ||
        std::find(users_.begin(), users_.end(), user) != users_.end())
        return;
    users_.push_back(user);
}

void GameModeState::UnregUser(GameModeUser* user) noexcept
{
    const auto found = std::find(users_.begin(), users_.end(), user);
    if (found != users_.end())
        users_.erase(found);
}

void GameModeState::SendEvent(
    std::uint32_t eventId, const GameModeEventData* data) const
{
    for (auto* user : users_)
        user->OnProcessEvent(eventId, data);
}

std::vector<GameModeCommand> GameModeState::TakeCommands()
{
    std::vector<GameModeCommand> result;
    result.swap(commands_);
    return result;
}

void GameModeState::OnGameFrame(
    float deltaTime, float physicsAlpha, bool worldPaused)
{
    static_cast<void>(deltaTime);
    static_cast<void>(physicsAlpha);
    if (worldPaused || paused_ || startRaceFrame_ < 0)
        return;

    ++startRaceFrame_;
    if (startRaceFrame_ > 1 && loadingPresentedFrames_ >= 2U)
    {
        startRaceFrame_ = -1;
        raceLoading_ = false;
        raceActive_ = true;
        commands_.push_back(GameModeCommand::DoStartRace);
    }
}

} // namespace r3d::game::originalrace::source
