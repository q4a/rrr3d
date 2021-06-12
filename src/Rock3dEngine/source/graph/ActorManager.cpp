#include "stdafx.h"

#include "graph\\ActorManager.h"

namespace r3d
{

namespace graph
{

ActorManager::User::User(ActorManager* owner, const UserDesc& desc): _owner(owner), _desc(desc), _octreeUser(0), _group(0), rayUser(false)
{
	LSL_ASSERT(_desc.actor && !_desc.actor->_user && desc.scenes.size() == _owner->cNumScenes);

	_desc.actor->_user = this;
}

ActorManager::User::~User()
{
	FreeOctree();
	SetGroup(0);

	_desc.actor->_user = 0;
}

void ActorManager::User::InitOctree()
{
	if (_desc.culling && !_desc.dynamic)
	{
		_octreeUser = &_owner->_octree.GetUsers().Add(GetAABB(), _desc.scenes);
		_octreeUser->AddRef();
		_octreeUser->SetData(this);				
	}
}

void ActorManager::User::FreeOctree()
{
	if (_octreeUser)
	{
		_octreeUser->Release();
		_owner->_octree.GetUsers().Delete(_octreeUser);
	}
}

void ActorManager::User::OnRebuildOctree()
{
	FreeOctree();
	InitOctree();
}

ActorManager::Group* ActorManager::User::GetGroup()
{
	return _group;
}

void ActorManager::User::SetGroup(Group* value)
{
	if (_group != value)
	{
		if (_group)
			_owner->ReleaseGroup(_group);
		_group = value;
		if (_group)
			_owner->AddRefGroup(_group);
	}
}	

AABB ActorManager::User::GetAABB() const
{
	return _desc.actor->GetWorldAABB(false);
}

Actor* ActorManager::User::GetActor()
{
	return _desc.actor;
}

const ActorManager::UserDesc& ActorManager::User::GetDesc() const
{
	return _desc;
}




ActorManager::ActorManager(unsigned sceneCnt): cNumScenes(sceneCnt), _octree(sceneCnt)
{
	_dynUserList = new UserList[cNumScenes];

	_defGroup = new AnyGroup();
	_defGroup->AddRef();
}

ActorManager::~ActorManager()
{
	ClearUserList();
	ResetCache();

	_defGroup->Release();
	delete _defGroup;

	delete[] _dynUserList;
}

ActorManager::Group* ActorManager::GetOrCreateGroup(Actor* actor)
{
	return _defGroup;
}

void ActorManager::AddRefGroup(Group* value)
{
	value->AddRef();
}

void ActorManager::ReleaseGroup(Group* value)
{
	if (value->Release() == 0)
		_groupList.Remove(value);
}

ActorManager::CameraCache::iterator ActorManager::CameraCull(const graph::CameraCI* camera)
{
	const Frustum& frustum = camera->GetFrustum();

	CameraCache::iterator iterCamera = _cameraCache.find(camera);
	bool compCull = false;
	if (iterCamera == _cameraCache.end())
	{
		iterCamera = _cameraCache.insert(iterCamera, CameraCache::value_type(camera, CacheValue()));
		compCull = true;
	}
	else 	
		compCull = iterCamera->second.idState != camera->IdState();	
	if (compCull)
	{
		iterCamera->second.pos = _octree.Culling(frustum);
		iterCamera->second.idState = camera->IdState();
	}

	return iterCamera;
}

bool ActorManager::PullInRayTargetGroup(User* user, unsigned scene, const graph::CameraCI* camera, const D3DXVECTOR3& rayTarget, float rayTargetSize)
{
	RayUsers::iterator iter = _rayUsers.find(user);
	if (iter != _rayUsers.end())
		iter->second.draw = true;

	D3DXVECTOR3 vec3;
	D3DXVec3TransformCoord(&vec3, &rayTarget, &camera->GetViewProj());
	vec3.z = 0.0f;
	D3DXVec3TransformCoord(&vec3, &vec3, &camera->GetInvViewProj());
	D3DXVECTOR3 ray = rayTarget - vec3;
	float rayLen = D3DXVec3Length(&ray);
	D3DXVec3Normalize(&ray, &ray);

	float nearDist, farDist;
	if (user->GetAABB().LineCastIntersect(vec3, ray, nearDist, farDist) && rayLen - farDist > rayTargetSize * 1.5f)
	{
		if (iter == _rayUsers.end())
		{
			iter = _rayUsers.insert(iter, RayUsers::value_type(user, RayUser()));
			iter->second.draw = true;
			user->rayUser = true;
		}

		iter->second.overloap = true;
		return true;
	}

	return false;
}

void ActorManager::RemoveRayUser(User* user)
{
	user->rayUser = false;
	_rayUsers.erase(user);
}

ActorManager::User* ActorManager::InsertActor(const UserDesc& desc)
{
	User* user = new User(this, desc);
	_userList.push_back(user);
	user->SetGroup(GetOrCreateGroup(desc.actor));

	if (desc.dynamic || !desc.culling)
	{
		for (unsigned i = 0; i < cNumScenes; ++i)
			if (desc.scenes[i])
				_dynUserList[i].push_back(user);
	}
	//
	if (IsBuildOctree())
		user->OnRebuildOctree();

	return user;
}

void ActorManager::RemoveActor(User* value)
{
	if (value->GetDesc().dynamic || !value->GetDesc().culling)
	{
		for (unsigned i = 0; i < cNumScenes; ++i)
			if (value->GetDesc().scenes[i])
				_dynUserList[i].Remove(value);
	}
	
	_userList.Remove(value);
	RemoveRayUser(value);

	delete value;
}

void ActorManager::RemoveActor(Actor* value)
{
	RemoveActor(static_cast<User*>(value->_user));
}

void ActorManager::ClearUserList()
{
	for (UserList::iterator iter = _userList.begin(); iter != _userList.end(); ++iter)
		delete (*iter);

	_userList.clear();
	_rayUsers.clear();
}

void ActorManager::GetActors(unsigned scene, ActorList& list)
{
	for (UserList::iterator iter = _userList.begin(); iter != _userList.end(); ++iter)
		if ((*iter)->GetDesc().scenes[scene])
			list.push_back((*iter)->GetActor());
}

void ActorManager::ResetCache()
{
	_cameraCache.clear();
}

void ActorManager::RebuildOctree(const AABB& worldAABB)
{
	for (UserList::iterator iter = _userList.begin(); iter != _userList.end(); ++iter)
		(*iter)->OnRebuildOctree();

	_octree.RebuildOctree(worldAABB);
}

void ActorManager::Culling(unsigned scene, const graph::CameraCI* camera, bool ignoreRayUsers, ActorList& list)
{
	LSL_ASSERT(camera);

	if (!IsBuildOctree())
	{
		GetActors(scene, list);
		return;
	}

	const Frustum& frustum = camera->GetFrustum();
	CameraCache::iterator iterCamera = CameraCull(camera);
	OctreeSort::Position& octreePos = iterCamera->second.pos;

	octreePos.BeginIterate(scene);
	graph::OctreeSort::UserNode* user = octreePos.Next(frustum);
	while (user)
	{
		User* myUser = static_cast<User*>(user->GetData());

		if (!(ignoreRayUsers && myUser->rayUser))
			list.push_back(myUser->GetActor());

		user = octreePos.Next(frustum);
	}
	octreePos.EndIterate();

	for (UserList::iterator iter = _dynUserList[scene].begin(); iter != _dynUserList[scene].end(); ++iter)
		if (!(*iter)->GetDesc().culling || frustum.ContainsAABB((*iter)->GetAABB()) != Frustum::scNoOverlap)
		{
			User* myUser = *iter;

			list.push_back(myUser->GetActor());
		}
}

bool ActorManager::IsBuildOctree() const
{
	return _octree.IsBuildOctree();
}

void ActorManager::BuildPlanar(unsigned scene)
{
	typedef graph::ActorManager::ActorList ActorList;

	_planars.clear();

	ActorList actors;
	GetActors(scene, actors);

	for (ActorList::const_iterator iter = actors.begin(); iter != actors.end(); ++iter)
	{
		graph::Actor* actor = *iter;

		GetPlanar(actor);
	}
}

const ActorManager::Planar& ActorManager::GetPlanar(Actor* actor)
{
	D3DXMATRIX mat = actor->GetInvWorldMat();
	D3DXMatrixTranspose(&mat, &mat);

	D3DXPLANE plane;
	D3DXPlaneTransform(&plane, &D3DXPLANE(actor->vec1()), &mat);
	D3DXPlaneNormalize(&plane, &plane);

	float minDist = 0;
	float minAngle = 0;
	//Planars::iterator planarIter = _planars.end();

	for (Planars::iterator iter = _planars.begin(); iter != _planars.end(); ++iter)
	{
		const D3DXPLANE& testPlane = iter->plane;
		float dist = abs(testPlane.d - plane.d);
		float angle = abs(D3DXPlaneDotNormal(&testPlane, &D3DXVECTOR3(plane)));

		if (dist < 0.5f && angle > 0.99f 
			//&& (planarIter == _planars.end() || 
			//(minDist >= dist && minAngle - angle < 0.002f) ||
			//(minDist - dist > -0.1f && minAngle <= angle))
			)
		{
			return *iter;
			//planarIter = iter;
			//minDist = dist;
			//minAngle = angle;
		}
	}

	//if (planarIter != _planars.end())
	//	return *planarIter;

	Planar planar;
	planar.id = _planars.size();
	planar.plane = plane;
	_planars.push_back(planar);

	return _planars.back();
}

void ActorManager::PullRayUsers(unsigned scene, const graph::CameraCI* camera, const D3DXVECTOR3& rayTarget, float rayTargetSize)
{
	const Frustum& frustum = camera->GetFrustum();
	CameraCache::iterator iterCamera = CameraCull(camera);
	OctreeSort::Position& octreePos = iterCamera->second.pos;

	octreePos.BeginIterate(scene);
	graph::OctreeSort::UserNode* user = octreePos.Next(frustum);
	while (user)
	{
		User* myUser = static_cast<User*>(user->GetData());

		PullInRayTargetGroup(myUser, scene, camera, rayTarget, rayTargetSize);
		user = octreePos.Next(frustum);
	}
	octreePos.EndIterate();
}

void ActorManager::Render(Engine& engine, unsigned scene, const graph::CameraCI* camera, bool ignoreRayUsers)
{
	LSL_ASSERT(camera);

	if (!IsBuildOctree())
	{
		for (UserList::iterator iter = _userList.begin(); iter != _userList.end(); ++iter)
			if ((*iter)->GetDesc().scenes[scene])
				(*iter)->GetActor()->Render(engine);
		return;
	}

	const Frustum& frustum = camera->GetFrustum();
	CameraCache::iterator iterCamera = CameraCull(camera);
	OctreeSort::Position& octreePos = iterCamera->second.pos;

	octreePos.BeginIterate(scene);
	graph::OctreeSort::UserNode* user = octreePos.Next(frustum);
	while (user)
	{
		User* myUser = static_cast<User*>(user->GetData());

		if (!(ignoreRayUsers && myUser->rayUser))
			myUser->GetActor()->Render(engine);

		user = octreePos.Next(frustum);
	}
	octreePos.EndIterate();

	for (UserList::iterator iter = _dynUserList[scene].begin(); iter != _dynUserList[scene].end(); ++iter)
		if (!(*iter)->GetDesc().culling || frustum.ContainsAABB((*iter)->GetAABB()) != Frustum::scNoOverlap)
		{
			User* myUser = *iter;

			myUser->GetActor()->Render(engine);			
		}
}

void ActorManager::Render(Engine& engine, unsigned scene, bool ignoreRayUsers)
{
	Render(engine, scene, &engine.GetContext().GetCamera(), ignoreRayUsers);
}

void ActorManager::RenderRayUsers(Engine& engine, float opacity)
{
	const float cSpeedOpacity = 0.25f;
	
	for (RayUsers::iterator iter = _rayUsers.begin(); iter != _rayUsers.end();)
	{
		User* user = iter->first;
		RayUser& rayUser = iter->second;
		Actor* actor = user->GetActor();

		if (!rayUser.draw)
		{
			user->rayUser = false;
			iter = _rayUsers.erase(iter);
			continue;
		}
		rayUser.draw = false;

		if (rayUser.overloap)			
			rayUser.time = lsl::ClampValue(rayUser.time + engine.GetDt(), 0.0f, cSpeedOpacity);
		else
			rayUser.time -= engine.GetDt();
		rayUser.overloap = false;

		if (rayUser.time < 0.0f)
		{
			actor->Render(engine);
			user->rayUser = false;
			iter = _rayUsers.erase(iter);
		}
		else
		{
			float opAnim = 1.0f - (rayUser.time / cSpeedOpacity) * (1.0f - opacity);

			engine.GetContext().SetCullOpacity(opAnim);
			actor->Render(engine);
			engine.GetContext().RestoreCullOpacity();

			++iter;
		}
	}
}

void ActorManager::RenderDebug(graph::Engine& engine)
{
	_octree.Render(engine, engine.GetContext().GetCamera().GetFrustum());

	//for (CameraCache::iterator iter = _cameraCache.begin(); iter != _cameraCache.end(); ++iter)
	//	Camera::RenderFrustum(engine, iter->first->GetInvCombTransform(graph::ctsViewProj), clrGreen);
}

const ActorManager::UserList& ActorManager::GetUserList()
{
	return _userList;
}

const AABB& ActorManager::GetWorldAABB() const
{
	return _octree.GetRootAABB();
}

}

}