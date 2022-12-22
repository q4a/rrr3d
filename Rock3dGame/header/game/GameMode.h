#pragma once

#include "Race.h"
#include "Menu.h"
#include "net/NetGame.h"
#include "net/SteamService.h"

namespace r3d
{
	namespace game
	{
		class GameMode : ControlEvent
		{
		public:
			struct Track
			{
				Track(): sound(nullptr), group(0)
				{
				}

				Track(snd::Sound* mSound, string mName, string mBand): sound(nullptr), name(mName), band(mBand),
				                                                       group(0) { SetSound(mSound); }

				Track(const Track& ref): sound(nullptr) { operator=(ref); }
				~Track() { SetSound(nullptr); }

				void SetSound(snd::Sound* value)
				{
					ReplaceRef(sound, value);
					sound = value;
				}

				Track& operator=(const Track& ref)
				{
					SetSound(ref.sound);
					name = ref.name;
					band = ref.band;
					group = ref.group;

					return *this;
				}

				snd::Sound* sound;
				string name;
				string band;
				int group;
			};

			using Tracks = Vector<Track>;
			using PlayList = Vector<int>;
		private:
			class MusicCat : public snd::Source::Report
			{
			private:
				GameMode* _game;
				PlayList _playList;
				snd::seek_pos _pos;
				snd::seek_pos _pcmTotal;
				Tracks _tracks;
				Track _curTrack;

				void GenRandom(int ignore);
			protected:
				void OnStreamEnd(snd::Proxy* sender, snd::PlayMode mode) override;
			public:
				MusicCat(GameMode* game);
				~MusicCat() override;

				void SaveGame(SWriter* writer);
				void LoadGame(SReader* reader);

				void SaveUser(SWriter* writer);
				void LoadUser(SReader* reader);

				void AddTrack(const Track& track);
				void ClearTracks();
				const Track& GetCurTrack() const;
				const Tracks& GetTracks() const;

				void Play(bool showInfo = true);
				bool Play(int trackIndex, bool excludeFromPlayList, bool showInfo = true);
				void Next();
				void Stop();
				void Pause(bool state);
			};

			struct Voice
			{
				Voice(): weight(0), sPlayer(false), ePlayer(false), forHuman(false), soundRef(nullptr),
				         soundExists(false)
				{
				}

				float weight;
				//нужно имя игрока вначале/в конце
				bool sPlayer;
				bool ePlayer;
				bool forHuman;
				//
				string sound;
				snd::Sound* soundRef;
				bool soundExists;
			};

			using Voices = List<Voice>;
			using Sounds = List<snd::Sound*>;

			enum BusyAction { baSkip = 0, baQueue, baReplace, cBusyActionEnd };

			static const char* cBusyActionStr[cBusyActionEnd];

			struct Comment
			{
				Comment(): time(0), lastPlayer(cUndefPlayerId), chance(0), delay(0), busy(baSkip), repeatPlayer(true)
				{
				}

				mutable float time;
				mutable int lastPlayer;
				//вероятность
				float chance;
				//минимальная задержка
				float delay;
				//действие в случае занятого комментатора
				BusyAction busy;
				//допустить последовательные повторения для одного и того же игрока
				bool repeatPlayer;
				//
				Voices voices;
			};

			using Comments = std::map<string, Comment>;

			class Commentator : snd::Source::Report, IGameUser
			{
			private:
				GameMode* _game;
				Comments _comments;
				float _delay;

				snd::Source* _source;
				Sounds _sounds;
				float _time;
				float _timeSilience;

				void Play(const Sounds& sounds, bool replace);
				bool Next();
				void CheckVoice(Voice& voice);

				void OnStreamEnd(snd::Proxy* sender, snd::PlayMode mode) override;
			public:
				Commentator(GameMode* game);
				~Commentator() override;

				void SaveGame(SWriter* writer);
				void LoadGame(SReader* reader);
				void CheckSounds();
				void ResetState();

				void OnProcessEvent(unsigned id, EventData* data) override;

				void Add(const string& name, const Comment& comment);
				void Add(const string& name, const string& sound, float chance, float delay, BusyAction busy = baSkip,
				         bool repeatPlayer = true, float weight = 0, bool sPlayer = false, bool ePlayer = false,
				         bool forHuman = false);
				void AddVoice(const string& name, const string& sound, float weight, bool sPlayer = false,
				              bool ePlayer = false, bool forHuman = false);
				void Clear();

				const Comment* FindComment(const string& name) const;
				const Voice* Generate(const Comment& comment, int playerId) const;
				const Voice* Generate(const string& name, int playerId) const;
				void Stop();
				bool IsSpeaking() const;

				void OnProgress(float deltaTime);

				float GetDelay() const;
				void SetDelay(float value);
			};

			using Users = Container<IGameUser*>;
		public:
			static const int cGoRaceWait = 0;
			static const int cGoRace1 = 1;
			static const int cGoRace2 = 2;
			static const int cGoRace3 = 3;
			static const int cGoRace = 4;

			enum PrefCamera { pcThirdPerson = 0, pcIsometric, cPrefCameraEnd };

			enum ProectionCamera { A = 0, B, cCamProectionEnd };

			enum SpltType { Horrizontal = 0, Vertical, Quad, cSplitTypeEnd };

			enum FovLevel { Fov60 = 0, Fov65, Fov70, Fov75, Fov80, Fov85, Fov90, Fov95, Fov100, Fov105, Fov110, Fov115, cFovLevelEnd };

			enum SubjectLevel { Def = 0, P1, P2, P3, P4, P5, P6, P7, Boss, Auto, cSubjectLevelEnd };

			enum MMStyle { Off = 0, MMA, MMB, MMC, MMD, MME, MMF, MMH, MMG, MMI, MMJ, MMK, cMMStyleEnd };

			enum HUDStyle { HUDOff = 0, Standart, Silver, cHUDStyleEnd };

			static const string cPrefCameraStr[cPrefCameraEnd];
		private:
			World* _world;
			Race* _race;
			NetGame* _netGame;

#ifdef STEAM_SERVICE
	SteamService* _steamService;
#endif

			Languages _languages;
			string _language;

			CommentatorStyles _commentatorStyles;
			string _commentatorStyle;
			PrefCamera _prefCamera;
			float _cameraDistance;

			unsigned _maxPlayers;
			unsigned _maxComputers;
			int _upgradeMaxLevel;
			int _selectedLevel;
			bool _weaponUpgrades;
			bool _survivalMode;
			bool _autoCamera;
			int _subjectView;		
			bool _camLock;
			unsigned _splitType;
			bool _staticCam;
			unsigned _camFov;
			int _camProection;
			bool _oilDestroyer;
			unsigned _lapsCount;
			unsigned _styleHUD;
			unsigned _minimapStyle;
			bool _enableMineBug;
			bool _disableVideo;
			Users _users;
			bool _discreteVideoChanged;
			bool _prefCameraAutodetect;

			snd::Source* _music;
			snd::Source::Report* _musicReport;
			MusicCat* _menuMusic;
			MusicCat* _gameMusic;
			float _fadeMusic;
			float _fadeSpeedMusic;

			Commentator* _commentator;
			Menu* _menu;

			int _startRace;
			bool _prepareGame;
			bool _startGame;
			float _startUpTime;
			float _movieTime;
			std::string _movieFileName;
			gui::PlaneFon* _guiLogo;
			gui::PlaneFon* _guiLogo2;
			gui::PlaneFon* _guiLogo3;
			gui::PlaneFon* _guiStartup;
			MapObj* _semaphore;

			float _goRaceTime;
			float _finishTime;

			void PrepareGame();
			void StartGame();
			void FreeIntro();
			void AdjustGameStartup();
			void SetSemaphore(MapObj* value);

			void DoStartRace();
			void DoExitRace();

			void SaveGameOpt(SWriter* writer);
			void LoadGameOpt(SReader* reader, bool discreteVideoChanges);
			void SaveConfig(SWriter* writer);
			void LoadConfig(SReader* reader, bool discreteVideoChanges);
			void ResetConfig();

			void SaveGameData(SWriter* writer);
			void LoadGameData(SReader* reader);
			void ResetGameData();

			void SaveGameData();
			void LoadGameData();

			bool OnHandleInput(const InputMessage& msg) override;
		public:
			GameMode(World* world);
			~GameMode() override;

			void RegFixedStepEvent(IFixedStepEvent* user);
			void UnregFixedStepEvent(IFixedStepEvent* user);

			void RegProgressEvent(IProgressEvent* user);
			void UnregProgressEvent(IProgressEvent* user);

			void RegLateProgressEvent(ILateProgressEvent* user);
			void UnregLateProgressEvent(ILateProgressEvent* user);

			void RegFrameEvent(IFrameEvent* user);
			void UnregFrameEvent(IFrameEvent* user);

			void RegUser(IGameUser* user);
			void UnregUser(IGameUser* user);
			void SendEvent(unsigned id, EventData* data = nullptr);

			void Run(bool playIntro);
			void Terminate();
			bool IsStartgame() const;
			unsigned time() const;

			void LoadConfig(bool discreteVideoChanges);
			void SaveConfig();
			void SaveGame(bool saveProfile);

			void StartMatch(Race::Mode mode, Difficulty difficulty, Race::Profile* profile, bool createPlayers,
			                bool netGame, bool netClient);
			void ExitMatch(bool saveGame);
			bool IsMatchStarted() const;

			void StartRace();
			void ExitRace(bool saveGame, const Race::Results* results = nullptr);
			void ExitRaceGoFinish();
			void GoRace(int stage);
			void GoRaceTimer();
			void RunFinishTimer();
			bool IsStartRace() const;
			bool IsRaceFinish() const;

			bool ChangePlanet(Planet* planet);

			void Pause(bool pause);
			bool IsPaused() const;

			void PlayMusic(const Track& track, snd::Source::Report* report, snd::seek_pos pos = 0,
			               bool showInfo = false);
			void StopMusic();
			void FadeInMusic(float sVolume = -1, float speed = 1.0f);
			void FadeOutMusic(float sVolume = -1, float speed = 1.0f);
			snd::Sound* GetSound(const string& name, bool assertFind = true);

			void PlayMovie(const std::string& name);
			bool IsMoviePlaying() const;

			void CheckStartupMenu();
			void OnResetView();
			void OnFinishFrameClose();

			void OnFrame(float deltaTime, float pxAlpha);
			void OnGraphEvent(HWND hwnd, long eventCode, LONG_PTR param1, LONG_PTR param2);

			World* GetWorld();
			Race* GetRace();
			Menu* GetMenu();
			NetGame* netGame();
#ifdef STEAM_SERVICE
	SteamService* steamService();
#endif
			MusicCat* menuMusic();
			MusicCat* gameMusic();

			const Language* FindLanguage(const string& name) const;
			const Language* FindLanguage(int primId) const;
			int FindLanguageIndex(const string& name) const;
			const Languages& GetLanguages() const;

			const string& GetLanguage() const;
			void SetLanguage(const string& value);
			const Language* GetLanguageParam() const;
			int GetLanguageIndex() const;

			void AutodetectLanguage();
			void ApplyLanguage();

			const CommentatorStyle* FindCommentatorStyle(const string& name) const;
			int FindCommentatorStyleIndex(const string& name) const;
			const CommentatorStyles& GetCommentatorStyles() const;

			const string& GetCommentatorStyle() const;
			void SetCommentatorStyle(const string& value);
			void AutodetectCommentatorStyle();

			PrefCamera GetPrefCamera() const;
			void SetPrefCamera(PrefCamera value);

			float GetCameraDistance() const;
			void SetCameraDistance(float value);

			unsigned maxPlayers() const;
			void maxPlayers(unsigned value);

			unsigned maxComputers() const;
			void maxComputers(unsigned value);

			int upgradeMaxLevel() const;
			void upgradeMaxLevel(int value);

			int SelectedLevel() const;
			void SelectedLevel(int value);

			bool weaponUpgrades() const;
			void weaponUpgrades(bool value);

			bool survivalMode() const;
			void survivalMode(bool value);

			bool autoCamera() const;
			void autoCamera(bool value);

			int subjectView() const;
			void subjectView(int value);

			bool CamLock() const;
			void CamLock(bool value);

			bool StaticCam() const;
			void StaticCam(bool value);

			unsigned CamFov() const;
			void CamFov(unsigned value);

			unsigned GetSplitMOde() const;
			void SplitMOde(unsigned value);

			int CamProection() const;
			void CamProection(int value);

			bool oilDestroyer() const;
			void oilDestroyer(bool value);

			Difficulty currentDiff() const;
			void currentDiff(Difficulty value);
			bool enabledOptionDiff() const;

			unsigned lapsCount() const;
			void lapsCount(unsigned value);

			unsigned StyleHUD() const;
			void StyleHUD(unsigned value);

			unsigned MinimapStyle() const;
			void MinimapStyle(unsigned value);

			bool enableMineBug() const;
			void enableMineBug(bool value);

			bool disableVideo() const;
			void disableVideo(bool value);

			bool fullScreen() const;
			void fullScreen(bool value);
		};
	}
}
