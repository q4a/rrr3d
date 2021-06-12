#include "stdafx.h"

#include "net\NetGame.h"
#include "game\GameMode.h"

namespace r3d
{

namespace game
{

NetGame* NetGame::_i;




NetGame::NetGame(GameMode* game): _game(game), _race(NULL), _port(58213), _started(false), _isHost(false), _isClient(false), _player(NULL)
{
	_i = this;

	netService().modelClasses().Add<NetRace>(1);
	netService().modelClasses().Add<NetPlayer>(2);
	//netService().modelClasses().Add<NetPlayerResponse>(3);

	netService().syncRate(70);
	netService().user(this);
	netService().profiler(&Profiler::I());
}

NetGame::~NetGame()
{
	netService().user(NULL);

	Close();

	for (Users::const_iterator iter = _users.begin(); iter != _users.end(); ++iter)
		(*iter)->Release();
	_users.Clear();
}

void NetGame::RegUser(INetGameUser* user)
{
	if (_users.IsFind(user))
		return;

	user->AddRef();
	_users.Insert(user);
}

void NetGame::UnregUser(INetGameUser* user)
{
	if (!_users.IsFind(user))
		return;

	_users.Remove(user);
	user->Release();
}

void NetGame::RegPlayer(NetPlayer* player)
{
	_players.push_back(player);

	if (player->owner() && player->model()->GetId() == Race::cHuman)
		_player = player;

	if (player->model()->IsHuman() || player->model()->IsOpponent())
		_netPlayers.push_back(player);
	else
		_aiPlayers.push_back(player);

	if (player->model()->IsOpponent())
		_netOpponents.push_back(player);

	for (Users::Position pos = _users.First(); INetGameUser** iter = _users.Current(pos); _users.Next(pos))
		(*iter)->OnConnectedPlayer(player);
}

void NetGame::UnregPlayer(NetPlayer* player)
{
	_aiPlayers.Remove(player);
	_netOpponents.Remove(player);
	_netPlayers.Remove(player);
	_players.Remove(player);

	if (_player == player)
		_player = NULL;

	for (Users::Position pos = _users.First(); INetGameUser** iter = _users.Current(pos); _users.Next(pos))
		(*iter)->OnDisconnectedPlayer(player);
}

void NetGame::RegRace(NetRace* race)
{
	LSL_ASSERT(_race == NULL);

	_race = race;
}

void NetGame::UnregRace(NetRace* race)
{
	if (_race == race)
		_race = NULL;
}

bool NetGame::OnConnected(net::INetConnection* sender)
{
	bool res = true;

	for (Users::Position pos = _users.First(); INetGameUser** iter = _users.Current(pos); _users.Next(pos))
		res &= (*iter)->OnConnected(sender);

	return res;
}

void NetGame::OnDisconnected(net::INetConnection* sender)
{
	for (Users::Position pos = _users.First(); INetGameUser** iter = _users.Current(pos); _users.Next(pos))
		(*iter)->OnDisconnected(sender);

	if (_isClient)
		Close();
}

void NetGame::OnConnectionFailed(net::INetConnection* sender, unsigned error)
{
	if (_isClient)
		Close();

	for (Users::Position pos = _users.First(); INetGameUser** iter = _users.Current(pos); _users.Next(pos))
		(*iter)->OnConnectionFailed(sender, error);
}

void NetGame::OnReceiveCmd(const net::NetMessage& msg, const net::NetCmdHeader& header, const void* data, unsigned size)
{
	for (Users::Position pos = _users.First(); INetGameUser** iter = _users.Current(pos); _users.Next(pos))
		(*iter)->OnReceiveCmd(msg, header, data, size);
}

void NetGame::OnPingComplete()
{
	for (Users::Position pos = _users.First(); INetGameUser** iter = _users.Current(pos); _users.Next(pos))
		(*iter)->OnPingComplete();
}

void NetGame::OnFailed(unsigned error)
{
	Close();

	for (Users::Position pos = _users.First(); INetGameUser** iter = _users.Current(pos); _users.Next(pos))
		(*iter)->OnFailed(error);
}

void NetGame::Initializate()
{
	netService().Initializate();
}

void NetGame::Finalizate()
{
	Close();

	netService().Finalizate();
}

void NetGame::PingHosts()
{
#if _DEBUG
	netService().Ping(_port, 500, 250);
#else
	netService().Ping(_port, 3000, 500);
#endif
}

void NetGame::CancelPing()
{
	netService().CancelPing();
}

bool NetGame::IsPingProcess()
{
	return netService().IsPingProcess();
}

void NetGame::CreateHost(net::INetAcceptorImpl* impl)
{
	if (_isHost)
		return;
	Close();
	_started = true;
	_isHost = true;

	lsl::appLog.fileName = "appLog.txt";
	
	netService().StartServer(_port, impl);
	netPlayer()->MakeModel<NetRace>();
}

bool NetGame::Connect(const net::Endpoint& endpoint, bool useDefaultPort, net::INetAcceptorImpl* impl)
{
	if (_isClient)
		return false;

	Close();
	_started = true;
	_isClient = true;
	net::Endpoint hostEndpoint = endpoint;

	if (useDefaultPort)
		hostEndpoint.port = _port;

	//lsl::appLog.fileName = "clientLog.txt";

	if (!netService().Connect(hostEndpoint, impl))
	{
		Close();
		return false;
	}

	return true;
}

void NetGame::Close()
{
	if (!isStarted())
		return;
	_started = false;

	_aiPlayers.clear();
	_netPlayers.clear();
	_netOpponents.clear();
	_players.clear();
	_player = NULL;

	_race = NULL;

	netService().Close();	

	_isHost = false;
	_isClient = false;
}

int NetGame::port() const
{
	return _port;
}

bool NetGame::isStarted() const
{
	return _started;
}

bool NetGame::isHost() const
{
	return _isHost;
}

bool NetGame::isClient() const
{
	return _isClient;
}

void NetGame::Process(unsigned time, float deltaTime)
{
	Profiler::I().Begin("net");

	if (netService().IsInit())
	{
		netService().Process(time);

		for (NetPlayers::iterator iter = _players.begin(); iter != _players.end(); ++iter)
			(*iter)->Process(deltaTime);
	}

	Profiler::I().End();
}

void NetGame::SendEvent(unsigned id, NetEventData* data)
{
	for (Users::Position pos = _users.First(); INetGameUser** iter = _users.Current(pos); _users.Next(pos))
		(*iter)->OnProcessNetEvent(id, data);
}

void NetGame::DisconnectPlayer(NetPlayer* player)
{
	net::INetConnection* connection = netService().GetConnectionById(player->ownerId());

	LSL_ASSERT(connection);

	netService().Disconnect(connection);
}

bool NetGame::AllPlayersReady()
{
	for (NetPlayers::const_iterator iter = _netOpponents.begin(); iter != _netOpponents.end(); ++iter)
		if (!(*iter)->IsRaceReady())
			return false;
	return true;
}

NetPlayer* NetGame::GetPlayer(unsigned plrId)
{
	for (NetPlayers::iterator iter = _players.begin(); iter != _players.end(); ++iter)
		if ((*iter)->id() == plrId)
			return *iter;
	return NULL;
}

NetPlayer* NetGame::GetPlayerByOwnerId(int id)
{
	for (NetPlayers::iterator iter = _players.begin(); iter != _players.end(); ++iter)
		if ((*iter)->ownerId() == id)
			return *iter;
	return NULL;
}

NetPlayer* NetGame::GetPlayerByLocalId(int id)
{
	for (NetPlayers::iterator iter = _players.begin(); iter != _players.end(); ++iter)
		if ((*iter)->model()->GetId() == id)
			return *iter;
	return NULL;
}

NetPlayer* NetGame::GetPlayer(Player* plr)
{
	if (plr == NULL)
		return NULL;

	for (NetPlayers::iterator iter = _players.begin(); iter != _players.end(); ++iter)
		if ((*iter)->model() == plr)
			return *iter;
	return NULL;
}

NetPlayer* NetGame::GetPlayer(MapObj* mapObj)
{
	return mapObj ? GetPlayer(game()->GetRace()->GetPlayerByMapObj(mapObj)) : NULL;
}

NetPlayer* NetGame::player()
{
	return _player;
}

const NetGame::NetPlayers& NetGame::players() const
{
	return _players;
}

const NetGame::NetPlayers& NetGame::netPlayers() const
{
	return _netPlayers;
}

const NetGame::NetPlayers& NetGame::netOpponents() const
{
	return _netOpponents;
}

const NetGame::NetPlayers& NetGame::aiPlayers() const
{
	return _aiPlayers;
}

GameMode* NetGame::game()
{
	return _game;
}

NetRace* NetGame::race()
{
	return _race;
}

net::INetService& NetGame::netService()
{
	return net::GetNetService();
}

net::INetPlayer* NetGame::netPlayer()
{
	return netService().player();
}

bool NetGame::GetAdapterAddresses(lsl::StringVec& addrVec)
{
	return netService().GetAdapterAddresses(addrVec);
}

NetGame* NetGame::I()
{
	return _i;
}

}

}