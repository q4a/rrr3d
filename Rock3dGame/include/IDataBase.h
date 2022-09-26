#pragma once

namespace r3d
{
	namespace edit
	{
		class IMapContr : public Object
		{
		public:
			virtual void OnAddMapObj() = 0;
			virtual void OnDelMapObj() = 0;
			virtual void OnTransformMapObj() = 0;
		};

		class IMapObjRec : public ExternInterf
		{
		public:
			virtual unsigned GetCategory() = 0;
			virtual std::string GetCategoryName() = 0;

			virtual const std::string& GetName() const = 0;
			virtual void SetName(const std::string& value) = 0;
		};

		using IMapObjRecRef = AutoRef<IMapObjRec>;

		class IRecordNode;
		using IRecordNodeRef = AutoRef<IRecordNode>;

		class IRecordNode : public ExternInterf
		{
		public:
			virtual IMapObjRecRef FirstRecord() = 0;
			virtual void NextRecord(IMapObjRecRef& ref) = 0;

			virtual IRecordNodeRef FirstNode() = 0;
			virtual void NextNode(IRecordNodeRef& ref) = 0;

			virtual const std::string& GetName() const = 0;
		};

		class IMapObjLib : public virtual IRecordNode
		{
		public:
		};

		using IMapObjLibRef = AutoRef<IMapObjLib>;

		class IDataBase : public Object
		{
		public:
			virtual IMapObjLibRef GetMapObjLib(unsigned i) = 0;
			virtual unsigned GetMapObjLibCnt() = 0;
		};
	}
}
