#ifndef R3D_GRAPH_OCTREERENDER
#define R3D_GRAPH_OCTREERENDER

#include "SceneNode.h"

namespace r3d
{

namespace graph
{

class OctreeRender: public lsl::Component
{
public:
	typedef lsl::Container<SceneNode*> Group;
	typedef std::vector<Group*> GroupList;
private:
	GroupList _groupList;
public:
	OctreeRender();
	virtual ~OctreeRender();

	void Render(graph::Engine& engine, unsigned index);

	Group& AddGroup();
	void DelGroup(unsigned index);
	void ClearGroupList();

	void RemoveScObj(SceneNode* value);

	Group& GetGroup(unsigned index);
	const GroupList& GetGroupList();
};

}

}

#endif