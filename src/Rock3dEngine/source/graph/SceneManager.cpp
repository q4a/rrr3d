#include "stdafx.h"

#include "graph/SceneManager.h"
#include "EulerAngles.h"
#include "lslUtility.h"
#include "r3dExceptions.h"
#include <cassert>

#include "res/GraphResource.h"

namespace r3d
{

namespace graph
{

BaseSceneNode::BaseSceneNode(): _parent(0), _proxyMaster(0), _visible(true), _time(0), _rotInvalidate(0), _rotChanged(true), _position(NullVector), _scale(IdentityVector), _direction(XVector), _up(ZVector), _rollAngle(0), _pitchAngle(0), _turnAngle(0), _rot(NullQuaternion), colorBB(clrRed), storeCoords(true), invertCullFace(false), _tag(0), _animMode(amNone), animDuration(1.0f), frame(0), speedPos(NullVector), speedScale(NullVector), speedRot(NullQuaternion), autoRot(false), _nodeDynRef(0)

#ifdef _DEBUG
		, showBB(false),
		renderBB(NULL),
#else
		, showBB(false),
#endif
		showBBIncludeChild(false)
{
	_changes.set();
	_bbChanges.set();

	_children = new Children(this);
	_proxyList = new ProxyList(this);
}

BaseSceneNode::~BaseSceneNode()
{
	LSL_ASSERT(_proxyList->Empty() && _children->Empty() && _childBBDynList.empty());

	animMode(amNone);

	//
	if (_proxyMaster)
		_proxyMaster->GetProxyList().Remove(this);

	//��� �������� ����������, � ����� SetParent, ������� ������������� ������ 0 sceneRoot
	if (_parent)
		_parent->GetChildren().Remove(this);
	ClearSceneList();
	
	delete _proxyList;
	delete _children;

#ifdef _DEBUG
	lsl::SafeDelete(renderBB);
#endif
}

void BaseSceneNode::RenderBB(graph::Engine& engine, const AABB& aabb, const glm::vec4& colorBB)
{
	using res::VertexPD;

	glm::vec4 linesColor = colorBB;

	engine.GetContext().SetRenderState(graph::rsLighting, false);

	BoundBox box(aabb);

	VertexPD lines[36];
	lines[0] = VertexPD(box.v[0], linesColor);
	lines[1] = VertexPD(box.v[1], linesColor);
	lines[2] = VertexPD(box.v[1], linesColor);
	lines[3] = VertexPD(box.v[2], linesColor);
						
	lines[4] = VertexPD(box.v[2], linesColor);
	lines[5] = VertexPD(box.v[3], linesColor);
	lines[6] = VertexPD(box.v[3], linesColor);
	lines[7] = VertexPD(box.v[0], linesColor);

	//near plane
	lines[8] = VertexPD(box.v[0], linesColor);
	lines[9] = VertexPD(box.v[4], linesColor);
	lines[10] = VertexPD(box.v[1], linesColor);
	lines[11] = VertexPD(box.v[5], linesColor);
	lines[12] = VertexPD(box.v[2], linesColor);
	lines[13] = VertexPD(box.v[6], linesColor);
	lines[14] = VertexPD(box.v[3], linesColor);
	lines[15] = VertexPD(box.v[7], linesColor);

	//far plane
	lines[16] = VertexPD(box.v[4], linesColor);
	lines[17] = VertexPD(box.v[5], linesColor);
	lines[18] = VertexPD(box.v[5], linesColor);
	lines[19] = VertexPD(box.v[6], linesColor);
	lines[20] = VertexPD(box.v[6], linesColor);
	lines[21] = VertexPD(box.v[7], linesColor);
	lines[22] = VertexPD(box.v[7], linesColor);
	lines[23] = VertexPD(box.v[4], linesColor);

	for (int i = 0; i < 6; ++i)
	{
		lines[24 + i * 2] = VertexPD(aabb.GetPlaneVert(i, 0), linesColor);
		lines[24 + i * 2 + 1] = VertexPD(aabb.GetPlaneVert(i, 1), linesColor);
	}
	

	engine.GetDriver().GetDevice()->SetFVF(VertexPD::fvf);

	do
	{
		engine.BeginDraw();
		engine.GetDriver().GetDevice()->DrawPrimitiveUP(D3DPT_LINELIST, 18, lines, sizeof(VertexPD));
	}
	while (!engine.EndDraw(true));
	engine.GetContext().RestoreRenderState(graph::rsLighting);
}

void BaseSceneNode::ExtractRotation(_RotationStyle style) const
{
	//��������, ��������� �� �� ������ ������ ������ � �����
	if (_rotInvalidate.test(style))
	{
		ApplyTransformationChanged();
		EulerAngles eulAng;
		switch (style)   
		{			
		case rsEulerAngles:	
			eulAng = Eul_FromHMatrix(_rotMat.m, EulOrdXYZs);
			_rollAngle = -eulAng.x;
			_pitchAngle = -eulAng.y;
			_turnAngle = -eulAng.z;
			break;
				 
		case rsQuaternion:
			_rot = glm::quat_cast(Matrix4DxToGlm(_rotMat));
			_rot = glm::quat(-_rot.w, _rot.x, _rot.y, _rot.z);
			break;

		case rsVectors:
			_direction = glm::vec3(_rotMat.m[0][0], _rotMat.m[0][1], _rotMat.m[0][2]);
			_up = glm::vec3(_rotMat.m[2][0], _rotMat.m[2][1], _rotMat.m[2][2]);
			break;
		}
		_rotInvalidate.reset(style);
	}
}

void BaseSceneNode::ChangedRotation(_RotationStyle style)
{
	_rotInvalidate.set();
	_rotInvalidate.reset(style);
	_rotChanged = true;
	TransformationChanged();
}

void BaseSceneNode::AddToScene(SceneManager* scene)
{
	LSL_ASSERT(std::find(_sceneList.begin(), _sceneList.end(), scene) == _sceneList.end());

	_sceneList.push_back(scene);
}

void BaseSceneNode::DeleteFromScene(SceneManager* scene)
{
	LSL_ASSERT(std::find(_sceneList.begin(), _sceneList.end(), scene) != _sceneList.end());

	_sceneList.remove(scene);
}

void BaseSceneNode::InsertChild(BaseSceneNode* value)
{
	value->_parent = this;
	value->WorldTransformationChanged();
	ChildStructureChanged(value);

	if (value->IsNodeBBDyn())
		value->SetDynBB(true);
}

void BaseSceneNode::RemoveChild(BaseSceneNode* value)
{
	if (value->IsNodeBBDyn())
		value->SetDynBB(false);

	value->_parent = 0;
	ChildStructureChanged(value);
}

void BaseSceneNode::InsertChildBBDyn(BaseSceneNode* value)
{
	LSL_ASSERT(_childBBDynList.Find(value) == _childBBDynList.end());

	_childBBDynList.push_back(value);

	value->AddRef();
}

void BaseSceneNode::RemoveChildBBDyn(BaseSceneNode* value)
{
	LSL_ASSERT(_childBBDynList.Find(value) != _childBBDynList.end());

	value->Release();

	_childBBDynList.Remove(value);
}

void BaseSceneNode::SetDynBB(bool value)
{
	LSL_ASSERT(_parent);

	if (value)
	{
		//���� ��� ��� ������������������
		if (_nodeDynRef)
			return;
		//�������������� ����
		_nodeDynRef = 1;

		_parent->InsertChildBBDyn(this);
		//���� ���� ���� �� ���� ������� �� ���� ������������ � �������� �������������
		if (_childBBDynList.size() == 1 && _parent->_parent)
			_parent->SetDynBB(value);
	}
	else
	{
		//���� ��� �� ��� ����������
		if (!_nodeDynRef)
			return;
		//����������� ����
		_nodeDynRef = 0;

		_parent->RemoveChildBBDyn(this);
		//���� ������������ �������� ������ ��� �� ���� ��������� ���� ������������
		if (_childBBDynList.size() == 0 && _parent->_parent)
			_parent->SetDynBB(value);
	}
}

void BaseSceneNode::BuildMatrix() const
{	
	//������� �� ������ �� ��������� ����������� �����������
	//���������� �� ������ �� ��������� �����������, �.�. ������� ����� ���� � ����
	_localMat = MatrixMultiply(MatrixMultiply(GetScaleMat(), GetRotMat()), GetTransMat());
	MatrixInverse(&_invLocalMat, 0, _localMat);
}

void BaseSceneNode::BuildWorldMatrix() const
{
	if (_parent)
	{
		const D3DMATRIX& mat = _parent->GetWorldMat();
		_worldMat = MatrixMultiply(_localMat, mat);
		MatrixInverse(&_invWorldMat, 0, _worldMat);
	}
	else
	{
		_worldMat = _localMat;
		_invWorldMat = _invLocalMat;
	}
}

void BaseSceneNode::ApplyTransformationChanged() const
{
	if (_changes.test(ocTransformation))
	{
		_changes.reset(ocTransformation);
		BuildMatrix();
	}
	if (_changes.test(ocWorldTransformation))
	{
		_changes.reset(ocWorldTransformation);
		BuildWorldMatrix();
	}
}

void BaseSceneNode::WorldTransformationChanged()
{	
	if (!_changes.test(ocWorldTransformation))
	{
		_changes.set(ocWorldTransformation);
		_bbChanges.set(bbcAbsStructure);
		_bbChanges.set(bbcWorldIncludeChild);
		for (Children::const_iterator iter = _children->begin(); iter != _children->end(); ++iter)
			(*iter)->WorldTransformationChanged();
	}
}

void BaseSceneNode::TransformationChanged()
{
	if (!_changes.test(ocTransformation)) 
	{
		_changes.set(ocTransformation);
		WorldTransformationChanged();
	}
	NotifyChanged(this, 0);
}

void BaseSceneNode::ChildStructureChanged(BaseSceneNode* child)
{
	if (!_bbChanges.test(bbcChild))
	{
		_bbChanges.set(bbcChild);
		_bbChanges.set(bbcIncludeChild);
		_bbChanges.set(bbcWorldIncludeChild);

		//�������� ���������� ������ ����������� �� ��������� ��������� ������
		if (_parent)
			_parent->ChildStructureChanged(this);
	}
}

AABB BaseSceneNode::LocalDimensions() const
{
	return AABB(IdentityVector);
}

void BaseSceneNode::Save(lsl::SWriter* writer)
{
	_MyBase::Save(writer);

	if (storeCoords)
	{
		writer->WriteValue("pos", glm::value_ptr(GetPos()), 3);
		writer->WriteValue("scale", glm::value_ptr(GetScale()), 3);
		writer->WriteValue("rot", glm::value_ptr(GetRot()), 4);
	}

	writer->WriteValue("invertCullFace", invertCullFace);

	if (!_sceneList.empty())
	{
		std::stringstream stream;
		for (SceneList::iterator iter = _sceneList.begin(); iter != _sceneList.end(); ++iter)
		{
			if (iter != _sceneList.begin())
				stream << " ";
			stream << (*iter)->GetComponentPath(0);
		}

		writer->WriteValue("scene", stream.str());
	}

	writer->WriteValue("tag", _tag);

	writer->WriteValue("animMode", _animMode);
	writer->WriteValue("animDuration", animDuration);
	writer->WriteValue("frame", frame);

	writer->WriteValue("speedPos", glm::value_ptr(speedPos), 3);
	writer->WriteValue("speedScale", glm::value_ptr(speedScale), 3);
	writer->WriteValue("speedRot", glm::value_ptr(speedRot), 4);

	//writer->WriteValue("options", _options.to_string());
}

void BaseSceneNode::Load(lsl::SReader* reader)
{
	_MyBase::Load(reader);

	ClearSceneList();

	if (storeCoords)
	{
		reader->ReadValue("pos", glm::value_ptr(_position), 3);
		reader->ReadValue("scale", glm::value_ptr(_scale), 3);
		reader->ReadValue("rot", glm::value_ptr(_rot), 4);
	}

	reader->ReadValue("invertCullFace", invertCullFace);

	std::string str;
	if (lsl::SReader* child = reader->ReadValue("scene", str))
	{
		std::stringstream stream(str);
		while (!stream.eof() && !stream.fail())
		{
			std::string path;
			stream >> path;
			child->AddFixUp(path, "", this, 0);
		}
	}
	ChangedRotation(rsQuaternion);

	reader->ReadValue("tag", _tag);

	int val;
	reader->ReadValue("animMode", val);
	animMode(AnimMode(val));

	reader->ReadValue("animDuration", animDuration);
	reader->ReadValue("frame", frame);

	reader->ReadValue("speedPos", glm::value_ptr(speedPos), 3);
	reader->ReadValue("speedScale", glm::value_ptr(speedScale), 3);
	reader->ReadValue("speedRot", glm::value_ptr(speedRot), 4);

	//std::string optsStr;
	//reader->ReadValue("options", optsStr);
	//_options = NodeOpts(optsStr);
}

void BaseSceneNode::OnFixUp(const FixUpNames& fixUpNames)
{
	const std::string cScene = "scene";

	for (FixUpNames::const_iterator iter = fixUpNames.begin(); iter != fixUpNames.end(); ++iter)
		if (iter->name == "scene")
			InsertToScene(iter->GetComponent<SceneManager*>());
}

void BaseSceneNode::Assign(BaseSceneNode& value)
{
	SetPos(value.GetPos());
	SetRot(value.GetRot());
	SetScale(value.GetScale());
}

void BaseSceneNode::Render(graph::Engine& engine, const D3DMATRIX& worldMat)
{
	if (_visible)
	{
		engine.GetContext().PushFrame(frame);
		engine.GetContext().SetWorldMat(worldMat);
		if (invertCullFace)
			engine.GetContext().SetInvertingCullFace(true);

		DoRender(engine);

#ifdef _DEBUG
		if (renderBB)
		{
			engine.GetContext().SetWorldMat(worldMat);
			RenderBB(engine, *renderBB, colorBB);
		}
		else
#endif
			if (showBB)
			{
				engine.GetContext().SetWorldMat(worldMat);
				RenderBB(engine, GetLocalAABB(showBBIncludeChild), colorBB);
			}

		if (invertCullFace)
			engine.GetContext().SetInvertingCullFace(false);
		engine.GetContext().PopFrame();
	}
}

void BaseSceneNode::Render(graph::Engine& engine)
{	
	Render(engine, GetWorldMat());
}

void BaseSceneNode::OnProgress(float deltaTime)
{
	if (_animMode != amNone)
	{
		_time += deltaTime;
		frame = GetFrame(_time);

		SetPos(GetPos() + speedPos * deltaTime);

		if (autoRot)
		{
			glm::vec3 dir = glm::normalize(speedPos);
			glm::quat rot;
			QuatShortestArc(XVector, dir, rot);
			SetRot(speedRot * rot);
		}
		else
		{
			glm::vec3 rotAxe = glm::axis(speedRot);
			float rotAngle = glm::angle(speedRot);
			glm::quat rotDt = glm::angleAxis(rotAngle * deltaTime, rotAxe);
			SetRot(rotDt * GetRot());
		}

		SetScale(GetScale() + speedScale * deltaTime);
	}	
}

void BaseSceneNode::StructureChanged()
{
	if (!_bbChanges.test(bbcStructure))
	{
		_bbChanges.set(bbcStructure);
		_bbChanges.set(bbcAbsStructure);
		_bbChanges.set(bbcIncludeChild);
		_bbChanges.set(bbcWorldIncludeChild);

		if (_parent)
			_parent->ChildStructureChanged(this);
	}
	NotifyChanged(this, 0);
}

void BaseSceneNode::MoveAroundTarget(const glm::vec3& worldTarget, float pitchDelta, float turnDelta)
{
	// normalT2C points away from the direction the camera is looking
	glm::vec3 originalT2C = GetWorldPos() - worldTarget;
	glm::vec3 normalT2C = originalT2C;
	float dist = glm::length(normalT2C);
	normalT2C = glm::normalize(normalT2C);
	// normalRight points to the camera's right
	// the camera is pitching around this axis.
	glm::vec3 worldUp = GetWorldUp();
	glm::vec3 normalCameraRight = glm::cross(worldUp, normalT2C);
	if (glm::length(normalCameraRight) < 0.001f)
		normalCameraRight = XVector;
	else
		normalCameraRight = glm::normalize(normalCameraRight);
	// calculate the current pitch.
	// 0 is looking down and PI is looking up
	float pitchNow = acos(glm::dot(worldUp, normalT2C));
	pitchNow = lsl::ClampValue(pitchNow + pitchDelta, 0 + 0.025f, glm::pi<float>() - 0.025f);
	// create a new vector pointing up and then rotate it down
	// into the new position
	normalT2C = GetWorldUp();
	D3DMATRIX pitchMat = MatrixRotationAxis(normalCameraRight, pitchNow);
	D3DMATRIX turnMat = MatrixRotationAxis(worldUp, -turnDelta);
	normalT2C = Vec3TransformNormal(normalT2C, pitchMat);
	normalT2C = Vec3TransformNormal(normalT2C, turnMat);
	normalT2C *= dist;
	glm::vec3 newPos = GetWorldPos() + (normalT2C - originalT2C);
	if (_parent)
		_parent->WorldToLocalCoord(newPos, newPos);
	SetPos(newPos);
}

void BaseSceneNode::AdjustDistToTarget(const glm::vec3& worldTarget, float distance)
{
	SetWorldPos(worldTarget + GetWorldDir() * (-distance));
}

void BaseSceneNode::WorldToLocal(const glm::vec4& vec, glm::vec4& out) const
{
	const D3DMATRIX& mat = GetInvWorldMat();
	out = Vec4Transform(vec, mat);
}

void BaseSceneNode::WorldToLocalCoord(const glm::vec3& vec, glm::vec3& out) const
{
	out = Vec3TransformCoord(vec, GetInvWorldMat());
}

void BaseSceneNode::WorldToLocalNorm(const glm::vec3& vec, glm::vec3& out) const
{
	out = Vec3TransformNormal(vec, GetInvWorldMat());
}

void BaseSceneNode::LocalToWorld(const glm::vec4& vec, glm::vec4& out) const
{
	out = Vec4Transform(vec, GetWorldMat());
}

void BaseSceneNode::LocalToWorldCoord(const glm::vec3& vec, glm::vec3& out) const
{
	out = Vec3TransformCoord(vec, GetWorldMat());
}

void BaseSceneNode::LocalToWorldNorm(const glm::vec3& vec, glm::vec3& out) const
{
	out = Vec3TransformNormal(vec, GetWorldMat());
}

void BaseSceneNode::ParentToLocal(const glm::vec4& vec, glm::vec4& out) const
{
	const D3DMATRIX& mat = GetInvMat();
	out = Vec4Transform(vec, mat);
}

void BaseSceneNode::LocalToParent(const glm::vec4& vec, glm::vec4& out) const
{
	const D3DMATRIX& mat = GetMat();
	out = Vec4Transform(vec, mat);
}

unsigned BaseSceneNode::RayCastIntersBB(const glm::vec3& wRayPos, const glm::vec3& wRayVec, bool includeChild) const
{
	glm::vec3 rayPos;
	WorldToLocalCoord(wRayPos, rayPos);
	glm::vec3 rayVec;
	WorldToLocalNorm(wRayVec, rayVec);

	float tNear, tFar;
	AABB aabb = GetLocalAABB(includeChild);

	return aabb.RayCastIntersect(rayPos, rayVec, tNear, tFar);
}

unsigned BaseSceneNode::RayCastIntersBB(const glm::vec3& wRayPos, const glm::vec3& wRayVec, glm::vec3& wNearVec, glm::vec3& wFarVec, bool includeChild) const
{
	glm::vec3 rayPos;
	WorldToLocalCoord(wRayPos, rayPos);
	glm::vec3 rayVec;
	WorldToLocalNorm(wRayVec, rayVec);

	AABB aabb = GetLocalAABB(includeChild);
	unsigned res = aabb.RayCastIntersect(rayPos, rayVec, wNearVec, wFarVec);
	if (res >= 1)
		LocalToWorldCoord(wNearVec, wNearVec);
	if (res >= 2)
		LocalToWorldNorm(wFarVec, wFarVec);

	return res;
}

void BaseSceneNode::InsertToScene(SceneManager* scene)
{
	LSL_ASSERT(scene);

	scene->InsertObject(this);
}

void BaseSceneNode::RemoveFromScene(SceneManager* scene)
{
	LSL_ASSERT(scene);

	scene->RemoveObject(this);
}

void BaseSceneNode::ClearSceneList()
{
	while (!_sceneList.empty())
		RemoveFromScene(*_sceneList.begin());
}

bool BaseSceneNode::IsBBDyn() const
{
	return _options[noDynStructure];
}

bool BaseSceneNode::IsNodeBBDyn() const
{
	return IsBBDyn() || !_childBBDynList.empty();
}

const BaseSceneNode::SceneList& BaseSceneNode::GetSceneList() const
{
	return _sceneList;
}

void BaseSceneNode::SetSceneList(const SceneList& value)
{
	for (SceneList::const_iterator iter = value.begin(); iter != value.end(); ++iter)
		InsertToScene(*iter);
}

void BaseSceneNode::SetScene(SceneManager* scene, bool set)
{
	LSL_ASSERT(scene);

	bool find = std::find(_sceneList.begin(), _sceneList.end(), scene) != _sceneList.end();
	if (set && !find)	
		InsertToScene(scene);
	else
		if (!set && find)
			RemoveFromScene(scene);
}

BaseSceneNode* BaseSceneNode::GetParent()
{
	return _parent;
}

const BaseSceneNode* BaseSceneNode::GetParent() const
{
	return _parent;
}

void BaseSceneNode::SetParent(BaseSceneNode* value)
{
	if (_parent != value)
	{
		if (_parent)
			_parent->_children->Remove(this);
		if (value)					
			value->_children->Insert(this);
		//���� �������� ������� �������� �� ���. � ����. ����
		//else
		//	if (_scene)
		//		_scene->GetRoot()->_children->push_back(this);
	}
}

BaseSceneNode::Children& BaseSceneNode::GetChildren()
{
	return *_children;
}

BaseSceneNode* BaseSceneNode::GetProxyMaster()
{
	return _proxyMaster;
}

BaseSceneNode::ProxyList& BaseSceneNode::GetProxyList()
{
	return *_proxyList;
}

bool BaseSceneNode::GetVisible() const
{
	return _visible;
}

void BaseSceneNode::SetVisible(bool value)
{
	_visible = value;
	for (Children::const_iterator iter = _children->begin(); iter != _children->end(); ++iter)
		(*iter)->SetVisible(value);

	//NotifyChanged(this, 0);
}

const glm::vec3& BaseSceneNode::GetPos() const
{
	return _position;
}

void BaseSceneNode::SetPos(const glm::vec3& value)
{
	_position = value;
	TransformationChanged();
}

const glm::vec3& BaseSceneNode::GetScale() const
{
	return _scale;
}

void BaseSceneNode::SetScale(const glm::vec3& value)
{
	_scale = value;
	TransformationChanged();
}

void BaseSceneNode::SetScale(float value)
{
	SetScale(glm::vec3(value, value, value));
}

const glm::vec3& BaseSceneNode::GetDir() const
{
	ExtractRotation(rsVectors);
	return _direction;
}

void BaseSceneNode::SetDir(const glm::vec3& value)
{
	ExtractRotation(rsVectors);

	if (glm::length(value) == 0)
		_direction = XVector;
	else
		_direction = glm::normalize(value);

	glm::vec3 right = GetRight();
	if (glm::length(right) < floatErrComp)
	{
		right = glm::cross(ZVector, _up);
		if (glm::length(right) < floatErrComp)
			right = glm::cross(XVector, _up);
	}
	right = glm::normalize(right);
	_up = glm::cross(_direction, right);

	ChangedRotation(rsVectors);
}

glm::vec3 BaseSceneNode::GetRight() const
{
	ExtractRotation(rsVectors);
	glm::vec3 tmp = glm::cross(_up, _direction);
	return tmp;
}

void BaseSceneNode::SetRight(const glm::vec3& value)
{
	ExtractRotation(rsVectors);

	glm::vec3 right;
	if (glm::length(value) == 0)
		right = YVector;
	else
		right = glm::normalize(value);

	_up = glm::cross(right, _direction);
	if (glm::length(_up) < floatErrComp)
	{
		_up = glm::cross(_direction, XVector);
		if (glm::length(_up) < floatErrComp)
			_up = glm::cross(_direction, YVector);
	}
	_up = glm::normalize(_up);
	_direction = glm::cross(_up, right);

	ChangedRotation(rsVectors);
}

const glm::vec3& BaseSceneNode::GetUp() const
{	
	ExtractRotation(rsVectors);
	return _up;
}

void BaseSceneNode::SetUp(const glm::vec3& value)
{
	ExtractRotation(rsVectors);

	if (glm::length(value) == 0)
		_up = ZVector;
	_up = glm::normalize(value);
	glm::vec3 right = GetRight();
	if (glm::length(right) < floatErrComp)
	{
		right = glm::cross(ZVector, _up);
		if (glm::length(right) < floatErrComp)
			right = glm::cross(XVector, _up);
	}
	right = glm::normalize(right);
	_direction = glm::cross(right, _up);

	ChangedRotation(rsVectors);
}

float BaseSceneNode::GetRollAngle() const
{	
	ExtractRotation(rsEulerAngles);
	return _rollAngle;
}

void BaseSceneNode::SetRollAngle(float value)
{		
	ExtractRotation(rsEulerAngles);
	_rollAngle = value;
	ChangedRotation(rsEulerAngles);
}

float BaseSceneNode::GetPitchAngle() const
{
	ExtractRotation(rsEulerAngles);
	return _pitchAngle;
}

void BaseSceneNode::SetPitchAngle(float value)
{			
	ExtractRotation(rsEulerAngles);
	_pitchAngle = value;
	ChangedRotation(rsEulerAngles);
}

float BaseSceneNode::GetTurnAngle() const
{
	ExtractRotation(rsEulerAngles);
	return _turnAngle;
}

void BaseSceneNode::SetTurnAngle(float value)
{
	ExtractRotation(rsEulerAngles);
	_turnAngle = value;
	ChangedRotation(rsEulerAngles);
}

const glm::quat& BaseSceneNode::GetRot() const
{
	ExtractRotation(rsQuaternion);
	return _rot;
}

void BaseSceneNode::SetRot(const glm::quat& value)
{
	_rot = value;
	ChangedRotation(rsQuaternion);
}

D3DMATRIX BaseSceneNode::GetScaleMat() const
{
	glm::vec3 vec = _scale;
	//���� ��������������� ������� ����, �� ������� ����� ��������� �����������. ������� ��������� �� ������ ����� ��������� ����� �����
	//if (glm::length(vec) < 0.0001f)
	//	vec = IdentityVector * 0.0005f;

	D3DMATRIX scaleMat = MatrixScaling(vec.x, vec.y, vec.z);
	return scaleMat;
}

D3DMATRIX BaseSceneNode::GetRotMat() const
{
	if (!_rotChanged)
		return _rotMat;

	if (!_rotInvalidate.test(rsVectors))
	{
		glm::vec3 right = GetRight();
		_rotMat._11 = _direction.x;
		_rotMat._12 = _direction.y;
		_rotMat._13 = _direction.z;
		_rotMat._14 = 0.0f;
		_rotMat._21 = right.x;
		_rotMat._22 = right.y;
		_rotMat._23 = right.z;
		_rotMat._24 = 0.0f;
		_rotMat._31 = _up.x;
		_rotMat._32 = _up.y;
		_rotMat._33 = _up.z;
		_rotMat._34 = 0.0f;
		_rotMat._41 = 0.0f;
		_rotMat._42 = 0.0f;
		_rotMat._43 = 0.0f;
		_rotMat._44 = 1.0f;
	}
	else
	{
		if (!_rotInvalidate.test(rsQuaternion))
		{
			_rotMat = Matrix4GlmToDx(glm::transpose(glm::mat4_cast(_rot)));
		}
		else
		{
			if (!_rotInvalidate.test(rsEulerAngles))
			{
				EulerAngles eulAng = Eul_(-_rollAngle, -_pitchAngle, -_turnAngle, EulOrdXYZs);
				Eul_ToHMatrix(eulAng, _rotMat.m);
				//D3DXMatrixRotationYawPitchRoll(&_rotMat, _pitchAngle, _rollAngle, _turnAngle);
			}
			else
			{
				_rotMat = IdentityMatrix;
			}
		}
	}
	
	_rotChanged = false;
	return _rotMat;
}

D3DMATRIX BaseSceneNode::GetTransMat() const
{
	return MatrixTranslation(_position.x, _position.y, _position.z);
}

const D3DMATRIX& BaseSceneNode::GetMat() const
{
	ApplyTransformationChanged();
	return _localMat;
}

void BaseSceneNode::SetLocalMat(const D3DMATRIX& value)
{
	_direction = glm::vec3(value.m[0][0], value.m[0][1], value.m[0][2]);
	glm::vec3 right = glm::vec3(value.m[1][0], value.m[1][1], value.m[1][2]);
	_up = glm::vec3(value.m[2][0], value.m[2][1], value.m[2][2]);
	_position = glm::vec3(value.m[3][0], value.m[3][1], value.m[3][2]);

	_direction = glm::normalize(_direction);
	_up = glm::normalize(_up);

	ChangedRotation(rsVectors);
	TransformationChanged();
}

const D3DMATRIX& BaseSceneNode::GetInvMat() const
{
	ApplyTransformationChanged();
	return _invLocalMat;
}

const D3DMATRIX& BaseSceneNode::GetWorldMat() const
{
	ApplyTransformationChanged();
	return _worldMat;
}

const D3DMATRIX& BaseSceneNode::GetInvWorldMat() const
{
	ApplyTransformationChanged();
	return _invWorldMat;
}

D3DMATRIX BaseSceneNode::GetCombMat(CombMatType type) const
{
	switch (type)
	{
	case cmtScaleTrans:
		return MatrixMultiply(GetScaleMat(), GetTransMat());
		
	case cmtScaleRot:
		return MatrixMultiply(GetScaleMat(), GetTransMat());
		
	case cmtRotTrans:
		return MatrixMultiply(GetRotMat(), GetTransMat());

	default:
		LSL_ASSERT(false);
		return IdentityMatrix;
	}
}

D3DMATRIX BaseSceneNode::GetWorldCombMat(CombMatType type) const
{
	switch (type)
	{
	case cmtScaleTrans:
	{
		D3DMATRIX scaleMat = GetWorldScale();
		glm::vec3 vec = GetWorldPos();
		D3DMATRIX transMat = MatrixTranslation(vec.x, vec.y, vec.z);

		return MatrixMultiply(scaleMat, transMat);
	}
		
	case cmtScaleRot:
	{
		D3DMATRIX scaleMat = GetWorldScale();

		D3DMATRIX rotMat = Matrix4GlmToDx(glm::transpose(glm::mat4_cast(GetWorldRot())));

		return MatrixMultiply(scaleMat, rotMat);
	}

	case cmtRotTrans:
	{
		D3DMATRIX rotMat = Matrix4GlmToDx(glm::transpose(glm::mat4_cast(GetWorldRot())));
		glm::vec3 pos = GetWorldPos();
		D3DMATRIX transMat = MatrixTranslation(pos.x, pos.y, pos.z);

		return MatrixMultiply(rotMat, transMat);
	}

	default:
		LSL_ASSERT(false);
		return IdentityMatrix;
	}
}

glm::vec3 BaseSceneNode::GetWorldPos() const
{
	D3DMATRIX tMat = GetWorldMat();
	return glm::vec3(tMat.m[3][0], tMat.m[3][1], tMat.m[3][2]);
}

void BaseSceneNode::SetWorldPos(const glm::vec3& value)
{
	if (_parent)
	{
		glm::vec3 pos;
		_parent->WorldToLocalCoord(value, pos);
		SetPos(pos);
	}
	else
		SetPos(value);
}

glm::quat BaseSceneNode::GetWorldRot() const
{
	glm::quat res = GetRot();
	const BaseSceneNode* curObj = this;
	while (curObj = curObj->GetParent())
		res = curObj->GetRot() * res;
	return res;
}

void BaseSceneNode::SetWorldRot(const glm::quat& value)
{
	if (_parent)
		return SetRot(_parent->GetWorldRot() * value);
	else
		return SetRot(value);
}

D3DMATRIX BaseSceneNode::GetWorldScale() const
{
	D3DMATRIX res = MatrixMultiply(IdentityMatrix, GetInvWorldMat());

	const BaseSceneNode* node = this;
	do
	{
		//��������� �������� ���������������
		res = MatrixMultiply(res, node->GetScaleMat());
		//��������� �� ������� ������������� ������
		res = MatrixMultiply(res, node->GetMat());

		node = node->GetParent();
			
	}
	while (node);

	//��������� �� �������������� �����������
	res._41 = res._42 = res._43 = 0;
	res._44 = 1.0f;

	return res;
}

glm::vec3 BaseSceneNode::GetWorldDir() const
{
	D3DMATRIX tMat = GetWorldMat();
	glm::vec3 res = glm::normalize(glm::vec3(tMat.m[0][0], tMat.m[0][1], tMat.m[0][2]));
	return res;
}

glm::vec3 BaseSceneNode::GetWorldRight() const
{
	D3DMATRIX tMat = GetWorldMat();
	return glm::vec3(tMat.m[1][0], tMat.m[1][1], tMat.m[1][2]);
}

glm::vec3 BaseSceneNode::GetWorldUp() const
{
	D3DMATRIX tMat = GetWorldMat();
	return glm::vec3(tMat.m[2][0], tMat.m[2][1], tMat.m[2][2]);
}

glm::vec3 BaseSceneNode::GetWorldSizes(bool includeChild) const
{
	const AABB& aabb = GetWorldAABB(includeChild);
	return (aabb.max - aabb.min);
}

glm::vec3 BaseSceneNode::GetWorldCenterPos(bool includeChild) const
{
	const AABB& aabb = GetWorldAABB(includeChild);
	return (aabb.min + aabb.max) / 2.0f;
}

bool AABBAreEqual(const AABB& bb1, const AABB& bb2)
{
	return glm::length(bb1.min - bb2.min) < floatErrComp && glm::length(bb1.max - bb2.max) < floatErrComp;
}

const AABB& BaseSceneNode::GetLocalAABB(bool includeChild) const
{
	if (_bbChanges.test(bbcStructure) || IsBBDyn())
	{		
		_aabbLocal = LocalDimensions();
		_bbChanges.reset(bbcStructure);
	}
	
	if (includeChild)
	{
		if (_bbChanges.test(bbcIncludeChild) || IsNodeBBDyn())
		{
			_aabbIncludingChildren = _aabbLocal;
			AABB childBB = GetAABBOfChildren();
			_aabbIncludingChildren.Add(childBB);
			_bbChanges.reset(bbcIncludeChild);
		}

		return _aabbIncludingChildren;
	}
	else
		return _aabbLocal;
}

const AABB& BaseSceneNode::GetWorldAABB(bool includeChild) const
{
	if (includeChild)
	{
		if (_bbChanges.test(bbcWorldIncludeChild) || IsBBDyn())
		{
			_bbChanges.reset(bbcWorldIncludeChild);
			AABB::Transform(GetLocalAABB(true), GetWorldMat(), _aabbWorldIncludeChild);
		}

		return _aabbWorldIncludeChild;
	}
	else
	{
		if (_bbChanges.test(bbcAbsStructure) || IsBBDyn())
		{
			_bbChanges.reset(bbcAbsStructure);
			AABB::Transform(GetLocalAABB(false), GetWorldMat(), _aabbWorld);
		}

		return _aabbWorld;
	}
}

const AABB& BaseSceneNode::GetAABBOfChildren() const
{
	if (_bbChanges.test(bbcChild))
	{
		_aabbOfChildren = NullAABB;
		for (Children::const_iterator iter = _children->begin(); iter != _children->end(); ++iter)
		{
			BaseSceneNode* child = *iter;
			if (!child->IsNodeBBDyn())
			{
				AABB childBB = child->GetLocalAABB(true);
				if (!AABBAreEqual(childBB, NullAABB))
				{
					childBB.Transform(child->GetMat());
					_aabbOfChildren.Add(childBB);
				}
			}	
		}
		_bbChanges.reset(bbcChild);
	}

	for (_ChildBBDynList::const_iterator iter = _childBBDynList.begin(); iter != _childBBDynList.end(); ++iter)
	{
		BaseSceneNode* child = *iter;
		AABB childBB = child->GetLocalAABB(true);
		if (!AABBAreEqual(childBB, NullAABB))
		{
			childBB.Transform(child->GetMat());
			_aabbOfChildren.Add(childBB);
		}
	}

	return _aabbOfChildren;
}

bool BaseSceneNode::GetOpt(NodeOpt option) const
{
	return _options[option];
}

void BaseSceneNode::SetOpt(NodeOpt option, bool value)
{
	if (_options[option] != value)
	{
		_options[option] = value;

		switch (option)
		{
		case noDynStructure:
			if (_parent)
			{
				SetDynBB(value);
				_parent->ChildStructureChanged(this);
			}
			break;
		}
	}
}

float BaseSceneNode::GetFrame(float time) const
{
	float frame;
	frame = time/std::max(animDuration, 0.001f);

	switch (_animMode)
	{
	case amNone:
		return 0;

	case amOnce:
		return std::min(frame, 1.0f);

	case amRepeat:
		return frame - static_cast<int>(frame);

	case amTile:
		return frame;

	case amTwoSide:
		frame = modf(frame, &frame);
		return (frame > 0.5f ? 1.0f - frame : frame) * 2.0f;

	case amManual:
		return time;

	case amInheritance:
		return _parent->frame;

	default:
		LSL_ASSERT(false);
	}

	return 0;
}

int BaseSceneNode::tag() const
{
	return _tag;
}

void BaseSceneNode::tag(int value)
{
	_tag = value;
}

BaseSceneNode::AnimMode BaseSceneNode::animMode() const
{
	return _animMode;
}

void BaseSceneNode::animMode(BaseSceneNode::AnimMode value)
{
	if (_animMode != value)
	{
		if (_animMode == amNone)
			RegProgress();
		else if (value == amNone)
			UnregProgress();

		_animMode = value;
	}
}

void SceneDummy::DoRender(graph::Engine& engine)
{
	//Nothing
}

Camera::Camera(): _changedCI(true)
{	
}

void Camera::RenderFrustum(graph::Engine& engine, const D3DMATRIX& invViewProj, const glm::vec4& colorBB)
{
	using res::VertexPD;

	Frustum::Corners frustumV;
	Frustum::CalculateCorners(frustumV, invViewProj);
	glm::vec4 linesColor = colorBB;

	VertexPD lines[24];
	lines[0] = VertexPD(frustumV[0], linesColor);
	lines[1] = VertexPD(frustumV[1], linesColor);
	lines[2] = VertexPD(frustumV[2], linesColor);
	lines[3] = VertexPD(frustumV[3], linesColor);
						
	lines[4] = VertexPD(frustumV[4], linesColor);
	lines[5] = VertexPD(frustumV[5], linesColor);
	lines[6] = VertexPD(frustumV[6], linesColor);
	lines[7] = VertexPD(frustumV[7], linesColor);

	//near plane
	lines[8] = VertexPD(frustumV[1], linesColor);
	lines[9] = VertexPD(frustumV[3], linesColor);
	lines[10] = VertexPD(frustumV[3], linesColor);
	lines[11] = VertexPD(frustumV[7], linesColor);
	lines[12] = VertexPD(frustumV[7], linesColor);
	lines[13] = VertexPD(frustumV[5], linesColor);
	lines[14] = VertexPD(frustumV[5], linesColor);
	lines[15] = VertexPD(frustumV[1], linesColor);

	//far plane
	lines[16] = VertexPD(frustumV[0], linesColor);
	lines[17] = VertexPD(frustumV[2], linesColor);
	lines[18] = VertexPD(frustumV[2], linesColor);
	lines[19] = VertexPD(frustumV[6], linesColor);
	lines[20] = VertexPD(frustumV[6], linesColor);
	lines[21] = VertexPD(frustumV[4], linesColor);
	lines[22] = VertexPD(frustumV[4], linesColor);
	lines[23] = VertexPD(frustumV[0], linesColor);

	engine.GetContext().SetWorldMat(IdentityMatrix);
	engine.GetContext().SetRenderState(graph::rsDiffuseMaterialSource, D3DMCS_COLOR1);
	engine.GetContext().SetRenderState(graph::rsLighting, false);
	engine.GetDriver().GetDevice()->SetFVF(VertexPD::fvf);

	engine.GetDriver().GetDevice()->DrawPrimitiveUP(D3DPT_LINELIST, 12, lines, sizeof(VertexPD));
	
	engine.GetContext().RestoreRenderState(graph::rsDiffuseMaterialSource);
	engine.GetContext().RestoreRenderState(graph::rsLighting);
}

void Camera::BuildContextInfo() const
{
	if (!_changedCI)
		return;

	_changedCI = false;

	_desc.pos = GetWorldPos();
	_desc.dir = GetWorldDir();
	_desc.up = GetWorldUp();

	_contextInfo.SetDesc(_desc);
}

void Camera::ChangedCI()
{
	_changedCI = true;
}

void Camera::WorldTransformationChanged()
{
	_MyBase::WorldTransformationChanged();

	ChangedCI();
}

void Camera::DoRender(graph::Engine& engine)
{
	RenderFrustum(engine, GetContextInfo().GetInvViewProj(), clrYellow);
}

void Camera::Apply(graph::Engine& engine)
{
	BuildContextInfo();

	engine.GetContext().ApplyCamera(&_contextInfo);
}

void Camera::UnApply(graph::Engine& engine)
{
	engine.GetContext().UnApplyCamera(&_contextInfo);
}

void Camera::AdjustNearFarPlane(const AABB& aabb, float nearDist, float farDist)
{
	BuildContextInfo();

	_contextInfo.AdjustNearFarPlane(aabb, nearDist, farDist);
	_desc.nearDist = _contextInfo.GetDesc().nearDist;
	_desc.farDist = _contextInfo.GetDesc().farDist;

	ChangedCI();
}

const graph::CameraCI& Camera::GetContextInfo() const
{
	BuildContextInfo();

	return _contextInfo;
}

float Camera::GetWidth() const
{
	return _desc.width;
}

void Camera::SetWidth(float value)
{
	if (_desc.width != value)
	{
		_desc.width = value;
		ChangedCI();
	}
}

float Camera::GetAspect() const
{
	return _desc.aspect;
}

void Camera::SetAspect(float value)
{
	if (_desc.aspect != value)
	{
		_desc.aspect = value;
		ChangedCI();
	}
}

float Camera::GetFov() const
{
	return _desc.fov;
}

void Camera::SetFov(float value)
{
	if (_desc.fov != value)
	{
		_desc.fov = value;
		ChangedCI();
	}
}

float Camera::GetNear() const
{
	return _desc.nearDist;
}

void Camera::SetNear(float value)
{
	if (_desc.nearDist != value)
	{
		_desc.nearDist = value;
		ChangedCI();
	}
}

float Camera::GetFar() const
{
	return _desc.farDist;
}

void Camera::SetFar(float value)
{
	if (_desc.farDist != value)
	{
		_desc.farDist = value;
		ChangedCI();
	}
}

CameraStyle Camera::GetStyle() const
{
	return _desc.style;
}

void Camera::SetStyle(CameraStyle value)
{
	if (_desc.style != value)
	{
		_desc.style = value;
		ChangedCI();
	}
}

LightSource::LightSource(): _changedCI(true)
{	
}

LightSource::~LightSource()
{
	SetShadowMap(0);
}

void LightSource::BuildContextInfo() const
{
	_desc.pos = GetWorldPos();
	_desc.dir = GetWorldDir();
	_desc.up = GetWorldUp();

	_contextInfo.SetDesc(_desc);

	_changedCI = false;
}

void LightSource::ChangedCI()
{
	_changedCI = true;
}

void LightSource::WorldTransformationChanged()
{
	_MyBase::WorldTransformationChanged();

	ChangedCI();
}

void LightSource::DoRender(graph::Engine& engine)
{
	Camera::RenderFrustum(engine, _contextInfo.GetCamera().GetInvViewProj(), clrYellow);
}

void LightSource::Apply(graph::Engine& engine, DWORD lightIndex)
{
	if (_changedCI)
		BuildContextInfo();

	engine.GetContext().AddLight(&_contextInfo);
	engine.GetContext().SetLightEnable(&_contextInfo, true);
}

void LightSource::UnApply(graph::Engine& engine, DWORD lightIndex)
{
	engine.GetContext().RestoreLightEnable(&_contextInfo);
	engine.GetContext().RemoveLight(&_contextInfo);
}

void LightSource::AdjustNearFarPlane(const AABB& aabb, float nearDist, float farDist)
{
	_contextInfo.AdjustNearFarPlane(aabb, nearDist, farDist);
	_desc.nearDist = _contextInfo.GetDesc().nearDist;
	_desc.range = _contextInfo.GetDesc().range;
}

const graph::LightCI& LightSource::GetContextInfo() const
{
	if (_changedCI)
		BuildContextInfo();

	return _contextInfo;
}

const glm::vec4& LightSource::GetAmbient() const
{
	return _desc.ambient;
}

void LightSource::SetAmbient(const glm::vec4& value)
{
	_desc.ambient = value;
	ChangedCI();
}

const glm::vec4& LightSource::GetDiffuse() const
{
	return _desc.diffuse;
}

void LightSource::SetDiffuse(const glm::vec4& value)
{
	_desc.diffuse = value;
	ChangedCI();
}

const glm::vec4& LightSource::GetSpecular() const
{
	return _desc.specular;
}

void LightSource::SetSpecular(const glm::vec4& value)
{
	_desc.specular = value;
	ChangedCI();
}

float LightSource::GetNear() const
{
	return _desc.nearDist;
}

void LightSource::SetNear(float value)
{
	_desc.nearDist = value;
}

float LightSource::GetFar() const
{
	return _desc.range;
}

void LightSource::SetFar(float value)
{
	_desc.range = value;
	ChangedCI();
}

D3DLIGHTTYPE LightSource::GetType() const
{
	return _desc.type;
}

void LightSource::SetType(D3DLIGHTTYPE value)
{
	_desc.type = value;
	ChangedCI();
}

float LightSource::GetFalloff() const
{
	return _desc.falloff;
}

void LightSource::SetFalloff(float value)
{
	_desc.falloff = value;
	ChangedCI();
}

float LightSource::GetPhi() const
{
	return _desc.phi;
}

void LightSource::SetPhi(float value)
{
	_desc.phi = value;
	ChangedCI();
}

float LightSource::GetTheta() const
{
	return _desc.theta;
}

void LightSource::SetTheta(float value)
{
	_desc.theta = value;
	ChangedCI();
}

graph::Tex2DResource* LightSource::GetShadowMap()
{
	return _desc.shadowMap;
}

void LightSource::SetShadowMap(graph::Tex2DResource* value)
{
	if (ReplaceRef(_desc.shadowMap, value))
	{
		_desc.shadowMap = value;
		ChangedCI();
	}
}

SceneManager::SceneManager(): _sceneRender(0)
{	
}

SceneManager::~SceneManager()
{
	LSL_ASSERT(_objects.empty());

	SetSceneRender(0);
}

void SceneManager::Render(graph::Engine& engine)
{
	//�������� ������ ������������ ����������
	if (_sceneRender)
		_sceneRender->Render(engine, this);
	//�������� ����, �� Forward ���������
	else	
		for (Objects::iterator iter = _objects.begin(); iter != _objects.end(); ++iter)
			(*iter)->Render(engine);
}

void SceneManager::InsertObject(BaseSceneNode* object)
{
	object->AddToScene(this);

	_objects.push_back(object);
}

void SceneManager::RemoveObject(BaseSceneNode* object)
{
	object->DeleteFromScene(this);

	_objects.remove(object);
}

const SceneManager::Objects& SceneManager::GetObjects() const
{
	return _objects;
}

SceneRender* SceneManager::GetSceneRender()
{
	return _sceneRender;
}

void SceneManager::SetSceneRender(SceneRender* value)
{
	if (_sceneRender != value)
	{
		if (_sceneRender)		
			_sceneRender->RemoveScene(this);
		if (value)
			value->InsertScene(this);
	}
}

SceneRender::~SceneRender()
{
	LSL_ASSERT(_sceneList.empty());
}

void SceneRender::InsertScene(SceneManager* value)
{
	LSL_ASSERT(value->_sceneRender);

	value->_sceneRender = this;
	_sceneList.push_back(value);
}

void SceneRender::RemoveScene(SceneManager* value)
{
	LSL_ASSERT(value->_sceneRender == this);

	value->_sceneRender = 0;
	_sceneList.remove(value);
}

const SceneRender::SceneList& SceneRender::GetSceneList() const
{
	return _sceneList;
}

}

}