#ifndef LSL_VARIANT
#define LSL_VARIANT

#include "lslCommon.h"
#include "lslMath.h"

namespace lsl
{

class Variant
{
public:
	enum Type {vtVoid = 0, vtInt, vtUInt, vtFloat, vtDouble, vtBool, vtChar, cTypeForce = 1000};
	static const unsigned cTypeEnd = 7;
private:
	static const unsigned cTypeSize[cTypeEnd];
protected:
	static unsigned GetTypeSize(Type type);
private:
	//Данные
	int _buf[16];
	void* _data;
	//Размер в байтах, если:
	//=0  - значение value = 0
	//>0  - массив размером count, причем count - это количество элементов valType, если valType = vtVoid, то количество байтов
	unsigned _count;
	//Тип значения
	Type _type;
protected:
	//Копирование данных
	void AssignData(const void* value, Type type, unsigned count, unsigned oldTypeSize, unsigned newTypeSize);
	void AssignData(const void* value, Type type, unsigned count);
	void AssignData(const Variant& value);
	template<class _Type> void AssignData(const _Type* value, unsigned count);
	void FreeData();

	//Приведение типа
	template<class _Type> const _Type* Cast() const;
	template<class _Type> _Type* Cast();

	//Преобразование из _Type в Type
	template<class _Type> Type GetTypeOf() const;
	template<> Type GetTypeOf<void>() const;
	template<> Type GetTypeOf<int>() const;
	template<> Type GetTypeOf<unsigned>() const;
	template<> Type GetTypeOf<float>() const;
	template<> Type GetTypeOf<double>() const;
	template<> Type GetTypeOf<bool>() const;
	template<> Type GetTypeOf<char>() const;
public:
	Variant();
	Variant(const Variant& value);
	Variant(const void* value, unsigned size);
	Variant(const int* value, unsigned count);
	Variant(const unsigned* value, unsigned count);
	Variant(const float* value, unsigned count);
	Variant(const double* value, unsigned count);
	Variant(const bool* value, unsigned count);
	Variant(const char* value, unsigned count);
	//
	Variant(const int& value);
	Variant(const unsigned& value);
	Variant(const float& value);
	Variant(const double& value);
	Variant(const bool& value);
	Variant(const char& value);
	//
	~Variant();

	//Проверка типа
	template<class _Type> bool CheckType() const;
	bool CheckType(Type type) const;
	template<class _Type> const _Type* IsType() const;
	//Запись в тип
	template<class _Type> _Type& AsType(unsigned index = 0);
	//Преобразование типа
	template<class _Type> void Convert(_Type* outVal, unsigned index = 0, unsigned count = 1) const;
	template<class _Type> _Type Convert(unsigned index = 0) const;

	//Чтение состояния
	const void* GetData() const;
	void SetData(const void* value, unsigned size);
	unsigned GetSize() const;
	unsigned GetCount() const;
	Type GetType() const;

	//Опреаторы
	Variant& operator=(const Variant& value);
	bool operator==(const Variant& value) const;
	bool operator!=(const Variant& value) const;
};

class VariantVec: private Variant
{
private:
	typedef Variant _MyBase;

	static const unsigned cMyTypeEnd = 4;
	static const unsigned cMyTypeSize[cMyTypeEnd];

	static unsigned GetTypeSize(Type type);
public:
	static const Type vtVec2 = (Type)(_MyBase::cTypeEnd + 0);
	static const Type vtVec3 = (Type)(_MyBase::cTypeEnd + 1);
	static const Type vtVec4 = (Type)(_MyBase::cTypeEnd + 2);
	static const Type vtMatrix = (Type)(_MyBase::cTypeEnd + 3);

	static const unsigned cTypeEnd = _MyBase::cTypeEnd + cMyTypeEnd;
protected:
	void AssignData(const void* value, Type type, unsigned count);
	void AssignData(const VariantVec& value);
	template<class _Type> void AssignData(const _Type* value, unsigned count);

	template<class _Type> Type GetTypeOf() const;
	template<> Type GetTypeOf<D3DXVECTOR2>() const;
	template<> Type GetTypeOf<D3DXVECTOR3>() const;
	template<> Type GetTypeOf<D3DXVECTOR4>() const;
	template<> Type GetTypeOf<D3DXMATRIX>() const;
public:
	VariantVec();
	VariantVec(const VariantVec& value);
	VariantVec(const void* value, unsigned size);
	VariantVec(const int* value, unsigned count);
	VariantVec(const unsigned* value, unsigned count);
	VariantVec(const float* value, unsigned count);
	VariantVec(const double* value, unsigned count);
	VariantVec(const bool* value, unsigned count);
	VariantVec(const char* value, unsigned count);
	//
	VariantVec(const D3DXVECTOR2* value, unsigned count);
	VariantVec(const D3DXVECTOR3* value, unsigned count);
	VariantVec(const D3DXVECTOR4* value, unsigned count);
	VariantVec(const D3DXCOLOR* value, unsigned count);
	VariantVec(const D3DXMATRIX* value, unsigned count);
	//
	VariantVec(const int& value);
	VariantVec(const unsigned& value);
	VariantVec(const float& value);
	VariantVec(const double& value);
	VariantVec(const bool& value);
	VariantVec(const char& value);
	//
	VariantVec(const D3DXVECTOR2& value);
	VariantVec(const D3DXVECTOR3& value);
	VariantVec(const D3DXVECTOR4& value);
	VariantVec(const D3DXCOLOR& value);
	VariantVec(const D3DXMATRIX& value);

	using _MyBase::CheckType;
	using _MyBase::IsType;
	using _MyBase::AsType;
	using _MyBase::Convert;	

	using _MyBase::GetData;
	void SetData(const void* value, unsigned size);
	unsigned GetSize() const;
	using _MyBase::GetCount;
	using _MyBase::GetType;

	VariantVec& operator=(const VariantVec& value);
	bool operator==(const VariantVec& value) const;
	bool operator!=(const VariantVec& value) const;
};




inline Variant::Variant(): _data(0)
{
}

inline Variant::Variant(const Variant& value): _data(0)
{
	AssignData(value);
}

inline Variant::Variant(const void* value, unsigned size): _data(0)
{
	AssignData(value, size);
}

inline Variant::Variant(const int* value, unsigned count): _data(0)
{
	AssignData(value, count);
}

inline Variant::Variant(const unsigned* value, unsigned count): _data(0)
{
	AssignData(value, count);
}

inline Variant::Variant(const float* value, unsigned count): _data(0)
{
	AssignData(value, count);
}

inline Variant::Variant(const double* value, unsigned count): _data(0)
{
	AssignData(value, count);
}

inline Variant::Variant(const bool* value, unsigned count): _data(0)
{
	AssignData(value, count);
}

inline Variant::Variant(const char* value, unsigned count): _data(0)
{
	AssignData(value, count);
}

inline Variant::Variant(const int& value): _data(0)
{
	AssignData(&value, 1);
}

inline Variant::Variant(const unsigned& value): _data(0)
{
	AssignData(&value, 1);
}

inline Variant::Variant(const float& value): _data(0)
{
	AssignData(&value, 1);
}

inline Variant::Variant(const double& value): _data(0)
{
	AssignData(&value, 1);
}

inline Variant::Variant(const bool& value): _data(0)
{
	AssignData(&value, 1);
}

inline Variant::Variant(const char& value): _data(0)
{
	AssignData(&value, 1);
}

inline Variant::~Variant()
{
	FreeData();
}

inline unsigned Variant::GetTypeSize(Type type)
{
	return cTypeSize[type];
}

inline void Variant::AssignData(const void* value, Type type, unsigned count, unsigned oldTypeSize, unsigned newTypeSize)
{
	if (!value)
	{
		FreeData();
		return;
	}

	unsigned oldSize = oldTypeSize * _count;
	unsigned newSize = newTypeSize * count;

	LSL_ASSERT(newSize > 0 && newSize <= sizeof(_buf));

	//if (oldSize != newSize)
	//	_data = _data ? realloc(_data, newSize) : malloc(newSize);
	//memcpy(_buf, value, newSize);

	if (newTypeSize % 4 == 0)
	{
		int t = count * newTypeSize / 4;
		for (int i = 0; i < t; ++i)
			_buf[i] = ((const int*)value)[i];
	}
	else
		memcpy(_buf, value, newSize);

	_data = _buf;

	_type = type;
	_count = count;
}
	
inline void Variant::AssignData(const void* value, Type type, unsigned count)
{
	AssignData(value, type, count, _data ? cTypeSize[_type] : 0, cTypeSize[type]);
}

inline void Variant::AssignData(const Variant& value)
{
	AssignData(value._data, value._type, value._count);
}

template<class _Type> inline void Variant::AssignData(const _Type* value, unsigned count)
{
	AssignData(value, GetTypeOf<_Type>(), count);
}

inline void Variant::FreeData()
{
	if (_data)
	{
		//free(_data);
		_data = 0;
	}
}

template<class _Type> inline const _Type* Variant::Cast() const
{
	return reinterpret_cast<const _Type*>(_data);
}

template<class _Type> inline _Type* Variant::Cast()
{
	return reinterpret_cast<_Type*>(_data);
}

template<class _Type> inline Variant::Type Variant::GetTypeOf() const
{
	return vtVoid;
}

template<> inline Variant::Type Variant::GetTypeOf<void>() const
{
	return vtVoid;
}

template<> inline Variant::Type Variant::GetTypeOf<int>() const
{
	return vtInt;
}

template<> inline Variant::Type Variant::GetTypeOf<unsigned>() const
{
	return vtInt;
}

template<> inline Variant::Type Variant::GetTypeOf<float>() const
{
	return vtFloat;
}

template<> inline Variant::Type Variant::GetTypeOf<double>() const
{
	return vtDouble;
}

template<> inline Variant::Type Variant::GetTypeOf<bool>() const
{
	return vtBool;
}

template<> inline Variant::Type Variant::GetTypeOf<char>() const
{
	return vtChar;
}

template<class _Type> inline bool Variant::CheckType() const
{
	return _type == GetTypeOf<_Type>();
}

inline bool Variant::CheckType(Type type) const
{
	return type == _type;
}

template<class _Type> inline const _Type* Variant::IsType() const
{
	return CheckType<_Type>() ? Cast<_Type>() : 0;
}

template<class _Type> inline _Type& Variant::AsType(unsigned index)
{
	_Type* res = IsType<_Type>(index);

	LSL_ASSERT(res);

	return res[index];
}

template<class _Type> inline void Variant::Convert(_Type* outVal, unsigned index, unsigned count) const
{
	LSL_ASSERT(outVal && _count >= index + count);

	for (unsigned i = 0; i < count; ++i)
		switch (_type)
		{
		case vtInt:
			outVal[i] = static_cast<const _Type>(IsType<int>()[i + index]);
			break;

		case vtUInt:
			outVal[i] = static_cast<const _Type>(IsType<unsigned>()[i + index]);
			break;
		
		case vtFloat:
			outVal[i] = static_cast<const _Type>(IsType<float>()[i + index]);
			break;

		case vtDouble:
			outVal[i] = static_cast<const _Type>(IsType<double>()[i + index]);
			break;

		case vtBool:
			outVal[i] = static_cast<const _Type>(IsType<bool>()[i + index]);
			break;

		case vtChar:
			outVal[i] = static_cast<const _Type>(IsType<char>()[i + index]);
			break;

		default:
			LSL_ASSERT(false);
		}
}

template<class _Type> inline _Type Variant::Convert(unsigned index) const
{
	_Type res;
	Convert(&res, index, 1);
	return res;
}

inline const void* Variant::GetData() const
{
	return _data;
}

inline void Variant::SetData(const void* value, unsigned size)
{
	AssignData(value, size);
}

inline unsigned Variant::GetSize() const
{
	return cTypeSize[_type] * _count;
}

inline unsigned Variant::GetCount() const
{
	return _count;
}

inline Variant::Type Variant::GetType() const
{
	return _type;
}

inline Variant& Variant::operator=(const Variant& value)
{
	AssignData(value);

	return *this;
}

inline bool Variant::operator==(const Variant& value) const
{
	return _type == value._type && _count == value._count && memcmp(_data, value._data, GetSize());
}

inline bool Variant::operator!=(const Variant& value) const
{
	return !operator==(value);
}




inline VariantVec::VariantVec()
{
}

inline VariantVec::VariantVec(const VariantVec& value)
{
	AssignData(value);
}

inline VariantVec::VariantVec(const void* value, unsigned size)
{
	AssignData(value, size);
}

inline VariantVec::VariantVec(const int* value, unsigned count)
{
	AssignData(value, count);
}

inline VariantVec::VariantVec(const unsigned* value, unsigned count)
{
	AssignData(value, count);
}

inline VariantVec::VariantVec(const float* value, unsigned count)
{
	AssignData(value, count);
}

inline VariantVec::VariantVec(const double* value, unsigned count)
{
	AssignData(value, count);
}

inline VariantVec::VariantVec(const bool* value, unsigned count)
{
	AssignData(value, count);
}

inline VariantVec::VariantVec(const char* value, unsigned count)
{
	AssignData(value, count);
}

inline VariantVec::VariantVec(const D3DXVECTOR2* value, unsigned count)
{
	AssignData(value, count);
}


inline VariantVec::VariantVec(const D3DXVECTOR3* value, unsigned count)
{
	AssignData(value, count);
}

inline VariantVec::VariantVec(const D3DXVECTOR4* value, unsigned count)
{
	AssignData(value, count);
}

inline VariantVec::VariantVec(const D3DXCOLOR* value, unsigned count)
{
	AssignData<D3DXVECTOR4>(reinterpret_cast<const D3DXVECTOR4*>(value), count);
}

inline VariantVec::VariantVec(const D3DXMATRIX* value, unsigned count)
{
	AssignData(value, count);
}

inline VariantVec::VariantVec(const int& value)
{
	AssignData(&value, 1);
}

inline VariantVec::VariantVec(const unsigned& value)
{
	AssignData(&value, 1);
}

inline VariantVec::VariantVec(const float& value)
{
	AssignData(&value, 1);
}

inline VariantVec::VariantVec(const double& value)
{
	AssignData(&value, 1);
}

inline VariantVec::VariantVec(const bool& value)
{
	AssignData(&value, 1);
}

inline VariantVec::VariantVec(const char& value)
{
	AssignData(&value, 1);
}

inline VariantVec::VariantVec(const D3DXVECTOR2& value)
{
	AssignData(&value, 1);
}


inline VariantVec::VariantVec(const D3DXVECTOR3& value)
{
	AssignData(&value, 1);
}

inline VariantVec::VariantVec(const D3DXVECTOR4& value)
{
	AssignData(&value, 1);
}

inline VariantVec::VariantVec(const D3DXCOLOR& value)
{
	AssignData(reinterpret_cast<const D3DXVECTOR4*>(&value), 1);
}

inline VariantVec::VariantVec(const D3DXMATRIX& value)
{
	AssignData(&value, 1);
}

inline unsigned VariantVec::GetTypeSize(Type type)
{
	return type < _MyBase::cTypeEnd ? _MyBase::GetTypeSize(type) : cMyTypeSize[type - _MyBase::cTypeEnd];
}

inline void VariantVec::AssignData(const void* value, Type type, unsigned count)
{
	_MyBase::AssignData(value, type, count, GetData() ? GetTypeSize(GetType()) : 0, value ? GetTypeSize(type) : 0);
}

inline void VariantVec::AssignData(const VariantVec& value)
{
	AssignData(value.GetData(), value.GetType(), value.GetCount());
}

template<class _Type> inline void VariantVec::AssignData(const _Type* value, unsigned count)
{
	AssignData(value, GetTypeOf<_Type>(), count);
}

template<class _Type> inline VariantVec::Type VariantVec::GetTypeOf() const
{
	return _MyBase::GetTypeOf<_Type>();
}

template<> inline VariantVec::Type VariantVec::GetTypeOf<D3DXVECTOR2>() const
{
	return vtVec2;
}

template<> inline VariantVec::Type VariantVec::GetTypeOf<D3DXVECTOR3>() const
{
	return vtVec3;
}

template<> inline VariantVec::Type VariantVec::GetTypeOf<D3DXVECTOR4>() const
{
	return vtVec4;
}

template<> inline VariantVec::Type VariantVec::GetTypeOf<D3DXMATRIX>() const
{
	return vtMatrix;
}

inline void VariantVec::SetData(const void* value, unsigned size)
{
	AssignData(value, size);
}

inline unsigned VariantVec::GetSize() const
{
	return GetTypeSize(GetType()) * GetCount();
}

inline VariantVec& VariantVec::operator=(const VariantVec& value)
{
	AssignData(value);

	return *this;
}

inline bool VariantVec::operator==(const VariantVec& value) const
{
	return _MyBase::operator==(value);
}

inline bool VariantVec::operator!=(const VariantVec& value) const
{
	return _MyBase::operator!=(value);
}

}

#endif