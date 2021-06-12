#include "stdafx.h"
#include "game\AchievmentModel.h"

#include "game\World.h"
#include "lslSerialFileXML.h"

namespace r3d
{

namespace game
{

const std::string Achievment::cStateStr[cStateEnd] = {"asLocked", "asUnlocked", "asOpened"};

const std::string AchievmentModel::cViper = "viper";
const std::string AchievmentModel::cBuggi = "buggi";
const std::string AchievmentModel::cAirblade = "airblade";
const std::string AchievmentModel::cReflector = "reflector";
const std::string AchievmentModel::cDroid = "droid";
const std::string AchievmentModel::cTankchetti = "tankchetti";
const std::string AchievmentModel::cPhaser = "phaser";
const std::string AchievmentModel::cMustang = "mustang";
const std::string AchievmentModel::cArmor4 = "armor4";

const std::string AchievmentModel::cDoubleKill = "doubleKill";
const std::string AchievmentModel::cTripleKill = "tripleKill";
const std::string AchievmentModel::cDevastator = "devastator";
const std::string AchievmentModel::cMegaRacer = "megaRacer";
const std::string AchievmentModel::cBulletProf = "bulletProf";
const std::string AchievmentModel::cBreakRacer = "breakRacer";
const std::string AchievmentModel::cMedicate = "medicate";
const std::string AchievmentModel::cExplosivo = "explosivo";
const std::string AchievmentModel::cMoneybags = "moneyBags";
const std::string AchievmentModel::cSurvival = "survival";
const std::string AchievmentModel::cFirstBlood = "firstBlood";
const std::string AchievmentModel::cArmored = "armored";




Achievment::Achievment(const Desc& desc): _state(asLocked), _price(0)
{
	_owner = desc.owner;
	_classId = desc.classId;
	_name = desc.name;
}

Achievment::~Achievment()
{
}

void Achievment::ChangeState(State state)
{
	_state = state;
	OnStateChanged();
}

void Achievment::RegProgressEvent()
{
	_owner->game()->RegProgressEvent(this);
}

void Achievment::UnregProgressEvent()
{
	_owner->game()->UnregProgressEvent(this);
}

void Achievment::SaveTo(lsl::SWriter* writer)
{
	lsl::SWriteEnum(writer, "state", _state, cStateStr, cStateEnd);
	writer->WriteValue("price", _price);	
}

void Achievment::LoadFrom(lsl::SReader* reader)
{
	lsl::SReadEnum(reader, "state", _state, cStateStr, cStateEnd);
	reader->ReadValue("price", _price);	
}

void Achievment::Unlock()
{
	if (_state == asLocked)
		ChangeState(asUnlocked);
}

void Achievment::Open()
{
	if (_state == asUnlocked)
		ChangeState(asOpened);
}

bool Achievment::Buy(Player* player)
{
	if (_state == asUnlocked)
	{
		if (_owner->ÑonsumePoints(_price))
		{
			Open();
			return true;
		}
	}

	return false;
}

Achievment::State Achievment::state() const
{
	return _state;
}

int Achievment::price() const
{
	return _price;
}

void Achievment::price(int value)
{
	_price = value;
}

AchievmentModel* Achievment::owner() const
{
	return _owner;
}

unsigned Achievment::classId() const
{
	return _classId;
}

const std::string& Achievment::name() const
{
	return _name;
}

Race* Achievment::race() const
{
	return _owner->race();
}




AchievmentMapObj::AchievmentMapObj(const Desc& desc): Achievment(desc)
{
}

AchievmentMapObj::~AchievmentMapObj()
{
	_records.Clear();
}

void AchievmentMapObj::SaveTo(lsl::SWriter* writer)
{
	Achievment::SaveTo(writer);

	writer->WriteValue("records", &_records);
}

void AchievmentMapObj::LoadFrom(lsl::SReader* reader)
{
	Achievment::LoadFrom(reader);

	reader->ReadValue("records", &_records);
}

Record* AchievmentMapObj::GetRecord()
{
	return _records.Size() > 0 ? _records.front() : NULL;
}

void AchievmentMapObj::SetRecord(Record* value)
{
	if (_records.Size() > 0)
	{
		if (Object::ReplaceRef(_records.front(), value))
			_records.front() = value;
	}
	else
		_records.Insert(value);
}

void AchievmentMapObj::AddRecord(Record* value)
{
	_records.Insert(value);
}

bool AchievmentMapObj::ContainsRecord(Record* value) const
{
	return _records.Find(value) != _records.end();
}

const AchievmentMapObj::Records& AchievmentMapObj::GetRecords() const
{
	return _records;
}




AchievmentGamer::AchievmentGamer(const Desc& desc): Achievment(desc), _gamerId(0)
{
}

void AchievmentGamer::SaveTo(lsl::SWriter* writer)
{
	Achievment::SaveTo(writer);

	writer->WriteValue("gamerId", _gamerId);
}

void AchievmentGamer::LoadFrom(lsl::SReader* reader)
{
	Achievment::LoadFrom(reader);

	reader->ReadValue("gamerId", _gamerId);
}

int AchievmentGamer::GetGamerId()
{
	return _gamerId;
}

void AchievmentGamer::SetGamerId(int value)
{
	_gamerId = value;
}




AchievmentCondition::AchievmentCondition(const Desc& desc): _reward(0), _iterNum(0), _iterCount(1)
{
	_owner = desc.owner;
	_classId = desc.classId;
	_name = desc.name;

	_owner->game()->RegUser(this);
}

AchievmentCondition::~AchievmentCondition()
{
	_owner->game()->UnregUser(this);
}

void AchievmentCondition::RegProgressEvent()
{
	_owner->game()->RegProgressEvent(this);
}

void AchievmentCondition::UnregProgressEvent()
{
	_owner->game()->UnregProgressEvent(this);
}

void AchievmentCondition::SaveTo(lsl::SWriter* writer)
{
	writer->WriteValue("reward", _reward);
	writer->WriteValue("iterNum", _iterNum);
	writer->WriteValue("iterCount", _iterCount);
}

void AchievmentCondition::LoadFrom(lsl::SReader* reader)
{
	reader->ReadValue("reward", _reward);
	reader->ReadValue("iterNum", _iterNum);
	reader->ReadValue("iterCount", _iterCount);
}

void AchievmentCondition::CompleteIteration()
{
	if (_owner->game()->IsStartRace() && !_owner->game()->IsRaceFinish())
	{
		if (++_iterNum >= _iterCount)
		{
			Complete();
		}
	}
}

void AchievmentCondition::Complete()
{
	_iterNum = 0;
	_owner->AddPoints(_reward);

	_owner->game()->SendEvent(cAchievmentConditionComplete, &MyEventData(Race::cHuman, this));
}

void AchievmentCondition::ResetRaceState()
{
	OnResetRaceState();
}

AchievmentModel* AchievmentCondition::owner()
{
	return _owner;
}

unsigned AchievmentCondition::classId() const
{
	return _classId;
}

const std::string& AchievmentCondition::name() const
{
	return _name;
}

int AchievmentCondition::iterNum() const
{
	return _iterNum;
}

void AchievmentCondition::iterNum(int value)
{
	_iterNum = value;
}

int AchievmentCondition::iterCount() const
{
	return _iterCount;
}

void AchievmentCondition::iterCount(int value)
{
	_iterCount = value;
}

int AchievmentCondition::reward() const
{
	return _reward;
}

void AchievmentCondition::reward(int value)
{
	_reward = value;
}




AchievmentConditionBonus::AchievmentConditionBonus(const Desc& desc): AchievmentCondition(desc), _bonusCount(0), _bonusTotalCount(0)
{
}

void AchievmentConditionBonus::OnResetRaceState()
{
	_bonusCount = 0;
	_bonusTotalCount = 0;
}

void AchievmentConditionBonus::OnProcessEvent(unsigned id, EventData* data)
{
	Map* map = owner()->game()->GetWorld()->GetMap();
	
	switch (id)
	{
		case cPlayerPickItem:
		{
			Player::MyEventData* myData = static_cast<Player::MyEventData*>(data);
			if (myData->bonusType != _bonusType)
				break;

			if (_bonusTotalCount == 0)
				_bonusTotalCount = map->GetMapObjCount(myData->record);
			
			if (data && data->playerId == Race::cHuman)
			{				
				++_bonusCount;

				if (_bonusCount >= _bonusTotalCount)
				{
					CompleteIteration();
					_bonusCount = 0;
				}
			}			
			break;
		}
	}
}

void AchievmentConditionBonus::SaveTo(lsl::SWriter* writer)
{
	AchievmentCondition::SaveTo(writer);

	lsl::SWriteEnum(writer, "bonusType", _bonusType, GameObject::cBonusTypeStr, GameObject::cBonusTypeEnd);
}

void AchievmentConditionBonus::LoadFrom(lsl::SReader* reader)
{
	AchievmentCondition::LoadFrom(reader);

	lsl::SReadEnum(reader, "bonusType", _bonusType, GameObject::cBonusTypeStr, GameObject::cBonusTypeEnd);
}

GameObject::BonusType AchievmentConditionBonus::bonusType() const
{
	return _bonusType;
}

void AchievmentConditionBonus::bonusType(GameObject::BonusType value)
{
	_bonusType = value;
}




AchievmentConditionSpeedKill::AchievmentConditionSpeedKill(const Desc& desc): AchievmentCondition(desc), _time(0), _curKills(0), _killsNum(0), _killsTime(0)
{
	RegProgressEvent();
}

AchievmentConditionSpeedKill::~AchievmentConditionSpeedKill()
{
	UnregProgressEvent();
}

void AchievmentConditionSpeedKill::OnResetRaceState()
{
	_time = 0;
	_curKills = 0;
}

void AchievmentConditionSpeedKill::OnProcessEvent(unsigned id, EventData* data)
{
	if (!(data && data->playerId == Race::cHuman))
		return;

	switch (id)
	{
		case cPlayerKill:
		{
			if (++_curKills >= _killsNum)
			{
				_curKills = 0;
				_time = 0;
				CompleteIteration();
			}
			else
				_time = _killsTime;
			break;
		}
	}
}

void AchievmentConditionSpeedKill::OnProgress(float deltaTime)
{
	if (_time > 0 && (_time -= deltaTime) <= 0)
	{
		_time = 0.0f;
		_curKills = 0;
	}
}

void AchievmentConditionSpeedKill::SaveTo(lsl::SWriter* writer)
{
	AchievmentCondition::SaveTo(writer);

	writer->WriteValue("killsNum", _killsNum);
	writer->WriteValue("killsTime", _killsTime);
}

void AchievmentConditionSpeedKill::LoadFrom(lsl::SReader* reader)
{
	AchievmentCondition::LoadFrom(reader);

	reader->ReadValue("killsNum", _killsNum);
	reader->ReadValue("killsTime", _killsTime);
}

int AchievmentConditionSpeedKill::killsNum() const
{
	return _killsNum;
}

void AchievmentConditionSpeedKill::killsNum(int value)
{
	_killsNum = value;
}

float AchievmentConditionSpeedKill::killsTime() const
{
	return _killsTime;
}

void AchievmentConditionSpeedKill::killsTime(float value)
{
	_killsTime = value;
}
	



AchievmentConditionRaceKill::AchievmentConditionRaceKill(const Desc& desc): AchievmentCondition(desc), _curKills(0), _killsNum(0)
{
}

void AchievmentConditionRaceKill::OnResetRaceState()
{
	_curKills = 0;
}

void AchievmentConditionRaceKill::OnProcessEvent(unsigned id, EventData* data)
{
	if (!(data && data->playerId == Race::cHuman))
		return;

	switch (id)
	{
	case cPlayerKill:
		{
			if (++_curKills >= _killsNum)
			{
				_curKills = 0;				
				CompleteIteration();
			}
			break;
		}
	}
}	

void AchievmentConditionRaceKill::SaveTo(lsl::SWriter* writer)
{
	AchievmentCondition::SaveTo(writer);

	writer->WriteValue("killsNum", _killsNum);
}

void AchievmentConditionRaceKill::LoadFrom(lsl::SReader* reader)
{
	AchievmentCondition::LoadFrom(reader);

	reader->ReadValue("killsNum", _killsNum);
}

int AchievmentConditionRaceKill::killsNum() const
{
	return _killsNum;
}

void AchievmentConditionRaceKill::killsNum(int value)
{
	_killsNum = value;
}




AchievmentConditionLapPass::AchievmentConditionLapPass(const Desc& desc): AchievmentCondition(desc), _place(1), _lapCount(0)
{
}

void AchievmentConditionLapPass::OnResetRaceState()
{
	_lapCount = 0;
}

void AchievmentConditionLapPass::OnProcessEvent(unsigned id, EventData* data)
{
	if (!(data && data->playerId == Race::cHuman))
		return;

	switch (id)
	{
		case cRacePassLap:
		{
			if (owner()->player()->GetPlace() == 1)
				++_lapCount;
			break;
		}

		case cRaceFinish:
		{
			if (_lapCount >= owner()->race()->GetTournament().GetCurTrack().GetLapsCount())
				CompleteIteration();
			break;
		}
	}
}	

void AchievmentConditionLapPass::SaveTo(lsl::SWriter* writer)
{
	AchievmentCondition::SaveTo(writer);

	writer->WriteValue("place", _place);
}

void AchievmentConditionLapPass::LoadFrom(lsl::SReader* reader)
{
	AchievmentCondition::LoadFrom(reader);

	reader->ReadValue("place", _place);
}

int AchievmentConditionLapPass::place() const
{
	return _place;
}

void AchievmentConditionLapPass::place(int value)
{
	_place = value;
}




AchievmentConditionDodge::AchievmentConditionDodge(const Desc& desc): AchievmentCondition(desc), _damage(0)
{
}

void AchievmentConditionDodge::OnResetRaceState()
{
	_damage = 0;
}

void AchievmentConditionDodge::OnProcessEvent(unsigned id, EventData* data)
{
	if (!(data && data->playerId == Race::cHuman))
		return;

	switch (id)
	{
		case cPlayerDeath:
		{
			_damage += 1;
			break;
		}

		case cPlayerDamage:
		{
			GameObject::MyEventData* myData = static_cast<GameObject::MyEventData*>(data);
			if (myData->damage > 0.0f)
			{
				_damage += 1;
			}
			break;
		}

		case cRacePassLap:
		{
			if (_damage == 0 && owner()->player()->GetCar().numLaps == 1)
				CompleteIteration();
			_damage = 0;
			break;
		}
	}
}




AchievmentConditionLapBreak::AchievmentConditionLapBreak(const Desc& desc): AchievmentCondition(desc), _place(0)
{
}

void AchievmentConditionLapBreak::OnResetRaceState()
{
	_place = 0;
}

void AchievmentConditionLapBreak::OnProcessEvent(unsigned id, EventData* data)
{
	if (!(data && data->playerId == Race::cHuman))
		return;

	switch (id)
	{
		case cRacePassLap:
		{
			int newPlace = owner()->player()->GetPlace();
			unsigned numLaps = owner()->race()->GetTournament().GetCurTrack().GetLapsCount();

			if (owner()->player()->GetCar().numLaps >= numLaps && _place - newPlace >= (int)owner()->race()->GetPlayerList().size() - 1)
				CompleteIteration();

			_place = newPlace;
			break;
		}
	}
}




AchievmentConditionSurvival::AchievmentConditionSurvival(const Desc& desc): AchievmentCondition(desc), _curDeaths(0)
{
}

void AchievmentConditionSurvival::OnResetRaceState()
{
	_curDeaths = 0;
}

void AchievmentConditionSurvival::OnProcessEvent(unsigned id, EventData* data)
{
	if (!(data && data->playerId == Race::cHuman))
		return;

	switch (id)
	{
		case cRacePassLap:
		{
			if (owner()->player()->GetCar().numLaps == owner()->race()->GetTournament().GetCurTrack().GetLapsCount() - 1 && _curDeaths == 0)
				CompleteIteration();
			break;
		}

		case cPlayerDeath:
		{
			++_curDeaths;
			break;
		}
	}
}	




AchievmentConditionFirstKill::AchievmentConditionFirstKill(const Desc& desc): AchievmentCondition(desc), _curKills(0)
{
}

void AchievmentConditionFirstKill::OnResetRaceState()
{
	_curKills = 0;
}

void AchievmentConditionFirstKill::OnProcessEvent(unsigned id, EventData* data)
{
	switch (id)
	{
		case cPlayerKill:
		{
			if (data && data->playerId == Race::cHuman && _curKills == 0)
				CompleteIteration();
			++_curKills;
			break;
		}
	}
}




AchievmentConditionTouchKill::AchievmentConditionTouchKill(const Desc& desc): AchievmentCondition(desc)
{
}

void AchievmentConditionTouchKill::OnProcessEvent(unsigned id, EventData* data)
{
	switch (id)
	{
		case cPlayerDeath:
		{
			Player::MyEventData* myData = static_cast<Player::MyEventData*>(data);
			if ((myData->damageType == GameObject::dtTouch || myData->damageType == GameObject::dtDeathPlane) && myData->targetPlayerId == Race::cHuman && myData->playerId != Race::cHuman)
				CompleteIteration();
			break;
		}
	}
}




AchievmentModel::AchievmentModel(Race* race, const lsl::string& name): _race(race), _points(0)
{
	_classes.Add<Achievment>(cUndef);
	_classes.Add<AchievmentMapObj>(cMapObj);
	_classes.Add<AchievmentGamer>(cGamer);

	_condClasses.Add<AchievmentCondition>(cUndef);
	_condClasses.Add<AchievmentConditionBonus>(cBonus);
	_condClasses.Add<AchievmentConditionSpeedKill>(cSpeedKill);
	_condClasses.Add<AchievmentConditionRaceKill>(cRaceKill);
	_condClasses.Add<AchievmentConditionLapPass>(cLapPass);
	_condClasses.Add<AchievmentConditionDodge>(cDodge);	
	_condClasses.Add<AchievmentConditionLapBreak>(cLapBreak);
	_condClasses.Add<AchievmentConditionSurvival>(cLapSurvival);
	_condClasses.Add<AchievmentConditionFirstKill>(cFirstKill);
	_condClasses.Add<AchievmentConditionTouchKill>(cTouchKill);	
	
	SetName(name);
	SetOwner(race);

	LoadLib();
}

AchievmentModel::~AchievmentModel()
{
	DeleteAll();	
	DeleteAllCond();
}

void AchievmentModel::GenerateLib()
{	
	AchievmentGamer* gamer;
	AchievmentMapObj* mapObj;
	AchievmentConditionBonus* condBonus;
	AchievmentConditionSpeedKill* condSpeedKill;
	AchievmentConditionRaceKill* condRaceKill;
	AchievmentConditionLapPass* condLapPass;
	AchievmentConditionDodge* condDodge;
	AchievmentConditionLapBreak* condLapBreak;
	AchievmentConditionSurvival* condSurvival;
	AchievmentConditionFirstKill* condFirstKill;
	AchievmentConditionTouchKill* condTouchKill;

	_points = 0;

	gamer = Add<AchievmentGamer>(cViper);
	gamer->price(700);
	const Planet::PlayerData* playerData = _race->GetTournament().GetPlayerData(_SC(scViper));
	if (playerData)
		gamer->SetGamerId(playerData->id);
	gamer->Unlock();	

	mapObj = Add<AchievmentMapObj>(cBuggi);
	mapObj->price(1000);
	mapObj->SetRecord(_race->GetDB()->GetRecord(MapObjLib::ctCar, "buggi"));
	mapObj->Unlock();

	mapObj = Add<AchievmentMapObj>(cAirblade);
	mapObj->price(1000);	
	mapObj->AddRecord(_race->GetDB()->GetRecord(MapObjLib::ctCar, "manticoraBoss"));
	mapObj->AddRecord(_race->GetDB()->GetRecord(MapObjLib::ctCar, "gusenizaBoss"));
	mapObj->AddRecord(_race->GetDB()->GetRecord(MapObjLib::ctCar, "podushkaBoss"));
	mapObj->AddRecord(_race->GetDB()->GetRecord(MapObjLib::ctCar, "monstertruckBoss"));	
	mapObj->AddRecord(_race->GetDB()->GetRecord(MapObjLib::ctCar, "devildriverBoss"));
	mapObj->Unlock();

	mapObj = Add<AchievmentMapObj>(cReflector);
	mapObj->price(700);
	mapObj->SetRecord(&_race->GetWorkshop().GetRecord("reflector"));
	mapObj->Unlock();

	mapObj = Add<AchievmentMapObj>(cDroid);
	mapObj->price(1300);
	mapObj->SetRecord(&_race->GetWorkshop().GetRecord("droid"));
	mapObj->Unlock();

	mapObj = Add<AchievmentMapObj>(cArmor4);
	mapObj->price(1300);
	mapObj->Unlock();

	mapObj = Add<AchievmentMapObj>(cTankchetti);
	mapObj->price(1300);
	mapObj->SetRecord(_race->GetDB()->GetRecord(MapObjLib::ctCar, "tankchetti"));
	mapObj->Unlock();	

	mapObj = Add<AchievmentMapObj>(cPhaser);
	mapObj->price(1300);
	mapObj->SetRecord(&_race->GetWorkshop().GetRecord("rezonator"));
	mapObj->Unlock();

	mapObj = Add<AchievmentMapObj>(cMustang);
	mapObj->price(1300);
	mapObj->SetRecord(_race->GetDB()->GetRecord(MapObjLib::ctCar, "mustang"));
	mapObj->Unlock();	

	condBonus = AddCond<AchievmentConditionBonus>(cMedicate);
	condBonus->bonusType(GameObject::btMedpack);
	condBonus->reward(700);	

	condBonus = AddCond<AchievmentConditionBonus>(cMoneybags);
	condBonus->bonusType(GameObject::btMoney);
	condBonus->reward(700);	

	condBonus = AddCond<AchievmentConditionBonus>(cExplosivo);
	condBonus->bonusType(GameObject::btCharge);
	condBonus->reward(700);	

	condSpeedKill = AddCond<AchievmentConditionSpeedKill>(cDoubleKill);
	condSpeedKill->reward(500);
	condSpeedKill->killsNum(2);
	condSpeedKill->killsTime(4.0f);

	condSpeedKill = AddCond<AchievmentConditionSpeedKill>(cTripleKill);
	condSpeedKill->reward(1000);
	condSpeedKill->killsNum(3);
	condSpeedKill->killsTime(4.0f);

	condRaceKill = AddCond<AchievmentConditionRaceKill>(cDevastator);
	condRaceKill->reward(800);
	condRaceKill->killsNum(5);

	condLapPass = AddCond<AchievmentConditionLapPass>(cMegaRacer);
	condLapPass->reward(400);
	condLapPass->place(1);

	condDodge = AddCond<AchievmentConditionDodge>(cBulletProf);
	condDodge->reward(400);

	condLapBreak = AddCond<AchievmentConditionLapBreak>(cBreakRacer);
	condLapBreak->reward(400);

	condSurvival = AddCond<AchievmentConditionSurvival>(cSurvival);
	condSurvival->reward(400);

	condFirstKill = AddCond<AchievmentConditionFirstKill>(cFirstBlood);
	condFirstKill->reward(400);

	condTouchKill = AddCond<AchievmentConditionTouchKill>(cArmored);
	condTouchKill->reward(1000);
}

void AchievmentModel::LoadLib()
{
	DeleteAll();

	lsl::RootNode rootNode("achievmentRoot", _race);

	try
	{		
		SerialFileXML xml;		
		xml.LoadNodeFromFile(rootNode, "achievment.xml");

		rootNode.Load(this);
	}
	catch (const EUnableToOpen&)
	{
		GenerateLib();
		rootNode.Save(this);

		SerialFileXML xml;
		xml.SaveNodeToFile(rootNode, "achievment.xml");
	}
}

void AchievmentModel::Save(lsl::SWriter* writer)
{
	writer->WriteValue("points", _points);

	lsl::SWriter* items = writer->NewDummyNode("items");	
	for (Items::iterator iter = _items.begin(); iter != _items.end(); ++iter)
	{
		lsl::SWriter* child = items->NewDummyNode(iter->first.c_str());
		child->WriteAttr("classId", iter->second->classId());		

		iter->second->SaveTo(child);		
	}

	lsl::SWriter* conditions = writer->NewDummyNode("conditions");
	for (Conditions::iterator iter = _conditions.begin(); iter != _conditions.end(); ++iter)
	{
		lsl::SWriter* child = conditions->NewDummyNode(iter->first.c_str());
		child->WriteAttr("classId", iter->second->classId());

		iter->second->SaveTo(child);
	}
}

void AchievmentModel::Load(lsl::SReader* reader)
{
	DeleteAll();

	reader->ReadValue("points", _points);

	lsl::SReader* items = reader->ReadValue("items");
	if (items)
	{
		lsl::SReader* child = items->FirstChildValue();
		while (child)
		{	
			const lsl::SerialNode::ValueDesc* attr = child->ReadAttr("classId");
			LSL_ASSERT(attr);
			unsigned classId;
			attr->CastTo<unsigned>(&classId);

			Achievment* item = Add(classId, child->GetMyName());
			item->LoadFrom(child);

			child = child->NextValue();
		}
	}

	lsl::SReader* conditions = reader->ReadValue("conditions");
	if (conditions)
	{
		lsl::SReader* child = conditions->FirstChildValue();
		while (child)
		{	
			const lsl::SerialNode::ValueDesc* attr = child->ReadAttr("classId");
			LSL_ASSERT(attr);
			unsigned classId;
			attr->CastTo<unsigned>(&classId);

			AchievmentCondition* item = AddCond(classId, child->GetMyName());
			item->LoadFrom(child);

			child = child->NextValue();
		}
	}
}

void AchievmentModel::SaveLib()
{
	lsl::RootNode rootNode("achievmentRoot", _race);
	rootNode.Save(this);

	SerialFileXML xml;
	xml.SaveNodeToFile(rootNode, "achievment.xml");
}

void AchievmentModel::ResetRaceState()
{
	for (Conditions::const_iterator iter = _conditions.begin(); iter != _conditions.end(); ++iter)
		iter->second->ResetRaceState();
}

Achievment* AchievmentModel::Add(unsigned classId, const std::string& name)
{
	LSL_ASSERT(_items.find(name) == _items.end());

	Achievment::Desc desc;
	desc.owner = this;
	desc.classId = classId;
	desc.name = name;

	Achievment* item = _classes.CreateInst(classId, desc);
	_items[name] = item;

	return item;
}

void AchievmentModel::Delete(const std::string id)
{
	Items::const_iterator iter = _items.find(id);
	
	LSL_ASSERT(iter != _items.end());

	delete iter->second;
	_items.erase(iter);
}

void AchievmentModel::Delete(Achievment* item)
{
	Delete(item->name());
}

void AchievmentModel::DeleteAll()
{
	for (Items::const_iterator iter = _items.begin(); iter != _items.end(); ++iter)
		delete iter->second;
	_items.clear();
}

Achievment* AchievmentModel::Get(const std::string& name) const
{
	Items::const_iterator iter = _items.find(name);
	if (iter != _items.end())
		return iter->second;

	return NULL;
}

AchievmentCondition* AchievmentModel::AddCond(unsigned classId, const std::string& name)
{
	LSL_ASSERT(_items.find(name) == _items.end());

	AchievmentCondition::Desc desc;
	desc.owner = this;
	desc.classId = classId;
	desc.name = name;

	AchievmentCondition* item = _condClasses.CreateInst(classId, desc);
	_conditions[name] = item;

	return item;
}

void AchievmentModel::DeleteCond(const std::string id)
{
	Conditions::const_iterator iter = _conditions.find(id);

	LSL_ASSERT(iter != _conditions.end());

	delete iter->second;
	_conditions.erase(iter);
}

void AchievmentModel::DeleteCond(Achievment* item)
{
	Delete(item->name());
}

void AchievmentModel::DeleteAllCond()
{
	for (Conditions::const_iterator iter = _conditions.begin(); iter != _conditions.end(); ++iter)
		delete iter->second;
	_conditions.clear();
}

AchievmentCondition* AchievmentModel::GetCond(const std::string& name)
{
	Conditions::const_iterator iter = _conditions.find(name);
	if (iter != _conditions.end())
		return iter->second;

	return NULL;
}

void AchievmentModel::AddPoints(int value)
{
	if (_race->IsCampaign())
	{
		Difficulty diff = _race->GetProfile() ? _race->GetProfile()->difficulty() : gdEasy;

		switch (diff)
		{
		case gdHard:
			value = Floor<int>(value * 1.5f);
			break;
		case gdNormal:
			value = Floor<int>(value * 1.2f);
			break;
		}

		_points += value;

#ifdef STEAM_SERVICE
		if (_race->GetGame()->steamService()->isInit())
			_race->GetGame()->steamService()->steamStats()->AddStat(SteamStats::stPoints, value);
#endif
	}
}

bool AchievmentModel::ÑonsumePoints(int value)
{
	if (_points < value)
		return false;

	_points -= value;
	return true;
}

int AchievmentModel::points() const
{
	return _points;
}

bool AchievmentModel::CheckAchievment(const std::string& id) const
{
	Achievment* item = Get(id);
	if (item != NULL)
		return item->state() == Achievment::asOpened;

	return true;
}

bool AchievmentModel::CheckMapObj(Record* record) const
{
	for (Items::const_iterator iter = _items.begin(); iter != _items.end(); ++iter)
	{
		Achievment* item = iter->second;
		if (item->state() != Achievment::asOpened && item->classId() == cMapObj && static_cast<AchievmentMapObj*>(item)->ContainsRecord(record))
			return false;
	}

	return true;
}

bool AchievmentModel::CheckGamerId(int gamerId) const
{
	for (Items::const_iterator iter = _items.begin(); iter != _items.end(); ++iter)
	{
		Achievment* item = iter->second;
		if (item->state() != Achievment::asOpened && item->classId() == cGamer && static_cast<AchievmentGamer*>(item)->GetGamerId() == gamerId)
			return false;
	}

	return true;
}

Race* AchievmentModel::race()
{
	return _race;
}

GameMode* AchievmentModel::game()
{
	return _race->GetGame();
}

Player* AchievmentModel::player()
{
	return _race->GetHuman()->GetPlayer();
}

AchievmentModel::Classes& AchievmentModel::classes()
{
	return _classes;
}

AchievmentModel::CondClasses& AchievmentModel::condClasses()
{
	return _condClasses;
}

}

}