#pragma once

#include "Player.h"

namespace r3d
{

namespace game
{

class AchievmentModel;
class GameMode;

class Achievment: public lsl::Object, protected IProgressEvent
{
public:
	struct Desc
	{
		AchievmentModel* owner;
		unsigned classId;
		std::string name;
	};

	enum State {asLocked, asUnlocked, asOpened, cStateEnd};
	static const std::string cStateStr[cStateEnd];
private:
	AchievmentModel* _owner;
	unsigned _classId;	
	State _state;
	int _price;
	std::string _name;

	void ChangeState(State state);
protected:
	void RegProgressEvent();
	void UnregProgressEvent();

	virtual void OnStateChanged() {}
	virtual void OnProgress(float deltaTime) {}
public:
	Achievment(const Desc& desc);
	virtual ~Achievment();

	virtual void SaveTo(lsl::SWriter* writer);
	virtual void LoadFrom(lsl::SReader* reader);

	void Unlock();
	void Open();
	bool Buy(Player* player);
	State state() const;

	int price() const;
	void price(int value);

	//unlock condition
	//...

	AchievmentModel* owner() const;
	unsigned classId() const;
	const std::string& name() const;

	Race* race() const;
};

class AchievmentMapObj: public Achievment
{
public:
	typedef RecordList<Record> Records;
private:
	Records _records;
public:
	AchievmentMapObj(const Desc& desc);
	virtual ~AchievmentMapObj();

	virtual void SaveTo(lsl::SWriter* writer);
	virtual void LoadFrom(lsl::SReader* reader);

	Record* GetRecord();
	void SetRecord(Record* value);
	void AddRecord(Record* value);
	bool ContainsRecord(Record* value) const;
	const Records& GetRecords() const;
};

class AchievmentGamer: public Achievment
{
private:
	int _gamerId;
public:
	AchievmentGamer(const Desc& desc);

	virtual void SaveTo(lsl::SWriter* writer);
	virtual void LoadFrom(lsl::SReader* reader);

	int GetGamerId();
	void SetGamerId(int value);
};

class AchievmentCondition: protected IGameUser, protected IProgressEvent
{
public:
	struct Desc
	{
		AchievmentModel* owner;
		unsigned classId;
		std::string name;
	};

	struct MyEventData: EventData
	{
		AchievmentCondition* condition;

		MyEventData(int player, AchievmentCondition* mCondition): EventData(player), condition(mCondition) {}
	};
private:
	AchievmentModel* _owner;

	std::string _name;
	unsigned _classId;
	int _reward;
	int _iterNum;
	int _iterCount;
protected:
	void RegProgressEvent();
	void UnregProgressEvent();

	virtual void OnResetRaceState() {}
	virtual void OnProgress(float deltaTime) {}
public:
	AchievmentCondition(const Desc& desc);
	virtual ~AchievmentCondition();

	virtual void SaveTo(lsl::SWriter* writer);
	virtual void LoadFrom(lsl::SReader* reader);	

	void CompleteIteration();
	void Complete();
	void ResetRaceState();

	AchievmentModel* owner();
	unsigned classId() const;
	const std::string& name() const;

	int iterNum() const;
	void iterNum(int value);

	int iterCount() const;
	void iterCount(int value);

	int reward() const;	
	void reward(int value);
};

class AchievmentConditionBonus: public AchievmentCondition
{
private:
	GameObject::BonusType _bonusType;
	int _bonusCount;
	int _bonusTotalCount;
protected:	
	virtual void OnResetRaceState();
	virtual void OnProcessEvent(unsigned id, EventData* data);
public:
	AchievmentConditionBonus(const Desc& desc);

	virtual void SaveTo(lsl::SWriter* writer);
	virtual void LoadFrom(lsl::SReader* reader);

	GameObject::BonusType bonusType() const;
	void bonusType(GameObject::BonusType value);
};

class AchievmentConditionSpeedKill: public AchievmentCondition
{
private:	
	int _killsNum;
	float _killsTime;

	float _time;
	int _curKills;
protected:	
	virtual void OnResetRaceState();
	virtual void OnProcessEvent(unsigned id, EventData* data);
	virtual void OnProgress(float deltaTime);
public:
	AchievmentConditionSpeedKill(const Desc& desc);
	virtual ~AchievmentConditionSpeedKill();

	virtual void SaveTo(lsl::SWriter* writer);
	virtual void LoadFrom(lsl::SReader* reader);	

	int killsNum() const;
	void killsNum(int value);

	float killsTime() const;
	void killsTime(float value);
};

class AchievmentConditionRaceKill: public AchievmentCondition
{
private:	
	int _killsNum;
	int _curKills;
protected:	
	virtual void OnResetRaceState();
	virtual void OnProcessEvent(unsigned id, EventData* data);
public:
	AchievmentConditionRaceKill(const Desc& desc);

	virtual void SaveTo(lsl::SWriter* writer);
	virtual void LoadFrom(lsl::SReader* reader);	

	int killsNum() const;
	void killsNum(int value);
};

class AchievmentConditionLapPass: public AchievmentCondition
{
private:		
	int _place;
	unsigned _lapCount;
protected:	
	virtual void OnResetRaceState();
	virtual void OnProcessEvent(unsigned id, EventData* data);
public:
	AchievmentConditionLapPass(const Desc& desc);

	virtual void SaveTo(lsl::SWriter* writer);
	virtual void LoadFrom(lsl::SReader* reader);	

	int place() const;
	void place(int value);
};

class AchievmentConditionDodge: public AchievmentCondition
{
private:		
	int _damage;
protected:	
	virtual void OnResetRaceState();
	virtual void OnProcessEvent(unsigned id, EventData* data);
public:
	AchievmentConditionDodge(const Desc& desc);
};

class AchievmentConditionLapBreak: public AchievmentCondition
{
private:		
	int _place;
protected:	
	virtual void OnResetRaceState();
	virtual void OnProcessEvent(unsigned id, EventData* data);
public:
	AchievmentConditionLapBreak(const Desc& desc);
};

class AchievmentConditionSurvival: public AchievmentCondition
{
private:		
	int _curDeaths;
protected:	
	virtual void OnResetRaceState();
	virtual void OnProcessEvent(unsigned id, EventData* data);
public:
	AchievmentConditionSurvival(const Desc& desc);
};

class AchievmentConditionFirstKill: public AchievmentCondition
{
private:		
	int _curKills;
protected:	
	virtual void OnResetRaceState();
	virtual void OnProcessEvent(unsigned id, EventData* data);
public:
	AchievmentConditionFirstKill(const Desc& desc);
};

class AchievmentConditionTouchKill: public AchievmentCondition
{
protected:
	virtual void OnProcessEvent(unsigned id, EventData* data);
public:
	AchievmentConditionTouchKill(const Desc& desc);
};

class AchievmentModel: public lsl::Component
{
public:
	typedef lsl::ClassList<unsigned, Achievment, Achievment::Desc> Classes;
	typedef lsl::ClassList<unsigned, AchievmentCondition, AchievmentCondition::Desc> CondClasses;
	typedef std::map<std::string, Achievment*> Items;
	typedef std::map<std::string, AchievmentCondition*> Conditions;

	static const std::string cViper;
	static const std::string cBuggi;
	static const std::string cAirblade;
	static const std::string cReflector;
	static const std::string cDroid;
	static const std::string cTankchetti;
	static const std::string cPhaser;
	static const std::string cMustang;
	static const std::string cArmor4;

	static const std::string cDoubleKill;
	static const std::string cTripleKill;
	static const std::string cDevastator;
	static const std::string cMegaRacer;	
	static const std::string cBulletProf;
	static const std::string cBreakRacer;
	static const std::string cMedicate;
	static const std::string cExplosivo;
	static const std::string cMoneybags;
	static const std::string cSurvival;
	static const std::string cFirstBlood;
	static const std::string cArmored;

	static const unsigned cUndef = 0;
	//
	static const unsigned cMapObj = 1;
	static const unsigned cGamer = 2;
	//
	static const unsigned cBonus = 1;
	static const unsigned cSpeedKill = 2;
	static const unsigned cRaceKill = 3;
	static const unsigned cLapPass = 4;
	static const unsigned cDodge = 5;
	static const unsigned cLapBreak = 6;
	static const unsigned cLapSurvival = 7;
	static const unsigned cFirstKill = 8;
	static const unsigned cTouchKill = 9;
private:
	Race* _race;
	Classes _classes;
	CondClasses _condClasses;
	Items _items;
	Conditions _conditions;
	int _points;

	void GenerateLib();
	void LoadLib();	
protected:
	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
public:
	AchievmentModel(Race* race, const lsl::string& name);
	~AchievmentModel();

	void SaveLib();
	void ResetRaceState();	

	Achievment* Add(unsigned classId, const std::string& name);
	template<class _Type> _Type* Add(const std::string& name);
	void Delete(const std::string id);
	void Delete(Achievment* item);
	void DeleteAll();	
	Achievment* Get(const std::string& name) const;

	AchievmentCondition* AddCond(unsigned classId, const std::string& name);
	template<class _Type> _Type* AddCond(const std::string& name);
	void DeleteCond(const std::string id);
	void DeleteCond(Achievment* item);
	void DeleteAllCond();	
	AchievmentCondition* GetCond(const std::string& name);

	void AddPoints(int value);
	bool ÑonsumePoints(int value);
	int points() const;

	bool CheckAchievment(const std::string& id) const;
	bool CheckMapObj(Record* record) const;
	bool CheckGamerId(int gamerId) const;

	Race* race();
	GameMode* game();
	Player* player();

	Classes& classes();
	CondClasses& condClasses();
};




template<class _Type> _Type* AchievmentModel::Add(const std::string& name)
{
	return lsl::StaticCast<_Type*>(Add(_classes.GetByClass<_Type>().GetKey(), name));
}

template<class _Type> _Type* AchievmentModel::AddCond(const std::string& name)
{
	return lsl::StaticCast<_Type*>(AddCond(_condClasses.GetByClass<_Type>().GetKey(), name));
}

}

}