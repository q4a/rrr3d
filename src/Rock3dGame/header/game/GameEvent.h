#pragma once

namespace r3d
{

namespace game
{

static const int cUndefPlayerId = 0xFF000000;

struct EventData
{
	int playerId;
	bool success;

	EventData(): playerId(cUndefPlayerId), success(false) {}
	EventData(int mPlayerId): playerId(mPlayerId), success(false) {}
	EventData(bool mSuccess): playerId(cUndefPlayerId), success(mSuccess) {}
};

class IGameEvent: public virtual ObjReference
{
private:
	bool _removed;
public:
	IGameEvent(): _removed(false) {}

	virtual ~IGameEvent() {}

	void Remove()
	{
		_removed = true;
	}

	bool removed() const
	{
		return _removed;
	}
};

class IFixedStepEvent: public virtual IGameEvent
{
public:
	virtual void OnFixedStep(float deltaTime) = 0;
};

class IProgressEvent: public virtual IGameEvent
{
public:
	virtual void OnProgress(float deltaTime) = 0;
};

class ILateProgressEvent: public virtual IGameEvent
{
public:
	virtual void OnLateProgress(float deltaTime, bool pxStep) = 0;
};

class IFrameEvent: public virtual IGameEvent
{
public:
	virtual void OnFrame(float deltaTime, float pxAlpha) = 0;
};

class IGameUser: public virtual IGameEvent
{
public:
	virtual void OnProcessEvent(unsigned id, EventData* data) {}
};

enum GameEvent {
	//race
	cRaceStartWait = 0,
	cRaceStart,
	cRaceStartTime1,
	cRaceStartTime2,
	cRaceStartTime3,
	cRaceFinish,
	cRaceLastLap,
	cRacePassLap,
	cRaceFinishTimeEnd,

	//player
	cPlayerOverboard,
	cPlayerDeathMine,
	cPlayerMoveInverse,
	cPlayerLostControl,
	cPlayerLeadFinish,
	cPlayerSecondFinish,
	cPlayerThirdFinish,
	cPlayerLastFinish,
	cPlayerLeadChanged,
	cPlayerThirdChanged,
	cPlayerThirdFar,
	cPlayerLastFar,
	cPlayerDomination,
	cPlayerLowLife,
	cPlayerDeath,
	cPlayerFinishFirst,
	cPlayerFinishSecond,
	cPlayerFinishThird,
	cPlayerFinishLast,
	cPlayerPickItem,
	cPlayerDamage,
	cPlayerKill,
	cPlayerDispose,
	cPlayerSpeedArrow,

	cHumanShot,

	cSteamGetUserStats,
	cSteamStoreUserStats,
	cSteamGetLeaders,
	cSteamStoreLeaders,
	cSteamGetLobbyList,
	cSteamUpdateLobby,
	cSteamEnteredLobby,
	cSteamCreateLobby,
	cSteamUpdateLobbyMember,
	cSteamUpdateLobbyMembers,
	cSteamLobbyGameCreated,
	cSteamHostCreated,
	cSteamHostDisconnected,	
	cSteamHostList,
	cSteamAuthEnded,

	//
	cAchievmentConditionComplete,
	cUpgradeMaxLevelChanged,
	cWeaponMaxLevelChanged,
	cVideoStopped,

	cRaceMenuInvalidate,

	cEventEnd
};

static const std::string cEventNameMap[cEventEnd] = {
	//race
	"raceStartWait",
	"raceStart",
	"raceStartTime1",
	"raceStartTime2",
	"raceStartTime3",
	"raceFinish",
	"raceLastLap",
	"racePassLap",
	"raceFinishTimeEnd",

	//player
	"playerOverboard",
	"playerDeathMine",	
	"playerMoveInverse",
	"playerLostControl",
	"playerLeadFinish",
	"playerSecondFinish",
	"playerThirdFinish",
	"playerLastFinish",
	"playerLeadChanged",
	"playerThirdChanged",
	"playerThirdFar",
	"playerLastFar",
	"playerDomination",
	"playerLowLife",
	"playerDeath",
	"playerFinishFirst",
	"playerFinishSecond",
	"playerFinishThird",
	"playerFinishLast",
	"playerPickItem",
	"playerDamage",
	"playerKill",
	"playerDispose",
	"playerSpeedArrow",

	"humanShot",

	"steamGetUserStats",
	"steamStoreUserStats",
	"steamGetLeaders",
	"steamStoreLeaders",
	"steamGetLobbyList",
	"steamUpdateLobby",
	"steamEnteredLobby",
	"steamCreateLobby",
	"steamUpdateLobbyMember",
	"steamUpdateLobbyMembers",
	"steamLobbyGameCreated",
	"steamHostCreated",
	"steamHostDisconnected",	
	"steamHostList",
	"steamAuthEnded"

	//
	"achievmentConditionComplete",
	"upgradeMaxLevelChanged",
	"weaponMaxLevelChanged",
	"videoStopped",

	"raceMenuInvalidate"
};

}

}