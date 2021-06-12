#include "stdafx.h"

#include "lslSerializable.h"
#include "lslSerialization.h"

namespace lsl
{

const char* Serializable::cInclude = "include";
const char* Serializable::cMaster = "master";
const char* Serializable::cItem = "item";




Serializable::Serializable(): _masterSer(0), _lockCnt(0)
{
}

Serializable::~Serializable()
{
	if (_lockCnt)
		throw Error("Serializable::~Serializable()");

	ClearProxySer();
	SetMasterSer(0);
}

void Serializable::AssignFromSNode(SerialNode* node)
{
	node->Load(this);
}

void Serializable::AssignFromSer(Serializable* value, lsl::Component* root)
{
	RootNode* serNode = new RootNode("tmp", root);
	serNode->Save(value);
	AssignFromSNode(serNode);
	delete serNode;
}

void Serializable::InsertProxySer(Serializable* value)
{
	LSL_ASSERT(!value->_masterSer && value->_lockCnt == 0);

	value->_masterSer = this;
	_proxySerList.push_back(value);
}

void Serializable::RemoveProxySer(Serializable* value)
{
	LSL_ASSERT(value->_masterSer == this && value->_lockCnt == 0);

	value->_masterSer = 0;
	_proxySerList.remove(value);
}

void Serializable::ClearProxySer()
{
	for (ProxySerList::iterator iter = _proxySerList.begin(); iter != _proxySerList.end(); ++iter)
	{
		LSL_ASSERT((*iter)->_lockCnt == 0);

		(*iter)->_masterSer = 0;
	}
	_proxySerList.clear();
}

void Serializable::SyncProxySer(lsl::Component* root)
{
	for (ProxySerList::iterator iter = _proxySerList.begin(); iter != _proxySerList.end(); ++iter)
		(*iter)->AssignFromSer(this, root);
}

Serializable* Serializable::GetMasterSer()
{
	return _masterSer;
}

void Serializable::SetMasterSer(Serializable* value)
{
	if (_masterSer != value)
	{
		if (_masterSer)
			_masterSer->RemoveProxySer(this);
		if (value)
			value->InsertProxySer(this);
	}
}

const Serializable::ProxySerList& Serializable::GetProxySerList() const
{
	return _proxySerList;
}

}