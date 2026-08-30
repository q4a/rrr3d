#include "OriginalGameMode.h"
#include "OriginalWorld.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{

namespace source = r3d::game::originalrace::source;

int fail(const std::string& message)
{
    std::cerr << "original World/GameMode smoke failed: " << message << '\n';
    return 1;
}

class Host final : public source::WorldHost
{
public:
    explicit Host(std::vector<int>& order) : order_(order) {}

    void OnLogicProgress(float) override { order_.push_back(1); }
    void OnEnvironmentFrame(float) override { order_.push_back(2); }
    void OnNetworkFrame(float) override { order_.push_back(3); }
    void OnControlFrame(float) override { order_.push_back(4); }

private:
    std::vector<int>& order_;
};

class ProgressProbe final : public source::ProgressEvent
{
public:
    ProgressProbe(int id, std::vector<int>& order, bool remove)
        : id_(id), order_(order), remove_(remove)
    {
    }

    void OnProgress(float) override
    {
        order_.push_back(id_);
        if (remove_)
            Remove();
    }

private:
    int id_ = 0;
    std::vector<int>& order_;
    bool remove_ = false;
};

class FixedProbe final : public source::FixedStepEvent
{
public:
    FixedProbe(int id, std::vector<int>& order) : id_(id), order_(order) {}
    void OnFixedStep(float) override { order_.push_back(id_); }

private:
    int id_ = 0;
    std::vector<int>& order_;
};

class LateProbe final : public source::LateProgressEvent
{
public:
    LateProbe(int id, std::vector<int>& order) : id_(id), order_(order) {}
    void OnLateProgress(float, bool physicsStep) override
    {
        order_.push_back(physicsStep ? id_ : -id_);
    }

private:
    int id_ = 0;
    std::vector<int>& order_;
};

class FrameProbe final : public source::FrameEvent
{
public:
    FrameProbe(int id, std::vector<int>& order) : id_(id), order_(order) {}
    void OnFrame(float, float) override { order_.push_back(id_); }

private:
    int id_ = 0;
    std::vector<int>& order_;
};

class UserProbe final : public source::GameModeUser
{
public:
    UserProbe(int id, std::vector<int>& order) : id_(id), order_(order) {}
    void OnProcessEvent(
        std::uint32_t eventId,
        const source::GameModeEventData* data) override
    {
        order_.push_back(
            id_ + static_cast<int>(eventId) +
            (data != nullptr ? data->playerId : 0));
    }

private:
    int id_ = 0;
    std::vector<int>& order_;
};

} // namespace

int main()
{
    source::GameModeStartupState startup;
    startup.Run(true);
    auto startupFrame = startup.OnFrame(1.5F);
    if (!startupFrame.active ||
        std::abs(startupFrame.firstLogoAlpha - 0.5F) > 0.000001F ||
        startupFrame.secondLogoAlpha != 0.0F || startupFrame.loadFrame)
        return fail("first source startup logo timing differs");
    startupFrame = startup.OnFrame(5.5F);
    if (startupFrame.firstLogoAlpha != 0.0F ||
        startupFrame.secondLogoAlpha != 0.0F ||
        std::abs(startup.ElapsedSeconds() - 7.0F) > 0.000001F)
        return fail("source inter-logo blank frame differs");
    startupFrame = startup.OnFrame(1.5F);
    if (std::abs(startupFrame.secondLogoAlpha - 1.0F) > 0.000001F)
        return fail("second source startup logo timing differs");
    startup.SkipIntro();
    startupFrame = startup.OnFrame(0.0F);
    if (!startupFrame.active || !startupFrame.loadFrame ||
        startupFrame.startGame)
        return fail("source -2 startup loading frame differs");
    startupFrame = startup.OnFrame(0.0F);
    if (startupFrame.active || startupFrame.loadFrame ||
        !startupFrame.prepareGame || !startupFrame.freeIntro ||
        !startupFrame.startGame || startup.IsActive() ||
        !startup.IsPrepared() || !startup.IsStarted())
        return fail("source -3 StartGame transition differs");
    startup.Run(true);
    if (startup.IsActive())
        return fail("source Run restarted an already started GameMode");

    source::GameModeStartupState noIntroStartup;
    noIntroStartup.Run(false);
    if (!noIntroStartup.IsActive())
        return fail("source Run(false) skipped the startup state");
    auto noIntroFrame = noIntroStartup.OnFrame(0.0F);
    if (!noIntroFrame.active || noIntroFrame.loadFrame ||
        noIntroFrame.startGame)
        return fail("source Run(false) blank frame differs");
    noIntroFrame = noIntroStartup.OnFrame(0.0F);
    if (!noIntroFrame.active || !noIntroFrame.loadFrame ||
        noIntroFrame.startGame)
        return fail("source Run(false) -2 loading frame differs");
    noIntroFrame = noIntroStartup.OnFrame(0.0F);
    if (noIntroFrame.active || !noIntroFrame.prepareGame ||
        !noIntroFrame.freeIntro || !noIntroFrame.startGame ||
        !noIntroStartup.IsPrepared() || !noIntroStartup.IsStarted())
        return fail("source Run(false) -3 Prepare/Start differs");

    source::GameModeStartupMenuState startupMenu(true, true, true);
    if (startupMenu.Check() !=
            source::GameModeStartupMenuCommand::ShowStartOptions ||
        startupMenu.PreferredCameraAutodetectPending() ||
        !startupMenu.DiscreteVideoChangePending() ||
        startupMenu.Check() !=
            source::GameModeStartupMenuCommand::UseFixedFrameRate ||
        startupMenu.DiscreteVideoChangePending() ||
        startupMenu.Check() != source::GameModeStartupMenuCommand::None)
        return fail("source CheckStartupMenu precedence differs");
    source::GameModeStartupMenuState integratedGpu(false, true, false);
    if (integratedGpu.Check() !=
            source::GameModeStartupMenuCommand::ShowDiscreteVideoMessage ||
        integratedGpu.Check() != source::GameModeStartupMenuCommand::None)
        return fail("source integrated-GPU startup warning differs");

    source::GameModeMovieState movie;
    movie.Play();
    auto movieFrame = movie.OnFrame();
    if (!movieFrame.prepareWindow || movie.FrameState() != 1)
        return fail("source movie window preparation differs");
    movieFrame = movie.OnFrame();
    if (!movieFrame.enterVideoMode || !movie.IsVideoMode() ||
        movie.FrameState() != 2)
        return fail("source movie video-mode entry differs");
    for (int frame = 0; frame < 4; ++frame)
    {
        movieFrame = movie.OnFrame();
        if (movieFrame.openAndPlay)
            return fail("source movie opened before four wait frames");
    }
    movieFrame = movie.OnFrame();
    if (!movieFrame.openAndPlay || movie.FrameState() != 7)
        return fail("source movie Open/Play frame differs");
    movie.NotifyPlaybackEnded();
    movieFrame = movie.OnFrame();
    if (movieFrame.unload || movie.FrameState() != 9)
        return fail("source movie completion tail frame 8 differs");
    movieFrame = movie.OnFrame();
    if (!movieFrame.unload || movie.FrameState() != 10)
        return fail("source movie unload frame differs");
    movie.OnFrame();
    movie.OnFrame();
    movieFrame = movie.OnFrame();
    if (!movieFrame.exitVideoMode || !movieFrame.videoStopped ||
        movie.IsPlaying() || movie.IsVideoMode())
        return fail("source movie cVideoStopped transition differs");

    source::GameModeMusicFadeState musicFade;
    if (musicFade.GetVolume() != 1.0F ||
        musicFade.GetTargetVolume() != 1.0F ||
        musicFade.GetSpeed() != 0.0F)
        return fail("source music fade defaults differ");
    musicFade.FadeOutMusic(0.0F, 1.0F);
    if (musicFade.GetVolume() != 0.0F ||
        musicFade.GetTargetVolume() != 1.0F ||
        musicFade.GetSpeed() != 1.0F ||
        !musicFade.OnFrame(0.25F) ||
        std::abs(musicFade.GetVolume() - 0.25F) > 0.000001F)
        return fail("source FadeOutMusic first frame differs");
    if (!musicFade.OnFrame(0.25F) ||
        std::abs(musicFade.GetVolume() - 0.4375F) > 0.000001F)
        return fail("source music exponential interpolation differs");
    if (musicFade.OnFrame(1.0F, false) ||
        std::abs(musicFade.GetVolume() - 0.4375F) > 0.000001F)
        return fail("inactive source music fade advanced");
    musicFade.FadeInMusic(-1.0F, 2.0F);
    if (musicFade.GetTargetVolume() != 0.0F ||
        !musicFade.OnFrame(0.5F) ||
        std::abs(musicFade.GetVolume() - 0.328125F) > 0.000001F)
        return fail("source FadeInMusic target/speed differs");

    source::WorldFrameClock frameClock;
    if (frameClock.SmoothDelta(-1.0F) != 0.0F ||
        frameClock.GetSynchronizedFrameCount() != 1U)
        return fail("negative platform delta was not sanitized");
    frameClock.Reset();
    if (std::abs(frameClock.SmoothDelta(1.0F / 120.0F) -
                 1.0F / 120.0F) > 0.000001F ||
        std::abs(frameClock.SmoothDelta(1.0F / 40.0F) -
                 1.0F / 60.0F) > 0.000001F)
        return fail("15-frame synchronized average differs from source");
    frameClock.Reset();
    const auto halfStep = frameClock.Schedule(
        1.0F / 120.0F, true);
    const auto fullStep = frameClock.Schedule(
        1.0F / 120.0F, true);
    if (halfStep.fixedSteps != 0U ||
        !halfStep.lateProgressWithoutPhysics ||
        std::abs(halfStep.physicsAlpha - 0.5F) > 0.000001F ||
        fullStep.fixedSteps != 1U ||
        fullStep.lateProgressWithoutPhysics ||
        std::abs(fullStep.physicsAlpha) > 0.000001F)
        return fail("source 1/60 accumulator/alpha differs");
    const auto clamped = frameClock.Schedule(1.0F, false);
    if (clamped.fixedSteps != source::WorldFrameClock::maximumFixedSteps ||
        clamped.physicsAlpha != -1.0F ||
        std::abs(clamped.deltaTime - 7.0F / 60.0F) > 0.000001F)
        return fail("source maximum simulation delta differs");
    frameClock.Reset();
    frameClock.SmoothDelta(0.01F);
    if (frameClock.SmoothDelta(0.02F, false) != 0.02F ||
        frameClock.GetSynchronizedFrameCount() != 0U)
        return fail("disabled synchronization did not clear source history");

    std::vector<int> order;
    Host host(order);
    source::WorldEventPump world(&host);
    source::GameModeState game;
    world.SetGameMode(&game);

    ProgressProbe progress1(10, order, false);
    ProgressProbe progress2(11, order, true);
    world.RegProgressEvent(&progress1);
    world.RegProgressEvent(&progress1);
    world.RegProgressEvent(&progress2);
    world.Progress(0.1F);
    world.Progress(0.1F);
    if (order != std::vector<int>{1, 10, 11, 1, 10})
        return fail("logic/progress order or deferred removal differs");

    order.clear();
    FixedProbe fixed1(20, order);
    FixedProbe fixed2(21, order);
    LateProbe late1(30, order);
    LateProbe late2(31, order);
    FrameProbe frame1(40, order);
    FrameProbe frame2(41, order);
    world.RegFixedStepEvent(&fixed1);
    world.RegFixedStepEvent(&fixed2);
    world.RegLateProgressEvent(&late1);
    world.RegLateProgressEvent(&late2);
    world.RegFrameEvent(&frame1);
    world.RegFrameEvent(&frame2);
    world.FixedStep(0.02F);
    world.LateProgress(0.02F, true);
    world.FrameStep(0.02F, 0.5F);
    if (order != std::vector<int>{20, 21, 30, 31, 40, 41, 2, 3, 4})
        return fail("fixed/late/frame/backend order differs from World.cpp");

    order.clear();
    world.Pause(true);
    world.FrameStep(0.02F, 0.5F);
    if (order != std::vector<int>{3})
        return fail("paused frame did not preserve only network/game path");
    world.ResetInput(true);
    if (!world.InputWasReset())
        return fail("World::ResetInput state was not retained");
    world.ResetInput(false);
    world.Pause(false);

    game.StartMatch();
    if (!game.IsMatchStarted() || !game.StartRace())
        return fail("match/race admission failed");
    if (game.TakeCommands() !=
        std::vector<source::GameModeCommand>{
            source::GameModeCommand::ShowRaceInfo})
        return fail("StartRace did not select Menu::msInfo");
    game.OnLoadingFramePresented();
    world.FrameStep(0.02F, 0.0F);
    if (!game.TakeCommands().empty())
        return fail("DoStartRace fired before the second source frame");
    game.OnLoadingFramePresented();
    world.FrameStep(0.02F, 0.0F);
    if (game.TakeCommands() !=
            std::vector<source::GameModeCommand>{
                source::GameModeCommand::DoStartRace} ||
        game.IsRaceLoading() || !game.IsRaceActive())
        return fail("two-frame StartRace -> DoStartRace gate differs");

    game.Pause(true);
    if (game.TakeCommands() !=
        std::vector<source::GameModeCommand>{
            source::GameModeCommand::MuteEffects})
        return fail("pause did not emit effects mute");
    game.Pause(false);
    if (game.TakeCommands() !=
        std::vector<source::GameModeCommand>{
            source::GameModeCommand::UnmuteEffects})
        return fail("resume did not emit effects unmute");

    if (!game.ExitRace(true) || game.IsRaceActive())
        return fail("ExitRace did not leave active source state");
    if (game.TakeCommands() !=
        std::vector<source::GameModeCommand>{
            source::GameModeCommand::StopCommentator,
            source::GameModeCommand::StopGameMusic,
            source::GameModeCommand::ClearSemaphore,
            source::GameModeCommand::ExitRaceBackend,
            source::GameModeCommand::SaveGame})
        return fail("ExitRace backend command order differs");
    game.ExitRaceGoFinish();
    game.OnFinishFrameClose();
    if (game.TakeCommands() !=
        std::vector<source::GameModeCommand>{
            source::GameModeCommand::ShowFinish,
            source::GameModeCommand::StopCommentator,
            source::GameModeCommand::FadeOutMusic,
            source::GameModeCommand::ResumeMenuMusic})
        return fail("finish/close command order differs");
    if (game.GetMusicSourceVolume() != 0.0F ||
        game.GetMusicFadeState().GetTargetVolume() != 1.0F ||
        !game.OnMusicFrame(0.25F) ||
        std::abs(game.GetMusicSourceVolume() - 0.25F) > 0.000001F)
        return fail("OnFinishFrameClose music fade ownership differs");

    order.clear();
    UserProbe user1(100, order);
    UserProbe user2(200, order);
    game.RegUser(&user1);
    game.RegUser(&user1);
    game.RegUser(&user2);
    const source::GameModeEventData data{7, true};
    game.SendEvent(5U, &data);
    if (order != std::vector<int>{112, 212})
        return fail("GameMode user order/duplicate suppression differs");
    game.UnregUser(&user1);
    order.clear();
    game.SendEvent(1U);
    if (order != std::vector<int>{201})
        return fail("GameMode user removal differs");

    std::cout << "original World/GameMode smoke passed\n";
    return 0;
}
