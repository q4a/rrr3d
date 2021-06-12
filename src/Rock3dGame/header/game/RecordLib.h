#pragma once

namespace r3d
{

namespace game
{

class Record: public lsl::Object
{
	friend class RecordLib;
	friend class RecordNode;
public:
	typedef RecordLib Lib;

	struct Desc
	{
		Desc(): name(""), lib(0), parent(0), src(0) {}

		Desc(const std::string& mName, RecordLib* mLib, RecordNode* mParent, lsl::SerialNode* mSrc): name(mName), lib(mLib), parent(mParent), src(mSrc) {}

		std::string name;
		RecordLib* lib;
		RecordNode* parent;
		lsl::SerialNode* src;
	};
private:
	std::string _name;

	RecordLib* _lib;
	RecordNode* _parent;
	lsl::SerialNode* _src;
protected:
	Record(const Desc& desc);
	virtual ~Record();

	lsl::SerialNode* GetSrc();
public:
	void Save(Serializable* root);
	void Load(Serializable* root);

	RecordLib* GetLib();
	RecordNode* GetParent();

	const std::string& GetName() const;
	void SetName(const std::string& value);
};

class RecordNode
{
	friend class RecordLib;
public:
	typedef Record::Desc Desc;
	typedef List<Record*> RecordList;
	typedef List<RecordNode*> NodeList;
private:
	std::string _name;

	RecordLib* _lib;
	RecordNode* _parent;
	lsl::SerialNode* _src;

	RecordList _recordList;
	NodeList _nodeList;

	Record* FindRecord(const lsl::StringList& strList);
	//Поиск узла
	//res - самый глубокий узел соответствующий путю
	//outList - оставшийся путь
	RecordNode* FindNode(const lsl::StringList& strList, lsl::StringList& outList);
protected:
	RecordNode(const Desc& desc);
	~RecordNode();

	Record* AddRecord(const std::string& name, lsl::SerialNode* src);
	RecordNode* AddNode(const std::string& name, lsl::SerialNode* src);
	void ClearStructure();

	lsl::SerialNode* GetSrc();
public:
	Record* AddRecord(const std::string& name);
	void DelRecord(Record* value);
	
	RecordNode* AddNode(const std::string& name);
	void DelNode(RecordNode* value);

	//Поиск начинается с дочерних узлов
	Record* FindRecord(const std::string& path);
	RecordNode* FindNode(const std::string& path);

	void Clear();	
	void SrcSync();

	RecordLib* GetLib();
	RecordNode* GetParent();

	const RecordList& GetRecordList() const;
	const NodeList& GetNodeList() const;

	const std::string& GetName() const;
	void SetName(const std::string& value);
};

class RecordLib: public RecordNode, public Component
{
	friend Record;
	friend RecordNode;
private:
	typedef RecordNode _MyBase;	
public:
	typedef RecordLib Lib;

	static lsl::SWriter* SaveRecordRef(lsl::SWriter* writer, const std::string& name, Record* record);
	static Record* LoadRecordRefFrom(lsl::SReader* reader);
	static Record* LoadRecordRef(lsl::SReader* reader, const std::string& name);
private:
	lsl::SerialNode* _rootSrc;
protected:
	virtual Record* CreateRecord(const Record::Desc& desc);
	virtual void DestroyRecord(Record* record);

	virtual RecordNode* CreateNode(const RecordNode::Desc& desc);
	virtual void DestroyNode(RecordNode* node);

	lsl::SerialNode* CreateSrc(const std::string& name, RecordNode* parent, bool node);
	void DestroySrc(lsl::SerialNode* src, RecordNode* parent);

	virtual bool ValidateName(const std::string& name, RecordNode* parent);
	void CheckName(const std::string& name, RecordNode* parent);
	Record* FindRecordBySrc(lsl::SerialNode* src, RecordNode* curNode);
public:
	RecordLib(const std::string& name, lsl::SerialNode* rootSrc);
	~RecordLib();

	//name - имя, может включать разделители '\\' для группировки по узлам
	Record* GetOrCreateRecord(const std::string& name);

	void SetName(const std::string& value);
};

template<class _Record> class RecordList: public lsl::Container<_Record*>, public lsl::Serializable
{
private:
	typedef lsl::Container<_Record> _MyCont;
protected:
	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);	
};




template<class _Record> void RecordList<_Record>::Save(lsl::SWriter* writer)
{
	unsigned i = 0;
	for (iterator iter = begin(); iter != end(); ++iter, ++i)
	{
		std::stringstream sstream;
		sstream << "item" << i;

		_Record::Lib::SaveRecordRef(writer, sstream.str().c_str(), *iter);
	}
}

template<class _Record> void RecordList<_Record>::Load(lsl::SReader* reader)
{
	Clear();

	lsl::SReader* child = reader->FirstChildValue();
	while (child)
	{
		Insert(_Record::Lib::LoadRecordRefFrom(child));
		child = child->NextValue();
	}
}

}

}