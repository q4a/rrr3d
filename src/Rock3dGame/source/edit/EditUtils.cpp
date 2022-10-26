#include "stdafx.h"
#include "game\World.h"
#include "edit\Utils.h"

#include "edit\Map.h"

namespace r3d
{

namespace edit
{

ScNodeCont::ScNodeCont(graph::BaseSceneNode* node): _node(node)
{
	LSL_ASSERT(_node);

	_node->AddRef();
}

ScNodeCont::~ScNodeCont()
{
	_node->Release();
}

void ScNodeCont::Reset(graph::BaseSceneNode* node)
{
	if (ReplaceRef(_node, node))
		_node = node;

	LSL_ASSERT(_node);
}

bool ScNodeCont::RayCastInters(const glm::vec3& rayPos, const glm::vec3& rayVec) const
{
	return _node->RayCastIntersBB(rayPos, rayVec, true) > 0;
}

bool ScNodeCont::Compare(const IMapObjRef& node) const
{
	return &node->GetImpl<MapObj>()->GetInst()->GetGameObj().GetGrActor() == _node;
}

glm::vec3 ScNodeCont::GetPos() const
{
	return _node->GetWorldPos();
}

void ScNodeCont::SetPos(const glm::vec3& value)
{
	_node->SetWorldPos(value);
}

glm::quat ScNodeCont::GetRot() const
{
	return _node->GetWorldRot();
}

void ScNodeCont::SetRot(const glm::quat& value)
{
	_node->SetWorldRot(value);
}

glm::vec3 ScNodeCont::GetScale() const
{
	return _node->GetScale();
}

void ScNodeCont::SetScale(const glm::vec3& value)
{
	_node->SetScale(value);
}

glm::vec3 ScNodeCont::GetDir() const
{
	return _node->GetWorldDir();
}

glm::vec3 ScNodeCont::GetRight() const
{
	return _node->GetWorldRight();
}

glm::vec3 ScNodeCont::GetUp() const
{
	return _node->GetWorldUp();
}

D3DXMATRIX ScNodeCont::GetMat() const
{
	return _node->GetWorldMat();
}

AABB ScNodeCont::GetAABB() const
{
	return _node->GetLocalAABB(true);
}

graph::BaseSceneNode* ScNodeCont::GetNode()
{
	return _node;
}

}

}