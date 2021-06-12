#include "stdafx.h"
#include "game\World.h"

#include "game\GameMode.h"
#include <mbctype.h>
#include "lslSerialFileXml.h"

namespace r3d
{

namespace game
{

const char* GameMode::cBusyActionStr[cBusyActionEnd] = {"baSkip", "baQueue", "baReplace"};
const lsl::string GameMode::cPrefCameraStr[GameMode::cPrefCameraEnd] = {"pcThirdPerson", "pcIsometric"};




GameMode::GameMode(World* world): _world(world), _musicReport(0), _fadeMusic(1.0f), _fadeSpeedMusic(0.0f), _startUpTime(-1), _movieTime(-1), _movieFileName(""), _guiLogo(0), _guiLogo2(NULL), _guiStartup(0), _semaphore(NULL), _startRace(-1), _prepareGame(false), _startGame(false), _goRaceTime(-1), _finishTime(-1), _menu(0), _race(0), _netGame(0), _maxPlayers(6), _maxComputers(5), _upgradeMaxLevel(Garage::cUpgMaxLevel), _weaponMaxLevel(Garage::cWeaponMaxLevel), _springBorders(true), _lapsCount(4), _enableHUD(true), _enableMineBug(true), _disableVideo(false), _prefCamera(pcIsometric), _cameraDistance(1.25f), _discreteVideoChanged(false), _prefCameraAutodetect(false)
#ifdef STEAM_SERVICE
	, _steamService(NULL)
#endif
{
	LSL_LOG("game LoadMusic");

	_world->GetResManager()->LoadMusic();

	LSL_LOG("init music");

	_music = _world->GetLogic()->CreateSndSource(Logic::scMusic);
	_music->AddRef();

	_gameMusic = new MusicCat(this);
	_menuMusic = new MusicCat(this);
	_commentator = new Commentator(this);

	LSL_LOG("load cfg");

	LoadGameData();
	LoadConfig(true);

	ApplyLanguage();

	_world->GetControl()->InsertEvent(this);
}
	
GameMode::~GameMode()
{
	_world->GetControl()->RemoveEvent(this);

	for (Users::const_iterator iter = _users.begin(); iter != _users.end(); ++iter)
		(*iter)->Release();
	_users.Clear();

	ExitRace(false);
	FreeIntro();

	lsl::SafeDelete(_menu);

	delete _commentator;

	delete _menuMusic;
	delete _gameMusic;
	_music->Release();
	_world->GetLogic()->ReleaseSndSource(_music);

	lsl::SafeDelete(_netGame);
#ifdef STEAM_SERVICE
	lsl::SafeDelete(_steamService);
#endif
	lsl::SafeDelete(_race);	
}

GameMode::MusicCat::MusicCat(GameMode* game): _game(game), _pos(0), _pcmTotal(0)
{
}

GameMode::MusicCat::~MusicCat()
{
	Stop();
}

void GameMode::MusicCat::GenRandom(int ignore)
{
	struct Node
	{
		int group;
		GameMode::PlayList playList;

		bool operator<(const Node& ref)
		{
			return group < ref.group;
		}
	};

	typedef std::list<Node> Nodes;
	typedef lsl::Vector<int> Slots;

	Nodes nodes;
	PlayList defGroupList;
	_playList.clear();

	for (unsigned i = 0; i < _tracks.size(); ++i)
	{
		const Track& track = _tracks[i];

		if (track.group == 0)
			defGroupList.push_back(i);
		else
		{
			Nodes::iterator iter;

			for (iter = nodes.begin(); iter != nodes.end(); ++iter)
				if (iter->group == track.group)
					break;

			if (iter != nodes.end())
			{
				iter->playList.push_back(i);
			}
			else
			{
				Node node;
				node.group = track.group;
				node.playList.push_back(i);

				nodes.push_back(node);
			}
		}
	}

	Slots slots;
	nodes.sort();
	_playList.resize(_tracks.size());

	for (unsigned i = 0; i < _tracks.size(); ++i)
		slots.push_back(i);

	for (Nodes::iterator iter = nodes.begin(); iter != nodes.end(); ++iter)
	{
		Node& node = *iter;
		PlayList list;

		std::random_shuffle(node.playList.begin(), node.playList.end());

		for (unsigned j = 0; j < node.playList.size(); ++j)
		{
			unsigned slotsCount = std::max(slots.size() - Floor<unsigned>(slots.size() / (float)node.playList.size()), node.playList.size());
			unsigned slotsOffset = slots.size() - slotsCount;
			unsigned count = std::max(node.playList.size() - 1, 1U);
			int index = (count * slotsOffset + 2 * (slotsCount - 1) * j) / (2 * count);
			
			index = slots[index];
			_playList[index] = node.playList[j];
			list.push_back(index);
		}

		for (unsigned i = 0; i < list.size(); ++i)
			slots.Remove(list[i]);
	}

	std::random_shuffle(defGroupList.begin(), defGroupList.end());

	for (unsigned i = 0; i < defGroupList.size(); ++i)
		_playList[slots[i]] = defGroupList[i];

	//необходимо исключить возможность совпадения последнего элемента новой последовательности с прошлым треком
	if (!_playList.empty() && _playList.back() == ignore)
		std::iter_swap(_playList.begin(), --_playList.end());
}

void GameMode::MusicCat::OnStreamEnd(snd::Proxy* sender, snd::PlayMode mode)
{
	Next();
}

void GameMode::MusicCat::SaveGame(lsl::SWriter* writer)
{
	lsl::SWriter* tracksNode = writer->NewDummyNode("tracks");
	for (unsigned i = 0; i < _tracks.size(); ++i)
	{		
		lsl::SWriter* track = tracksNode->NewDummyNode(lsl::StrFmt("sound%d", i).c_str());
		track->WriteRef("item", _tracks[i].sound);
		track->WriteValue("name", _tracks[i].name);
		track->WriteValue("band", _tracks[i].band);
		track->WriteValue("group", _tracks[i].group);
	}
}

void GameMode::MusicCat::LoadGame(lsl::SReader* reader)
{
	ClearTracks();
	_playList.clear();

	lsl::SReader* tracksNode = reader->ReadValue("tracks");
	lsl::SReader* track = tracksNode ? tracksNode->FirstChildValue() : 0;
	while (track)
	{
		Track item;
		lsl::Serializable::FixUpName fixName;
		if (track->ReadRef("item", true, 0, &fixName))
			item.SetSound(fixName.GetCollItem<snd::Sound*>());
		track->ReadValue("name", item.name);
		track->ReadValue("band", item.band);
		track->ReadValue("group", item.group);
		_tracks.push_back(item);

		track = track->NextValue();
	}
}

void GameMode::MusicCat::SaveUser(lsl::SWriter* writer)
{
	lsl::StringVec vec;
	lsl::string str;
	for (unsigned i = 0; i < _playList.size(); ++i)
		vec.push_back(lsl::StrFmt("%d", _playList[i]).c_str());
	lsl::StrLinkValues(vec, str);
	writer->WriteValue("playList", str);
}

void GameMode::MusicCat::LoadUser(lsl::SReader* reader)
{
	lsl::StringVec vec;
	lsl::string str;
	reader->ReadValue("playList", str);
	lsl::StrExtractValues(str, vec);
	for (unsigned i = 0; i < vec.size(); ++i)
	{
		std::stringstream sstream(vec[i]);
		int v;
		sstream >> v;
		_playList.push_back(v);
	}
}

void GameMode::MusicCat::AddTrack(const Track& track)
{
	_tracks.push_back(track);
}

void GameMode::MusicCat::ClearTracks()
{
	_curTrack = Track();
	_tracks.clear();	
}

const GameMode::Track& GameMode::MusicCat::GetCurTrack() const
{
	return _curTrack;
}

const GameMode::Tracks& GameMode::MusicCat::GetTracks() const
{
	return _tracks;
}

void GameMode::MusicCat::Play(bool showInfo)
{
	if (_playList.empty())
		GenRandom(-1);

	while (!_playList.empty())
	{
		int ind = _playList.back();
		_playList.pop_back();		

		if (Play(ind, false, showInfo))
			break;
	}
}

bool GameMode::MusicCat::Play(int trackIndex, bool excludeFromPlayList, bool showInfo)
{
	if (excludeFromPlayList)
		_playList.Remove(trackIndex);

	if (trackIndex >= 0 && static_cast<unsigned>(trackIndex) < _tracks.size())
	{
		_game->PlayMusic(_tracks[trackIndex], this, 0, showInfo);
		_curTrack = _tracks[trackIndex];

		if (_playList.empty())
			GenRandom(trackIndex);

		return true;
	}

	return false;
}

void GameMode::MusicCat::Next()
{
	Play();
}

void GameMode::MusicCat::Stop()
{
	_game->StopMusic();
}

void GameMode::MusicCat::Pause(bool state)
{
	if (state)
	{
		_pos = _game->_music->GetPos();
		_pcmTotal = _curTrack.sound ? _curTrack.sound->GetPCMTotal() : 0;

		Stop();
	}
	else
	{
		if (_pos < _pcmTotal)
			_game->PlayMusic(_curTrack, this, _pos, false);
		else
			Play(true);
		_pos = 0;
	}
}

GameMode::Commentator::Commentator(GameMode* game): _game(game), _delay(0), _time(0), _timeSilience(0)
{
	_source = _game->GetWorld()->GetLogic()->CreateSndSource(Logic::scVoice);
	_source->AddRef();
	_source->RegReport(this);

	_game->RegUser(this);
}

GameMode::Commentator::~Commentator()
{
	_game->UnregUser(this);

	Clear();

	_source->Release();
	_source->UnregReport(this);
	_game->GetWorld()->GetLogic()->ReleaseSndSource(_source);	
}

void GameMode::Commentator::Play(const Sounds& sounds, bool replace)
{
	if (replace)
	{
		Stop();
		_sounds = sounds;
		if (Next())
			_timeSilience = 0;
	}
	else
	{
		bool isSpeaking = IsSpeaking();
		_sounds.insert(_sounds.end(), sounds.begin(), sounds.end());
		if (!isSpeaking && Next())
			_timeSilience = 0;
	}	
}

bool GameMode::Commentator::Next()
{
	if (_sounds.empty())
	{
		Stop();
		return false;
	}

	_source->SetSound(_sounds.front());
	_sounds.pop_front();
	_source->SetPos(0);
	_source->Play();

	return true;
}

void GameMode::Commentator::CheckVoice(Voice& voice)
{
	if (voice.soundRef == NULL)
		voice.soundRef = _game->GetSound(voice.sound, false);

	voice.soundExists = voice.soundRef && lsl::FileSystem::GetInstance()->FileExists(voice.soundRef->GetFileName());
}

void GameMode::Commentator::OnStreamEnd(snd::Proxy* sender, snd::PlayMode mode)
{
	Next();
}

void GameMode::Commentator::SaveGame(lsl::SWriter* writer)
{
	writer->WriteValue("delay", _delay);

	lsl::SWriter* comments = writer->NewDummyNode("comments");
	for (Comments::const_iterator iter = _comments.begin(); iter != _comments.end(); ++iter)
	{
		lsl::SWriter* comment = comments->NewDummyNode(iter->first.c_str());
		const Comment& item = iter->second;

		comment->WriteValue("chance", item.chance);
		comment->WriteValue("delay", item.delay);
		lsl::SWriteEnum(comment, "busy", item.busy, cBusyActionStr, cBusyActionEnd);
		comment->WriteValue("repeatPlayer", item.repeatPlayer);

		lsl::SWriter* voices = comment->NewDummyNode("voices");
		int i = 0;
		for (Voices::const_iterator vIter = item.voices.begin(); vIter != item.voices.end(); ++vIter, ++i)
		{
			lsl::SWriter* voice = voices->NewDummyNode(lsl::StrFmt("voice%d", i).c_str());
			voice->WriteValue("weight", vIter->weight);
			voice->WriteValue("sPlayer", vIter->sPlayer);
			voice->WriteValue("ePlayer", vIter->ePlayer);
			voice->WriteValue("forHuman", vIter->forHuman);
			voice->WriteValue("sound", vIter->sound);
		}
	}
}

void GameMode::Commentator::LoadGame(lsl::SReader* reader)
{
	Clear();

	reader->ReadValue("delay", _delay);

	lsl::SReader* comments = reader->ReadValue("comments");
	lsl::SReader* comment = comments ? comments->FirstChildValue() : 0;
	while (comment)
	{
		Comment item;		
		comment->ReadValue("chance", item.chance);
		comment->ReadValue("delay", item.delay);
		lsl::SReadEnum(comment, "busy", item.busy, cBusyActionStr, cBusyActionEnd);
		comment->ReadValue("repeatPlayer", item.repeatPlayer);

		lsl::SReader* voices = comment->ReadValue("voices");
		lsl::SReader* voice = voices ? voices->FirstChildValue() : 0;
		while (voice)
		{
			Voice itemVoice;
			voice->ReadValue("weight", itemVoice.weight);
			voice->ReadValue("sPlayer", itemVoice.sPlayer);
			voice->ReadValue("ePlayer", itemVoice.ePlayer);
			voice->ReadValue("forHuman", itemVoice.forHuman);
			voice->ReadValue("sound", itemVoice.sound);

			//SReader* child = voice->ReadValue("sound");
			//child->ReadAttr("item")->CastTo<lsl::string>(&itemVoice.sound, 1);

			item.voices.push_back(itemVoice);

			voice = voice->NextValue();
		}

		Add(comment->GetMyName(), item);
		comment = comment->NextValue();
	}
}

void GameMode::Commentator::CheckSounds()
{
	for (Comments::iterator iter = _comments.begin(); iter != _comments.end(); ++iter)
		for (Voices::iterator vIter = iter->second.voices.begin(); vIter != iter->second.voices.end(); ++vIter)
			CheckVoice(*vIter);
}

void GameMode::Commentator::ResetState()
{
	_time = 0;
	_timeSilience = 0;

	for (Comments::iterator iter = _comments.begin(); iter != _comments.end(); ++iter)
	{
		iter->second.time = 0.0f;
		iter->second.lastPlayer = cUndefPlayerId;
	}
}

void GameMode::Commentator::OnProcessEvent(unsigned id, EventData* data)
{
	std::string name = cEventNameMap[id];
	const Comment* comment = FindComment(name);

	if (comment == 0)
		return;
	if (comment->busy == baSkip && (_delay > _timeSilience || IsSpeaking()))
		return;

	Sounds sounds;
	int plrId = data != NULL ? data->playerId : cUndefPlayerId;
	const Voice* voice = Generate(*comment, plrId);

	if (voice == 0)
		return;

	Player* player = data != NULL ? _game->GetRace()->GetPlayerById(data->playerId) : NULL;
	lsl::string plrName = player != NULL ? player->GetName() : "undef";

	if (voice->sPlayer && !plrName.empty())
	{
		const Voice* voice = Generate(plrName, plrId);
		if (voice)
			sounds.push_back(voice->soundRef);
		else
			return;
	}

	sounds.push_back(voice->soundRef);

	if (voice->ePlayer && !plrName.empty())
	{
		const Voice* voice = Generate(plrName, plrId);
		if (voice)
			sounds.push_back(voice->soundRef);
		else
			return;
	}

	Play(sounds, comment->busy == baReplace || comment->busy == baSkip);

	lsl::string soundsName;
	for (Sounds::const_iterator iter = sounds.begin(); iter != sounds.end(); ++iter)
		soundsName += (*iter)->GetFileName() + ",";

	//LSL_LOG(lsl::StrFmt("playVoice id=%s sounds=%s", name.c_str(), soundsName.c_str()));
}

void GameMode::Commentator::Add(const lsl::string& name, const Comment& comment)
{
	LSL_ASSERT(_comments.find(name) == _comments.end());

	Comment newComment = comment;

	for (Voices::iterator iter = newComment.voices.begin(); iter != newComment.voices.end(); ++iter)
		CheckVoice(*iter);

	_comments[name] = newComment;
}

void GameMode::Commentator::Add(const lsl::string& name, const lsl::string& sound, float chance, float delay, BusyAction busy, bool repeatPlayer, float weight, bool sPlayer, bool ePlayer, bool forHuman)
{
	Comment comment;
	comment.chance = chance;
	comment.delay = delay;
	comment.busy = busy;
	comment.repeatPlayer = repeatPlayer;

	Voice voice;
	voice.sPlayer = sPlayer;
	voice.ePlayer = ePlayer;
	voice.forHuman = forHuman;
	voice.weight = weight;
	voice.sound = sound;

	comment.voices.push_back(voice);

	Add(name, comment);
}

void GameMode::Commentator::AddVoice(const lsl::string& name, const lsl::string& sound, float weight, bool sPlayer, bool ePlayer, bool forHuman)
{
	Comments::iterator iter = _comments.find(name);
	LSL_ASSERT(iter != _comments.end());

	Voice voice;
	voice.sPlayer = sPlayer;
	voice.ePlayer = ePlayer;
	voice.forHuman = forHuman;
	voice.weight = weight;
	voice.sound = sound;
	CheckVoice(voice);

	iter->second.voices.push_back(voice);
}

void GameMode::Commentator::Clear()
{
	Stop();

	_sounds.clear();
	_comments.clear();
}

const GameMode::Comment* GameMode::Commentator::FindComment(const lsl::string& name) const
{
	Comments::const_iterator iter = _comments.find(name);
	if (iter == _comments.end())
		return 0;
	return &iter->second;
}

const GameMode::Voice* GameMode::Commentator::Generate(const Comment& comment, int playerId) const
{
	if (comment.chance < Random() * 100.0f)
		return 0;
	if (!comment.repeatPlayer && playerId != cUndefPlayerId && comment.lastPlayer == playerId)
		return 0;

	if (_time >= comment.time)
	{
		comment.time = _time + comment.delay;
		comment.lastPlayer = playerId;

		float summWeight = 0;
		for (Voices::const_iterator iter = comment.voices.begin(); iter != comment.voices.end(); ++iter)
		{
			if (iter->soundExists && (iter->forHuman == false || Race::IsHumanId(playerId)))
				summWeight += iter->weight;
		}

		float weight = summWeight * Random();
		summWeight = 0;
		for (Voices::const_iterator iter = comment.voices.begin(); iter != comment.voices.end(); ++iter)
		{
			if (iter->soundExists && (iter->forHuman == false || Race::IsHumanId(playerId)))
			{
				if (weight >= summWeight && weight <= summWeight + iter->weight)
					return &(*iter);

				summWeight += iter->weight;
			}
		}
	}

	return 0;
}

const GameMode::Voice* GameMode::Commentator::Generate(const lsl::string& name, int playerId) const
{
	const Comment* comment = FindComment(name);
	if (comment == 0)
		return 0;	

	return Generate(*comment, playerId);
}

void GameMode::Commentator::Stop()
{
	_sounds.clear();
	_source->Stop();
	_source->SetSound(0);
}

bool GameMode::Commentator::IsSpeaking() const
{
	return _source->IsPlaying() || !_sounds.empty();
}

void GameMode::Commentator::OnProgress(float deltaTime)
{
	_time += deltaTime;
	_timeSilience += deltaTime;
}

float GameMode::Commentator::GetDelay() const
{
	return _delay;
}

void GameMode::Commentator::SetDelay(float value)
{
	_delay = value;
}

void GameMode::PrepareGame()
{
	if (_prepareGame)
		return;
	_prepareGame = true;

	LSL_LOG("prepare game");

	_world->ResetInput();
	_world->LoadRes();

	LSL_LOG("game init race");

	_race = new Race(this, "race");	

	LSL_LOG("game init net");

#ifdef STEAM_SERVICE
	_steamService = new SteamService(this);

	LSL_LOG("steam sync");

	_steamService->Sync();
#endif

	_netGame = new NetGame(this);
}

void GameMode::StartGame()
{
	if (_startGame)
		return;
	_startGame = true;

	LSL_LOG("game start");

	_world->ResetInput();
	FreeIntro();

	LSL_LOG("game init menu");

	_menu = new Menu(this);
	_menu->SetState(Menu::msMain2);
	_menu->ShowMusicInfo(_menuMusic->GetCurTrack().band, _menuMusic->GetCurTrack().name);

#ifdef STEAM_SERVICE
	if (!_steamService->isInit())
		_menu->ShowSteamErrorMessage();
	else
#endif
		CheckStartupMenu();

	//debug
/*#if DEBUG_PX
	_menu->StartMatch(Race::rmSkirmish, gdHard, NULL, false);

	Garage::Car* car = _race->GetGarage().FindCar("podushka");
	_race->GetHuman()->GetPlayer()->SetColor(clrYellow);
	_race->GetHuman()->GetPlayer()->AddPoints(999999);
	_race->GetHuman()->GetPlayer()->AddMoney(999999);
	_race->GetAchievment().AddPoints(999999);
	_race->GetGarage().BuyCar(_race->GetHuman()->GetPlayer(), car);

	for (int i = Player::stWeapon1; i <= Player::stWeapon4; ++i)
	{
		Slot* slot = _race->GetHuman()->GetPlayer()->GetSlotInst((Player::SlotType)i);
		WeaponItem* weapon = slot ? slot->GetItem().IsWeaponItem() : 0;
		if (weapon)
			weapon->SetCntCharge(99);
	}
	
	_race->GetTournament().SetCurPlanet(_race->GetTournament().GetPlanets()[1]);
	_race->GetTournament().GetCurPlanet().Unlock();
	_race->GetTournament().GetCurPlanet().Open();
	//_race->GetTournament().GetCurPlanet().SetPass(2);
	_race->GetTournament().SetCurTrack(_race->GetTournament().GetPlanets()[1]->GetTracks()[0]);
	_race->GetHuman()->GetPlayer()->AddPoints(999999);
	
	StartRace();
#endif*/
}

void GameMode::FreeIntro()
{
	if (_startUpTime == -1)
		return;

	_startUpTime = -1;

	GetWorld()->GetEnv()->SetGUIMode(false);

	if (_guiLogo)
		_world->GetGraph()->GetGUI().ReleaseWidget(_guiLogo);
	_guiLogo = 0;

	if (_guiLogo2)
		_world->GetGraph()->GetGUI().ReleaseWidget(_guiLogo2);
	_guiLogo2 = 0;

	if (_guiStartup)
		_world->GetGraph()->GetGUI().ReleaseWidget(_guiStartup);
	_guiStartup = 0;
}

void GameMode::AdjustGameStartup()
{
	D3DXVECTOR2 size = _world->GetGraph()->GetGUI().GetVPSize();

	if (_guiLogo)
	{
		_guiLogo->SetSize(Menu::GetImageSize(_guiLogo->GetMaterial()));
		_guiLogo->SetPos(size/2.0f);
	}
	if (_guiLogo2)
	{
		_guiLogo2->SetSize(Menu::GetImageSize(_guiLogo2->GetMaterial()));
		_guiLogo2->SetPos(size/2.0f);
	}
	if (_guiStartup)
	{
		_guiStartup->SetSize(Menu::GetImageAspectSize(_guiStartup->GetMaterial(), size));
		_guiStartup->SetPos(size/2.0f);
	}	
}

void GameMode::SetSemaphore(MapObj* value)
{
	if (lsl::Object::ReplaceRef(_semaphore, value))
		_semaphore = value;
}

void GameMode::DoStartRace()
{
	_startRace = -1;

	_commentator->ResetState();
	_world->GetCamera()->StopFly();
	_world->GetResManager()->LoadWorld(GetRace()->GetTournament().GetCurPlanet().GetWorldType());
	GetRace()->StartRace();
	SetSemaphore(_world->GetMap()->GetSemaphore());

	_menu->SetState(Menu::msHud);

	_menuMusic->Pause(true);
	_gameMusic->Play();

#if DEBUG_PX
	GoRace(cGoRace);
#else
	GoRace(cGoRaceWait);
#endif
}

void GameMode::SaveGameOpt(lsl::SWriter* writer)
{
	lsl::SWriter* quality = writer->NewDummyNode("quality");
	if (quality)
	{		
		quality->WriteValue("filtering", _world->GetEnv()->GetFiltering());
		quality->WriteValue("msaa", _world->GetEnv()->GetMultisampling());
		quality->WriteValue("shadow", _world->GetEnv()->GetShadowQuality());
		quality->WriteValue("environment", _world->GetEnv()->GetEnvQuality());
		quality->WriteValue("light", _world->GetEnv()->GetLightQuality());	
		quality->WriteValue("postEffect", _world->GetEnv()->GetPostEffQuality());
		lsl::SWriteEnum(quality, "frameRateMode", _world->GetEnv()->GetSyncFrameRate(), Environment::cSyncFrameRateStr, Environment::cSyncFrameRateEnd);
	}

	lsl::SWriteValue(writer, "resolution", _world->GetView()->GetDesc().resolution);

	lsl::SWriter* volume = writer->NewDummyNode("volume");
	if (volume)
	{
		volume->WriteValue("musicVolume", _world->GetLogic()->GetVolume(Logic::scMusic));
		volume->WriteValue("effectsVolume", _world->GetLogic()->GetVolume(Logic::scEffects));
		volume->WriteValue("voiceVolume", _world->GetLogic()->GetVolume(Logic::scVoice));		
	}

	writer->WriteValue("maxPlayers", _maxPlayers);
	writer->WriteValue("maxComputers", _maxComputers);
	writer->WriteValue("upgradeMaxLevel", _upgradeMaxLevel);
	writer->WriteValue("weaponMaxLevel", _weaponMaxLevel);
	writer->WriteValue("springBorders", _springBorders);
	writer->WriteValue("lapsCount", _lapsCount);
	writer->WriteValue("enableHUD", _enableHUD);
	writer->WriteValue("enableMineBug", _enableMineBug);
	writer->WriteValue("disableVideo", _disableVideo);
	writer->WriteValue("fullScreen", fullScreen());

	writer->WriteValue("language", _language);
	writer->WriteValue("commentatorStyle", _commentatorStyle);

	lsl::SWriteEnum(writer, "prefCamera", _prefCamera, cPrefCameraStr, cPrefCameraEnd);
	writer->WriteValue("cameraDistance", _cameraDistance);

	writer->WriteValue("discreteVideoCard", _world->GetGraph()->discreteVideoCard());

	lsl::SWriter* controls = writer->NewDummyNode("controls");
	if (controls)
	{
		for (int i = 0; i < cControllerTypeEnd; ++i)
		{
			lsl::SWriter* controller = controls->NewDummyNode(cControllerTypeStr[i].c_str());

			for (int j = 0; j < cGameActionEnd; ++j)
				controller->WriteValue(cGameActionStr[j].c_str(), _world->GetControl()->GetGameActionInfo((ControllerType)i, (GameAction)j).name);
		}
	}
}

void GameMode::LoadGameOpt(lsl::SReader* reader, bool discreteVideoChanges)
{
	int intVal = 0;
	bool bVal = false;
	float fVal = 0;
	lsl::Point pnt;
	lsl::string str;
	Environment::SyncFrameRate syncFrameRate;

	lsl::SReader* quality = reader->ReadValue("quality");
	if (quality)
	{
		if (quality->ReadValue("filtering", intVal))
			_world->GetEnv()->SetFiltering(Environment::Filtering(intVal));
		if (quality->ReadValue("msaa", intVal))
			_world->GetEnv()->SetMultisampling(Environment::Multisampling(intVal));
		if (quality->ReadValue("shadow", intVal))
			_world->GetEnv()->SetShadowQuality(Environment::Quality(intVal));
		if (quality->ReadValue("environment", intVal))
			_world->GetEnv()->SetEnvQuality(Environment::Quality(intVal));
		if (quality->ReadValue("light", intVal))
			_world->GetEnv()->SetLightQuality(Environment::Quality(intVal));
		if (quality->ReadValue("postEffect", intVal))
			_world->GetEnv()->SetPostEffQuality(Environment::Quality(intVal));		
		if (lsl::SReadEnum(quality, "frameRateMode", syncFrameRate, Environment::cSyncFrameRateStr, Environment::cSyncFrameRateEnd))
			_world->GetEnv()->SetSyncFrameRate(syncFrameRate);
	}
	else
		_world->GetEnv()->AutodetectQuality();

	if (lsl::SReadValue(reader, "resolution", pnt))
	{
		View::Desc desc = _world->GetView()->GetDesc();
		if (desc.resolution.x != pnt.x || desc.resolution.y != pnt.y)
		{
			desc.resolution = pnt;
			_world->GetView()->Reset(desc);
		}
	}

	lsl::SReader* volume = reader->ReadValue("volume");
	if (volume)
	{
		if (volume->ReadValue("musicVolume", fVal))
			_world->GetLogic()->SetVolume(Logic::scMusic, fVal);
		if (volume->ReadValue("effectsVolume", fVal))
			_world->GetLogic()->SetVolume(Logic::scEffects, fVal);
		if (volume->ReadValue("voiceVolume", fVal))
			_world->GetLogic()->SetVolume(Logic::scVoice, fVal);
	}
	else
		_world->GetLogic()->AutodetectVolume();

	reader->ReadValue("maxPlayers", _maxPlayers);
	reader->ReadValue("maxComputers", _maxComputers);

	if (reader->ReadValue("upgradeMaxLevel", intVal))
		upgradeMaxLevel(intVal);
	if (reader->ReadValue("weaponMaxLevel", intVal))
		weaponMaxLevel(intVal);
	if (reader->ReadValue("springBorders", bVal))
		springBorders(bVal);

	if (reader->ReadValue("lapsCount", intVal))
		lapsCount(intVal);

	reader->ReadValue("enableHUD", _enableHUD);
	reader->ReadValue("enableMineBug", _enableMineBug);
	reader->ReadValue("disableVideo", _disableVideo);

	if (reader->ReadValue("fullScreen", bVal))
		fullScreen(bVal);

	if (reader->ReadValue("language", str) && !str.empty())
		SetLanguage(str);
	else
		AutodetectLanguage();

	if (reader->ReadValue("commentatorStyle", str) && !str.empty())
		SetCommentatorStyle(str);
	else
		AutodetectCommentatorStyle();

	if (!lsl::SReadEnum(reader, "prefCamera", _prefCamera, cPrefCameraStr, cPrefCameraEnd))
		_prefCameraAutodetect = true;

	reader->ReadValue("cameraDistance", _cameraDistance);

	if (discreteVideoChanges)
	{
		bool discreteVideoCard;
		if (reader->ReadValue("discreteVideoCard", discreteVideoCard))
			_discreteVideoChanged = _world->GetGraph()->discreteVideoCard() != discreteVideoCard;
		else
			_discreteVideoChanged = true;
	}

	lsl::SReader* controls = reader->ReadValue("controls");
	if (controls)
		for (int i = 0; i < cControllerTypeEnd; ++i)
		{
			lsl::SReader* controller = controls->ReadValue(cControllerTypeStr[i].c_str());

			if (controller)
				for (int j = 0; j < cGameActionEnd; ++j)
				{
					lsl::string key;
					if (controller->ReadValue(cGameActionStr[j].c_str(), key))
					{
						VirtualKey virtualKey = _world->GetControl()->GetVirtualKeyFromName((ControllerType)i, key);
						_world->GetControl()->SetGameKey((ControllerType)i, (GameAction)j, virtualKey);
					}
				}
		}
}

void GameMode::SaveConfig(lsl::SWriter* writer)
{
	SaveGameOpt(writer);

	_menuMusic->SaveUser(writer->NewDummyNode("menuMusic"));
	_gameMusic->SaveUser(writer->NewDummyNode("gameMusic"));
}

void GameMode::LoadConfig(lsl::SReader* reader, bool discreteVideoChanges)
{
	LoadGameOpt(reader, discreteVideoChanges);

	lsl::SReader* menuMusic = reader->ReadValue("menuMusic");
	if (menuMusic)
		_menuMusic->LoadUser(menuMusic);

	lsl::SReader* gameMusic = reader->ReadValue("gameMusic");
	if (gameMusic)
		_gameMusic->LoadUser(gameMusic);
}

void GameMode::ResetConfig()
{
	AutodetectLanguage();
	AutodetectCommentatorStyle();
	_world->GetEnv()->AutodetectQuality();
	_world->GetLogic()->AutodetectVolume();

	_prefCamera = pcIsometric;	
	_cameraDistance = 1.25f;
	_discreteVideoChanged = true;
	_prefCameraAutodetect = true;
}

void GameMode::SaveGameData(lsl::SWriter* writer)
{	
	lsl::SWriter* languages = writer->NewDummyNode("languages");

	for (Languages::const_iterator iter = _languages.begin(); iter != _languages.end(); ++iter)
	{
		lsl::SWriter* language = languages->NewDummyNode(iter->name.c_str());
		language->WriteValue("file", iter->file);		
		language->WriteValue("locale", iter->locale);
		lsl::SWriteEnum(language, "charset", iter->charset, cLangCharsetStr, cLangCharsetEnd);
		language->WriteValue("primId", iter->primId);
	}

	lsl::SWriter* commentators = writer->NewDummyNode("commentators");

	for (CommentatorStyles::const_iterator iter = _commentatorStyles.begin(); iter != _commentatorStyles.end(); ++iter)
	{
		lsl::SWriter* style = commentators->NewDummyNode(iter->name.c_str());
		//style->WriteValue("file", iter->file);
	}

	_menuMusic->SaveGame(writer->NewDummyNode("menuMusic"));
	_gameMusic->SaveGame(writer->NewDummyNode("gameMusic"));	
	_commentator->SaveGame(writer->NewDummyNode("commentator"));
}

void GameMode::LoadGameData(lsl::SReader* reader)
{
	_languages.clear();

	lsl::SReader* languages = reader->ReadValue("languages");
	if (languages)
	{
		lsl::SReader* language = languages->FirstChildValue();
		while (language)
		{
			Language item;
			item.charset = lcDefault;

			item.name = language->GetMyName();
			language->ReadValue("file", item.file);				
			language->ReadValue("locale", item.locale);
			lsl::SReadEnum(language, "charset", item.charset, cLangCharsetStr, cLangCharsetEnd);
			language->ReadValue("primId", item.primId);
			_languages.push_back(item);

			language = language->NextValue();
		}
	}

	_commentatorStyles.clear();

	lsl::SReader* commentators = reader->ReadValue("commentators");
	if (commentators)
	{
		lsl::SReader* commentator = commentators->FirstChildValue();
		while (commentator)
		{
			CommentatorStyle item;
			item.name = commentator->GetMyName();
			_commentatorStyles.push_back(item);

			commentator = commentator->NextValue();
		}
	}

	lsl::SReader* menuMusic = reader->ReadValue("menuMusic");
	if (menuMusic)
		_menuMusic->LoadGame(menuMusic);

	lsl::SReader* gameMusic = reader->ReadValue("gameMusic");
	if (gameMusic)
		_gameMusic->LoadGame(gameMusic);

	lsl::SReader* commentator = reader->ReadValue("commentator");
	if (commentator)
		_commentator->LoadGame(commentator);
}

void GameMode::ResetGameData()
{
	_languages.clear();
	Language lang;

	lang.name = "english";
	lang.file = "Data\\english.txt";
	lang.primId = 9;
	lang.locale = "english";
	lang.charset = lcEastEurope;
	_languages.push_back(lang);

	lang.name = "russian";
	lang.file = "Data\\russian.txt";
	lang.primId = 25;
	lang.locale = "russian";
	lang.charset = lcRussian;
	_languages.push_back(lang);

	lang.name = "portuguese";
	lang.file = "Data\\portuguese.txt";
	lang.primId = 22;
	lang.locale = "portuguese";
	lang.charset = lcEastEurope;
	_languages.push_back(lang);	

	_commentatorStyles.clear();
	CommentatorStyle commentatorStyle;

	commentatorStyle.name = "russian";
	_commentatorStyles.push_back(commentatorStyle);

	_commentator->Clear();
	_commentator->Add(cEventNameMap[cRaceStartTime1], "Voice\\start1.ogg", 100, 0, baSkip, true, 50);
	_commentator->AddVoice(cEventNameMap[cRaceStartTime1], "Voice\\start2.ogg", 50);
	//
	_commentator->Add(cEventNameMap[cRaceFinish], "Voice\\finish1.ogg", 100, 0, baQueue);
	_commentator->Add(cEventNameMap[cRaceLastLap], "Voice\\lastLap1.ogg", 100, 0, baQueue);
 
	_commentator->Add(cEventNameMap[cPlayerOverboard], "Voice\\lowLuck1.ogg", 100, 0, baSkip, true, 33, false, false, true);
	_commentator->AddVoice(cEventNameMap[cPlayerOverboard], "Voice\\overboard1.ogg", 33, false, false, true);
	_commentator->AddVoice(cEventNameMap[cPlayerOverboard], "Voice\\playerLostControl1.ogg", 33, true, false, true);
	//
	_commentator->Add(cEventNameMap[cPlayerDeathMine], "Voice\\lowLuck1.ogg", 100, 0, baSkip, true, 0, false, false, true);
	_commentator->Add(cEventNameMap[cPlayerKill], "Voice\\playerKill1.ogg", 100, 0, baSkip, true, 0, false, false, true);
	
	_commentator->Add(cEventNameMap[cPlayerMoveInverse], "Voice\\playerMoveInverse1.ogg", 100, 0, baQueue, false, 0, true);
	_commentator->Add(cEventNameMap[cPlayerLostControl], "Voice\\playerLostControl1.ogg", 100, 0, baSkip, true, 0, true);
	_commentator->Add(cEventNameMap[cPlayerLeadFinish], "Voice\\leaderFinish1.ogg", 100, 0, baQueue, true, 0, true);
	_commentator->Add(cEventNameMap[cPlayerLeadChanged], "Voice\\leaderChanged1.ogg", 100, 0, baQueue, true);
	_commentator->Add(cEventNameMap[cPlayerLastFar], "Voice\\lastFar1.ogg", 100, 0, baSkip, false, 0, true, false);
	_commentator->Add(cEventNameMap[cPlayerLowLife], "Voice\\lowLife1.ogg", 100, 40, baSkip, true, 0, true, false);
	_commentator->Add(cEventNameMap[cPlayerDeath], "Voice\\death1.ogg", 100, 40, baSkip, true, 0, true, false);
	_commentator->Add(cEventNameMap[cPlayerFinishFirst], "Voice\\finishFirst1.ogg", 100, 0, baSkip, true, 0, true, false);
	_commentator->Add(cEventNameMap[cPlayerFinishSecond], "Voice\\finishSecond1.ogg", 100, 0, baSkip, true, 0, true, false);
	_commentator->Add(cEventNameMap[cPlayerFinishThird], "Voice\\finishLast1.ogg", 100, 0, baSkip, true, 0, true, false);
	_commentator->Add("svRip", "Voice\\rip.ogg", 100, 0);
	_commentator->Add("svSnake", "Voice\\snake.ogg", 100, 0);
	_commentator->Add("svTyler", "Voice\\tailer.ogg", 100, 0);
	_commentator->Add("svTarkvin", "Voice\\tarkvin.ogg", 100, 0);
	
	_menuMusic->ClearTracks();
	_menuMusic->AddTrack(Track(GetSound("Music\\Track1.ogg"), "Cold Hard Bitch", "Jet"));
	_menuMusic->AddTrack(Track(GetSound("Music\\Track14.ogg"), "Angel's wings (acoustic)", "Social Distortion"));
	_menuMusic->AddTrack(Track(GetSound("Music\\Track15.ogg"), "On our Way", "Stereoside"));

	_gameMusic->ClearTracks();
	_gameMusic->AddTrack(Track(GetSound("Music\\Track1.ogg"), "Cold Hard Bitch", "Jet"));
	_gameMusic->AddTrack(Track(GetSound("Music\\Track2.ogg"), "Hell Yeah", "Rev Theory"));
	_gameMusic->AddTrack(Track(GetSound("Music\\Track3.ogg"), "I Won't Back Down (Heartbreakers cover)", "Edgewater"));
	_gameMusic->AddTrack(Track(GetSound("Music\\Track4.ogg"), "Gentleman", "Theory of a Deadman"));
	_gameMusic->AddTrack(Track(GetSound("Music\\Track5.ogg"), "Turn It Up", "Fallzone"));
	_gameMusic->AddTrack(Track(GetSound("Music\\Track6.ogg"), "What I Want (feat Slash)", "Daughtry"));
	_gameMusic->AddTrack(Track(GetSound("Music\\Track7.ogg"), "The thunder rolls", "Jet black stare"));
	_gameMusic->AddTrack(Track(GetSound("Music\\Track8.ogg"), "Can't Take It With You", "Social Distortion"));
	_gameMusic->AddTrack(Track(GetSound("Music\\Track9.ogg"), "Born To Be Wild (Steppenwolf cover) (Bonus Track)", "Hinder"));
	_gameMusic->AddTrack(Track(GetSound("Music\\Track10.ogg"), "Burn", "Lucio Antolini"));
	_gameMusic->AddTrack(Track(GetSound("Music\\Track11.ogg"), "Fight", "No vacancy"));
	_gameMusic->AddTrack(Track(GetSound("Music\\Track12.ogg"), "Highway Star ('Deep Purple cover)", "Buckcherry"));
	_gameMusic->AddTrack(Track(GetSound("Music\\Track13.ogg"), "Watch me burn", "Lansdowne"));
}

void GameMode::SaveGameData()
{
	lsl::RootNode node("root", _world);

	node.BeginSave();
	SaveGameData(&node);
	node.EndSave();

	lsl::SerialFileXML file;
	file.SaveNodeToFile(node, "game.xml");
}

void GameMode::LoadGameData()
{
	try
	{
		lsl::SerialFileXML file;
		lsl::RootNode node("root", _world);

		file.LoadNodeFromFile(node, "game.xml");

		node.BeginLoad();	
		LoadGameData(&node);
		node.EndLoad();
	}
	catch (const lsl::EUnableToOpen&) 
	{
		ResetGameData();
		SaveGameData();
	}
}

bool GameMode::OnHandleInput(const InputMessage& msg)
{
	if (msg.state != ksDown || msg.repeat)
		return false;

	if (msg.action == gaEscape)
	{
		if (_startUpTime == -9)
		{
			_world->GetVideo()->Stop();	
			_startUpTime = -10;
		}
		else if (_movieTime == 7)
		{
			_world->GetVideo()->Stop();	
			_movieTime = 8;
		}
		else if (_guiLogo && _startUpTime >= 0.0f)
		{
			//if (_guiLogo->GetMaterial().GetAlpha() > 0.15f)
			//	_startUpTime = 7;
			//else if (_guiLogo2->GetMaterial().GetAlpha() > 0.15f)
				_startUpTime = -2;
		}

		return false;
	}

#ifndef _RETAIL
	if (msg.action == gaDebug1)
	{
		if (_race)
			_race->ResetCarPos();

		DISPLAY_DEVICE dd;
		dd.cb = sizeof(DISPLAY_DEVICE);

		/*unsigned i = 0;
		while (EnumDisplayDevices(NULL, i, &dd, 0))
		{
			LSL_LOG(lsl::StrFmt("device i=%d name=%s desc=%s primary=%d active=%d mirroring=%d removable=%d vga=%d pruned=%d", i, dd.DeviceName, dd.DeviceString, (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) != 0, (dd.StateFlags & DISPLAY_DEVICE_ACTIVE) != 0, (dd.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) != 0, (dd.StateFlags & DISPLAY_DEVICE_REMOVABLE) != 0, (dd.StateFlags & DISPLAY_DEVICE_VGA_COMPATIBLE) != 0, (dd.StateFlags & DISPLAY_DEVICE_MODESPRUNED) != 0));		
			++i;		
		}*/

		return true;
	}

	if (msg.action == gaDebug2)
	{
		bool enb = _world->GetGraph()->GetGraphOption(GraphManager::goBloom);

		_world->GetGraph()->SetGraphOption(GraphManager::goBloom, !enb, GraphManager::gqHigh);
		_world->GetGraph()->SetGraphOption(GraphManager::goHDR, !enb, GraphManager::gqHigh);
	}

	if (msg.action == gaDebug3)
	{
		View::Desc desc = _world->GetView()->GetDesc();
		desc.fullscreen = !desc.fullscreen;
		_world->GetView()->Reset(desc);
	}

	if (msg.action == gaDebug4)
	{
		//_steamService->Sync();		
	}

	if (msg.action == gaDebug5)
	{
		//_steamService->steamStats()->ResetAll();
		//_steamService->Sync();
	}
#endif

	return false;
}

void GameMode::RegFixedStepEvent(IFixedStepEvent* user)
{
	_world->RegFixedStepEvent(user);
}

void GameMode::UnregFixedStepEvent(IFixedStepEvent* user)
{
	_world->UnregFixedStepEvent(user);
}

void GameMode::RegProgressEvent(IProgressEvent* user)
{
	_world->RegProgressEvent(user);
}

void GameMode::UnregProgressEvent(IProgressEvent* user)
{
	_world->UnregProgressEvent(user);
}

void GameMode::RegLateProgressEvent(ILateProgressEvent* user)
{
	_world->RegLateProgressEvent(user);
}

void GameMode::UnregLateProgressEvent(ILateProgressEvent* user)
{
	_world->UnregLateProgressEvent(user);
}

void GameMode::RegFrameEvent(IFrameEvent* user)
{
	_world->RegFrameEvent(user);
}

void GameMode::UnregFrameEvent(IFrameEvent* user)
{
	_world->UnregFrameEvent(user);
}

void GameMode::RegUser(IGameUser* user)
{
	if (_users.IsFind(user))
		return;

	user->AddRef();
	_users.Insert(user);
}

void GameMode::UnregUser(IGameUser* user)
{
	if (!_users.IsFind(user))
		return;

	_users.Remove(user);
	user->Release();
}

void GameMode::SendEvent(unsigned id, EventData* data)
{
	for (Users::Position pos = _users.First(); IGameUser** iter = _users.Current(pos); _users.Next(pos))
		(*iter)->OnProcessEvent(id, data);
}

void GameMode::Run(bool playIntro)
{
	if (_startGame || _startUpTime != -1)
		return;
	_startUpTime = 0;

	if (playIntro)
	{
		_guiLogo = _world->GetGraph()->GetGUI().CreatePlaneFon();
		_guiLogo->GetMaterial().GetSampler().GetOrCreateTex()->GetOrCreateData()->SetFileName("Data\\GUI\\yardLogo.png");
		_guiLogo->GetMaterial().GetSampler().GetTex()->Init(_world->GetGraph()->GetEngine());
		_guiLogo->GetMaterial().SetBlending(gui::Material::bmTransparency);
		_guiLogo->GetMaterial().SetColor(D3DXCOLOR(1, 1, 1, 0));

		_guiLogo2 = _world->GetGraph()->GetGUI().CreatePlaneFon();
		_guiLogo2->GetMaterial().GetSampler().GetOrCreateTex()->GetOrCreateData()->SetFileName("Data\\GUI\\laboratoria24.png");		
		_guiLogo2->GetMaterial().GetSampler().GetTex()->Init(_world->GetGraph()->GetEngine());
		_guiLogo2->GetMaterial().SetBlending(gui::Material::bmTransparency);
		_guiLogo2->GetMaterial().SetColor(D3DXCOLOR(1, 1, 1, 0));
	}

	_guiStartup = _world->GetGraph()->GetGUI().CreatePlaneFon();
	_guiStartup->GetMaterial().GetSampler().GetOrCreateTex()->GetOrCreateData()->SetFileName("Data\\GUI\\startLogo.dds");
	_guiStartup->GetMaterial().GetSampler().GetTex()->Init(_world->GetGraph()->GetEngine());
	_guiStartup->GetMaterial().GetSampler().SetFiltering(graph::Sampler2d::sfLinear);
	_guiStartup->GetMaterial().SetBlending(gui::Material::bmTransparency);	
	_guiStartup->GetMaterial().SetColor(D3DXCOLOR(1, 1, 1, 0));

	AdjustGameStartup();
	_world->GetGraph()->SetGUIMode(true);

	_menuMusic->Play(false);
}

void GameMode::Terminate()
{
	SaveConfig();
	_world->Terminate();
}

bool GameMode::IsStartgame() const
{
	return _startGame;
}

unsigned GameMode::time() const
{
	return _world->time();
}

void GameMode::SaveConfig()
{
	lsl::RootNode node("root", _world);

	node.BeginSave();
	SaveConfig(&node);
	node.EndSave();

	lsl::SerialFileXML file;
	file.SaveNodeToFile(node, "user.xml");
}

void GameMode::LoadConfig(bool discreteVideoChanges)
{
	try
	{
		lsl::SerialFileXML file;
		lsl::RootNode node("root", _world);

		file.LoadNodeFromFile(node, "user.xml");

		node.BeginLoad();	
		LoadConfig(&node, discreteVideoChanges);
		node.EndLoad();
	}
	catch (const lsl::EUnableToOpen&) 
	{
		ResetConfig();
		SaveConfig();
	}
}

void GameMode::SaveGame(bool saveProfile)
{	
	if (saveProfile)
	{
		_race->SaveLib();
		_race->SaveProfile();
		_race->GetAchievment().SaveLib();
	}

#ifdef STEAM_SERVICE
	_steamService->Sync();
#endif
}

void GameMode::StartMatch(Race::Mode mode, Difficulty difficulty, Race::Profile* profile, bool createPlayers, bool netGame, bool netClient)
{
	if (profile)
	{
		_race->EnterProfile(profile, mode);
		_race->LoadProfile();
	}
	else
		_race->NewProfile(mode, netGame, netClient);

	if (difficulty != cDifficultyEnd)
		_race->GetProfile()->difficulty(difficulty);

	_race->GetGarage().SetUpgradeMaxLevel(_upgradeMaxLevel);
	_race->GetGarage().SetWeaponMaxLevel(_weaponMaxLevel);
	_race->SetSpringBorders(_springBorders);
	_race->SetEnableMineBug(_enableMineBug);
	_race->GetTournament().SetLapsCount(_lapsCount);

	if (createPlayers)
		_race->CreatePlayers(mode == Race::rmSkirmish ? _maxComputers : Race::cCampaignMaxPlayers - 1);
}

void GameMode::ExitMatch(bool saveGame)
{
	Race::Profile* emptyProfile = _race->GetProfile();
	if (emptyProfile && _race->FindProfile(emptyProfile) == _race->GetProfiles().end())
		emptyProfile = NULL;
	if (emptyProfile)
		for (Race::PlayerList::const_iterator iter = _race->GetPlayerList().begin(); iter != _race->GetPlayerList().end(); ++iter)
		{
			if ((*iter)->IsHuman() || (*iter)->IsOpponent())
			{
				emptyProfile = NULL;
				break;
			}
		}

	if (emptyProfile)
		emptyProfile->AddRef();
	else
		SaveGame(saveGame);

	_race->ExitProfile();

	if (emptyProfile)
	{
		emptyProfile->Release();
		_race->DelProfile(emptyProfile);
		SaveGame(true);
	}
}

bool GameMode::IsMatchStarted() const
{
	return _race->IsMatchStarted();	
}

void GameMode::StartRace()
{
	_startRace = 0;
	_goRaceTime = -1;
	_finishTime = -1;

	_menu->SetState(Menu::msInfo);	
}

void GameMode::ExitRace(bool saveGame, const Race::Results* results)
{
	if (_race == NULL || !_race->IsStartRace())
		return;

	_finishTime = -1;
	_startRace = -1;
	_goRaceTime = -1;

	_commentator->Stop();
	_gameMusic->Stop();
	SetSemaphore(NULL);
	_race->ExitRace(results);	

	SaveGame(saveGame);
}

void GameMode::ExitRaceGoFinish()
{
	_menu->SetState(Menu::msFinish2);
}

void GameMode::GoRace(int stage)
{
	D3DXCOLOR semaphoreColor = clrWhite;
	int semaphoreIndex = 0;

	switch (stage)
	{
	case cGoRaceWait:
		semaphoreColor = clrRed;
		semaphoreIndex = 1;
		SendEvent(cRaceStartWait, NULL);
		break;

	case cGoRace1:
		semaphoreColor = clrRed;
		semaphoreIndex = 1;
		SendEvent(cRaceStartTime1, NULL);
		break;

	case cGoRace2:
		semaphoreColor = clrYellow;
		semaphoreIndex = 2;
		SendEvent(cRaceStartTime2, NULL);
		break;

	case cGoRace3:
		semaphoreColor = clrYellow;
		semaphoreIndex = 2;
		SendEvent(cRaceStartTime3, NULL);
		break;

	case cGoRace:
		semaphoreColor = clrGreen;
		semaphoreIndex = 3;
		_goRaceTime = -1;
		_race->GoRace();
		SendEvent(cRaceStart, NULL);
		break;
	}

	if (_semaphore)
	{
		graph::SceneNode::Nodes::const_iterator iter = _semaphore->GetGameObj().GetGrActor().GetNodes().begin();

		for (int i = 0; i < 4; ++i, ++iter)
		{
			(*iter)->GetMaterial()->SetColor(i == semaphoreIndex ? semaphoreColor : clrWhite);
		}
	}
}

void GameMode::GoRaceTimer()
{
	_goRaceTime = 0;
}

void GameMode::RunFinishTimer()
{
	_finishTime = 0;
}

bool GameMode::IsStartRace() const
{
	return _startGame && _race->IsStartRace();
}

bool GameMode::IsRaceFinish() const
{
	return _startGame && IsStartRace() && _finishTime != -1;
}

bool GameMode::ChangePlanet(Planet* planet)
{
	if (_race->GetPlanetChampion() && planet == _race->GetTournament().GetNextPlanet())
		planet->Unlock();

	if (_race->GetTournament().ChangePlanet(planet))
	{
		_race->ResetChampion();
		return true;
	}

	return false;
}

void GameMode::Pause(bool pause)
{
	_world->Pause(pause);
	_world->GetLogic()->Mute(Logic::scEffects, pause);
}

bool GameMode::IsPaused() const
{
	return _world->IsPaused();
}

void GameMode::PlayMusic(const Track& track, snd::Source::Report* report, snd::seek_pos pos, bool showInfo)
{
	StopMusic();

	_music->SetSound(track.sound, true);

	if (lsl::Object::ReplaceRef(_musicReport, report))
	{
		_musicReport = report;
		_music->RegReport(report);
	}

	//_music->SetPos(6000000);
	_music->SetPos(pos);
	_music->Play();

	if (showInfo && _startGame)
		_menu->ShowMusicInfo(track.band, track.name);
}

void GameMode::StopMusic()
{
	if (_musicReport)
	{
		_music->UnregReport(_musicReport);
		lsl::SafeRelease(_musicReport);
	}

	_music->Stop();	

	snd::Sound* lastSound = _music->GetSound();
	_music->SetSound(NULL, true);
	if (lastSound)
		lastSound->Free();
}

void GameMode::FadeInMusic(float sVolume, float speed)
{
	_fadeMusic = 0.0f;
	_fadeSpeedMusic = speed;

	if (sVolume >= 0)
		_music->SetVolume(sVolume);
}

void GameMode::FadeOutMusic(float sVolume, float speed)
{
	_fadeMusic = 1.0f;
	_fadeSpeedMusic = speed;

	if (sVolume >= 0)
		_music->SetVolume(sVolume);
}

snd::Sound* GameMode::GetSound(const lsl::string& name, bool assertFind)
{
	snd::Sound* snd = _world->GetResManager()->GetSoundLib().Find(name);

	LSL_ASSERT(!assertFind || snd);

	return snd;
}

void GameMode::PlayMovie(const std::string& name)
{
	_movieFileName = name;
	_movieTime = 0.0f;
}

bool GameMode::IsMoviePlaying() const
{
	return _movieTime != -1;
}

void GameMode::CheckStartupMenu()
{
	if (_prefCameraAutodetect)
	{
		_menu->ShowStartOptions(true);
		_prefCameraAutodetect = false;
	}
	else if (_discreteVideoChanged)
	{
		if (GetWorld()->GetGraph()->discreteVideoCard())
			GetWorld()->GetEnv()->SetSyncFrameRate(Environment::sfrFixed);
		else
			_menu->ShowDiscreteVideoCardMessage();
		_discreteVideoChanged = false;
	}
}

void GameMode::OnResetView()
{
	AdjustGameStartup();

	if (_menu)
		_menu->OnResetView();
}

void GameMode::OnFinishFrameClose()
{
	_commentator->Stop();
	FadeOutMusic(0);
	_menuMusic->Pause(false);
}

void GameMode::OnFrame(float deltaTime, float pxAlpha)
{	
	const float logoDelay = 1.0f;
	const float logoFadeTime = 1.0f;
	const float logoTime = 3.0f;
	const float cGoRaceLag = 1.0f;

	if (_world->IsPaused())
		return;

	if (_startUpTime != -1)
	{
		if (_startUpTime == -14)
		{
			_world->SetVideoMode(false);
			_menuMusic->Pause(false);
			StartGame();
		}
		else if (_startUpTime >= -13 && _startUpTime <= -10)
		{
			if (_startUpTime == -11)
				_world->GetVideo()->Unload();
			--_startUpTime;
		}
		else if (_startUpTime == -9)
		{
			//nothing
		}
		else if (_startUpTime == -8)
		{
			_world->GetVideo()->Open("Data\\Video\\main.avi");
			_world->GetVideo()->Play();
			_startUpTime = -9;
		}
		else if (_startUpTime >= -7 && _startUpTime <= -4)
		{
			--_startUpTime;
		}
		else if (_startUpTime == -3)
		{
			PrepareGame();

			//if (_guiLogo)
			//{
			//	_world->SetVideoMode(true);
			//	_menuMusic->Pause(true);
			//	_startUpTime = -4;
			//}
			//else
				StartGame();
		}
		else if (_startUpTime == -2)
		{
			_guiStartup->GetMaterial().SetColor(D3DXCOLOR(1, 1, 1, 1));
			_startUpTime = -3;
		}
		else if (_guiLogo)
		{			
			_startUpTime += deltaTime;
			float logo2Delay = logoFadeTime + logoTime + logoFadeTime + logoDelay + logoDelay;

			if (_startUpTime < logoFadeTime + logoDelay)
			{
				_guiLogo->GetMaterial().SetAlpha(std::max((_startUpTime - logoDelay)/logoFadeTime, 0.0f));
				_guiLogo2->GetMaterial().SetAlpha(0.0f);
			}
			else if (_startUpTime < logoFadeTime + logoTime + logoDelay)
			{
				_guiLogo->GetMaterial().SetAlpha(1.0f);
				_guiLogo2->GetMaterial().SetAlpha(0.0f);
			}
			else if (_startUpTime < logoFadeTime + logoTime + logoFadeTime + logoDelay)
			{
				_guiLogo->GetMaterial().SetAlpha(1.0f - (_startUpTime - logoFadeTime - logoTime - logoDelay)/logoFadeTime);
				_guiLogo2->GetMaterial().SetAlpha(0.0f);
			}
			else if (_startUpTime < logoFadeTime + logo2Delay)
			{
				_guiLogo2->GetMaterial().SetAlpha(std::max((_startUpTime - logo2Delay)/logoFadeTime, 0.0f));
				_guiLogo->GetMaterial().SetAlpha(0.0f);
			}
			else if (_startUpTime < logoFadeTime + logoTime + logo2Delay)
			{
				_guiLogo2->GetMaterial().SetAlpha(1.0f);
				_guiLogo->GetMaterial().SetAlpha(0.0f);
			}
			else if (_startUpTime < logoFadeTime + logoTime + logoFadeTime + logo2Delay)
			{
				_guiLogo2->GetMaterial().SetAlpha(1.0f - (_startUpTime - logoFadeTime - logoTime - logo2Delay)/logoFadeTime);
				_guiLogo->GetMaterial().SetAlpha(0.0f);
			}
			else
				_startUpTime = -2;
		}
		else
			_startUpTime = -2;
	} 
	else if (_startGame && !IsMoviePlaying())
	{
		_menu->OnProgress(deltaTime);
		_commentator->OnProgress(deltaTime);

		float dtVolume = (_fadeMusic - _music->GetVolume()) * deltaTime / (_fadeSpeedMusic > 0 ? _fadeSpeedMusic : 1.0f);
		if (dtVolume != 0)
			_music->SetVolume(ClampValue(_music->GetVolume() + dtVolume, 0.0f, 1.0f));

		if (_startRace >= 0 && (++_startRace) > 1)
			DoStartRace();

		if (_goRaceTime >= 0)
		{
			int stage1 = Floor<int>(_goRaceTime - cGoRaceLag);
			int stage2 = Floor<int>((_goRaceTime += deltaTime) - cGoRaceLag);
			if (stage1 != stage2)
			{
				int stage = cGoRace1 + stage2;
				GoRace(stage);
			}
		}

		if (_finishTime >= 0 && (_finishTime += deltaTime) > 3.0f)
		{
			_finishTime = -1;
			SendEvent(cRaceFinishTimeEnd);
		}
	}

	if (_movieTime != -1)
	{
		//wait one frames
		if (_movieTime == 0)
		{
			if (_world->GetView()->GetDesc().fullscreen)
			{
				graph::DisplayMode screenMode = _world->GetGraph()->GetScreenMode();
				View::SetWindowSize(_world->GetView()->GetDesc().handle, lsl::Point(screenMode.width, screenMode.height), true);

				InvalidateRect(_world->GetView()->GetDesc().handle, NULL, true);
				UpdateWindow(_world->GetView()->GetDesc().handle);
			}
			_movieTime = 1;
		}
		else if (_movieTime == 1)
		{			
			_world->SetVideoMode(true);
			_menuMusic->Pause(true);

			InvalidateRect(_world->GetView()->GetDesc().handle, NULL, true);
			UpdateWindow(_world->GetView()->GetDesc().handle);

			_movieTime = 2;
		}
		else if (_movieTime >= 2 && _movieTime <= 5)
		{
			++_movieTime;
		}
		else if (_movieTime == 6)
		{
			_world->GetVideo()->Open(_movieFileName);
			_world->GetVideo()->Play();
			_world->ResetInput();
			_movieTime = 7;
		}
		else if (_movieTime == 7)
		{
			//nothing
		}
		else if (_movieTime >= 8 && _movieTime <= 11)
		{
			if (_movieTime == 9)
				_world->GetVideo()->Unload();
			++_movieTime;
		}
		else if (_movieTime == 12)
		{
			_world->SetVideoMode(false);
			_menuMusic->Pause(false);
			_movieTime = -1;

			SendEvent(cVideoStopped);
		}
	}
}

void GameMode::OnGraphEvent(HWND hwnd, long eventCode, LONG_PTR param1, LONG_PTR param2)
{
	switch (eventCode)
	{		
	case EC_COMPLETE:
	case EC_USERABORT:
	case EC_ERRORABORT:		
		if (_startUpTime != -1)
			_startUpTime = -10;
		if (_movieTime != -1)
			_movieTime = 8;
		break;	
	}
}

World* GameMode::GetWorld()
{
	return _world;
}

Race* GameMode::GetRace()
{
	return _race;
}

Menu* GameMode::GetMenu()
{
	return _menu;
}

NetGame* GameMode::netGame()
{
	return _netGame;
}

#ifdef STEAM_SERVICE

SteamService* GameMode::steamService()
{
	return _steamService;
}

#endif

GameMode::MusicCat* GameMode::menuMusic()
{
	return _menuMusic;
}

GameMode::MusicCat* GameMode::gameMusic()
{
	return _gameMusic;
}

const Language* GameMode::FindLanguage(const lsl::string& name) const
{
	for (Languages::const_iterator iter = _languages.begin(); iter != _languages.end(); ++iter)
		if (iter->name == name)
			return &(*iter);
	return 0;
}

const Language* GameMode::FindLanguage(int primId) const
{
	for (Languages::const_iterator iter = _languages.begin(); iter != _languages.end(); ++iter)
		if (iter->primId == primId)
			return &(*iter);
	return 0;
}

int GameMode::FindLanguageIndex(const lsl::string& name) const
{
	for (int i = 0; i < (int)_languages.size(); ++i)
		if (_languages[i].name == name)
			return i;
	return -1;
}

const Languages& GameMode::GetLanguages() const
{
	return _languages;
}

const lsl::string& GameMode::GetLanguage() const
{
	return _language;
}

void GameMode::SetLanguage(const lsl::string& value)
{
	if (_language == value)
		return;
	_language = value;
}

const Language* GameMode::GetLanguageParam() const
{
	return FindLanguage(_language);
}

int GameMode::GetLanguageIndex() const
{
	return FindLanguageIndex(_language);
}

void GameMode::AutodetectLanguage()
{
	LANGID lang = GetUserDefaultUILanguage();
	LANGID prim = PRIMARYLANGID(lang);

	const Language* language = FindLanguage(prim);
	if (language == 0 && !_languages.empty())
		language = &_languages.front();

	if (language)		
		SetLanguage(language->name);		

	//SetThreadUILanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
	//SetThreadUILanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
	//setlocale(LC_ALL, "english");
	//_setmbcp(_MB_CP_LOCALE);
	//SetThreadLocale(MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT));
	//GetSystemDefaultLCID	
	//setlocale( LC_ALL, ".1252" ); 
	//pt-BR	
}

void GameMode::ApplyLanguage()
{
	const Language* lang = GetLanguageParam();

	if (lang)
	{
		_setmbcp(_MB_CP_LOCALE);
		char* res = setlocale(LC_ALL, lang->locale.c_str());
		SetThreadLocale(MAKELCID(MAKELANGID(lang->primId, SUBLANG_NEUTRAL), SORT_DEFAULT));

		LSL_LOG(lsl::StrFmt("setlocale=%s lang=%s return=%s", lang->locale.c_str(), _language.c_str(), res != NULL ? res : "null"));

		GetWorld()->GetResManager()->SetFontCharset(lang->charset);
		GetWorld()->GetResManager()->GetStringLib().LoadFromFile(lang->file);
	}
}

const CommentatorStyle* GameMode::FindCommentatorStyle(const lsl::string& name) const
{
	for (CommentatorStyles::const_iterator iter = _commentatorStyles.begin(); iter != _commentatorStyles.end(); ++iter)
		if (iter->name == name)
			return &(*iter);
	return NULL;
}

int GameMode::FindCommentatorStyleIndex(const lsl::string& name) const
{
	for (int i = 0; i < (int)_commentatorStyles.size(); ++i)
		if (_commentatorStyles[i].name == name)
			return i;
	return -1;
}

const CommentatorStyles& GameMode::GetCommentatorStyles() const
{
	return _commentatorStyles;
}

const lsl::string& GameMode::GetCommentatorStyle() const
{
	return _commentatorStyle;
}

void GameMode::SetCommentatorStyle(const lsl::string& value)
{
	if (_commentatorStyle == value)
		return;
	_commentatorStyle = value;

	const CommentatorStyle* style = FindCommentatorStyle(value);
	if (style)
	{
		GetWorld()->GetResManager()->LoadCommentator(*style);
		_commentator->CheckSounds();
	}
}

void GameMode::AutodetectCommentatorStyle()
{
	const Language* lang = GetLanguageParam();

	if (lang && lang->charset == lcRussian && FindCommentatorStyle("russian"))
	{
		SetCommentatorStyle("russian");
	}
	else if (FindCommentatorStyle("english"))
	{
		SetCommentatorStyle("english");
	}
	else
		SetCommentatorStyle(_commentatorStyles.size() > 0 ? _commentatorStyles.front().name : "");
}

GameMode::PrefCamera GameMode::GetPrefCamera() const
{
	return _prefCamera;
}

void GameMode::SetPrefCamera(PrefCamera value)
{
	_prefCamera = value;
}

float GameMode::GetCameraDistance() const
{
	return _cameraDistance;
}

void GameMode::SetCameraDistance(float value)
{
	_cameraDistance = value;
}

unsigned GameMode::maxPlayers() const
{
	return _maxPlayers;
}

void GameMode::maxPlayers(unsigned value)
{
	_maxPlayers = value;
}

unsigned GameMode::maxComputers() const
{
	return _maxComputers;
}

void GameMode::maxComputers(unsigned value)
{
	_maxComputers = value;

	if (IsMatchStarted() && _race->IsSkirmish() && !_netGame->isStarted())
		_race->CreatePlayers(value);
}

int GameMode::upgradeMaxLevel() const
{
	return _upgradeMaxLevel;
}

void GameMode::upgradeMaxLevel(int value)
{	
	_upgradeMaxLevel = value;

	if (_startGame)
		_race->GetGarage().SetUpgradeMaxLevel(_upgradeMaxLevel);
}

int GameMode::weaponMaxLevel() const
{
	return _weaponMaxLevel;
}

void GameMode::weaponMaxLevel(int value)
{
	_weaponMaxLevel = value;

	if (_startGame)
		_race->GetGarage().SetWeaponMaxLevel(_weaponMaxLevel);
}

bool GameMode::springBorders() const
{
	return _springBorders;
}

void GameMode::springBorders(bool value)
{
	_springBorders = value;

	if (_startGame)
		_race->SetSpringBorders(value);
}

Difficulty GameMode::currentDiff() const
{
	return enabledOptionDiff() ? _race->GetProfile()->difficulty() : cDifficultyEnd;
}

void GameMode::currentDiff(Difficulty value)
{
	if (enabledOptionDiff())
		_race->GetProfile()->difficulty(value);
}

bool GameMode::enabledOptionDiff() const
{
	return _race->GetProfile() != NULL;
}

unsigned GameMode::lapsCount() const
{
	return _lapsCount;
}

void GameMode::lapsCount(unsigned value)
{
	_lapsCount = value;

	if (_startGame)
		_race->GetTournament().SetLapsCount(value);
}

bool GameMode::enableHUD() const
{
	return _enableHUD;
}

void GameMode::enableHUD(bool value)
{
	_enableHUD = value;
}

bool GameMode::enableMineBug() const
{
	return _enableMineBug;
}

void GameMode::enableMineBug(bool value)
{
	_enableMineBug = value;
}

bool GameMode::disableVideo() const
{
	return _disableVideo;
}

void GameMode::disableVideo(bool value)
{
	_disableVideo = value;
}

bool GameMode::fullScreen() const
{
	return _world->GetView()->GetDesc().fullscreen;
}

void GameMode::fullScreen(bool value)
{
	View::Desc desc = _world->GetView()->GetDesc();

	if (desc.fullscreen != value)
	{
		desc.fullscreen = value;
		_world->GetView()->Reset(desc);
	}
}

}

}