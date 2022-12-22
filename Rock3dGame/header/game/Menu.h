#pragma once

#include "ResourceManager.h"
#include "DataBase.h"
#include "Race.h"
#include "net/NetGame.h"
#include "net/SteamService.h"

#include "DialogMenu2.h"
#include "MainMenu2.h"
#include "RaceMenu2.h"
#include "HudMenu.h"
#include "OptionsMenu.h"
#include "FinishMenu.h"
#include "FinalMenu.h"

namespace r3d
{
	namespace game
	{
		class Menu : IGameUser, INetGameUser
		{
		public:
			enum NavDir { ndLeft = 0, ndRight, ndUp, ndDown, cNavDirEnd };

			struct NavElement
			{
				gui::Widget* widget;
				gui::Widget* nextWidget[cNavDirEnd];
				VirtualKey actionKeys[2];
			};

		private:
			class MyControlEvent : public ControlEvent
			{
			private:
				Menu* _menu;
			public:
				MyControlEvent(Menu* menu);

				bool OnMouseMoveEvent(const MouseMove& mMove) override;
				bool OnHandleInput(const InputMessage& msg) override;
			};

			class MyDisconnectEvent : public gui::Widget::Event
			{
			private:
				Menu* _menu;
			public:
				MyDisconnectEvent(Menu* menu);

				bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick) override;
			};

			class MySyncModeEvent : public gui::Widget::Event
			{
			private:
				Menu* _menu;
			public:
				MySyncModeEvent(Menu* menu);

				bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick) override;
			};

			class MySteamErrorEvent : public gui::Widget::Event
			{
			private:
				Menu* _menu;
			public:
				MySteamErrorEvent(Menu* menu);

				bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick) override;
			};

			class MySteamSavingEvent : public gui::Widget::Event
			{
			private:
				Menu* _menu;
			public:
				MySteamSavingEvent(Menu* menu);

				bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick) override;
			};

			using NavElements = std::map<gui::Widget*, NavElement>;
			using NavElementsList = List<std::pair<gui::Widget*, NavElements>>;
		public:
			enum State { msMain2, msRace2, msHud, msFinish2, msInfo, msFinal, cStateEnd };

			class SoundSheme : public gui::Widget::Event
			{
			private:
				Menu* _menu;

				snd::Sound* _clickDown;
				snd::Sound* _mouseEnter;
				snd::Sound* _selectItem;
				snd::Sound* _focused;
			protected:
				bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick) override;
				bool OnMouseEnter(gui::Widget* sender, const gui::MouseMove& mMove) override;
				void OnMouseLeave(gui::Widget* sender, bool wasReset) override;
				bool OnMouseOver(gui::Widget* sender, const gui::MouseMove& mMove) override;
				bool OnSelect(gui::Widget* sender, Object* item) override;
				void OnFocusChanged(gui::Widget* sender) override;
			public:
				SoundSheme(Menu* menu);
				~SoundSheme();

				snd::Sound* clickDown() const;
				void clickDown(snd::Sound* value);

				snd::Sound* mouseEnter() const;
				void mouseEnter(snd::Sound* value);

				snd::Sound* selectItem() const;
				void selectItem(snd::Sound* value);

				snd::Sound* focused() const;
				void focused(snd::Sound* value);
			};

			enum SoundShemeType
			{
				ssButton1 = 0,
				ssButton2,
				ssButton3,
				ssButton4,
				ssButton5,
				ssStepper,
				cSoundShemeTypeEnd
			};

			static const D3DXCOLOR cTextColor;
			//����������� ����������
			static const D3DXVECTOR2 cWinSize;
			//����������-��������������
			static const D3DXVECTOR2 cMinWinSize;
			//�����������-��������������
			static const D3DXVECTOR2 cMaxWinSize;

			static D3DXVECTOR2 GetImageSize(gui::Material& material);
			//��������������� �� newSize � ����������� ���������� ����������� Aspect
			static D3DXVECTOR2 GetAspectSize(const D3DXVECTOR2& curSize, const D3DXVECTOR2& newSize);
			static D3DXVECTOR2 GetImageAspectSize(gui::Material& material, const D3DXVECTOR2& newSize);
			static D3DXVECTOR2 StretchImage(D3DXVECTOR2 imageSize, const D3DXVECTOR2& size, bool keepAspect = true,
			                                bool fillRect = false, bool scaleDown = true, bool scaleUp = true);
			static D3DXVECTOR2 StretchImage(gui::Material& material, const D3DXVECTOR2& size, bool keepAspect = true,
			                                bool fillRect = false, bool scaleDown = true, bool scaleUp = true);
			static D3DXQUATERNION GetIsoRot();
		private:
			GameMode* _game;
			MyControlEvent* _controlEvent;
			MyDisconnectEvent* _disconnectEvent;
			MySyncModeEvent* _syncModeEvent;
			MySteamErrorEvent* _steamErrorEvent;
			MySteamSavingEvent* _steamSavingEvent;
			snd::Source* _audioSource;
			SoundSheme* _soundShemes[cSoundShemeTypeEnd];
			NavElementsList _navElementsList;

			State _state;
			bool _loadingVisible;
			bool _optionsVisible;
			bool _startOptionsVisible;

			gui::PlaneFon* _screenFon;
			gui::PlaneFon* _cursor;

			n::MainMenu* _mainMenu2;
			n::RaceMenu* _raceMenu2;
			HudMenu* _hudMenu;
			n::FinishMenu* _finishMenu2;
			FinalMenu* _finalMenu;
			n::InfoMenu* _infoMenu;
			n::OptionsMenu* _optionsMenu;
			n::StartOptionsMenu* _startOptionsMenu;

			n::AcceptDialog* _acceptDlg;
			n::WeaponDialog* _weaponDlg;
			n::InfoDialog* _messageDlg;
			n::MusicDialog* _musicDlg;
			n::UserChat* _userChat;
			float _weaponTime;
			float _messageTime;
			float _musicTime;
			bool _Ispectator;
			bool _IsFriendFrame;

			void SetScreenFon(bool init);
			void SetMainMenu2(bool init);
			void SetRaceMenu2(bool init);
			void SetHudMenu(bool init);
			void SetFinishMenu2(bool init);
			void SetFinalMenu(bool init);
			void SetInfoMenu(bool init);
			void SetOptionsMenu(bool init);
			void SetStartOptionsMenu(bool init);

			void ApplyState(State state);

			NavElement NavElementFind(const NavElements& elements, const NavElement& element,
			                          List<gui::Widget*> ignoreList, NavDir navDir);

			void DoPlayFinal();

			void OnDisconnectedPlayer(NetPlayer* sender) override;
			void OnFailed(unsigned error) override;
			void OnProcessNetEvent(unsigned id, NetEventData* data) override;

			bool OnMouseMoveEvent(const MouseMove& mMove);
			bool OnHandleInput(const InputMessage& msg);

		public:
			World* GetWorld();
			Menu(GameMode* game);
			~Menu() override;
			//������� �� ������ ����� � ������� ��������� (����)
			unsigned _maxPrize;
			//���������� ����������� ������� � ������� ���������
			unsigned _raceRestCount;
			unsigned _currentPoints;
			unsigned _reqestPoints;
			//���������� �����, ������� ����� ����� ������� �� ��������� ���������
			unsigned _potentialPoints;

			//centUnscacle - ������� ��������� � ������ ������, ������� �� �������������� � ������ ���������
			D3DXVECTOR2 WinToLocal(const D3DXVECTOR2& vec, bool centUnscacle = false);
			void AdjustLayout(const D3DXVECTOR2& vpSize);
			//��������� ����� ����������
			void OnResetView();
			void OnFinishClose();
			void SponsorMessage();

			void ShowCursor(bool value);
			bool IsCursorVisible() const;

			void ShowAccept(bool fixmenu, const std::string& message, const std::string& yesText,
			                const std::string& noText, const D3DXVECTOR2& pos, gui::Widget::Anchor align,
			                gui::Widget::Event* guiEvent, Object* data = nullptr, bool maxButtonsSize = false,
			                bool maxMode = false, bool disableFocus = false);
			void HideAccept();
			gui::Widget* GetAcceptSender();
			bool GetAcceptResultYes();
			Object* GetAcceptData();
			//
			void ShowWeaponDialog(const std::string& title, const std::string& message, const std::string& moneyText,
			                      const std::string& damageText, const D3DXVECTOR2& pos, gui::Widget::Anchor align,
			                      float timeDelay = 1.0f);
			void HideWeaponDialog();
			//
			void ShowMessage(const std::string& title, const std::string& message, const std::string& okText,
			                 const D3DXVECTOR2& pos, gui::Widget::Anchor align, float timeDelay = 0.0f,
			                 gui::Widget::Event* guiEvent = nullptr, bool okButton = true);
			void ShowMessageLoading();
			void HideMessage();
			gui::Widget* GetMessageSender();
			//
			void ShowMusicInfo(const std::string& title, const std::string& text);
			void HideMusicInfo();
			//
			void ShowDiscreteVideoCardMessage();
			void ShowSteamErrorMessage();
			void ShowSteamSavingMessage();
			//
			void ShowChat(bool show);
			void ClearChat();
			bool IsChatVisible() const;
			n::UserChat* GetUserChat();
			//
			void ShowChatInput(bool show);
			void ClearChatInput();
			bool IsChatInputVisible() const;

			void Terminate();

			//matchmaking
			void InitializeNet();
			void FinalizateNet();
			void StartMatch(Race::Mode mode, Difficulty difficulty, Race::Profile* profile, bool netGame,
			                net::INetAcceptorImpl* impl = nullptr);
			void ExitMatch(bool kicked = false);
			//
			bool ConnectMatch(const net::Endpoint& endpoint, bool useDefaultPort,
			                  net::INetAcceptorImpl* impl = nullptr);
			void MatchConnected();
			void DisconnectMatch();
			//
			void CarSlotsChanged();
			//
			unsigned GetMaxPrize();
			unsigned GetRaceRestCount();
			unsigned GetCurrentPoints();
			unsigned GetReqestPoints();
			unsigned GetPotentialPoints();
			//
			unsigned GetCurTrack();
			unsigned GetTracksCount();
			//
			bool IsSingePlayer();
			bool IsCampaign();
			bool IsSkirmish();
			bool IsNetGame();
			bool ISpectator();
			void ISpectator(bool value);
#ifdef STEAM_SERVICE
	bool IsSteamSavingInProcess();
#endif
			//
			void DelProfile(int profileIndex);
			Race::Profile* GetLastProfile(bool netGame);	
			Player* GetPlayer();
			Player* GetSecondPlayer();
			Player* GetCurrentPlayer();
			
			
			//match
			void SetFriendFrame(bool value);
			bool IsFriendFrame();
			void SetGamerId(int gamerId);
			const D3DXCOLOR& GetCarColor();
			void SetCarColor(const D3DXCOLOR& color);
			bool BuyCar(Garage::Car* car);
			void ChangePlanet(Planet* value);

			int GetUpgradeMaxLevel();
			void SetUpgradeMaxLevel(int value);
			bool EnabledOptionUpgradeMaxLevel();

			int GetSelectedLevel();
			void SetSelectedLevel(int value);
			bool EnabledOptionSelectedLevel();

			Difficulty currentDiff();
			void currentDiff(Difficulty value);
			bool enabledOptionDiff();

			unsigned lapsCount();
			void lapsCount(unsigned value);
			bool enabledLapsCount();

			unsigned maxPlayers();
			void maxPlayers(unsigned value);
			bool enabledMaxPlayers();

			unsigned maxComputers();
			void maxComputers(unsigned value);
			bool enabledMaxComputers();

			bool weaponUpgrades();
			void weaponUpgrades(bool value);
			bool enabledWeaponUpgrades();

			bool survivalMode();
			void survivalMode(bool value);
			bool enabledSurvivalMode();

			bool autoCamera();
			void autoCamera(bool value);

			int subjectView();
			void subjectView(int value);

			bool camLock();
			void camLock(bool value);

			unsigned splitType();
			void splitType(unsigned value);

			bool staticCam();
			void staticCam(bool value);

			unsigned camFov();
			void camFov(unsigned value);

			int camProection();
			void camProection(int value);

			bool oilDestroyer();
			void oilDestroyer(bool value);
			bool enabledOilDestroyer();

			unsigned StyleHUD();
			void StyleHUD(unsigned value);

			unsigned MinimapStyle();
			void MinimapStyle(unsigned value);

			bool enableMiniMap();
			void enableMiniMap(bool value);

			bool enableMineBug();
			void enableMineBug(bool value);
			bool activeEnableMineBug();

			//race
			void StartRace();
			void ExitRace();

			void RegUser(IGameUser* user);
			void UnregUser(IGameUser* user);
			void SendEvent(unsigned id, EventData* data = nullptr);
			void OnProcessEvent(unsigned id, EventData* data) override;

			gui::Dummy* CreateDummy(gui::Widget* parent, gui::Widget::Event* guiEvent,
			                        SoundShemeType soundSheme = cSoundShemeTypeEnd);
			gui::PlaneFon* CreatePlane(gui::Widget* parent, gui::Widget::Event* guiEven, graph::Tex2DResource* image,
			                           bool imageSize, const D3DXVECTOR2& size = IdentityVec2,
			                           gui::Material::Blending blend = gui::Material::bmOpaque,
			                           SoundShemeType soundSheme = cSoundShemeTypeEnd);
			gui::PlaneFon* CreatePlane(gui::Widget* parent, gui::Widget::Event* guiEven, const std::string& image,
			                           bool imageSize, const D3DXVECTOR2& size = IdentityVec2,
			                           gui::Material::Blending blend = gui::Material::bmOpaque,
			                           SoundShemeType soundSheme = cSoundShemeTypeEnd);
			gui::Button* CreateArrowButton(gui::Widget* parent, gui::Widget::Event* guiEvent,
			                               const D3DXVECTOR2& size = IdentityVec2);
			gui::Button* CreateSpaceArrowButton(gui::Widget* parent, gui::Widget::Event* guiEvent,
			                                    const D3DXVECTOR2& size = IdentityVec2);
			//
			gui::Button* Menu::CreateMenuButton(const string& name, const std::string& font, const std::string& norm,
			                                    const std::string& sel, gui::Widget* parent,
			                                    gui::Widget::Event* guiEvent, const D3DXVECTOR2& size,
			                                    gui::Button::Style style, const D3DXCOLOR& textColor,
			                                    SoundShemeType soundSheme);
			gui::Button* CreateMenuButton(StringValue name, const std::string& font, const std::string& norm,
			                              const std::string& sel, gui::Widget* parent, gui::Widget::Event* guiEvent,
			                              const D3DXVECTOR2& size = IdentityVec2,
			                              gui::Button::Style style = gui::Button::bsSimple,
			                              const D3DXCOLOR& textColor = clrWhite, SoundShemeType soundSheme = ssButton1);
			gui::Button* CreateMenuButton(const std::string& name, gui::Widget* parent, gui::Widget::Event* guiEvent,
			                              const D3DXVECTOR2& size = IdentityVec2,
			                              SoundShemeType soundSheme = ssButton1);
			gui::Button* CreateMenuButton(StringValue name, gui::Widget* parent, gui::Widget::Event* guiEvent,
			                              const D3DXVECTOR2& size = IdentityVec2,
			                              SoundShemeType soundSheme = ssButton1);
			gui::Button* CreateMenuButton2(StringValue name, gui::Widget* parent, gui::Widget::Event* guiEvent);
			//
			gui::Button* CreateArrow(gui::Widget* parent, gui::Widget::Event* guiEvent);
			gui::Button* CreateUpArrow(gui::Widget* parent, gui::Widget::Event* guiEvent);
			gui::Button* CreateDWNArrow(gui::Widget* parent, gui::Widget::Event* guiEvent);
			gui::Button* CreateUpArrowR(gui::Widget* parent, gui::Widget::Event* guiEvent);
			gui::Button* CreateDWNArrowR(gui::Widget* parent, gui::Widget::Event* guiEvent);
			gui::Label* CreateLabel(const std::string& name, gui::Widget* parent, const std::string& font,
			                        const D3DXVECTOR2& size = NullVec2,
			                        gui::Text::HorAlign horAlign = gui::Text::haCenter,
			                        gui::Text::VertAlign vertAlign = gui::Text::vaCenter,
			                        const D3DXCOLOR& color = cTextColor);
			gui::Label* CreateLabel(StringValue name, gui::Widget* parent, const std::string& font,
			                        const D3DXVECTOR2& size = NullVec2,
			                        gui::Text::HorAlign horAlign = gui::Text::haCenter,
			                        gui::Text::VertAlign vertAlign = gui::Text::vaCenter,
			                        const D3DXCOLOR& color = cTextColor);
			gui::DropBox* CreateDropBox(gui::Widget* parent, gui::Widget::Event* guiEvent, const StringList& items);
			gui::TrackBar* CreateTrackBar(gui::Widget* parent, gui::Widget::Event* guiEvent, const D3DXVECTOR2& size);
			gui::ListBox* CreateListBox(gui::Widget* parent, gui::Widget::Event* guiEvent,
			                            const D3DXVECTOR2& size = D3DXVECTOR2(200.0f, 600.0f),
			                            const D3DXVECTOR2& itemSize = D3DXVECTOR2(75.0f, 75.0f),
			                            const D3DXVECTOR2& itemSpace = D3DXVECTOR2(10.0f, 10.0f));
			gui::ProgressBar* CreateBar(gui::Widget* parent, gui::Widget::Event* guiEvent, const std::string& front,
			                            const std::string& back,
			                            gui::ProgressBar::Style style = gui::ProgressBar::psHorizontal);
			gui::ChargeBar* CreateChargeBar(gui::Widget* parent, gui::Widget::Event* guiEvent, unsigned maxCharge,
			                                unsigned curCharge);
			gui::ColorList* CreateColorList(gui::Widget* parent, gui::Widget::Event* guiEvent, const D3DXVECTOR2& size);
			gui::ViewPort3d* CreateItemBox(gui::Widget* parent, gui::Widget::Event* guiEvent,
			                               graph::IndexedVBMesh* mesh, graph::Tex2DResource* meshTex);
			gui::Button* CreateCloseButton(gui::Widget* parent, gui::Widget::Event* guiEvent,
			                               const D3DXVECTOR2& size = IdentityVec2);
			gui::ScrollBox* CreateScrollBox(gui::Widget* parent, gui::Widget::Event* guiEvent,
			                                const D3DXVECTOR2& size = D3DXVECTOR2(200.0f, 600.0f));
			gui::Grid* CreateGrid(gui::Widget* parent, gui::Widget::Event* guiEvent, gui::Grid::Style style,
			                      const D3DXVECTOR2& cellSize = IdentityVec2, unsigned maxCellsOnLine = 0);
			gui::StepperBox* CreateStepper(const StringList& items, gui::Widget* parent, gui::Widget::Event* guiEvent);

			gui::ViewPort3d* CreateViewPort3d(gui::Widget* parent, gui::Widget::Event* guiEvent, const std::string& fon,
			                                  gui::ViewPort3d::Style style = gui::ViewPort3d::msStatic,
			                                  bool isoRot = true, bool fonSize = true,
			                                  const D3DXVECTOR2& size = IdentityVec2,
			                                  SoundShemeType soundSheme = cSoundShemeTypeEnd);
			gui::Mesh3d* CreateMesh3d(gui::ViewPort3d* parent, graph::IndexedVBMesh* mesh,
			                          graph::Tex2DResource* meshTex, int meshId = -1);
			gui::ViewPort3d* CreateMesh3dBox(gui::Widget* parent, gui::Widget::Event* guiEvent,
			                                 graph::IndexedVBMesh* mesh, graph::Tex2DResource* meshTex,
			                                 gui::ViewPort3d::Style style = gui::ViewPort3d::msStatic,
			                                 SoundShemeType soundSheme = cSoundShemeTypeEnd);
			gui::Plane3d* CreatePlane3d(gui::ViewPort3d* parent, const std::string& fon, const D3DXVECTOR2& size);

			void ReleaseWidget(gui::Widget* widget);

			void SetButtonEnabled(gui::Button* button, bool enable);
			void SetStepperEnabled(gui::StepperBox* stepper, bool enable);

			void OnProgress(float deltaTime);

			State GetState() const;
			void SetState(State value);
			void SetGamersState();

			void ShowLoading(bool show);
			bool IsLoadingVisible() const;

			void ShowOptions(bool show);
			bool IsOptionsVisible() const;

			void ShowStartOptions(bool show);
			bool IsStartOptionsVisible() const;

			ControlManager* GetControl();
			Environment* GetEnv();
			DataBase* GetDB();
			gui::Manager* GetGUI();
			GameMode* GetGame();
			Race* GetRace();
			Trace* GetTrace();
			NetGame* GetNet();
#ifdef STEAM_SERVICE
	SteamService* GetSteamService();
#endif

			float GetVolume(Logic::SndCategory cat);
			void SetVolume(Logic::SndCategory cat, float value);

			graph::Tex2DResource* GetTexture(const std::string& name, bool ifNullThrow = true);
			graph::IndexedVBMesh* GetMesh(const std::string& name);
			graph::TextFont* GetFont(const std::string& name);
			snd::Sound* GetSound(const string& name);
			SoundSheme* GetSoundSheme(SoundShemeType sheme);
			const std::string& GetString(StringValue value);
			const std::string& GetString(const std::string& value);
			bool HasString(StringValue value);
			bool HasString(const std::string& value);
			string FormatCurrency(int val, string unit = "");

			D3DXVECTOR2 GetAspectSize();

			const graph::DisplayModes& GetDisplayModes();
			bool FindNearMode(const Point& resolution, graph::DisplayMode& mode);
			Point GetDisplayMode();
			void SetDisplayMode(const Point& resolution);
			int GetDisplayModeIndex();

			bool GetFullScreen();
			void SetFullScreen(bool value);

			bool GetDisableVideo();
			void SetDisableVideo(bool value);

			void SaveGameOpt();

			const string& GetLanguage() const;
			void SetLanguage(const string& value);
			const Language* GetLanguageParam() const;
			int GetLanguageIndex() const;

			void Pause(bool pause);
			bool IsPaused();

			void PlaySound(snd::Sound* sound);
			void PlaySound(const string& soundName);
			void StopSound();

			void PlayMusic(snd::Sound* sound, const string& name, const string& band, bool showInfo);
			void PlayMusic(const string& soundName, const string& name, const string& band, bool showInfo);
			void StopMusic();

			void PlayMovie(const std::string& name);
			bool IsMoviePlaying() const;

			void RegNavElements(gui::Widget* key, const NavElements& value);
			void UnregNavElements(gui::Widget* key);
			void SetNavElements(gui::Widget* key, bool reg, NavElement elements[], int count);
			NavElementsList::iterator GetNavElements(gui::Widget* key);
		};
	}
}