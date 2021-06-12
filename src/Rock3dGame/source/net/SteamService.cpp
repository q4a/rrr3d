#include "stdafx.h"
#include "net\SteamService.h"

#ifdef STEAM_SERVICE

#include "game\GameMode.h"

namespace r3d
{

namespace game
{

const lsl::string SteamStats::cStatName[SteamStats::cStatTypeEnd] = {"stPoints", "stMoney", "stNumGames", "stNumWins", "stNumKills", "stNumDeaths"};
const lsl::string SteamStats::cAchievmentName[cAchievmentTypeEnd] = {"atWinner", "atWins10", "atWins25", "atWins50", "atWins100", "atKills1", "atKills10", "atKills25", "atKills50", "atKills100", "atLoveAndPeace", "atUndead", "atWellWhoDoesNotHappen", "atSeeYouSoon", "atWhereNoManHasGoneBefore", "atNiceStart", "atSpeedKill", "atWeAreTheChampions", "atOldSchool", "atEasyChampion", "atNormalChampion", "atHardChampion"};
SteamStats::StatType SteamStats::cAchievmentStat[cAchievmentTypeEnd] = {SteamStats::stNumWins, SteamStats::stNumWins, SteamStats::stNumWins, SteamStats::stNumWins, SteamStats::stNumWins, SteamStats::stNumKills, SteamStats::stNumKills, SteamStats::stNumKills, SteamStats::stNumKills, SteamStats::stNumKills, SteamStats::cStatTypeEnd, SteamStats::stNumDeaths, SteamStats::cStatTypeEnd, SteamStats::cStatTypeEnd, SteamStats::cStatTypeEnd, SteamStats::cStatTypeEnd, SteamStats::cStatTypeEnd, SteamStats::cStatTypeEnd, SteamStats::cStatTypeEnd, SteamStats::cStatTypeEnd, SteamStats::cStatTypeEnd, SteamStats::cStatTypeEnd};
const unsigned SteamStats::cAchievmentStep[cAchievmentTypeEnd] = {1, 5, 5, 5, 5, 1, 5, 5, 5, 5, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const std::pair<unsigned, unsigned> SteamStats::cAchievmentMinMax[SteamStats::cAchievmentTypeEnd] = {std::make_pair(0, 1), std::make_pair(1, 10), std::make_pair(10, 25), std::make_pair(25, 50), std::make_pair(50, 100), std::make_pair(0, 1), std::make_pair(1, 10), std::make_pair(10, 25), std::make_pair(25, 50), std::make_pair(50, 100), std::make_pair(0, 1), std::make_pair(0, 50), std::make_pair(0, 1), std::make_pair(0, 1), std::make_pair(0, 1), std::make_pair(0, 1), std::make_pair(0, 1), std::make_pair(0, 1), std::make_pair(0, 1), std::make_pair(0, 1), std::make_pair(0, 1), std::make_pair(0, 1)};

const SteamStats::StatType SteamLeaderboard::cLeadersStat[SteamLeaderboard::cLeadersTypeEnd] = {SteamStats::stPoints, SteamStats::stMoney, SteamStats::stNumGames, SteamStats::stNumWins, SteamStats::stNumKills, SteamStats::stNumDeaths};
const lsl::string SteamLeaderboard::cLeadersName[cLeadersTypeEnd] = {"ltPoints", "ltMoney", "ltNumGames", "ltNumWins", "ltNumKills", "ltNumDeaths"};

const lsl::string SteamServer::cServerVersion = "1.0.0.0";
const unsigned SteamServer::cAuthenticationPort = 8766;
const unsigned SteamServer::cServerPort = 58213;//27015;
const unsigned SteamServer::cMasterServerUpdaterPort = 27016;
const lsl::string SteamServer::cGameDir = "motor rock";

bool SteamService::_init = false;
const float SteamService::cSyncWaitTime = 60.0f;




//-----------------------------------------------------------------------------
// Purpose: callback hook for debug text emitted from the Steam API
//-----------------------------------------------------------------------------
extern "C" void __cdecl SteamAPIDebugTextHook( int nSeverity, const char *pchDebugText )
{
	// if you're running in the debugger, only warnings (nSeverity >= 1) will be sent
	// if you add -debug_steamapi to the command-line, a lot of extra informational messages will also be sent
	LSL_LOG( pchDebugText );

	if ( nSeverity >= 1 )
	{
		// place to set a breakpoint for catching API errors
		int x = 3;
		x = x;
	}
}




NetConnectionSteam::NetConnectionSteam(NetAcceptorSteam* owner): _owner(owner), _isOpen(false)
{	
}

NetConnectionSteam::~NetConnectionSteam()
{
	Close();
}

bool NetConnectionSteam::Send(const void* data, unsigned size)
{
	_owner->SendMessage(k_EMsgReliable, _steamId, (const BYTE*)data, size);
	OnSendCmd(true, size);

	return true;
}

bool NetConnectionSteam::Receive(void* data, unsigned size, unsigned& numBytes)
{
	net::IStreamBuf* buf = _owner->GetPacket(k_EMsgReliable, _steamId);
	if (buf == NULL || buf->size() == 0)
		return false;

	numBytes = std::min(size, buf->size());
	memmove(data, buf->data(), numBytes);
	buf->Consume(numBytes);

	return true;
}

bool NetConnectionSteam::IsAvailable(unsigned& size)
{
	net::IStreamBuf* buf = _owner->GetPacket(k_EMsgReliable, _steamId);
	size = buf ? buf->size() : NULL;

	return true;
}

void NetConnectionSteam::Accept()
{
	Close();

	_isOpen = true;

	_owner->NewAcception(this);
	service()->server()->NeedAcception();
}

bool NetConnectionSteam::Connect(const net::Endpoint& endpoint)
{
	Close();

	_isOpen = true;

	_owner->NewConnection(this);
	service()->NeedConnection();

	return true;
}

void NetConnectionSteam::Close()
{
	if (_isOpen)
	{
		_isOpen = false;

		_owner->RemoveAcception(this);
		_owner->RemoveConnection(this);
	}
}

bool NetConnectionSteam::isOpen() const
{
	return _isOpen;
}

net::Endpoint NetConnectionSteam::localEndpoint()
{
	return net::Endpoint(_steamId.GetAccountID(), 0);
}

net::Endpoint NetConnectionSteam::remoteEndpoint()
{
	return net::Endpoint(_steamId.GetAccountID(), 0);
}

lsl::string NetConnectionSteam::userName() const
{
	const char* pName = SteamFriends()->GetFriendPersonaName(_steamId);

	return pName != NULL ? pName : "";
}

void NetConnectionSteam::OnAccepted(CSteamID steamId, bool success)
{
	if (!_isOpen)
		return;

	if (!success)
	{
		LSL_LOG("OnAccepted failed");

		NotifyAccepted(false);
	}
	else
	{
		_steamId = steamId;
		NotifyAccepted(true);
	}
}

void NetConnectionSteam::OnConnected(CSteamID steamId, bool success)
{
	if (!_isOpen)
		return;

	if (!success)
	{
		LSL_LOG("OnConnected failed ");

		NotifyConnected(false);
	}
	else
	{
		_steamId = steamId;
		NotifyConnected(true);
	}
}

void NetConnectionSteam::OnSendCmd(bool success, std::size_t numBytes)
{
	if (!_isOpen)
		return;

	if (!success)
	{
		LSL_LOG("OnSendCmd failed");

		NotifySend(numBytes, false);
	}
	else
	{
		NotifySend(numBytes, true);
	}
}

SteamService* NetConnectionSteam::service()
{
	return _owner->service();
}




NetChannelSteam::NetChannelSteam(NetAcceptorSteam* owner): _owner(owner), _isOpen(false), _isBind(false)
{	
}

NetChannelSteam::~NetChannelSteam()
{
	Close();
}

bool NetChannelSteam::Send(const net::Endpoint& endpoint, const void* data, unsigned size)
{
	unsigned accountId = endpoint.addressLong;
	CSteamID steamId = service()->server()->FindClient(accountId);

	if (!steamId.IsValid() && service()->serverSteamId().GetAccountID() == accountId)
		steamId = service()->serverSteamId();

	if (!steamId.IsValid())
	{
		OnSend(false, size);
		return false;
	}

	_owner->SendMessage(k_EMsgUnreliable, steamId, (const BYTE*)data, size);

	OnSend(true, size);

	return true;
}

bool NetChannelSteam::Receive(void* data, unsigned size, unsigned& numBytes, net::Endpoint& remoteEndpoint, bool fromOutput)
{
	for (NetAcceptorSteam::PacketMap::const_iterator iter = _owner->GetPacketMap().begin(); iter != _owner->GetPacketMap().end(); ++iter)
		if (iter->second.second && iter->second.second->size() > 0)
		{
			net::IStreamBuf* buf = iter->second.second;
			remoteEndpoint = net::Endpoint(iter->first.GetAccountID(), 0);

			numBytes = std::min(size, buf->size());
			memmove(data, buf->data(), numBytes);
			buf->Consume(numBytes);

			return true;
		}

	return false;
}

bool NetChannelSteam::IsAvailable(unsigned& size, bool fromOutput)
{
	size = 0;

	for (NetAcceptorSteam::PacketMap::const_iterator iter = _owner->GetPacketMap().begin(); iter != _owner->GetPacketMap().end(); ++iter)
		if (iter->second.second)
			size += iter->second.second->size();

	return true;
}

void NetChannelSteam::Flush()
{
}

void NetChannelSteam::Open(bool broadcast)
{
	if (_isOpen)
		return;
	_isOpen = true;
}

void NetChannelSteam::Bind(const net::Endpoint& endpoint)
{
	if (_isBind)
		Unbind();

	_isBind = true;
	_bindEndpoint = endpoint;
}

void NetChannelSteam::Unbind()
{
	if (!_isBind)
		return;

	_isBind = false;
}

void NetChannelSteam::Close()
{
	if (!_isOpen)
		return;
	_isOpen = false;

	Unbind();
}

bool NetChannelSteam::IsOpen() const
{
	return _isOpen;
}

bool NetChannelSteam::IsBind() const
{
	return _isBind;
}

bool NetChannelSteam::broadcast() const
{
	return false;
}

net::Endpoint NetChannelSteam::bindEndpoint() const
{
	return _bindEndpoint;
}

net::Endpoint NetChannelSteam::localEndpoint()
{
	return net::Endpoint(service()->serverSteamId().GetAccountID(), 0);
}

void NetChannelSteam::OnSend(bool success, std::size_t numBytes)
{
	if (!_isOpen)
		return;

	if (!success)
	{
		LSL_LOG("OnSendCmd failed");

		NotifySend(numBytes, false);
	}
	else
	{
		NotifySend(numBytes, true);
	}
}

SteamService* NetChannelSteam::service()
{
	return _owner->service();
}




NetAcceptorSteam::NetAcceptorSteam(SteamService* service): _service(service), _isOpen(false)
{
	_steam = net::GetNetService().CreateStreamBuf();
}

NetAcceptorSteam::~NetAcceptorSteam()
{
	Close();

	for (PacketMap::const_iterator iter = _packetMap.begin(); iter != _packetMap.end(); ++iter)
	{
		net::GetNetService().ReleaseStreamBuf(iter->second.first);
		net::GetNetService().ReleaseStreamBuf(iter->second.second);
	}
	_packetMap.clear();

	net::GetNetService().ReleaseStreamBuf(_steam);
}

void NetAcceptorSteam::Listen(const net::Endpoint& endpoint)
{
	if (_isOpen)
		return;
	_isOpen = true;
}

void NetAcceptorSteam::Close()
{
	if (!_isOpen)
		return;
	_isOpen = false;
}

net::INetConnectionImpl* NetAcceptorSteam::NewConnection()
{
	return new NetConnectionSteam(this);
}

void NetAcceptorSteam::ReleaseConnection(net::INetConnectionImpl* connection)
{
	delete static_cast<NetConnectionSteam*>(connection);
}

net::INetChannelImpl* NetAcceptorSteam::NewChannel()
{
	return new NetChannelSteam(this);
}

void NetAcceptorSteam::ReleaseChannel(net::INetChannelImpl* channel)
{
	delete static_cast<NetChannelSteam*>(channel);
}

void NetAcceptorSteam::NewAcception(NetConnectionSteam* connection)
{
	if (_acceptionList.IsFind(connection))
		return;

	_acceptionList.push_back(connection);
}

NetAcceptorSteam::Connections::const_iterator NetAcceptorSteam::RemoveAcception(Connections::const_iterator iter)
{
	return _acceptionList.erase(iter);
}

void NetAcceptorSteam::RemoveAcception(NetConnectionSteam* connection)
{
	_acceptionList.Remove(connection);
}

const NetAcceptorSteam::Connections& NetAcceptorSteam::acceptionList() const
{
	return _acceptionList;
}

void NetAcceptorSteam::NewConnection(NetConnectionSteam* connection)
{
	if (_connectionList.IsFind(connection))
		return;

	_connectionList.push_back(connection);
}

NetAcceptorSteam::Connections::const_iterator NetAcceptorSteam::RemoveConnection(Connections::const_iterator iter)
{
	return _connectionList.erase(iter);
}

void NetAcceptorSteam::RemoveConnection(NetConnectionSteam* connection)
{
	_connectionList.Remove(connection);
}

const NetAcceptorSteam::Connections& NetAcceptorSteam::connectionList() const
{
	return _connectionList;
}

void NetAcceptorSteam::SendMessage(EMessage msg, const CSteamID& steamId, const BYTE* data, unsigned size)
{
	unsigned headerSz = sizeof(EMessage);
	unsigned msgSz = headerSz + size;

	memmove(_steam->Prepare(headerSz), &msg, headerSz);
	_steam->Commit(headerSz);

	memmove(_steam->Prepare(size), data, size);
	_steam->Commit(size);

	if (_service->server()->hostInit() && steamId != _service->server()->steamId())
		SteamGameServerNetworking()->SendP2PPacket(steamId, _steam->data(), msgSz, msg == k_EMsgReliable ? k_EP2PSendReliable : k_EP2PSendUnreliable);
	else
		SteamNetworking()->SendP2PPacket(steamId, _steam->data(), msgSz, msg == k_EMsgReliable ? k_EP2PSendReliable : k_EP2PSendUnreliable);

	_steam->Consume(msgSz);
}

bool NetAcceptorSteam::HandleMessage(const CSteamID& steamId, EMessage msg, const BYTE* data, unsigned size)
{
	if (msg != k_EMsgReliable && msg != k_EMsgUnreliable)
		return false;

	net::IStreamBuf* stream = GetOrCreatePacket(msg, steamId);
	unsigned headerSz = sizeof(EMessage);
	unsigned dataSz = size - headerSz;	

	BYTE* dest = stream->Prepare(dataSz);
	memmove(dest, data + headerSz, dataSz);
	stream->Commit(dataSz);

	return true;
}

net::IStreamBuf* NetAcceptorSteam::GetOrCreatePacket(EMessage msg, const CSteamID& steamId)
{
	PacketMap::const_iterator iter = _packetMap.find(steamId);
	if (iter == _packetMap.end())
		iter = _packetMap.insert(iter, PacketMap::value_type(steamId, std::make_pair(net::GetNetService().CreateStreamBuf(), net::GetNetService().CreateStreamBuf())));

	return msg == k_EMsgReliable ? iter->second.first : iter->second.second;
}

net::IStreamBuf* NetAcceptorSteam::GetPacket(EMessage msg, const CSteamID& steamId)
{
	PacketMap::const_iterator iter = _packetMap.find(steamId);

	return iter != _packetMap.end() ? (msg == k_EMsgReliable ? iter->second.first : iter->second.second) : NULL;
}

const NetAcceptorSteam::PacketMap& NetAcceptorSteam::GetPacketMap() const
{
	return _packetMap;
}

SteamService* NetAcceptorSteam::service()
{
	return _service;
}




SteamStats::SteamStats(SteamService* service): _service(service), _userStatsLoading(false), _userStatsSaving(false), _raceNumShots(0),
	_callbackUserStatsReceived( this, &SteamStats::OnUserStatsReceived ),
	_callbackUserStatsStored( this, &SteamStats::OnUserStatsStored )
{	
}

SteamStats::~SteamStats()
{
}

//-----------------------------------------------------------------------------
// Purpose: We have stats data from Steam. It is authoritative, so update
//			our data with those results now.
//-----------------------------------------------------------------------------
void SteamStats::OnUserStatsReceived( UserStatsReceived_t *pCallback )
{
	// we may get callbacks for other games' stats arriving, ignore them
	if (!_userStatsLoading || _service->gameId().ToUint64() != pCallback->m_nGameID)
		return;

	_userStatsLoading = false;
	bool success = false;
	
	if ( k_EResultOK == pCallback->m_eResult )
	{
		int intVal = 0;

		// load achievements
		for (int i = 0; i < cAchievmentTypeEnd; ++i)
		{
			Achievment& ach = _achievments[i];
			_achievments[i].loaded = true;
			SteamUserStats()->GetAchievement(cAchievmentName[i].c_str(), &ach.unlocked);
		}

		// load stats
		for (int i = 0; i < cStatTypeEnd; ++i)
			if (SteamUserStats()->GetStat(cStatName[i].c_str(), &intVal))
			{
				_stats[i].value = intVal + _stats[i].dValue;
				_stats[i].dValue = 0;
			}

		success = true;
	}
	else
	{
		success = false;
	}

	_service->SendEvent(cSteamGetUserStats, &EventData(success));
}

//-----------------------------------------------------------------------------
// Purpose: Our stats data was stored!
//-----------------------------------------------------------------------------
void SteamStats::OnUserStatsStored( UserStatsStored_t *pCallback )
{
	// we may get callbacks for other games' stats arriving, ignore them
	if (!_userStatsSaving || _service->gameId().ToUint64() != pCallback->m_nGameID)
		return;

	_userStatsSaving = false;
	bool success = false;
	
	if ( k_EResultOK == pCallback->m_eResult )
	{
		success = true;
	}
	else if ( k_EResultInvalidParam == pCallback->m_eResult )
	{
		// One or more stats we set broke a constraint. They've been reverted,
		// and we should re-iterate the values now to keep in sync.
		LSL_LOG( "StoreStats - some failed to validate\n" );

		// Fake up a callback here so that we re-load the values.
		UserStatsReceived_t callback;
		callback.m_eResult = k_EResultOK;
		callback.m_nGameID = _service->gameId().ToUint64();
		OnUserStatsReceived( &callback );
	}
	else
	{
		success = false;
	}

	_service->SendEvent(cSteamStoreUserStats, &EventData(success));
}

bool SteamStats::GetUserStats()
{
	if (_userStatsLoading)
		return true;

	if (SteamUserStats()->RequestCurrentStats())
	{
		_userStatsLoading = true;
		return true;
	}

	return false;
}

bool SteamStats::StoreUserStats()
{
	if (_userStatsSaving)
		return true;
	_userStatsSaving = true;

	// set stats
	for (int i = 0; i < cStatTypeEnd; ++i)
	{
		_stats[i].value += _stats[i].dValue;
		SteamUserStats()->SetStat(cStatName[i].c_str(), _stats[i].value);
		_stats[i].dValue = 0;
	}

	if (SteamUserStats()->StoreStats())
		return true;

	return false;
}

void SteamStats::AddStat(StatType type, int value)
{	
	_stats[type].dValue += value;
	unsigned total = (unsigned)GetStatValue(type);

	for (int i = 0; i < cAchievmentTypeEnd; ++i)
		if (_achievments[i].loaded && !_achievments[i].unlocked && cAchievmentStat[i] == type && cAchievmentStep[i] > 0 && ((total == cAchievmentMinMax[i].first) || (total > cAchievmentMinMax[i].first && (total % cAchievmentStep[i]) == 0)))
		{
			IndicateAchievment((AchievmentType)i, total);
			return;
		}
}

int SteamStats::GetStatValue(StatType type) const
{
	return _stats[type].value + _stats[type].dValue;
}

void SteamStats::IndicateAchievment(AchievmentType type, int curProgress)
{
	if (_achievments[type].unlocked || !_achievments[type].loaded)
		return;

	unsigned progress = lsl::ClampValue((unsigned)curProgress, cAchievmentMinMax[type].first, cAchievmentMinMax[type].second);

	if (progress >= cAchievmentMinMax[type].second)
		UnlockAchievment(type);
	else
		SteamUserStats()->IndicateAchievementProgress(cAchievmentName[type].c_str(), progress, cAchievmentMinMax[type].second);
}

void SteamStats::UnlockAchievment(AchievmentType type)
{
	if (_achievments[type].unlocked || !_achievments[type].loaded)
		return;

	_achievments[type].unlocked = true;	
	SteamUserStats()->SetAchievement(cAchievmentName[type].c_str());

	if (type != atWeAreTheChampions)
	{
		bool unlockedAll = true;

		for (int i = 0; i < cAchievmentTypeEnd; ++i)
			if (i != atWeAreTheChampions && _achievments[i].loaded && !_achievments[i].unlocked)
			{
				unlockedAll = false;
				break;
			}

		if (unlockedAll)
			UnlockAchievment(atWeAreTheChampions);
	}

	StoreUserStats();
}

void SteamStats::ResetAll()
{
	SteamUserStats()->ResetAllStats(true);
}

void SteamStats::OnProcessEvent(unsigned id, EventData* data)
{
	switch (id)
	{
	case cRaceStart:
		_raceNumShots = 0;
		break;

	case cRaceFinish:
		AddStat(stNumGames, 1);

		if (game()->GetRace()->GetPlayerList().size() >= 2 && game()->GetRace()->GetHuman()->GetPlayer()->GetPlace() == 1)
		{
			AddStat(stNumWins, 1);
		}

		if (_raceNumShots == 0 && game()->GetRace()->GetTournament().GetCurTrack().GetLapsCount() >= 4 && game()->GetRace()->GetPlayerList().size() >= 2)
		{
			UnlockAchievment(atLoveAndPeace);
		}

		if (game()->GetRace()->IsCampaign() && game()->GetRace()->GetTournament().GetCurPlanetIndex() == 0 && game()->GetRace()->GetTournament().GetCurTrackIndex() == 0 && game()->GetRace()->GetTournament().GetCurPlanet().GetPass() == 1 && game()->GetRace()->GetHuman()->GetPlayer()->GetPlace() == 1)
		{
			UnlockAchievment(atNiceStart);
		}
		break;

	case cRacePassLap:
		if (data->playerId == Race::cHuman)
		{
			_lapKills.clear();
		}
		break;

	case cPlayerKill:
		if (data->playerId == Race::cHuman)
		{
			GameObject::MyEventData* myData = static_cast<GameObject::MyEventData*>(data);
			Player* player = game()->GetRace()->GetPlayerById(data->playerId);

			if (data->playerId != myData->targetPlayerId)
				_lapKills.push_back(myData->targetPlayerId);			

			if (player && player->GetCar().numLaps >= 3 && player->GetCar().numLaps >= game()->GetRace()->GetTournament().GetCurTrack().GetLapsCount() - 1 && _lapKills.size() >= 3)
			{
				UnlockAchievment(atSpeedKill);
			}

			AddStat(stNumKills, 1);
		}
		break;

	case cHumanShot:
		++_raceNumShots;		
		break;

	case cPlayerDeath:
		if (data->playerId == Race::cHuman)
		{
			AddStat(stNumDeaths, 1);

			Player* plr = game()->GetRace()->GetPlayerById(data->playerId);

			if (plr && plr->GetCar().IsMainPath(true) && plr->GetCar().GetPathLength(true) - plr->GetCar().GetDist(true) < 10.0f)
			{
				UnlockAchievment(atWellWhoDoesNotHappen);
			}
		}
		break;
	}
}

SteamService* SteamStats::service()
{
	return _service;
}

GameMode* SteamStats::game()
{
	return _service->game();
}




SteamLeaderboard::SteamLeaderboard(SteamService* service): _service(service), _leadersLoading(false), _userStatsUploading(0)
{
}

SteamLeaderboard::~SteamLeaderboard()
{
}

void SteamLeaderboard::OnFindLeaderboard(LeaderboardFindResult_t* pFindLeaderboardResult, bool bIOFailure)
{
	_leadersLoading = false;

	if (!bIOFailure)
	{
		for (int i = 0; i < cLeadersTypeEnd; ++i)
			if (cLeadersName[i] == SteamUserStats()->GetLeaderboardName( pFindLeaderboardResult->m_hSteamLeaderboard ))
			{
				_leaders[i].board = pFindLeaderboardResult->m_hSteamLeaderboard;
				break;
			}

		if (FindLeaders())
			return;
	}

	_service->SendEvent(cSteamGetLeaders, &EventData(!bIOFailure));
}

void SteamLeaderboard::OnUploadUserStats(LeaderboardScoreUploaded_t* pFindLearderboardResult, bool bIOFailure)
{
	if (_userStatsUploading > 0 && (--_userStatsUploading) == 0)
		_service->SendEvent(cSteamStoreLeaders, &EventData(!bIOFailure));
}

bool SteamLeaderboard::FindLeaders()
{	
	if (_leadersLoading)
		return true;

	for (int i = 0; i < cLeadersTypeEnd; ++i)
	{
		if (_leaders[i].board != NULL)
			continue;

		SteamAPICall_t hSteamAPICall = SteamUserStats()->FindOrCreateLeaderboard(cLeadersName[i].c_str(), k_ELeaderboardSortMethodAscending, k_ELeaderboardDisplayTypeTimeSeconds);

		if (hSteamAPICall != NULL)
		{
			_leadersLoading = true;

			// set the function to call when this API call has completed
			_callResultFindLeaderboard.Set(hSteamAPICall, this, &SteamLeaderboard::OnFindLeaderboard);

			return true;
		}
	}

	return false;
}

bool SteamLeaderboard::UploadUserStats()
{
	if (_userStatsUploading > 0)
		return true;

	for (int i = 0; i < cLeadersTypeEnd; ++i)
	{
		LSL_ASSERT(_leaders[i].board);

		int value = _service->steamStats()->GetStatValue(cLeadersStat[i]);

		// if the user won, update the leaderboard with the time it took. If the user's previous time was faster, this time will be thrown out.
		SteamAPICall_t hSteamAPICall = SteamUserStats()->UploadLeaderboardScore(_leaders[i].board, k_ELeaderboardUploadScoreMethodKeepBest, value, NULL, 0);

		if (hSteamAPICall != NULL)
		{			
			++_userStatsUploading;

			// set the function to call when this API call has completed
			_leaders[i].callResultUploadUsersStats.Set(hSteamAPICall, this, &SteamLeaderboard::OnUploadUserStats);
		}
	}

	return _userStatsUploading > 0;
}

SteamService* SteamLeaderboard::service()
{
	return _service;
}

GameMode* SteamLeaderboard::game()
{
	return _service->game();
}




SteamLobby::SteamLobby(SteamService* service): _service(service), _lobbyListLoading(false), _lobbyLoading(false), _matchStarting(false), _lobbyCreated(false),
	_callbackLobbyDataUpdated(this, &SteamLobby::OnLobbyDataUpdatedCallback),
	_callbackPersonaStateChange(this, &SteamLobby::OnPersonaStateChange),
	_callbackChatDataUpdate(this, &SteamLobby::OnLobbyChatUpdate),
	_callbackLobbyGameCreated(this, &SteamLobby::OnLobbyGameCreated)
{
	_currentLobby.Clear();
}

SteamLobby::~SteamLobby()
{	
}

std::string SteamLobby::GetLobbyName(CSteamID id)
{
	const char* pName = SteamMatchmaking()->GetLobbyData(id, "name");

	return pName != NULL ? pName : "";
}

void SteamLobby::SetLobbdyName(CSteamID id, const std::string& name)
{
	SteamMatchmaking()->SetLobbyData(id, "name", name.c_str());
}

bool SteamLobby::GetLobbyMemberReady(CSteamID id) const
{
	const char* pName = SteamMatchmaking()->GetLobbyMemberData(_currentLobby, id, "ready");

	return pName != NULL && pName[0] == '1';
}

void SteamLobby::SetLobbyMemberReady(bool state)
{
	SteamMatchmaking()->SetLobbyMemberData(_currentLobby, "name", state ? "1" : "0");
}

void SteamLobby::UpdateLobbyMembers()
{
	_lobbyMemberList.clear();
	int cLobbyMembers = SteamMatchmaking()->GetNumLobbyMembers( _currentLobby );

	for ( int i = 0; i < cLobbyMembers; i++ )
	{
		CSteamID steamIDLobbyMember = SteamMatchmaking()->GetLobbyMemberByIndex( _currentLobby, i ) ;

		// ignore yourself.
		if ( SteamUser()->GetSteamID() == steamIDLobbyMember )
			continue;

		LobbyMember member;

		//id
		member.id = steamIDLobbyMember;

		// we get the details of a user from the ISteamFriends interface
		const char* pName = SteamFriends()->GetFriendPersonaName(steamIDLobbyMember);
		member.name = pName != NULL ? pName : "";
		member.ready = GetLobbyMemberReady(steamIDLobbyMember);

		_lobbyMemberList.push_back(member);
	}

	_service->SendEvent(cSteamUpdateLobbyMembers, &EventData(true));
}

void SteamLobby::OnLobbyMatchListCallback(LobbyMatchList_t* pLobbyMatchList, bool bIOFailure)
{
	_lobbyList.clear();
	_lobbyListLoading = false;

	if ( bIOFailure )
	{
		// we had a Steam I/O failure - we probably timed out talking to the Steam back-end servers
		// doesn't matter in this case, we can just act if no lobbies were received
	}

	// lobbies are returned in order of closeness to the user, so add them to the list in that order
	for (unsigned i = 0; i < pLobbyMatchList->m_nLobbiesMatching; ++i)
	{
		CSteamID steamIDLobby = SteamMatchmaking()->GetLobbyByIndex(i);

		// add the lobby to the list
		Lobby lobby;
		lobby.id = steamIDLobby;

		// pull the name from the lobby metadata
		lsl::string name = GetLobbyName(steamIDLobby);

		if (!name.empty())
		{
			// set the lobby name
			lobby.name = name;
		}
		else
		{
			// we don't have info about the lobby yet, request it
			SteamMatchmaking()->RequestLobbyData(steamIDLobby);

			// results will be returned via LobbyDataUpdate_t callback
			lobby.name = lsl::StrFmt("Lobby %d", steamIDLobby.GetAccountID());
		}

		_lobbyList.push_back(lobby);
	}

	_service->SendEvent(cSteamGetLobbyList, &EventData(!bIOFailure));
}

void SteamLobby::OnLobbyEntered(LobbyEnter_t* pCallback, bool bIOFailure)
{
	if (!_lobbyLoading)
		return;
	_lobbyLoading = false;

	bool success = pCallback->m_EChatRoomEnterResponse == k_EChatRoomEnterResponseSuccess;

	if ( success )
	{
		// success
		// move forward the state
		_currentLobby = pCallback->m_ulSteamIDLobby;
	}
	else
	{
		// failed, show error
		LSL_LOG( "Failed to enter lobby" );
	}

	_service->SendEvent(cSteamEnteredLobby, &EventData(success));

	UpdateLobbyMembers();
}

void SteamLobby::OnLobbyCreated(LobbyCreated_t *pCallback, bool bIOFailure)
{
	if (!_lobbyLoading)
		return;
	_lobbyLoading = false;

	bool success = pCallback->m_eResult == k_EResultOK;

	// record which lobby we're in
	if ( success )
	{
		// success
		_currentLobby = pCallback->m_ulSteamIDLobby;

		// set the name of the lobby if it's ours		
		SetLobbdyName(_currentLobby, lsl::StrFmt("%s's lobby", SteamFriends()->GetPersonaName()));
	}
	else
	{
		// failed, show error
		LSL_LOG( "Failed to create lobby (lost connection to Steam back-end servers." );
	}

	_service->SendEvent(cSteamCreateLobby, &EventData(success));
}

void SteamLobby::OnLobbyDataUpdatedCallback( LobbyDataUpdate_t *pCallback )
{
	// find the lobby in our local list 
	LobbyList::iterator iter;

	for( iter = _lobbyList.begin(); iter != _lobbyList.end(); ++iter )
	{
		// update the name of the lobby
		if ( iter->id == pCallback->m_ulSteamIDLobby )
		{
			// extract the display name from the lobby metadata
			iter->name = GetLobbyName(iter->id);

			_service->SendEvent(cSteamUpdateLobby, &EventData(true));
			return;
		}
	}

	UpdateLobbyMembers();
}

//-----------------------------------------------------------------------------
// Purpose: Handles a user in the lobby changing their name or details
//			( note: joining and leaving is handled below by CLobby::OnLobbyChatUpdate() )
//-----------------------------------------------------------------------------
void SteamLobby::OnPersonaStateChange( PersonaStateChange_t *pCallback )
{
	// callbacks are broadcast to all listeners, so we'll get this for every friend who changes state
	// so make sure the user is in the lobby before acting
	if ( !SteamFriends()->IsUserInSource( pCallback->m_ulSteamID, _currentLobby ) )
		return;

	for (MemberList::iterator iter = _lobbyMemberList.begin(); iter != _lobbyMemberList.end(); ++iter)
	{
		if (iter->id == pCallback->m_ulSteamID)
		{
			const char* pName = SteamFriends()->GetFriendPersonaName(iter->id);
			iter->name = pName != NULL ? pName : "";

			_service->SendEvent(cSteamUpdateLobbyMember, &EventData(true));
			return;
		}
	}

	UpdateLobbyMembers();
}

//-----------------------------------------------------------------------------
// Purpose: Handles users in the lobby joining or leaving
//-----------------------------------------------------------------------------
void SteamLobby::OnLobbyChatUpdate( LobbyChatUpdate_t *pCallback )
{
	// callbacks are broadcast to all listeners, so we'll get this for every lobby we're requesting
	if ( _currentLobby != pCallback->m_ulSteamIDLobby )
		return;

	UpdateLobbyMembers();
}

void SteamLobby::OnLobbyGameCreated( LobbyGameCreated_t *pCallback )
{
	if (!_currentLobby.IsValid())
		return;

	LeaveLobby();

	CSteamID steamId = CSteamID(pCallback->m_ulSteamIDGameServer);

	_service->SendEvent(cSteamLobbyGameCreated, &MyEventData(steamId.IsValid(), steamId));
}

void SteamLobby::ProcessEvent(int id, EventData* data)
{
	switch (id)
	{
	case cSteamHostCreated:
		if (_matchStarting)
		{
			_matchStarting = false;

			//if (data->success)
			{
				SteamMatchmaking()->SetLobbyGameServer(_currentLobby, 0, 0, _service->server()->steamId());
			}

			LeaveLobby();
		}
		break;
	}
}

void SteamLobby::RequestLobbyList()
{
	if (_lobbyListLoading)
		return;

	_lobbyListLoading = true;

	// request all lobbies for this game
	SteamAPICall_t hSteamAPICall = SteamMatchmaking()->RequestLobbyList();

	// set the function to call when this API call has completed
	_callResultLobbyMatchList.Set(hSteamAPICall, this, &SteamLobby::OnLobbyMatchListCallback);
}

const SteamLobby::LobbyList& SteamLobby::GetLobbyList() const
{
	return _lobbyList;
}

void SteamLobby::JoinLobby(CSteamID id)
{
	if (_lobbyLoading)
		return;

	LeaveLobby();
	_lobbyLoading = true;

	// start joining the lobby
	SteamAPICall_t hSteamAPICall = SteamMatchmaking()->JoinLobby(id);
	// set the function to call when this API completes
	_callResultLobbyEntered.Set( hSteamAPICall, this, &SteamLobby::OnLobbyEntered );
}

void SteamLobby::CreateLobby(ELobbyType type, int maxMembers)
{
	if (_lobbyLoading)
		return;

	LeaveLobby();
	_lobbyLoading = true;
	_lobbyCreated = true;

	// ask steam to create a lobby
	SteamAPICall_t hSteamAPICall = SteamMatchmaking()->CreateLobby(type, maxMembers);
	// set the function to call when this completes
	_callResultLobbyCreated.Set( hSteamAPICall, this, &SteamLobby::OnLobbyCreated );

	SteamFriends()->SetRichPresence( "status", "Creating a lobby" );
}

void SteamLobby::LeaveLobby()
{
	_lobbyLoading = false;
	_lobbyCreated = false;

	if (_currentLobby.IsValid())
	{
		SteamMatchmaking()->LeaveLobby(_currentLobby);
		_currentLobby.Clear();
	}

	_lobbyMemberList.clear();
}

void SteamLobby::ReadyLobby(bool ready)
{
	SetLobbyMemberReady(ready);
}

bool SteamLobby::isReadyLobby() const
{
	return GetLobbyMemberReady(SteamUser()->GetSteamID());
}

bool SteamLobby::lobbyCreated() const
{
	return _lobbyCreated;
}

bool SteamLobby::StartMath()
{
	if (_matchStarting)
		return false;

	if (_currentLobby.IsValid() && _service->server()->CreateHost())
	{
		_matchStarting = true;
		return true;
	}

	return false;
}

const SteamLobby::MemberList& SteamLobby::GetLobbyMembers() const
{
	return _lobbyMemberList;
}

CSteamID SteamLobby::currentLobby() const
{
	return _currentLobby;
}

SteamService* SteamLobby::service()
{
	return _service;
}

GameMode* SteamLobby::game()
{
	return _service->game();
}




SteamServer::SteamServer(SteamService* service): _service(service), _serverListRequest(NULL), _serverConnecting(false), _hostInit(false), _internetListLoading(false),
	_callbackSteamServersConnected( this, &SteamServer::OnSteamServersConnected ),
	_callbackSteamServersDisconnected( this, &SteamServer::OnSteamServersDisconnected ),
	_callbackSteamServersConnectFailure( this, &SteamServer::OnSteamServersConnectFailure ),
	_callbackP2PSessionConnectFail( this, &SteamServer::OnP2PSessionConnectFail ),
	_callbackP2PSessionRequest( this, &SteamServer::OnP2PSessionRequest ),
	_callbackGSAuthTicketResponse( this, &SteamServer::OnValidateAuthTicketResponse )
{
}

SteamServer::~SteamServer()
{
	// If _serverListRequest request is outstanding, make sure we release it properly
	if ( _serverListRequest )
	{
		SteamMatchmakingServers()->ReleaseRequest( _serverListRequest );
		_serverListRequest = NULL;
	}

	ShutdownHost();
}

//-----------------------------------------------------------------------------
// Purpose: Send data to the current server
//-----------------------------------------------------------------------------
bool SteamServer::SendData(CSteamID steamId, const void *pData, uint32 nSizeOfData )
{
	if ( !SteamGameServerNetworking()->SendP2PPacket( steamId, pData, nSizeOfData, k_EP2PSendReliable ) )
	{
		LSL_LOG( "Failed sending data to server\n" );

		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handle sending data to a client at a given index
//-----------------------------------------------------------------------------
bool SteamServer::SendDataToClient( uint32 uShipIndex, char *pData, uint32 nSizeOfData )
{
	// Validate index
	if ( uShipIndex >= cMaxPlayers )
		return false;

	if ( !SteamGameServerNetworking()->SendP2PPacket( _rgClientData[uShipIndex].m_SteamIDUser, pData, nSizeOfData, k_EP2PSendUnreliable ) )
	{
		LSL_LOG( "Failed sending data to a client\n" );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Removes a player at the given position
//-----------------------------------------------------------------------------
void SteamServer::RemovePlayerFromServer( uint32 uShipPosition )
{
	if ( uShipPosition >= cMaxPlayers )
	{
		LSL_LOG( "Trying to remove ship at invalid position\n" );
		return;
	}

	LSL_LOG( "Removing a ship\n" );

	// Tell the GS the user is leaving the server
	SteamGameServer()->EndAuthSession( _rgClientData[uShipPosition].m_SteamIDUser );

	memset( &_rgClientData[uShipPosition], 0, sizeof( ClientConnectionData_t ) );
}

void SteamServer::SendUpdatedServerDetailsToSteam()
{
	// Set state variables, relevant to any master server updates or client pings
	//

	// These server state variables may be changed at any time.  Note that there is no lnoger a mechanism
	// to send the player count.  The player count is maintained by steam and you should use the player
	// creation/authentication functions to maintain your player count.
	SteamGameServer()->SetMaxPlayerCount( Race::cMaxPlayers );
	SteamGameServer()->SetPasswordProtected( false );
	SteamGameServer()->SetServerName( GetServerName().c_str() );
	SteamGameServer()->SetBotPlayerCount( 0 ); // optional, defaults to zero
	SteamGameServer()->SetMapName( "RRR Map" );

	// game type is a special string you can use for your game to differentiate different game play types occurring on the same maps
	// When users search for this parameter they do a sub-string search of this string 
	// (i.e if you report "abc" and a client requests "ab" they return your server)
	//SteamGameServer()->SetGameType( "dm" );

	// update any rule values we publish
	//SteamMasterServerUpdater()->SetKeyValue( "rule1_setting", "value" );
	//SteamMasterServerUpdater()->SetKeyValue( "rule2_setting", "value2" );
}

void SteamServer::ReceiveNetworkData()
{
	char *pchRecvBuf = NULL;
	uint32 cubMsgSize;
	CSteamID steamIDRemote;

	while ( SteamGameServerNetworking()->IsP2PPacketAvailable( &cubMsgSize ) )
	{
		// free any previous receive buffer
		if ( pchRecvBuf )
			free( pchRecvBuf );

		// alloc a new receive buffer of the right size
		pchRecvBuf = (char *)malloc( cubMsgSize );

		// see if there is any data waiting on the socket
		if ( !SteamGameServerNetworking()->ReadP2PPacket( pchRecvBuf, cubMsgSize, &cubMsgSize, &steamIDRemote ) )
			break;

		if ( cubMsgSize < sizeof( DWORD ) )
		{
			LSL_LOG( "Got garbage on server socket, too short\n" );
			continue;
		}

		EMessage eMsg = (EMessage)*(DWORD*)pchRecvBuf;

		switch ( eMsg )
		{
			case k_EMsgClientInitiateConnection:
			{
				// We always let clients do this without even checking for room on the server since we reserve that for 
				// the authentication phase of the connection which comes next
				MsgServerSendInfo_t msg;
				msg.SetSteamIDServer( SteamGameServer()->GetSteamID().ConvertToUint64() );
				msg.SetServerName( GetServerName().c_str() );

				// You can only make use of VAC when using the Steam authentication system
				msg.SetSecure( SteamGameServer()->BSecure() );
				
				SendData( steamIDRemote, &msg, sizeof( MsgServerSendInfo_t ) );
				break;
			}

			case k_EMsgClientBeginAuthentication:
			{
				if ( cubMsgSize != sizeof( MsgClientBeginAuthentication_t ) )
				{
					LSL_LOG( "Bad connection attempt msg\n" );
					continue;
				}

				MsgClientBeginAuthentication_t *pMsg = (MsgClientBeginAuthentication_t*)pchRecvBuf;

				OnClientBeginAuthentication( steamIDRemote, (void*)pMsg->GetTokenPtr(), pMsg->GetTokenLen() );
			}

			default:				
				_service->acceptor()->HandleMessage(steamIDRemote, eMsg, (const BYTE*)pchRecvBuf, cubMsgSize);
				break;
		}
	}

	if ( pchRecvBuf )
		free( pchRecvBuf );
}

//-----------------------------------------------------------------------------
// Purpose: Callback from Steam telling us about a server that has responded
//-----------------------------------------------------------------------------
void SteamServer::ServerResponded( HServerListRequest hReq, int iServer )
{
	if (!_internetListLoading)
		return;

	gameserveritem_t* pServer = SteamMatchmakingServers()->GetServerDetails( hReq, iServer );
	
	if ( pServer )
	{
		// Filter out servers that don't match our appid here (might get these in LAN calls since we can't put more filters on it)
		//if ( pServer->m_nAppID == SteamUtils()->GetAppID() )
		{
			Host host;
			host.inst = pServer;
			_hostList.push_back(host);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Callback from Steam telling us about a server that has failed to respond
//-----------------------------------------------------------------------------
void SteamServer::ServerFailedToRespond( HServerListRequest hReq, int iServer )
{
	if (!_internetListLoading)
		return;

	gameserveritem_t* pServer = SteamMatchmakingServers()->GetServerDetails( hReq, iServer );
	if (pServer)
	{
		lsl::string name = pServer->GetName();
		unsigned ip = pServer->m_NetAdr.GetIP();
	}

	// bugbug jmccaskey - why would we ever need this?  Remove servers from our list I guess?
}

//-----------------------------------------------------------------------------
// Purpose: Callback from Steam telling us a refresh is complete
//-----------------------------------------------------------------------------
void SteamServer::RefreshComplete( HServerListRequest hReq, EMatchMakingServerResponse response ) 
{
	if (!_internetListLoading)
		return;
	_internetListLoading = false;

	// Doesn't really matter to us whether the response tells us the refresh succeeded or failed,
	// we just track whether we are done refreshing or not

	_service->SendEvent(cSteamHostList, &EventData(true));
}

//-----------------------------------------------------------------------------
// Purpose: Handle a new client connecting
//-----------------------------------------------------------------------------
void SteamServer::OnClientBeginAuthentication( CSteamID steamIDClient, void *pToken, uint32 uTokenLen )
{
	// First, check this isn't a duplicate and we already have a user logged on from the same steamid
	for( unsigned i = 0; i < cMaxPlayers; ++i ) 
	{
		if ( _rgClientData[i].m_SteamIDUser == steamIDClient )
		{
			// We already logged them on... (should maybe tell them again incase they don't know?)
			return;
		}
	}

	// Second, do we have room?
	unsigned nPendingOrActivePlayerCount = 0;
	for ( uint32 i = 0; i < cMaxPlayers; ++i )
	{
		if ( _rgPendingClientData[i].m_bActive )
			++nPendingOrActivePlayerCount;
		
		if ( _rgClientData[i].m_bActive )
			++nPendingOrActivePlayerCount;
	}

	// We are full (or will be if the pending players auth), deny new login
	if ( nPendingOrActivePlayerCount >= cMaxPlayers )
	{
		MsgServerFailAuthentication_t msg;
		SteamGameServerNetworking()->SendP2PPacket( steamIDClient, &msg, sizeof( msg ), k_EP2PSendReliable );
	}
			
	// If we get here there is room, add the player as pending
	for( uint32 i = 0; i < cMaxPlayers; ++i ) 
	{
		if ( !_rgPendingClientData[i].m_bActive )
		{
			//_rgPendingClientData[i].m_ulTickCountLastData = m_pGameEngine->GetGameTickCount();

			// authenticate the user with the Steam back-end servers
			if ( k_EBeginAuthSessionResultOK != SteamGameServer()->BeginAuthSession( pToken, uTokenLen, steamIDClient ) )
			{
				MsgServerFailAuthentication_t msg;
				SteamGameServerNetworking()->SendP2PPacket( steamIDClient, &msg, sizeof( msg ), k_EP2PSendReliable );
				break;
			}

			_rgPendingClientData[i].m_SteamIDUser = steamIDClient;
			_rgPendingClientData[i].m_bActive = true;
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: A new client that connected has had their authentication processed
//-----------------------------------------------------------------------------
void SteamServer::OnAuthCompleted( bool bAuthSuccessful, uint32 iPendingAuthIndex )
{
	if ( !bAuthSuccessful )
	{
		// Tell the GS the user is leaving the server
		SteamGameServer()->EndAuthSession( _rgPendingClientData[iPendingAuthIndex].m_SteamIDUser );

		// Send a deny for the client, and zero out the pending data
		MsgServerFailAuthentication_t msg;
		SteamGameServerNetworking()->SendP2PPacket( _rgPendingClientData[iPendingAuthIndex].m_SteamIDUser, &msg, sizeof( msg ), k_EP2PSendReliable );
		memset( &_rgPendingClientData[iPendingAuthIndex], 0, sizeof( ClientConnectionData_t ) );
		return;
	}


	bool bAddedOk = false;

	for( uint32 i = 0; i < cMaxPlayers; ++i ) 
	{
		if ( !_rgClientData[i].m_bActive )
		{
			// copy over the data from the pending array
			memcpy( &_rgClientData[i], &_rgPendingClientData[iPendingAuthIndex], sizeof( ClientConnectionData_t ) );
			memset( &_rgPendingClientData[iPendingAuthIndex], 0, sizeof( ClientConnectionData_t	) );
			//_rgClientData[i].m_ulTickCountLastData = m_pGameEngine->GetGameTickCount();

			// Add a new ship, make it dead immediately
			MsgServerPassAuthentication_t msg;
			msg.SetPlayerPosition( i );
			SendDataToClient( i, (char*)&msg, sizeof( msg ) );

			bAddedOk = true;
			break;
		}
	}

	// If we just successfully added the player, check if they are #2 so we can restart the round
	if ( bAddedOk )
	{
		uint32 uPlayers = 0;
		for( uint32 i = 0; i < cMaxPlayers; ++i ) 
		{
			if ( _rgClientData[i].m_bActive )
				++uPlayers;
		}

		NeedAcception();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Take any action we need to on Steam notifying us we are now logged in
//-----------------------------------------------------------------------------
void SteamServer::OnSteamServersConnected( SteamServersConnected_t *pLogonSuccess )
{
	if (!_hostInit || !_serverConnecting)
		return;
	_serverConnecting = false;

	SendUpdatedServerDetailsToSteam();
	_service->SendEvent(cSteamHostCreated, &EventData(true));
}

//-----------------------------------------------------------------------------
// Purpose: Called when we were previously logged into steam but get logged out
//-----------------------------------------------------------------------------
void SteamServer::OnSteamServersDisconnected( SteamServersDisconnected_t *pLoggedOff )
{
	if (!_hostInit)
		return;

	ShutdownHost();

	_service->SendEvent(cSteamHostDisconnected, &EventData(false));	
}

//-----------------------------------------------------------------------------
// Purpose: Called when an attempt to login to Steam fails
//-----------------------------------------------------------------------------
void SteamServer::OnSteamServersConnectFailure( SteamServerConnectFailure_t *pConnectFailure )
{
	if (!_hostInit || !_serverConnecting)
		return;

	ShutdownHost();

	_service->SendEvent(cSteamHostCreated, &EventData(false));	
}

//-----------------------------------------------------------------------------
// Purpose: Handle clients connecting
//-----------------------------------------------------------------------------
void SteamServer::OnP2PSessionRequest( P2PSessionRequest_t *pCallback )
{
	// we'll accept a connection from anyone
	SteamGameServerNetworking()->AcceptP2PSessionWithUser( pCallback->m_steamIDRemote );
}


//-----------------------------------------------------------------------------
// Purpose: Handle clients disconnecting
//-----------------------------------------------------------------------------
void SteamServer::OnP2PSessionConnectFail( P2PSessionConnectFail_t *pCallback )
{
	// socket has closed, kick the user associated with it
	for( uint32 i = 0; i < cMaxPlayers; ++i )
	{
		// If there is no ship, skip
		if ( !_rgClientData[i].m_bActive )
			continue;

		if ( _rgClientData[i].m_SteamIDUser == pCallback->m_steamIDRemote )
		{
			LSL_LOG( "Disconnected dropped user\n" );
			RemovePlayerFromServer( i );
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Tells us Steam3 (VAC and newer license checking) has accepted the user connection
//-----------------------------------------------------------------------------
void SteamServer::OnValidateAuthTicketResponse( ValidateAuthTicketResponse_t *pResponse )
{
	if ( pResponse->m_eAuthSessionResponse == k_EAuthSessionResponseOK )
	{
		// This is the final approval, and means we should let the client play (find the pending auth by steamid)
		for ( uint32 i = 0; i < cMaxPlayers; ++i )
		{
			if ( !_rgPendingClientData[i].m_bActive )
				continue;
			else if ( _rgPendingClientData[i].m_SteamIDUser == pResponse->m_SteamID )
			{
				LSL_LOG( "Auth completed for a client\n" );
				OnAuthCompleted( true, i );
				return;
			}
		}
	}
	else
	{
		// Looks like we shouldn't let this user play, kick them
		for ( uint32 i = 0; i < cMaxPlayers; ++i )
		{
			if ( !_rgPendingClientData[i].m_bActive )
				continue;
			else if ( _rgPendingClientData[i].m_SteamIDUser == pResponse->m_SteamID )
			{
				LSL_LOG( "Auth failed for a client\n" );
				OnAuthCompleted( false, i );
				return;
			}
		}
	}
}

void SteamServer::Process(float deltaTime)
{
	if (_hostInit)
	{
		ReceiveNetworkData();

		SteamGameServer_RunCallbacks();
	}
}

bool SteamServer::CreateHost()
{
	if (_hostInit || _serverConnecting)
		return true;

	_hostInit = true;
	_serverConnecting = true;	

	// zero the client connection data
	memset( &_rgClientData, 0, sizeof( _rgClientData ) );
	memset( &_rgPendingClientData, 0, sizeof( _rgPendingClientData ) );

	// Don't let Steam do authentication
	EServerMode eMode = eServerModeAuthenticationAndSecure;

	// Initialize the SteamGameServer interface, we tell it some info about us, and we request support
	// for both Authentication (making sure users own games) and secure mode, VAC running in our game
	// and kicking users who are VAC banned
	if ( !SteamGameServer_Init( INADDR_ANY, cAuthenticationPort, cServerPort, cMasterServerUpdaterPort, eMode, cServerVersion.c_str() ) )
	{
		LSL_LOG( "SteamGameServer_Init call failed\n" );
		return false;
	}

	if ( SteamGameServer() )
	{
		// Set the "game dir".
		// This is currently required for all games.  However, soon we will be
		// using the AppID for most purposes, and this string will only be needed
		// for mods.  it may not be changed after the server has logged on
		SteamGameServer()->SetModDir( cGameDir.c_str() );

		// These fields are currently required, but will go away soon.
		// See their documentation for more info
		SteamGameServer()->SetProduct( cGameDir.c_str() );
		SteamGameServer()->SetGameDescription( "" );
		SteamGameServer()->SetDedicatedServer(true);

		// We don't support specators in our game.
		// .... but if we did:
		//SteamGameServer()->SetSpectatorPort( ... );
		//SteamGameServer()->SetSpectatorServerName( ... );		

		// Initiate Anonymous logon.
		// Coming soon: Logging into authenticated, persistent game server account
		SteamGameServer()->LogOnAnonymous();

		// We want to actively update the master server with our presence so players can
		// find us via the steam matchmaking/server browser interfaces		
		SteamGameServer()->EnableHeartbeats( true );
		//SteamGameServer()->ForceHeartbeat();

		return true;
	}
	else
	{
		LSL_LOG( "SteamGameServer() interface is invalid\n" );

		return false;
	}
}

void SteamServer::ShutdownHost()
{
	if (!_hostInit)
		return;

	_hostInit = false;
	_serverConnecting = false;
	
	// Notify Steam master server we are going offline
	SteamGameServer()->EnableHeartbeats( false );

	for ( unsigned i=0; i < cMaxPlayers; ++i )
	{
		if ( _rgClientData[i].m_bActive )
		{
			// Tell this client we are exiting
			MsgServerExiting_t msg;
			SendDataToClient( i, (char*)&msg, sizeof(msg) );
		}
	}

	// Notify Steam master server we are going offline
	SteamGameServer()->EnableHeartbeats( false );

	// Disconnect from the steam servers
	SteamGameServer()->LogOff();

	// release our reference to the steam client library
	SteamGameServer_Shutdown();
}

bool SteamServer::hostInit() const
{
	return _hostInit;
}

bool SteamServer::serverConnecting() const
{
	return _serverConnecting;
}

bool SteamServer::serverConnected() const
{
	return _hostInit && !_serverConnecting;
}

void SteamServer::FindInternetList()
{
	// If we are still finishing the previous refresh, then ignore this new request
	if (_internetListLoading)
		return;

	// Track that we are now in a refresh, what type of refresh, and reset our server count
	_internetListLoading = true;	
	_hostList.clear();

	// If another request is outstanding, make sure we release it properly
	if ( _serverListRequest )
	{
		SteamMatchmakingServers()->ReleaseRequest( _serverListRequest );
		_serverListRequest = NULL;
	}

	LSL_LOG( "Refreshing internet servers\n" );

	// Allocate some filters, there are some common pre-defined values that can be used:
	//
	// "gamedir" -- this is used to specify mods inside or a single product/appid
	// "secure" -- this is used to specify whether anti-cheat is enabled for a server
	// "gametype" -- this is used to specify game type and is set to whatever your game server code sets

	MatchMakingKeyValuePair_t pFilters[2];
	MatchMakingKeyValuePair_t* pFilter = pFilters;

	strncpy( pFilters[ 0 ].m_szKey, "gamedir", sizeof(pFilters[ 0 ].m_szKey) );
	strncpy( pFilters[ 0 ].m_szValue, cGameDir.c_str(), sizeof(pFilters[ 0 ].m_szValue) );

	strncpy( pFilters[ 1 ].m_szKey, "secure", sizeof(pFilters[ 1 ].m_szKey) );
	strncpy( pFilters[ 1 ].m_szValue, "1", sizeof(pFilters[ 1 ].m_szValue) );

	//strncpy( pFilters[ 2 ].m_szKey, "gametype", sizeof(pFilters[ 1 ].m_szValue) );
	//strncpy( pFilters[ 2 ].m_szValue, "dm", sizeof(pFilters[ 1 ].m_szValue) );

	// bugbug jmccaskey - passing just the appid without filters results in getting all servers rather than
	// servers filtered by appid alone.  So, we'll use the filters to filter the results better.
	_serverListRequest = SteamMatchmakingServers()->RequestInternetServerList( SteamUtils()->GetAppID(), &pFilter, ARRAYSIZE(pFilters), this );
}

// Initiate a refresh of LAN servers
void SteamServer::FindLanServers()
{
	// If we are still finishing the previous refresh, then ignore this new request
	if (_internetListLoading)
		return;

	// Track that we are now in a refresh, what type of refresh, and reset our server count
	_internetListLoading = true;	
	_hostList.clear();

	// If _serverListRequest request is outstanding, make sure we release it properly
	if ( _serverListRequest )
	{
		SteamMatchmakingServers()->ReleaseRequest( _serverListRequest );
		_serverListRequest = NULL;
	}

	LSL_LOG( "Refreshing LAN servers\n" );

	// LAN refresh doesn't accept filters like internet above does
	_serverListRequest = SteamMatchmakingServers()->RequestLANServerList( SteamUtils()->GetAppID(), this );
}

const SteamServer::HostList& SteamServer::hostList() const
{
	return _hostList;
}

void SteamServer::NeedAcception()
{
	for (NetAcceptorSteam::Connections::const_iterator iter = _service->acceptor()->acceptionList().begin(); iter != _service->acceptor()->acceptionList().end();)
	{
		bool isFind = false;

		for (int i = 0; i < cMaxPlayers; ++i)
			if (_rgClientData[i].m_bActive && _rgClientData[i].m_SteamIDUser != SteamUser()->GetSteamID() && _rgClientData[i].connection == NULL)
			{
				_rgClientData[i].connection = *iter;
				iter = _service->acceptor()->RemoveAcception(iter);
				_rgClientData[i].connection->OnAccepted(_rgClientData[i].m_SteamIDUser, true);
				isFind = true;
				break;
			}

			if (!isFind)
				break;
	}
}

CSteamID SteamServer::FindClient(unsigned accountId) const
{
	for (int i = 0; i < cMaxPlayers; ++i)
		if (_rgClientData[i].m_bActive && _rgClientData[i].m_SteamIDUser.GetAccountID() == accountId)
			return _rgClientData[i].m_SteamIDUser;

	return CSteamID();
}

lsl::string SteamServer::GetServerName() const
{
	return lsl::StrFmt("%s's game", SteamFriends()->GetPersonaName());
}

SteamService* SteamServer::service()
{
	return _service;
}

GameMode* SteamServer::game()
{
	return _service->game();
}

CSteamID SteamServer::steamId()
{
	return _hostInit ? SteamGameServer()->GetSteamID() : CSteamID();
}




SteamService::SteamService(GameMode* game): _game(game), _syncStage(cSyncStageEnd), _syncWaitTime(-1.0f), _initiatingConnection(false), _connected(false)
{
	if (_init)
	{
		_gameId = CGameID(SteamUtils()->GetAppID());
		_game->RegUser(this);
	}

	_leaderboard = new SteamLeaderboard(this);
	_steamStats = new SteamStats(this);
	_lobby = new SteamLobby(this);
	_server = new SteamServer(this);
	_acceptor = new NetAcceptorSteam(this);
	// initialize P2P auth engine
	//_pP2PAuthedGame = new CP2PAuthedGame( this );
}

SteamService::~SteamService()
{
	_game->UnregUser(this);

	CloseConnection();

	//if ( _pP2PAuthedGame )
	//{
	//	_pP2PAuthedGame->EndGame();
	//	delete _pP2PAuthedGame;
	//	_pP2PAuthedGame = NULL;
	//}

	delete _acceptor;
	delete _server;
	delete _lobby;
	delete _steamStats;
	delete _leaderboard;
}

bool SteamService::DoSync()
{
	if (_steamStats->GetUserStats())
	{
		_syncWaitTime = -1.0f;
		_syncStage = ssGetUserStats;
		return true;
	}
	else
	{
		DoSyncFailed();
		return false;
	}
}

void SteamService::DoSyncFailed()
{
	_syncStage = cSyncStageEnd;
	_syncWaitTime = cSyncWaitTime;
}

//-----------------------------------------------------------------------------
// Purpose: Send data to the current server
//-----------------------------------------------------------------------------
bool SteamService::SendData( const void *pData, uint32 nSizeOfData )
{
	if ( !SteamNetworking()->SendP2PPacket( _serverSteamId, pData, nSizeOfData, k_EP2PSendReliable ) )
	{
		LSL_LOG( "Failed sending data to server\n" );

		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Receive basic server info from the server after we initiate a connection
//-----------------------------------------------------------------------------
void SteamService::ReceiveServerInfo( CSteamID steamIDGameServer, bool bVACSecure, const char *pchServerName )
{
	_serverSteamId = steamIDGameServer;

	// look up the servers IP and Port from the connection
	P2PSessionState_t p2pSessionState;
	SteamNetworking()->GetP2PSessionState( steamIDGameServer, &p2pSessionState );
	_hostIP = p2pSessionState.m_nRemoteIP;
	_hostPort = p2pSessionState.m_nRemotePort;

	// set how to connect to the game server, using the Rich Presence API
	// this lets our friends connect to this game via their friends list
	UpdateRichPresenceConnectionInfo();

	MsgClientBeginAuthentication_t msg;

	char rgchToken[1024];
	uint32 unTokenLen = 0;
	_hAuthTicket = SteamUser()->GetAuthSessionTicket( rgchToken, sizeof( rgchToken ), &unTokenLen );
	msg.SetToken( rgchToken, unTokenLen );

	if ( msg.GetTokenLen() < 1 )
		LSL_LOG( "Warning: Looks like GetAuthSessionTicket didn't give us a good ticket\n" );

	SendData( &msg, sizeof(msg) );
}

//-----------------------------------------------------------------------------
// Purpose: Handle the server telling us it is exiting
//-----------------------------------------------------------------------------
void SteamService::ReceiveServerExiting()
{
	if ( _hAuthTicket != k_HAuthTicketInvalid )
	{
		SteamUser()->CancelAuthTicket( _hAuthTicket );
	}

	_hAuthTicket = k_HAuthTicketInvalid;
}

void SteamService::DisconnectFromServer()
{
	if ( _connected || _initiatingConnection )
	{
		if ( _hAuthTicket != k_HAuthTicketInvalid )
			SteamUser()->CancelAuthTicket( _hAuthTicket );
		_hAuthTicket = k_HAuthTicketInvalid;

		MsgClientLeavingServer_t msg;
		SendData( &msg, sizeof(msg) );		
	}

	//if ( _pP2PAuthedGame )
	//{
	//	_pP2PAuthedGame->EndGame();
	//}

	// forget the game server ID
	if ( _serverSteamId.IsValid() )
	{
		SteamNetworking()->CloseP2PSessionWithUser( _serverSteamId );
		_serverSteamId = CSteamID();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Updates what we show to friends about what we're doing and how to connect
//-----------------------------------------------------------------------------
void SteamService::UpdateRichPresenceConnectionInfo()
{
	// connect string that will come back to us on the command line	when a friend tries to join our game
	lsl::string rgchConnectString;

	if ( _lobby->currentLobby().IsValid() )
	{
		// lobby connection method
		rgchConnectString = lsl::StrFmt( "+connect_lobby %llu", _lobby->currentLobby().ConvertToUint64() );
	}
	else if ( _server->serverConnected() )
	{
		// game server connection method
		rgchConnectString = lsl::StrFmt( "+connect %d:%d", _hostIP, _hostPort );
	}

	SteamFriends()->SetRichPresence( "connect", rgchConnectString.c_str() );
}

//-----------------------------------------------------------------------------
// Purpose: Receive an authentication response from the server
//-----------------------------------------------------------------------------
void SteamService::ReceiveServerAuthenticationResponse( bool bSuccess, uint32 uPlayerPosition )
{
	if ( !bSuccess )
	{
		//SetConnectionFailureText( "Connection failure.\nMultiplayer authentication failed\n" );
		//SetGameState( k_EClientGameConnectionFailure );
		CloseConnection();

		SendEvent(cSteamAuthEnded, &EventData(false));
	}
	else
	{
		// Is this a duplicate message? If so ignore it...
		if ( !_initiatingConnection )
			return;

		// set information so our friends can join the lobby
		UpdateRichPresenceConnectionInfo();

		_initiatingConnection = false;
		_connected = true;
		NeedConnection();

		SendEvent(cSteamAuthEnded, &EventData(true));

		// send a ping, to measure round-trip time
		//m_ulPingSentTime = m_pGameEngine->GetGameTickCount();
		//MsgClientPing_t msg;
		//BSendServerData( &msg, sizeof( msg ) );
	}
}

void SteamService::ReceiveNetworkData()
{
	char rgchRecvBuf[1024];
	char *pchRecvBuf = rgchRecvBuf;
	uint32 cubMsgSize;

	for (;;)
	{
		// reset the receive buffer
		if ( pchRecvBuf != rgchRecvBuf )
		{
			free( pchRecvBuf );
			pchRecvBuf = rgchRecvBuf;
		}

		// see if there is any data waiting on the socket
		if ( !SteamNetworking()->IsP2PPacketAvailable( &cubMsgSize ) )
			break;

		// not enough space in default buffer
		// alloc custom size and try again
		if ( cubMsgSize > sizeof(rgchRecvBuf) )
		{
			pchRecvBuf = (char *)malloc( cubMsgSize );
		}
		CSteamID steamIDRemote;
		if ( !SteamNetworking()->ReadP2PPacket( pchRecvBuf, cubMsgSize, &cubMsgSize, &steamIDRemote ) )
			break;

		// see if it's from the game server
		if ( steamIDRemote == _serverSteamId )
		{
			if ( cubMsgSize < sizeof( DWORD ) )
			{
				LSL_LOG( "Got garbage on client socket, too short\n" );
			}

			EMessage eMsg = (EMessage)*(DWORD*)pchRecvBuf;

			switch ( eMsg )
			{
			case k_EMsgServerSendInfo:
				if (_initiatingConnection)
				{
					if ( cubMsgSize != sizeof( MsgServerSendInfo_t ) )
					{
						LSL_LOG ("Bad server info msg\n" );
						continue;
					}
					MsgServerSendInfo_t *pMsg = (MsgServerSendInfo_t*)pchRecvBuf;

					// pull the IP address of the user from the socket
					ReceiveServerInfo( CSteamID( pMsg->GetSteamIDServer() ), pMsg->GetSecure(), pMsg->GetServerName() );
				}
				break;

			case k_EMsgServerExiting:
				if (_initiatingConnection)
				{
					if ( cubMsgSize != sizeof( MsgServerExiting_t ) )
					{
						LSL_LOG( "Bad server exiting msg\n" );
					}
					ReceiveServerExiting();
				}
				break;

			case k_EMsgServerPassAuthentication:
				if (_initiatingConnection)
				{
					if ( cubMsgSize != sizeof( MsgServerPassAuthentication_t ) )
					{
						LSL_LOG( "Bad accept connection msg\n" );
						continue;
					}
					MsgServerPassAuthentication_t *pMsg = (MsgServerPassAuthentication_t*)pchRecvBuf;

					// Our game client doesn't really care about whether the server is secure, or what its 
					// steamID is, but if it did we would pass them in here as they are part of the accept message
					ReceiveServerAuthenticationResponse( true, pMsg->GetPlayerPosition() );
				}
				break;

			case k_EMsgServerFailAuthentication:
				if (_initiatingConnection)
				{
					ReceiveServerAuthenticationResponse( false, 0 );
				}
				break;
			
			default:
				_acceptor->HandleMessage( steamIDRemote, eMsg, (const BYTE*)pchRecvBuf, cubMsgSize );
				break;
			}
		}
		else 
		{
			if ( cubMsgSize >= sizeof( DWORD ) )
			{
				EMessage eMsg = (EMessage)*(DWORD*)pchRecvBuf;

				_acceptor->HandleMessage( steamIDRemote, eMsg, (const BYTE*)pchRecvBuf, cubMsgSize );
			
				//if ( _pP2PAuthedGame->HandleMessage( eMsg, pchRecvBuf ) )
				//	continue; // this was a P2P auth message
			}

			// the message is from another player			
			//if ( _acceptor->HandleMessage( steamIDRemote, (BYTE*)pchRecvBuf, cubMsgSize ) )
			//	continue; // this was a P2P auth message

			// Unhandled message
			LSL_LOG( "Received unknown message on our listen socket\n" );
		}
	}
}

void SteamService::OnProcessEvent(unsigned id, EventData* data)
{
	if (!_init)
		return;

	_steamStats->OnProcessEvent(id, data);
}

void SteamService::Process(float deltaTime)
{
	if (_init)
	{
		SteamAPI_RunCallbacks();

		ReceiveNetworkData();

		_server->Process(deltaTime);
	}

	if (_syncStage == cSyncStageEnd && _syncWaitTime >= 0.0f && (_syncWaitTime -= deltaTime) <= 0.0f)
	{
		DoSync();
	}
}

void SteamService::Sync(float waitTime, bool immediately)
{
	if (!_init)
		return;

	if (_syncStage != cSyncStageEnd || (!immediately && _syncWaitTime >= 0))
	{
		_syncWaitTime = std::max(_syncWaitTime, waitTime);
		return;
	}

	DoSync();
}

bool SteamService::syncInProcess() const
{
	return _syncStage != cSyncStageEnd || _syncWaitTime >= 0;
}

unsigned SteamService::time() const
{
	return _game->time();
}

void SteamService::SendEvent(unsigned id, EventData* data)
{
	switch (id)
	{
	case cSteamGetUserStats:
		if (_syncStage == ssGetUserStats)
		{
			if (data->success && _steamStats->StoreUserStats())
				_syncStage = ssStoreUserStats;
			else
				DoSyncFailed();
		}
		break;

	case cSteamStoreUserStats:
		if (_syncStage == ssStoreUserStats)
		{
			if (data->success && _leaderboard->FindLeaders())
				_syncStage = ssGetLeaders;
			else if (data->success && _leaderboard->UploadUserStats())
				_syncStage = ssStoreLeaders;
			else
				DoSyncFailed();
		}
		return;

	case cSteamGetLeaders:
		if (_syncStage == ssGetLeaders)
		{
			if (data->success && _leaderboard->UploadUserStats())
				_syncStage = ssStoreLeaders;
			else
				DoSyncFailed();
		}
		return;

	case cSteamStoreLeaders:
		if (_syncStage == ssStoreLeaders)
		{
			if (data->success)
				_syncStage = cSyncStageEnd;
			else
				DoSyncFailed();
		}
		return;
	}

	_lobby->ProcessEvent(id, data);

	_game->SendEvent(id, data);
}

bool SteamService::InitiateConnection(CSteamID steamIDGameServer)
{
	_serverSteamId = steamIDGameServer;

	//send the packet to the server
	MsgClientInitiateConnection_t msg;
	
	if (SendData( &msg, sizeof( msg ) ))
	{
		_initiatingConnection = true;
		return true;
	}

	return false;
}

void SteamService::CloseConnection()
{
	DisconnectFromServer();

	_connected = false;
	_initiatingConnection = false;
}

void SteamService::NeedConnection()
{
	if (_connected)
	{
		for (NetAcceptorSteam::Connections::const_iterator iter = acceptor()->connectionList().begin(); iter != acceptor()->connectionList().end();)
		{
			(*iter)->OnConnected(_serverSteamId, true);
			iter = acceptor()->RemoveConnection(iter);
		}
	}
}

bool SteamService::initiatingConnection() const
{
	return _initiatingConnection;
}

CSteamID SteamService::serverSteamId() const
{
	return _serverSteamId;
}

int SteamService::hostIP() const
{
	return _hostIP;
}

int SteamService::hostPort() const
{
	return _hostPort;
}

GameMode* SteamService::game()
{
	return _game;
}

const CGameID& SteamService::gameId()
{
	return _gameId;
}

SteamLeaderboard* SteamService::leaderboard()
{
	return _leaderboard;
}

SteamStats* SteamService::steamStats()
{
	return _steamStats;
}

SteamLobby* SteamService::lobby()
{
	return _lobby;
}

SteamServer* SteamService::server()
{
	return _server;
}

NetAcceptorSteam* SteamService::acceptor()
{
	return _acceptor;
}

SteamService::InitRes SteamService::Initialize()
{
	if (_init)
		return irSuccess;	

#ifndef _RETAIL
	const int cAppId = 263260;	
	if ( SteamAPI_RestartAppIfNecessary( k_uAppIdInvalid ) )
	{
		// if Steam is not running or the game wasn't started through Steam, SteamAPI_RestartAppIfNecessary starts the 
		// local Steam client and also launches this game again.

		// Once you get a public Steam AppID assigned for this game, you need to replace k_uAppIdInvalid with it and
		// removed steam_appid.txt from the game depot.

		return irRestart;
	}
#endif

	// Initialize SteamAPI, if this fails we bail out since we depend on Steam for lots of stuff.
	// You don't necessarily have to though if you write your code to check whether all the Steam
	// interfaces are NULL before using them and provide alternate paths when they are unavailable.
	//
	// This will also load the in-game steam overlay dll into your process.  That dll is normally
	// injected by steam when it launches games, but by calling this you cause it to always load,
	// even when not launched via steam.
	if ( !SteamAPI_Init() )
	{
		LSL_LOG( "SteamAPI_Init() failed" );
		LSL_LOG( "Steam must be running to play this game (SteamAPI_Init() failed).\n" );

		return irFailed;
	}

	if (!SteamUser()->GetSteamID().IsValid())
	{
		LSL_LOG( "!SteamUser()->GetSteamID().IsValid()" );

		SteamAPI_Shutdown();

		return irFailed;
	}

	_init = true;

	// set our debug handler
	SteamClient()->SetWarningMessageHook( &SteamAPIDebugTextHook );

	// Tell Steam where it's overlay should show notification dialogs, this can be top right, top left,
	// bottom right, bottom left. The default position is the bottom left if you don't call this.  
	// Generally you should use the default and not call this as users will be most comfortable with 
	// the default position.  The API is provided in case the bottom right creates a serious conflict 
	// with important UI in your game.
	SteamUtils()->SetOverlayNotificationPosition( k_EPositionTopRight );

	//const char *pchServerAddress, *pchLobbyID;
	//ParseCommandLine( pchCmdLine, &pchServerAddress, &pchLobbyID );	

	return irSuccess;
}

void SteamService::Finalize()
{
	if (!_init)
		return;
	_init = false;

	// Shutdown the SteamAPI
	SteamAPI_Shutdown();
}

bool SteamService::isInit()
{
	return _init;
}

}

}

#endif