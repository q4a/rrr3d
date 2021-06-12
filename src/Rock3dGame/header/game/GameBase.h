#pragma once

#include "MapObj.h"
#include "GameEvent.h"

#include "graph\\Actor.h"
#include "px\\PhysX.h"
#include "snd\Audio.h"

namespace r3d
{

namespace game
{

class Logic;
class Behavior;

enum ScActorType {satBaseObj, satMeshObj, cScActorTypeEnd};
enum Difficulty {gdEasy, gdNormal, gdHard, cDifficultyEnd};

extern const lsl::string cDifficultyStr[cDifficultyEnd];
extern const char* cScActorTypeStr[cScActorTypeEnd];

class GameObject;

class GameObjListener: public virtual ObjReference
{
public:
	enum DamageType
	{
		dtSimple,
		dtEnergy,
		dtMine,
		dtTouch,
		dtDeathPlane		
	};

	enum BonusType
	{
		btMoney,
		btCharge,
		btMedpack,
		btImmortal,

		cBonusTypeEnd
	};

	static const std::string cBonusTypeStr[cBonusTypeEnd];
public:
	//sender - отправитель сообщени€, может быть сам this
	//”даление объекта из пам€ти
	virtual void OnDestroy(GameObject* sender) {}
	//—мерть от нанесенного повреждени€
	virtual void OnDeath(GameObject* sender, DamageType damageType, GameObject* target) {}
	//Ќанесено повреждение
	virtual void OnDamage(GameObject* sender, float value, DamageType damageType) {}
	//ћало жизней, посылаетс€ Behavior
	virtual void OnLowLife(GameObject* sender, Behavior* behavior) {}
	//
	virtual void OnContact(const px::Scene::OnContactEvent& contact) {}
};

class Behavior: public lsl::Object, public GameObjListener, public Serializable
{
	friend class Behaviors;
protected:
	enum PxNotify {pxContact = 0, pxContactModify, cPxNotifyEnd};
	typedef lsl::Bitset<cPxNotifyEnd> PxNotifies;
private:
	Behaviors* _owner;
	PxNotifies _pxNotifies;
	bool _removed;
protected:
	virtual void OnShot(const D3DXVECTOR3& pos) {}
	virtual void OnMotor(float deltaTime, float rpm, float minRPM, float maxRPM) {}
	virtual void OnImmortalStatus(bool status) {}

	virtual void SaveSource(lsl::SWriter* writer) {}
	virtual void LoadSource(lsl::SReader* reader) {}
	virtual void SaveProxy(lsl::SWriter* writer) {}
	virtual void LoadProxy(lsl::SReader* reader) {}

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);

	bool GetPxNotify(PxNotify notify) const;
	void SetPxNotify(PxNotify notify, bool value);
public:
	Behavior(Behaviors* owner);

	virtual void OnProgress(float deltaTime) = 0;

	void Remove();
	bool IsRemoved() const;

	Behaviors* GetOwner();
	GameObject* GetGameObj();
	Logic* GetLogic();
};

class TouchDeath: public Behavior
{
	typedef Behavior _MyBase;
public:
	TouchDeath(Behaviors* owner);

	virtual void OnContact(const px::Scene::OnContactEvent& contact);
	virtual void OnProgress(float deltaTime) {}
};

class ResurrectObj: public Behavior
{
	typedef Behavior _MyBase;
private:
	bool _resurrect;
protected:
	virtual void Resurrect();
	virtual void OnDeath(GameObject* sender, DamageType damageType, GameObject* target);

	bool IsResurrect() const;
public:
	ResurrectObj(Behaviors* owner);

	virtual void OnProgress(float deltaTime) {}
};

class FxSystemWaitingEnd: public ResurrectObj
{
	typedef ResurrectObj _MyBase;
protected:
	virtual void Resurrect();
public:
	FxSystemWaitingEnd(Behaviors* owner);

	virtual void OnProgress(float deltaTime);
};

class FxSystemSrcSpeed: public Behavior
{
	typedef Behavior _MyBase;
public:
	FxSystemSrcSpeed(Behaviors* owner);

	virtual void OnProgress(float deltaTime);
};

class EventEffect: public Behavior
{
	typedef Behavior _MyBase;
private:
	class GameObjEvent: public GameObjListener
	{
	private:
		EventEffect* _effect;
	public:
		GameObjEvent(EventEffect* effect);

		virtual void OnDestroy(GameObject* sender);
	};

	typedef lsl::List<MapObj*> EffObjList;
protected:
	struct EffectDesc
	{
		EffectDesc(): pos(NullVector), rot(NullQuaternion), child(true), parent(NULL) {}

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

		EffectSound(): sound(NULL), source(NULL), source3d(NULL) {}
	};
	typedef lsl::Vector<EffectSound> SoundList;
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

	void DestroyEffObj(MapObj* mapObj, bool destrWorld = false);

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
	MapObj* GetMakeEffect();
	bool IsEffectMaked() const;

	//—пециально дл€ effect-ов
	virtual void OnDestroyEffect(MapObj* sender) {}

	virtual void SaveSource(lsl::SWriter* writer);
	virtual void LoadSource(lsl::SReader* reader);
	virtual void OnFixUp(const FixUpNames& fixUpNames);

	snd::Source* GiveSource();
	snd::Source3d* GiveSource3d();
public:
	EventEffect(Behaviors* owner);
	virtual ~EventEffect();

	virtual void OnProgress(float deltaTime);

	MapObjRec* GetEffect();
	void SetEffect(MapObjRec* value);

	void AddSound(snd::Sound* sound);
	void ClearSounds();
	const SoundList& GetSounds();	

	snd::Sound* GetSound();
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

class LowLifePoints: public EventEffect
{
	typedef EventEffect _MyBase;
private:
	float _lifeLevel;
protected:
	virtual void SaveSource(lsl::SWriter* writer);
	virtual void LoadSource(lsl::SReader* reader);
public:
	LowLifePoints(Behaviors* owner);

	virtual void OnProgress(float deltaTime);

	//уровень жизней, [0..1]
	float GetLifeLevel() const;
	void SetLifeLevel(float value);
};

class DamageEffect: public EventEffect
{
	typedef EventEffect _MyBase;
private:
	DamageType _damageType;
protected:
	virtual void OnDamage(GameObject* sender, float value, DamageType damageType);

	virtual void SaveSource(lsl::SWriter* writer);
	virtual void LoadSource(lsl::SReader* reader);
public:
	DamageEffect(Behaviors* owner);

	virtual void OnProgress(float deltaTime);

	DamageType GetDamageType() const;
	void SetDamageType(DamageType value);
};

class DeathEffect: public EventEffect
{
	typedef EventEffect _MyBase;
private:
	bool _effectPxIgnoreSenderCar;
	bool _targetChild;
protected:
	virtual void OnDeath(GameObject* sender, DamageType damageType, GameObject* target);
	virtual void SaveSource(lsl::SWriter* writer);
	virtual void LoadSource(lsl::SReader* reader);
public:
	DeathEffect(Behaviors* owner);

	virtual void OnProgress(float deltaTime);

	bool GetEffectPxIgnoreSenderCar() const;
	void SetEffectPxIgnoreSenderCar(bool value);

	bool GetTargetChild() const;
	void SetTargetChild(bool value);
};

class LifeEffect: public EventEffect
{
	typedef EventEffect _MyBase;
private:
	bool _play;
public:
	LifeEffect(Behaviors* owner);	

	virtual void OnProgress(float deltaTime);
};

class PxWheelSlipEffect: public EventEffect
{
	typedef EventEffect _MyBase;
public:
	PxWheelSlipEffect(Behaviors* owner);
	virtual ~PxWheelSlipEffect();

	virtual void OnProgress(float deltaTime);
};

class ShotEffect: public EventEffect
{
	typedef EventEffect _MyBase;
protected:
	virtual void OnShot(const D3DXVECTOR3& pos);
public:
	ShotEffect(Behaviors* owner);
};

class ImmortalEffect: public EventEffect
{
	typedef EventEffect _MyBase;
private:
	float _fadeInTime;
	float _fadeOutTime;
	float _dmgTime;
	D3DXVECTOR3 _scale;
	D3DXVECTOR3 _scaleK;
protected:
	virtual void OnImmortalStatus(bool status);
	virtual void OnDamage(GameObject* sender, float value, DamageType damageType);

	virtual void SaveSource(lsl::SWriter* writer);
	virtual void LoadSource(lsl::SReader* reader);
public:
	ImmortalEffect(Behaviors* owner);

	virtual void OnProgress(float deltaTime);

	const D3DXVECTOR3& GetScaleK() const;
	void SetScaleK(const D3DXVECTOR3& value);
};

class SlowEffect: public EventEffect
{
	typedef EventEffect _MyBase;
protected:
	virtual void OnDestroyEffect(MapObj* sender);
public:
	SlowEffect(Behaviors* owner);	

	virtual void OnProgress(float deltaTime);
};

class SoundMotor: public Behavior
{
	typedef Behavior _MyBase;
private:
	snd::Sound* _sndIdle;
	snd::Sound* _sndRPM;
	
	bool _init;
	float _curRPM;
	snd::Source3d* _srcIdle;
	snd::Source3d* _srcRPM;
	D3DXVECTOR2 _rpmVolumeRange;
	D3DXVECTOR2 _rpmFreqRange;

	void Init();
	void Free();
protected:
	virtual void OnMotor(float deltaTime, float rpm, float minRPM, float maxRPM);

	virtual void SaveSource(lsl::SWriter* writer);
	virtual void LoadSource(lsl::SReader* reader);
	virtual void OnFixUp(const FixUpNames& fixUpNames);
public:
	SoundMotor(Behaviors* owner);
	virtual ~SoundMotor();

	virtual void OnProgress(float deltaTime);

	snd::Sound* GetSndIdle();
	void SetSndIdle(snd::Sound* value);

	snd::Sound* GetSndRPM();
	void SetSndRPM(snd::Sound* value);

	const D3DXVECTOR2& GetRPMVolumeRange() const;
	void SetRPMVolumeRange(const D3DXVECTOR2& value);

	const D3DXVECTOR2& GetRPMFreqRange() const;
	void SetRPMFreqRange(const D3DXVECTOR2& value);
};

class GusenizaAnim: public Behavior
{
	typedef Behavior _MyBase;
private:	
	float _xAnimOff;	
public:
	GusenizaAnim(Behaviors* owner);
	virtual ~GusenizaAnim();

	virtual void OnProgress(float deltaTime);
};

class PodushkaAnim: public Behavior
{
	typedef Behavior _MyBase;
private:	
	int _targetTag;
	graph::IVBMeshNode* _target;
protected:
	virtual void SaveSource(lsl::SWriter* writer);
	virtual void LoadSource(lsl::SReader* reader);
public:
	PodushkaAnim(Behaviors* owner);

	virtual void OnProgress(float deltaTime);

	int targetTag() const;
	void targetTag(int value);
};

enum BehaviorType {btTouchDeath = 0, btResurrectObj, btFxSystemWaitingEnd, btFxSystemSrcSpeed, btLowLifePoints, btDamageEffect, btDeathEffect, btLifeEffect, btSlowEffect, btPxWheelSlipEffect, btShotEffect, btImmortalEffect, btSoundMotor, btGusenizaAnim, btPodushkaAnim, cBehaviorTypeEnd};

class Behaviors: public lsl::Collection<Behavior, BehaviorType, Behaviors*, Behaviors*>
{
	friend class GameObject;

	typedef lsl::Collection<Behavior, BehaviorType, Behaviors*, Behaviors*> _MyBase;
public:
	typedef _MyBase::ClassList ClassList;

	static ClassList classList;

	static void InitClassList();
private:
	GameObject* _gameObj;
protected:
	virtual void InsertItem(const Value& value);
	virtual void RemoveItem(const Value& value);
public:
	Behaviors(GameObject* gameObj);
	virtual ~Behaviors();

	Behavior* Find(BehaviorType type);
	template<class _Type> _Type* Find();

	void OnProgress(float deltaTime);
	//выстрел
	//pos - относительные координаты
	void OnShot(const D3DXVECTOR3& pos);
	void OnMotor(float deltaTime, float rpm, float minRPM, float maxRPM);
	void OnImmortalStatus(bool status);

	GameObject* GetGameObj();

	bool storeProxy;
	bool storeSource;
};




template<class _Type> _Type* Behaviors::Find()
{
	return lsl::StaticCast<_Type*>(Find(classList.GetByClass<_Type>().GetKey()));
}

}

}