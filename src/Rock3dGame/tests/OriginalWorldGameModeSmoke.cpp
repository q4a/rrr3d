#include "OriginalGameMode.h"
#include "OriginalWorld.h"

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
