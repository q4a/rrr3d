#ifndef IO_TYPES
#define IO_TYPES

#include "lslCommon.h"
#include "lslCollection.h"
#include "lslUtility.h"
#include "lslObject.h"

namespace lsl
{

template<class _Resource> class IOResource
{
public:
	typedef _Resource Resource;	
public:
	virtual ~IOResource() {}

	virtual void LoadFromStream(Resource& outData, std::istream& stream) = 0;
	virtual void SaveToStream(const Resource& inData, std::ostream& stream) = 0;
};

//Поддерживаемые операторы ОС
//..\\ сдвиг на каталог назад
class FileSystem
{
private:
	static std::auto_ptr<FileSystem> _instance;
public:
	enum OpenMode {omText, omBinary};
	
	static const DWORD cAppend   = 0x1 << 0;
	static const DWORD cTruncate = 0x1 << 1;

	static FileSystem* GetInstance();
	static void Release();
private:
	std::wstring _appPath;

	template<class _T> std::basic_istream<_T, std::char_traits<_T>>* NewInStream(const std::string& fileName, OpenMode openMode, DWORD flags);
	template<class _T> std::basic_ostream<_T, std::char_traits<_T>>* NewOutStream(const std::string& fileName, OpenMode openMode, DWORD flags);
public:
	FileSystem(const std::wstring& appPath);	

	std::istream* NewInStream(const std::string& fileName, OpenMode openMode, DWORD flags);
	std::wistream* NewInStreamW(const std::string& fileName, OpenMode openMode, DWORD flags);

	std::ostream* NewOutStream(const std::string& fileName, OpenMode openMode, DWORD flags);
	std::wostream* NewOutStreamW(const std::string& fileName, OpenMode openMode, DWORD flags);

	void FreeStream(std::ios_base* stream);	

	bool FileExists(const std::string& fileName);

	const std::wstring& appPath() const;
};

class ResourcesTraits
{
	friend class Resource;
protected:
	void SetItemResTraits(Resource* item, ResourcesTraits* value);
	virtual void OnAddRefItem(Resource* item) {}
	virtual void OnReleaseItem(Resource* item) {}
};

class FileResource;

class ResourceUser: public virtual lsl::Object
{
public:
	virtual void OnInitResource(Resource* sender) = 0;
	virtual void OnFreeResource(Resource* sender) = 0;
};

class Resource: public CollectionItem
{
	friend ResourcesTraits;
private:
	typedef CollectionItem _MyBase;
public:
	typedef lsl::Container<ResourceUser*> UserList;
private:
	ResourcesTraits* _resTraits;
	bool _init;
	bool _dynamic;
protected:
	//Выделение ресурса
	virtual void DoInit() = 0;
	//Освобождение ресурса
	virtual void DoFree() = 0;
	//Обновление ресурса
	virtual void DoUpdate() = 0;
public:
	Resource();
	virtual ~Resource();

	virtual void AddRef() const;
	virtual unsigned Release() const;

	//Инициализация ресурса
	void Init();
	//Освобождение занятого ресурса
	void Free();
	//Полная перезагрузка ресурса
	void Reload();
	//Обновление ресурса без пересоздания. Обычно загрузка данных из внешнего источника либо обновление состояния из-за изменившихся данных
	void Update();
	
	//Состояние инициализации
	bool IsInit() const;

	ResourcesTraits* GetResTraits();

	//Свойство "динамичность", ресурс обновляется только по требованию, иначе вызов Update происходит после инициализации
	bool GetDynamic() const;
	void SetDynamic(bool value);

	UserList userList;
};

//Тип ресурса который имеет методы загрузки и сохранения в файл, путь к файлу запоминается. Содержимое ресурса связано с содержимым файла, поэтому загрузка осуществляется в процессе инициализации. Т.к. путь к файлу запоминается, загрузка(или иначе инциализация) может откладываться до момента начала использования ресурса, а после завершения его использования может осуществляться освобождение занятой памяти т.д.
class FileResource: public Resource
{
private:
	std::string _fileName;
protected:
	virtual void DoLoadFromStream(std::istream& stream, const std::string& fileExt);	
public:
	void LoadFromStream(std::istream& stream, const std::string& fileExt);
	void SaveToStream(std::ostream& stream, const std::string& fileExt);
	void LoadFromFile(const std::string& fileName);
	void SaveToFile(const std::string& fileName);
	void Load();

	const std::string& GetFileName() const;
	void SetFileName(const std::string& value);
};

class BufferResource: public FileResource
{
private:
	char* _data;
protected:
	virtual void DoInit();
	virtual void DoFree();
	virtual void DoUpdate();
public:
	virtual ~BufferResource();

	void CopyDataFrom(const void* source);
	void CopyDataTo(void* target, int offset = 0, int size = -1) const;

	char* GetData();
	const char* GetData() const;
	virtual unsigned GetSize() const = 0;
};

class BinaryResource: public BufferResource
{
private:
	unsigned _size;
protected:
	virtual void DoLoadFromStream(std::istream& stream, const std::string& fileExt);
public:
	BinaryResource();

	virtual unsigned GetSize() const;
	void SetSize(unsigned value);
};

template<class _Resource, class _IdType, class _Arg, class _ArgThis> class ResourceCollection: public ComCollection<_Resource, _IdType, _Arg, _ArgThis>, public ResourcesTraits
{
private:
	typedef ComCollection<_Resource, _IdType, _Arg, _ArgThis> _MyBase;
protected:
	virtual void InsertItem(const Value& value);
};




template<class _Resource, class _IdType, class _Arg, class _ArgThis> void ResourceCollection<_Resource, _IdType, _Arg, _ArgThis>::InsertItem(const Value& value)
{
	_MyBase::InsertItem(value);

	SetItemResTraits(value, this);
}




inline std::wstring GetAppPath()
{
	wchar_t buf[1024];
	unsigned size = GetModuleFileNameW(NULL, buf, 1024);
	
	std::wstring res(buf, size);
	lsl::ExtractFilePath(res, res);

	return res;
}

inline std::wstring GetAppFilePath(const std::string& localFileName)
{
	return GetAppPath() + ConvertStrAToW(localFileName);
}

}

#endif