#pragma once

#include "NetBase.h"
#include "NetPlayer.h"
#include "NetRace.h"

namespace r3d
{

namespace game
{

class NetGame: net::INetServiceUser
{	
	friend NetPlayer;
	friend NetRace;
private:
	typedef lsl::Container<INetGameUser*> Users;

	static NetGame* _i;
public:
	typedef List<NetPlayer*> NetPlayers;
private:
	GameMode* _game;
	Users _users;
	NetRace* _race;

	NetPlayers _players;
	NetPlayers _netPlayers;
	NetPlayers _netOpponents;
	NetPlayers _aiPlayers;
	NetPlayer* _player;

	int _port;
	bool _started;
	bool _isHost;
	bool _isClient;	

	void RegPlayer(NetPlayer* player);
	void UnregPlayer(NetPlayer* player);

	void RegRace(NetRace* race);
	void UnregRace(NetRace* race);
protected:	
	virtual bool OnConnected(net::INetConnection* sender);
	virtual void OnDisconnected(net::INetConnection* sender);
	virtual void OnConnectionFailed(net::INetConnection* sender, unsigned error);
	virtual void OnReceiveCmd(const net::NetMessage& msg, const net::NetCmdHeader& header, const void* data, unsigned size);
	virtual void OnPingComplete();
	virtual void OnFailed(unsigned error);
public:
	NetGame(GameMode* game);
	~NetGame();

	void RegUser(INetGameUser* user);
	void UnregUser(INetGameUser* user);

	void Initializate();
	void Finalizate();
	void PingHosts();
	void CancelPing();
	bool IsPingProcess();

	void CreateHost(net::INetAcceptorImpl* impl);
	bool Connect(const net::Endpoint& endpoint, bool useDefaultPort, net::INetAcceptorImpl* impl);
	void Close();

	int port() const;
	bool isStarted() const;
	bool isHost() const;
	bool isClient() const;	

	void Process(unsigned time, float deltaTime);	
	void SendEvent(unsigned id, NetEventData* data = NULL);

	void DisconnectPlayer(NetPlayer* player);
	bool AllPlayersReady();

	NetPlayer* GetPlayer(unsigned plrId);
	NetPlayer* GetPlayerByOwnerId(int id);
	NetPlayer* GetPlayerByLocalId(int id);
	NetPlayer* GetPlayer(Player* plr);
	NetPlayer* GetPlayer(MapObj* mapObj);

	NetPlayer* player();
	const NetPlayers& players() const;
	const NetPlayers& netPlayers() const;
	const NetPlayers& netOpponents() const;
	const NetPlayers& aiPlayers() const;	

	GameMode* game();
	NetRace* race();

	net::INetService& netService();
	net::INetPlayer* netPlayer();

	bool GetAdapterAddresses(lsl::StringVec& addrVec);

	static NetGame* I();
};

}

}