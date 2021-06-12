#pragma once

#include "MenuSystem.h"

namespace r3d
{

namespace game
{

namespace n
{

class MainMenu;

class GameModeFrame: public MenuFrame
{
private:	
	enum MenuItem {miChampionship = 0, miSkirmish, miBack, cMenuItemEnd};
private:
	MainMenu* _mainMenu;
protected:
	virtual void OnShow(bool value);
	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);
public:
	GameModeFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent);	
};

class DifficultyFrame: public MenuFrame, IGameUser
{
private:
	enum State {miBack = cDifficultyEnd, cMenuItemEnd};
private:
	MainMenu* _mainMenu;
	Difficulty _difficulty;
protected:
	virtual void OnShow(bool value);
	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);
	virtual void OnProcessEvent(unsigned id, EventData* data);
public:
	DifficultyFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent);	
	virtual ~DifficultyFrame();
};

class NetBrowserFrame: public MenuFrame, INetGameUser
{
private:	
	enum MenuItem {miBack, cMenuItemEnd};
private:
	MainMenu* _mainMenu;
	int _gridScroll;

	gui::Label* _info;
	gui::Grid* _grid;
	gui::Button* _downArrow;
	gui::Button* _upArrow;	

	void UpdateGrid();
	void AdjustGrid(const D3DXVECTOR2& vpSize);
	void ScrollGrid(int step);

	void StartWaiting(bool start, StringValue hint = svNull);
	void Refresh();
protected:
	virtual void OnShow(bool value);
	virtual void OnInvalidate();
	virtual void OnAdjustLayout(const D3DXVECTOR2& vpSize);
	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);

	virtual void OnPingComplete();
public:
	NetBrowserFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent);
	~NetBrowserFrame();
};

class NetIPAddressFrame: public MenuFrame, ControlEvent
{
private:	
	enum MenuItem {miConnect, miBack, cMenuItemEnd};
	enum Label {mlIPAdress, mlInfo, cLabelEnd};
private:
	gui::Label* _labels[cLabelEnd];

	MainMenu* _mainMenu;

	void StartWaiting(bool start, StringValue hint = svNull);
	void PushLine(std::string text);
protected:
	virtual void OnShow(bool value);
	virtual void OnAdjustLayout(const D3DXVECTOR2& vpSize);

	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);
	virtual bool OnHandleInput(const InputMessage& msg);
public:
	NetIPAddressFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent);
	virtual ~NetIPAddressFrame();
};

#ifdef STEAM_SERVICE

class LobbyFrame: public MenuFrame, IGameUser
{
private:	
	enum MenuItem {miStart, miBack, cMenuItemEnd};
private:
	MainMenu* _mainMenu;
	int _gridScroll;

	gui::Label* _info;
	gui::Grid* _grid;
	gui::Button* _downArrow;
	gui::Button* _upArrow;	

	void UpdateGrid();
	void AdjustGrid(const D3DXVECTOR2& vpSize);
	void ScrollGrid(int step);

	void StartWaiting(bool start, StringValue hint = svNull);
	void Refresh();
protected:
	virtual void OnShow(bool value);
	virtual void OnInvalidate();
	virtual void OnAdjustLayout(const D3DXVECTOR2& vpSize);
	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);		

	virtual void OnProcessEvent(unsigned id, EventData* data);
public:
	LobbyFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent);
	~LobbyFrame();
};

class MatchmakingFrame: public MenuFrame, IGameUser
{
private:	
	enum MenuItem {miBack, cMenuItemEnd};
private:
	MainMenu* _mainMenu;
	int _gridScroll;

	gui::Label* _info;
	gui::Grid* _grid;
	gui::Button* _downArrow;
	gui::Button* _upArrow;	

	void UpdateGrid();
	void AdjustGrid(const D3DXVECTOR2& vpSize);
	void ScrollGrid(int step);

	void StartWaiting(bool start, StringValue hint = svNull);
	void Refresh();
protected:
	virtual void OnShow(bool value);
	virtual void OnInvalidate();
	virtual void OnAdjustLayout(const D3DXVECTOR2& vpSize);
	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);		

	virtual void OnProcessEvent(unsigned id, EventData* data);
public:
	MatchmakingFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent);
	~MatchmakingFrame();
};

class SteamBrowserFrame: public MenuFrame, IGameUser, INetGameUser
{
private:	
	enum MenuItem {miBack, cMenuItemEnd};
private:
	MainMenu* _mainMenu;
	int _gridScroll;

	gui::Label* _info;
	gui::Grid* _grid;
	gui::Button* _downArrow;
	gui::Button* _upArrow;	

	void UpdateGrid();
	void AdjustGrid(const D3DXVECTOR2& vpSize);
	void ScrollGrid(int step);

	void StartWaiting(bool start, StringValue hint = svNull);
	void Refresh();
protected:
	virtual void OnShow(bool value);
	virtual void OnInvalidate();
	virtual void OnAdjustLayout(const D3DXVECTOR2& vpSize);
	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);		

	virtual void OnProcessEvent(unsigned id, EventData* data);

	virtual void OnDisconnected(net::INetConnection* sender);
	virtual void OnConnectionFailed(net::INetConnection* sender, unsigned error);
	virtual void OnConnectedPlayer(NetPlayer* sender);
	virtual void OnDisconnectedPlayer(NetPlayer* sender);
public:
	SteamBrowserFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent);
	~SteamBrowserFrame();
};

#endif

class ServerTypeFrame: public MenuFrame
{
private:	
	enum MenuItem {miLocal = 0, 
#ifdef STEAM_SERVICE
	#ifndef _RETAIL
		miSteam,
	#endif
		miLobby,
#endif
		miBack, cMenuItemEnd};
private:
	MainMenu* _mainMenu;
protected:
	virtual void OnShow(bool value);
	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);
public:
	ServerTypeFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent);	
};

class ClientTypeFrame: public MenuFrame
{
private:	
	enum MenuItem {miConnectLan, miConnectIP, 
#ifdef STEAM_SERVICE
	#ifndef _RETAIL
		miConnectSteam, miConnectSteamLan,
	#endif
		miConnectLobby,
#endif
		miBack, cMenuItemEnd};
private:
	MainMenu* _mainMenu;
protected:
	virtual void OnShow(bool value);
	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);
public:
	ClientTypeFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent);
};

class NetworkFrame: public MenuFrame
{
private:	
	enum MenuItem {miCreate = 0, miConnect, miBack, cMenuItemEnd};
	enum Label {mlIPAdress, cLabelEnd};
private:
	MainMenu* _mainMenu;

	gui::Label* _labels[cLabelEnd];
protected:
	virtual void OnShow(bool value);		
	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);		
public:
	NetworkFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent);
	virtual ~NetworkFrame();

	virtual void OnAdjustLayout(const D3DXVECTOR2& vpSize);
};

class TournamentFrame: public MenuFrame
{
private:	
	enum MenuItem {miContinue = 0, miNewGame, miLoad, miBack, cMenuItemEnd};
private:
	MainMenu* _mainMenu;
protected:
	virtual void OnShow(bool value);
	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);
public:
	TournamentFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent);
};

class CreditsFrame: public MenuFrame
{
private:	
	enum MenuItem {miBack, cMenuItemEnd};
private:
	MainMenu* _mainMenu;
	float _scrollTime;

	gui::Label* _label;
protected:
	virtual void OnShow(bool value);
	virtual void OnAdjustLayout(const D3DXVECTOR2& vpSize);
	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);
public:
	CreditsFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent);
	virtual ~CreditsFrame();

	void OnProgress(float deltaTime);
};

class ProfileFrame: public MenuFrame
{
private:	
	enum MenuItem {miBack, cMenuItemEnd};
private:
	MainMenu* _mainMenu;
	int _gridScroll;

	gui::Grid* _grid;
	gui::Button* _downArrow;
	gui::Button* _upArrow;	

	void AdjustGrid(const D3DXVECTOR2& vpSize);
	void ScrollGrid(int step);
	void UpdateGrid();
protected:	
	virtual void OnShow(bool value);
	virtual void OnInvalidate();
	virtual void OnAdjustLayout(const D3DXVECTOR2& vpSize);

	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);
public:
	ProfileFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent);
	virtual ~ProfileFrame();
};

class MainFrame: public MenuFrame
{
private:	
	enum MenuItem {miSingle = 0, miNetwork, miOptions, miAuthors, miExit, cMenuItemEnd};
private:
	MainMenu* _mainMenu;
protected:	
	virtual void OnShow(bool value);
	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);
public:
	MainFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent);
};

class MainMenu: IGameUser, INetGameUser
{
public:
	enum State {msMain = 0, msGameMode, msNetwork, msNetBrowser, msLoad, msTournament, msCredits, msDifficulty, msNetIPAdress, msMatchmaking, msServerType, msClientType, msLobby, msSteamBrowser, cStateEnd};

	enum ServerType {stLocal = 0, stLobby, stSteam, cServerTypeEnd};

	typedef std::list<State> StateStack;
private:
	Menu* _menu;
	StateStack _stateStack;
	State _state;
	ServerType _serverType;

	gui::Dummy* _root;
	gui::PlaneFon* _winFrame;
	gui::PlaneFon* _topPanel;
	gui::PlaneFon* _bottomPanel;
	gui::Label* _version;

	std::vector<gui::Button*> _menuItems;

	Race::Mode _steamHostMode;
	Difficulty _steamHostDifficulty;
	Race::Profile* _steamHostProfile;
	bool _steamHostStarting;
	bool _steamConnecting;
	
	MainFrame* _mainFrame;
	GameModeFrame* _gameMode;
	NetBrowserFrame* _netBrowser;
	NetworkFrame* _network;
	ProfileFrame* _profile;	
	TournamentFrame* _tournament;
	CreditsFrame* _credits;
	DifficultyFrame* _difficultyFrame;
	NetIPAddressFrame* _netIPAddressFrame;
	ServerTypeFrame* _serverTypeFrame;
	ClientTypeFrame* _clientTypeFrame;
#ifdef STEAM_SERVICE
	MatchmakingFrame* _matchmakingFrame;
	LobbyFrame* _lobbyFrame;
	SteamBrowserFrame* _steamBrowser;
#endif

	void ApplyState(State state);
	void AdjustMenuItems(const D3DXVECTOR2& vpSize);

	virtual void OnProcessEvent(unsigned id, EventData* data);

	virtual void OnDisconnected(net::INetConnection* sender);
	virtual void OnConnectionFailed(net::INetConnection* sender, unsigned error);
	virtual void OnConnectedPlayer(NetPlayer* sender);
	virtual void OnDisconnectedPlayer(NetPlayer* sender);	
public:
	MainMenu(Menu* menu, gui::Widget* parent);
	virtual ~MainMenu();

	void AdjustLayout(const D3DXVECTOR2& vpSize);
	void Show(bool value);
	void OnProgress(float deltaTime);

	gui::Widget* GetRoot();

	State GetState() const;
	void SetState(State value);

	void PushState(State value);
	void BackState();
	void ClearStates();
	bool ContainsState(State value) const;

	ServerType GetServerType() const;
	void SetServerType(ServerType value);

	gui::Button* GetItem(int index);
	void SetItems(const lsl::string menuItemsStr[], unsigned count, bool mainMenu, gui::Widget::Event* guiEvent);
	void InvalidateItems();

	void StartMatch(Race::Mode mode, Difficulty difficulty, Race::Profile* profile, bool startLobby = false);
	void ConnectMatch(const net::Endpoint& endpoint
#ifdef STEAM_SERVICE
		, const CSteamID& steamId
#endif
		);
	Race::Mode steamHostMode() const;
	Difficulty steamHostDifficulty() const;
	Race::Profile* steamHostProfile() const;

	gui::Button* CreateMenuButton(const lsl::string& name, gui::Widget* parent, gui::Widget::Event* guiEvent);
	gui::Button* CreateMenuButton(StringValue name, gui::Widget* parent, gui::Widget::Event* guiEvent);
};

}

}

}