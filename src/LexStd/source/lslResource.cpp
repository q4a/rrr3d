#include "stdafx.h"

#include "lslResource.h"
#include "lslUtility.h"
#include <fstream>
#include <cassert>

namespace lsl
{

std::auto_ptr<FileSystem> FileSystem::_instance;




FileSystem::FileSystem(const std::wstring& appPath): _appPath(appPath)
{
}

FileSystem* FileSystem::GetInstance()
{
	if (!_instance.get())
		_instance.reset(new FileSystem(GetAppPath()));

	return _instance.get();
}

void FileSystem::Release()
{
	_instance.reset();
}

template<class _T> std::basic_istream<_T, std::char_traits<_T>>* FileSystem::NewInStream(const std::string& fileName, OpenMode openMode, DWORD flags)
{
	std::ios_base::open_mode ioOpMode = std::ios_base::in;
	switch (openMode)
	{
	case omBinary:
		ioOpMode += std::ios::binary;
		break;
	}
	ioOpMode |= (flags & cAppend) ? std::ios::app : 0;
	ioOpMode |= (flags & cTruncate) ? std::ios::trunc : 0;	

	std::wstring path = GetAppFilePath(fileName);

	std::basic_ifstream<_T, std::char_traits<_T>>* fs = new std::basic_ifstream<_T, std::char_traits<_T>>(path.c_str(), ioOpMode);
	LSL_ASSERT(fs);

	if (fs->fail())	
	{
		fs->clear();
		fs->open(fileName.c_str(), ioOpMode);
		if (fs->fail())
		{
			lsl::SafeDelete(fs);			
			throw EUnableToOpen(fileName);
		}		
	}

	return fs;
}

template<class _T> std::basic_ostream<_T, std::char_traits<_T>>* FileSystem::NewOutStream(const std::string& fileName, OpenMode openMode, DWORD flags)
{
	std::ios_base::open_mode ioOpMode = std::ios_base::out;
	switch (openMode)
	{
	case omBinary:
		ioOpMode += std::ios::binary;
		break;
	}

	ioOpMode |= (flags & cAppend) ? std::ios::app : 0;
	ioOpMode |= (flags & cTruncate) ? std::ios::trunc : 0;

	std::wstring path = GetAppFilePath(fileName);

	std::basic_ofstream<_T, std::char_traits<_T>>* fs = new std::basic_ofstream<_T, std::char_traits<_T>>(path.c_str(), ioOpMode);
	LSL_ASSERT(fs);

	if (fs->fail())	
	{
		fs->clear();
		fs->open(fileName.c_str(), ioOpMode);
		if (fs->fail())
		{
			lsl::SafeDelete(fs);
			throw Error(std::string("Unable to write file ") + fileName);
		}		
	}

	return fs;
}

std::istream* FileSystem::NewInStream(const std::string& fileName, OpenMode openMode, DWORD flags)
{
	return NewInStream<std::istream::_Ctype::_Elem>(fileName, openMode, flags);
}

std::wistream* FileSystem::NewInStreamW(const std::string& fileName, OpenMode openMode, DWORD flags)
{
	return NewInStream<std::wistream::_Ctype::_Elem>(fileName, openMode, flags);
}

std::ostream* FileSystem::NewOutStream(const std::string& fileName, OpenMode openMode, DWORD flags)
{
	return NewOutStream<std::ostream::_Ctype::_Elem>(fileName, openMode, flags);
}

std::wostream* FileSystem::NewOutStreamW(const std::string& fileName, OpenMode openMode, DWORD flags)
{
	return NewOutStream<std::wostream::_Ctype::_Elem>(fileName, openMode, flags);
}

void FileSystem::FreeStream(std::ios_base* stream)
{
	if (stream)	
		delete stream;
}

bool FileSystem::FileExists(const std::string& fileName)
{
	DWORD dwAttrib = GetFileAttributesW(GetAppFilePath(fileName).c_str());

	return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

const std::wstring& FileSystem::appPath() const
{
	return _appPath;
}




void ResourcesTraits::SetItemResTraits(Resource* item, ResourcesTraits* value)
{
	item->_resTraits = value;
}




Resource::Resource(): _resTraits(0), _init(false), _dynamic(false)
{
}

Resource::~Resource()
{
	assert(!_init);
}

void Resource::AddRef() const
{
	_MyBase::AddRef();

	if (_resTraits)
		_resTraits->OnAddRefItem(const_cast<Resource*>(this));
}

unsigned Resource::Release() const
{
	unsigned ref = _MyBase::Release();

	if (_resTraits)
		_resTraits->OnReleaseItem(const_cast<Resource*>(this));
	
	return ref;
}

void Resource::Init()
{
	if (!_init)
	{
		_init = true;

		DoInit();
		
		if (!_dynamic)
			DoUpdate();

		for (UserList::Position pos = userList.First(); ResourceUser** iter = userList.Current(pos); userList.Next(pos))
			(*iter)->OnInitResource(this);
	}
}

void Resource::Free()
{
	if (_init)
	{
		_init = false;

		for (UserList::Position pos = userList.First(); ResourceUser** iter = userList.Current(pos); userList.Next(pos))
			(*iter)->OnFreeResource(this);

		DoFree();
	}
}

void Resource::Reload()
{
	Free();
	Init();
}

void Resource::Update()
{
	LSL_ASSERT(_init);

	DoUpdate();
}

bool Resource::IsInit() const
{
	return _init;
}

ResourcesTraits* Resource::GetResTraits()
{
	return _resTraits;
}

bool Resource::GetDynamic() const
{
	return _dynamic;
}

void Resource::SetDynamic(bool value)
{
	_dynamic = value;
}




void FileResource::DoLoadFromStream(std::istream& stream, const std::string& fileExt)
{
	//свидетельствует о том что загрузчик не существует
	assert(false);
}

void FileResource::LoadFromStream(std::istream& stream, const std::string& fileExt)
{
	DoLoadFromStream(stream, fileExt);
}

void FileResource::SaveToStream(std::ostream& stream, const std::string& fileExt)
{
	LSL_ASSERT(IsInit());
	//
}

void FileResource::LoadFromFile(const std::string& fileName)
{
	_fileName = fileName;
	Load();
}

void FileResource::SaveToFile(const std::string& fileName)
{
	//
}

void FileResource::Load()
{
	std::istream* inputStream = FileSystem::GetInstance()->NewInStream(_fileName, FileSystem::omBinary, 0);
	try
	{
		std::string fileExt;
		ExtractFileExt(fileExt, _fileName);
		LoadFromStream(*inputStream, fileExt);
	}
	LSL_FINALLY(FileSystem::GetInstance()->FreeStream(inputStream);)
}

const std::string& FileResource::GetFileName() const
{
	return _fileName;
}

void FileResource::SetFileName(const std::string& value)
{
	_fileName = value;
}




BufferResource::~BufferResource()
{
	Free();
}

void BufferResource::DoInit()
{
	LSL_ASSERT(GetSize() != 0);

	_data = new char[GetSize()];
}

void BufferResource::DoFree()
{
	delete[] _data;
}

void BufferResource::DoUpdate()
{
	//Nothing
}

void BufferResource::CopyDataFrom(const void* source)
{
	Init();
	memmove(_data, source, GetSize());
	Update();
}

void BufferResource::CopyDataTo(void* target, int offset, int size) const
{
	if (size == -1)
		size = GetSize();

	memmove(target, _data + offset, size);
}

char* BufferResource::GetData()
{
	return _data;
}

const char* BufferResource::GetData() const
{
	return _data;
}




BinaryResource::BinaryResource(): _size(0)
{
}

void BinaryResource::DoLoadFromStream(std::istream& stream, const std::string& fileExt)
{
	stream.seekg(0, std::ios::end);
	SetSize(static_cast<unsigned>(stream.tellg()));
	stream.seekg(0, std::ios::beg);
	Init();
	stream.read(GetData(), GetSize());
	Update();

	LSL_ASSERT(!stream.fail());
}

unsigned BinaryResource::GetSize() const
{
	return _size;
}

void BinaryResource::SetSize(unsigned value)
{
	if (_size != value)
	{
		_size = value;
		Free();
	}
}

}