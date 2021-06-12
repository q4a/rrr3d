#include "stdafx.h"

#include "graph\\SceneNode.h"

namespace r3d
{

namespace graph
{

const char* SceneNode::cNodeTypeStr[cNodeTypeEnd] = {"ntNode", "ntIVBMesh", "ntPlane", "ntSprite", "ntScreenSprite", "ntWaterPlane", "ntGrassField", "ntParticleSystem", "ntMovCoordSys", "ntScaleCoordSys"};

SceneNode::ClassList SceneNode::classList;




SceneNode::SceneNode()
{
	InitClassList();

	_nodes = new Nodes(this);
	_nodes->SetClassList(&classList);
	_nodes->SetClassNames(cNodeTypeStr, cNodeTypeEnd);

	_proxyList = new ProxyList(this);
}

SceneNode::~SceneNode()
{
	delete _proxyList;
	delete _nodes;
}

void SceneNode::InitClassList()
{
	static bool initClassList = false;

	if (!initClassList)
	{
		initClassList = true;

		classList.Add<SceneNode>(ntNode);
		classList.Add<IVBMeshNode>(ntIVBMesh);
		classList.Add<PlaneNode>(ntPlane);
		classList.Add<Sprite>(ntSprite);
		classList.Add<ScreenSprite>(ntScreenSprite);
		classList.Add<graph::WaterPlane>(ntWaterPlane);
		classList.Add<graph::GrassField>(ntGrassField);
		classList.Add<graph::FxParticleSystem>(ntParticleSystem);
		classList.Add<graph::MovCoordSys>(ntMovCoordSys);		
		classList.Add<graph::ScaleCoordSys>(ntScaleCoordSys);
	}
}

void SceneNode::ChildStructureChanged(BaseSceneNode* child)
{
	_MyBase::ChildStructureChanged(child);

	SetOpt(noDynStructure, child->IsNodeBBDyn());

	StructureChanged();
}

void SceneNode::DoRender(graph::Engine& engine)
{
	for (Nodes::iterator iter = _nodes->begin(); iter != _nodes->end(); ++iter)
		(*iter)->Render(engine);
}

void SceneNode::RenderProxy(graph::Engine& engine, Proxy* proxy)
{
	for (Nodes::iterator iter = _nodes->begin(); iter != _nodes->end(); ++iter)
		(*iter)->DoRender(engine);
}

AABB SceneNode::LocalDimensions() const
{
	AABB res = NullAABB;
	bool bRes = false;
	for (Nodes::iterator iter = _nodes->begin(); iter != _nodes->end(); ++iter)	
	{
		AABB aabb = (*iter)->GetLocalAABB(false);
		aabb.Transform((*iter)->GetMat());
		if (!bRes)
		{
			res = aabb;
			bRes = true;
		}
		else
			res.Add(aabb);
	}

	return res;
}

void SceneNode::Save(lsl::SWriter* writer)
{
	_MyBase::Save(writer);

	writer->WriteValue("nodes", _nodes);
}

void SceneNode::Load(lsl::SReader* reader)
{
	_MyBase::Load(reader);

	reader->ReadValue("nodes", _nodes);
}

SceneNode::Nodes& SceneNode::GetNodes()
{
	return *_nodes;
}

SceneNode::ProxyList& SceneNode::GetProxyList()
{
	return *_proxyList;
}

BaseSceneNode* SceneNode::GetNodeByTag(int tag)
{
	for (Nodes::const_iterator iter = _nodes->begin(); iter != _nodes->end(); ++iter)
		if ((*iter)->tag() == tag)
			return *iter;
	return NULL;
}

}

}