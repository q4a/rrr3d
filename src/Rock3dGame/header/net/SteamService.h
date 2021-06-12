#pragma once

#ifdef STEAM_SERVICE

#include "SteamCommon.h"
#include "p2pauth.h"

namespace r3d
{

namespace game
{

class GameMode;
class NetAcceptorSteam;

class NetConnectionSteam: public net::INetConnectionImpl
{
private:
	NetAcceptorSteam* _owner;
	CSteamID _steamId;
	bool _isOpen;	
public:
	NetConnectionSteam(NetAcceptorSteam* owner);
	~NetConnectionSteam();

	virtual bool Send(const void* data, unsigned size);
	virtual bool Receive(void* data, unsigned size, unsigned& numBytes);
	virtual bool IsAvailable(unsigned& size);	

	virtual void Accept();
	virtual bool Connect(const net::Endpoint& endpoint);
	virtual void Close();
	virtual bool isOpen() const;

	virtual net::Endpoint localEndpoint();
	virtual net::Endpoint remoteEndpoint();
	virtual lsl::string userName() const;

	void OnAccepted(CSteamID steamId, bool success);
	void OnConnected(CSteamID steamId, bool success);
	void OnSendCmd(bool success, std::size_t numBytes);

	SteamService* service();
};

class NetChannelSteam: public net::INetChannelImpl
{
private:
	NetAcceptorSteam* _owner;	
	bool _isOpen;
	bool _isBind;
	net::Endpoint _bindEndpoint;
public:
	NetChannelSteam(NetAcceptorSteam* owner);
	~NetChannelSteam();

	bool Send(const net::Endpoint& endpoint, const void* data, unsigned size);
	bool Receive(void* data, unsigned size, unsigned& numBytes, net::Endpoint& remoteEndpoint, bool fromOutput);
	bool IsAvailable(unsigned& size, bool fromOutput);
	void Flush();

	void Open(bool broadcast);
	void Bind(const net::Endpoint& endpoint);
	void Unbind();
	void Close();
	bool IsOpen() const;
	bool IsBind() const;
	bool broadcast() const;
	net::Endpoint bindEndpoint() const;

	net::Endpoint localEndpoint();

	void OnSend(bool success, std::size_t numBytes);

	SteamService* service();
};

class NetAcceptorSteam: public net::INetAcceptorImpl
{	
public:
	typedef lsl::List<NetConnectionSteam*> Connections;
	typedef std::map<CSteamID, std::pair<net::IStreamBuf*, net::IStreamBuf*>> PacketMap;
private:
	SteamService* _service;
	bool _isOpen;
	PacketMap _packetMap;
	Connections _acceptionList;
	Connections _connectionList;
	net::IStreamBuf* _steam;
public:
	NetAcceptorSteam(SteamService* service);
	~NetAcceptorSteam();

	void Listen(const net::Endpoint& endpoint);
	void Close();

	net::INetConnectionImpl* NewConnection();
	void ReleaseConnection(net::INetConnectionImpl* connection);

	net::INetChannelImpl* NewChannel();
	void ReleaseChannel(net::INetChannelImpl* channel);

	void NewAcception(NetConnectionSteam* connection);
	Connections::const_iterator RemoveAcception(Connections::const_iterator iter);
	void RemoveAcception(NetConnectionSteam* connection);
	const Connections& acceptionList() const;

	void NewConnection(NetConnectionSteam* connection);
	Connections::const_iterator RemoveConnection(Connections::const_iterator iter);
	void RemoveConnection(NetConnectionSteam* connection);
	const Connections& connectionList() const;

	void SendMessage(EMessage msg, const CSteamID& steamId, const BYTE* data, unsigned size);
	bool HandleMessage(const CSteamID& steamId, EMessage msg, const BYTE* data, unsigned size);
	net::IStreamBuf* GetOrCreatePacket(EMessage msg, const CSteamID& steamId);
	net::IStreamBuf* GetPacket(EMessage msg, const CSteamID& steamId);
	const PacketMap& GetPacketMap() const;

	SteamService* service();
};

class SteamStats
{
private:
	struct Stat
	{	
		Stat(): value(0), dValue(0) {}

		int value;
		int dValue;
	};

	struct Achievment
	{
		Achievment(): unlocked(false), loaded(false) {}

		bool unlocked;
		bool loaded;
	};
public:
	enum StatType { stPoints = 0, stMoney, stNumGames, stNumWins, stNumKills, stNumDeaths, cStatTypeEnd };
	static const lsl::string cStatName[cStatTypeEnd];

	enum AchievmentType { atWinner = 0, atWins10, atWins25, atWins50, atWins100, atKills1, atKills10, atKills25, atKills50, atKills100, atLoveAndPeace, atUndead, atWellWhoDoesNotHappen, atSeeYouSoon, atWhereNoManHasGoneBefore, atNiceStart, atSpeedKill, atWeAreTheChampions, atOldSchool, atEasyChampion, atNormalChampion, atHardChampion, cAchievmentTypeEnd };
	static const lsl::string cAchievmentName[cAchievmentTypeEnd];
	static StatType cAchievmentStat[cAchievmentTypeEnd];
	static const unsigned cAchievmentStep[cAchievmentTypeEnd];
	static const std::pair<unsigned, unsigned> cAchievmentMinMax[cAchievmentTypeEnd];
private:
	SteamService* _service;
	Stat _stats[cStatTypeEnd];
	Achievment _achievments[cAchievmentTypeEnd];

	bool _userStatsLoading;
	bool _userStatsSaving;

	int _raceNumShots;
	lsl::List<int> _lapKills;

	STEAM_CALLBACK( SteamStats, OnUserStatsReceived, UserStatsReceived_t, _callbackUserStatsReceived );
	STEAM_CALLBACK( SteamStats, OnUserStatsStored, UserStatsStored_t, _callbackUserStatsStored );	
public:
	SteamStats(SteamService* service);
	~SteamStats();

	bool GetUserStats();
	bool StoreUserStats();	

	void AddStat(StatType type, int value);	
	int GetStatValue(StatType type) const;

	void IndicateAchievment(AchievmentType type, int curProgress);
	void UnlockAchievment(AchievmentType type);
	void ResetAll();

	void OnProcessEvent(unsigned id, EventData* data);

	SteamService* service();
	GameMode* game();
};

class SteamLeaderboard
{
private:
	struct Leaders
	{	
		Leaders(): board(0) {}

		SteamLeaderboard_t board;
		CCallResult<SteamLeaderboard, LeaderboardScoreUploaded_t> callResultUploadUsersStats;
	};
public:
	enum LeadersType {ltPoints = 0, ltMoney, ltNumGames, ltNumWins, ltNumKills, ltNumDeaths, cLeadersTypeEnd};
	static const SteamStats::StatType cLeadersStat[cLeadersTypeEnd];
	static const lsl::string cLeadersName[cLeadersTypeEnd];
private:
	SteamService* _service;

	Leaders _leaders[cLeadersTypeEnd];
	bool _leadersLoading;
	int _userStatsUploading;

	CCallResult<SteamLeaderboard, LeaderboardFindResult_t> _callResultFindLeaderboard;	

	// Called when SteamUserStats()->FindOrCreateLeaderboard() returns asynchronously
	void OnFindLeaderboard(LeaderboardFindResult_t* pFindLeaderboardResult, bool bIOFailure);
	// Called when SteamUserStats()->UploadLeaderboardScore() returns asynchronously
	void OnUploadUserStats(LeaderboardScoreUploaded_t* pFindLeaderboardResult, bool bIOFailure);
public:
	SteamLeaderboard(SteamService* service);
	~SteamLeaderboard();

	bool FindLeaders();
	bool UploadUserStats();	

	SteamService* service();
	GameMode* game();
};

class SteamLobby
{
public:
	struct Lobby
	{
		CSteamID id;
		std::string name;		
	};
	typedef std::vector<Lobby> LobbyList;

	struct LobbyMember
	{
		CSteamID id;
		std::string name;
		bool ready;
	};
	typedef std::vector<LobbyMember> MemberList;

	struct MyEventData: EventData
	{
		CSteamID steamId;

		MyEventData(bool mSuccess, CSteamID mSteamId): EventData(mSuccess), steamId(mSteamId) { }
	};
private:
	SteamService* _service;

	LobbyList _lobbyList;
	MemberList _lobbyMemberList;
	CSteamID _currentLobby;
	bool _lobbyListLoading;
	bool _lobbyLoading;
	bool _matchStarting;
	bool _lobbyCreated;

	CCallResult<SteamLobby, LobbyMatchList_t> _callResultLobbyMatchList;
	CCallResult<SteamLobby, LobbyEnter_t> _callResultLobbyEntered;
	CCallResult<SteamLobby, LobbyCreated_t> _callResultLobbyCreated;

	std::string GetLobbyName(CSteamID id);
	void SetLobbdyName(CSteamID id, const std::string& name);

	bool GetLobbyMemberReady(CSteamID id) const;
	void SetLobbyMemberReady(bool state);

	void UpdateLobbyMembers();

	void OnLobbyMatchListCallback(LobbyMatchList_t* pLobbyMatchList, bool bIOFailure);
	void OnLobbyEntered(LobbyEnter_t* pCallback, bool bIOFailure);
	void OnLobbyCreated(LobbyCreated_t *pCallback, bool bIOFailure);

	STEAM_CALLBACK( SteamLobby, OnLobbyDataUpdatedCallback, LobbyDataUpdate_t, _callbackLobbyDataUpdated );
	// user state change handler
	STEAM_CALLBACK( SteamLobby, OnPersonaStateChange, PersonaStateChange_t, _callbackPersonaStateChange );
	//
	STEAM_CALLBACK( SteamLobby, OnLobbyChatUpdate, LobbyChatUpdate_t, _callbackChatDataUpdate );
	STEAM_CALLBACK( SteamLobby, OnLobbyGameCreated, LobbyGameCreated_t, _callbackLobbyGameCreated );
public:
	SteamLobby(SteamService* service);
	~SteamLobby();

	void ProcessEvent(int id, EventData* data);

	void RequestLobbyList();
	const LobbyList& GetLobbyList() const;

	void JoinLobby(CSteamID id);
	void CreateLobby(ELobbyType type, int maxMembers);
	void LeaveLobby();

	void ReadyLobby(bool ready);
	bool isReadyLobby() const;
	bool lobbyCreated() const;

	bool StartMath();

	const MemberList& GetLobbyMembers() const;
	CSteamID currentLobby() const;	

	SteamService* service();
	GameMode* game();
};

class SteamServer: ISteamMatchmakingServerListResponse
{
public:
	struct Host
	{
		gameserveritem_t* inst;
	};

	struct ClientConnectionData_t
	{
		bool m_bActive;					// Is this slot in use? Or is it available for new connections?
		CSteamID m_SteamIDUser;			// What is the steamid of the player?
		uint64 m_ulTickCountLastData;	// What was the last time we got data from the player?
		NetConnectionSteam* connection;
	};

	typedef std::vector<Host> HostList;

	// Current game server version
	static const lsl::string cServerVersion;
	// UDP port for the spacewar server to do authentication on (ie, talk to Steam on)
	static const unsigned cAuthenticationPort;
	// UDP port for the spacewar server to listen on
	static const unsigned cServerPort;
	// UDP port for the master server updater to listen on
	static const unsigned cMasterServerUpdaterPort;
	//
	static const lsl::string cGameDir;
	//
	static const unsigned cMaxPlayers = MAX_PLAYERS_PER_SERVER;
private:
	SteamService* _service;

	// Track what server list request is currently running
	HServerListRequest _serverListRequest;

	// List of game servers
	HostList _hostList;

	// Vector to keep track of client connections
	ClientConnectionData_t _rgClientData[cMaxPlayers];

	// Vector to keep track of client connections which are pending auth
	ClientConnectionData_t _rgPendingClientData[cMaxPlayers];

	bool _hostInit;
	bool _serverConnecting;	
	bool _internetListLoading;

	// Send data to a client at the given ship index
	bool SendData(CSteamID steamId, const void *pData, uint32 nSizeOfData);
	// Send data to a client at the given ship index
	bool SendDataToClient( uint32 uShipIndex, char *pData, uint32 nSizeOfData );
	// Removes a player from the server
	void RemovePlayerFromServer( uint32 uShipPosition );
	// Function to tell Steam about our servers details
	void SendUpdatedServerDetailsToSteam();
	// Checks for any incoming network data, then dispatches it
	void ReceiveNetworkData();	

	// ISteamMatchmakingServerListResponse
	void ServerResponded( HServerListRequest hReq, int iServer );
	void ServerFailedToRespond( HServerListRequest hReq, int iServer );
	void RefreshComplete( HServerListRequest hReq, EMatchMakingServerResponse response );

	// Connect a client, will send a success/failure response to the client
	void OnClientBeginAuthentication( CSteamID steamIDClient, void *pToken, uint32 uTokenLen );
	// Handles authentication completing for a client
	void OnAuthCompleted( bool bAuthSuccess, uint32 iPendingAuthIndex );

	// Tells us when we have successfully connected to Steam
	STEAM_GAMESERVER_CALLBACK( SteamServer, OnSteamServersConnected, SteamServersConnected_t, _callbackSteamServersConnected );
	// Tells us when there was a failure to connect to Steam
	STEAM_GAMESERVER_CALLBACK( SteamServer, OnSteamServersConnectFailure, SteamServerConnectFailure_t, _callbackSteamServersConnectFailure );
	// Tells us when we have been logged out of Steam
	STEAM_GAMESERVER_CALLBACK( SteamServer, OnSteamServersDisconnected, SteamServersDisconnected_t, _callbackSteamServersDisconnected );

	// client connection state
	STEAM_GAMESERVER_CALLBACK( SteamServer, OnP2PSessionRequest, P2PSessionRequest_t, _callbackP2PSessionRequest );
	STEAM_GAMESERVER_CALLBACK( SteamServer, OnP2PSessionConnectFail, P2PSessionConnectFail_t, _callbackP2PSessionConnectFail );
	// Tells us a client has been authenticated and approved to play by Steam (passes auth, license check, VAC status, etc...)
	STEAM_GAMESERVER_CALLBACK( SteamServer, OnValidateAuthTicketResponse, ValidateAuthTicketResponse_t, _callbackGSAuthTicketResponse );
public:
	SteamServer(SteamService* service);
	~SteamServer();

	void Process(float deltaTime);

	bool CreateHost();
	void ShutdownHost();
	bool hostInit() const;
	bool serverConnecting() const;
	bool serverConnected() const;

	// Initiate a refresh of internet servers
	void FindInternetList();
	// Initiate a refresh of LAN servers
	void FindLanServers();
	//
	const HostList& hostList() const;

	void NeedAcception();
	CSteamID FindClient(unsigned accountId) const;
	lsl::string GetServerName() const;

	SteamService* service();
	GameMode* game();
	CSteamID steamId();
};

class SteamService: IGameUser
{
	static bool _init;
public:
	enum InitRes { irFailed, irRestart, irSuccess };
	enum SyncStage { ssGetUserStats, ssStoreUserStats, ssGetLeaders, ssStoreLeaders, cSyncStageEnd };

	static const float cSyncWaitTime;	
private:
	GameMode* _game;
	SteamLeaderboard* _leaderboard;
	SteamStats* _steamStats;
	SteamLobby* _lobby;
	SteamServer* _server;
	NetAcceptorSteam* _acceptor;
	// p2p game auth manager
	//CP2PAuthedGame* _pP2PAuthedGame;
	
	CGameID _gameId;	
	SyncStage _syncStage;
	float _syncWaitTime;

	CSteamID _serverSteamId;
	int _hostIP;
	int _hostPort;
	HAuthTicket _hAuthTicket;
	bool _initiatingConnection;
	bool _connected;

	bool DoSync();
	void DoSyncFailed();

	// Send data to a client at the given ship index
	bool SendData(const void *pData, uint32 nSizeOfData);
	// Receive a response from the server for a connection attempt
	void ReceiveServerInfo( CSteamID steamIDGameServer, bool bVACSecure, const char *pchServerName );
	// Handle the server exiting
	void ReceiveServerExiting();
	// Disconnects from a server (telling it so) if we are connected
	void DisconnectFromServer();
	// Updates what we show to friends about what we're doing and how to connect
	void UpdateRichPresenceConnectionInfo();
	// Receive a response from the server for a connection attempt
	void ReceiveServerAuthenticationResponse( bool bSuccess, uint32 uPlayerPosition );
	// Checks for any incoming network data, then dispatches it
	void ReceiveNetworkData();	

	virtual void OnProcessEvent(unsigned id, EventData* data);
public:
	SteamService(GameMode* game);
	~SteamService();

	void Process(float deltaTime);
	void Sync(float waitTime = 0, bool immediately = false);
	bool syncInProcess() const;
	unsigned time() const;

	void SendEvent(unsigned id, EventData* data = NULL);

	//client
	bool InitiateConnection(CSteamID steamIDGameServer);
	void CloseConnection();
	void NeedConnection();
	bool initiatingConnection() const;
	CSteamID serverSteamId() const;
	int hostIP() const;
	int hostPort() const;

	GameMode* game();
	const CGameID& gameId();

	SteamLeaderboard* leaderboard();
	SteamStats* steamStats();
	SteamLobby* lobby();
	SteamServer* server();
	NetAcceptorSteam* acceptor();

	static InitRes Initialize();
	static void Finalize();
	static bool isInit();
};

}

}

#endif