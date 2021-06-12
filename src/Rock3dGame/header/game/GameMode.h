#pragma once

#include "Race.h"
#include "Menu.h"
#include "net\NetGame.h"
#include "net\SteamService.h"

namespace r3d
{

namespace game
{

class GameMode: ControlEvent
{	
public:
	struct Track
	{
		Track(): sound(0), group(0) {}
		Track(snd::Sound* mSound, lsl::string mName, lsl::string mBand): sound(0), name(mName), band(mBand), group(0) {SetSound(mSound);}
		Track(const Track& ref): sound(0) {operator=(ref);}
		~Track() {SetSound(0);}

		void SetSound(snd::Sound* value) {lsl::Object::ReplaceRef(sound, value); sound = value;}

		Track& operator=(const Track& ref) 
		{ 
			SetSound(ref.sound);
			name = ref.name;
			band = ref.band;
			group = ref.group;

			return *this;
		}

		snd::Sound* sound;
		lsl::string name;
		lsl::string band;
		int group;
	};
	typedef lsl::Vector<Track> Tracks;
	typedef lsl::Vector<int> PlayList;
private:
	class MusicCat: public snd::Source::Report
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
		virtual void OnStreamEnd(snd::Proxy* sender, snd::PlayMode mode);
	public:
		MusicCat(GameMode* game);
		~MusicCat();

		void SaveGame(lsl::SWriter* writer);
		void LoadGame(lsl::SReader* reader);

		void SaveUser(lsl::SWriter* writer);
		void LoadUser(lsl::SReader* reader);

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
		Voice(): weight(0), sPlayer(false), ePlayer(false), forHuman(false), soundRef(NULL), soundExists(false) {}

		float weight;
		//нужно имя игрока вначале/в конце
		bool sPlayer;
		bool ePlayer;
		bool forHuman;
		//
		lsl::string sound;
		snd::Sound* soundRef;
		bool soundExists;
	};
	typedef lsl::List<Voice> Voices;
	typedef lsl::List<snd::Sound*> Sounds;
	enum BusyAction {baSkip = 0, baQueue, baReplace, cBusyActionEnd};
	static const char* cBusyActionStr[cBusyActionEnd];

	struct Comment
	{
		Comment(): time(0), lastPlayer(cUndefPlayerId), chance(0), delay(0), busy(baSkip), repeatPlayer(true) {}

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
	typedef std::map<lsl::string, Comment> Comments;

	class Commentator: snd::Source::Report, IGameUser
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

		virtual void OnStreamEnd(snd::Proxy* sender, snd::PlayMode mode);
	public:
		Commentator(GameMode* game);
		~Commentator();

		void SaveGame(lsl::SWriter* writer);
		void LoadGame(lsl::SReader* reader);
		void CheckSounds();
		void ResetState();

		void OnProcessEvent(unsigned id, EventData* data);

		void Add(const lsl::string& name, const Comment& comment);
		void Add(const lsl::string& name, const lsl::string& sound, float chance, float delay, BusyAction busy = baSkip, bool repeatPlayer = true, float weight = 0, bool sPlayer = false, bool ePlayer = false, bool forHuman = false);
		void AddVoice(const lsl::string& name, const lsl::string& sound, float weight, bool sPlayer = false, bool ePlayer = false, bool forHuman = false);
		void Clear();

		const Comment* FindComment(const lsl::string& name) const;
		const Voice* Generate(const Comment& comment, int playerId) const;
		const Voice* Generate(const lsl::string& name, int playerId) const;
		void Stop();
		bool IsSpeaking() const;

		void OnProgress(float deltaTime);

		float GetDelay() const;
		void SetDelay(float value);
	};

	typedef lsl::Container<IGameUser*> Users;
public:
	static const int cGoRaceWait = 0;
	static const int cGoRace1 = 1;
	static const int cGoRace2 = 2;
	static const int cGoRace3 = 3;
	static const int cGoRace = 4;

	enum PrefCamera {pcThirdPerson = 0, pcIsometric, cPrefCameraEnd};	

	static const lsl::string cPrefCameraStr[cPrefCameraEnd];	
private:
	World* _world;
	Race* _race;
	NetGame* _netGame;

#ifdef STEAM_SERVICE
	SteamService* _steamService;
#endif

	Languages _languages;
	lsl::string _language;
	
	CommentatorStyles _commentatorStyles;
	lsl::string _commentatorStyle;
	PrefCamera _prefCamera;
	float _cameraDistance;

	unsigned _maxPlayers;
	unsigned _maxComputers;
	int _upgradeMaxLevel;
	int _weaponMaxLevel;
	bool _springBorders;
	unsigned _lapsCount;
	bool _enableHUD;
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
	
	void SaveGameOpt(lsl::SWriter* writer);
	void LoadGameOpt(lsl::SReader* reader, bool discreteVideoChanges);	
	void SaveConfig(lsl::SWriter* writer);
	void LoadConfig(lsl::SReader* reader, bool discreteVideoChanges);
	void ResetConfig();

	void SaveGameData(lsl::SWriter* writer);
	void LoadGameData(lsl::SReader* reader);
	void ResetGameData();

	void SaveGameData();
	void LoadGameData();

	virtual bool OnHandleInput(const InputMessage& msg);
public:
	GameMode(World* world);
	~GameMode();

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
	void SendEvent(unsigned id, EventData* data = NULL);

	void Run(bool playIntro);
	void Terminate();
	bool IsStartgame() const;
	unsigned time() const;
	
	void LoadConfig(bool discreteVideoChanges);
	void SaveConfig();
	void SaveGame(bool saveProfile);

	void StartMatch(Race::Mode mode, Difficulty difficulty, Race::Profile* profile, bool createPlayers, bool netGame, bool netClient);
	void ExitMatch(bool saveGame);
	bool IsMatchStarted() const;

	void StartRace();
	void ExitRace(bool saveGame, const Race::Results* results = NULL);
	void ExitRaceGoFinish();
	void GoRace(int stage);
	void GoRaceTimer();
	void RunFinishTimer();
	bool IsStartRace() const;
	bool IsRaceFinish() const;

	bool ChangePlanet(Planet* planet);	

	void Pause(bool pause);
	bool IsPaused() const;

	void PlayMusic(const Track& track, snd::Source::Report* report, snd::seek_pos pos = 0, bool showInfo = false);
	void StopMusic();
	void FadeInMusic(float sVolume = -1, float speed = 1.0f);
	void FadeOutMusic(float sVolume = -1, float speed = 1.0f);	
	snd::Sound* GetSound(const lsl::string& name, bool assertFind = true);

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

	const Language* FindLanguage(const lsl::string& name) const;
	const Language* FindLanguage(int primId) const;
	int FindLanguageIndex(const lsl::string& name) const;
	const Languages& GetLanguages() const;

	const lsl::string& GetLanguage() const;
	void SetLanguage(const lsl::string& value);
	const Language* GetLanguageParam() const;
	int GetLanguageIndex() const;

	void AutodetectLanguage();
	void ApplyLanguage();

	const CommentatorStyle* FindCommentatorStyle(const lsl::string& name) const;
	int FindCommentatorStyleIndex(const lsl::string& name) const;
	const CommentatorStyles& GetCommentatorStyles() const;

	const lsl::string& GetCommentatorStyle() const;
	void SetCommentatorStyle(const lsl::string& value);
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

	int weaponMaxLevel() const;
	void weaponMaxLevel(int value);

	bool springBorders() const;
	void springBorders(bool value);

	Difficulty currentDiff() const;
	void currentDiff(Difficulty value);
	bool enabledOptionDiff() const;

	unsigned lapsCount() const;
	void lapsCount(unsigned value);

	bool enableHUD() const;
	void enableHUD(bool value);

	bool enableMineBug() const;
	void enableMineBug(bool value);

	bool disableVideo() const;
	void disableVideo(bool value);

	bool fullScreen() const;
	void fullScreen(bool value);
};

}

}