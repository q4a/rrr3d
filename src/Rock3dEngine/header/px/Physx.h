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

#ifdef _WIN32
	#include "NxPhysics.h"
	#include "NxCooking.h"
#else
	#include "PxPhysicsAPI.h"
	#include "cooking/PxCooking.h"
#endif

#ifdef DEBUG_MEMORY
	#pragma pop_macro("new")
	#pragma pop_macro("malloc")
	#pragma pop_macro("free")
#endif

#include "res/GraphResource.h"
#include "r3dMath.h"
#include "lslCollection.h"
#include "lslException.h"

#include <set>
#include <vector>

namespace r3d
{

namespace px
{

#ifndef _WIN32
// PhysX 3+ scopes everything in namespace physx; the 2.8 Nx* types were
// global. Verified that nothing this project declares collides with a
// PhysX 4.1 type name, so importing the namespace here is safe.
using namespace physx;
#endif

//D3DX and PhysX vector types are layout-compatible but unrelated, so
//conversion is explicit. PhysX 2.8's NxVec3 interoperated with D3DXVECTOR3
//through get()/set(); PhysX 3+ has nothing of the sort, and the game layer
//converts at the same boundaries the engine does.
inline PxVec3 ToPx(const D3DXVECTOR3& value)
{
	return PxVec3(value.x, value.y, value.z);
}

inline D3DXVECTOR3 FromPx(const PxVec3& value)
{
	return D3DXVECTOR3(value.x, value.y, value.z);
}

inline PxQuat ToPx(const D3DXQUATERNION& value)
{
	return PxQuat(value.x, value.y, value.z, value.w);
}

inline D3DXQUATERNION FromPx(const PxQuat& value)
{
	return D3DXQUATERNION(value.x, value.y, value.z, value.w);
}

//PhysX 3+ removed NxActor's momentum accessors; there is only velocity, mass
//and the inertia tensor. These reproduce the 2.8 definitions exactly:
//
//  linear  momentum = mass * linearVelocity
//  angular momentum = worldInertiaTensor * angularVelocity
//
//The world inertia tensor is R*I*R^T, with R the rotation of the body's
//centre-of-mass frame and I the diagonal mass-space tensor PhysX stores. Note
//that angular momentum is NOT mass * angularVelocity -- treating it that way
//would be wrong for every body whose inertia is not isotropic, which is all of
//them.
//
//These sit on the multiplayer sync path, so an error here changes netplay
//behaviour rather than failing to build.
D3DXVECTOR3 GetLinearMomentum(const PxRigidDynamic& body);
void SetLinearMomentum(PxRigidDynamic& body, const D3DXVECTOR3& value);

D3DXVECTOR3 GetAngularMomentum(const PxRigidDynamic& body);
void SetAngularMomentum(PxRigidDynamic& body, const D3DXVECTOR3& value);

//NxActor::addLocalForce / addLocalTorque. PhysX 3+ has neither. Both took a
//vector in the actor's frame and applied it at the CENTRE OF MASS -- 2.8 had
//separate addLocalForceAtLocalPos for positioned forces -- so the equivalent
//is addForce/addTorque with the vector rotated into world space.
//
//Deliberately NOT PxRigidBodyExt::addLocalForceAtLocalPos with a zero
//position: that applies at the actor origin, which generates torque the 2.8
//call never produced on any body whose centre of mass is offset. Every car
//here sets one, via bfLockCenterOfMass.
//NxActor::computeKineticEnergy, which PhysX 3+ dropped. Translational plus
//rotational: 0.5*m*v^2 + 0.5*w.(I*w), with the angular term evaluated in mass
//space where the inertia tensor is diagonal.
float ComputeKineticEnergy(const PxRigidDynamic& body);

void AddLocalForce(PxRigidDynamic& body, const D3DXVECTOR3& force, PxForceMode::Enum mode);
void AddLocalTorque(PxRigidDynamic& body, const D3DXVECTOR3& torque, PxForceMode::Enum mode);

//MSVC's permissive mode lets an in-class `friend class Actor;` introduce the
//name into the enclosing namespace. Standard C++ does not, so the types these
//classes refer to before their definitions are declared here explicitly.
class SceneUser;
class Actor;
class Manager;
class Shape;
class Shapes;
class VehicleScene;

class Scene: public lsl::Component
{
	friend class Actor;
	friend class Manager;
private:
	typedef std::list<SceneUser*> UserList;

	//NxUserContactModify. The 2.8 callback returned a bool to reject a contact
	//outright; PhysX 3+ returns void and a contact is rejected by calling
	//PxContactSet::ignore() on it, which is what a false return maps to.
	class ContactModify: public PxContactModifyCallback
	{
	private:
		Scene* _scene;
	public:
		ContactModify(Scene* scene);

		virtual void onContactModify(PxContactModifyPair* const pairs, PxU32 count);
	};

	//NxScene::setActorPairFlags(a, b, NX_IGNORE_PAIR). PhysX 3+ has no pairwise
	//actor table; the equivalent is a filter callback, which is the only place
	//actor identity is available -- PxSimulationFilterShader sees filter data
	//and nothing else.
	//
	//Returning eCALLBACK for every pair would be needlessly expensive, so
	//actors that participate in an exception are marked in filter data word1
	//and the shader defers only for those.
	class PairFilter: public PxSimulationFilterCallback
	{
	private:
		Scene* _scene;
	public:
		PairFilter(Scene* scene);

		virtual PxFilterFlags pairFound(PxU32 pairID,
			PxFilterObjectAttributes attributes0, PxFilterData filterData0, const PxActor* a0, const PxShape* s0,
			PxFilterObjectAttributes attributes1, PxFilterData filterData1, const PxActor* a1, const PxShape* s1,
			PxPairFlags& pairFlags);

		virtual void pairLost(PxU32 pairID, PxFilterObjectAttributes, PxFilterData,
			PxFilterObjectAttributes, PxFilterData, bool) {}

		virtual bool statusChange(PxU32&, PxPairFlags&, PxFilterFlags&) {return false;}
	};

	//NxUserContactReport and NxUserNotify merged. PhysX 3+ takes exactly one
	//PxSimulationEventCallback per scene, so the two 2.8 interfaces cannot stay
	//separate objects.
	class SimulationEvents: public PxSimulationEventCallback
	{
	private:
		Scene* _scene;
	public:
		SimulationEvents(Scene* scene);

		virtual void onConstraintBreak(PxConstraintInfo* constraints, PxU32 count) {}
		virtual void onWake(PxActor** actors, PxU32 count);
		virtual void onSleep(PxActor** actors, PxU32 count);
		virtual void onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 count);
		virtual void onTrigger(PxTriggerPair* pairs, PxU32 count) {}
		virtual void onAdvance(const PxRigidBody*const* bodyBuffer, const PxTransform* poseBuffer, const PxU32 count) {}
	};
public:
	enum CollDisGroup {cdgDefault = 0, cdgShot = 1, cdgShotBorder = 2, cdgShotTransparency = 3, cdgWheel = 4, cdgShotTrack = 5, cdgTrackPlane = 6, cdgPlaneDeath = 7, cCollDisGroupEnd = 32};
	enum GroupMask {gmDef = 0x0, gmTemp = 0x1, cGroupMaskEnd};

	//NxActorDesc::contactReportFlags. PhysX 3+ asks for these as pair flags in
	//the filter shader instead, before any actor is consulted, so these are
	//stored and serialised but do not currently reach the simulation -- the
	//shader requests reports for every pair. See SceneFilterShader.
	enum ContactReportFlag
	{
		crfNotifyAll                 = 0xFFFF,  //NX_NOTIFY_ALL
		crfNotifyContactModification = 0x10000  //NX_NOTIFY_CONTACT_MODIFICATION
	};

	//NxShape::setGroupsMask. PhysX 3+ has no per-shape setter, but the default
	//filter shader reads the same four-word mask out of filter data words 2 and
	//3, so writing them directly keeps 2.8's per-shape granularity --
	//PxSetGroupsMask only works per actor. Words 0 and 1 are left alone; word 0
	//is the collision group.
	static void SetShapeGroupsMask(PxShape& shape, const PxGroupsMask& mask);
	static PxGroupsMask GetShapeGroupsMask(const PxShape& shape);

	//Filter data word1 bit marking an actor as having pair exceptions.
	static const PxU32 cPairExceptionBit = 0x1;

	//Contact points are pulled out of the pair one batch at a time. 2.8 handed
	//over an unbounded NxConstContactStream; this is the largest batch the
	//engine will look at in one callback.
	static constexpr unsigned cMaxContactPoints = 32;

	struct OnContactEvent
	{
		//внешний актер, соотв индексу 1
		Actor* actor;
		unsigned actorIndex;

		const PxContactPair* pair;
		unsigned events;

		float deltaTime;
		//BEHAVIOUR GAP: 2.8 reported summed normal and friction forces per pair.
		//PhysX 3+ reports a single per-point impulse that already combines both,
		//so sumNormalForce is that impulse divided by the step and
		//sumFrictionForce is left zero rather than reported wrongly.
		D3DXVECTOR3 sumNormalForce;
		D3DXVECTOR3 sumFrictionForce;
	};
	struct OnContactModifyEvent
	{
		//внешний актер, соотв индексу 1
		Actor* actor;
		unsigned actorIndex;

		const PxShape* shape0;
		const PxShape* shape1;
		//BEHAVIOUR GAP: 2.8 passed the colliding feature indices directly.
		//PhysX 3+ exposes a face index per contact point via
		//PxContactSet::getFaceIndex, so these are reported as zero.
		unsigned featureIndex0;
		unsigned featureIndex1;

		PxContactSet* contacts;
	};

	//static const float maxTimeStep;
	//static const unsigned maxSimIter;
	static const PxVec3 cDefGravity;
	static const int cDefMatInd;

	static Actor* GetActorFromNx(const PxRigidActor* actor);
	static Actor* GetActorFromNxShape(const PxShape* shape);
private:
	typedef std::set<std::pair<const PxActor*, const PxActor*> > IgnoredPairs;

	Manager* _manager;
	ContactModify* _contactModify;
	SimulationEvents* _simulationEvents;
	PairFilter* _pairFilter;
	//Ordered by pointer so lookup does not depend on which actor came first.
	IgnoredPairs _ignoredPairs;

	void MarkPairException(PxRigidActor& actor);
	//PhysX 3+ requires the application to supply the worker thread pool that
	//2.8 created internally.
	PxDefaultCpuDispatcher* _cpuDispatcher;
	PxScene* _nxScene;

	//The vehicle model. Held by pointer and declared only as a forward
	//reference so that the vehicle SDK headers stay out of this one, which
	//every translation unit that touches physics already includes.
	VehicleScene* _vehicleScene;

	//A vehicle cannot be built until its actor has its body and all its wheels,
	//and nothing signals when that is. So actors that carry WheelShapes are
	//noted here and turned into vehicles at the start of the next step.
	std::set<Actor*> _pendingVehicles;

	void UpdateVehicles(float deltaTime);

	UserList _userList;
	float _lastDeltaTime;

	void CreateGroundPlane();
protected:
	Scene(Manager* manager);
	virtual ~Scene();

	//PhysX 3+ has no actor descriptor, and static versus dynamic is fixed at
	//creation rather than switchable afterwards. `dynamic` is the same
	//discriminator the 2.8 code already used, NxActorDesc::body != 0.
	PxRigidActor* CreateNxActor(const PxTransform& pose, bool dynamic, Actor* actor);
	void ReleaseNxActor(PxRigidActor* nxActor, Actor* actor);
public:
	void Compute(float deltaTime);

	//An actor that carries WheelShapes wants a PxVehicleNoDrive built for it.
	//Registration is deferred rather than immediate because the wheels arrive
	//one at a time and the body may not be set yet.
	void NotifyVehicleActor(Actor* actor);
	void ForgetVehicleActor(Actor* actor);

	//NxScene::raycastClosestShape. PhysX 3+ reports hits through a buffer and
	//expresses the static/dynamic choice as query flags rather than an
	//NxShapesType, so the 2.8 shape-group bitfield still selects candidates but
	//goes in as filter data.
	//groupsMask is optional and matches NxScene::raycastClosestShape's last
	//argument; it goes into filter data words 2 and 3, the layout the default
	//filter shader reads. See SetShapeGroupsMask.
	PxShape* RaycastClosestShape(const D3DXVECTOR3& origin, const D3DXVECTOR3& dir,
		float maxDist, unsigned groups, PxQueryFlags queryFlags, PxRaycastHit& outHit,
		const PxGroupsMask* groupsMask = 0);

	//NxShape::getGroup. The group lives in filter data word0, which is what the
	//default filter shader reads.
	static unsigned GetShapeGroup(const PxShape& shape);

	//NxScene::setActorPairFlags(a, b, NX_IGNORE_PAIR).
	void SetActorPairIgnored(PxRigidActor& actor0, PxRigidActor& actor1, bool ignored);
	bool IsActorPairIgnored(const PxActor* actor0, const PxActor* actor1) const;

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
	static PxCooking* _nxCooking;
	//PhysX 3+ requires an explicit foundation, which owns the allocator and
	//error callback. PhysX 2.8 created these implicitly.
	static PxFoundation* _nxFoundation;
	//PhysX 2.8 gave every scene a built-in material at index 0. PhysX 3+ has no
	//material table at all -- shapes hold PxMaterial pointers -- so the default
	//is created once here with the same friction and restitution the 2.8 code
	//assigned to index 0.
	static PxMaterial* _defMaterial;
	static std::vector<PxMaterial*> _materials;
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

	static PxMaterial& GetDefaultMaterial();

	//PhysX 2.8 kept materials in a scene-wide table and shapes referenced them
	//by index. PhysX 3+ has no table -- shapes hold PxMaterial pointers -- so
	//this reinstates the index, which the content and the save format both use.
	//Index 0 is always the default material, as it was in 2.8.
	static PxU16 RegisterMaterial(PxMaterial* material);
	static PxMaterial* GetMaterialByIndex(PxU16 index);
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
	res::MeshData* _meshData;
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

	res::MeshData* GetMeshData();
	void SetMeshData(res::MeshData* value);

	bool IsEmpty() const;
};

//class ConvexMesh

class Shapes;

enum ShapeType {stUnknown = 0, stBox, stTriangleMesh, stConvexMesh, stWheel, stPlane, stCapsule, stSphere, SHAPE_TYPEN_END, SHAPE_TYPE_FORCE = 1000};

class Shape: public lsl::CollectionItem, public lsl::Serializable
{
	friend class Actor;
public:
	static constexpr ShapeType Type = stUnknown;
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
	//PhysX 3+ removed the descriptor pattern. A shape is now created from a
	//geometry, and the properties that used to live on NxShapeDesc are set on
	//the PxShape afterwards.
	virtual PxGeometryHolder CreateGeometry() = 0;
	void ReloadNxShape(bool allowInitialization = false);
	//PhysX 3+ has no per-shape dimension setters -- NxSphereShape::setRadius and
	//friends are gone. A shape's geometry is replaced wholesale instead, so
	//every setter that changes a dimension re-derives it from CreateGeometry().
	void SyncGeometry();

	D3DXVECTOR3 TransformLocalPos(const D3DXVECTOR3& inValue);
	void SyncPos();
	void SyncRot();
	virtual void SyncScale();

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
public:
	Shape(Shapes* owner);

	//
	//Applies localPose, contact offset and collision filtering to a shape
	//created from CreateGeometry().
	virtual void ApplyToShape(PxShape& shape);

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
	static constexpr ShapeType Type = stPlane;
private:
	D3DXVECTOR3 _normal;
	float _dist;

	//A PhysX 3+ plane is defined by its pose, not by its geometry.
	void SyncPlanePose();
protected:
	virtual PxGeometryHolder CreateGeometry();
	virtual void ApplyToShape(PxShape& shape);

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
public:
	PlaneShape(Shapes* owner);

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
	static constexpr ShapeType Type = stBox;
private:
	D3DXVECTOR3 _dimensions;
protected:
	virtual PxGeometryHolder CreateGeometry();

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
public:
	BoxShape(Shapes* owner);

	const D3DXVECTOR3& GetDimensions() const;
	void SetDimensions(const D3DXVECTOR3& value);
};

class SphereShape: public Shape
{
private:
	typedef Shape _MyBase;
public:
	static constexpr ShapeType Type = stSphere;
private:
	float _radius;	
protected:
	virtual PxGeometryHolder CreateGeometry();

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
public:
	SphereShape(Shapes* owner);

	float GetRadius() const;
	void SetRadius(float value);
};

class CapsuleShape: public Shape
{
private:
	typedef Shape _MyBase;
public:
	static constexpr ShapeType Type = stCapsule;
private:
	float _radius;
	float _height;
	unsigned _capsuleFlags;
protected:
	virtual PxGeometryHolder CreateGeometry();
	//PhysX capsules run along X where PhysX 2.8's ran along Y; the local
	//pose has to carry that rotation.
	virtual void ApplyToShape(PxShape& shape);

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
public:
	CapsuleShape(Shapes* owner);

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
	static constexpr ShapeType Type = stTriangleMesh;
private:
	TriangleMesh* _mesh;
	int _meshId;
	PxTriangleMesh* _nxMesh;

	void FreeNxMesh();
protected:
	virtual PxGeometryHolder CreateGeometry();
	virtual void SyncScale();

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
	virtual void OnFixUp(const FixUpNames& fixUpNames);
public:
	TriangleMeshShape(Shapes* owner);
	virtual ~TriangleMeshShape();

	TriangleMesh* GetMesh();
	void SetMesh(TriangleMesh* value, int meshId = -1);

	int GetMeshId();
};

class ConvexShape: public Shape
{
private:
	typedef Shape _MyBase;
public:
	static constexpr ShapeType Type = stConvexMesh;
private:
	TriangleMesh* _mesh;
	int _meshId;
	PxConvexMesh* _nxMesh;

	void FreeNxMesh();
protected:
	virtual PxGeometryHolder CreateGeometry();

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
	virtual void OnFixUp(const FixUpNames& fixUpNames);
public:
	ConvexShape(Shapes* owner);
	virtual ~ConvexShape();

	TriangleMesh* GetMesh();
	void SetMesh(TriangleMesh* value, int meshId = -1);

	int GetMeshId();
};

//PhysX 3+ deleted NxWheelShape and the entire raycast-wheel model with it, so
//NxSpringDesc, NxTireFunctionDesc and NxWheelShapeDesc have no successor
//types -- PxVehicleTireData parameterises grip completely differently.
//
//These are project-owned copies of the 2.8 layouts: same fields, same
//defaults, same names. That is deliberate. Every car's handling in
//Data/Car/*Wheel.txt and db.xml is written in these terms, as is the on-disk
//format of every saved game, so the migration reproduces the 2.8 curve maths
//against these values in a PxVehicleComputeTireForce shader rather than
//retuning the content by ear against a game that does not run yet.

//NxSpringDesc.
struct SpringDesc
{
	float spring;       //default 0
	float damper;       //default 0
	float targetValue;  //default 0, the suspension rest length in [0,1]

	SpringDesc();
};

//NxTireFunctionDesc. Force(slip) is a two-piece cubic Hermite spline running
//(0,0) -> (extremumSlip, extremumValue) -> (asymptoteSlip, asymptoteValue),
//with a zero tangent at both named points.
struct TireFunctionDesc
{
	float extremumSlip;     //default 1.0
	float extremumValue;    //default 0.02
	float asymptoteSlip;    //default 2.0
	float asymptoteValue;   //default 0.01
	float stiffnessFactor;  //default 1000000.0 -- quite stiff, per the SDK

	TireFunctionDesc();
};

//NxWheelShapeFlags.
enum WheelFlag
{
	wfWheelAxisContactNormal = 1 << 0,  //NX_WF_WHEEL_AXIS_CONTACT_NORMAL
	wfInputLatSlipVelocity   = 1 << 1,  //NX_WF_INPUT_LAT_SLIPVELOCITY
	wfInputLngSlipVelocity   = 1 << 2,  //NX_WF_INPUT_LNG_SLIPVELOCITY
	wfUnscaledSpringBehavior = 1 << 3,  //NX_WF_UNSCALED_SPRING_BEHAVIOR
	wfAxleSpeedOverride      = 1 << 4,  //NX_WF_AXLE_SPEED_OVERRIDE
	wfEmulateLegacyWheel     = 1 << 5,  //NX_WF_EMULATE_LEGACY_WHEEL
	wfClampedFriction        = 1 << 6   //NX_WF_CLAMPED_FRICTION
};

//NxWheelShapeDesc, minus the fields this project never read. brakeTorque is
//among them: the 2.8 descriptor carried it but WheelShape never stored it,
//and braking is applied through motorTorque instead.
struct WheelDesc
{
	float radius;            //default 1.0
	float suspensionTravel;  //default 1.0
	SpringDesc suspension;

	TireFunctionDesc longitudalTireForceFunction;
	TireFunctionDesc lateralTireForceFunction;

	float inverseWheelMass;  //default 1.0
	unsigned wheelFlags;     //default 0
	float motorTorque;       //default 0.0
	float steerAngle;        //default 0.0

	WheelDesc();
};

//NxWheelContactData, field for field. PhysX 2.8's wheel shape raycast its own
//suspension every step and reported the result through this. PxVehicle reports
//the equivalent through PxVehicleWheelQueryResult after simulation instead, so
//the struct survives as the shape of the data the game reads.
struct WheelContactData
{
	D3DXVECTOR3 contactPoint;
	D3DXVECTOR3 contactNormal;
	//The direction the wheel points, and the sideways direction at right angles
	//to it.
	D3DXVECTOR3 longitudalDirection;
	D3DXVECTOR3 lateralDirection;

	float contactForce;
	float longitudalSlip;
	float lateralSlip;
	float longitudalImpulse;
	float lateralImpulse;

	PxU16 otherShapeMaterialIndex;
	//Where on the suspension travel the wheel would rest on this contact.
	float contactPosition;

	WheelContactData();
};

class WheelShape: public Shape
{
private:
	typedef Shape _MyBase;
public:
	static constexpr ShapeType Type = stWheel;

	//NxUserWheelContactModify. PhysX 4.1 has no equivalent hook: the vehicle
	//SDK reports wheel contacts through PxVehicleWheelQueryResult *after*
	//simulation rather than letting the user rewrite them during it, so this
	//interface survives but nothing calls it yet.
	class ContactModify: public lsl::Object
	{
	public:
		virtual ~ContactModify() {}

		virtual bool onWheelContact(WheelShape* wheelShape, D3DXVECTOR3& contactPoint, D3DXVECTOR3& contactNormal,
			float& contactPosition, float& normalForce, PxShape* otherShape, PxU16& otherShapeMaterialIndex,
			PxU32 otherShapeFeatureIndex) = 0;
	};
private:
	float _radius;
	float _suspensionTravel;
	SpringDesc _suspension;

	TireFunctionDesc _longitudalTireForceFunction;
	TireFunctionDesc _lateralTireForceFunction;

	float _inverseWheelMass;
	UINT _wheelFlags;
	float _motorTorque;
	float _steerAngle;
	//Solver outputs. In 2.8 these were read live off a shape the solver
	//updated; now Vehicle::SyncOutputs writes them from the step's
	//PxVehicleWheelQueryResult, which amounts to the same freshness.
	float _axleSpeed;
	float _brakeTorque;
	//2.8 had no separate drag channel because it did not need one -- see the
	//note on SetDragTorque.
	float _dragTorque;
	WheelContactData _contact;
	PxShape* _contactShape;
	ContactModify* _contactModify;
protected:
	virtual PxGeometryHolder CreateGeometry();
	virtual void ApplyToShape(PxShape& shape);

	void SaveTireForceFunction(lsl::SWriter* writer, const TireFunctionDesc& func);
	void LoadTireForceFunction(lsl::SReader* reader, TireFunctionDesc& func);
	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
public:
	WheelShape(Shapes* owner);
	virtual ~WheelShape();

	void AssignFromDesc(const WheelDesc& desc, bool reloadShape = true);
	void AssignToDesc(WheelDesc& desc);

	float GetRadius() const;
	void SetRadius(float value);

	float GetSuspensionTravel() const;
	void SetSuspensionTravel(float value);

	const SpringDesc& GetSuspension() const;
	void SetSuspension(const SpringDesc& value);

	const TireFunctionDesc& GetLongitudalTireForceFunction() const;
	void SetLongitudalTireForceFunction(const TireFunctionDesc& value);

	const TireFunctionDesc& GetLateralTireForceFunction() const;
	void SetLateralTireForceFunction(const TireFunctionDesc& value);

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

	//NxWheelShape's runtime state, now sourced from the PxVehicleNoDrive that
	//px::Vehicle builds for this wheel's actor. Null until that vehicle exists,
	//and null while the wheel is in the air; every caller tests the return.
	PxShape* GetContact(WheelContactData& data) const;
	//Written once per step by Vehicle::SyncOutputs. Not for the game layer.
	void SetContactData(const WheelContactData& data, PxShape* contactShape);

	float GetAxleSpeed() const;
	void SetAxleSpeed(float value);

	//The driver's brake. PxVehicle treats this as a lock -- and as a declaration
	//that the car is not being accelerated -- so only real braking belongs here.
	float GetBrakeTorque() const;
	void SetBrakeTorque(float value);

	/*
	 * Idle drag: engine braking and rolling resistance, opposing whichever way
	 * the wheel turns.
	 *
	 * A channel 2.8 did not need. NxWheelShapeDesc called motorTorque the "sum
	 * engine torque on the wheel axle" and brakeTorque a torque summed against
	 * it, so a car's 400 Nm rest torque could be handed over as a brake and the
	 * arithmetic came out right. PxVehicle's brake is not a summand: it is a
	 * lock, and beyond that a mode switch. PxVehicleUpdate.cpp's updateNoDrive
	 * computes
	 *
	 *     isIntentionToAccelerate = (maxAccel > 0 && 0 == maxBrake)
	 *
	 * over *every* wheel of the vehicle, and when it is false each wheel turning
	 * slowly accumulates a timer that, after one second, activates a sticky-tire
	 * constraint holding the contact point at rest.
	 *
	 * The game brakes all four wheels and drives two, so the undriven pair kept
	 * 400 Nm at all times, every car was permanently declared to be coasting,
	 * and the constraints pinned it. A constraint is solved, not summed, so it
	 * beat the 54 kN the tire shader was computing -- and nothing measured on
	 * the force side could see it. Measured: mean speed 0.14 m/s with the brake,
	 * 14.28 m/s without.
	 *
	 * So drag travels separately and reaches PxVehicle as a negative drive
	 * torque, which is what 2.8 made of it anyway.
	 */
	float GetDragTorque() const;
	void SetDragTorque(float value);
};

//PhysX 3+ has no body descriptor -- a rigid body is configured through
//setters. This carries the five fields this project actually used from
//NxBodyDesc, so the type disappears from the game-facing headers.
//
//massLocalPose keeps D3DX's row-vector layout: rows 0-2 are the basis and
//row 3 is the translation, which matches the existing 12-float on-disk
//format exactly, so saved games and the object database stay readable.
struct BodyDesc
{
	float mass;
	unsigned flags;
	D3DXMATRIX massLocalPose;
	float sleepEnergyThreshold;
	D3DXVECTOR3 linearVelocity;

	BodyDesc();
};

//Replacements for the NX_BF_/NX_AF_ flags this project used. PhysX 3+ splits
//these across PxActorFlag and PxRigidBodyFlag, and eNO_RESPONSE has no direct
//equivalent -- it is expressed by clearing PxShapeFlag::eSIMULATION_SHAPE.
enum BodyFlag
{
	bfDisableGravity      = 1 << 0,  //NX_BF_DISABLE_GRAVITY
	bfLockCenterOfMass    = 1 << 1,  //NX_AF_LOCK_COM
	bfDisableResponse     = 1 << 2,  //NX_AF_DISABLE_RESPONSE
	bfContactModification = 1 << 3   //NX_AF_CONTACT_MODIFICATION
};

class Body: public lsl::Serializable
{
private:
	Actor* _actor;
	BodyDesc _desc;
protected:
	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);	
public:
	Body(Actor* actor);

	const BodyDesc& GetDesc();
	void SetDesc(const BodyDesc& value);
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
	//Некоторые методы в Shape требуют пересоздания PxShape
	friend void Shape::ReloadNxShape(bool allowInitialization);

		//Жесткая связь _parent - _child реализуется с помощью shape, поэтому координаты требуется преобразовывать вручную
public:
	typedef std::list<Actor*> Children;

	//What survives of NxActorDesc. Everything else it carried is now computed
	//at creation: the pose from _pos/_rot, the body from _body, and the shapes
	//by attaching them to the actor rather than by describing them up front.
	//Kept as a struct because both fields are serialised by name.
	struct Desc
	{
		unsigned flags;
		unsigned contactReportFlags;

		Desc();
	};
private:
	ActorUser* _owner;
	Scene* _scene;
	Body* _body;
	Shapes* _shapes;

	Actor* _parent;
	Children _children;

	Desc _desc;
	PxRigidActor* _nxActor;

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

	//PhysX 3+ attaches shapes to an actor that already exists, so the descriptor
	//list the 2.8 code built up front -- and then unpacked back into the Shape
	//objects afterwards -- is replaced by creating each shape directly.
	unsigned CountShapesIncludeChildren() const;
	void CreateNxShapesIncludeChildren();
	//Установка _nxActor для всех Actor (в том числе и дочерних). Если аргумент равен нулю то сразу происходит обнуление _nxShape для всех Shape
	void SetNxActorIncludeChildren(PxRigidActor* value);
	//Applies BodyDesc to a freshly created PxRigidDynamic. 2.8 passed these
	//through NxActorDesc::body at creation time.
	void ApplyBodyDesc();

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

	BoxShape& AddBBShape(const AABB& aabb);

	ActorUser* GetOwner();

	PxRigidActor* GetNxActor();
	//A dynamic actor, or null when this actor is static. Callers that need
	//mass, velocity or forces want this rather than GetNxActor().
	PxRigidDynamic* GetNxDynamic();
	//Менеджер, один для всей иерархии, изменение влечет также изменение в дочерних узлах
	Scene* GetScene();
	void SetScene(Scene* value);

	Actor* GetParent();
	void SetParent(Actor* value);
	//A car's wheels each live on their own child actor, so anything that works
	//on a whole vehicle has to walk these.
	const Children& GetChildren() const;

	Body* GetBody();
	void SetBody(const BodyDesc* value);
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
inline PxPhysics& GetSDK();
inline PxCooking& GetCooking();




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

#endif