#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace r3d::game::originalrace
{

// Names intentionally match GameMode::PreferredCamera.  Keeping the original
// serialized tokens lets an existing Windows user.xml be imported without a
// migration step.
enum class PreferredCamera
{
    ThirdPerson,
    Isometric,
};

struct QualityConfig
{
    std::uint32_t filtering = 3;
    std::uint32_t msaa = 0;
    std::uint32_t shadow = 1;
    std::uint32_t environment = 2;
    std::uint32_t light = 2;
    std::uint32_t postEffect = 2;
    std::string frameRateMode = "sfrFixed";
};

struct UserConfig
{
    QualityConfig quality;
    PreferredCamera preferredCamera = PreferredCamera::Isometric;
    float cameraDistance = 1.25F;
    float musicVolume = 1.2F;
    float effectsVolume = 0.8F;
    float voiceVolume = 1.2F;
    std::uint32_t resolutionWidth = 1280;
    std::uint32_t resolutionHeight = 800;
    std::uint32_t maxPlayers = 6;
    std::uint32_t maxComputers = 5;
    std::uint32_t upgradeMaxLevel = 2;
    std::uint32_t weaponMaxLevel = 4;
    std::uint32_t lapsCount = 4;
    bool springBorders = true;
    bool enableHud = true;
    bool enableMineBug = true;
    bool disableVideo = false;
    bool fullScreen = true;
    bool discreteVideoCard = true;
    std::string language = "english";
    std::string commentatorStyle = "english";
    std::map<std::string, std::string> keyboardControls;
    std::map<std::string, std::string> gamepadControls;
    std::string menuMusicPlaylist = "2";
    std::string gameMusicPlaylist = "6,10,4,7,1,9,8,3,5,0";
};

struct ProfileSlot
{
    std::string record;
    std::uint32_t charge = 0;
    bool hasCharge = false;
};

struct PlanetProgress
{
    std::uint32_t state = 2;
    std::uint32_t pass = 0;
};

struct PlayerProfile
{
    static constexpr std::size_t planetCount = 6;
    static constexpr std::size_t slotCount = 10;
    static constexpr std::size_t hyperSlot = 4;
    static constexpr std::size_t mineSlot = 5;
    static constexpr std::size_t firstWeaponSlot = 6;
    static constexpr std::size_t weaponSlotCount = 4;

    std::string name = "profile1";
    std::string difficulty = "gdNormal";
    bool carChanged = false;
    std::uint32_t minimumDifficulty = 0;
    std::array<PlanetProgress, planetCount> planets{};
    std::uint32_t currentTrack = 0;
    std::uint32_t currentPlanet = 0;
    std::uint32_t currentPass = 1;
    std::string currentCar =
        "world\\db\\root\\ctCar\\marauder";
    std::uint32_t playerId = 0;
    std::uint32_t gamerId = 10;
    std::uint32_t networkSlot = 0;
    std::array<float, 4> color{1.0F, 1.0F, 1.0F, 1.0F};
    std::uint32_t money = 0;
    std::uint32_t points = 0;
    std::array<ProfileSlot, slotCount> slots{};
};

struct AchievementRecord
{
    std::string name;
    std::string library;
    std::string record;
};

struct AchievementItemProfile
{
    std::uint32_t classId = 0;
    std::map<std::string, std::string> values;
    std::vector<AchievementRecord> records;
};

struct AchievementConditionProfile
{
    std::uint32_t classId = 0;
    std::map<std::string, std::string> values;
};

struct ProfileState
{
    UserConfig config;
    // GameMode::LoadGameOpt distinguishes an absent serialized camera from
    // the pcIsometric default.  The distinction drives StartOptionsMenu on
    // first launch and must survive the portable XML adapter.
    bool preferredCameraSerialized = false;
    bool discreteVideoCardSerialized = false;
    PlayerProfile player;
    std::vector<std::string> profiles{"profile1"};
    std::vector<std::string> networkProfiles;
    std::string lastNetworkProfile;
    std::vector<std::uint32_t> planetsCompleted;
    std::uint32_t achievementPoints = 0;
    std::map<std::string, AchievementItemProfile> achievementItems;
    std::map<std::string, AchievementConditionProfile>
        achievementConditions;
    std::map<std::string, std::uint32_t> achievementIterations;
    std::uint32_t tutorialStage = 3;
};

// Deterministic copy of the defaults installed before original profile XML
// is read.  Smoke tests use it so a player's saved workshop loadout cannot
// change source-provenance assertions.
ProfileState makeOriginalDefaultProfileState();

// Source-equivalent Race::MakeProfileName/NewProfile helpers used by the
// portable MainMenu2 flow.  Championship profiles are persistent; the
// original SkProfile named "skirmish" is temporary and is never added to
// race.xml's profile list.
std::string makeOriginalProfileName(
    const ProfileState& state, std::string_view base = "profile");
std::string beginOriginalChampionshipProfile(
    ProfileState& state, std::string_view difficulty);
PlayerProfile makeOriginalSkirmishProfile(
    const ProfileState& state, std::string_view difficulty);
ProfileState makeOriginalSkirmishPersistenceState(
    const ProfileState& runtimeState,
    const PlayerProfile& championshipPlayer);
bool runOriginalProfileFlowSmokeTest(std::string& error);

class OriginalProfileStore
{
public:
    explicit OriginalProfileStore(
        std::filesystem::path saveDirectory,
        std::filesystem::path legacyDirectory = {});

    ProfileState load(std::string& warning) const;
    bool selectProfile(ProfileState& state, std::string_view name,
                       std::string& error) const;
    bool deleteProfile(ProfileState& state, std::string_view name,
                       std::string& error) const;
    bool save(const ProfileState& state, std::string& error) const;

    const std::filesystem::path& saveDirectory() const noexcept;

private:
    std::filesystem::path loadPath(
        const std::filesystem::path& relative) const;

    std::filesystem::path saveDirectory_;
    std::filesystem::path legacyDirectory_;
};

} // namespace r3d::game::originalrace
