#include "stdafx.h"
#include "game\RecordLib.h"

namespace r3d
{

namespace game
{

void DevideStr(std::string::const_iterator sIter, std::string::const_iterator eIter, lsl::StringList& outList, char dev = '\\')
{

	std::string::const_iterator lastIter = sIter;
	for (std::string::const_iterator iter = sIter; iter != eIter;)
	{
		++iter;

		if (iter == eIter)		
			outList.push_back(std::string(lastIter, iter));		
		else if (*iter == dev)
		{
			outList.push_back(std::string(lastIter, iter));
			lastIter = iter;
			++lastIter;
		}
	}
}




Record::Record(const Desc& desc): _lib(desc.lib), _parent(desc.parent), _name(desc.name), _src(desc.src)
{
	if (_src)
		_src->AddRef();
}

Record::~Record()
{
	lsl::SafeRelease(_src);
}

lsl::SerialNode* Record::GetSrc()
{
	return _src;
}

void Record::Save(Serializable* root)
{
	_src->Save(root);
}

void Record::Load(Serializable* root)
{
	_src->Load(root);
}

RecordLib* Record::GetLib()
{
	return _lib;
}

RecordNode* Record::GetParent()
{
	return _parent;
}

const std::string& Record::GetName() const
{
	return _name;
}

void Record::SetName(const std::string& value)
{
	if (_name != value)
	{
		if (!_lib->ValidateName(value, _parent))
			throw lsl::Error("void Record::SetName(const std::string& value)");

		_name = value;
		_src->SetName(value);
	}
}




RecordNode::RecordNode(const Desc& desc): _lib(desc.lib), _parent(desc.parent), _name(desc.name), _src(desc.src)
{
	if (_src)
		_src->AddRef();
}

RecordNode::~RecordNode()
{
	Clear();

	lsl::SafeRelease(_src);
}

Record* RecordNode::FindRecord(const lsl::StringList& strList)
{
	lsl::StringList outList;
	RecordNode* res = FindNode(strList, outList);

	if (res && outList.size() == 1)
	{
		for (RecordList::iterator iter = res->_recordList.begin(); iter != res->_recordList.end(); ++iter)
			if ((*iter)->GetName() == outList.front())
				return *iter;
	}
	return 0;
}

RecordNode* RecordNode::FindNode(const lsl::StringList& strList, lsl::StringList& outList)
{
	RecordNode* node = this;

	for (lsl::StringList::const_iterator iter = strList.begin(); iter != strList.end(); ++iter)
	{	
		RecordNode* newNode = 0;
		for (NodeList::iterator iterNode = node->_nodeList.begin(); iterNode != node->_nodeList.end(); ++iterNode)
			if ((*iterNode)->GetName() == *iter)
			{
				newNode = *iterNode;
				break;
			}
		if (newNode == 0)
		{
			outList.insert(outList.end(), iter, strList.end());
			return node;
		}

		node = newNode;		
	}

	return node;
}

Record* RecordNode::AddRecord(const std::string& name, lsl::SerialNode* src)
{
	_lib->CheckName(name, this);

	Record::Desc desc;
	desc.lib = _lib;
	desc.name = name;
	desc.parent = this;
	desc.src = src;

	_recordList.push_back(_lib->CreateRecord(desc));

	return _recordList.back();
}

RecordNode* RecordNode::AddNode(const std::string& name, lsl::SerialNode* src)
{
	_lib->CheckName(name, this);

	Record::Desc desc;
	desc.lib = _lib;
	desc.name = name;
	desc.parent = this;
	desc.src = src;

	_nodeList.push_back(_lib->CreateNode(desc));

	return _nodeList.back();
}

void RecordNode::ClearStructure()
{
	for (RecordList::iterator iter = _recordList.begin(); iter != _recordList.end(); ++iter)
		_lib->DestroyRecord(*iter);
	for (NodeList::iterator iter = _nodeList.begin(); iter != _nodeList.end(); ++iter)
		_lib->DestroyNode(*iter);

	_recordList.clear();
	_nodeList.clear();
}

lsl::SerialNode* RecordNode::GetSrc()
{
	return _src;
}

Record* RecordNode::AddRecord(const std::string& name)
{
	return AddRecord(name, _lib->CreateSrc(name, this, false));
}

void RecordNode::DelRecord(Record* value)
{
	LSL_ASSERT(value->GetParent() == this);

	_recordList.Remove(value);
	_lib->DestroySrc(value->GetSrc(), this);
	_lib->DestroyRecord(value);
}

RecordNode* RecordNode::AddNode(const std::string& name)
{
	return AddNode(name, _lib->CreateSrc(name, this, true));
}

void RecordNode::DelNode(RecordNode* value)
{
	LSL_ASSERT(value->GetParent() == this);

	_nodeList.Remove(value);
	_lib->DestroySrc(value->GetSrc(), this);
	_lib->DestroyNode(value);
}

Record* RecordNode::FindRecord(const std::string& path)
{
	if (path.empty())
		return 0;

	lsl::StringList strList;
	DevideStr(path.begin(), path.end(), strList);

	return FindRecord(strList);
}

RecordNode* RecordNode::FindNode(const std::string& path)
{
	if (path.empty())
		return 0;

	lsl::StringList strList;
	DevideStr(path.begin(), path.end(), strList);

	lsl::StringList outStr;
	RecordNode* node = FindNode(strList, outStr);

	return outStr.empty() ? node : 0;
}

void RecordNode::Clear()
{
	ClearStructure();
	_src->Clear();
}

void RecordNode::SrcSync()
{
	ClearStructure();

	for (SerialNode::Elements::iterator iter = _src->GetElements().begin(); iter != _src->GetElements().end(); ++iter)
	{
		SerialNode::Attributes::const_iterator iterAttr = (*iter)->GetAttributes().find(SerialFile::cFolder);
		const SerialNode::ValueDesc* desc = iterAttr != (*iter)->GetAttributes().end() ? iterAttr->second : 0;
		if (desc && desc->ToBool() && *desc->ToBool())
			AddNode((*iter)->GetName(), *iter)->SrcSync();
		else		
			AddRecord((*iter)->GetName(), *iter);
	}
}

RecordLib* RecordNode::GetLib()
{
	return _lib;
}

RecordNode* RecordNode::GetParent()
{
	return _parent;
}

const RecordNode::RecordList& RecordNode::GetRecordList() const
{
	return _recordList;
}

const RecordNode::NodeList& RecordNode::GetNodeList() const
{
	return _nodeList;
}

const std::string& RecordNode::GetName() const
{
	return _name;
}

void RecordNode::SetName(const std::string& value)
{
	if (_name != value)
	{
		if (!_lib->ValidateName(value, _parent))
			throw lsl::Error("void Record::SetName(const std::string& value)");

		_name = value;
		_src->SetName(value);
	}
}




RecordLib::RecordLib(const std::string& name, lsl::SerialNode* rootSrc): _MyBase(Desc(name, 0, 0, 0)), _rootSrc(rootSrc)
{
	LSL_ASSERT(_rootSrc);

	_rootSrc->AddRef();

	_lib = this;
	RecordNode::_name = name;
	Component::SetName(name);

	_src = rootSrc->GetElements().Find(name);
	if (!_src)
		_src = CreateSrc(name, 0, true);
	_src->AddRef();
}

RecordLib::~RecordLib()
{
	_rootSrc->Release();
}

lsl::SWriter* RecordLib::SaveRecordRef(lsl::SWriter* writer, const std::string& name, Record* record)
{
	if (!record)
		return 0;

	RecordLib* lib = record->GetLib();
	lsl::SWriter* child = lsl::SerialNode::WriteRefNode(writer, name.c_str(), record->_src);
	child->WriteAttr("lib", lib->GetComponentPath(0));

	return child;
}

Record* RecordLib::LoadRecordRefFrom(lsl::SReader* reader)
{
	if (reader == 0)
		return 0;

	lsl::SerialNode* node;
	const lsl::SReader::ValueDesc* desc = reader->ReadAttr("lib");
	if (lsl::SerialNode::ReadRefNodeFrom(reader, &node) && desc)
	{
		std::string path;
		desc->CastTo(&path);
		RecordLib* lib = lsl::StaticCast<RecordLib*>(reader->GetRoot()->AbsoluteFindComponent(path));
		
		return lib->FindRecordBySrc(node, lib);
	}

	return 0;
}

Record* RecordLib::LoadRecordRef(lsl::SReader* reader, const std::string& name)
{	
	if (lsl::SReader* child = reader->ReadValue(name.c_str()))	
		return LoadRecordRefFrom(child);

	return 0;
}

Record* RecordLib::CreateRecord(const Record::Desc& desc)
{
	return new Record(desc);
}

void RecordLib::DestroyRecord(Record* record)
{
	delete record;
}

RecordNode* RecordLib::CreateNode(const RecordNode::Desc& desc)
{
	return new RecordNode(desc);
}

void RecordLib::DestroyNode(RecordNode* node)
{
	delete node;
}

lsl::SerialNode* RecordLib::CreateSrc(const std::string& name, RecordNode* parent, bool node)
{
	lsl::SerialNode* srcParent = parent ? parent->GetSrc() : _rootSrc;

	lsl::SerialNode* src = srcParent->GetElements().Add(name);	
	if (node)
		src->AddAttribute(lsl::SerialFile::cFolder.c_str(), true);
	
	return src;
}

void RecordLib::DestroySrc(lsl::SerialNode* src, RecordNode* parent)
{
	lsl::SerialNode* srcParent = parent ? parent->GetSrc() : _rootSrc;

	srcParent->GetElements().Delete(src);
}

bool RecordLib::ValidateName(const std::string& name, RecordNode* parent)
{
	//parent == 0 --> sender is RecordLib
	//проверка выполняется в узле Component
	if (!parent)
		return true;

	return !parent->FindRecord(name) && !parent->FindNode(name);
}

void RecordLib::CheckName(const std::string& name, RecordNode* parent)
{
	LSL_ASSERT(ValidateName(name, parent));
}

Record* RecordLib::FindRecordBySrc(lsl::SerialNode* src, RecordNode* curNode)
{
	for (RecordList::const_iterator iter = curNode->GetRecordList().begin(); iter != curNode->GetRecordList().end(); ++iter)
		if ((*iter)->_src == src)
			return *iter;

	for (NodeList::const_iterator iter = curNode->GetNodeList().begin(); iter != curNode->GetNodeList().end(); ++iter)
		if (Record* record = FindRecordBySrc(src, *iter))
			return record;

	return 0;
}

Record* RecordLib::GetOrCreateRecord(const std::string& name)
{
	lsl::StringList stringList;
	DevideStr(name.begin(), name.end(), stringList);

	RecordNode* node = this;
	for (lsl::StringList::iterator iter = stringList.begin(); iter != stringList.end();)
	{	
		std::string str = *iter;
		++iter;
		
		if (iter != stringList.end())
		{
			RecordNode* newNode = node->FindNode(str);
			if (newNode == 0)
				newNode = node->AddNode(str);
			node = newNode;
		}
		else
		{
			Record* record = node->FindRecord(str);
			if (record == 0)
				record = node->AddRecord(str);

			return record;
		}
	}

	LSL_ASSERT(false);

	return 0;
}

void RecordLib::SetName(const std::string& value)
{
	_MyBase::SetName(value);
	Component::SetName(value);
}

}

}