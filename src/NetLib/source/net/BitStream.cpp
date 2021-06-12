#include "stdafx.h"

#include "net\BitStream.h"

namespace net
{

const float BitStream::cFloatErr = 0.001f;




BitStream::BitStream(): _pos(0), _time(0), _deltaTime(0), _isWriting(false), _isReading(false), _changedCount(0), _updatedCount(0), _isSleeping(true)
{
}

BitStream::Val& BitStream::Get()
{
	if (_pos >= _values.size())
	{
		_pos = _values.size();
		_values.resize(_pos + 1);		
	}

	return _values[_pos];
}

void BitStream::Next()
{
	++_pos;
}

void BitStream::ValUpdated(const Val& val, bool updated) const
{
	if (val.updated != updated)
	{
		val.updated = updated;

		if (updated)
			++_updatedCount;
		else if(_updatedCount > 0)
			--_updatedCount;
	}
}

void BitStream::ValChanged(const Val& val, bool changed) const
{
	if (val.changed != changed)
	{
		val.changed = changed;

		if (changed)
			++_changedCount;
		else if(_changedCount > 0)
			--_changedCount;
	}
}

void BitStream::Reset(bool isWriting, bool isReading, unsigned time)
{
	_deltaTime = !_isSleeping ? time - _time : 0;
	_time = time;	

	_isWriting = isWriting;
	_isReading = isReading;		
	_pos = 0;
}

void BitStream::Clear()
{
	_isSleeping = true;	
	_time = 0;
	_deltaTime = 0;
	_isWriting = false;
	_isReading = false;
	_pos = 0;

	_values.clear();
	_changedCount = 0;	
	_updatedCount = 0;
}

unsigned BitStream::time() const
{
	return _time;
}

unsigned BitStream::deltaTime() const
{
	return _deltaTime;
}

bool BitStream::isWriting() const
{
	return _isWriting;
}

bool BitStream::isReading() const
{
	return _isReading;
}

bool BitStream::isEmpty() const
{
	return _values.size() == 0;
}

bool BitStream::isChanged() const
{
	return _changedCount > 0;
}

bool BitStream::isUpdated() const
{
	return _updatedCount > 0;
}

bool BitStream::isSleeping() const
{
	return _isSleeping;
}

bool BitStream::isPutSleep() const
{
	return !_isSleeping && _changedCount == 0;
}

void BitStream::Read(std::istream& stream)
{
	ValHeader header;
	net::Read(stream, &header.typeHeader, sizeof(header.typeHeader));
	_isSleeping = header.typeHeader.sleeping;	

	while (header.typeHeader.type != cBitTypeEnd)
	{
		net::Read(stream, header.pos);
		if (header.pos >= _values.size())
			_values.resize(header.pos + 1);

		Val& val = _values[header.pos];

		if (header.typeHeader.type == btData)
		{
			net::Read(stream, header.size);
			val.bit.NewData(header.size);
			net::Read(stream, val.bit.ptr, header.size);
		}
		else
		{
			header.size = BitValue::GetSize((BitType)header.typeHeader.type);
			net::Read(stream, &val, header.size);
		}		
		val.bit.type = (BitType)header.typeHeader.type;
		val.bit.size = header.size;
		ValUpdated(val, true);
		ValChanged(val, false);

		net::Read(stream, &header.typeHeader, sizeof(header.typeHeader));
	}
}

void BitStream::Write(std::ostream& stream, bool diff, bool changed, bool updated) const
{
	unsigned endPos = changed ? _pos : _values.size();
	if (diff)
		_isSleeping = _changedCount == 0;

	for (unsigned i = 0; i < endPos; ++i)
	{
		if (changed && !_values[i].changed)
			continue;
		if (updated && !_values[i].updated)
			continue;

		if (diff)
		{
			ValUpdated(_values[i], false);
			ValChanged(_values[i], false);
		}

		ValHeader header;
		header.typeHeader.type = _values[i].bit.type;
		header.typeHeader.sleeping = _isSleeping;
		header.pos = i;
		header.size = _values[i].bit.size;		

		if (header.typeHeader.type == btData)
		{
			net::Write(stream, &header, sizeof(header));
		}
		else
		{	 
			net::Write(stream, &header.typeHeader, sizeof(header.typeHeader));
			net::Write(stream, header.pos);
		}

		net::Write(stream, &_values[i].bit, _values[i].bit.size);
	}

	TypeHeader typeHeader;
	typeHeader.type = cBitTypeEnd;
	typeHeader.sleeping = _isSleeping;
	net::Write(stream, &typeHeader, sizeof(typeHeader));
}

void BitStream::Serialize(BitValue& value, bool equal)
{
	if (_isReading)
	{	
		Val& val = Get();
		ValUpdated(val, false);
		ValChanged(val, false);		
		if (!equal)
			value = val.bit;

		Next();
	}
	else if (_isWriting)
	{
		if (!equal)
		{
			Val& val = Get();
			val.bit = value;
			ValUpdated(val, false);
			ValChanged(val, true);			
			_isSleeping = false;
		}
		Next();
	}
}

void BitStream::Serialize(BYTE& value)
{
	Serialize(value, btByte, Get().bit.byteVal == value);
}

void BitStream::Serialize(char& value)
{
	Serialize(value, btChar, Get().bit.charVal == value);
}

void BitStream::Serialize(bool& value)
{
	Serialize(value, btBool, Get().bit.boolVal == value);
}

void BitStream::Serialize(unsigned& value)
{
	Serialize(value, btUint, Get().bit.uintVal == value);
}

void BitStream::Serialize(int& value)
{
	Serialize(value, btInt, Get().bit.intVal == value);
}

void BitStream::Serialize(long& value)
{
	Serialize(value, btLong, Get().bit.lVal == value);
}

void BitStream::Serialize(long long& value)
{
	Serialize(value, btLongLong, Get().bit.llVal == value);
}

void BitStream::Serialize(float& value, float err)
{
	Serialize(value, btFloat, abs(Get().bit.fltVal - value) < err);
}

void BitStream::Serialize(double& value, float err)
{
	Serialize(value, btDouble, abs(Get().bit.dblVal - value) < err);
}

void BitStream::Serialize(float value[], int count, float err)
{
	bool eq = true;
	BitValue& bitVal = Get().bit;
	for (int i = 0; i < count; ++i)
		if (abs(value[i] - bitVal.vecVal[i]) >= err)
		{
			eq = false;
			break;
		}

	BitType type;

	switch (count)
	{
	case 1:
		type = btFloat;
		Serialize((float&)*value, type, eq);
		break;
	case 2:
		type = btVec2;
		Serialize((D3DXVECTOR2&)*value, type, eq);
		break;
	case 3:
		type = btVec3;
		Serialize((D3DXVECTOR3&)*value, type, eq);
		break;
	case 4:
		type = btVec4;
		Serialize((D3DXVECTOR4&)*value, type, eq);
		break;
	default:
		LSL_ASSERT("void Serialize(float[] value, int count, float err)");
	}
}

void BitStream::Serialize(D3DXVECTOR2& value, float err)
{
	Serialize(value, 2, err);
}

void BitStream::Serialize(D3DXVECTOR3& value, float err)
{
	Serialize(value, 3, err);
}

void BitStream::Serialize(D3DXVECTOR4& value, float err)
{
	Serialize(value, 4, err);
}

void BitStream::Serialize(D3DXQUATERNION& value, float err)
{
	Serialize(value, 4, err);
}

void BitStream::Serialize(D3DXCOLOR& value, float err)
{
	Serialize(value, 4, err);
}

void BitStream::Serialize(void*& data, unsigned size, bool cmp)
{
	if (cmp)
	{
		if (Get().bit.size != size)
		{
			Next();
			return;
		}
		if (!memcmp(data, Get().bit.ptr, size))
		{
			Next();
			return;
		}
	}

	BitValue bitVal;
	if (_isWriting)
		BitValue::Make(bitVal, data, size);

	Serialize(bitVal, false);

	if (_isReading)
		BitValue::Get(bitVal, data, size);
}

}