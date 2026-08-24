#include "OriginalProfile.h"

#include <tinyxml.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <limits>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>

namespace r3d::game::originalrace
{
namespace
{

TiXmlElement* child(TiXmlNode* node, const char* name)
{
    return node == nullptr ? nullptr : node->FirstChildElement(name);
}

const char* value(TiXmlNode* node, const char* name)
{
    auto* item = child(node, name);
    return item == nullptr ? nullptr : item->GetText();
}

void append(TiXmlNode& parent, const char* name, const std::string& text)
{
    auto* item = new TiXmlElement(name);
    item->LinkEndChild(new TiXmlText(text));
    parent.LinkEndChild(item);
}

void append(TiXmlNode& parent, const char* name, const char* text)
{
    append(parent, name, std::string(text));
}

void append(TiXmlNode& parent, const char* name, float number)
{
    std::ostringstream stream;
    stream.precision(8);
    stream << number;
    append(parent, name, stream.str());
}

void append(TiXmlNode& parent, const char* name, std::uint32_t number)
{
    append(parent, name, std::to_string(number));
}

void append(TiXmlNode& parent, const char* name, bool flag)
{
    append(parent, name, flag ? "true" : "false");
}

bool parseBool(const char* text, bool fallback)
{
    if (text == nullptr)
        return fallback;
    const std::string token(text);
    if (token == "true" || token == "1")
        return true;
    if (token == "false" || token == "0")
        return false;
    return fallback;
}

std::uint32_t parseUnsigned(const char* text, std::uint32_t fallback)
{
    if (text == nullptr)
        return fallback;
    try
    {
        const unsigned long parsed = std::stoul(text);
        return parsed > std::numeric_limits<std::uint32_t>::max()
                   ? fallback
                   : static_cast<std::uint32_t>(parsed);
    }
    catch (const std::exception&)
    {
        return fallback;
    }
}

float parseFloat(const char* text, float fallback)
{
    if (text == nullptr)
        return fallback;
    try
    {
        const float parsed = std::stof(text);
        return std::isfinite(parsed) ? parsed : fallback;
    }
    catch (const std::exception&)
    {
        return fallback;
    }
}

std::string cleanFileName(std::string name)
{
    name.erase(std::remove_if(name.begin(), name.end(), [](char item) {
                   return item == '/' || item == '\\' || item == ':' ||
                          item == '\0';
               }),
               name.end());
    return name.empty() ? "profile1" : name;
}

std::string workshopReference(std::string_view record)
{
    return "world\\race\\workshopRoot\\workshop\\" +
           std::string(record);
}

void installOriginalMobilityDefaults(PlayerProfile& profile)
{
    // Garage::Car::InstallDefaults always equips the four locked mobility
    // slots before a Windows profile is applied.  Some legacy/profile files
    // omit those locked slots and serialize only charge-bearing equipment.
    // Treating an omitted locked slot as unequipped gives Player::ApplyMobility
    // zero engine torque and zero armour, so steering still works but the car
    // cannot move and dies on the first damage event.
    profile.slots[0].record = workshopReference("wheel1");
    profile.slots[1].record = workshopReference("truba1");
    profile.slots[2].record = workshopReference("armor1");
    profile.slots[3].record = workshopReference("engine1");
}

void installOriginalDefaults(ProfileState& state)
{
    state.config.keyboardControls = {
        {"gaAccel", "Up Arrow"},       {"gaBreak", "Down Arrow"},
        {"gaWheelLeft", "Left Arrow"}, {"gaWheelRight", "Right Arrow"},
        {"gaShot", "W"},               {"gaShot1", "1"},
        {"gaShot2", "2"},              {"gaShot3", "3"},
        {"gaShot4", "4"},              {"gaShotAll", "Space"},
        {"gaHyper", "Q"},              {"gaMine", "E"},
        {"gaWeaponDown", "None"},       {"gaWeaponUp", "None"},
        {"gaViewSwitch", "C"},          {"gaAction", "Enter"},
        {"gaEscape", "Escape"},         {"gaResetCar", "R"},
        {"gaDebug1", "F1"},             {"gaDebug2", "F2"},
        {"gaDebug3", "F3"},             {"gaDebug4", "F4"},
        {"gaDebug5", "F5"},             {"gaDebug6", "F6"},
        {"gaDebug7", "F7"},
    };
    state.config.gamepadControls = {
        {"gaAccel", "A"},
        {"gaBreak", "B"},
        {"gaWheelLeft", "DPad Left"},
        {"gaWheelRight", "DPad Right"},
        {"gaShot", "X"},
        {"gaShot1", "None"},
        {"gaShot2", "None"},
        {"gaShot3", "None"},
        {"gaShot4", "None"},
        {"gaShotAll", "Y"},
        {"gaHyper", "Left Trigger"},
        {"gaMine", "Right Trigger"},
        {"gaWeaponDown", "Left Shoulder"},
        {"gaWeaponUp", "Right Shoulder"},
        {"gaViewSwitch", "R.Thumb Press"},
        {"gaAction", "A"},
        {"gaEscape", "Start"},
        {"gaResetCar", "Back"},
        {"gaDebug1", "None"},
        {"gaDebug2", "None"},
        {"gaDebug3", "None"},
        {"gaDebug4", "None"},
        {"gaDebug5", "None"},
        {"gaDebug6", "None"},
        {"gaDebug7", "None"},
    };

    state.player.planets.front() = {0, 1};
    installOriginalMobilityDefaults(state.player);
    state.player.slots[4] = {workshopReference("spring"), 1, true};
    state.player.slots[5] = {workshopReference("maslo"), 1, true};
    state.player.slots[6] = {workshopReference("bulletGun"), 4, true};
}

void readControlMap(TiXmlNode* controls, const char* controller,
                    std::map<std::string, std::string>& output)
{
    auto* root = child(controls, controller);
    if (root == nullptr)
        return;
    output.clear();
    for (auto* item = root->FirstChildElement(); item != nullptr;
         item = item->NextSiblingElement())
    {
        if (item->GetText() != nullptr)
            output[item->Value()] = item->GetText();
    }
}

void loadConfig(const std::filesystem::path& path, UserConfig& config,
                bool& preferredCameraSerialized,
                bool& discreteVideoCardSerialized)
{
    preferredCameraSerialized = false;
    discreteVideoCardSerialized = false;
    TiXmlDocument document(path.string());
    if (!document.LoadFile() || document.RootElement() == nullptr)
        return;
    auto* root = document.RootElement();
    auto* quality = child(root, "quality");
    config.quality.filtering = parseUnsigned(
        value(quality, "filtering"), config.quality.filtering);
    config.quality.msaa =
        parseUnsigned(value(quality, "msaa"), config.quality.msaa);
    config.quality.shadow =
        parseUnsigned(value(quality, "shadow"), config.quality.shadow);
    config.quality.environment = parseUnsigned(
        value(quality, "environment"), config.quality.environment);
    config.quality.light =
        parseUnsigned(value(quality, "light"), config.quality.light);
    config.quality.postEffect = parseUnsigned(
        value(quality, "postEffect"), config.quality.postEffect);
    if (const char* token = value(quality, "frameRateMode"))
        config.quality.frameRateMode = token;

    if (const char* resolution = value(root, "resolution"))
    {
        std::istringstream stream(resolution);
        stream >> config.resolutionWidth >> config.resolutionHeight;
    }

    auto* volume = child(root, "volume");
    config.musicVolume = std::clamp(
        parseFloat(value(volume, "musicVolume"), config.musicVolume),
        0.0F, 2.0F);
    config.effectsVolume = std::clamp(
        parseFloat(value(volume, "effectsVolume"), config.effectsVolume),
        0.0F, 2.0F);
    config.voiceVolume = std::clamp(
        parseFloat(value(volume, "voiceVolume"), config.voiceVolume),
        0.0F, 2.0F);
    config.maxPlayers =
        parseUnsigned(value(root, "maxPlayers"), config.maxPlayers);
    config.maxComputers =
        parseUnsigned(value(root, "maxComputers"), config.maxComputers);
    config.upgradeMaxLevel = parseUnsigned(
        value(root, "upgradeMaxLevel"), config.upgradeMaxLevel);
    config.weaponMaxLevel = parseUnsigned(
        value(root, "weaponMaxLevel"), config.weaponMaxLevel);
    config.lapsCount =
        parseUnsigned(value(root, "lapsCount"), config.lapsCount);
    config.springBorders =
        parseBool(value(root, "springBorders"), config.springBorders);
    config.enableHud =
        parseBool(value(root, "enableHUD"), config.enableHud);
    config.enableMineBug =
        parseBool(value(root, "enableMineBug"), config.enableMineBug);
    config.disableVideo =
        parseBool(value(root, "disableVideo"), config.disableVideo);
    config.fullScreen =
        parseBool(value(root, "fullScreen"), config.fullScreen);
    if (const char* token = value(root, "discreteVideoCard"))
    {
        config.discreteVideoCard =
            parseBool(token, config.discreteVideoCard);
        discreteVideoCardSerialized = true;
    }
    if (const char* token = value(root, "language"))
        config.language = token;
    if (const char* token = value(root, "commentatorStyle"))
        config.commentatorStyle = token;
    if (const char* token = value(root, "prefCamera"))
    {
        config.preferredCamera =
            std::string(token) == "pcThirdPerson"
                ? PreferredCamera::ThirdPerson
                : PreferredCamera::Isometric;
        preferredCameraSerialized = true;
    }
    config.cameraDistance = std::clamp(
        parseFloat(value(root, "cameraDistance"), config.cameraDistance),
        0.6F, 2.5F);
    readControlMap(child(root, "controls"), "ctKeyboard",
                   config.keyboardControls);
    readControlMap(child(root, "controls"), "ctGamepad",
                   config.gamepadControls);
    if (const char* token = value(child(root, "menuMusic"), "playList"))
        config.menuMusicPlaylist = token;
    if (const char* token = value(child(root, "gameMusic"), "playList"))
        config.gameMusicPlaylist = token;
}

std::vector<std::string> splitList(const char* text)
{
    std::vector<std::string> result;
    if (text == nullptr)
        return result;
    std::string token;
    std::istringstream stream(text);
    while (std::getline(stream, token, ','))
    {
        token.erase(
            std::remove_if(token.begin(), token.end(),
                           [](unsigned char item) {
                               return std::isspace(item) != 0;
                           }),
            token.end());
        if (!token.empty())
            result.push_back(token);
    }
    return result;
}

std::string joinList(const std::vector<std::string>& values)
{
    std::string result;
    for (const auto& item : values)
    {
        if (!result.empty())
            result += ',';
        result += item;
    }
    return result;
}

void loadRaceLibrary(const std::filesystem::path& path,
                     ProfileState& state)
{
    TiXmlDocument document(path.string());
    if (!document.LoadFile() || document.RootElement() == nullptr)
        return;
    auto* root = document.RootElement();
    state.profiles = splitList(value(root, "profiles"));
    state.networkProfiles = splitList(value(root, "netProfiles"));
    state.lastProfile.clear();
    state.player.name.clear();
    if (const char* token = value(root, "lastProfile");
        token != nullptr && *token != '\0')
    {
        const auto lastProfile = cleanFileName(token);
        if (std::find(
                state.profiles.begin(), state.profiles.end(),
                lastProfile) != state.profiles.end())
        {
            state.lastProfile = lastProfile;
            state.player.name = lastProfile;
        }
    }
    if (state.player.name.empty() && !state.profiles.empty())
    {
        state.lastProfile = cleanFileName(state.profiles.front());
        state.player.name = state.lastProfile;
    }
    if (const char* token = value(root, "lastNetProfile"))
        state.lastNetworkProfile = cleanFileName(token);
    state.planetsCompleted.clear();
    for (const auto& token : splitList(value(root, "planetsCompleted")))
        state.planetsCompleted.push_back(parseUnsigned(token.c_str(), 0U));
    state.tutorialStage =
        parseUnsigned(value(root, "tutorialStage"), state.tutorialStage);
}

void loadProfileRoot(TiXmlElement* root, PlayerProfile& profile)
{
    if (root == nullptr)
        return;
    profile.carChanged =
        parseBool(value(root, "carChanged"), profile.carChanged);
    profile.minimumDifficulty = parseUnsigned(
        value(root, "minDifficulty"), profile.minimumDifficulty);
    for (std::size_t index = 0; index < profile.planets.size(); ++index)
    {
        const std::string nodeName = "planet" + std::to_string(index);
        auto* planet = child(root, nodeName.c_str());
        if (planet == nullptr)
            continue;
        profile.planets[index].state = parseUnsigned(
            value(planet, "state"), profile.planets[index].state);
        profile.planets[index].pass = parseUnsigned(
            value(planet, "pass"), profile.planets[index].pass);
        // Planet::Reset starts at psUnavailable/pass 0.  In the Windows
        // lifecycle, moving to pass 1 is only possible through
        // Unlock()->Open()->SetPass(1), which stores psOpen.  Older macOS
        // builds advanced `pass` directly and therefore wrote an impossible
        // psUnavailable/pass>0 pair.  Preserve the earned pass while restoring
        // the exact source invariant so CompletePass(pass - 1) can rebuild the
        // original Workshop assortment.
        if (profile.planets[index].state == 2U &&
            profile.planets[index].pass > 0U)
        {
            profile.planets[index].state = 0U;
        }
    }
    profile.currentPlanet = std::min<std::uint32_t>(
        parseUnsigned(value(root, "planet"), profile.currentPlanet),
        static_cast<std::uint32_t>(profile.planets.size() - 1U));
    profile.currentTrack =
        parseUnsigned(value(root, "track"), profile.currentTrack);
    profile.currentPass = profile.planets[profile.currentPlanet].pass;
    if (const char* token = value(root, "dfficulty"))
        profile.difficulty = token;

    auto* humans = child(root, "humans");
    auto* human = humans == nullptr ? nullptr
                                    : humans->FirstChildElement();
    if (human == nullptr)
        return;
    for (auto& slot : profile.slots)
        slot = {};
    installOriginalMobilityDefaults(profile);
    if (const char* token = value(human, "car"))
        profile.currentCar = token;
    profile.playerId =
        parseUnsigned(value(human, "plrId"), profile.playerId);
    profile.gamerId =
        parseUnsigned(value(human, "gamerId"), profile.gamerId);
    profile.networkSlot =
        parseUnsigned(value(human, "netSlot"), profile.networkSlot);
    if (const char* token = value(human, "color"))
    {
        std::istringstream stream(token);
        stream >> profile.color[0] >> profile.color[1] >>
            profile.color[2] >> profile.color[3];
    }
    profile.money = parseUnsigned(value(human, "money"), profile.money);
    profile.points = parseUnsigned(value(human, "points"), profile.points);
    for (std::size_t index = 0; index < profile.slots.size(); ++index)
    {
        const std::string nodeName = "slot" + std::to_string(index);
        auto* slot = child(human, nodeName.c_str());
        if (slot == nullptr || slot->GetText() == nullptr)
            continue;
        profile.slots[index].record = slot->GetText();
        if (const char* charge = slot->Attribute("charge"))
        {
            profile.slots[index].charge =
                parseUnsigned(charge, profile.slots[index].charge);
            profile.slots[index].hasCharge = true;
        }
    }
}

void loadProfile(const std::filesystem::path& path,
                 PlayerProfile& profile)
{
    TiXmlDocument document(path.string());
    if (!document.LoadFile() || document.RootElement() == nullptr)
        return;
    loadProfileRoot(document.RootElement(), profile);
}

void loadAchievements(const std::filesystem::path& path,
                      ProfileState& state)
{
    TiXmlDocument document(path.string());
    if (!document.LoadFile() || document.RootElement() == nullptr)
        return;
    auto* root = document.RootElement();
    state.achievementPoints = parseUnsigned(
        value(root, "points"), state.achievementPoints);
    auto* items = child(root, "items");
    if (items != nullptr)
    {
        for (auto* item = items->FirstChildElement(); item != nullptr;
             item = item->NextSiblingElement())
        {
            auto& output = state.achievementItems[item->Value()];
            if (const char* classId = item->Attribute("classId"))
                output.classId =
                    parseUnsigned(classId, output.classId);
            for (auto* field = item->FirstChildElement();
                 field != nullptr; field = field->NextSiblingElement())
            {
                if (std::string_view(field->Value()) == "records")
                {
                    output.records.clear();
                    for (auto* record = field->FirstChildElement();
                         record != nullptr;
                         record = record->NextSiblingElement())
                    {
                        AchievementRecord loaded;
                        loaded.name = record->Value();
                        if (const char* library =
                                record->Attribute("lib"))
                            loaded.library = library;
                        if (record->GetText() != nullptr)
                            loaded.record = record->GetText();
                        output.records.push_back(std::move(loaded));
                    }
                }
                else if (field->GetText() != nullptr)
                {
                    output.values[field->Value()] = field->GetText();
                }
            }
        }
    }
    auto* conditions = child(root, "conditions");
    if (conditions == nullptr)
        return;
    for (auto* condition = conditions->FirstChildElement();
         condition != nullptr;
         condition = condition->NextSiblingElement())
    {
        auto& output =
            state.achievementConditions[condition->Value()];
        if (const char* classId = condition->Attribute("classId"))
            output.classId =
                parseUnsigned(classId, output.classId);
        for (auto* field = condition->FirstChildElement();
             field != nullptr; field = field->NextSiblingElement())
        {
            if (field->GetText() != nullptr)
                output.values[field->Value()] = field->GetText();
        }
        state.achievementIterations[condition->Value()] =
            parseUnsigned(value(condition, "iterNum"),
                          state.achievementIterations[
                              condition->Value()]);
    }
}

bool saveAtomic(TiXmlDocument& document,
                const std::filesystem::path& destination,
                std::string& error)
{
    std::error_code fileError;
    std::filesystem::create_directories(destination.parent_path(),
                                        fileError);
    if (fileError)
    {
        error = "unable to create profile directory: " +
                fileError.message();
        return false;
    }
    auto temporary = destination;
    temporary += ".tmp";
    if (!document.SaveFile(temporary.string()))
    {
        error = "unable to write " + temporary.string();
        return false;
    }
    std::filesystem::rename(temporary, destination, fileError);
    if (fileError)
    {
        std::filesystem::remove(destination, fileError);
        fileError.clear();
        std::filesystem::rename(temporary, destination, fileError);
    }
    if (fileError)
    {
        error = "unable to install " + destination.string() + ": " +
                fileError.message();
        return false;
    }
    return true;
}

void appendControls(
    TiXmlNode& controls, const char* name,
    const std::map<std::string, std::string>& bindings)
{
    auto* controller = new TiXmlElement(name);
    controls.LinkEndChild(controller);
    for (const auto& [action, key] : bindings)
        append(*controller, action.c_str(), key);
}

TiXmlElement* appendReference(TiXmlNode& parent, const char* name,
                              const ProfileSlot& slot)
{
    auto* item = new TiXmlElement(name);
    if (slot.hasCharge)
        item->SetAttribute("charge",
                           static_cast<int>(slot.charge));
    item->SetAttribute("lib", "world\\race\\workshop");
    item->LinkEndChild(new TiXmlText(slot.record));
    parent.LinkEndChild(item);
    return item;
}

} // namespace

ProfileState makeOriginalDefaultProfileState()
{
    ProfileState state;
    installOriginalDefaults(state);
    return state;
}

std::string makeOriginalProfileName(
    const ProfileState& state, std::string_view base)
{
    const std::string prefix =
        base.empty() ? std::string("profile") : std::string(base);
    for (std::uint32_t suffix = 1U;; ++suffix)
    {
        const std::string candidate =
            prefix + std::to_string(suffix);
        if (std::find(state.profiles.begin(), state.profiles.end(),
                      candidate) == state.profiles.end() &&
            std::find(state.networkProfiles.begin(),
                      state.networkProfiles.end(), candidate) ==
                state.networkProfiles.end())
        {
            return candidate;
        }
    }
}

std::string beginOriginalChampionshipProfile(
    ProfileState& state, std::string_view difficulty, bool network)
{
    const std::string name = makeOriginalProfileName(state);
    auto player = makeOriginalDefaultProfileState().player;
    player.name = name;
    player.difficulty = std::string(difficulty);
    state.player = std::move(player);
    auto& profiles = network ? state.networkProfiles : state.profiles;
    profiles.push_back(name);
    if (network)
        state.lastNetworkProfile = name;
    else
        state.lastProfile = name;
    return name;
}

PlayerProfile makeOriginalSkirmishProfile(
    const ProfileState& state, std::string_view difficulty)
{
    auto player = makeOriginalDefaultProfileState().player;
    player.name = "skirmish";
    player.difficulty = std::string(difficulty);

    // SkProfile::EnterGame opens every planet recorded by
    // Race::GetPlanetsCompleted, then always opens planet zero and selects it.
    for (const auto index : state.planetsCompleted)
    {
        if (index < player.planets.size())
            player.planets[index] = {0U, 1U};
    }
    player.planets.front() = {0U, 1U};
    player.currentPlanet = 0U;
    player.currentTrack = 0U;
    player.currentPass = 1U;
    return player;
}

std::string serializeOriginalNetworkProfile(
    const PlayerProfile& player, bool championship)
{
    TiXmlDocument document;
    document.LinkEndChild(new TiXmlDeclaration("1.0", "UTF-8", ""));
    auto* profile = new TiXmlElement("profile");
    document.LinkEndChild(profile);

    if (championship)
    {
        append(*profile, "carChanged", player.carChanged);
        append(*profile, "minDifficulty", player.minimumDifficulty);
        for (std::size_t index = 0; index < player.planets.size(); ++index)
        {
            auto* planet = new TiXmlElement(
                ("planet" + std::to_string(index)).c_str());
            profile->LinkEndChild(planet);
            append(*planet, "state", player.planets[index].state);
            append(*planet, "pass", player.planets[index].pass);
        }
        append(*profile, "planet", player.currentPlanet);
        append(*profile, "track", player.currentTrack);
        auto* humans = new TiXmlElement("humans");
        profile->LinkEndChild(humans);
        auto* human = new TiXmlElement("human0");
        humans->LinkEndChild(human);
        auto* car = new TiXmlElement("car");
        car->SetAttribute("lib", "world\\db\\ctCar");
        car->LinkEndChild(new TiXmlText(player.currentCar));
        human->LinkEndChild(car);
        append(*human, "plrId", player.playerId);
        append(*human, "gamerId", player.gamerId);
        append(*human, "netSlot", player.networkSlot);
        std::ostringstream color;
        color.precision(8);
        color << player.color[0] << ' ' << player.color[1] << ' '
              << player.color[2] << ' ' << player.color[3];
        append(*human, "color", color.str());
        append(*human, "money", player.money);
        append(*human, "points", player.points);
        for (std::size_t index = 0; index < player.slots.size(); ++index)
        {
            if (player.slots[index].record.empty())
                continue;
            appendReference(
                *human, ("slot" + std::to_string(index)).c_str(),
                player.slots[index]);
        }
    }
    append(*profile, "dfficulty", player.difficulty);

    TiXmlPrinter printer;
    printer.SetIndent("");
    printer.SetLineBreak("");
    document.Accept(&printer);
    return printer.CStr();
}

bool deserializeOriginalNetworkProfile(
    std::string_view xml, bool championship, PlayerProfile& profile,
    std::string& error)
{
    error.clear();
    if (xml.empty())
    {
        error = "NetRace profile XML is empty";
        return false;
    }

    const std::string source(xml);
    TiXmlDocument document;
    document.Parse(source.c_str(), nullptr, TIXML_ENCODING_UTF8);
    auto* root = document.RootElement();
    if (document.Error() || root == nullptr ||
        std::string_view(root->Value()) != "profile")
    {
        error = document.ErrorDesc() != nullptr
                    ? document.ErrorDesc()
                    : "NetRace profile XML has no profile root";
        return false;
    }

    const auto name = profile.name;
    auto decoded = makeOriginalDefaultProfileState().player;
    decoded.name = name.empty() ? "netClient" : name;
    if (championship)
    {
        // SnProfile::LoadGame restores tournament and the serialized human.
        loadProfileRoot(root, decoded);
    }
    else
    {
        // SkProfile::LoadGame calls EnterGame and Profile::LoadGame then reads
        // only the common, intentionally misspelled difficulty value.
        decoded.planets.front() = {0U, 1U};
        decoded.currentPlanet = 0U;
        decoded.currentTrack = 0U;
        decoded.currentPass = 1U;
        if (const char* token = value(root, "dfficulty"))
            decoded.difficulty = token;
    }
    profile = std::move(decoded);
    return true;
}

ProfileState makeOriginalSkirmishPersistenceState(
    const ProfileState& runtimeState,
    const PlayerProfile& championshipPlayer)
{
    auto persisted = runtimeState;
    persisted.player = championshipPlayer;
    return persisted;
}

bool runOriginalProfileFlowSmokeTest(std::string& error)
{
    error.clear();
    auto state = makeOriginalDefaultProfileState();
    if (state.tutorialStage != 0U)
    {
        error = "Race::_tutorialStage did not start at source stage zero";
        return false;
    }
    state.profiles = {"profile1", "profile3"};
    state.player.name = "profile1";
    state.player.money = 999U;
    state.player.points = 123U;
    state.planetsCompleted = {2U, 4U, 99U};

    auto wireSource = state.player;
    wireSource.name = "hostProfile";
    wireSource.difficulty = "gdHard";
    wireSource.carChanged = true;
    wireSource.minimumDifficulty = 2U;
    wireSource.currentPlanet = 2U;
    wireSource.currentTrack = 1U;
    wireSource.planets[2] = {0U, 2U};
    wireSource.currentPass = 2U;
    wireSource.gamerId = 14U;
    wireSource.money = 1777U;
    wireSource.slots[6] =
        {workshopReference("rocketGun"), 3U, true};
    const auto wireXml =
        serializeOriginalNetworkProfile(wireSource, true);
    PlayerProfile wireDecoded;
    wireDecoded.name = "netClient";
    if (!deserializeOriginalNetworkProfile(
            wireXml, true, wireDecoded, error) ||
        wireDecoded.name != "netClient" ||
        wireDecoded.difficulty != wireSource.difficulty ||
        wireDecoded.carChanged != wireSource.carChanged ||
        wireDecoded.minimumDifficulty != wireSource.minimumDifficulty ||
        wireDecoded.currentPlanet != wireSource.currentPlanet ||
        wireDecoded.currentTrack != wireSource.currentTrack ||
        wireDecoded.currentPass != wireSource.currentPass ||
        wireDecoded.currentCar != wireSource.currentCar ||
        wireDecoded.gamerId != wireSource.gamerId ||
        wireDecoded.money != wireSource.money ||
        wireDecoded.slots[6].record != wireSource.slots[6].record ||
        wireDecoded.slots[6].charge != wireSource.slots[6].charge)
    {
        if (error.empty())
            error = "NetRace SnProfile XML round-trip changed source state";
        return false;
    }
    const auto skirmishWire =
        makeOriginalSkirmishProfile(state, "gdEasy");
    PlayerProfile skirmishDecoded;
    if (!deserializeOriginalNetworkProfile(
            serializeOriginalNetworkProfile(skirmishWire, false), false,
            skirmishDecoded, error) ||
        skirmishDecoded.difficulty != "gdEasy" ||
        skirmishDecoded.currentPlanet != 0U ||
        skirmishDecoded.currentPass != 1U)
    {
        if (error.empty())
            error = "NetRace SkProfile XML round-trip changed source state";
        return false;
    }

    const auto campaignBeforeSkirmish = state.player;
    const auto skirmish =
        makeOriginalSkirmishProfile(state, "gdEasy");
    if (skirmish.name != "skirmish" ||
        skirmish.difficulty != "gdEasy" ||
        skirmish.money != 0U || skirmish.points != 0U ||
        skirmish.planets[0].state != 0U ||
        skirmish.planets[2].state != 0U ||
        skirmish.planets[4].state != 0U ||
        skirmish.planets[1].state != 2U ||
        state.player.name != campaignBeforeSkirmish.name ||
        state.player.money != campaignBeforeSkirmish.money ||
        state.profiles.size() != 2U)
    {
        error =
            "Race::SkProfile flow did not remain temporary or did not "
            "open the source planets";
        return false;
    }

    auto skirmishRuntime = state;
    skirmishRuntime.player = skirmish;
    skirmishRuntime.config.lapsCount = 7U;
    skirmishRuntime.achievementPoints = 42U;
    const auto persisted =
        makeOriginalSkirmishPersistenceState(
            skirmishRuntime, campaignBeforeSkirmish);
    if (persisted.player.name != "profile1" ||
        persisted.player.money != 999U ||
        persisted.config.lapsCount != 7U ||
        persisted.achievementPoints != 42U ||
        std::find(
            persisted.profiles.begin(), persisted.profiles.end(),
            "skirmish") != persisted.profiles.end())
    {
        error =
            "SkProfile persistence did not retain global state while "
            "protecting the championship profile";
        return false;
    }

    const auto newName =
        beginOriginalChampionshipProfile(state, "gdHard");
    if (newName != "profile2" ||
        state.player.name != "profile2" ||
        state.player.difficulty != "gdHard" ||
        state.player.money != 0U || state.player.points != 0U ||
        state.player.planets[0].state != 0U ||
        state.player.planets[0].pass != 1U ||
        state.profiles !=
            std::vector<std::string>{
                "profile1", "profile3", "profile2"})
    {
        error =
            "Race::MakeProfileName/NewProfile championship semantics "
            "did not match the source";
        return false;
    }

    const auto smokeDirectory =
        std::filesystem::temp_directory_path() /
        ("rrr3d-profile-flow-smoke-" +
         std::to_string(
             reinterpret_cast<std::uintptr_t>(&state)));
    std::error_code fileError;
    std::filesystem::remove_all(smokeDirectory, fileError);
    OriginalProfileStore smokeStore(smokeDirectory);
    auto networkState = state;
    const auto networkName = beginOriginalChampionshipProfile(
        networkState, "gdEasy", true);
    networkState.player.money = 456U;
    if (networkName != "profile4" ||
        networkState.profiles != state.profiles ||
        networkState.networkProfiles !=
            std::vector<std::string>{"profile4"} ||
        networkState.lastProfile != "profile2" ||
        networkState.lastNetworkProfile != "profile4" ||
        !smokeStore.save(networkState, error))
    {
        std::filesystem::remove_all(smokeDirectory, fileError);
        if (error.empty())
            error = "network/offline profile library separation failed";
        return false;
    }
    std::string networkWarning;
    auto reloadedNetwork = smokeStore.load(networkWarning);
    if (!networkWarning.empty() ||
        reloadedNetwork.lastProfile != "profile2" ||
        reloadedNetwork.lastNetworkProfile != "profile4" ||
        reloadedNetwork.player.name != "profile2" ||
        !smokeStore.selectProfile(
            reloadedNetwork, "profile4", error, true) ||
        reloadedNetwork.player.name != "profile4" ||
        reloadedNetwork.player.money != 456U)
    {
        std::filesystem::remove_all(smokeDirectory, fileError);
        if (error.empty())
            error = "network profile selection replaced offline cursor";
        return false;
    }
    auto deleteState = makeOriginalDefaultProfileState();
    if (!smokeStore.save(deleteState, error) ||
        !smokeStore.deleteProfile(
            deleteState, "profile1", error))
    {
        std::filesystem::remove_all(smokeDirectory, fileError);
        return false;
    }
    std::string loadWarning;
    const auto reloaded = smokeStore.load(loadWarning);
    std::filesystem::remove_all(smokeDirectory, fileError);
    if (!loadWarning.empty() ||
        !deleteState.profiles.empty() ||
        !deleteState.player.name.empty() ||
        !reloaded.profiles.empty() ||
        !reloaded.player.name.empty())
    {
        error =
            "Race::DelProfile/SaveLib restored the deleted final "
            "profile";
        return false;
    }
    return true;
}

OriginalProfileStore::OriginalProfileStore(
    std::filesystem::path saveDirectory,
    std::filesystem::path legacyDirectory)
    : saveDirectory_(std::move(saveDirectory)),
      legacyDirectory_(std::move(legacyDirectory))
{
}

std::filesystem::path OriginalProfileStore::loadPath(
    const std::filesystem::path& relative) const
{
    const auto saved = saveDirectory_ / relative;
    std::error_code error;
    if (std::filesystem::is_regular_file(saved, error))
        return saved;
    const auto legacy = legacyDirectory_ / relative;
    error.clear();
    if (!legacyDirectory_.empty() &&
        std::filesystem::is_regular_file(legacy, error))
        return legacy;
    return saved;
}

ProfileState OriginalProfileStore::load(std::string& warning) const
{
    ProfileState state;
    installOriginalDefaults(state);
    warning.clear();
    try
    {
        loadConfig(
            loadPath("user.xml"), state.config,
            state.preferredCameraSerialized,
            state.discreteVideoCardSerialized);
        loadRaceLibrary(loadPath("race.xml"), state);
        if (!state.player.name.empty())
        {
            loadProfile(
                loadPath(std::filesystem::path("Profile") /
                         (cleanFileName(state.player.name) + ".xml")),
                state.player);
        }
        const auto legacyAchievements =
            legacyDirectory_ / "achievment.xml";
        const auto savedAchievements =
            saveDirectory_ / "achievment.xml";
        std::error_code fileError;
        const bool hasLegacyAchievements =
            !legacyDirectory_.empty() &&
            std::filesystem::is_regular_file(
                legacyAchievements, fileError);
        if (hasLegacyAchievements)
            loadAchievements(legacyAchievements, state);
        fileError.clear();
        const bool hasSavedAchievements =
            std::filesystem::is_regular_file(
                savedAchievements, fileError);
        if (hasSavedAchievements &&
            (!hasLegacyAchievements ||
             savedAchievements != legacyAchievements))
            loadAchievements(savedAchievements, state);
        else if (!hasLegacyAchievements)
            loadAchievements(loadPath("achievment.xml"), state);
    }
    catch (const std::exception& exception)
    {
        warning = exception.what();
    }
    return state;
}

bool OriginalProfileStore::selectProfile(
    ProfileState& state, std::string_view name,
    std::string& error, bool network) const
{
    error.clear();
    const auto profileName = cleanFileName(std::string(name));
    const auto& profiles =
        network ? state.networkProfiles : state.profiles;
    if (std::find(profiles.begin(), profiles.end(), profileName) ==
        profiles.end())
    {
        error = "unknown original profile: " + profileName;
        return false;
    }

    // Profile::LoadGameFile catches EUnableToOpen after Profile::Enter has
    // installed source defaults, so a library entry with a missing XML is
    // still a valid selectable profile.
    auto selected = makeOriginalDefaultProfileState().player;
    selected.name = profileName;
    loadProfile(
        loadPath(
            std::filesystem::path("Profile") /
            (profileName + ".xml")),
        selected);
    state.player = std::move(selected);
    if (network)
        state.lastNetworkProfile = profileName;
    else
        state.lastProfile = profileName;
    return true;
}

bool OriginalProfileStore::deleteProfile(
    ProfileState& state, std::string_view name,
    std::string& error, bool network) const
{
    error.clear();
    const auto profileName = cleanFileName(std::string(name));
    auto& profiles = network ? state.networkProfiles : state.profiles;
    const auto found = std::find(
        profiles.begin(), profiles.end(), profileName);
    if (found == profiles.end())
    {
        error = "unknown original profile: " + profileName;
        return false;
    }

    const bool deletingCurrent =
        state.player.name == profileName;
    profiles.erase(found);
    if (network && state.lastNetworkProfile == profileName)
        state.lastNetworkProfile.clear();
    if (!network && state.lastProfile == profileName)
        state.lastProfile.clear();
    if (deletingCurrent)
    {
        if (profiles.empty())
        {
            state.player = makeOriginalDefaultProfileState().player;
            state.player.name.clear();
        }
        else
        {
            auto selected = makeOriginalDefaultProfileState().player;
            selected.name = profiles.front();
            loadProfile(
                loadPath(
                    std::filesystem::path("Profile") /
                    (cleanFileName(selected.name) + ".xml")),
                selected);
            state.player = std::move(selected);
            if (network)
                state.lastNetworkProfile = state.player.name;
            else
                state.lastProfile = state.player.name;
        }
    }
    return save(state, error);
}

bool OriginalProfileStore::save(const ProfileState& state,
                                std::string& error) const
{
    error.clear();
    TiXmlDocument configDocument;
    configDocument.LinkEndChild(
        new TiXmlDeclaration("1.0", "UTF-8", ""));
    auto* config = new TiXmlElement("root");
    configDocument.LinkEndChild(config);
    auto* quality = new TiXmlElement("quality");
    config->LinkEndChild(quality);
    append(*quality, "filtering", state.config.quality.filtering);
    append(*quality, "msaa", state.config.quality.msaa);
    append(*quality, "shadow", state.config.quality.shadow);
    append(*quality, "environment", state.config.quality.environment);
    append(*quality, "light", state.config.quality.light);
    append(*quality, "postEffect", state.config.quality.postEffect);
    append(*quality, "frameRateMode",
           state.config.quality.frameRateMode);
    append(*config, "resolution",
           std::to_string(state.config.resolutionWidth) + " " +
               std::to_string(state.config.resolutionHeight));
    auto* volume = new TiXmlElement("volume");
    config->LinkEndChild(volume);
    append(*volume, "musicVolume", state.config.musicVolume);
    append(*volume, "effectsVolume", state.config.effectsVolume);
    append(*volume, "voiceVolume", state.config.voiceVolume);
    append(*config, "maxPlayers", state.config.maxPlayers);
    append(*config, "maxComputers", state.config.maxComputers);
    append(*config, "upgradeMaxLevel", state.config.upgradeMaxLevel);
    append(*config, "weaponMaxLevel", state.config.weaponMaxLevel);
    append(*config, "springBorders", state.config.springBorders);
    append(*config, "lapsCount", state.config.lapsCount);
    append(*config, "enableHUD", state.config.enableHud);
    append(*config, "enableMineBug", state.config.enableMineBug);
    append(*config, "disableVideo", state.config.disableVideo);
    append(*config, "fullScreen", state.config.fullScreen);
    append(*config, "language", state.config.language);
    append(*config, "commentatorStyle",
           state.config.commentatorStyle);
    append(*config, "prefCamera",
           state.config.preferredCamera == PreferredCamera::ThirdPerson
               ? "pcThirdPerson"
               : "pcIsometric");
    append(*config, "cameraDistance", state.config.cameraDistance);
    append(*config, "discreteVideoCard",
           state.config.discreteVideoCard);
    auto* controls = new TiXmlElement("controls");
    config->LinkEndChild(controls);
    appendControls(*controls, "ctKeyboard",
                   state.config.keyboardControls);
    appendControls(*controls, "ctGamepad",
                   state.config.gamepadControls);
    auto* menuMusic = new TiXmlElement("menuMusic");
    config->LinkEndChild(menuMusic);
    append(*menuMusic, "playList", state.config.menuMusicPlaylist);
    auto* gameMusic = new TiXmlElement("gameMusic");
    config->LinkEndChild(gameMusic);
    append(*gameMusic, "playList", state.config.gameMusicPlaylist);
    if (!saveAtomic(configDocument, saveDirectory_ / "user.xml", error))
        return false;

    TiXmlDocument raceDocument;
    raceDocument.LinkEndChild(
        new TiXmlDeclaration("1.0", "UTF-8", ""));
    auto* race = new TiXmlElement("raceRoot");
    raceDocument.LinkEndChild(race);
    const std::string profileName =
        state.player.name.empty()
            ? std::string{}
            : cleanFileName(state.player.name);
    const bool hasOfflineProfile =
        !profileName.empty() &&
        std::find(
            state.profiles.begin(), state.profiles.end(),
            profileName) != state.profiles.end();
    const bool hasNetworkProfile =
        !profileName.empty() &&
        std::find(
            state.networkProfiles.begin(), state.networkProfiles.end(),
            profileName) != state.networkProfiles.end();
    const bool hasCurrentProfile =
        hasOfflineProfile || hasNetworkProfile;
    append(*race, "profiles", joinList(state.profiles));
    append(*race, "netProfiles", joinList(state.networkProfiles));
    if (hasOfflineProfile)
        append(*race, "lastProfile", profileName);
    else if (!state.lastProfile.empty())
        append(*race, "lastProfile",
               cleanFileName(state.lastProfile));
    if (hasNetworkProfile)
        append(*race, "lastNetProfile", profileName);
    else if (!state.lastNetworkProfile.empty())
        append(*race, "lastNetProfile",
               cleanFileName(state.lastNetworkProfile));
    std::vector<std::string> completed;
    completed.reserve(state.planetsCompleted.size());
    for (const auto planet : state.planetsCompleted)
        completed.push_back(std::to_string(planet));
    append(*race, "planetsCompleted", joinList(completed));
    append(*race, "tutorialStage", state.tutorialStage);
    if (!saveAtomic(raceDocument, saveDirectory_ / "race.xml", error))
        return false;

    if (hasCurrentProfile)
    {
        TiXmlDocument profileDocument;
        profileDocument.LinkEndChild(
            new TiXmlDeclaration("1.0", "UTF-8", ""));
        auto* profile = new TiXmlElement("profile");
        profileDocument.LinkEndChild(profile);
        append(*profile, "carChanged", state.player.carChanged);
        append(*profile, "minDifficulty",
               state.player.minimumDifficulty);
        for (std::size_t index = 0;
             index < state.player.planets.size(); ++index)
        {
            auto* planet = new TiXmlElement(
                ("planet" + std::to_string(index)).c_str());
            profile->LinkEndChild(planet);
            append(*planet, "state",
                   state.player.planets[index].state);
            append(*planet, "pass",
                   state.player.planets[index].pass);
        }
        append(*profile, "planet", state.player.currentPlanet);
        append(*profile, "track", state.player.currentTrack);
        auto* humans = new TiXmlElement("humans");
        profile->LinkEndChild(humans);
        auto* human = new TiXmlElement("human0");
        humans->LinkEndChild(human);
        auto* car = new TiXmlElement("car");
        car->SetAttribute("lib", "world\\db\\ctCar");
        car->LinkEndChild(new TiXmlText(state.player.currentCar));
        human->LinkEndChild(car);
        append(*human, "plrId", state.player.playerId);
        append(*human, "gamerId", state.player.gamerId);
        append(*human, "netSlot", state.player.networkSlot);
        std::ostringstream color;
        color.precision(8);
        color << state.player.color[0] << ' '
              << state.player.color[1] << ' '
              << state.player.color[2] << ' '
              << state.player.color[3];
        append(*human, "color", color.str());
        append(*human, "money", state.player.money);
        append(*human, "points", state.player.points);
        for (std::size_t index = 0;
             index < state.player.slots.size(); ++index)
        {
            if (state.player.slots[index].record.empty())
                continue;
            appendReference(
                *human,
                ("slot" + std::to_string(index)).c_str(),
                state.player.slots[index]);
        }
        append(*profile, "dfficulty", state.player.difficulty);
        if (!saveAtomic(
                profileDocument,
                saveDirectory_ / "Profile" /
                    (profileName + ".xml"),
                error))
        {
            return false;
        }
    }

    TiXmlDocument achievementDocument;
    achievementDocument.LinkEndChild(
        new TiXmlDeclaration("1.0", "UTF-8", ""));
    auto* achievementRoot = new TiXmlElement("achievmentRoot");
    achievementDocument.LinkEndChild(achievementRoot);
    append(*achievementRoot, "points", state.achievementPoints);
    auto* achievementItems = new TiXmlElement("items");
    achievementRoot->LinkEndChild(achievementItems);
    for (const auto& [name, item] : state.achievementItems)
    {
        auto* output = new TiXmlElement(name.c_str());
        output->SetAttribute("classId",
                             static_cast<int>(item.classId));
        achievementItems->LinkEndChild(output);
        for (const auto& [field, fieldValue] : item.values)
            append(*output, field.c_str(), fieldValue);
        if (!item.records.empty())
        {
            auto* records = new TiXmlElement("records");
            output->LinkEndChild(records);
            for (const auto& record : item.records)
            {
                auto* recordNode =
                    new TiXmlElement(record.name.c_str());
                if (!record.library.empty())
                    recordNode->SetAttribute(
                        "lib", record.library.c_str());
                if (!record.record.empty())
                    recordNode->LinkEndChild(
                        new TiXmlText(record.record));
                records->LinkEndChild(recordNode);
            }
        }
    }
    auto* achievementConditions = new TiXmlElement("conditions");
    achievementRoot->LinkEndChild(achievementConditions);
    auto conditions = state.achievementConditions;
    for (const auto& [name, iteration] : state.achievementIterations)
        conditions[name].values["iterNum"] =
            std::to_string(iteration);
    for (const auto& [name, conditionProfile] : conditions)
    {
        auto* condition = new TiXmlElement(name.c_str());
        condition->SetAttribute(
            "classId",
            static_cast<int>(conditionProfile.classId));
        achievementConditions->LinkEndChild(condition);
        for (const auto& [field, fieldValue] :
             conditionProfile.values)
            append(*condition, field.c_str(), fieldValue);
    }
    return saveAtomic(
        achievementDocument, saveDirectory_ / "achievment.xml", error);
}

const std::filesystem::path&
OriginalProfileStore::saveDirectory() const noexcept
{
    return saveDirectory_;
}

} // namespace r3d::game::originalrace
