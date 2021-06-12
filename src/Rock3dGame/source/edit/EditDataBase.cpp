#include "stdafx.h"
#include "game\World.h"

#include "edit\DataBase.h"
#include "edit\Edit.h"


namespace r3d
{

namespace edit
{

MapObjRec::MapObjRec(Inst* inst): ExternImpl<Inst>(inst)
{
}

unsigned MapObjRec::GetCategory()
{
	return GetInst()->GetCategory();
}

std::string MapObjRec::GetCategoryName()
{
	return game::IMapObjLib_cCategoryStr[GetCategory()];
}

const std::string& MapObjRec::GetName() const
{
	return GetInst()->GetName();
}

void MapObjRec::SetName(const std::string& value)
{
	GetInst()->SetName(value);
}




RecordNode::RecordNode(Inst* inst): ExternImpl<Inst>(inst)
{
}

IMapObjRecRef RecordNode::FirstRecord()
{
	Inst::RecordList::const_iterator iter = GetInst()->GetRecordList().begin();
	MapObjRec* rec = 0;
	if (iter != GetInst()->GetRecordList().end())
	{
		rec = new MapObjRec(static_cast<game::MapObjRec*>(*iter));
		rec->libIter = iter;
	}

	return IMapObjRecRef(rec, rec);
}

void RecordNode::NextRecord(IMapObjRecRef& ref)
{
	MapObjRec* rec = ref->GetImpl<MapObjRec>();
	
	if (++(rec->libIter) != GetInst()->GetRecordList().end())
	{
		MapObjRec* newRec = new MapObjRec(static_cast<game::MapObjRec*>(*rec->libIter));
		newRec->libIter = rec->libIter;
		ref.Reset(newRec, newRec);
	}
	else
		ref.Release();
}

IRecordNodeRef RecordNode::FirstNode()
{
	Inst::NodeList::const_iterator iter = GetInst()->GetNodeList().begin();
	RecordNode* node = 0;
	if (iter != GetInst()->GetNodeList().end())
	{
		node = new RecordNode(*iter);
		node->libIter = iter;
	}

	return IRecordNodeRef(node, node);
}

void RecordNode::NextNode(IRecordNodeRef& ref)
{
	RecordNode* node = ref->GetImpl<RecordNode>();
	
	if (++(node->libIter) != GetInst()->GetNodeList().end())
	{
		RecordNode* newNode = new RecordNode(*node->libIter);
		newNode->libIter = node->libIter;
		ref.Reset(newNode, newNode);
	}
	else
		ref.Release();
}

const std::string& RecordNode::GetName() const
{
	return GetInst()->GetName();
}




MapObjLib::MapObjLib(Inst* inst): RecordNode(inst)
{
}




DataBase::DataBase(Edit* edit): _edit(edit)
{
}

game::DataBase* DataBase::GetInst()
{
	return _edit->GetWorld()->GetDB();
}

IMapObjLibRef DataBase::GetMapObjLib(unsigned i)
{
	MapObjLib* lib = new MapObjLib(GetInst()->GetMapObjLib(game::MapObjLib::Category(i)));
	return IMapObjLibRef(lib);
}

unsigned DataBase::GetMapObjLibCnt()
{
	return game::MapObjLib::cCategoryEnd;
}

}

}