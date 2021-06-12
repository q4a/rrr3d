#include "stdafx.h"

#include "lslSerialization.h"
#include <lslException.h>

namespace lsl
{

const std::string SerialFile::cParse = "parse";
const std::string SerialFile::cFolder = "folder";




int SIOTraits::GetValTypeSize(ValType type)
{
	switch (type)
	{
	case vtUnknown:
		throw lsl::Error("int SIOTraits::GetValTypeSize(ValType type)");
		break;

	case vtInt:
		return sizeof(int);
		break;

	case vtUInt:
		return sizeof(unsigned int);
		break;

	case vtFloat:
		return sizeof(float);
		break;

	case vtDouble:
		return sizeof(double);
		break;

	case vtBool:
		return sizeof(bool);
		break;

	case vtChar:
		return sizeof(char);
		break;

	default:
		throw lsl::Error("int SIOTraits::GetValTypeSize(ValType type)");
	}
}




SWriter::SWriter()
{
}

SWriter* SWriter::WriteValue(const char* name, const int* value, unsigned count)
{
	return WriteValue(name, ValueDesc(value, count));
}

SWriter* SWriter::WriteValue(const char* name, const unsigned* value, unsigned count)
{
	return WriteValue(name, ValueDesc(value, count));
}

SWriter* SWriter::WriteValue(const char* name, const float* value, unsigned count)
{
	return WriteValue(name, ValueDesc(value, count));
}

SWriter* SWriter::WriteValue(const char* name, const double* value, unsigned count)
{
	return WriteValue(name, ValueDesc(value, count));
}

SWriter* SWriter::WriteValue(const char* name, Serializable* value)
{
	LSL_ASSERT(value);

	SWriter* node = NewDummyNode(name);
	node->SaveSerializable(value);
	return node;
}

SWriter* SWriter::WriteRef(const char* name, const Component* component)
{
	return WriteValue(name, component ? component->GetComponentPath(0) : "NULL");
}

SWriter* SWriter::WriteRef(const char* name, const CollectionItem* item)
{
	if (item)
	{
		LSL_ASSERT(item->GetCollection());
		
		SWriter* writer = WriteRef(name, item->GetCollection());
		writer->WriteAttr(lsl::Serializable::cItem, item->GetName());
		return writer;
	}
	return 0;
}




SReader::SReader()
{
}

bool SReader::ResolveFixUp(_FixUpName& inOutName)
{
	if (inOutName.path == "NULL")
	{
		inOutName.component = 0;
		return true;
	}

	inOutName.component = GetRoot()->AbsoluteFindComponent(inOutName.path);
	if (inOutName.component)		
		if (!inOutName.nameCollItem.empty())
		{
			CollectionTraits* coll = inOutName.GetComponent<CollectionTraits*>();
			inOutName.collItem = coll->FindItem(inOutName.nameCollItem);
			if (inOutName.collItem)
				return true;
		}
		else
			return true;

	return false;
}

void SReader::FixUp()
{
	//FixUp ссылок на компоненты
	//результат, добавл€ютс€ только успешные фиксупы
	typedef std::map<Serializable*, _FixUpNames> OnSend;

	OnSend onSend;

	for (_FixUpNames::iterator iter = _fixUpNames.begin(); iter != _fixUpNames.end(); ++iter)
		if (ResolveFixUp(*iter))
			onSend[iter->target].push_back(*iter);

	for (OnSend::iterator iter = onSend.begin(); iter != onSend.end(); ++iter)
		iter->first->OnFixUp(iter->second);

	_fixUpNames.clear();
}

bool SReader::AddFixUp(const std::string& path, const std::string& collItem, Serializable* target, _FixUpName* fixUpName)
{
	_FixUpName fixUp;
	fixUp.name = GetMyName();
	fixUp.path = path;
	fixUp.nameCollItem = collItem;
	fixUp.target = target;
	fixUp.sender = this;
	fixUp.component = 0;
	fixUp.collItem = 0;

	if (path.empty())
		return false;

	if (target)
	{
		_fixUpNames.push_back(fixUp);
		return true;		
	}
	else
	{
		LSL_ASSERT(fixUpName);

		*fixUpName = fixUp;
		return ResolveFixUp(*fixUpName);
	}
}

bool SReader::AddFixUp(bool collItem, Serializable* target, _FixUpName* fixUpName)
{
	_FixUpName fixUp;
	if (GetRef(collItem, fixUp))	
		return AddFixUp(fixUp.path, fixUp.nameCollItem, target, fixUpName);

	return false;	
}

bool SReader::GetRef(bool collItem, _FixUpName& fixUpName)
{
	fixUpName.name = GetMyName();
	fixUpName.path = "";
	fixUpName.nameCollItem = "";
	fixUpName.target = 0;
	fixUpName.sender = this;
	fixUpName.component = 0;
	fixUpName.collItem = 0;

	if (collItem)
		if (const ValueDesc* desc = ReadAttr(Serializable::cItem))
		{
			desc->CastTo(&fixUpName.nameCollItem);
			if (fixUpName.nameCollItem.empty())
				return false;
		}
		else
			return false;
	//
	GetVal().CastTo(&fixUpName.path);
	if (fixUpName.path.empty())
		return false;

	return true;
}

SReader* SReader::ReadValue(const char* name, ValueDesc value)
{
	SReader* reader = ReadValue(name);
	if (!reader)
		return 0;
	const ValueDesc& desc = reader->GetVal();

	void* outVal = const_cast<void*>(value.value);
	switch (value.type)
	{
	case vtChar:
		desc.CastTo<char>((char*)outVal, value.count);
		break;
	case vtInt:
		desc.CastTo<int>((int*)outVal, value.count);
		break;
	case vtUInt:
		desc.CastTo<unsigned int>((unsigned int*)outVal, value.count);
		break;
	case vtFloat:
		desc.CastTo<float>((float*)outVal, value.count);
		break;
	case vtDouble:
		desc.CastTo<double>((double*)outVal, value.count);
		break;
	case vtBool:
		desc.CastTo<bool>((bool*)outVal, value.count);
		break;
	}

	return reader;
}

SReader* SReader::ReadValue(const char* name, std::string& value)
{
	if (SReader* reader = ReadValue(name))
	{
		reader->GetVal().CastTo<std::string>(&value, 1);
		return reader;
	}
	else
		return 0;
}

SReader* SReader::ReadValue(const char* name, int* value, unsigned count)
{
	return ReadValue(name, ValueDesc(value, count));
}

SReader* SReader::ReadValue(const char* name, unsigned* value, unsigned count)
{
	return ReadValue(name, ValueDesc(value, count));
}

SReader* SReader::ReadValue(const char* name, float* value, unsigned count)
{
	return ReadValue(name, ValueDesc(value, count));
}

SReader* SReader::ReadValue(const char* name, double* value, unsigned count)
{
	return ReadValue(name, ValueDesc(value, count));
}

SReader* SReader::ReadValue(const char* name, Serializable* value)
{
	LSL_ASSERT(value);

	if (SReader* reader = ReadValue(name))
	{
		reader->LoadSerializable(value);
		return reader;
	}
	else
		return 0;
}

SReader* SReader::ReadRef(const char* name, bool collItem, Serializable* target, _FixUpName* fixUpName)
{
	SReader* child = ReadValue(name);
	if (child && child->AddFixUp(collItem, target, fixUpName))
		return child;
	else
		return 0;
}




SerialNode::SerialNode(): _value(ValueDesc()), _linkSer(0), _masterSer(0), _masterNode(0), _linkSerRefCnt(0), _proxyLoad(0), _beginSave(0), _beginLoad(0)
{
	_elements = new Elements(this);
}

SerialNode::~SerialNode()
{
	LSL_ASSERT(!IsProxyLoad() && !_linkSer);

	Clear();
	delete _elements;
}

SWriter* SerialNode::WriteRefNode(SWriter* writer, const std::string& name, SerialNode* node)
{
	return writer->WriteRef(name.c_str(), node);
}

bool SerialNode::ReadRefNodeFrom(SReader* reader, SerialNode** outNode)
{
	*outNode = 0;
	_FixUpName fixUp;
	if (reader->AddFixUp(false, 0, &fixUp))
	{
		//  сожалению сущесвтует нека€ двойсвенность с dynamic_cast, поэтому так нельз€
		//*outNode = outName.GetComponent<SerialNode*>();
		//ѕока без динмаической проверки типов во врем€ дебага
		*outNode = static_cast<SerialNode*>(fixUp.component);
		return true;
	}

	return false;
}

SReader* SerialNode::ReadRefNode(SReader* reader, const std::string& name, SerialNode** outNode)
{
	*outNode = 0;
	SReader* child = reader->ReadValue(name.c_str());
	if (child && ReadRefNodeFrom(child, outNode))	
		return child;

	return 0;
}

SerialNode* SerialNode::FindLinkSer(Serializable* value)
{
	SerialNode* result;
	if (result = (_linkSer == value ? this : 0))
		return result;

	for (Elements::iterator iter = _elements->begin(); iter != _elements->end(); ++iter)
		if (result = (*iter)->FindLinkSer(value))
			return result;

	return 0;
}

void SerialNode::ResolveProxyRef()
{
	if (_linkSer)
	{
		if (_linkSer->GetMasterSer())
		{
			lsl::SerialNode* root = GetRootNode();
			
			SerialNode* master = root->FindLinkSer(_linkSer->GetMasterSer());
			if (master)
			{
				WriteAttr(Serializable::cMaster, master->GetComponentPath(root));
				unsigned mLock = --(master->_linkSer->_lockCnt);
				if (mLock == 0)
					master->_linkSer = 0;
				else
					LSL_ASSERT(mLock > 0);
			}
			else
			{
				WriteAttr(Serializable::cMaster, Serializable::cInclude);
				_linkSer->Save(this);
			}
		}

		unsigned mLock = --(_linkSer->_lockCnt);
		if (mLock == 0)
			_linkSer = 0;
		else
			LSL_ASSERT(mLock > 0);
	}

	for (Elements::iterator iter = _elements->begin(); iter != _elements->end(); ++iter)
		(*iter)->ResolveProxyRef();
}

void SerialNode::OnFixUp()
{
	SerialNode* child = FirstChild();
	while (child)
	{
		child->OnFixUp();
		child = child->NextNode();
	}

	FixUp();

	//FixUp ссылок на узлы, востановление прокси списков
	//Include master
	Serializable* ser = _masterSer;
	if (_masterNode)
	{
		LSL_ASSERT(!ser && _masterNode->_linkSer);

		//Ќайденый master
		ser = _masterNode->_linkSer;
		_masterNode->ReleaseSerLink();
	}
	//ѕоскольку узел может содержать как Serializable, так и просто значение, то необходима проверка на то что узел содержит именно Serializable
	if (_linkSer)
	{
		_linkSer->SetMasterSer(ser);
		ReleaseSerLink();
	}
	_masterSer = 0;
	_masterNode = 0;
}

void SerialNode::AddRefSerLink()
{
	++_linkSerRefCnt;
}

void SerialNode::ReleaseSerLink()
{
	LSL_ASSERT(_linkSerRefCnt > 0);

	if (--_linkSerRefCnt == 0)
		_linkSer = 0;
}

void SerialNode::BeginProxyLoad()
{
	++_proxyLoad;
	for (Elements::iterator iter = _elements->begin(); iter != _elements->end(); ++iter)
		(*iter)->BeginProxyLoad();
}

void SerialNode::EndProxyLoad()
{
	LSL_ASSERT(_proxyLoad > 0);

	for (Elements::iterator iter = _elements->begin(); iter != _elements->end(); ++iter)
		(*iter)->EndProxyLoad();
	--_proxyLoad;
}

bool SerialNode::IsProxyLoad() const
{
	return _proxyLoad > 0;
}

void SerialNode::SaveSerializable(Serializable* value)
{
	//—ссылка об€зательно должны быть обнулена, что говорит о правильном процессе разрешени€ прокси ссылок. “акже записать ссылку можно только один раз
	LSL_ASSERT(value && !_linkSer);

	//”словие того что прилинкованный узел используетс€ в разрешении прокси ссылок
	//if (!value->GetProxySerList().empty() || value->GetMasterSer())
	//{
	//ƒобавл€ем ссылку на Serializable к которому прилинкован узел
	_linkSer = value;
	//ƒобавл€ем ссылку на Serializable, чтобы знать сколько узлов его используют
	++_linkSer->_lockCnt;
	//}

	if (value->GetMasterSer())	
		++value->GetMasterSer()->_lockCnt;	
	//≈сли отсутствует мастер объект, то сохран€ем данные
	else
		value->Save(this);
}

void SerialNode::WriteAttr(const char* name, const ValueDesc& desc)
{
	AddAttribute(name, desc);
}

void SerialNode::SetVal(const ValueDesc& desc)
{
	SetValue(desc);
}

SWriter* SerialNode::NewDummyNode(const char* name)
{
	return _elements->Add(name);
}

SWriter* SerialNode::WriteValue(const char* name, const ValueDesc& desc)
{
	SerialNode* child = _elements->Add(name);
	child->SetValue(desc);
	return child;
}

void SerialNode::LoadSerializable(Serializable* value)
{
	LSL_ASSERT(value && !(_linkSer && !IsProxyLoad()));

	lsl::Serializable* masterSer = 0;
	lsl::SerialNode* masterNode = 0;

	if (const ValueDesc* masterDesc = ReadAttr(Serializable::cMaster))
	{
		std::string path;
		masterDesc->CastTo<std::string>(&path);
		if (path == lsl::Serializable::cInclude)
		{
			masterSer = value->GetMasterSer();
			value->Load(this);
		}
		else
		{
			//»скомый объект SerialNode уже должен существовать поскольку создание иерархии происходит в момент загрузки из источника.   тому же искомый объект сам по себе содержит данные которые должны быть по заложенной логике доступны на момент загрузки
			masterNode = GetRootNode()->FindNode(path);
			if (masterNode)
			{
				masterNode->AddRefSerLink();
				masterNode->BeginProxyLoad();
				value->Load(masterNode);
				masterNode->EndProxyLoad();
			}
		}
	}
	else
		value->Load(this);

	//ƒобавл€ем ссылку на прилинкованный Serializable только если из этого узла не загружаетс€ прокси
	if (!IsProxyLoad())
	{
		//»з одного узла не могут загружатьс€ два объекта, поскольку в этом случае непон€тно какой именно объект линковать к узлу
		LSL_ASSERT(!_linkSer);

		AddRefSerLink();
		_linkSer = value;
		_masterSer = masterSer;
		_masterNode = masterNode;		
	}
}

const SerialNode::ValueDesc* SerialNode::ReadAttr(const char* name)
{
	Attributes::iterator iter = _attributes.find(name);
	if (iter != _attributes.end())
		return iter->second;
	else
		return 0;
}

const std::string& SerialNode::GetMyName() const
{
	return GetName();
}

const SerialNode::ValueDesc& SerialNode::GetVal() const
{
	return _value;
}

SReader* SerialNode::ReadValue(const char* name)
{
	SerialNode* node = FindChild(name);
	return node;
}

SReader* SerialNode::GetOwnerValue()
{
	return GetOwner();
}

SReader* SerialNode::FirstChildValue()
{
	return FirstChild();
}

SReader* SerialNode::PrevValue()
{
	return PrevNode();
}

SReader* SerialNode::NextValue()
{
	return NextNode();
}

SerialNode* SerialNode::IsRoot()
{
	return 0;
}

SerialNode* SerialNode::FirstChild()
{
	return _elements->begin() != _elements->end() ? *_elements->begin() : 0;
}

SerialNode* SerialNode::PrevNode()
{
	if (GetOwner() && GetOwner()->_elements->Count() > 1)
	{
		SerialNodes::iterator iter = GetOwner()->_elements->Find(this);
		return iter != GetOwner()->_elements->begin() ? *(--iter) : 0;
	}
	else
		return 0;
}

SerialNode* SerialNode::NextNode()
{
	if (GetOwner() && GetOwner()->_elements->Count() > 1)
	{
		SerialNodes::iterator iter = GetOwner()->_elements->Find(this);
		++iter;
		return iter != GetOwner()->_elements->end() ? *iter : 0;
	}
	else
		return 0;
}

SerialNode::Value* SerialNode::AddAttribute(const char* name, const ValueDesc& desc)
{
	if (_attributes.find(name) != _attributes.end())
		throw lsl::Error("SerialNode::Value* SerialNode::AddAttribute(const char* name, const ValueDesc& desc)");

	Value* value = new Value(desc);
	_attributes[name] = value;
	return value;
}

void SerialNode::DelAttribute(Attributes::iterator value)
{
	Value* item = value->second;
	_attributes.erase(value);
	delete item;
}

void SerialNode::ClearAttributes()
{
	for (Attributes::iterator iter = _attributes.begin(); iter != _attributes.end(); ++iter)
		delete (iter->second);
	_attributes.clear();
}

void SerialNode::Clear()
{
	ClearAttributes();
	_elements->Clear();
}

SerialNode* SerialNode::FindChild(const std::string& name)
{
	return static_cast<SerialNode*>(_MyBase::FindChild(name));
}

SerialNode* SerialNode::FindNode(const std::string& name)
{
	return static_cast<SerialNode*>(_MyBase::FindComponent(name));
}

SWriter* SerialNode::BeginSave()
{
	if (_beginSave++ == 0)	
		Clear();

	return this;
}

void SerialNode::EndSave()
{
	LSL_ASSERT(_beginSave > 0);

	if (--_beginSave == 0)	
		//–азрешение и подстановка путей дл€ прокси ссылок
		ResolveProxyRef();
}

SReader* SerialNode::BeginLoad()
{
	_beginLoad++;

	return this;
}

void SerialNode::EndLoad()
{
	LSL_ASSERT(_beginLoad > 0);

	if (--_beginLoad == 0)	
		//закончили чтение корневого компонента, врем€ fixUp
		OnFixUp();
}

void SerialNode::Save(Serializable* root)
{
	BeginSave();
	try
	{
		//«апись напр€мую, т.е. без учета прокси ссылок
		root->Save(this);		
	}
	LSL_FINALLY(EndSave();)
}

void SerialNode::Load(Serializable* root)
{
	BeginLoad();	
	try
	{
		//чтение напр€мую, т.е. без учета прокси ссылок
		root->Load(this);
	}
	LSL_FINALLY(EndLoad();)
}

const SerialNode::ValueDesc& SerialNode::GetValue() const
{
	return _value;
}

void SerialNode::SetValue(const ValueDesc& value)
{
	_value.Assign(value);
}

void SerialNode::SetValue(const std::string& value)
{
	_value.AssignFromString(value);
}

SerialNode* SerialNode::GetOwner()
{
	return !IsRoot() ? static_cast<SerialNode*>(_MyBase::GetOwner()) : 0;
}

lsl::Component* SerialNode::GetRoot()
{
	return Component::GetRoot();
}

SerialNode* SerialNode::GetRootNode()
{
	SerialNode* res = 0;
	SerialNode* root = this;
	do
	{
		res = root;
		root = root->GetOwner();
	}
	while (root);

	return res;
}

SerialNode::Elements& SerialNode::GetElements()
{
	return *_elements;
}

const SerialNode::Attributes& SerialNode::GetAttributes() const
{
	return _attributes;
}




SerialNodes::SerialNodes(SerialNode* owner): _owner(owner)
{
}

SerialNodes::~SerialNodes()
{
	Clear();
}

SerialNode* SerialNodes::Add(const std::string& name)
{
	SerialNode* node = new SerialNode();
	node->SetName(name);
	node->SetOwner(_owner);
	_list.push_back(node);
	return node;
}

void SerialNodes::Delete(iterator iter)
{
	SerialNode* node = *iter;
	_list.erase(iter);
	delete node;
}

void SerialNodes::Delete(SerialNode* value)
{
	Delete(Find(value));
}

void SerialNodes::Clear()
{
	for (_List::const_iterator iter = _list.begin(); iter != _list.end(); ++iter)
		delete *iter;
	_list.clear();
}

unsigned SerialNodes::Count() const
{
	return _list.size();
}

SerialNodes::iterator SerialNodes::Find(SerialNode* node)
{
	return std::find(_list.begin(), _list.end(), node);
}

SerialNode* SerialNodes::Find(const std::string& name)
{
	for (_List::iterator iter = _list.begin(); iter != _list.end(); ++iter)
		if ((*iter)->GetName() == name)
			return *iter;

	return 0;
}

SerialNodes::iterator SerialNodes::begin()
{
	return _list.begin();
}

SerialNodes::iterator SerialNodes::end()
{
	return _list.end();
}

SerialNodes::const_iterator SerialNodes::begin() const
{
	return _list.begin();
}

SerialNodes::const_iterator SerialNodes::end() const
{
	return _list.end();
}

SerialNode* SerialNodes::GetOwner()
{
	return _owner;
}




RootNode::RootNode(const std::string& name, Component* owner)
{
	SetName(name);
	SetOwner(owner);
}

SerialNode* RootNode::IsRoot()
{
	return this;
}

}