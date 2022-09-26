#pragma once

#include "NetBase.h"
#include "NetPlayer.h"
#include "NetRace.h"

namespace r3d
{
	namespace game
	{
		class NetGame : net::INetServiceUser
		{
			friend NetPlayer;
			friend NetRace;
		private:
			using Users = Container<INetGameUser*>;

			static NetGame* _i;
		public:
			using NetPlayers = List<NetPlayer*>;
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
			bool OnConnected(net::INetConnection* sender) override;
			void OnDisconnected(net::INetConnection* sender) override;
			void OnConnectionFailed(net::INetConnection* sender, unsigned error) override;
			void OnReceiveCmd(const net::NetMessage& msg, const net::NetCmdHeader& header, const void* data,
			                  unsigned size) override;
			void OnPingComplete() override;
			void OnFailed(unsigned error) override;
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
			void SendEvent(unsigned id, NetEventData* data = nullptr);

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

			bool GetAdapterAddresses(StringVec& addrVec);

			static NetGame* I();
		};
	}
}
