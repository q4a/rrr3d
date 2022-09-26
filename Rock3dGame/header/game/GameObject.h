#ifndef R3D_GAME_GAMEOBJECT
#define R3D_GAME_GAMEOBJECT

#include "GameBase.h"

namespace r3d
{
	namespace game
	{
		class Proj;
		class GameCar;

		class GameObject : public Component, public GameObjListener, public px::ActorUser, IFrameEvent, IProgressEvent,
		                   ILateProgressEvent, IFixedStepEvent, public virtual IGameEvent
		{
			friend class MapObj;
			friend class PxActorUser;
			friend class Logic;
		private:
			using _MyBase = Component;
		public:
			using Children = std::list<GameObject*>;
			using IncludeList = MapObjects;

			using ListenerList = Container<GameObjListener*>;

			enum LiveState { lsLive, lsDeath };

			struct RayCastHit
			{
				GameObject* gameActor;
				float distance;
			};

			struct MyEventData : public EventData
			{
				int targetPlayerId;
				GameObject* target;
				float damage;
				DamageType damageType;

				MyEventData(): targetPlayerId(cUndefPlayerId), target(nullptr)
				{
				}

				MyEventData(int mPlayerId, int mTargetPlayerId = cUndefPlayerId, GameObject* mTarget = nullptr,
				            float mDamage = 0.0f,
				            DamageType mDamageType = dtSimple): EventData(mPlayerId), targetPlayerId(mTargetPlayerId),
				                                                target(mTarget), damage(mDamage),
				                                                damageType(mDamageType)
				{
				}
			};

		private:
			MapObj* _mapObj;
			Logic* _logic;

			graph::Actor* _grActor;
			px::Actor* _pxActor;

			GameObject* _parent;
			Children _children;
			IncludeList* _includeList;
			ListenerList _listenerList;
			Behaviors* _behaviors;

			LiveState _liveState;
			//Начальное количество здоровья
			//<0 - бесконечное число здоровья
			float _maxLife;
			bool _reverse;
			bool _rage;
			float _maxTimeLife;
			//Текущее число здоровья
			float _life;
			float _timeLife;
			//
			bool _immortalFlag;
			//
			bool _shellFlag;
			//
			float _immortalTime;
			//
			float _shellTime;
			//вспомогательный флаг, чтобы избежать двойного вызова Destroy
			bool _destroy;

			unsigned _frameEventCount;
			unsigned _progressEventCount;
			unsigned _lateProgressEventCount;
			unsigned _fixedStepEventCount;
			bool _syncFrameEvent;
			bool _bodyProgressEvent;

			int _touchPlayerId;
			float _touchPlayerTime;

			D3DXVECTOR3 _posSync;
			D3DXVECTOR3 _posSyncDir;
			float _posSyncLength;

			D3DXQUATERNION _rotSync;
			D3DXVECTOR3 _rotSyncAxis;
			float _rotSyncAngle;

			D3DXVECTOR3 _posSync2;
			D3DXVECTOR3 _posSyncDir2;
			float _posSyncDist2;
			float _posSyncLength2;

			D3DXQUATERNION _rotSync2;
			D3DXVECTOR3 _rotSyncAxis2;
			float _rotSyncAngle2;
			float _rotSyncLength2;

			D3DXVECTOR3 _pxPosLerp;
			D3DXQUATERNION _pxRotLerp;
			D3DXVECTOR3 _pxVelocityLerp;

			D3DXVECTOR3 _pxPrevPos;
			D3DXQUATERNION _pxPrevRot;
			D3DXVECTOR3 _pxPrevVelocity;

			void SetSyncFrameEvent(bool value);
			void SetBodyProgressEvent(bool value);
		protected:
			void Destroy();

			void RegFrameEvent();
			void UnregFrameEvent();

			void RegProgressEvent();
			void UnregProgressEvent();

			void RegLateProgressEvent();
			void UnregLateProgressEvent();

			void RegFixedStepEvent();
			void UnregFixedStepEvent();

			static D3DXVECTOR3 GetContactPoint(const px::Scene::OnContactEvent& contact);
			static bool ContainsContactGroup(NxContactStreamIterator& contIter, unsigned actorIndex,
			                                 px::Scene::CollDisGroup group);

			//изменения ссылки на объект логки, хак
			virtual void LogicReleased()
			{
			};

			virtual void LogicInited()
			{
			};

			bool OnContactModify(const px::Scene::OnContactModifyEvent& contact) override { return true; }
			void OnContact(const px::Scene::OnContactEvent& contact) override;
			void OnWake() override;
			void OnSleep() override;

			virtual void OnImmortalStatus(bool status)
			{
			}

			virtual void OnShellStatus(bool status)
			{
			}

			void RayCastClosestActor(const D3DXVECTOR3& rayStart, const D3DXVECTOR3& rayDir, NxShapesType shapesType,
			                         RayCastHit& hit, unsigned groups = 0xFFFFFFFF, unsigned mask = 0,
			                         float maxDist = NX_MAX_F32) const;

			void DoDeath(DamageType damageType = dtSimple, GameObject* target = nullptr);
			void SendDeath(DamageType damageType = dtSimple, GameObject* target = nullptr);

			//Синхронизация с физикой, дожна вызваться до OnProgress
			virtual void OnPxSync(float alpha);
			void OnLateProgress(float deltaTime, bool pxStep) override;
			void OnFrame(float deltaTime, float pxAlpha) override;
			void OnFixedStep(float deltaTime) override;

			void SaveCoords(SWriter* writer);
			void LoadCoords(SReader* reader);
			//
			virtual void SaveSource(SWriter* writer);
			virtual void LoadSource(SReader* reader);
			//
			virtual void SaveProxy(SWriter* writer);
			virtual void LoadProxy(SReader* reader);

			void Save(SWriter* writer) override;
			void Load(SReader* reader) override;
			void OnFixUp(const FixUpNames& fixUpNames) override;
		public:
			GameObject();
			~GameObject() override;

			void Assign(GameObject* value);

			virtual void InsertChild(GameObject* value);
			virtual void RemoveChild(GameObject* value);
			void ClearChildren();

			void InsertListener(GameObjListener* value);
			void RemoveListener(GameObjListener* value);
			void ClearListenerList();

			void Death(DamageType damageType = dtSimple, GameObject* target = nullptr);
			void Resc();
			void Damage(int senderPlayerId, float value, float newLife, bool death, DamageType damageType);
			void Damage(int senderPlayerId, float value, DamageType damageType = dtSimple);
			//поделчить, клампится до MaxLife
			void Healt(float life);
			void LowLife(Behavior* behavior);

			bool GetImmortalFlag() const;
			void SetImmortalFlag(bool value);

			bool GetShellFlag() const;
			void SetShellFlag(bool value);

			void Immortal(float time);
			bool IsImmortal() const;

			void Shell(float time);
			bool IsShell() const;

			MapObj* GetMapObj() const;

			Logic* GetLogic() const;
			void SetLogic(Logic* value);

			void SendEvent(unsigned id, int playerId, MyEventData* data = nullptr) const;
			void SendEvent(unsigned id, MyEventData* data = nullptr) const;

			//это методы для установления связи включения как объекта сцены
			GameObject* GetParent() const;
			void SetParent(GameObject* value);
			const Children& GetChildren() const;

			IncludeList& GetIncludeList() const;

			graph::Actor& GetGrActor() const;
			px::Actor& GetPxActor() const;
			NxActor* GetNxActor() const;

			virtual Proj* IsProj();
			virtual GameCar* IsCar();

			//Интерфейс пользователя для изменения трансформации
			//Локальные трансформации
			//Позиция
			const D3DXVECTOR3& GetPos() const;
			virtual void SetPos(const D3DXVECTOR3& value);
			//Растяжение
			const D3DXVECTOR3& GetScale() const;
			virtual void SetScale(const D3DXVECTOR3& value);
			void SetScale(float value);
			//Поворот
			const D3DXQUATERNION& GetRot() const;
			virtual void SetRot(const D3DXQUATERNION& value);
			//Абсолютные трансформации
			//Позиция
			D3DXVECTOR3 GetWorldPos() const;
			virtual void SetWorldPos(const D3DXVECTOR3& value);
			//Поворот
			D3DXQUATERNION GetWorldRot() const;
			virtual void SetWorldRot(const D3DXQUATERNION& value);

			void SetWorldDir(const D3DXVECTOR3& value);
			void SetWorldUp(const D3DXVECTOR3& value);

			const D3DXVECTOR3& GetPosSync() const;
			void SetPosSync(const D3DXVECTOR3& value);

			const D3DXQUATERNION& GetRotSync() const;
			void SetRotSync(const D3DXQUATERNION& value);

			const D3DXVECTOR3& GetPosSync2() const;
			void SetPosSync2(const D3DXVECTOR3& curSync, const D3DXVECTOR3& newSync);

			const D3DXQUATERNION& GetRotSync2() const;
			void SetRotSync2(const D3DXQUATERNION& curSync, const D3DXQUATERNION& newSync);

			const D3DXVECTOR3& GetPxPosLerp() const;
			const D3DXQUATERNION& GetPxRotLerp() const;
			const D3DXVECTOR3& GetPxVelocityLerp() const;

			const D3DXVECTOR3& GetPxPrevPos() const;
			const D3DXQUATERNION& GetPxPrevRot() const;
			const D3DXVECTOR3& GetPxPrevVelocity() const;

			LiveState GetLiveState() const;

			float GetMaxLife() const;
			void SetMaxLife(float value);

			bool GetReverse() const;
			void SetReverse(bool value);

			bool GetRage() const;
			void SetRage(bool value);

			float GetTimeLife() const;
			void SetTimeLife(float value);

			float GetMaxTimeLife() const;
			void SetMaxTimeLife(float value);

			float GetLife() const;
			void SetLife(float value);

			int GetTouchPlayerId() const;

			Behaviors& GetBehaviors() const;

			bool storeSource;
			bool storeProxy;

			void OnProgress(float deltaTime) override;

			static GameObject* GetGameObjFromActor(px::Actor* actor);
			static GameObject* GetGameObjFromActor(NxActor* actor);
			static GameObject* GetGameObjFromShape(NxShape* shape);
		};
	}
}

#endif
