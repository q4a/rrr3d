#ifndef R3D_GAME_LOGICMANAGER
#define R3D_GAME_LOGICMANAGER

#include "GameObject.h"
#include "Map.h"
#include "Player.h"
#include "snd\Audio.h"

namespace r3d
{

namespace game
{

class Map;
class Race;
class NetGame;
class Logic;

class LogicBehavior: public lsl::Object, public Serializable, protected IProgressEvent
{
	friend class LogicBehaviors;
private:
	LogicBehaviors* _owner;	
protected:
	void RegProgressEvent();
	void UnregProgressEvent();

	virtual void OnContact(const px::Scene::OnContactEvent& contact1, const px::Scene::OnContactEvent& contact2) {}
	virtual void OnProgress(float deltaTime) {}

	virtual void Save(SWriter* writer) {}
	virtual void Load(SReader* reader) {}
public:
	LogicBehavior(LogicBehaviors* owner);

	LogicBehaviors* GetOwner();
	Logic* GetLogic();
	Map* GetMap();
};

class LogicEventEffect: public LogicBehavior
{
	typedef LogicBehavior _MyBase;
private:
	class GameObjEvent: public GameObjListener
	{
	private:
		LogicEventEffect* _effect;
	public:
		GameObjEvent(LogicEventEffect* effect);

		virtual void OnDestroy(GameObject* sender);
	};

	typedef lsl::List<MapObj*> EffObjList;
protected:
	struct EffectDesc
	{
		EffectDesc(): pos(NullVector) {}

		D3DXVECTOR3 pos;
	};
private:
	GameObjEvent* _gameObjEvent;
	MapObjRec* _effect;
	EffObjList _effObjList;

	D3DXVECTOR3 _pos;

	void InsertEffObj(MapObj* mapObj);
	void RemoveEffObj(MapObj* mapObj);
	void ClearEffObjList();

	void DestroyEffObj(MapObj* mapObj);
protected:
	virtual MapObj* CreateEffect(const EffectDesc& desc);
	MapObj* CreateEffect();
	virtual void DeleteEffect(MapObj* mapObj);

	virtual void OnDestroyEffect(MapObj* sender) {}
	
	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
public:
	LogicEventEffect(LogicBehaviors* owner);
	virtual ~LogicEventEffect();

	MapObjRec* GetEffect();
	void SetEffect(MapObjRec* value);

	const D3DXVECTOR3& GetPos() const;
	void SetPos(const D3DXVECTOR3& value);
};

class PairPxContactEffect: public LogicEventEffect
{
	typedef LogicEventEffect _MyBase;
private:
	struct Key
	{
		Key(px::Actor* mActor1, px::Actor* mActor2): actor1(mActor1), actor2(mActor2) {}

		px::Actor* actor1;
		px::Actor* actor2;

		bool operator<(const Key& key) const
		{
			//альтернатива, некорректна
			//return (unsigned)actor1 + (unsigned)actor2 < (unsigned)key.actor1 + (unsigned)key.actor2;
			
			//с учетом перемены мест слагаемое не должно изменяться
			return actor1 == key.actor1 ? actor2 < key.actor2 : actor1 < key.actor1;
		}
	};
	struct Contact
	{
		Contact(): shape1(0), shape2(0), point(NullVector), effect(0), time(0) {}

		bool operator==(const Contact& contact) const
		{
			return effect == contact.effect;
		}

		NxShape* shape1;
		NxShape* shape2;
		D3DXVECTOR3 point;		

		MapObj* effect;
		float time;
	};
	typedef lsl::List<Contact> ContactList;
	struct ContactNode
	{
	private:
		ContactNode(const ContactNode& node);
		ContactNode& operator=(const ContactNode& contact);
	public:
		ContactNode(): source(0) {last = list.end();}

		ContactList list;
		ContactList::iterator last;
		snd::Source3d* source;
	};

	typedef std::map<Key, ContactNode*> ContactMap;
public:
	typedef lsl::Vector<snd::Sound*> Sounds;
private:
	Sounds _sounds;

	ContactMap _contactMap;

	ContactMap::iterator GetOrCreateContact(const Key& key);
	void InsertContact(ContactMap::iterator iter, NxShape* shape1, NxShape* shape2, const D3DXVECTOR3& point);
	ContactMap::iterator ReleaseContact(ContactMap::iterator iter, ContactList::iterator cIter1, ContactList::iterator cIter2, bool death, float deltaTime = 0.0f, float cRelTime = -1.0f);
	void ReleaseContacts(bool death);

	void RemoveContactByEffect(MapObj* effect);	
protected:
	virtual void OnDestroyEffect(MapObj* sender);
	virtual void OnContact(const px::Scene::OnContactEvent& contact1, const px::Scene::OnContactEvent& contact2);
	virtual void OnProgress(float deltaTime);
public:
	PairPxContactEffect(LogicBehaviors* owner);
	virtual ~PairPxContactEffect();
	
	void InsertSound(snd::Sound* value);
	Sounds::iterator RemoveSound(Sounds::const_iterator iter);
	void RemoveSound(snd::Sound* sound);
	void ClearSounds();

	const Sounds& GetSounds() const;
};

enum LogicBehaviorType {lbtPairPxContactEffect = 0, cLogicBehaviorTypeEnd};

class LogicBehaviors: public lsl::Collection<LogicBehavior, LogicBehaviorType, LogicBehaviors*, LogicBehaviors*>
{
	friend class Logic;

	typedef lsl::Collection<LogicBehavior, LogicBehaviorType, LogicBehaviors*, LogicBehaviors*> _MyBase;
public:
	typedef _MyBase::ClassList ClassList;

	static ClassList classList;

	static void InitClassList();
private:
	Logic* _logic;

	void OnContact(const px::Scene::OnContactEvent& contact1, const px::Scene::OnContactEvent& contact2);
public:
	LogicBehaviors(Logic* logic);
	virtual ~LogicBehaviors();

	Logic* GetLogic();
};

class Map;

class Logic: public lsl::Object, public Serializable
{
public:
	enum SndCategory {scMusic = 0, scEffects, scVoice, cSndCategoryEnd};
private:
	typedef lsl::List<GameObject*> GameObjList;	

	class PxSceneUser: public px::SceneUser
	{
	private:
		Logic* _logic;
	protected:
		virtual void OnContact(const px::Scene::OnContactEvent& contact1, const px::Scene::OnContactEvent& contact2);
	public:
		PxSceneUser(Logic* logic);
	};
private:
	World* _world;
	LogicBehaviors* _behaviors;
	PxSceneUser* _pxSceneUser;
	GameObjList _gameObjList;

	snd::SubmixVoice* _sndCat[cSndCategoryEnd];
	float _volume[cSndCategoryEnd];
	bool _mute[cSndCategoryEnd];
	bool _initSndCat;

	D3DXVECTOR2 _touchBorderDamage;
	D3DXVECTOR2 _touchBorderDamageForce;
	D3DXVECTOR2 _touchCarDamage;
	D3DXVECTOR2 _touchCarDamageForce;

	void InitSndCat();
	void FreeSndCat();	
protected:
	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
public:
	Logic(World* world);
	virtual ~Logic();

	void RegFixedStepEvent(IFixedStepEvent* user);
	void UnregFixedStepEvent(IFixedStepEvent* user);

	void RegProgressEvent(IProgressEvent* user);
	void UnregProgressEvent(IProgressEvent* user);

	void RegLateProgressEvent(ILateProgressEvent* user);
	void UnregLateProgressEvent(ILateProgressEvent* user);

	void RegFrameEvent(IFrameEvent* user);
	void UnregFrameEvent(IFrameEvent* user);

	void RegGameObj(GameObject* gameObj);
	void CleanGameObjs();

	void Shot(Player* player, MapObj* target, Player::SlotType type);
	void Shot(Player* player, MapObj* target);
	void Damage(GameObject* sender, int senderPlayerId, GameObject* target, float value, GameObject::DamageType damageType);
	bool TakeBonus(GameObject* sender, GameObject* bonus, GameObject::BonusType type, float value);
	bool MineContact(Proj* sender, GameObject* target, const D3DXVECTOR3& point);

	snd::Source* CreateSndSource(SndCategory category);
	snd::Source3d* CreateSndSource3d(SndCategory category);
	void ReleaseSndSource(snd::Source* source);
	
	float GetVolume(SndCategory category);
	void SetVolume(SndCategory category, float value);
	void AutodetectVolume();
	void Mute(SndCategory category, bool value);

	const D3DXVECTOR2& GetTouchBorderDamage() const;
	void SetTouchBorderDamage(const D3DXVECTOR2& value);

	const D3DXVECTOR2& GetTouchBorderDamageForce() const;
	void SetTouchBorderDamageForce(const D3DXVECTOR2& value);

	const D3DXVECTOR2& GetTouchCarDamage() const;
	void SetTouchCarDamage(const D3DXVECTOR2& value);

	const D3DXVECTOR2& GetTouchCarDamageForce() const;
	void SetTouchCarDamageForce(const D3DXVECTOR2& value);

	void OnProgress(float deltaTime);

	bool IsNetGame();

	Map* GetMap();
	Race* GetRace();
	NetGame* GetNet();
	px::Scene* GetPxScene();
	snd::Engine* GetAudio();
	LogicBehaviors& GetBehaviors();	
};

}

}

#endif