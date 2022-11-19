#ifndef R3D_GAME_GAMEOBJECT
#define R3D_GAME_GAMEOBJECT

#include "GameBase.h"

namespace r3d
{

namespace game
{

class Proj;
class GameCar;

class GameObject: public lsl::Component, public GameObjListener, public px::ActorUser, IFrameEvent, IProgressEvent, ILateProgressEvent, IFixedStepEvent, public virtual IGameEvent
{
	friend class MapObj;
	friend class PxActorUser;
	friend class Logic;
private:
	typedef Component _MyBase;
public:
	typedef std::list<GameObject*> Children;
	typedef MapObjects IncludeList;

	typedef lsl::Container<GameObjListener*> ListenerList;

	enum LiveState {lsLive, lsDeath};

	struct RayCastHit
	{
		GameObject* gameActor;
		float distance;
	};

	struct MyEventData: public EventData
	{
		int targetPlayerId;
		GameObject* target;
		float damage;
		GameObject::DamageType damageType;

		MyEventData(): targetPlayerId(cUndefPlayerId), target(NULL) {}
		MyEventData(int mPlayerId, int mTargetPlayerId = cUndefPlayerId, GameObject* mTarget = NULL, float mDamage = 0.0f, GameObject::DamageType mDamageType = GameObject::dtSimple): EventData(mPlayerId), targetPlayerId(mTargetPlayerId), target(mTarget), damage(mDamage), damageType(mDamageType) {}
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
	float _maxTimeLife;
	//Текущее число здоровья
	float _life;
	float _timeLife;
	//
	bool _immortalFlag;
	//
	float _immortalTime;
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

	glm::vec3 _posSync;
	glm::vec3 _posSyncDir;
	float _posSyncLength;

	glm::quat _rotSync;
	glm::vec3 _rotSyncAxis;
	float _rotSyncAngle;

	glm::vec3 _posSync2;
	glm::vec3 _posSyncDir2;
	float _posSyncDist2;
	float _posSyncLength2;

	glm::quat _rotSync2;
	glm::vec3 _rotSyncAxis2;
	float _rotSyncAngle2;
	float _rotSyncLength2;

	glm::vec3 _pxPosLerp;
	glm::quat _pxRotLerp;
	glm::vec3 _pxVelocityLerp;

	glm::vec3 _pxPrevPos;
	glm::quat _pxPrevRot;
	glm::vec3 _pxPrevVelocity;

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

	glm::vec3 GetContactPoint(const px::Scene::OnContactEvent& contact);
	bool ContainsContactGroup(NxContactStreamIterator& contIter, unsigned actorIndex, px::Scene::CollDisGroup group);

	//изменения ссылки на объект логки, хак
	virtual void LogicReleased() {};
	virtual void LogicInited() {};

	virtual bool OnContactModify(const px::Scene::OnContactModifyEvent& contact) {return true;}
	virtual void OnContact(const px::Scene::OnContactEvent& contact);
	virtual void OnWake();
	virtual void OnSleep();

	virtual void OnImmortalStatus(bool status) {}

	void DoDeath(DamageType damageType = dtSimple, GameObject* target = NULL);
	void SendDeath(DamageType damageType = dtSimple, GameObject* target = NULL);

	//Синхронизация с физикой, дожна вызваться до OnProgress
	virtual void OnPxSync(float alpha);
	virtual void OnLateProgress(float deltaTime, bool pxStep);
	virtual void OnFrame(float deltaTime, float pxAlpha);
	virtual void OnFixedStep(float deltaTime);

	void SaveCoords(lsl::SWriter* writer);
	void LoadCoords(lsl::SReader* reader);
	//
	virtual void SaveSource(lsl::SWriter* writer);
	virtual void LoadSource(lsl::SReader* reader);
	//
	virtual void SaveProxy(lsl::SWriter* writer);
	virtual void LoadProxy(lsl::SReader* reader);

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
	virtual void OnFixUp(const FixUpNames& fixUpNames);
public:
	GameObject();
	virtual ~GameObject();

	void Assign(GameObject* value);

	virtual void InsertChild(GameObject* value);
	virtual void RemoveChild(GameObject* value);
	void ClearChildren();

	void InsertListener(GameObjListener* value);
	void RemoveListener(GameObjListener* value);
	void ClearListenerList();

	void Death(DamageType damageType = dtSimple, GameObject* target = NULL);
	void Resc();
	void Damage(int senderPlayerId, float value, float newLife, bool death, DamageType damageType);
	void Damage(int senderPlayerId, float value, DamageType damageType = dtSimple);
	//поделчить, клампится до MaxLife
	void Healt(float life);
	void LowLife(Behavior* behavior);

	bool GetImmortalFlag() const;
	void SetImmortalFlag(bool value);

	void Immortal(float time);
	bool IsImmortal() const;

	MapObj* GetMapObj();

	Logic* GetLogic();
	void SetLogic(Logic* value);

	void SendEvent(unsigned id, int playerId, MyEventData* data = NULL);
	void SendEvent(unsigned id, MyEventData* data = NULL);

	//это методы для установления связи включения как объекта сцены
	GameObject* GetParent();
	void SetParent(GameObject* value);
	const Children& GetChildren() const;

	IncludeList& GetIncludeList();

	graph::Actor& GetGrActor();
	px::Actor& GetPxActor();
	NxActor* GetNxActor();

	virtual Proj* IsProj();
	virtual GameCar* IsCar();

	//Интерфейс пользователя для изменения трансформации
	//Локальные трансформации
	//Позиция
	const glm::vec3& GetPos() const;
	virtual void SetPos(const glm::vec3& value);
	//Растяжение
	const glm::vec3& GetScale() const;
	virtual void SetScale(const glm::vec3& value);
	void SetScale(float value);
	//Поворот
	const glm::quat& GetRot() const;
	virtual void SetRot(const glm::quat& value);
	//Абсолютные трансформации
	//Позиция
	glm::vec3 GetWorldPos() const;
	virtual void SetWorldPos(const glm::vec3& value);
	//Поворот
	glm::quat GetWorldRot() const;
	virtual void SetWorldRot(const glm::quat& value);

	void SetWorldDir(const glm::vec3& value);
	void SetWorldUp(const glm::vec3& value);

	const glm::vec3& GetPosSync() const;
	void SetPosSync(const glm::vec3& value);

	const glm::quat& GetRotSync() const;
	void SetRotSync(const glm::quat& value);

	const glm::vec3& GetPosSync2() const;
	void SetPosSync2(const glm::vec3& curSync, const glm::vec3& newSync);

	const glm::quat& GetRotSync2() const;
	void SetRotSync2(const glm::quat& curSync, const glm::quat& newSync);

	const glm::vec3& GetPxPosLerp() const;
	const glm::quat& GetPxRotLerp() const;
	const glm::vec3& GetPxVelocityLerp() const;

	const glm::vec3& GetPxPrevPos() const;
	const glm::quat& GetPxPrevRot() const;
	const glm::vec3& GetPxPrevVelocity() const;

	LiveState GetLiveState() const;

	float GetMaxLife() const;
	void SetMaxLife(float value);

	float GetTimeLife() const;
	void SetTimeLife(float value);

	float GetMaxTimeLife() const;
	void SetMaxTimeLife(float value);

	float GetLife() const;
	void SetLife(float value);

	int GetTouchPlayerId() const;

	Behaviors& GetBehaviors();

	bool storeSource;
	bool storeProxy;

	virtual void OnProgress(float deltaTime);

	static GameObject* GetGameObjFromActor(px::Actor* actor);
	static GameObject* GetGameObjFromActor(NxActor* actor);
	static GameObject* GetGameObjFromShape(NxShape* shape);
};

}

}

#endif