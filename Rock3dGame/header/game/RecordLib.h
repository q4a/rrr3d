#pragma once

namespace r3d
{
	namespace game
	{
		class Record : public Object
		{
			friend class RecordLib;
			friend class RecordNode;
		public:
			using Lib = RecordLib;

			struct Desc
			{
				Desc(): name(""), lib(nullptr), parent(nullptr), src(nullptr)
				{
				}

				Desc(const std::string& mName, RecordLib* mLib, RecordNode* mParent, SerialNode* mSrc): name(mName),
					lib(mLib), parent(mParent), src(mSrc)
				{
				}

				std::string name;
				RecordLib* lib;
				RecordNode* parent;
				SerialNode* src;
			};

		private:
			std::string _name;

			RecordLib* _lib;
			RecordNode* _parent;
			SerialNode* _src;
		protected:
			Record(const Desc& desc);
			~Record() override;

			SerialNode* GetSrc() const;
		public:
			void Save(Serializable* root) const;
			void Load(Serializable* root) const;

			RecordLib* GetLib() const;
			RecordNode* GetParent() const;

			const std::string& GetName() const;
			void SetName(const std::string& value);
		};

		class RecordNode
		{
			friend class RecordLib;
		public:
			using Desc = Record::Desc;
			using RecordList = List<Record*>;
			using NodeList = List<RecordNode*>;
		private:
			std::string _name;

			RecordLib* _lib;
			RecordNode* _parent;
			SerialNode* _src;

			RecordList _recordList;
			NodeList _nodeList;

			Record* FindRecord(const StringList& strList);
			//Поиск узла
			//res - самый глубокий узел соответствующий путю
			//outList - оставшийся путь
			RecordNode* FindNode(const StringList& strList, StringList& outList);
		protected:
			RecordNode(const Desc& desc);
			~RecordNode();

			Record* AddRecord(const std::string& name, SerialNode* src);
			RecordNode* AddNode(const std::string& name, SerialNode* src);
			void ClearStructure();

			SerialNode* GetSrc() const;
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

			RecordLib* GetLib() const;
			RecordNode* GetParent() const;

			const RecordList& GetRecordList() const;
			const NodeList& GetNodeList() const;

			const std::string& GetName() const;
			virtual void SetName(const std::string& value);
		};

		class RecordLib : public RecordNode, public Component
		{
			friend Record;
			friend RecordNode;
		private:
			using _MyBase = RecordNode;
		public:
			using Lib = RecordLib;

			static SWriter* SaveRecordRef(SWriter* writer, const std::string& name, Record* record);
			static Record* LoadRecordRefFrom(SReader* reader);
			static Record* LoadRecordRef(SReader* reader, const std::string& name);
		private:
			SerialNode* _rootSrc;
		protected:
			virtual Record* CreateRecord(const Record::Desc& desc);
			virtual void DestroyRecord(Record* record);

			virtual RecordNode* CreateNode(const Desc& desc);
			virtual void DestroyNode(RecordNode* node);

			SerialNode* CreateSrc(const std::string& name, RecordNode* parent, bool node) const;
			void DestroySrc(SerialNode* src, RecordNode* parent) const;

			virtual bool ValidateName(const std::string& name, RecordNode* parent);
			static void CheckName(const std::string& name, RecordNode* parent);
			static Record* FindRecordBySrc(SerialNode* src, RecordNode* curNode);
		public:
			RecordLib(const std::string& name, SerialNode* rootSrc);
			~RecordLib() override;

			//name - имя, может включать разделители '\\' для группировки по узлам
			virtual Record* GetOrCreateRecord(const std::string& name);

			void SetName(const std::string& value) override;
		};

		template <class _Record>
		class RecordList : public Container<_Record*>, public Serializable
		{
		private:
			using _MyCont = Container<_Record>;
		protected:
			void Save(SWriter* writer) override;
			void Load(SReader* reader) override;
		};

		template <class _Record>
		class record_list final : public RecordList<_Record>
		{
		public:
		};


		template <class _Record>
		void RecordList<_Record>::Save(SWriter* writer)
		{
			unsigned i = 0;
			for (iterator iter = begin(); iter != end(); ++iter, ++i)
			{
				std::stringstream sstream;
				sstream << "item" << i;

				_Record::Lib::SaveRecordRef(writer, sstream.str().c_str(), *iter);
			}
		}

		template <class _Record>
		void RecordList<_Record>::Load(SReader* reader)
		{
			Clear();

			SReader* child = reader->FirstChildValue();
			while (child)
			{
				Insert(_Record::Lib::LoadRecordRefFrom(child));
				child = child->NextValue();
			}
		}
	}
}
