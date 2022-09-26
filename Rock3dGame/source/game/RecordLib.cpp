#include "stdafx.h"
#include "game/RecordLib.h"

namespace r3d
{
	namespace game
	{
		void DevideStr(std::string::const_iterator sIter, std::string::const_iterator eIter, StringList& outList,
		               char dev = '\\')
		{
			auto lastIter = sIter;
			for (auto iter = sIter; iter != eIter;)
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


		Record::Record(const Desc& desc): _name(desc.name), _lib(desc.lib), _parent(desc.parent), _src(desc.src)
		{
			if (_src)
				_src->AddRef();
		}

		Record::~Record()
		{
			SafeRelease(_src);
		}

		SerialNode* Record::GetSrc() const
		{
			return _src;
		}

		void Record::Save(Serializable* root) const
		{
			_src->Save(root);
		}

		void Record::Load(Serializable* root) const
		{
			_src->Load(root);
		}

		RecordLib* Record::GetLib() const
		{
			return _lib;
		}

		RecordNode* Record::GetParent() const
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
					throw Error("void Record::SetName(const std::string& value)");

				_name = value;
				_src->SetName(value);
			}
		}


		RecordNode::RecordNode(const Desc& desc): _name(desc.name), _lib(desc.lib), _parent(desc.parent), _src(desc.src)
		{
			if (_src)
				_src->AddRef();
		}

		RecordNode::~RecordNode()
		{
			Clear();

			SafeRelease(_src);
		}

		Record* RecordNode::FindRecord(const StringList& strList)
		{
			StringList outList;
			const RecordNode* res = FindNode(strList, outList);

			if (res && outList.size() == 1)
			{
				for (const auto& iter : res->_recordList)
					if (iter->GetName() == outList.front())
						return iter;
			}
			return nullptr;
		}

		RecordNode* RecordNode::FindNode(const StringList& strList, StringList& outList)
		{
			auto node = this;

			for (auto iter = strList.begin(); iter != strList.end(); ++iter)
			{
				RecordNode* newNode = nullptr;
				for (const auto& iterNode : node->_nodeList)
					if (iterNode->GetName() == *iter)
					{
						newNode = iterNode;
						break;
					}
				if (newNode == nullptr)
				{
					outList.insert(outList.end(), iter, strList.end());
					return node;
				}

				node = newNode;
			}

			return node;
		}

		Record* RecordNode::AddRecord(const std::string& name, SerialNode* src)
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

		RecordNode* RecordNode::AddNode(const std::string& name, SerialNode* src)
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
			for (const auto& iter : _recordList)
				_lib->DestroyRecord(iter);
			for (const auto& iter : _nodeList)
				_lib->DestroyNode(iter);

			_recordList.clear();
			_nodeList.clear();
		}

		SerialNode* RecordNode::GetSrc() const
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
				return nullptr;

			StringList strList;
			DevideStr(path.begin(), path.end(), strList);

			return FindRecord(strList);
		}

		RecordNode* RecordNode::FindNode(const std::string& path)
		{
			if (path.empty())
				return nullptr;

			StringList strList;
			DevideStr(path.begin(), path.end(), strList);

			StringList outStr;
			RecordNode* node = FindNode(strList, outStr);

			return outStr.empty() ? node : nullptr;
		}

		void RecordNode::Clear()
		{
			ClearStructure();
			_src->Clear();
		}

		void RecordNode::SrcSync()
		{
			ClearStructure();

			for (const auto& iter : _src->GetElements())
			{
				auto iterAttr = iter->GetAttributes().find(SerialFile::cFolder);
				const SerialNode::ValueDesc* desc = iterAttr != iter->GetAttributes().end()
					                                    ? iterAttr->second
					                                    : nullptr;
				if (desc && desc->ToBool() && *desc->ToBool())
					AddNode(iter->GetName(), iter)->SrcSync();
				else
					AddRecord(iter->GetName(), iter);
			}
		}

		RecordLib* RecordNode::GetLib() const
		{
			return _lib;
		}

		RecordNode* RecordNode::GetParent() const
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
					throw Error("void Record::SetName(const std::string& value)");

				_name = value;
				_src->SetName(value);
			}
		}


		RecordLib::RecordLib(const std::string& name, SerialNode* rootSrc): _MyBase(Desc(name, nullptr, nullptr,
			                                                                    nullptr)), _rootSrc(rootSrc)
		{
			LSL_ASSERT(_rootSrc);

			_rootSrc->AddRef();

			_lib = this;
			RecordNode::_name = name;
			Component::SetName(name);

			_src = rootSrc->GetElements().Find(name);
			if (!_src)
				_src = CreateSrc(name, nullptr, true);
			_src->AddRef();
		}

		RecordLib::~RecordLib()
		{
			_rootSrc->Release();
		}

		SWriter* RecordLib::SaveRecordRef(SWriter* writer, const std::string& name, Record* record)
		{
			if (!record)
				return nullptr;

			const RecordLib* lib = record->GetLib();
			SWriter* child = SerialNode::WriteRefNode(writer, name, record->_src);
			child->WriteAttr("lib", lib->GetComponentPath(nullptr));

			return child;
		}

		Record* RecordLib::LoadRecordRefFrom(SReader* reader)
		{
			if (reader == nullptr)
				return nullptr;

			SerialNode* node;
			const SReader::ValueDesc* desc = reader->ReadAttr("lib");
			if (SerialNode::ReadRefNodeFrom(reader, &node) && desc)
			{
				std::string path;
				desc->CastTo(&path);
				const auto lib = lsl::StaticCast<RecordLib*>(reader->GetRoot()->AbsoluteFindComponent(path));

				return lib->FindRecordBySrc(node, lib);
			}

			return nullptr;
		}

		Record* RecordLib::LoadRecordRef(SReader* reader, const std::string& name)
		{
			if (SReader* child = reader->ReadValue(name.c_str()))
				return LoadRecordRefFrom(child);

			return nullptr;
		}

		Record* RecordLib::CreateRecord(const Record::Desc& desc)
		{
			return new Record(desc);
		}

		void RecordLib::DestroyRecord(Record* record)
		{
			delete record;
		}

		RecordNode* RecordLib::CreateNode(const Desc& desc)
		{
			return new RecordNode(desc);
		}

		void RecordLib::DestroyNode(RecordNode* node)
		{
			delete node;
		}

		SerialNode* RecordLib::CreateSrc(const std::string& name, RecordNode* parent, bool node) const
		{
			SerialNode* srcParent = parent ? parent->GetSrc() : _rootSrc;

			SerialNode* src = srcParent->GetElements().Add(name);
			if (node)
				src->AddAttribute(SerialFile::cFolder.c_str(), true);

			return src;
		}

		void RecordLib::DestroySrc(SerialNode* src, RecordNode* parent) const
		{
			SerialNode* srcParent = parent ? parent->GetSrc() : _rootSrc;

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

		Record* RecordLib::FindRecordBySrc(SerialNode* src, RecordNode* curNode)
		{
			for (const auto iter : curNode->GetRecordList())
				if (iter->_src == src)
					return iter;

			for (const auto iter : curNode->GetNodeList())
				if (Record* record = FindRecordBySrc(src, iter))
					return record;

			return nullptr;
		}

		Record* RecordLib::GetOrCreateRecord(const std::string& name)
		{
			StringList stringList;
			DevideStr(name.begin(), name.end(), stringList);

			RecordNode* node = this;
			for (auto iter = stringList.begin(); iter != stringList.end();)
			{
				std::string str = *iter;
				++iter;

				if (iter != stringList.end())
				{
					RecordNode* newNode = node->FindNode(str);
					if (newNode == nullptr)
						newNode = node->AddNode(str);
					node = newNode;
				}
				else
				{
					Record* record = node->FindRecord(str);
					if (record == nullptr)
						record = node->AddRecord(str);

					return record;
				}
			}

			LSL_ASSERT(false);

			return nullptr;
		}

		void RecordLib::SetName(const std::string& value)
		{
			_MyBase::SetName(value);
			Component::SetName(value);
		}
	}
}
