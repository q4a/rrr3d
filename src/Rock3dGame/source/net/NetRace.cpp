#include "stdafx.h"

#include "net\NetRace.h"
#include "game\World.h"

namespace r3d
{

namespace game
{

NetRace::NetRace(const Desc& desc): NetModelRPC(desc), _net(NetGame::I())
{
	RegRPC(&NetRace::OnStartMatch);
	RegRPC(&NetRace::OnExitMatch);
	RegRPC(&NetRace::OnSetPlanet);
	RegRPC(&NetRace::OnSetTrack);	
	RegRPC(&NetRace::OnStartRace);
	RegRPC(&NetRace::OnExitRace);
	RegRPC(&NetRace::OnRaceGo);
	RegRPC(&NetRace::OnPause);
	RegRPC(&NetRace::OnDamage1);
	RegRPC(&NetRace::OnDamage2);
	RegRPC(&NetRace::OnSetUpgradeMaxLevel);	
	RegRPC(&NetRace::OnSetWeaponMaxLevel);
	RegRPC(&NetRace::OnSetCurrentDifficulty);
	RegRPC(&NetRace::OnSetLapsCount);
	RegRPC(&NetRace::OnSetMaxPlayers);
	RegRPC(&NetRace::OnSetMaxComputers);
	RegRPC(&NetRace::OnSetSpringBorders);
	RegRPC(&NetRace::OnSetEnableMineBug);
	RegRPC(&NetRace::OnPushLine);

	DescData data;
	data.Read(desc.stream);

	_net->RegRace(this);
	_net->RegUser(this);
	_net->game()->RegUser(this);
	_maxPlayers = _net->game()->maxPlayers();
	_maxComputers = _net->game()->maxComputers();	
}

NetRace::~NetRace()
{
	_net->game()->UnregUser(this);
	_net->UnregUser(this);
	_net->UnregRace(this);
}

void NetRace::ReadMatch(std::istream& stream)
{
	int mode;
	int upgradeMaxLevel;
	int weaponMaxLevel;
	unsigned lapsCount;
	int planet;
	int track;
	int wheater;
	bool springBorders;
	bool enableMineBug;

	net::Read(stream, mode);
	net::Read(stream, upgradeMaxLevel);
	net::Read(stream, weaponMaxLevel);
	net::Read(stream, lapsCount);
	net::Read(stream, _maxPlayers);
	net::Read(stream, _maxComputers);
	net::Read(stream, springBorders);
	net::Read(stream, enableMineBug);
	net::Read(stream, planet);
	net::Read(stream, track);
	net::Read(stream, wheater);

	DoStartMatch((Race::Mode)mode, cDifficultyEnd, NULL);

	Race::Profile* profile = race()->GetProfile();
	profile->LoadGame(stream);

	garage().SetUpgradeMaxLevel(upgradeMaxLevel);
	garage().SetWeaponMaxLevel(weaponMaxLevel);
	race()->SetSpringBorders(springBorders);
	race()->SetEnableMineBug(enableMineBug);
	tournament().SetLapsCount(lapsCount);
	game()->ChangePlanet(race()->GetTournament().GetPlanet(planet));
	race()->GetTournament().SetCurTrack(race()->GetTournament().GetCurPlanet().GetTracks()[track]);
	race()->GetTournament().SetWheater((Environment::Wheater)wheater);
}

void NetRace::WriteMatch(std::ostream& stream)
{
	net::Write(stream, race()->GetMode());
	net::Write(stream, garage().GetUpgradeMaxLevel());
	net::Write(stream, garage().GetWeaponMaxLevel());
	net::Write(stream, tournament().GetLapsCount());
	net::Write(stream, _maxPlayers);
	net::Write(stream, _maxComputers);
	net::Write(stream, race()->GetSpringBorders());
	net::Write(stream, race()->GetEnableMineBug());
	net::Write(stream, race()->GetTournament().GetCurPlanetIndex());
	net::Write(stream, race()->GetTournament().GetCurTrackIndex());
	net::Write(stream, (int)race()->GetTournament().GetWheater());

	race()->GetProfile()->SaveGame(stream);	
}

void NetRace::DoStartMatch(Race::Mode mode, Difficulty difficulty, Race::Profile* profile)
{
	game()->StartMatch(mode, difficulty, profile, false, true, _net->isClient());
}

void NetRace::MakeHuman()
{
	//if match started
	MakePlayer(Race::cHuman, netSlot());
}

void NetRace::MakePlayer(unsigned id, unsigned netSlot)
{
	std::ostream& stream = _net->netPlayer()->NewModel<NetPlayer>();
	NetPlayer::DescData data(id, netSlot);
	data.Write(stream);
	_net->netPlayer()->CloseCmd();
}

void NetRace::DoExitMatch()
{
	NetGame::NetPlayers players = _net->players();
	for (NetGame::NetPlayers::const_iterator iter = players.begin(); iter != players.end(); ++iter)
		this->player()->DeleteModel(*iter, true);
	
	game()->ExitMatch(_net->isHost());
}

void NetRace::DoStartRace()
{
	Race::PlayerList leaverList = GetLeaverList();

	for (Race::PlayerList::const_iterator iter = leaverList.begin(); iter != leaverList.end(); ++iter)
		race()->DelPlayer(*iter);

	for (NetGame::NetPlayers::const_iterator iter = _net->netPlayers().begin(); iter != _net->netPlayers().end(); ++iter)
		(*iter)->RaceStarted();

	game()->StartRace();
}

void NetRace::CheckGoWait()
{
	if (_net->isHost() && race()->IsStartRace() && !race()->IsRaceGo())
	{
		for (NetGame::NetPlayers::const_iterator iter = _net->netOpponents().begin(); iter != _net->netOpponents().end(); ++iter)
			if (!(*iter)->IsRaceGoWait())
				return;
		game()->GoRaceTimer();
	}
}

void NetRace::CheckFinish()
{
	if (_net->isHost() && race()->IsStartRace() && race()->IsRaceGo())
	{
		for (NetGame::NetPlayers::const_iterator iter = _net->netPlayers().begin(); iter != _net->netPlayers().end(); ++iter)
			if (!(*iter)->IsRaceFinish())
				return;
		game()->RunFinishTimer();
	}
}

void NetRace::OnStartMatch(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	ReadMatch(stream);

	MakeHuman();
}

void NetRace::OnExitMatch(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	if (_net->isHost())
		msg.Discard();

	DoExitMatch();
}

void NetRace::OnSetPlanet(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	int planet;
	int track;
	int wheater;

	net::Read(stream, planet);
	net::Read(stream, track);
	net::Read(stream, wheater);

	game()->ChangePlanet(race()->GetTournament().GetPlanet(planet));
	race()->GetTournament().SetCurTrack(race()->GetTournament().GetCurPlanet().GetTracks()[track]);
	race()->GetTournament().SetWheater((Environment::Wheater)wheater);
}

void NetRace::OnSetTrack(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	int track;
	net::Read(stream, track);
	race()->GetTournament().SetCurTrack(race()->GetTournament().GetCurPlanet().GetTracks()[track]);
}

void NetRace::OnStartRace(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	DoStartRace();	
}

void NetRace::OnExitRace(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	int trackIndex;
	int wheater;
	unsigned count;

	Race::Results results;

	net::Read(stream, trackIndex);
	net::Read(stream, wheater);
	net::Read(stream, count);	

	for (unsigned i = 0; i < count; ++i)
	{
		unsigned playerId;
		int playerPoints;
		int playerMoney;
		Race::Result result;

		net::Read(stream, playerId);
		net::Read(stream, playerPoints);
		net::Read(stream, playerMoney);

		net::Read(stream, result.money);
		net::Read(stream, result.pickMoney);
		net::Read(stream, result.place);
		net::Read(stream, result.points);
		net::Read(stream, result.voiceNameDur);

		NetPlayer* netPlayer = _net->GetPlayer(playerId);
		if (netPlayer == NULL)
		{
			LSL_LOG("NetRace::OnExitRace, netPlayer = NULL");
			continue;
		}

		result.playerId = netPlayer->model()->GetId();

		results.push_back(result);

		//skip, needed after game()->ExitRace
		//player->SetPoints(playerPoints);
		//client doest not sync his money with server side
		//player->SetMoney(playerMoney);
	}

	game()->ExitRace(_net->isHost(), &results);

	race()->GetTournament().SetCurTrack(race()->GetTournament().GetCurPlanet().GetTracks()[trackIndex]);
	race()->GetTournament().SetWheater((Environment::Wheater)wheater);

	game()->ExitRaceGoFinish();
}

void NetRace::OnRaceGo(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	int stage;
	net::Read(stream, stage);
	game()->GoRace(stage);
}

void NetRace::OnPause(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	bool pause;
	net::Read(stream, pause);
	game()->Pause(pause);
}

void NetRace::OnDamage1(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	unsigned sender;
	unsigned target;
	float value;
	int damageType;
	float targetLife;
	BYTE death;

	net::Read(stream, sender);
	net::Read(stream, target);
	net::Read(stream, value);
	net::Read(stream, damageType);
	net::Read(stream, targetLife);
	net::Read(stream, death);

	NetPlayer* targetPlayer = _net->GetPlayer(target);
	NetPlayer* senderPlayer = _net->GetPlayer(sender);

	if (targetPlayer == NULL || targetPlayer->model()->GetCar().mapObj == NULL)
	{
		LSL_LOG(lsl::StrFmt("NetRace::OnDamage target=NULL target=%d sender=%d value=%0.2f dmgType=%d", target, sender, value, damageType));
		return;
	}

	if (_net->isHost())
	{
		msg.Discard();
		Damage1(senderPlayer, targetPlayer, value, (GameObject::DamageType)damageType, net::cNetTargetOthers);
	}
	else
		targetPlayer->model()->GetCar().gameObj->Damage(senderPlayer ? senderPlayer->model()->GetId() : cUndefPlayerId, value, targetLife, death != 0 ? true : false, (GameObject::DamageType)damageType);
}

void NetRace::OnDamage2(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	unsigned sender;
	unsigned target;
	float value;
	int damageType;
	float targetLife;
	BYTE death;

	net::Read(stream, sender);
	net::Read(stream, target);
	net::Read(stream, value);
	net::Read(stream, damageType);
	net::Read(stream, targetLife);
	net::Read(stream, death);

	MapObj* targetMapObj = map()->GetMapObj(target);
	NetPlayer* senderPlayer = _net->GetPlayer(sender);

	if (targetMapObj == NULL)
	{
		LSL_LOG(lsl::StrFmt("NetRace::OnDamage target=NULL target=%d sender=%d value=%0.2f dmgType=%d", target, sender, value, damageType));
		return;
	}

	if (_net->isHost())
	{
		msg.Discard();
		Damage2(senderPlayer, targetMapObj, value, (GameObject::DamageType)damageType, net::cNetTargetOthers);
	}
	else
		targetMapObj->GetGameObj().Damage(senderPlayer ? senderPlayer->model()->GetId() : cUndefPlayerId, value, targetLife, death != 0 ? true : false, (GameObject::DamageType)damageType);
}

void NetRace::OnSetUpgradeMaxLevel(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	int level;

	net::Read(stream, level);

	garage().SetUpgradeMaxLevel(level);
}

void NetRace::OnSetWeaponMaxLevel(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	int level;

	net::Read(stream, level);

	garage().SetWeaponMaxLevel(level);
}

void NetRace::OnSetCurrentDifficulty(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	int level;

	net::Read(stream, level);

	_net->game()->currentDiff((Difficulty)level);
}

void NetRace::OnSetLapsCount(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	unsigned lapsCount;

	net::Read(stream, lapsCount);

	tournament().SetLapsCount(lapsCount);
}

void NetRace::OnSetMaxPlayers(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	net::Read(stream, _maxPlayers);
}

void NetRace::OnSetMaxComputers(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	net::Read(stream, _maxComputers);
}

void NetRace::OnSetSpringBorders(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	bool springBorders;

	net::Read(stream, springBorders);

	race()->SetSpringBorders(springBorders);
}

void NetRace::OnSetEnableMineBug(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	bool enableMineBug;

	net::Read(stream, enableMineBug);

	race()->SetEnableMineBug(enableMineBug);
}

void NetRace::OnPushLine(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	lsl::stringW line;
	net::Read(stream, line);

	_net->SendEvent(cNetRacePushLine, &MyEventData(id(), false, msg.sender, line));
}

void NetRace::Damage1(NetPlayer* sender, NetPlayer* target, float value, GameObject::DamageType damageType, unsigned netTarget)
{
	LSL_ASSERT(target && target->model()->GetCar().gameObj);

	GameObject* gameObj = target->model()->GetCar().gameObj;
	float targetLife = 0;
	BYTE death = 0;

	if (netTarget != net::cNetTargetAll)
	{		
		gameObj->Damage(sender ? sender->model()->GetId() : cUndefPlayerId, value, damageType);	
		targetLife = gameObj->GetLife();
		death = gameObj->GetLiveState() == GameObject::lsDeath;
	}

	std::ostream& stream = NewRPC(netTarget, &NetRace::OnDamage1);
	net::Write(stream, sender ? sender->id() : net::cUndefPlayer);
	net::Write(stream, target ? target->id() : net::cUndefPlayer);
	net::Write(stream, value);
	net::Write(stream, (int)damageType);
	net::Write(stream, targetLife);
	net::Write(stream, death);
	CloseRPC();
}

void NetRace::Damage2(NetPlayer* sender, MapObj* target, float value, GameObject::DamageType damageType, unsigned netTarget)
{
	LSL_ASSERT(target);

	GameObject& gameObj = target->GetGameObj();
	float targetLife = 0;
	BYTE death = 0;

	if (netTarget != net::cNetTargetAll)
	{		
		gameObj.Damage(sender ? sender->model()->GetId() : cUndefPlayerId, value, damageType);
		targetLife = gameObj.GetLife();
		death = gameObj.GetLiveState() == GameObject::lsDeath;
	}

	std::ostream& stream = NewRPC(netTarget, &NetRace::OnDamage2);
	net::Write(stream, sender ? sender->id() : net::cUndefPlayer);
	net::Write(stream, target ? target->GetId() : Map::cDefMapObjId);
	net::Write(stream, value);
	net::Write(stream, (int)damageType);
	net::Write(stream, targetLife);
	net::Write(stream, death);
	CloseRPC();
}

unsigned NetRace::netSlot()
{
	return player()->netIndex() + 1;
}

Garage& NetRace::garage()
{
	return _net->game()->GetRace()->GetGarage();
}

const Garage& NetRace::garage() const
{
	return _net->game()->GetRace()->GetGarage();
}

Tournament& NetRace::tournament()
{
	return _net->game()->GetRace()->GetTournament();
}

const Tournament& NetRace::tournament() const
{
	return _net->game()->GetRace()->GetTournament();
}

bool NetRace::OnConnected(net::INetConnection* sender)
{
	if (game()->GetRace()->IsStartRace())
		return false;

	unsigned maxPlayers = std::min(_net->game()->maxPlayers(), _net->game()->GetRace()->IsCampaign() ? (unsigned)Race::cCampaignMaxHumans : (unsigned)Race::cMaxPlayers);

	return _net->netPlayers().size() < maxPlayers;
}

void NetRace::OnDisconnectedPlayer(NetPlayer* sender)
{
	CheckGoWait();
	CheckFinish();
}

void NetRace::OnProcessNetEvent(unsigned id, NetEventData* data)
{
	switch (id)
	{
		case cNetPlayerGoWait:
		{
			CheckGoWait();
			return;
		}

		case cNetPlayerFinish:
		{
			CheckFinish();			
			return;
		}
	}
}

void NetRace::OnProcessEvent(unsigned id, EventData* data)
{
	if (id == cRaceStartWait)
	{
		_net->player()->RaceGoWait(true);
		return;
	}

	if (_net->isHost())
	{
		int stage = -1;
		if (id == cRaceStartTime1)
			stage = GameMode::cGoRace1;
		else if (id == cRaceStartTime2)
			stage = GameMode::cGoRace2;
		else if (id == cRaceStartTime3)
			stage = GameMode::cGoRace3;
		else if (id == cRaceStart)
			stage = GameMode::cGoRace;

		if (stage != -1)
		{
			std::ostream& stream = NewRPC(net::cNetTargetOthers, &NetRace::OnRaceGo);
			net::Write(stream, stage);
			CloseRPC();
			return;
		}
	}
}

void NetRace::OnDescWrite(const net::NetMessage& msg, std::ostream& stream)
{
	DescData().Write(stream);
}

void NetRace::OnStateRead(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	ReadMatch(stream);
	MakeHuman();
}

void NetRace::OnStateWrite(const net::NetMessage& msg, std::ostream& stream)
{
	WriteMatch(stream);
}

void NetRace::Process(float deltaTime)
{	
}

void NetRace::StartMatch(Race::Mode mode, Difficulty difficulty, Race::Profile* profile)
{
	DoStartMatch(mode, difficulty, profile);

	std::ostream& stream = NewRPC(net::cNetTargetOthers, &NetRace::OnStartMatch);
	WriteMatch(stream);
	CloseRPC();

	MakeHuman();
}

void NetRace::ExitMatch()
{
	DoExitMatch();

	MakeRPC(net::cNetTargetOthers, &NetRace::OnExitMatch);
}

Planet* NetRace::GetPlanet()
{
	return &race()->GetTournament().GetCurPlanet();
}

void NetRace::ChangePlanet(Planet* value)
{
	if (game()->ChangePlanet(value))
	{
		std::ostream& stream = NewRPC(net::cNetTargetOthers, &NetRace::OnSetPlanet);
		net::Write(stream, race()->GetTournament().GetCurPlanetIndex());
		net::Write(stream, race()->GetTournament().GetCurTrackIndex());
		net::Write(stream, (int)race()->GetTournament().GetWheater());
		CloseRPC();
	}
}

Planet::Track* NetRace::GetTrack()
{
	return &race()->GetTournament().GetCurTrack();
}

void NetRace::SetTrack(Planet::Track* value)
{
	race()->GetTournament().SetCurTrack(value);

	std::ostream& stream = NewRPC(net::cNetTargetOthers, &NetRace::OnSetTrack);
	net::Write(stream, race()->GetTournament().GetCurTrackIndex());
	CloseRPC();
}

void NetRace::StartRace()
{
	int humanCount = _net->netPlayers().size();
	int maxComputers = race()->IsCampaign() ? lsl::ClampValue((int)_net->game()->maxComputers(), Race::cComputerCount - 1, Race::cCampaignMaxPlayers - 1) : _net->game()->maxComputers();
	int maxPlayers = race()->IsCampaign() ? lsl::ClampValue((int)_net->game()->maxPlayers(), humanCount + maxComputers, maxComputers + Race::cCampaignMaxHumans) : _net->game()->maxPlayers();
	
	int computerCount = std::min(maxComputers, std::max(maxPlayers - humanCount, 0));
	int curComputerCount = _net->aiPlayers().size();

	for (int i = curComputerCount; i < computerCount; ++i)
	{
		MakePlayer(Race::cComputer1 + i, Race::cDefaultNetSlot);
	}

	for (int i = computerCount; i < curComputerCount; ++i)
	{
		if (_net->aiPlayers().size() > 0)
			player()->DeleteModel(_net->aiPlayers().back(), false);
	}

	MakeRPC(net::cNetTargetOthers, &NetRace::OnStartRace);	

	DoStartRace();
}

void NetRace::ExitRace()
{
	game()->ExitRace(_net->isHost());
	game()->ExitRaceGoFinish();

	std::ostream& stream = NewRPC(net::cNetTargetOthers, &NetRace::OnExitRace);

	unsigned count = _net->players().size();

	net::Write(stream, race()->GetTournament().GetCurTrackIndex());
	net::Write(stream, (int)race()->GetTournament().GetWheater());
	net::Write(stream, count);

	for (NetGame::NetPlayers::const_iterator iter = _net->players().begin(); iter != _net->players().end(); ++iter)
	{
		NetPlayer* netPlayer = *iter;
		Player* player = netPlayer->model();

		net::Write(stream, netPlayer->id());
		net::Write(stream, player->GetPoints());
		net::Write(stream, player->GetMoney());

		Race::Result result;
		const Race::Result* res = _net->game()->GetRace()->GetResult(player->GetId());
		if (res)
			result = *res;

		net::Write(stream, result.money);
		net::Write(stream, result.pickMoney);
		net::Write(stream, result.place);
		net::Write(stream, result.points);
		net::Write(stream, result.voiceNameDur);
	}
	
	CloseRPC();
}

void NetRace::Pause(bool pause)
{
	//game()->Pause(pause);

	//std::ostream& stream = NewRPC(net::cNetTargetOthers, &NetRace::OnPause);
	//net::Write(stream, pause);
	//CloseRPC();
}

void NetRace::Damage(int senderPlayerLocalId, MapObj* target, float value, GameObject::DamageType damageType)
{
	unsigned netTarget = 0;

	if (_net->isClient())
	{
		netTarget = net::cNetTargetAll;

		if (_net->player()->model()->GetId() == senderPlayerLocalId)
		{
			//continue
		}
		else
		{
			return;
		}
	}
	else
	{
		netTarget = net::cNetTargetOthers;

		if (Race::IsOpponentId(senderPlayerLocalId))
		{
			return;
		}
		else
		{
			//continue
		}
	}

	NetPlayer* senderPlayer = _net->GetPlayerByLocalId(senderPlayerLocalId);
	NetPlayer* targetPlayer = _net->GetPlayer(target);

	if (targetPlayer)
		Damage1(senderPlayer, targetPlayer, value, damageType, netTarget);
	else
		Damage2(senderPlayer, target, value, damageType, netTarget);
}

int NetRace::GetUpgradeMaxLevel() const
{
	return garage().GetUpgradeMaxLevel();
}

void NetRace::SetUpgradeMaxLevel(int value)
{
	if (_net->isHost() && _net->game()->upgradeMaxLevel() != value)
	{
		_net->game()->upgradeMaxLevel(value);

		std::ostream& stream = NewRPC(net::cNetTargetOthers, &NetRace::OnSetUpgradeMaxLevel);
		net::Write(stream, value);
		CloseRPC();
	}
}

int NetRace::GetWeaponMaxLevel() const
{
	return garage().GetWeaponMaxLevel();
}

void NetRace::SetWeaponMaxLevel(int value)
{
	if (_net->isHost() && _net->game()->weaponMaxLevel() != value)
	{
		_net->game()->weaponMaxLevel(value);
			
		std::ostream& stream = NewRPC(net::cNetTargetOthers, &NetRace::OnSetWeaponMaxLevel);
		net::Write(stream, value);
		CloseRPC();
	}
}

Difficulty NetRace::GetCurrentDifficulty() const
{
	return _net->game()->currentDiff();
}

void NetRace::SetCurrentDifficulty(Difficulty value)
{
	if (_net->isHost() && _net->game()->currentDiff() != value)
	{
		_net->game()->currentDiff(value);

		std::ostream& stream = NewRPC(net::cNetTargetOthers, &NetRace::OnSetCurrentDifficulty);
		net::Write(stream, (int)value);
		CloseRPC();
	}
}

unsigned NetRace::GetLapsCount() const
{
	return tournament().GetLapsCount();
}

void NetRace::SetLapsCount(unsigned value)
{
	if (_net->isHost() && _net->game()->lapsCount() != value)
	{
		_net->game()->lapsCount(value);
			
		std::ostream& stream = NewRPC(net::cNetTargetOthers, &NetRace::OnSetLapsCount);
		net::Write(stream, value);
		CloseRPC();
	}
}

unsigned NetRace::GetMaxPlayers() const
{
	return _maxPlayers;
}

void NetRace::SetMaxPlayers(unsigned value)
{
	if (_net->isHost() && _net->game()->maxPlayers() != value)
	{
		_maxPlayers = value;
		_net->game()->maxPlayers(value);

		std::ostream& stream = NewRPC(net::cNetTargetOthers, &NetRace::OnSetMaxPlayers);
		net::Write(stream, value);
		CloseRPC();
	}
}

unsigned NetRace::GetMaxComputers() const
{
	return _maxComputers;
}

void NetRace::SetMaxComputers(unsigned value)
{
	if (_net->isHost() && _net->game()->maxComputers() != value)
	{
		_maxComputers = value;
		_net->game()->maxComputers(value);

		std::ostream& stream = NewRPC(net::cNetTargetOthers, &NetRace::OnSetMaxComputers);
		net::Write(stream, value);
		CloseRPC();
	}
}

bool NetRace::GetSpringBorders() const
{
	return race()->GetSpringBorders();
}

void NetRace::SetSpringBorders(bool value)
{
	if (_net->isHost() && _net->game()->springBorders() != value)
	{
		_net->game()->springBorders(value);

		std::ostream& stream = NewRPC(net::cNetTargetOthers, &NetRace::OnSetSpringBorders);
		net::Write(stream, value);
		CloseRPC();
	}
}

bool NetRace::GetEnableMineBug() const
{
	return race()->GetEnableMineBug();
}

void NetRace::SetEnableMineBug(bool value)
{
	if (_net->isHost() && _net->game()->enableMineBug() != value)
	{
		_net->game()->enableMineBug(value);

		std::ostream& stream = NewRPC(net::cNetTargetOthers, &NetRace::OnSetEnableMineBug);
		net::Write(stream, value);
		CloseRPC();
	}
}

void NetRace::PushLine(const lsl::stringW& text)
{
	std::ostream& stream = NewRPC(net::cNetTargetOthers, &NetRace::OnPushLine);
	net::Write(stream, text);
	CloseRPC();
}

Race::PlayerList NetRace::GetLeaverList() const
{
	Race::PlayerList playerList;

	for (Race::PlayerList::const_iterator iter = race()->GetPlayerList().begin(); iter != race()->GetPlayerList().end(); ++iter)
	{
		if (_net->GetPlayer(*iter) == NULL)
			playerList.push_back(*iter);
	}

	return playerList;
}

GameMode* NetRace::game()
{
	return _net->game();
}

const Race* NetRace::race() const
{
	return _net->game()->GetRace();
}

Race* NetRace::race()
{
	return game()->GetRace();
}

Map* NetRace::map()
{
	return race()->GetMap();
}

}

}