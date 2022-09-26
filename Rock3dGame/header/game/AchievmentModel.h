#pragma once

#include "Player.h"

namespace r3d
{
	namespace game
	{
		extern bool TEST_BUILD;
		extern bool EDIT_MODE;
		extern int CAM_FOV;;
		extern unsigned int HUD_STYLE;
		extern unsigned int MM_STYLE;
		extern float ISOCAM_DIST;
		extern unsigned int lapsRest;
		extern unsigned int lapRestFix;
		extern float X_VPSIZE;
		extern float Y_VPSIZE;
		extern float X_VPSIZE_MMENU;
		extern float Y_VPSIZE_MMENU;
		extern float MMAP_OFFSET_X;
		extern float MMAP_OFFSET_Y;
		extern float FIX_OFFSET;
		extern float LOCAL_XPOS_HYPERITEM;
		extern float LOCAL_YPOS_HYPERITEM;
		extern float LOCAL_XPOS_MINEITEM;
		extern float LOCAL_YPOS_MINEITEM;
		extern float LOCAL_XPOS_WEAPONITEM;
		extern float LOCAL_YPOS_WEAPONITEM;
		extern float LOCAL_XPOS_WPN_BOX_ITEM;
		extern float LOCAL_YPOS_WPN_BOX_ITEM;
		extern float GUN2_OFFSET;
		extern float GUN3_OFFSET;
		extern float GUN4_OFFSET;
		extern float LL_OFFSET;
		extern int WEAPON_INDEXX;
		extern unsigned int L_COLORINDEX;
		extern unsigned int R_COLORINDEX;
		extern bool CLR_PAGE_CHANGED;
		extern bool RIGHT_CLR_PAGE_CHANGED;
		extern bool GAME_PAUSED;
		extern float CRATER_POSX;
		extern float CRATER_POSY;
		extern float CRATER_POSZ;
		extern bool CRATER_SPAWN;
		extern bool DOUBLE_JUMP;
		//Ì‡˜‡ÎÓ gamerID Ì‡·Î˛‰‡ÚÂÎÂÈ
		extern int SPECTATOR_ID_BEGIN;
		extern unsigned int SPECTATORS_COUNT;
		extern unsigned int TOTALPLAYERS_COUNT;
		extern bool DIVISION_END;
		extern bool PLANET_END;
		extern bool DLG_ONSHOW;
		extern int DLG_ITTER;

		class AchievmentModel;
		class GameMode;

		class Achievment : public Object, protected IProgressEvent
		{
		public:
			struct Desc
			{
				AchievmentModel* owner;
				unsigned classId;
				std::string name;
			};

			enum State { asLocked, asUnlocked, asOpened, cStateEnd };

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

			virtual void OnStateChanged()
			{
			}

			void OnProgress(float deltaTime) override
			{
			}

		public:
			Achievment(const Desc& desc);
			~Achievment() override;

			virtual void SaveTo(SWriter* writer);
			virtual void LoadFrom(SReader* reader);

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

		class AchievmentMapObj : public Achievment
		{
		public:
			using Records = RecordList<Record>;
		private:
			Records _records;
		public:
			AchievmentMapObj(const Desc& desc);
			~AchievmentMapObj() override;

			void SaveTo(SWriter* writer) override;
			void LoadFrom(SReader* reader) override;

			Record* GetRecord();
			void SetRecord(Record* value);
			void AddRecord(Record* value);
			bool ContainsRecord(Record* value) const;
			const Records& GetRecords() const;
		};

		class AchievmentGamer : public Achievment
		{
		private:
			int _gamerId;
		public:
			AchievmentGamer(const Desc& desc);

			void SaveTo(SWriter* writer) override;
			void LoadFrom(SReader* reader) override;

			int GetGamerId();
			void SetGamerId(int value);
		};

		class AchievmentCondition : protected IGameUser, protected IProgressEvent
		{
		public:
			struct Desc
			{
				AchievmentModel* owner;
				unsigned classId;
				std::string name;
			};

			struct MyEventData : EventData
			{
				AchievmentCondition* condition;

				MyEventData(int player, AchievmentCondition* mCondition): EventData(player), condition(mCondition)
				{
				}
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

			virtual void OnResetRaceState()
			{
			}

			void OnProgress(float deltaTime) override
			{
			}

		public:
			AchievmentCondition(const Desc& desc);
			~AchievmentCondition() override;

			virtual void SaveTo(SWriter* writer);
			virtual void LoadFrom(SReader* reader);

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

		class AchievmentConditionBonus : public AchievmentCondition
		{
		private:
			GameObject::BonusType _bonusType;
			int _bonusCount;
			int _bonusTotalCount;
		protected:
			void OnResetRaceState() override;
			void OnProcessEvent(unsigned id, EventData* data) override;
		public:
			AchievmentConditionBonus(const Desc& desc);

			void SaveTo(SWriter* writer) override;
			void LoadFrom(SReader* reader) override;

			GameObject::BonusType bonusType() const;
			void bonusType(GameObject::BonusType value);
		};

		class AchievmentConditionSpeedKill : public AchievmentCondition
		{
		private:
			int _killsNum;
			float _killsTime;

			float _time;
			int _curKills;
		protected:
			void OnResetRaceState() override;
			void OnProcessEvent(unsigned id, EventData* data) override;
			void OnProgress(float deltaTime) override;
		public:
			AchievmentConditionSpeedKill(const Desc& desc);
			~AchievmentConditionSpeedKill() override;

			void SaveTo(SWriter* writer) override;
			void LoadFrom(SReader* reader) override;

			int killsNum() const;
			void killsNum(int value);

			float killsTime() const;
			void killsTime(float value);
		};

		class AchievmentConditionBombKill : public AchievmentCondition
		{
		private:
			int _killzNum;
			float _killzTime;

			float _timez;
			int _curKillz;
		protected:
			void OnResetRaceState() override;
			void OnProcessEvent(unsigned id, EventData* data) override;
			void OnProgress(float deltaTime) override;
		public:
			AchievmentConditionBombKill(const Desc& desc);
			~AchievmentConditionBombKill() override;

			void SaveTo(SWriter* writer) override;
			void LoadFrom(SReader* reader) override;

			int killzNum() const;
			void killzNum(int value);

			float killzTime() const;
			void killzTime(float value);
		};

		class AchievmentConditionRaceKill : public AchievmentCondition
		{
		private:
			int _killsNum;
			int _curKills;
			Race* _race;
		protected:
			void OnResetRaceState() override;
			void OnProcessEvent(unsigned id, EventData* data) override;
		public:
			AchievmentConditionRaceKill(const Desc& desc);

			void SaveTo(SWriter* writer) override;
			void LoadFrom(SReader* reader) override;

			int killsNum() const;
			void killsNum(int value);
		};

		class AchievmentConditionLapPass : public AchievmentCondition
		{
		private:
			int _place;
			unsigned _lapCount;
		protected:
			void OnResetRaceState() override;
			void OnProcessEvent(unsigned id, EventData* data) override;
		public:
			AchievmentConditionLapPass(const Desc& desc);

			void SaveTo(SWriter* writer) override;
			void LoadFrom(SReader* reader) override;

			int place() const;
			void place(int value);
		};

		class AchievmentConditionDodge : public AchievmentCondition
		{
		private:
			int _damage;
		protected:
			void OnResetRaceState() override;
			void OnProcessEvent(unsigned id, EventData* data) override;
		public:
			AchievmentConditionDodge(const Desc& desc);
		};

		class AchievmentConditionLapBreak : public AchievmentCondition
		{
		private:
			int _place;
		protected:
			void OnResetRaceState() override;
			void OnProcessEvent(unsigned id, EventData* data) override;
		public:
			AchievmentConditionLapBreak(const Desc& desc);
		};

		class AchievmentConditionSurvival : public AchievmentCondition
		{
		private:
			int _curDeaths;
		protected:
			void OnResetRaceState() override;
			void OnProcessEvent(unsigned id, EventData* data) override;
		public:
			AchievmentConditionSurvival(const Desc& desc);
		};

		class AchievmentConditionFinalLap : public AchievmentCondition
		{
		private:
			int _curDeaths;
		protected:
			void OnResetRaceState() override;
			void OnProcessEvent(unsigned id, EventData* data) override;
		public:
			AchievmentConditionFinalLap(const Desc& desc);
		};

		class AchievmentConditionFirstKill : public AchievmentCondition
		{
		private:
			int _curKills;
		protected:
			void OnResetRaceState() override;
			void OnProcessEvent(unsigned id, EventData* data) override;
		public:
			AchievmentConditionFirstKill(const Desc& desc);
		};

		class AchievmentConditionTouchKill : public AchievmentCondition
		{
		protected:
			void OnProcessEvent(unsigned id, EventData* data) override;
		public:
			AchievmentConditionTouchKill(const Desc& desc);
		};

		class AchievmentModel : public Component
		{
		public:
			using Classes = ClassList<unsigned, Achievment, Achievment::Desc>;
			using CondClasses = ClassList<unsigned, AchievmentCondition, AchievmentCondition::Desc>;
			using Items = std::map<std::string, Achievment*>;
			using Conditions = std::map<std::string, AchievmentCondition*>;

			static const std::string cViper;
			static const std::string cBuggi;
			static const std::string cAirblade;
			static const std::string cReflector;
			static const std::string cDroid;
			static const std::string cTankchetti;
			static const std::string cPhaser;
			static const std::string cMustang;
			static const std::string cArmor4;

			//static const std::string cSingleKill;
			static const std::string cDoubleKill;
			static const std::string cTripleKill;
			static const std::string cMegaKill;
			static const std::string cMonsterKill;
			static const std::string cDevastator;
			static const std::string cMegaRacer;
			static const std::string cBreakRacer;
			static const std::string cMoneybags;
			static const std::string cSurvival;
			static const std::string cLastLap;
			static const std::string cLastLoop;
			static const std::string cFirstBlood;
			static const std::string cArmored;
			static const std::string cBomber;

			static const unsigned cUndef = 0;
			//
			static const unsigned cMapObj = 1;
			static const unsigned cGamer = 2;
			//
			static const unsigned cBonus = 1;
			static const unsigned cSpeedKill = 2;
			static const unsigned cRaceKill = 3;
			static const unsigned cLapPass = 4;
			static const unsigned cLapBreak = 5;
			static const unsigned cLapSurvival = 6;
			static const unsigned cFirstKill = 7;
			static const unsigned cTouchKill = 8;
			static const unsigned cBombKill = 9;
			static const unsigned cFinalLap = 10;
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
			void Save(SWriter* writer) override;
			void Load(SReader* reader) override;
		public:
			AchievmentModel(Race* race, const string& name);
			~AchievmentModel() override;

			void SaveLib();
			void ResetRaceState();

			Achievment* Add(unsigned classId, const std::string& name);
			template <class _Type>
			_Type* Add(const std::string& name);
			void Delete(std::string id);
			void Delete(Achievment* item);
			void DeleteAll();
			Achievment* Get(const std::string& name) const;

			AchievmentCondition* AddCond(unsigned classId, const std::string& name);
			template <class _Type>
			_Type* AddCond(const std::string& name);
			void DeleteCond(std::string id);
			void DeleteCond(Achievment* item);
			void DeleteAllCond();
			AchievmentCondition* GetCond(const std::string& name);

			void AddPoints(int value);
			bool —onsumePoints(int value);
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


		template <class _Type>
		_Type* AchievmentModel::Add(const std::string& name)
		{
			return lsl::StaticCast<_Type*>(Add(_classes.GetByClass<_Type>().GetKey(), name));
		}

		template <class _Type>
		_Type* AchievmentModel::AddCond(const std::string& name)
		{
			return lsl::StaticCast<_Type*>(AddCond(_condClasses.GetByClass<_Type>().GetKey(), name));
		}
	}
}
