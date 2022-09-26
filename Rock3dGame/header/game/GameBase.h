#pragma once

#include "MapObj.h"
#include "GameEvent.h"

#include "graph//Actor.h"
#include "px//PhysX.h"
#include "snd/Audio.h"

namespace r3d
{
	namespace game
	{
		class Logic;
		class Behavior;

		enum ScActorType { satBaseObj, satMeshObj, cScActorTypeEnd };

		enum Difficulty { gdEasy, gdNormal, gdHard, gdMaster, cDifficultyEnd };

		extern const string cDifficultyStr[cDifficultyEnd];
		extern const char* cScActorTypeStr[cScActorTypeEnd];

		class GameObject;

		class GameObjListener : public virtual ObjReference
		{
		public:
			enum DamageType
			{
				dtSimple,
				dtEnergy,
				dtPowerShell,
				dtBolt,
				dtLaser,
				dtResp,
				dtMine,
				dtTouch,
				dtAim,
				dtCash,
				dtNull,
				dtNone,
				dtRage,
				dtMedpack,
				dtMoney,
				dtReload,
				dtShieldOn,
				dtDeathPlane
			};

			enum BonusType
			{
				btMoney,
				btCharge,
				btMedpack,
				btImmortal,
				btRage,
				btWrong,
				btLastLap,
				btLucky,
				btUnLucky,
				btEmpty,

				cBonusTypeEnd
			};

			static const std::string cBonusTypeStr[cBonusTypeEnd];
		public:
			//sender - отправитель сообщени€, может быть сам this
			//”даление объекта из пам€ти
			virtual void OnDestroy(GameObject* sender)
			{
			}

			//—мерть от нанесенного повреждени€
			virtual void OnDeath(GameObject* sender, DamageType damageType, GameObject* target)
			{
			}

			//Ќанесено повреждение
			virtual void OnDamage(GameObject* sender, float value, DamageType damageType)
			{
			}

			//ћало жизней, посылаетс€ Behavior
			virtual void OnLowLife(GameObject* sender, Behavior* behavior)
			{
			}

			//
			virtual void OnContact(const px::Scene::OnContactEvent& contact)
			{
			}
		};

		class Behavior : public Object, public GameObjListener, public Serializable
		{
			friend class Behaviors;
		protected:
			enum PxNotify { pxContact = 0, pxContactModify, cPxNotifyEnd };

			using PxNotifies = Bitset<cPxNotifyEnd>;
		private:
			Behaviors* _owner;
			PxNotifies _pxNotifies;
			bool _removed;
		protected:
			virtual void OnShot(const D3DXVECTOR3& pos)
			{
			}

			virtual void OnMotor(float deltaTime, float rpm, float minRPM, float maxRPM)
			{
			}

			virtual void OnImmortalStatus(bool status)
			{
			}

			virtual void OnShellStatus(bool status)
			{
			}

			virtual void SaveSource(SWriter* writer)
			{
			}

			virtual void LoadSource(SReader* reader)
			{
			}

			virtual void SaveProxy(SWriter* writer)
			{
			}

			virtual void LoadProxy(SReader* reader)
			{
			}

			void Save(SWriter* writer) override;
			void Load(SReader* reader) override;

			bool GetPxNotify(PxNotify notify) const;
			void SetPxNotify(PxNotify notify, bool value);
		public:
			Behavior(Behaviors* owner);

			virtual void OnProgress(float deltaTime) = 0;

			void Remove();
			bool IsRemoved() const;

			Behaviors* GetOwner() const;
			GameObject* GetGameObj() const;
			Logic* GetLogic() const;
		};

		class TouchDeath : public Behavior
		{
			using _MyBase = Behavior;
		public:
			TouchDeath(Behaviors* owner);

			void OnContact(const px::Scene::OnContactEvent& contact) override;

			void OnProgress(float deltaTime) override
			{
			}
		};

		class ResurrectObj : public Behavior
		{
			using _MyBase = Behavior;
		private:
			bool _resurrect;
		protected:
			virtual void Resurrect();
			void OnDeath(GameObject* sender, DamageType damageType, GameObject* target) override;

			bool IsResurrect() const;
		public:
			ResurrectObj(Behaviors* owner);

			void OnProgress(float deltaTime) override
			{
			}
		};

		class FxSystemWaitingEnd : public ResurrectObj
		{
			using _MyBase = ResurrectObj;
		protected:
			void Resurrect() override;
		public:
			FxSystemWaitingEnd(Behaviors* owner);

			void OnProgress(float deltaTime) override;
		};

		class FxSystemSrcSpeed : public Behavior
		{
			using _MyBase = Behavior;
		public:
			FxSystemSrcSpeed(Behaviors* owner);

			void OnProgress(float deltaTime) override;
		};

		class EventEffect : public Behavior
		{
			using _MyBase = Behavior;
		private:
			class GameObjEvent : public GameObjListener
			{
			private:
				EventEffect* _effect;
			public:
				GameObjEvent(EventEffect* effect);

				void OnDestroy(GameObject* sender) override;
			};

			using EffObjList = List<MapObj*>;
		protected:
			struct EffectDesc
			{
				EffectDesc(): pos(NullVector), rot(NullQuaternion), child(true), parent(nullptr)
				{
				}

				D3DXVECTOR3 pos;
				D3DXQUATERNION rot;
				//дочерний
				//true - врем€ жизни совпадает с врменем жизни EventEffect, локальна€ система координат
				//false - за удаление отвечает Logic, мирова€ система координат
				bool child;
				//
				GameObject* parent;
			};

		public:
			struct EffectSound
			{
				snd::Sound* sound;
				snd::Source* source;
				snd::Source3d* source3d;

				EffectSound(): sound(nullptr), source(nullptr), source3d(nullptr)
				{
				}
			};

			using SoundList = Vector<EffectSound>;
		private:
			GameObjEvent* _gameObjEvent;
			MapObjRec* _effect;
			SoundList _sounds;

			D3DXVECTOR3 _pos;
			D3DXVECTOR3 _impulse;
			bool _ignoreRot;

			EffObjList _effObjList;
			MapObj* _makeEffect;

			void InsertEffObj(MapObj* mapObj);
			void RemoveEffObj(MapObj* mapObj);
			void ClearEffObjList();

			void DestroyEffObj(MapObj* mapObj, bool destrWorld = false) const;

			void InitSource();
			void FreeSource();
			void InitSource3d();
			void FreeSource3d();
		protected:
			virtual MapObj* CreateEffect(const EffectDesc& desc);
			MapObj* CreateEffect();
			virtual void DeleteEffect(MapObj* mapObj);
			void DeleteAllEffects();

			void MakeEffect(const EffectDesc& desc);
			void MakeEffect();
			void FreeEffect(bool death = false);
			MapObj* GetMakeEffect() const;
			bool IsEffectMaked() const;

			//—пециально дл€ effect-ов
			virtual void OnDestroyEffect(MapObj* sender)
			{
			}

			void SaveSource(SWriter* writer) override;
			void LoadSource(SReader* reader) override;
			void OnFixUp(const FixUpNames& fixUpNames) override;

			snd::Source* GiveSource();
			snd::Source3d* GiveSource3d();
		public:
			EventEffect(Behaviors* owner);
			~EventEffect() override;

			void OnProgress(float deltaTime) override;

			MapObjRec* GetEffect() const;
			void SetEffect(MapObjRec* value);

			void AddSound(snd::Sound* sound);
			void ClearSounds();
			const SoundList& GetSounds();

			snd::Sound* GetSound() const;
			void SetSound(snd::Sound* value);

			const D3DXVECTOR3& GetPos() const;
			void SetPos(const D3DXVECTOR3& value);

			//local coordinates
			const D3DXVECTOR3& GetImpulse() const;
			void SetImpulse(const D3DXVECTOR3& value);

			//игнорировать родительский поворот
			bool GetIgnoreRot() const;
			void SetIgnoreRot(bool value);
		};

		class LowLifePoints : public EventEffect
		{
			using _MyBase = EventEffect;
		private:
			float _lifeLevel;
		protected:
			void SaveSource(SWriter* writer) override;
			void LoadSource(SReader* reader) override;
		public:
			LowLifePoints(Behaviors* owner);

			void OnProgress(float deltaTime) override;

			//уровень жизней, [0..1]
			float GetLifeLevel() const;
			void SetLifeLevel(float value);
		};

		///////
		class BurnEffect : public EventEffect
		{
			using _MyBase = EventEffect;
		public:
			BurnEffect(Behaviors* owner);

			void OnProgress(float deltaTime) override;
		};

		///////
		class DamageEffect : public EventEffect
		{
			using _MyBase = EventEffect;
		private:
			DamageType _damageType;
		protected:
			void OnDamage(GameObject* sender, float value, DamageType damageType) override;

			void SaveSource(SWriter* writer) override;
			void LoadSource(SReader* reader) override;
		public:
			DamageEffect(Behaviors* owner);

			void OnProgress(float deltaTime) override;

			DamageType GetDamageType() const;
			void SetDamageType(DamageType value);
		};

		class DeathEffect : public EventEffect
		{
			using _MyBase = EventEffect;
		private:
			bool _effectPxIgnoreSenderCar;
			bool _targetChild;
		protected:
			void OnDeath(GameObject* sender, DamageType damageType, GameObject* target) override;
			void SaveSource(SWriter* writer) override;
			void LoadSource(SReader* reader) override;
		public:
			DeathEffect(Behaviors* owner);

			void OnProgress(float deltaTime) override;

			bool GetEffectPxIgnoreSenderCar() const;
			void SetEffectPxIgnoreSenderCar(bool value);

			bool GetTargetChild() const;
			void SetTargetChild(bool value);
		};

		class LifeEffect : public EventEffect
		{
			using _MyBase = EventEffect;
		private:
			bool _play;
		public:
			LifeEffect(Behaviors* owner);

			void OnProgress(float deltaTime) override;
		};

		class PxWheelSlipEffect : public EventEffect
		{
			using _MyBase = EventEffect;
		public:
			PxWheelSlipEffect(Behaviors* owner);
			~PxWheelSlipEffect() override;

			void OnProgress(float deltaTime) override;
		};

		class PxDriftEffect : public EventEffect
		{
			using _MyBase = EventEffect;
		public:
			PxDriftEffect(Behaviors* owner);
			~PxDriftEffect() override;

			void OnProgress(float deltaTime) override;
		};

		class ShotEffect : public EventEffect
		{
			using _MyBase = EventEffect;
		protected:
			void OnShot(const D3DXVECTOR3& pos) override;
		public:
			ShotEffect(Behaviors* owner);
		};

		class ImmortalEffect : public EventEffect
		{
			using _MyBase = EventEffect;
		private:
			float _fadeInTime;
			float _fadeOutTime;
			float _dmgTime;
			D3DXVECTOR3 _scale;
			D3DXVECTOR3 _scaleK;
		protected:
			void OnImmortalStatus(bool status) override;
			void OnDamage(GameObject* sender, float value, DamageType damageType) override;

			void SaveSource(SWriter* writer) override;
			void LoadSource(SReader* reader) override;
		public:
			ImmortalEffect(Behaviors* owner);

			void OnProgress(float deltaTime) override;

			const D3DXVECTOR3& GetScaleK() const;
			void SetScaleK(const D3DXVECTOR3& value);
		};

		class ShellEffect : public EventEffect
		{
			using _MyBase = EventEffect;
		private:
			float _fadeInTime;
			float _fadeOutTime;
			float _dmgTime;
			D3DXVECTOR3 _scale;
			D3DXVECTOR3 _scaleK;
		protected:
			void OnShellStatus(bool status) override;
			void OnDamage(GameObject* sender, float value, DamageType damageType) override;

			void SaveSource(SWriter* writer) override;
			void LoadSource(SReader* reader) override;
		public:
			ShellEffect(Behaviors* owner);

			void OnProgress(float deltaTime) override;

			const D3DXVECTOR3& GetScaleK() const;
			void SetScaleK(const D3DXVECTOR3& value);
		};

		class SlowEffect : public EventEffect
		{
			using _MyBase = EventEffect;
		protected:
			void OnDestroyEffect(MapObj* sender) override;
		public:
			SlowEffect(Behaviors* owner);

			void OnProgress(float deltaTime) override;
		};

		class SuperSlowEffect : public EventEffect
		{
			using _MyBase = EventEffect;
		protected:
			void OnDestroyEffect(MapObj* sender) override;
		public:
			SuperSlowEffect(Behaviors* owner);

			void OnProgress(float deltaTime) override;
		};

		class SoundMotor : public Behavior
		{
			using _MyBase = Behavior;
		private:
			snd::Sound* _sndIdle;
			snd::Sound* _sndRPM;

			bool _init;
			float _curRPM{};
			snd::Source3d* _srcIdle;
			snd::Source3d* _srcRPM;
			D3DXVECTOR2 _rpmVolumeRange;
			D3DXVECTOR2 _rpmFreqRange;

			void Init();
			void Free();
		protected:
			void OnMotor(float deltaTime, float rpm, float minRPM, float maxRPM) override;

			void SaveSource(SWriter* writer) override;
			void LoadSource(SReader* reader) override;
			void OnFixUp(const FixUpNames& fixUpNames) override;
		public:
			SoundMotor(Behaviors* owner);
			~SoundMotor() override;

			void OnProgress(float deltaTime) override;

			snd::Sound* GetSndIdle() const;
			void SetSndIdle(snd::Sound* value);

			snd::Sound* GetSndRPM() const;
			void SetSndRPM(snd::Sound* value);

			const D3DXVECTOR2& GetRPMVolumeRange() const;
			void SetRPMVolumeRange(const D3DXVECTOR2& value);

			const D3DXVECTOR2& GetRPMFreqRange() const;
			void SetRPMFreqRange(const D3DXVECTOR2& value);
		};

		class GusenizaAnim : public Behavior
		{
			using _MyBase = Behavior;
		private:
			float _xAnimOff;
		public:
			GusenizaAnim(Behaviors* owner);
			~GusenizaAnim() override;

			void OnProgress(float deltaTime) override;
		};

		class PodushkaAnim : public Behavior
		{
			using _MyBase = Behavior;
		private:
			int _targetTag;
			graph::IVBMeshNode* _target;
		protected:
			void SaveSource(SWriter* writer) override;
			void LoadSource(SReader* reader) override;
		public:
			PodushkaAnim(Behaviors* owner);

			void OnProgress(float deltaTime) override;

			int targetTag() const;
			void targetTag(int value);
		};

		enum BehaviorType
		{
			btTouchDeath = 0,
			btResurrectObj,
			btFxSystemWaitingEnd,
			btFxSystemSrcSpeed,
			btLowLifePoints,
			btBurnEffect,
			btDamageEffect,
			btDeathEffect,
			btLifeEffect,
			btSlowEffect,
			btSuperSlowEffect,
			btPxWheelSlipEffect,
			btPxDriftEffect,
			btShotEffect,
			btImmortalEffect,
			btShellEffect,
			btSoundMotor,
			btGusenizaAnim,
			btPodushkaAnim,
			cBehaviorTypeEnd
		};

		class Behaviors : public Collection<Behavior, BehaviorType, Behaviors*, Behaviors*>
		{
			friend class GameObject;

			using _MyBase = Collection<Behavior, BehaviorType, Behaviors*, Behaviors*>;
		public:
			using ClassList = ClassList;

			static ClassList classList;

			static void InitClassList();
		private:
			GameObject* _gameObj;
		protected:
			void InsertItem(const Value& value) override;
			void RemoveItem(const Value& value) override;
		public:
			Behaviors(GameObject* gameObj);
			~Behaviors() override;

			Behavior* Find(BehaviorType type);
			template <class _Type>
			_Type* Find();

			void OnProgress(float deltaTime);
			//выстрел
			//pos - относительные координаты
			void OnShot(const D3DXVECTOR3& pos);
			void OnMotor(float deltaTime, float rpm, float minRPM, float maxRPM);
			void OnImmortalStatus(bool status);
			void OnShellStatus(bool status);

			GameObject* GetGameObj() const;

			bool storeProxy;
			bool storeSource;
		};


		template <class _Type>
		_Type* Behaviors::Find()
		{
			return lsl::StaticCast<_Type*>(Find(classList.GetByClass<_Type>().GetKey()));
		}
	}
}
