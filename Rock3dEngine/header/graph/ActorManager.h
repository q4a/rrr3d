#ifndef R3D_GRAPH_NODEMANAGER
#define R3D_GRAPH_NODEMANAGER

#include "Actor.h"
#include "OctreeSort.h"

namespace r3d
{

class GraphManager;

namespace graph
{

class Actor;

class ActorManager
{
public:
	struct UserDesc
	{
		UserDesc(unsigned numScenes): scenes(numScenes)
		{
			SetDefault();
		}

		void SetDefault()
		{
			actor = 0;

			unsigned numScenes = scenes.size();
			scenes.clear();
			scenes.resize(numScenes);

			dynamic = false;			
			culling = true;
		}

		Actor* actor;
		lsl::BoolVec scenes;

		bool dynamic;
		bool culling;
	};

	typedef lsl::List<Actor*> ActorList;

	struct Planar
	{
		unsigned id;
		D3DXPLANE plane;
		//ActorList actors;
	};

	typedef std::list<Planar> Planars;
private:
	class Group: public lsl::Object
	{
	public:
		virtual void BeginDraw(graph::Engine& engine) = 0;
		virtual void EndDraw(graph::Engine& engine) = 0;
	};

	class AnyGroup: public Group
	{
	public:
		virtual void BeginDraw(graph::Engine& engine) {}
		virtual void EndDraw(graph::Engine& engine) {}
	};

	class LibMatGroup: public Group
	{
	public:
		virtual void BeginDraw(graph::Engine& engine) {}
		virtual void EndDraw(graph::Engine& engine) {}

		LibMaterial* libMat;
	};

	class User: public lsl::Object
	{
		friend ActorManager;
	private:
		ActorManager* _owner;
		UserDesc _desc;

		Group* _group;
		OctreeSort::UserNode* _octreeUser;

		User(ActorManager* owner, const UserDesc& desc);
		virtual ~User();

		void InitOctree();
		void FreeOctree();

		void OnRebuildOctree();

		Group* GetGroup();
		void SetGroup(Group* value);
	public:
		virtual AABB GetAABB() const;
		Actor* GetActor();
		const UserDesc& GetDesc() const;

		bool rayUser;
	};

	struct CacheValue
	{
		OctreeSort::Position pos;
		unsigned idState;
	};	
	typedef lsl::List<Group*> GroupList;
	typedef std::map<const CameraCI*, CacheValue> CameraCache;

	struct RayUser
	{
		RayUser(): time(0), draw(false), overloap(false) {}

		float time;
		bool draw;
		bool overloap;
	};
	typedef std::map<User*, RayUser> RayUsers;
public:
	typedef lsl::List<User*> UserList;	
private:
	//Список рендеров для актеров
	GroupList _groupList;
	//Список пользователей
	UserList _userList;
	//
	UserList* _dynUserList;
	//Octree сортировка
	OctreeSort _octree;
	//
	Planars _planars;
	
	AnyGroup* _defGroup;
	CameraCache _cameraCache;
	RayUsers _rayUsers;

	Group* GetOrCreateGroup(Actor* actor);
	void AddRefGroup(Group* value);
	void ReleaseGroup(Group* value);

	CameraCache::iterator CameraCull(const graph::CameraCI* camera);
	
	bool PullInRayTargetGroup(User* user, unsigned scene, const graph::CameraCI* camera, const D3DXVECTOR3& rayTarget, float rayTargetSize);	
	void RemoveRayUser(User* user);
public:
	ActorManager(unsigned sceneCnt);
	~ActorManager();

	User* InsertActor(const UserDesc& desc);
	void RemoveActor(User* value);
	void RemoveActor(Actor* value);
	void ClearUserList();
	void GetActors(unsigned scene, ActorList& list);

	void ResetCache();
	void RebuildOctree(const AABB& worldAABB);
	void Culling(unsigned scene, const graph::CameraCI* camera, bool ignoreRayUsers, ActorList& list);
	bool IsBuildOctree() const;

	void BuildPlanar(unsigned scene);
	const Planar& GetPlanar(Actor* actor);

	void PullRayUsers(unsigned scene, const graph::CameraCI* camera, const D3DXVECTOR3& rayTarget, float rayTargetSize);
	void Render(Engine& engine, unsigned scene, const graph::CameraCI* camera, bool ignoreRayUsers);
	void Render(Engine& engine, unsigned scene, bool ignoreRayUsers);
	void RenderRayUsers(Engine& engine, float opacity);
	void RenderDebug(graph::Engine& engine);

	const UserList& GetUserList();
	const AABB& GetWorldAABB() const;

	const unsigned cNumScenes;
};

}

}

#endif