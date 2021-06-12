//Есть идея сделать зависимый fixUp, т.е. сам reader(событие ему делегирует Serializable) определяет момент когда востанавилаются ссылки для его дочерей. Т.е. сделать все по аналогии с SReader::ReadXXX. Это нужно для точго чтобы управлять порядков восст. ссылок, на данный момент порядок фиксирован "с нижних уровней к верхним".

#ifndef LSL_SERIALIZATION
#define LSL_SERIALIZATION

#include "lslCommon.h"
#include "lslComponent.h"
#include "lslCollectionItem.h"
#include "lslSerializable.h"

namespace lsl
{

struct SIOTraits
{
public:
	enum ValType {vtUnknown, vtInt, vtUInt, vtFloat, vtDouble, vtBool, vtChar};
	
	struct ValueDesc
	{
	public:
		ValueDesc(): type(vtUnknown), value(0), count(0)
		{}
		ValueDesc(const void* pValue, ValType valType, int valCount): value(pValue), type(valType), count(valCount)
		{}
		//Фундаментальные типы
		ValueDesc(const char& val): value(&val), type(vtChar), count(1)
		{}
		ValueDesc(const int& val): value(&val), type(vtInt), count(1)
		{}
		ValueDesc(const unsigned int& val): value(&val), type(vtUInt), count(1)
		{}
		ValueDesc(const float& val): value(&val), type(vtFloat), count(1)
		{}
		ValueDesc(const double& val): value(&val), type(vtDouble), count(1)
		{}
		ValueDesc(const bool& val): value(&val), type(vtBool), count(1)
		{}
		//null terminated string
		ValueDesc(const char* val): value(val), type(vtChar), count(strlen(val))
		{}
		ValueDesc(const std::string& val): value(val.data()), type(vtChar), count(val.size())
		{}
		//Массивы
		ValueDesc(const int* val, int valCount): value(val), type(vtInt), count(valCount)
		{}
		ValueDesc(const unsigned* val, int valCount): value(val), type(vtUInt), count(valCount)
		{}
		ValueDesc(const float* val, int valCount): value(val), type(vtFloat), count(valCount)
		{}
		ValueDesc(const double* val, int valCount): value(val), type(vtDouble), count(valCount)
		{}

		//Преобразования
		const char* ToChar() const
		{
			return type == vtChar ? reinterpret_cast<const char*>(value) : 0;
		}
		const int* ToInt() const
		{
			return type == vtInt ? reinterpret_cast<const int*>(value) : 0;
		}
		const unsigned int* ToUInt() const
		{
			return type == vtUInt ? reinterpret_cast<const unsigned int*>(value) : 0;
		}
		const float* ToFloat() const
		{
			return type == vtFloat ? reinterpret_cast<const float*>(value) : 0;
		}
		const double* ToDouble() const
		{
			return type == vtDouble ? reinterpret_cast<const double*>(value) : 0;
		}
		const bool* ToBool() const
		{
			return type == vtBool ? reinterpret_cast<const bool*>(value) : 0;
		}

		template<class _Type> void CastTo(_Type* outVal, int cnt = 1) const
		{
			//Выключить предупреждение о преобразовании из типа bool в _Type
			#pragma warning(disable : 4800)

			if (IsDummy())
				*outVal = static_cast<_Type>(0);

			if (cnt > count)
				throw lsl::Error("data is corrupted");

			for (int i = 0; i < cnt; ++i)
				switch (type)
				{
				case vtChar:
					outVal[i] = static_cast<_Type>(ToChar()[i]);
					break;

				case vtInt:
					outVal[i] = static_cast<_Type>(ToInt()[i]);
					break;

				case vtUInt:
					outVal[i] = static_cast<_Type>(ToUInt()[i]);
					break;

				case vtFloat:
					outVal[i] = static_cast<_Type>(ToFloat()[i]);
					break;
				
				case vtDouble:
					outVal[i] = static_cast<_Type>(ToDouble()[i]);
					break;

				case vtBool:
					outVal[i] = static_cast<_Type>(ToBool()[i]);
					break;
				}

			//Восстановить умолчание
			#pragma warning(default : 4800)
		}

		template<class _Type> void CastToFromInt(_Type* outVal, int cnt = 1) const
		{
			int* tmp = new int[cnt];
			CastTo<int>(tmp, cnt);
			for (int i = 0; i < cnt; ++i)			
				outVal[i] = static_cast<_Type>(tmp[i]);
			delete[] tmp;
		}

		template<> void CastTo<std::string>(std::string* outVal, int cnt) const
		{
			cnt = count;
			std::stringstream sstream;
			//Для булевых значений
			sstream.setf(std::ios_base::boolalpha);

			if (type == vtChar)		
				sstream.write(ToChar(), cnt);
			else
				for (int i = 0; i < cnt; ++i)
				{
					if (i > 0)
						sstream << " ";
					switch (type)
					{
					case vtInt:
						sstream << ToInt()[i];
						break;

					case vtUInt:
						sstream << ToUInt()[i];
						break;

					case vtFloat:
						sstream << ToFloat()[i];
						break;
					
					case vtDouble:
						sstream << ToDouble()[i];
						break;

					case vtBool:
						sstream << ToBool()[i];
						break;
					}
				}

			*outVal = sstream.str();
		}

		bool IsDummy() const
		{
			return (value == 0 || count == 0);
		}

		//ссылка на данные
		const void* value;
		//тип
		ValType type;
		//Размер в байтах, если:
		//=0  - значение value = 0
		//>0  - массив размером count, причем count - это количество элементов valType, если valType = vtUnknown, то количество байтов
		int count;		
	};

	class Value: public ValueDesc
	{
	private:
		void FreeMem()
		{
			//Снимаем модификатор const, которые означает неизменяемость данных для дескриптора(т.к. хранится указатель на них). В самом типе Value данные копируются
			if (value)
			{
				free(const_cast<void*>(value));
				value = 0;
				count = 0;
			}
		}

		template<class _Type> bool AnalizeStream(const std::string& str)
		{	
			FreeMem();
			std::stringstream stream(str);
			//Для булевых значений
			stream.setf(std::ios_base::boolalpha);

			_Type* pValue = 0;
			int cnt = 0;
			
			while (!stream.eof())
			{
				pValue = static_cast<_Type*>(realloc(pValue, (++cnt) * sizeof(_Type)));
				stream >> pValue[cnt - 1];
				//если произошла ошибка то тип неподходит
				if (stream.fail())
				{
					free(pValue);
					return false;
				}
			}
			
			value = pValue;
			count = cnt;
			return true;
		}
		template<> bool AnalizeStream<char>(const std::string& str)
		{
			Assign(ValueDesc(str));
			return true;
		}
	public:
		Value(const ValueDesc& desc): ValueDesc()
		{
			Assign(desc);
		}
		~Value()
		{
			FreeMem();
		}

		void Assign(const ValueDesc& desc)
		{
			FreeMem();

			count = desc.count;
			type = desc.type;

			if (count < 0)
				throw lsl::Error("data is corrupted 2");

			if (count == 0)
				value = 0;
			else
			{
				int size = (type == vtUnknown ? 1 : GetValTypeSize(type));
				int memSize = count * size;
				char* newMem = static_cast<char*>(malloc(memSize));
				memcpy(newMem, desc.value, memSize);
				value = newMem;
			}			
		}

		void AssignFromString(const std::string& str)
		{
			if (AnalizeStream<int>(str))
				type = vtInt;
			else
				if (AnalizeStream<unsigned int>(str))
					type = vtUInt;
				else
					if (AnalizeStream<float>(str))
						type = vtFloat;
					else
						if (AnalizeStream<double>(str))
							type = vtDouble;
						else
							if (AnalizeStream<bool>(str))
								type = vtBool;
							else
								if (AnalizeStream<char>(str))
									type = vtChar;
								else
									throw lsl::Error("Неизвестный тип данных");
		}
	};
protected:
	SIOTraits() {};
public:
	static int GetValTypeSize(ValType type);

	//Необходима ссылка на корневой компонент для fixUp
	virtual Component* GetRoot() = 0;
	//Необходима ссылка на корневой узел для разрешения прокси ссылок а также востановления прокси списков
	virtual SerialNode* GetRootNode() = 0;
};

class SWriter: public virtual SIOTraits
{
	friend SerialNode;
protected:
	SWriter();
public:
	//запись аттрибутов к текущему узлу, аттрибуты это параметры которые описывают значение(т.е. узел) и их как правило получают ещё до загрузки значения
	virtual void WriteAttr(const char* name, const ValueDesc& desc) = 0;
	virtual void SetVal(const ValueDesc& desc) = 0;
	//В один узел может быть сохранен только один Serializable (необходимо для правильного разрешения прокси ссылок и вост. прокси листов)
	virtual void SaveSerializable(Serializable* value) = 0;

	//новый узел
	virtual SWriter* NewDummyNode(const char* name) = 0;
	//
	virtual SWriter* WriteValue(const char* name, const ValueDesc& desc) = 0;
	//более удобные формы записи
	SWriter* WriteValue(const char* name, const int* value, unsigned count);
	SWriter* WriteValue(const char* name, const unsigned* value, unsigned count);
	SWriter* WriteValue(const char* name, const float* value, unsigned count);
	SWriter* WriteValue(const char* name, const double* value, unsigned count);
	//
	SWriter* WriteValue(const char* name, Serializable* value);
	//запись ссылок
	//В виде обычного узла со значением
	SWriter* WriteRef(const char* name, const Component* component);
	//В виде узла с аттрибутом item и значением collection
	SWriter* WriteRef(const char* name, const CollectionItem* item);
};

class SReader: public virtual SIOTraits
{
	friend SerialNode;
private:
	typedef Serializable::FixUpName _FixUpName;
	typedef Serializable::FixUpNames _FixUpNames;
private:
	_FixUpNames _fixUpNames;

	bool ResolveFixUp(_FixUpName& inOutName);
	void FixUp();	
protected:
	SReader();
public:
	//Операции с узлом
	//чтение аттрибутов узла
	virtual const ValueDesc* ReadAttr(const char* name) = 0;
	
	//Запрос на восстановление ссылки:
	//path - путь до искомого компонента, включая его имя
	//collItem - имя элемента коллекции, указывается если ищется
	//target - вызвавший узел, отправитель запроса. Он же будет получателем
	//target != 0 - отложенный fixUp; 
	//target == 0 - fixUp в месте вызова, fixUpName не должен равняться нулю
	//fixUpName - информация о восстановлении
	bool AddFixUp(const std::string& path, const std::string& collItem, Serializable* target, _FixUpName* fixUpName);
	//Чтение ссылки из текущего узла и добавление её в FixUp, см. AddFixUp
	bool AddFixUp(bool collItem, Serializable* target, _FixUpName* fixUpName);

	//Работа с текущим узлом
	//загрузка Serializable из текущего узла
	virtual void LoadSerializable(Serializable* value) = 0;
	//имя узла
	virtual const std::string& GetMyName() const = 0;
	//значение узла
	virtual const ValueDesc& GetVal() const = 0;
	//Чтение ссылки
	bool GetRef(bool collItem, _FixUpName& fixUpName);

	//Операции с дочерними узлами
	//чтение узла, если его не существует то возвр. 0
	virtual SReader* ReadValue(const char* name) = 0;
	//вспомогательные операции
	//если операция чтение не удалась то возвр. 0 (даже если сущ. node)
	SReader* ReadValue(const char* name, ValueDesc value);
	SReader* ReadValue(const char* name, std::string& value);
	SReader* ReadValue(const char* name, int* value, unsigned count);
	SReader* ReadValue(const char* name, unsigned* value, unsigned count);
	SReader* ReadValue(const char* name, float* value, unsigned count);
	SReader* ReadValue(const char* name, double* value, unsigned count);
	//
	SReader* ReadValue(const char* name, Serializable* value);
	//чтение ссылки, содержащейся в узле с именем name, см. AddFixUp
	SReader* ReadRef(const char* name, bool collItem, Serializable* target, _FixUpName* fixUpName);

	//навигация по узлам, для динамического чтения
	virtual SReader* GetOwnerValue() = 0;
	//Первое(в порядке чтения, т.е. добавления в файл при записи) дочернее значение
	virtual SReader* FirstChildValue() = 0;
	//Предыдущее и следующее значение(в порядке чтения) из той иерархии что и сам узел
	virtual SReader* PrevValue() = 0;
	virtual SReader* NextValue() = 0;
};

//Защищенное наследование, в котором октрывается доступ к ссылочному классу Object
class SerialNode: protected Component, public virtual Object, public SWriter, public SReader
{
	//
	friend class SerialNodes;
private:
	typedef Component _MyBase;
public:
	typedef std::map<std::string, Value*> Attributes;
	typedef SerialNodes Elements;

	static SWriter* WriteRefNode(SWriter* writer, const std::string& name, SerialNode* node);
	static bool ReadRefNodeFrom(SReader* reader, SerialNode** outNode);
	static SReader* ReadRefNode(SReader* reader, const std::string& name, SerialNode** outNode);
private:
	Value _value;
	Attributes _attributes;
	Elements* _elements;

	Serializable* _linkSer;
	Serializable* _masterSer;
	SerialNode* _masterNode;
	int _linkSerRefCnt;
	int _proxyLoad;

	unsigned _beginSave;
	unsigned _beginLoad;

	SerialNode* FindLinkSer(Serializable* value);
	void ResolveProxyRef();
	//Начинается с нижних уровней и идет к верхним
	//Вообщем в каком порядке ссылки записаны в таком порядке и будут вызываться FixUp-ы
	void OnFixUp();

	void AddRefSerLink();
	void ReleaseSerLink();
	void BeginProxyLoad();
	void EndProxyLoad();
	bool IsProxyLoad() const;
protected:
	SerialNode();
	virtual ~SerialNode();

	virtual void SaveSerializable(Serializable* value);
	virtual void WriteAttr(const char* name, const ValueDesc& desc);
	virtual void SetVal(const ValueDesc& desc);
	virtual SWriter* NewDummyNode(const char* name);
	virtual SWriter* WriteValue(const char* name, const ValueDesc& desc);

	virtual void LoadSerializable(Serializable* value);
	virtual const ValueDesc* ReadAttr(const char* name);
	virtual const std::string& GetMyName() const;
	virtual const ValueDesc& GetVal() const;
	//
	virtual SReader* ReadValue(const char* name);
	//
	virtual SReader* GetOwnerValue();
	virtual SReader* FirstChildValue();
	virtual SReader* PrevValue();
	virtual SReader* NextValue();

	virtual SerialNode* IsRoot();
public:
	using _MyBase::MakeUniqueName;
	using _MyBase::GetComponentPath;
	using _MyBase::GetName;
	using _MyBase::SetName;

	SerialNode* FirstChild();
	SerialNode* PrevNode();
	SerialNode* NextNode();

	Value* AddAttribute(const char* name, const ValueDesc& desc);
	void DelAttribute(Attributes::iterator value);
	void ClearAttributes();
	void Clear();
	
	SerialNode* FindChild(const std::string& name);
	SerialNode* FindNode(const std::string& name);

	SWriter* BeginSave();
	void EndSave();
	SReader* BeginLoad();
	void EndLoad();

	//
	void Save(Serializable* root);
	void Load(Serializable* root);

	const ValueDesc& GetValue() const;
	void SetValue(const ValueDesc& value);
	void SetValue(const std::string& value);

	virtual lsl::Component* GetRoot();
	virtual SerialNode* GetRootNode();
	SerialNode* GetOwner();
	Elements& GetElements();	
	const Attributes& GetAttributes() const;	
};

class SerialNodes
{
private:
	typedef lsl::List<SerialNode*> _List;
	typedef CollectionTraits _MyBase;
public:
	typedef _List::iterator iterator;
	typedef _List::const_iterator const_iterator;	
private:
	_List _list;
	SerialNode* _owner;
public:
	SerialNodes(SerialNode* owner);
	virtual ~SerialNodes();

	SerialNode* Add(const std::string& name);
	void Delete(iterator iter);
	void Delete(SerialNode* value);
	void Clear();
	unsigned Count() const;

	iterator Find(SerialNode* node);
	SerialNode* Find(const std::string& name);

	iterator begin();
	iterator end();
	const_iterator begin() const;
	const_iterator end() const;

	SerialNode* GetOwner();
};

//Иерархия SerialNode изолирована от объектов Component(чтобы исключить в цепочке наличие классов отличных от SerialNode), однако полагаю будет полезно оставить возможность добавления RootNode, как корневого, в иерархию Component
class RootNode: public SerialNode
{
private:
	typedef SerialNode _MyBase;
protected:
	virtual SerialNode* IsRoot();
public:
	RootNode(const std::string& name, Component* owner);

	//using SerialNode::Save;
	//using SerialNode::Load;

	using Component::GetOwner;
	using Component::SetOwner;
};

class SerialFile
{
public:
	//Если имя аттрибута "parse", а значение "имя элемента", то значение элемента вставляется напрямую, без обработки
	static const std::string cParse;
	//Узел является каталогом
	static const std::string cFolder;
public:
	virtual void SaveNode(SerialNode& root, std::ostream& stream) = 0;
	virtual void LoadNode(SerialNode& root, std::istream& stream) = 0;
};

}

#endif