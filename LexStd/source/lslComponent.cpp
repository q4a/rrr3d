#include "stdafx.h"

#include "lslComponent.h"
#include "lslUtility.h"
#include "lslSerialization.h"

namespace lsl
{

const char* Component::cDefItemName = "item";




Component::Component(): _owner(0), storeName(true)
{
	_elements = new Elements(this);
}

Component::~Component()
{
	LSL_ASSERT(_elements->Size() == 0);

	SetOwner(0);
	delete _elements;
}

void Component::Save(SWriter* writer)
{
	if (storeName && !_name.empty())
		writer->WriteAttr("name", _name);
}

void Component::Load(SReader* reader)
{
	if (storeName)	
		if (const SReader::ValueDesc* desc = reader->ReadAttr("name"))
		{
			std::string name;
			desc->CastTo(&name);
			SetName(name);
		}
}

bool Component::ValidateChild(Component* child)
{
	bool validName = false;
	if (child->_name.empty())
	{
		child->_name = MakeUniqueName(cDefItemName);
		validName = true;
	}
	else
		validName = !FindItemByName(_elements->begin(), _elements->end(), child->_name, child);
	/*if (!validName)
	{
		unsigned nameCnt = 0;
		for (Elements::iterator iter = _elements->begin(); iter != _elements->end(); ++iter)
			if ((*iter)->GetName() == child->_name)
				if ((*iter) != child || (++nameCnt) > 1)
				{
					validName = false;
					break;
				}
		validName = true;
	}*/

	if (!child->GetOwner() && child != this && validName)
		return true;
	else
		return false;
}

Component* Component::FindChild(const std::string& name)
{
	for (Elements::const_iterator iter = _elements->begin(); iter != _elements->end(); ++iter)
		if ((*iter)->GetName() == name)
			return *iter;
	return 0;
}

const Component* Component::FindChild(const std::string& name) const
{
	for (Elements::const_iterator iter = _elements->begin(); iter != _elements->end(); ++iter)
		if ((*iter)->GetName() == name)
			return *iter;
	return 0;
}

Component* Component::FindComponent(const std::string& name)
{
	std::string::const_iterator curIter = name.begin();
	Component* curComp = this;
	while (curIter != name.end())
	{
		std::string::const_iterator nextIter;
		nextIter = std::find(curIter, name.end(), '\\');
		std::string childName = std::string(curIter, nextIter);
		//ищем дочь
		if (curComp)
			curComp = curComp->FindChild(childName);
		if (!curComp)
			return 0;

		curIter = nextIter;
		//начинаем со следующего символа если не конец строки
		if (curIter != name.end())
			++curIter;
	}

	return curComp;	
}

Component* Component::AbsoluteFindComponent(const std::string& name)
{
	std::string::const_iterator nextIter = std::find(name.begin(), name.end(), '\\');
	
	std::string rootName(name.begin(), nextIter);
	if (rootName != _name)
		throw lsl::Error("Component* Component::AbsoluteFindComponent(const std::string& name)");

	std::string path(++nextIter, name.end());	
	return FindComponent(path);
}

std::string Component::GetComponentPath(Component* top) const
{
	std::string resStr = _name;
	Component* owner = _owner;
	while (owner && owner != top)
	{
		resStr.insert(0, owner->GetName() + '\\');
		owner = owner->GetOwner();
	}

	return resStr;
}

bool Component::ValidateName(const std::string& name) const
{
	return !FindChild(name);
}

std::string Component::MakeUniqueName(const std::string& base) const
{
	std::ostringstream oss(base, std::ios::ate);
	std::streamoff nPos = oss.tellp();
	int cnt = 0;
	do
	{
		oss.seekp(nPos);
		oss<<cnt;
		++cnt;		
	}
	while (FindChild(oss.str()));
	
	return oss.str();
}

Component* Component::GetOwner()
{
	return _owner;
}

void Component::SetOwner(Component* value)
{
	if (_owner != value)
	{
		if (_owner)
			_owner->_elements->Remove(this);
		if (value)					
			value->_elements->Insert(this);
	}
}

Component* Component::GetRoot()
{
	Component* owner = this;
	while (owner->GetOwner())
		owner = owner->GetOwner();
	return owner;
}

const std::string& Component::GetName() const
{
	return _name;
}

void Component::SetName(const std::string& value)
{
	if (_name != value)
	{
		LSL_ASSERT(!(_owner && _name.empty()));

		if (ValidateComponentName(value) && !(_owner && _owner->FindComponent(value)))
		{
			_name = value;
			return;
		}
		
		LSL_ASSERT(false);
	}
}

Component::Elements& Component::GetElements() const
{
	return *_elements;
}




bool CheckDisabledChar(const char& item)
{
	return item == '\\' || item == '.';
}

bool ValidateComponentName(const std::string& name)
{
	return std::find_if(name.begin(), name.end(), &CheckDisabledChar) == name.end();
}

}