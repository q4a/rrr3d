#pragma once

namespace net
{

enum BitType {btByte = 0, btChar, btBool, btUint, btInt, btLong, btLongLong, btFloat, btDouble, btVec2, btVec3, btVec4, btStr, btData, cBitTypeEnd};

//128 bit value + 32 bit info
struct BitValue
{
	union
	{
		struct {
			unsigned _w1: 32;
			unsigned _w2: 32;
			unsigned _w3: 32;
			unsigned _w4: 32;

			unsigned size: 16;
			BitType type: 16;
		};

		BYTE byteVal;
		char charVal;
		bool boolVal;
		unsigned uintVal;
		int intVal;
		long lVal;
		long long llVal;
		float fltVal;
		double dblVal;

		struct
		{
			float x;
			float y;
			float z;
			float w;

			float& operator[](unsigned index) {return *(&x + index);}
			const float& operator[](unsigned index) const {return *(&x + index);}
		} vecVal;

		void* ptr;
		char* str;		
	};

	BitValue(): _w1(0), _w2(0), _w3(0), _w4(0), type(cBitTypeEnd), size(0) {}

	~BitValue()
	{
		if (SafePtr())
			free(ptr);
	}

	void* SafePtr()
	{
		return type == btData || type == btStr ? ptr : NULL;
	}

	void NewData(unsigned size)
	{
		ptr = realloc(SafePtr(), size);
	}

	void AssignData(const void* data, unsigned size)
	{
		NewData(size);
		memmove(ptr, data, size);
	}

	void GetData(void*& data, unsigned& mSize)
	{
		if (SafePtr() == NULL)
			return;

		mSize = size;
		data = realloc(data, size);
		memmove(data, ptr, size);
	}

	static unsigned GetSize(BitType type)
	{
		switch (type)
		{
		case btByte:
			return sizeof(BYTE);
		case btChar:
			return sizeof(char);
		case btBool:
			return sizeof(bool);
		case btUint:
			return sizeof(unsigned);
		case btInt:
			return sizeof(int);
		case btLong:
			return sizeof(long);
		case btLongLong:
			return sizeof(long long);
		case btFloat:
			return sizeof(float);
		case btDouble:
			return sizeof(double);
		case btVec2:
			return sizeof(float) * 2;
		case btVec3:
			return sizeof(float) * 3;
		case btVec4:
			return sizeof(float) * 4;
		default:
			return sizeof(void*);
		}
	}

	template<class _Type> static void Make(BitValue& bitVal, const _Type& val, BitType type);
	template<class _Type> static BitValue Make(const _Type& val, BitType type);	
	template<class _Type> static void Get(const BitValue& bitVal, _Type& val);

	NETLIB_API static void Make(BitValue& bitVal, void* val, unsigned size);
	NETLIB_API static void Get(BitValue& bitVal, void*& val, unsigned& size);
};

class BitStream
{
public:
	NETLIB_API static const float cFloatErr;
private:
	struct TypeHeader
	{
		BYTE type: 7;
		BYTE sleeping: 1;
	};

	struct ValHeader
	{
		TypeHeader typeHeader;
		BYTE pos;
		BYTE size;
	};

	struct Val
	{
		Val(): changed(false), updated(false) {}

		BitValue bit;
		mutable bool changed;
		mutable bool updated;
	};
private:
	std::vector<Val> _values;
	unsigned _pos;

	unsigned _time;
	unsigned _deltaTime;
	bool _isWriting;
	bool _isReading;
	mutable unsigned _changedCount;
	mutable unsigned _updatedCount;
	mutable bool _isSleeping;	

	Val& Get();
	void Next();
	void ValUpdated(const Val& val, bool updated) const;
	void ValChanged(const Val& val, bool changed) const;
public:
	NETLIB_API BitStream();	

	NETLIB_API void Reset(bool isWriting, bool isReading, unsigned time);
	NETLIB_API void Clear();

	NETLIB_API unsigned time() const;
	NETLIB_API unsigned deltaTime() const;
	NETLIB_API bool isWriting() const;
	NETLIB_API bool isReading() const;
	NETLIB_API bool isEmpty() const;
	NETLIB_API bool isChanged() const;
	NETLIB_API bool isUpdated() const;
	NETLIB_API bool isSleeping() const;
	NETLIB_API bool isPutSleep() const;	

	NETLIB_API void Read(std::istream& stream);
	NETLIB_API void Write(std::ostream& stream, bool diff, bool changed, bool updated) const;

	NETLIB_API void Serialize(BitValue& value, bool equal);
	template<class _Type> void Serialize(_Type& value, BitType type, bool equal);

	NETLIB_API void Serialize(BYTE& value);
	NETLIB_API void Serialize(char& value);
	NETLIB_API void Serialize(bool& value);	
	NETLIB_API void Serialize(unsigned& value);
	NETLIB_API void Serialize(int& value);
	NETLIB_API void Serialize(long& value);
	NETLIB_API void Serialize(long long& value);
	NETLIB_API void Serialize(float& value, float err = cFloatErr);
	NETLIB_API void Serialize(double& value, float err = cFloatErr);

	NETLIB_API void Serialize(float value[], int count, float err = cFloatErr);
	NETLIB_API void Serialize(D3DXVECTOR2& value, float err = cFloatErr);
	NETLIB_API void Serialize(D3DXVECTOR3& value, float err = cFloatErr);
	NETLIB_API void Serialize(D3DXVECTOR4& value, float err = cFloatErr);
	NETLIB_API void Serialize(D3DXQUATERNION& value, float err = cFloatErr);
	NETLIB_API void Serialize(D3DXCOLOR& value, float err = cFloatErr);

	NETLIB_API void Serialize(void*& data, unsigned size, bool cmp);
};




template<class _Type> inline void BitValue::Make(BitValue& bitVal, const _Type& val, BitType type)
{
	_Type* p = reinterpret_cast<_Type*>(&bitVal);
	*p = val;

	bitVal.type = type;
	bitVal.size = GetSize(type);
}

template<class _Type> inline BitValue BitValue::Make(const _Type& val, BitType type)
{
	BitValue bitVal;
	Make(bitVal, val, type);

	return bitVal;
}

template<class _Type> inline void BitValue::Get(const BitValue& bitVal, _Type& val)
{
	const _Type* p = reinterpret_cast<const _Type*>(&bitVal);
	val = *p;
}

inline void BitValue::Make(BitValue& bitVal, void* val, unsigned size)
{
	bitVal.AssignData(val, size);		
	bitVal.type = btData;
	bitVal.size = size;
}

inline void BitValue::Get(BitValue& bitVal, void*& data, unsigned& size)
{
	bitVal.GetData(data, size);
}




template<class _Type> void BitStream::Serialize(_Type& value, BitType type, bool equal)
{
	BitValue bitVal;
	if (_isWriting)
		BitValue::Make(bitVal, value, type);

	Serialize(bitVal, equal);

	if (_isReading && !equal)
		BitValue::Get(bitVal, value);
}

}