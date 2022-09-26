#pragma once

#include "IDataBase.h"

namespace r3d
{
	namespace edit
	{
		class Edit;

		class MapObjRec : public IMapObjRec, public ExternImpl<game::MapObjRec>
		{
		protected:
			VirtImpl* GetImpl() override { return this; }
		public:
			MapObjRec(Inst* inst);

			unsigned GetCategory() override;
			std::string GetCategoryName() override;

			const std::string& GetName() const override;
			void SetName(const std::string& value) override;

			//Можно использовать другой подход возвращая курсор, тогда не пришлось бы хранить итератор в самом классе-результате
			game::RecordNode::RecordList::const_iterator libIter;
		};

		class RecordNode : public virtual IRecordNode, public ExternImpl<game::RecordNode>
		{
		protected:
			VirtImpl* GetImpl() override { return this; }
		public:
			RecordNode(Inst* inst);

			//Можно использовать другой подход возвращая курсор, тогда не пришлось бы хранить итератор в самом классе-результате
			IMapObjRecRef FirstRecord() override;
			void NextRecord(IMapObjRecRef& ref) override;

			IRecordNodeRef FirstNode() override;
			void NextNode(IRecordNodeRef& ref) override;

			const std::string& GetName() const override;

			game::RecordNode::NodeList::const_iterator libIter;
		};

		//Выключить ошибочный warning
#pragma warning(disable : WARNING_MULTIPLE_VIRTUAL_INHERIT_C4250)
		class MapObjLib : public IMapObjLib, public RecordNode
		{
		public:
			using Inst = game::MapObjLib;
		protected:
			VirtImpl* GetImpl() override { return this; }
		public:
			MapObjLib(Inst* inst);
		};

		//Восстановить умолчание
#pragma warning(default : WARNING_MULTIPLE_VIRTUAL_INHERIT_C4250)

		class DataBase : public IDataBase
		{
		private:
			Edit* _edit;
		public:
			DataBase(Edit* edit);

			game::DataBase* GetInst() const;

			IMapObjLibRef GetMapObjLib(unsigned i) override;
			unsigned GetMapObjLibCnt() override;
		};
	}
}
