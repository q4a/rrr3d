#ifndef PHYSX_LIBRARY
#define PHYSX_LIBRARY

#ifdef DEBUG_MEMORY	
	#pragma push_macro("new")
	#pragma push_macro("malloc")
	#pragma push_macro("free")
	
	#undef new
	#undef malloc
	#undef free
#endif

#include "PxPhysicsAPI.h"
#include "PxCooking.h"

#ifdef DEBUG_MEMORY
	#pragma pop_macro("new")
	#pragma pop_macro("malloc")
	#pragma pop_macro("free")
#endif

#include "res\\GraphResource.h"
#include "r3dMath.h"
#include "lslCollection.h"
#include "lslException.h"
#include "PhysX2Wrapper.h"

namespace physx
{

namespace r3d
{

namespace px
{

class SceneUser;

class Scene: public lsl::Component
{
	friend class Actor;
	friend class Manager;
private:
	typedef std::list<SceneUser*> UserList;

	class ContactModify: public PxContactModifyCallback //public NxUserContactModify
	{
	private:
		Scene* _scene;
	public:
		ContactModify(Scene* scene);

#if 0
		virtual bool onContactConstraint(NxU32& changeFlags, const NxShape* shape0, const NxShape* shape1, const NxU32 featureIndex0, const NxU32 featureIndex1, NxContactCallbackData& data);
#endif
		virtual void onContactModify(PxContactModifyPair* const pairs, PxU32 count);
	};

#if 0
	class ContactReport: public NxUserContactReport
	{
	private:
		Scene* _scene;
	public:
		ContactReport(Scene* scene);

		virtual void onContactNotify(NxContactPair& pair, NxU32 events);
	};
#endif

	class UserNotify: public PxSimulationEventCallback //public NxUserNotify
	{
	private:
		Scene* _scene;
	public:
		UserNotify(Scene* scene);

#if 0
		virtual bool onJointBreak(NxReal breakingImpulse, NxJoint& brokenJoint) {return true;}
		virtual void onWake(NxActor** actors, NxU32 count);
		virtual void onSleep(NxActor** actors, NxU32 count);
#endif
		// from PxSimulationEventCallback
		virtual void onConstraintBreak(PxConstraintInfo* constraints, PxU32 count);
		virtual void onWake(PxActor** actors, PxU32 count);
		virtual void onSleep(PxActor** actors, PxU32 count);
		virtual void onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs);
		virtual void onTrigger(PxTriggerPair* pairs, PxU32 count);
		virtual void onAdvance(const PxRigidBody*const* bodyBuffer, const PxTransform* poseBuffer, const PxU32 count);
	};
public:
	enum CollDisGroup {cdgDefault = 0, cdgShot = 1, cdgShotBorder = 2, cdgShotTransparency = 3, cdgWheel = 4, cdgShotTrack = 5, cdgTrackPlane = 6, cdgPlaneDeath = 7, cCollDisGroupEnd = 32};
	enum GroupMask {gmDef = 0x0, gmTemp = 0x1, cGroupMaskEnd};
#if 0
	typedef NxUserContactModify ContactModifyTraits;
	typedef ContactModifyTraits::NxContactCallbackData NxContactCallbackData;
#endif
	typedef PxContactModifyCallback ContactModifyTraits;

	struct OnContactEvent
	{
		//внешний актер, соотв индексу 1
		Actor* actor;
		unsigned actorIndex;
		
#if 0 // check ApexSceneUserNotify.h
		NxContactPair* pair;
#endif
		unsigned events;

		float deltaTime;
		D3DXVECTOR3 sumNormalForce;
		D3DXVECTOR3 sumFrictionForce;
#if 0 // check ApexSceneUserNotify.h
		NxConstContactStream stream;
#endif
	};
	struct OnContactModifyEvent
	{
		//внешний актер, соотв индексу 1
#if 0
		Actor* actor;	
		unsigned actorIndex;

		const NxShape* shape0;
		const NxShape* shape1;
		unsigned featureIndex0;
		unsigned featureIndex1;

		NxU32* changeFlags;
		NxContactCallbackData* data;
#endif
		unsigned actorIndex;

		PxContactModifyPair* pairs;
		PxU32 count;
	};

	//static const float maxTimeStep;
	//static const unsigned maxSimIter;
	static const PxVec3 cDefGravity;
	static const int cDefMatInd;

	static Actor* GetActorFromNx(PxActor* actor);
	static Actor* GetActorFromNxShape(PxShape* shape);
private:
	Manager* _manager;
	ContactModify* _contactModify;
#if 0 // check ApexSceneUserNotify.h
	ContactReport* _contactReport;
#endif
	UserNotify* _userNotify;
	PxScene* _nxScene;

	UserList _userList;
	float _lastDeltaTime;

	void CreateGroundPlane();
protected:
	Scene(Manager* manager);
	virtual ~Scene();

#if 0 // check ApexActor.h
	NxActor* CreateNxActor(const NxActorDesc& desc, Actor* actor);
	void ReleaseNxActor(NxActor* nxActor, Actor* actor);
#endif
public:
	void Compute(float deltaTime);

	void InsertUser(SceneUser* value);
	void RemoveUser(SceneUser* value);

	PxScene* GetNxScene();
};

class SceneUser: public lsl::Object
{
	friend class Scene;
private:
	Scene* _scene;
protected:
	virtual void OnContact(const Scene::OnContactEvent& contact1, const Scene::OnContactEvent& contact2) {}
public:
	SceneUser(): _scene(0) {}
};

//Необходимо разделить понятия менеджер физики(который реализует инициализацию сдк) и сцену(разделение физических пространств)
class Manager: public lsl::Component
{	
	friend PxPhysics& GetSDK();
	friend PxCooking& GetCooking();
private:
	static PxPhysics* _nxSDK;
	static PxFoundation* _nxFoundation;
	static PxCooking* _nxCooking;
	static PxDefaultErrorCallback _nxErrorCallback;
	static PxDefaultAllocator _nxAllocator;
	static PxPvd* _nxPvd;
	static PxPvdTransport* _nxTransport;

	static unsigned _sdkRefCnt;
public:
	typedef std::list<Scene*> SceneList;

	static void InitSDK();
	static void ReleaseSDK();
private:	
	SceneList _sceneList;	
public:
	Manager();
	virtual ~Manager();

	void Compute(float deltaTime);

	Scene* AddScene();
	void DelScene(Scene* value);
	void ClearSceneList();
	const SceneList& GetSceneList();

	PxPhysics& GetSDK();
	PxCooking& GetCooking();
};

class TriangleMesh: public lsl::CollectionItem
{
private:
	struct MeshVal
	{
		MeshVal(): scale(IdentityVector), id(-1), tri(0), convex(0), sumRef(0), triRef(0), convexRef(0) {}

		bool operator==(const MeshVal& value) const
		{
			D3DXVECTOR3 err = scale - value.scale;
			float maxErr = std::max(abs(err.x), std::max(abs(err.y), abs(err.z)));
			//ошибка считается исходя что 1 - один метр, следовательно 1мм допустимая ошибка
			return id == value.id && maxErr < 0.001f;
		}

		//Растяжение меша
		D3DXVECTOR3 scale;
		//ид отдельной фигуры из меша. Если < 0 то используются все фигуры меша
		int id;

		PxTriangleMesh* tri;
		PxConvexMesh* convex;
		
		unsigned sumRef;
		unsigned triRef;
		unsigned convexRef;
	};

	typedef lsl::List<MeshVal> MeshList;
private:
	::r3d::res::MeshData* _meshData;
	MeshList _meshList;

	void LoadMesh(const D3DXVECTOR3& scale, int id, PxTriangleMeshDesc& desc);
	void FreeMesh(PxTriangleMeshDesc& desc);

	MeshList::iterator GetOrCreateMesh(const D3DXVECTOR3& scale, int id);
	void ReleaseMesh(MeshList::iterator iter);
public:
	TriangleMesh();
	virtual ~TriangleMesh();

	PxTriangleMesh* GetOrCreateTri(const D3DXVECTOR3& scale, int id);
	void ReleaseTri(PxTriangleMesh* mesh);

	PxConvexMesh* GetOrCreateConvex(const D3DXVECTOR3& scale, int id);
	void ReleaseConvex(PxConvexMesh* mesh);

	::r3d::res::MeshData* GetMeshData();
	void SetMeshData(::r3d::res::MeshData* value);

	bool IsEmpty() const;
};

//class ConvexMesh

class Shapes;

enum ShapeType {stUnknown = 0, stBox, stTriangleMesh, stConvexMesh, stWheel, stPlane, stCapsule, stSphere, SHAPE_TYPEN_END, SHAPE_TYPE_FORCE = 1000};

class Shape: public lsl::CollectionItem, public lsl::Serializable
{
	friend class Actor;
public:
	static const ShapeType Type = stUnknown;
private:
	ShapeType _type;
	Shapes* _owner;
	PxShape* _nxShape;

	D3DXVECTOR3 _pos;
	D3DXQUATERNION _rot;
	D3DXVECTOR3 _scale;
	unsigned _materialIndex;
	float _density;
	float _skinWidth;
	unsigned _group;
	bool _delayInitialization;

	void SetNxShape(PxShape* value);
protected:
	void SetType(ShapeType value);
	//
#if 0
	virtual NxShapeDesc* CreateDesc() = 0;
#endif
	void ReloadNxShape(bool allowInitialization = false);

	PxVec3 TransformLocalPos(const D3DXVECTOR3& inValue);
	void SyncPos();
	void SyncRot();
	virtual void SyncScale();

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
public:
	Shape(Shapes* owner);

	//
#if 0
	void AssignFromDesc(const NxShapeDesc& desc, bool reloadShape = true);
	void AssignToDesc(NxShapeDesc& desc);
#endif

	ShapeType GetType() const;
	Shapes* GetOwner();
	Actor* GetActor();

	PxShape* GetNxShape();

	const D3DXVECTOR3& GetPos() const;
	void SetPos(const D3DXVECTOR3& value);

	const D3DXQUATERNION& GetRot() const;
	void SetRot(const D3DXQUATERNION& value);

	const D3DXVECTOR3& GetScale() const;
	void SetScale(D3DXVECTOR3& value);

	PxU16 GetMaterialIndex();
	void SetMaterialIndex(PxU16 value);

	float GetDensity() const;
	void SetDensity(float value);

	float GetSkinWidth() const;
	void SetSkinWidth(float value);

	unsigned GetGroup() const;
	void SetGroup(unsigned value);
};

class PlaneShape: public Shape
{
	typedef Shape _MyBase;
public:
	static const ShapeType Type = stPlane;
private:
	D3DXVECTOR3 _normal;
	float _dist;
protected:
#if 0
	virtual NxShapeDesc* CreateDesc();
#endif

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
public:
	PlaneShape(Shapes* owner);

#if 0
	void AssignFromDesc(const NxPlaneShapeDesc& desc, bool reloadShape = true);
	void AssignToDesc(NxPlaneShapeDesc& desc);
#endif

	PxShape* GetNxShape();

	const D3DXVECTOR3& GetNormal() const;
	void SetNormal(const D3DXVECTOR3& value);
	
	float GetDist() const;
	void SetDist(float value);
};

class BoxShape: public Shape
{
private:
	typedef Shape _MyBase;
public:
	static const ShapeType Type = stBox;
private:
	D3DXVECTOR3 _dimensions;
protected:
	//virtual NxShapeDesc* CreateDesc();

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
public:
	BoxShape(Shapes* owner);

#if 0
	void AssignFromDesc(const NxBoxShapeDesc& desc, bool reloadShape = true);
	void AssignToDesc(NxBoxShapeDesc& desc);
#endif

	PxShape* GetNxShape();

	const D3DXVECTOR3& GetDimensions() const;
	void SetDimensions(const D3DXVECTOR3& value);
};

#if 0
class SphereShape: public Shape
{
private:
	typedef Shape _MyBase;
public:
	static const ShapeType Type = stSphere;
private:
	float _radius;	
protected:
	virtual NxShapeDesc* CreateDesc();

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
public:
	SphereShape(Shapes* owner);

	void AssignFromDesc(const NxSphereShapeDesc& desc, bool reloadShape = true);
	void AssignToDesc(NxSphereShapeDesc& desc);

	NxSphereShape* GetNxShape();

	float GetRadius() const;
	void SetRadius(float value);
};

class CapsuleShape: public Shape
{
private:
	typedef Shape _MyBase;
public:
	static const ShapeType Type = stCapsule;
private:
	float _radius;
	float _height;
	unsigned _capsuleFlags;
protected:
	virtual NxShapeDesc* CreateDesc();

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
public:
	CapsuleShape(Shapes* owner);

	void AssignFromDesc(const NxCapsuleShapeDesc& desc, bool reloadShape = true);
	void AssignToDesc(NxCapsuleShapeDesc& desc);

	NxCapsuleShape* GetNxShape();

	float GetRadius() const;
	void SetRadius(float value);

	float GetHeight() const;
	void SetHeight(float value);

	unsigned GetCapsuleFlags() const;
	void SetCapsuleFlags(unsigned value);
};

class TriangleMeshShape: public Shape
{
private:
	typedef Shape _MyBase;
public:
	static const ShapeType Type = stTriangleMesh;
private:
	TriangleMesh* _mesh;
	int _meshId;
	NxTriangleMesh* _nxMesh;

	void FreeNxMesh();
protected:
	virtual NxShapeDesc* CreateDesc();
	virtual void SyncScale();

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
	virtual void OnFixUp(const FixUpNames& fixUpNames);
public:
	TriangleMeshShape(Shapes* owner);
	virtual ~TriangleMeshShape();

	void AssignFromDesc(const NxTriangleMeshShapeDesc& desc, bool reloadShape = true);
	void AssignToDesc(NxTriangleMeshShapeDesc& desc);

	NxTriangleMeshShape* GetNxShape();

	TriangleMesh* GetMesh();
	void SetMesh(TriangleMesh* value, int meshId = -1);

	int GetMeshId();
};

class ConvexShape: public Shape
{
private:
	typedef Shape _MyBase;
public:
	static const ShapeType Type = stConvexMesh;
private:
	TriangleMesh* _mesh;
	int _meshId;
	NxConvexMesh* _nxMesh;

	void FreeNxMesh();
protected:
	virtual NxShapeDesc* CreateDesc();

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
	virtual void OnFixUp(const FixUpNames& fixUpNames);
public:
	ConvexShape(Shapes* owner);
	virtual ~ConvexShape();

	void AssignFromDesc(const NxConvexShapeDesc& desc, bool reloadShape = true);
	void AssignToDesc(NxConvexShapeDesc& desc);

	NxConvexShape* GetNxShape();

	TriangleMesh* GetMesh();
	void SetMesh(TriangleMesh* value, int meshId = -1);

	int GetMeshId();
};

class WheelShape: public Shape
{
private:
	typedef Shape _MyBase;
public:
	static const ShapeType Type = stWheel;

	class ContactModify: public NxUserWheelContactModify, public lsl::Object
	{};
private:
	float _radius;
	float _suspensionTravel;
	NxSpringDesc _suspension;

	NxTireFunctionDesc _longitudalTireForceFunction;
	NxTireFunctionDesc _lateralTireForceFunction;

	float _inverseWheelMass;
	UINT _wheelFlags;
	float _motorTorque;
	float _steerAngle;
	ContactModify* _contactModify;
protected:
	virtual NxShapeDesc* CreateDesc();

	void SaveTireForceFunction(lsl::SWriter* writer, const NxTireFunctionDesc& func);
	void LoadTireForceFunction(lsl::SReader* reader, NxTireFunctionDesc& func);
	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
public:
	WheelShape(Shapes* owner);
	virtual ~WheelShape();

	void AssignFromDesc(const NxWheelShapeDesc& desc, bool reloadShape = true);
	void AssignToDesc(NxWheelShapeDesc& desc);

	NxWheelShape* GetNxShape();

	float GetRadius() const;
	void SetRadius(float value);
	
	float GetSuspensionTravel() const;
	void SetSuspensionTravel(float value);
	
	const NxSpringDesc& GetSuspension() const;
	void SetSuspension(const NxSpringDesc& value);
	
	const NxTireFunctionDesc& GetLongitudalTireForceFunction() const;
	void SetLongitudalTireForceFunction(const NxTireFunctionDesc& value);
	
	const NxTireFunctionDesc& GetLateralTireForceFunction() const;
	void SetLateralTireForceFunction(const NxTireFunctionDesc& value);	
	
	float GetInverseWheelMass() const;
	void SetInverseWheelMass(float value);
	
	UINT GetWheelFlags() const;
	void SetWheelFlags(UINT value);

	float GetMotorTorque() const;
	void SetMotorTorque(float value);

	float GetSteerAngle() const;
	void SetSteerAngle(float value);

	ContactModify* GetContactModify();
	void SetContactModify(ContactModify* value);
};
#endif

class Body: public lsl::Serializable
{
private:
	Actor* _actor;
	MrBodyDesc _desc;
protected:
	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);	
public:
	Body(Actor* actor);

	const MrBodyDesc& GetDesc();
	void SetDesc(const MrBodyDesc& value);
};

//Класс предусматривает отложенную инициализацию NxActor. Для этого следует методы изменяющие его состояние заключать в блок BeginUpdate/EndUpdate.
class ActorUser
{
public:
	virtual void OnContact(const Scene::OnContactEvent& contact) = 0;
	virtual bool OnContactModify(const Scene::OnContactModifyEvent& contact) = 0;
	virtual void OnSetBody(bool enable) {}
	virtual void OnWake() {}
	virtual void OnSleep() {}
};

class Shapes: public lsl::ComCollection<Shape, ShapeType, Shapes*, Shapes*>
{
private:
	typedef lsl::ComCollection<Shape, ShapeType, Shapes*, Shapes*> _MyBase;
public:
	typedef _MyBase::ClassList ClassList;
	static ClassList classList;
		
	static void RegisterClasses();
private:
	Actor* _owner;
protected:
	virtual void InsertItem(const Value& value);
	virtual void RemoveItem(const Value& value);
public:
	Shapes(Actor* owner);

	Actor* GetActor();
};

class Actor: public lsl::Object, public lsl::Serializable
{
	friend Scene;
	friend Shapes;
	//Некоторые методы в Shape требуют пересоздания NxShape
	friend void Shape::ReloadNxShape(bool allowInitialization);

		//Жесткая связь _parent - _child реализуется с помощью shape, поэтому координаты требуется преобразовывать вручную
#if 0
private:
	typedef NxArray<NxShapeDesc*, NxAllocatorDefault> _NxShapeDescList;
#endif
public:	
	typedef std::list<Actor*> Children;
private:
	ActorUser* _owner;
	Scene* _scene;
	Body* _body;
	Shapes* _shapes;

	Actor* _parent;
	Children _children;

#if 0
	NxActorDesc _desc;
#endif
	PxActor* _nxActor;

	//координаты кэшируется, отностиельно _nxActor
	mutable D3DXVECTOR3 _pos;
	mutable D3DXQUATERNION _rot;
	mutable D3DXVECTOR3 _scale;
protected:
	//Динамическая инициализация shape 
	void CreateNxShape(Shape* shape);
	void DestroyNxShape(Shape* shape);
	//Если nxShape создан, перезагружает его
	void ReloadNxShape(Shape* shape, bool allowInitialization);

	//Методы для статической инициализации actor. При статической инициализации происходит создание всех фигур(в том числе и дочерних) в момент создания актера
	//Заполнения списка дескрипторами
#if 0
	void FillShapeDescList(_NxShapeDescList& shapeList);
	void FillShapeDescListIncludeChildren(_NxShapeDescList& shapeList);
#endif
	//Извлечение указателей созданных фигур из актера для внутреннего списка фигур
	void UnpackActorShapeList(PxShape*const* begin, PxShape*const* end);
	unsigned UnpackActorShapeListIncludeChildren(PxShape*const* shape, unsigned numShapes, unsigned curShape);
	//Установка _nxActor для всех Actor (в том числе и дочерних). Если аргумент равен нулю то сразу происходит обнуление _nxShape для всех Shape
	void SetNxActorIncludeChildren(PxActor* value);

	//Инициализация корневого актера
	void InitRootNxActor();
	void FreeRootNxActor();
	//Инициализация подчиненного актера
	void InitChildNxActor();
	void FreeChildNxActor();
	//Общая инициализация актера
	void InitNxActor();
	void FreeNxActor();
	void ReloadNxActor();

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
	virtual void OnFixUp(const FixUpNames& fixUpNames);
public:
	Actor(ActorUser* owner);
	virtual ~Actor();

	void InsertChild(Actor* child);
	void RemoveChild(Actor* child);

	void LocalToWorldPos(const D3DXVECTOR3& inValue, D3DXVECTOR3& outValue, bool nxActorSpace = false);
	void WorldToLocalPos(const D3DXVECTOR3& inValue, D3DXVECTOR3& outValue, bool nxActorSpace = false);

#if 0
	BoxShape& AddBBShape(const AABB& aabb, const NxBoxShapeDesc& desc = NxBoxShapeDesc());
#endif

	ActorUser* GetOwner();

	PxActor* GetNxActor();
	//Менеджер, один для всей иерархии, изменение влечет также изменение в дочерних узлах
	Scene* GetScene();
	void SetScene(Scene* value);

	Actor* GetParent();
	void SetParent(Actor* value);

	Body* GetBody();
	void SetBody(const MrBodyDesc* value);
	Shapes& GetShapes();

	unsigned GetFlags() const;
	bool GetFlag(unsigned value) const;
	void SetFlags(unsigned value);
	void SetFlag(unsigned value, bool set = true);

	unsigned GetContactReportFlags() const;
	bool GetContactReportFlag(unsigned value) const;
	void SetContactReportFlags(unsigned value);
	void SetContactReportFlag(unsigned value, bool set);

	//Локальные координаты в пространстве родителя. По концепции методы возращают текущие кординаты nxActor-a, если его не существуюет то кэшированные координаты Actor-a. Упрощенная реализация, пока пододит только для двухуровненной иерархии
	const D3DXVECTOR3& GetPos() const;
	void SetPos(const D3DXVECTOR3& value);
	const D3DXQUATERNION& GetRot() const;
	void SetRot(const D3DXQUATERNION& value);
	const D3DXVECTOR3& GetScale() const;
	void SetScale(const D3DXVECTOR3& value);

	D3DXVECTOR3 GetWorldScale() const;

	bool storeCoords;
};

//
static inline PxPhysics& GetSDK();
static inline PxCooking& GetCooking();




PxPhysics& GetSDK()
{
	LSL_ASSERT(Manager::_nxSDK);

	return *Manager::_nxSDK;
}

PxCooking& GetCooking()
{
	LSL_ASSERT(Manager::_nxCooking);

	return *Manager::_nxCooking;
}

}

}

} // namespace physx

#endif
