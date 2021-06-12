#include "stdafx.h"

#include "graph\\OctreeSort.h"

namespace r3d
{

namespace graph
{

const unsigned OctreeSort::cMinNodeUserCnt = 1;
const unsigned OctreeSort::cMinLeafSize = 100;




OctreeSort::Node::Node(unsigned numGroups, const AABB& aabb, Node* parent): cNumGroups(numGroups), _aabb(aabb), _parent(parent), _leaf(0), _userCnt(0), _refCnt(0), _lockCnt(0)
{
	_leaf = new Leaf[cNumGroups];
	_userMap = new UserMap[cNumGroups];
}

OctreeSort::Node::~Node() 
{
	LSL_ASSERT(_refCnt == 0 && _lockCnt == 0);

	ClearUser();
	ClearChildNode();

#ifdef _DEBUG
	for (unsigned i = 0; i < cNumGroups; ++i)
		LSL_ASSERT(_leaf[i].empty() && _userMap[i].empty());
#endif

	delete[] _userMap;
	delete[] _leaf;
}

void OctreeSort::Node::InsertToUserMap(UserNode* value, unsigned group)
{
	LSL_ASSERT(_lockCnt == 0);

	++_userMap[group][value];

	if (_parent)
		_parent->InsertToUserMap(value, group);
}

void OctreeSort::Node::RemoveFromUserMap(UserNode* value, unsigned group)
{
	LSL_ASSERT(_lockCnt == 0);

	UserMap::iterator iter = _userMap[group].find(value);
	if (--(iter->second) == 0)
		_userMap[group].erase(iter);

	if (_parent)
		_parent->RemoveFromUserMap(value, group);
}

void OctreeSort::Node::DoInsertUser(UserNode* value, unsigned group)
{
	_leaf[group].push_back(value);
	InsertToUserMap(value, group);

	++_userCnt;
}

void OctreeSort::Node::DoRemoveUser(UserNode* value, unsigned group)
{
	_leaf[group].Remove(value);	
	RemoveFromUserMap(value, group);

	--_userCnt;
}

void OctreeSort::Node::InsertUser(UserNode* value, unsigned group)
{
	LSL_ASSERT(_nodeList.empty() && !_leaf[group].IsFind(value));

	DoInsertUser(value, group);
	value->DoInsertToNode(this);
}

void OctreeSort::Node::InsertUser(const UserList& value)
{
	LSL_ASSERT(_nodeList.empty());

	for (UserList::const_iterator iter = value.begin(); iter != value.end(); ++iter)	
		for (unsigned i = 0; i < cNumGroups; ++i)
			if ((*iter)->GetGroup(i))
				InsertUser(*iter, i);
}

void OctreeSort::Node::RemoveUser(UserNode* value, unsigned group)
{
	LSL_ASSERT(_leaf[group].IsFind(value));

	DoRemoveUser(value, group);
	value->DoRemoveFromNode(this);
}

void OctreeSort::Node::ClearUser(unsigned group)
{
	for (UserList::iterator iter = _leaf[group].begin(); iter != _leaf[group].end(); ++iter)
	{
		(*iter)->DoRemoveFromNode(this);
		for (unsigned i = 0; i < cNumGroups; ++i)
			RemoveFromUserMap(*iter, i);
	}

	if (!_leaf[group].empty())
	{
		_leaf[group].clear();	
	}
}

void OctreeSort::Node::ClearUser()
{
	for (unsigned i = 0; i < cNumGroups; ++i)
		ClearUser(i);
}

OctreeSort::Node& OctreeSort::Node::AddNode(const AABB& aabb)
{
	Node* res = new Node(cNumGroups, aabb, this);
	_nodeList.push_back(res);	

	return *res;
}

void OctreeSort::Node::DelNode(Node* value)
{
	LSL_ASSERT(value->_parent == this);

	_nodeList.Remove(value);
	delete value;
}

void OctreeSort::Node::ClearChildNode()
{
	for (NodeList::iterator iter = _nodeList.begin(); iter != _nodeList.end(); ++iter)
		delete (*iter);

	_nodeList.clear();
}

void OctreeSort::Node::AddRef()
{
	++_refCnt;
}

void OctreeSort::Node::Release()
{
	LSL_ASSERT(_refCnt > 0);

	--_refCnt;
}

void OctreeSort::Node::Lock()
{
	++_lockCnt;
}

void OctreeSort::Node::Unlock()
{
	LSL_ASSERT(_lockCnt > 0);

	--_lockCnt;
}

const AABB& OctreeSort::Node::GetAABB() const
{
	return _aabb;
}

const OctreeSort::Leaf& OctreeSort::Node::GetLeaf(unsigned group) const
{
	return _leaf[group];
}

const OctreeSort::UserMap& OctreeSort::Node::GetUserMap(unsigned group) const
{
	return _userMap[group];
}

const OctreeSort::NodeList& OctreeSort::Node::GetNodeList() const
{
	return _nodeList;
}

unsigned OctreeSort::Node::GetUserCnt() const
{
	return _userCnt;
}




OctreeSort::UserNode::UserNode(OctreeSort* owner, const AABB& aabb, const lsl::BoolVec& groups): _owner(owner), _aabb(aabb), _groups(groups), _idPass(0), _data(0)
{
}

OctreeSort::UserNode::~UserNode()
{
	SetData(0);

	RemoveFromAllNodes();
}

void OctreeSort::UserNode::DoInsertToNode(const NodeMap::iterator& iter)
{
	if (iter->second == 0)	
		iter->first->AddRef();
	++(iter->second);
}

OctreeSort::UserNode::NodeMap::iterator OctreeSort::UserNode::DoInsertToNode(Node* value)
{
	NodeMap::iterator res = _nodeMap.insert(_nodeMap.end(), NodeMap::value_type(value, 0));
	DoInsertToNode(res);
	return res;
}

OctreeSort::UserNode::NodeMap::iterator OctreeSort::UserNode::DoRemoveFromNode(const NodeMap::iterator& iter)
{
	if (--iter->second == 0)
	{
		iter->first->Release();
		return _nodeMap.erase(iter);
	}

	NodeMap::iterator res = iter;
	return ++res;
}

OctreeSort::UserNode::NodeMap::iterator OctreeSort::UserNode::DoRemoveFromNode(Node* value)
{
	return DoRemoveFromNode(_nodeMap.find(value));
}

void OctreeSort::UserNode::InsertToNode(Node* value)
{
	LSL_ASSERT(_nodeMap.find(value) == _nodeMap.end());

	NodeMap::iterator iter = _nodeMap.insert(_nodeMap.end(), NodeMap::value_type(value, 0));

	for (unsigned i = 0; i < _owner->cNumGroups; ++i)
		if (_groups[i])
		{
			DoInsertToNode(iter);
			value->DoInsertUser(this, i);
		}
}

OctreeSort::UserNode::NodeMap::iterator OctreeSort::UserNode::RemoveFromNode(const NodeMap::iterator& iter)
{
	LSL_ASSERT(iter != _nodeMap.end());

	NodeMap::iterator res = iter;
	++res;

	for (unsigned i = 0; i < _owner->cNumGroups; ++i)
		if (_groups[i])
		{
			iter->first->DoRemoveUser(this, i);
			res = DoRemoveFromNode(iter);	
		}

	return res;
}

OctreeSort::UserNode::NodeMap::iterator OctreeSort::UserNode::RemoveFromNode(Node* value)
{
	return RemoveFromNode(_nodeMap.find(value));
}

void OctreeSort::UserNode::RemoveFromAllNodes()
{
	for (NodeMap::iterator iter = _nodeMap.begin(); iter != _nodeMap.end();)
		iter = RemoveFromNode(iter);
}

/*OctreeSort::UserNode::NodeMap::iterator OctreeSort::UserNode::SetGroup(const NodeMap::iterator& iter, unsigned group, bool value)
{
	NodeMap::iterator res = iter;

	if (value)
	{
		iter->first->DoRemoveUser(this, group);
		res = DoRemoveFromNode(iter);
		return res;
	}
	else
	{
		iter->first->DoInsertUser(this, group);
		DoInsertToNode(iter);
		return ++res;
	}

	return ++res;
}*/

/*void OctreeSort::UserNode::MoveTo(const AABB& aabb, Node* node)
{
	if (node->GetNodeList().empty())
	{
		InsertToNode(node);
		return;
	}

	for (NodeList::iterator iter = node->GetNodeList().begin(); iter != node->GetNodeList().end(); ++iter)
		if ((*iter)->GetAABB().ContainsAABB(aabb) != AABB::spNoOverloap)
			MoveTo(*iter);
}

void OctreeSort::UserNode::Move(const AABB& aabb)
{
	LSL_ASSERT(!_nodeMap.empty());

	for (NodeList::iterator iter = node->GetNodeList().begin(); iter != node->GetNodeList().end(); ++iter)
		if (node->GetAABB().ContainsAABB(aabb) == AABB::spNoOverloap)


	Node* node = _nodeMap.begin()->first;

	RemoveFromAllNodes();

	while (node)
		if (node->GetAABB().ContainsAABB(aabb) == AABB::spNoOverloap)
		{
			while (node->GetParent())
		}			
	}
}*/

OctreeSort* OctreeSort::UserNode::GetOwner()
{
	return _owner;
}

const AABB& OctreeSort::UserNode::GetAABB() const
{
	return _aabb;
}

bool OctreeSort::UserNode::GetGroup(unsigned group) const
{
	return _groups.at(group);
}

/*void OctreeSort::UserNode::SetGroup(unsigned group, bool value)
{
	_groups.at(group);

	for (NodeMap::iterator iter = _nodeMap.begin(); iter != _nodeMap.end();)
		if (_groups[group] != value)		
			SetGroup(iter, group, value);


	_groups[group] = value;
}*/

const lsl::BoolVec& OctreeSort::UserNode::GetGroups() const
{
	return _groups;
}

/*void OctreeSort::UserNode::SetGroups(const lsl::BoolVec& value)
{
	LSL_ASSERT(_owner->cNumGroups == value.size());

	for (NodeMap::iterator iter = _nodeMap.begin(); iter != _nodeMap.end();)
		for (unsigned i = 0; i < value.size(); ++i)
			if (_groups[i] != value[i])			
				iter = SetGroup(iter, i, value[i]);

	_groups = value;
}*/

lsl::Object* OctreeSort::UserNode::GetData()
{
	return _data;
}

void OctreeSort::UserNode::SetData(lsl::Object* value)
{
	if (ReplaceRef(_data, value))
		_data = value;
}




OctreeSort::OctreeSort(unsigned numGroups): cNumGroups(numGroups), _root(0), _idPass(0)
{
	_users = new Users(this);	

	int numNode = 0;
	for (float i =-1; i < 2; i += 2.0f)
		for (float j =-1; j < 2; j += 2.0f)
			for (float k =-1; k < 2; k += 2.0f, ++numNode)			
				_nodeOff[numNode] = D3DXVECTOR3(i, j, k);
}

OctreeSort::~OctreeSort()
{
	DestroyOctree();

	delete _users;
}

void OctreeSort::BuildOctree(Node& node, const UserList& userList)
{
	unsigned userCnt = userList.size();
	float size = node.GetAABB().GetDiameter();

	//Условия создания листа
	if (userCnt <= cMinNodeUserCnt || size <= cMinLeafSize)
	{
		node.InsertUser(userList);
		return;
	}

	D3DXVECTOR3 nodeSize = node.GetAABB().GetSizes() / 2.0f;
	for (int i = 0; i < cOctNum; ++i)
	{
		AABB aabb(nodeSize);
		aabb.Offset(node.GetAABB().GetCenter() + _nodeOff[i] * nodeSize/2);

		UserList list;
		//Ищем подходящих пользователей
		for (UserList::const_iterator iter = userList.begin(); iter != userList.end(); ++iter)
			if (aabb.ContainsAABB((*iter)->GetAABB()) != AABB::scNoOverlap)
				list.push_back(*iter);

		//Если нашли, следовательно добавляем новый узел
		if (!list.empty())
		{
			Node& child = node.AddNode(aabb);
			BuildOctree(child, list);
		}
	}
}

bool OctreeSort::IsBuildOctree() const
{
	return _root != 0;
}

void OctreeSort::InsertUser(UserNode* user)
{	
}

void OctreeSort::RemoveUser(UserNode* user)
{	
}

bool OctreeSort::DoCulling(const Frustum& frustum, Frustum::SpaceContains spaceCont, Node& node, Position& pos)
{
	switch (spaceCont)
	{

	//Узел полностью в камере
	case Frustum::scContainsFully:		
		return true;

	//Узел частично в камере
	case Frustum::scContainsPartially:
	{
		//Если узел конечный, то сразу можно добавить его полностью
		if (node.GetNodeList().empty())
			return true;

		NodeCull* children = new NodeCull[node.GetNodeList().size()];		
		//Узел имеет дочерей, следовательно проверяем их всех и если хотя бы один не может быть добавлен полностью разделяем узел
		int i = 0;
		for (NodeList::const_iterator iter = node.GetNodeList().begin(); iter != node.GetNodeList().end(); ++iter, ++i)
		{
			children[i].node = *iter;
			children[i].contains = frustum.ContainsAABB(children[i]->GetAABB());
			//Такой объект нашелся
			if (!DoCulling(frustum, children[i].contains, *children[i], pos))
			{
				//Добавляем все предыдущие узлы
				for (int j = 0; j < i; ++j)
					pos.InsertNode(children[j].node, children[j].contains);

				//Проверяем все следующие узлы
				NodeList::const_iterator iterNext = iter;
				++iterNext;
				for (; iterNext != node.GetNodeList().end(); ++iterNext)
					DoCulling(frustum, **iterNext, pos);

				delete[] children;
				return false;
			}
		}
		delete[] children;

		//Нет необохимости разделять узел
		return true;
	}

	//Узел не попал в камеру
	case Frustum::scNoOverlap:
		return false;
	}

	return false;
}

void OctreeSort::DoCulling(const Frustum& frustum, Node& node, Position& pos)
{
	Frustum::SpaceContains spaceCont = frustum.ContainsAABB(node.GetAABB());

	bool res = DoCulling(frustum, spaceCont, node, pos);
	if (res)
		pos.InsertNode(&node, spaceCont);
}

void OctreeSort::DestroyOctree()
{
	lsl::SafeDelete(_root);
}

void OctreeSort::RebuildOctree(const AABB& worldAABB)
{
	DestroyOctree();

	_root = new Node(cNumGroups, worldAABB, 0);

	UserList userList;
	userList.insert(userList.end(), _users->begin(), _users->end());
	BuildOctree(*_root, userList);	
}

OctreeSort::Position OctreeSort::Culling(const Frustum& frustum)
{
	LSL_ASSERT(_root);

	Position res(this);
	DoCulling(frustum, *_root, res);

	return res;	
}

void OctreeSort::RenderNode(graph::Engine& engine, const Frustum& frustum, const Node& node)
{
	//for (NodeList::const_iterator iter = node.GetNodeList().begin(); iter != node.GetNodeList().end(); ++iter)
	//	RenderNode(engine, frustum, **iter);

	//if (node.GetUserCnt() && frustum.ContainsAABB(node.GetAABB()) && &node != _root)	
		BaseSceneNode::RenderBB(engine, node.GetAABB(), node.GetUserCnt() ? clrBlue : clrRed);
}

void OctreeSort::Render(graph::Engine& engine, const Frustum& frustum)
{
	engine.GetContext().SetWorldMat(IdentityMatrix);

	RenderNode(engine, frustum, *_root);

	//for (Users::iterator iter = _users->begin(); iter != _users->end(); ++iter)
	//	BaseSceneNode::RenderBB(engine, (*iter)->GetAABB(), clrGreen);
}

void OctreeSort::Render(graph::Engine& engine, const Position& pos)
{
	engine.GetContext().SetWorldMat(IdentityMatrix);

	for (NodeCullList::const_iterator iter = pos._nodeList.begin(); iter != pos._nodeList.end(); ++iter)
		BaseSceneNode::RenderBB(engine, (*iter)->GetAABB(), (*iter)->GetUserCnt() ? clrBlue : clrRed);
}

OctreeSort::Users& OctreeSort::GetUsers()
{
	return *_users;
}

const AABB& OctreeSort::GetRootAABB() const
{
	LSL_ASSERT(IsBuildOctree());

	return _root->GetAABB();
}

}

}