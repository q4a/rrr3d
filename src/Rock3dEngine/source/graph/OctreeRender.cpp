#include "stdafx.h"

#include "graph\\OctreeRender.h"

namespace r3d
{

namespace graph
{

OctreeRender::OctreeRender()
{
}

OctreeRender::~OctreeRender()
{
	ClearGroupList();
}

void OctreeRender::Render(graph::Engine& engine, unsigned index)
{
	Group* group = _groupList[index];

	for (Group::const_iterator iter = group->begin(); iter != group->end(); ++iter)
		(*iter)->Render(engine);
}


OctreeRender::Group& OctreeRender::AddGroup()
{
	Group* group = new Group();
	_groupList.push_back(group);

	return *group;
}

void OctreeRender::DelGroup(unsigned index)
{
	Group* group = _groupList[index];
	_groupList.erase(std::find(_groupList.begin(), _groupList.end(), group));
	delete group;
}

void OctreeRender::ClearGroupList()
{
	for (GroupList::iterator iter = _groupList.begin(); iter != _groupList.end(); ++iter)
		delete (*iter);
	_groupList.clear();
}

void OctreeRender::RemoveScObj(SceneNode* value)
{
	for (GroupList::iterator iter = _groupList.begin(); iter != _groupList.end(); ++iter)
		if ((*iter)->Find(value) != (*iter)->end())
			(*iter)->Remove(value);
}

OctreeRender::Group& OctreeRender::GetGroup(unsigned index)
{
	return *_groupList[index];
}

const OctreeRender::GroupList& OctreeRender::GetGroupList()
{
	return _groupList;
}

}

}