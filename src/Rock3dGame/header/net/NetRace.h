#pragma once

#include "game\Race.h"
#include "NetBase.h"

namespace r3d
{

namespace game
{

class GameMode;
class NetGame;

class NetRace: public net::NetModelRPC<NetRace>, INetGameUser, IGameUser
{
public:
	struct DescData
	{
		DescData() {}

		void Read(std::istream& stream)
		{
		}

		void Write(std::ostream& stream)
		{
		}
	};

	struct MyEventData: NetEventData
	{
		unsigned senderPlayer;
		lsl::stringW text;

		MyEventData(unsigned mSender, bool mFailed, unsigned mSenderPlayer, const lsl::stringW& mText): NetEventData(mSender, mFailed), senderPlayer(mSenderPlayer), text(mText) {}
	};
private:
	NetGame* _net;
	unsigned _maxPlayers;
	unsigned _maxComputers;	

	void ReadMatch(std::istream& stream);
	void WriteMatch(std::ostream& stream);
	void DoStartMatch(Race::Mode mode, Difficulty difficulty, Race::Profile* profile);
	void MakeHuman();

	void MakePlayer(unsigned id, unsigned netSlot);
	void DoExitMatch();
	void DoStartRace();
	void CheckGoWait();
	void CheckFinish();

	void OnStartMatch(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);	
	void OnExitMatch(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	void OnSetPlanet(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	void OnSetTrack(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	void OnStartRace(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	void OnExitRace(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	void OnRaceGo(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	void OnPause(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	void OnDamage1(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	void OnDamage2(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);	
	void OnSetUpgradeMaxLevel(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);	
	void OnSetWeaponMaxLevel(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	void OnSetCurrentDifficulty(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);	
	void OnSetLapsCount(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);	
	void OnSetMaxPlayers(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	void OnSetMaxComputers(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	void OnSetSpringBorders(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	void OnSetEnableMineBug(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	void OnPushLine(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);

	void Damage1(NetPlayer* sender, NetPlayer* target, float value, GameObject::DamageType damageType, unsigned netTarget);
	void Damage2(NetPlayer* sender, MapObj* target, float value, GameObject::DamageType damageType, unsigned netTarget);

	unsigned netSlot();

	Garage& garage();
	const Garage& garage() const;

	Tournament& tournament();
	const Tournament& tournament() const;
protected:
	virtual bool OnConnected(net::INetConnection* sender);	
	virtual void OnDisconnectedPlayer(NetPlayer* sender);
	virtual void OnProcessNetEvent(unsigned id, NetEventData* data);
	virtual void OnProcessEvent(unsigned id, EventData* data);

	virtual void OnDescWrite(const net::NetMessage& msg, std::ostream& stream);
	virtual void OnStateRead(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	virtual void OnStateWrite(const net::NetMessage& msg, std::ostream& stream);
public:
	NetRace(const Desc& desc);
	virtual ~NetRace();

	void Process(float deltaTime);
	void ProcessEvent(const string& name, EventData* data);

	void StartMatch(Race::Mode mode, Difficulty difficulty, Race::Profile* profile);
	void ExitMatch();

	Planet* GetPlanet();
	void ChangePlanet(Planet* value);

	Planet::Track* GetTrack();
	void SetTrack(Planet::Track* value);

	void StartRace();
	void ExitRace();
	void Pause(bool pause);

	void Damage(int senderPlayerLocalId, MapObj* target, float value, GameObject::DamageType damageType);	

	int GetUpgradeMaxLevel() const;
	void SetUpgradeMaxLevel(int value);

	int GetWeaponMaxLevel() const;
	void SetWeaponMaxLevel(int value);

	Difficulty GetCurrentDifficulty() const;
	void SetCurrentDifficulty(Difficulty value);

	unsigned GetLapsCount() const;
	void SetLapsCount(unsigned value);

	unsigned GetMaxPlayers() const;
	void SetMaxPlayers(unsigned value);

	unsigned GetMaxComputers() const;
	void SetMaxComputers(unsigned value);

	bool GetSpringBorders() const;
	void SetSpringBorders(bool value);

	bool GetEnableMineBug() const;
	void SetEnableMineBug(bool value);

	void PushLine(const lsl::stringW& text);

	Race::PlayerList GetLeaverList() const;

	GameMode* game();		

	const Race* race() const;
	Race* race();

	Map* map();
};

}

}