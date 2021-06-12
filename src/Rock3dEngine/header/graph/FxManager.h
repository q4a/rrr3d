#ifndef R3D_SCENE_FXMANAGER
#define R3D_SCENE_FXMANAGER

#include "StdNode.h"
#include "px\\Physx.h"

#include "lslSerialValue.h"
#include "lslMath.h"

namespace r3d
{

namespace graph
{

class SceneNode;
class FxManager;

class FxParticle: public lsl::Object
{
private:
	D3DXVECTOR3 _pos;
	D3DXQUATERNION _rot;
	D3DXVECTOR3 _scale;
	
	mutable D3DXMATRIX _worldMat;
	mutable bool _worldMatChanged;

	mutable AABB _aabb;
	mutable bool _aabbChanged;

	BaseSceneNode* _child;

	void BuildWorldMat() const;
	void BuildAABB() const;
	void TransformChanged();
public:
	FxParticle();
	virtual ~FxParticle();

	const D3DXVECTOR3& GetPos();
	void SetPos(const D3DXVECTOR3& value);

	const D3DXQUATERNION& GetRot();
	void SetRot(const D3DXQUATERNION& value);

	const D3DXVECTOR3& GetScale();
	void SetScale(const D3DXVECTOR3& value);

	const D3DXMATRIX& GetMatrix() const;

	AABB GetAABB() const;

	BaseSceneNode* GetChild();
	void SetChild(BaseSceneNode* value);

	unsigned index;
	float time;
};

class FxParticleGroup: public lsl::Container<FxParticle*>
{
	friend class FxEmitter;
public:
	virtual ~FxParticleGroup()
	{
		LSL_ASSERT(Empty());
	}

	float maxLife;
	float life;
};

class FxEmitter: public lsl::Object, public lsl::Serializable
{	
	friend class FxParticleSystem;
	friend class FxManager;
private:
	typedef lsl::Object _MyBase;
	typedef lsl::Container<FxParticleGroup*> _GroupList;
public:
	enum MaxNumAction {mnaReplaceLatest = 0, mnaWaitingFree, cMaxNumActionEnd};
	static const char* cMaxNumActionStr[cMaxNumActionEnd];

	enum StartType 
	{
		//Относительно времени
		sotTime = 0,
		//Относительно расстояния между частицами
		sotDist,
		//Комбинированный, при выполнений хотябы одного условия из sotTime, sotDist
		sotCombine,
		
		//
		cStartTypeEnd
	};

	static const char* cStartTypeStr[cStartTypeEnd];

	struct ParticleDesc
	{
		ParticleDesc();

		//Максимально возможное число частиц
		//0 - то макс. число частиц не ограничено
		unsigned maxNum;
		//При достижении предела maxNum:
		// mnaWaitingFree - Новые не создаются, ожидается уничтожение старых
		// mnaReplaceLatest - Убиваются самые старые частицы и взамен них создаются новые (по умолчанию). Из-за  особенности реализации удаляются не самые старые!
		MaxNumAction maxNumAction;
		//Время жизни (в сек.) одной частицы. 
		//0 - Время жизни не ограничено
		FloatRange life;
		//Смещение в создании (в сек.) частиц
		FloatRange startTime;
		//Время в течении которого будут создаваться частицы
		//0 - неограничено
		float startDuration;
		//Тип смещения в создании
		StartType startType;
		//Плотность, или среднее число частиц за один момент создания. Такие частицы группируются и каждая группа характеризуется одинаковым моментом создания и смерти
		FloatRange density;
		//
		Vec3Range startPos;
		Vec3Range startScale;
		QuatRange startRot;

		FloatRange rangeLife;
		Vec3Range rangePos;
		Vec3Range rangeScale;
		QuatRange rangeRot;
	};
private:
	FxParticleSystem* _owner;
	ParticleDesc _particleDesc;
	//Если true то частица определена в мировой системе координат
	bool _worldCoordSys;
	bool _modeFading;

	_GroupList _groupList;

	float _curTime;
	//момент создания последней группы
	float _lastTimeQGroup;
	D3DXVECTOR3 _lastPosQGroup;
	//
	float _curDensParticle;
	unsigned _cntParticles;
	unsigned _numParticle;

	float _nextTimeCreate;
	float _nextDistCreate;

	FxParticleGroup* AddGroup();
	void DelGroup(_GroupList::iterator iter);
	void DelGroup(const _GroupList::Position& pos);
	void DelGroup(_GroupList::iterator stIter, _GroupList::iterator endIter);	
	void DelGroup(FxParticleGroup* value);
	void ClearGroupList();

	FxParticle* AddParticle(FxParticleGroup* group);
	void DelParticle(FxParticleGroup* group, FxParticleGroup::iterator iter);
	void DelParticle(FxParticleGroup* group, FxParticleGroup::iterator sIter, FxParticleGroup::iterator eIter);
	void DelParticle(FxParticleGroup* group, FxParticle* value);
	void ClearParticles(FxParticleGroup* group);

	//возвращает deltaTime >=0 если необходим запрос на создание
	float CheckTimeCreateQuery(D3DXVECTOR3& offPos);
	float CheckDistCreateQuery(D3DXVECTOR3& offPos);
protected:
	unsigned GetNextPartIndex() const;
	float CompRangeFrame(unsigned index) const;

	virtual FxParticle* CreateParticle();
	virtual void DestroyParticle(FxParticle* value);

	//Обновление или инциализация частицы для начального периода жизни
	//init указывает на начальную инициализацию частицы
	virtual void UpdateParticle(FxParticle* value, float dTime, bool init);
	virtual void UpdateGroup(FxParticleGroup* group, float dTime, bool init);

	//Запрос на создание num частиц с учетом условий ограничения
	void QueryCreateParticles(unsigned num, float deltaTime, const D3DXVECTOR3& offPos);
	void QueryCreateGroup(float deltaTime, const D3DXVECTOR3& offPos);	
	
	virtual AABB LocalDimensions() const;
	virtual void OnProgress(float deltaTime);

	//
	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);	
public:
	FxEmitter(FxParticleSystem* owner);
	virtual ~FxEmitter();

	void Reset();	

	FxParticleSystem* GetSystem();

	const ParticleDesc& GetParticleDesc() const;
	void SetParticleDesc(const ParticleDesc& value);

	unsigned GetCntParticle() const;

	//Используется ли мировая координатная система
	//true - эмиттер расчитывается в глобальной координатной системе, т.е. частицы создаются отностиельной локальных координат а живут в глобальных
	//false - в локальной, т.е. система координат передвигается вместе с эмиттером
	bool GetWorldCoordSys() const;
	void SetWorldCoordSys(bool value);
	//режим затухания
	bool GetModeFading() const;
	void SetModeFading(bool value);

	D3DXVECTOR3 GetLocalPos(FxParticle* particle) const;
	D3DXVECTOR3 GetWorldPos(FxParticle* particle) const;

	const D3DXMATRIX& GetMatrix() const;	
};

class FxParticleSystem: public BaseSceneNode
{
	friend FxEmitter;
private:
	typedef BaseSceneNode _MyBase;
public:
	enum EmitterType {etBase = 0, etFlow, cEmitterTypeEnd};
	
	class Emitters: public lsl::Collection<FxEmitter, EmitterType, FxParticleSystem*, void>
	{
	private:
		typedef lsl::Collection<FxEmitter, EmitterType, FxParticleSystem*, void> _MyBase;
	private:
		FxParticleSystem* _owner;
	protected:
		virtual void LoadItem(lsl::SReader* reader)
		{
			LoadItemFrom(reader, _owner);
		}
	public:
		Emitters(FxParticleSystem* owner): _owner(owner) {}

		Item& Add(EmitterType key)
		{
			return _MyBase::Add(key, _owner);
		}
		template<class _Type> _Type& Add()
		{
			return _MyBase::Add<_Type>(_owner);
		}
	};

	typedef Emitters::ClassList ClassList;
	static ClassList classList;

	enum ChildStyle 
	{
		//Прокси объекты, структура всех объектов одинакова
		csProxy = 0,

		//Уникальные объекты для каждой частицы
		csUnique, 
		
		cChildStyleEnd
	};

	static const char* cChildStyleStr[cChildStyleEnd];

	static void InitClassList();
private:
	FxManager* _fxManager;
	Emitters* _emitters;
	SceneNode* _child;
	ChildStyle _childStyle;

	AABB _aabb;
	D3DXVECTOR3 _srcSpeed;
protected:
	void OnCreateParticle(FxParticle* value);
	void OnDestroyParticle(FxParticle* value);
	void OnUpdateParticle(FxEmitter* emitter, FxParticle* value, float dTime, bool init);

	FxParticleGroup* AddGroup();
	void DelGroup(FxParticleGroup* value);

	virtual AABB LocalDimensions() const;

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
	virtual void OnFixUp(const FixUpNames& fixUpNames);
public:
	FxParticleSystem();
	virtual ~FxParticleSystem();

	virtual void DoRender(graph::Engine& engine);
	virtual void OnProgress(float deltaTime);

	void Reset();

	FxManager* GetFxManager();
	void SetFxManager(FxManager* value);

	Emitters& GetEmitters();

	//Прикрепляется к каждой частице
	SceneNode& GetChild();

	ChildStyle GetChildStyle() const;
	void SetChildStyle(ChildStyle value);
	//число частиц
	unsigned GetCntParticle() const;
	//режим затухания
	void SetModeFading(bool value);

	const D3DXVECTOR3& GetSrcSpeed() const;
	void SetSrcSpeed(const D3DXVECTOR3& value);

	MaterialNode material;
};

class FxManager: public lsl::Component
{
	friend class FxParticleSystem;
protected:
	class _GroupList: public lsl::Collection<FxParticleGroup, void, void, void>
	{
	private:
		FxManager* _owner;
	protected:
		virtual Item* CreateItem()
		{
			return _owner->CreateGroup();
		}
		virtual void DestroyItem(Item* value)
		{
			_owner->DestroyGroup(value);
		}
	public:
		_GroupList(FxManager* owner): _owner(owner) {}
	};

	typedef FxEmitter::_GroupList EmitterGroups;
private:
	_GroupList* _groupList;
protected:
	virtual FxParticleGroup* CreateGroup();
	virtual void DestroyGroup(FxParticleGroup* value);

	virtual void RenderGroup(graph::Engine& engine, FxEmitter* emitter, FxParticleGroup* group) = 0;
	virtual void RenderEmitter(graph::Engine& engine, FxEmitter* emitter);
	virtual void RenderSystem(graph::Engine& engine, FxParticleSystem* system);

	_GroupList& GetGroupList();
	const EmitterGroups& GetEmitterGroups(const FxEmitter* emitter) const;
public:
	FxManager();
	virtual ~FxManager();
};

//Операции поворота частицы не поддерживаются
//Преобразования текстурных координат не поддерживаются
class FxPointSpritesManager: public FxManager
{
private:
	typedef FxManager _MyBase;
protected:
	virtual void RenderGroup(graph::Engine& engine, FxEmitter* emitter, FxParticleGroup* group);
	virtual void RenderSystem(graph::Engine& engine, FxParticleSystem* system);
};

class FxSpritesManager: public FxManager
{
private:
	typedef FxManager _MyBase;
protected:
	virtual void RenderGroup(graph::Engine& engine, FxEmitter* emitter, FxParticleGroup* group);
public:
	FxSpritesManager();

	bool dirSprite;
};

class FxPlaneManager: public FxManager
{
private:
	typedef FxManager _MyBase;
protected:
	virtual void RenderGroup(graph::Engine& engine, FxEmitter* emitter, FxParticleGroup* group);
public:
	FxPlaneManager();
};

class FxNodeManager: public FxManager
{
private:
	SceneNode* _node;
protected:
	virtual void RenderGroup(graph::Engine& engine, FxEmitter* emitter, FxParticleGroup* group);
public:
	FxNodeManager();
	virtual ~FxNodeManager();

	SceneNode& GetNode();
};

class FxTrailManager: public FxManager
{
private:
	typedef FxManager _MyBase;
public:
	enum TypeDraw 
	{
		//Отрисовка по группам частиц каждая из которых со своим материалом
		tdPerGroup,
		//Отрисовка за один батч с материалом последней группы
		tdLastGroup
	};
private:
	float _trailWidth;

	void BuildVertexLine(res::VertexPT* vertex, const D3DXVECTOR3& pos, const D3DXVECTOR3& dir, const D3DXVECTOR3& camPos, float xTex);
	void DrawPath(graph::Engine& engine, FxParticleSystem* system, FxParticleGroup* group, res::VertexPT* vBuf, unsigned primCnt, unsigned sPrim);
protected:	
	virtual void RenderGroup(graph::Engine& engine, FxEmitter* emitter, FxParticleGroup* group);
	virtual void RenderEmitter(graph::Engine& engine, FxEmitter* emitter);
	virtual void RenderSystem(graph::Engine& engine, FxParticleSystem* system);
public:
	FxTrailManager();
	virtual ~FxTrailManager();

	float GetTrailWidth() const;
	void SetTrailWidth(float value);

	//Фиксированная нормаль у следов
	D3DXVECTOR3 fixedUpVec;
	bool fixedUp;
	//Тип отрисовки
	TypeDraw typeDraw;
};

class FxFlowParticle: public FxParticle
{
public:
	D3DXVECTOR3 speedPos;
	D3DXQUATERNION speedRot;
	D3DXVECTOR3 speedScale;
	D3DXVECTOR3 acceleration;
};

class FxFlowEmitter: public FxEmitter
{
private:
	typedef FxEmitter _MyBase;
public:
	struct FlowDesc
	{
		FlowDesc();

		Vec3Range speedPos;
		QuatRange speedRot;
		Vec3Range speedScale;
		Vec3Range acceleration;

		D3DXVECTOR3 gravitation;		
		//Автоповорот по направлению скорости перемещения
		//true - включено
		bool autoRot;
	};
private:
	FlowDesc _flowDesc;	
protected:
	virtual FxParticle* CreateParticle();
	virtual void UpdateParticle(FxParticle* value, float dTime, bool init);

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
public:
	FxFlowEmitter(FxParticleSystem* owner);

	const FlowDesc& GetFlowDesc() const;
	void SetFlowDesc(const FlowDesc& value);	
};

class FxPhysicsParticle: public FxParticle
{
public:
	px::Actor pxActor;

	FxPhysicsParticle(): pxActor(0) {}
};

class FxPhysicsEmitter: public FxEmitter
{
public:
	typedef FxEmitter _MyBase;
private:
	px::Scene* _pxScene;
protected:
	virtual FxParticle* CreateParticle();
	virtual void UpdateParticle(FxParticle* value, float dTime, bool init);
public:
	FxPhysicsEmitter(FxParticleSystem* owner);
	virtual ~FxPhysicsEmitter();

	px::Scene* GetPxScene();
	void SetPxScene(px::Scene* value);
};

}

}

#endif
