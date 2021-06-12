#pragma once

#include <stdio.h>
#include <tchar.h>

#include "lslCommon.h"
#include "lslUtility.h"
#include "lslException.h"
#include "lslContainer.h"
#include "lslClassList.h"
#include "lslSDK.h"

#define NETLIB_DYNAMIC_LINK

#ifdef NETLIB_DYNAMIC_LINK
	#ifdef NETLIB_EXPORTS
		#define NETLIB_API __declspec(dllexport)
	#else
		#define NETLIB_API __declspec(dllimport)
	#endif
#else
	#define NETLIB_API
#endif

namespace net
{

class INetService;

//special use (eg for masks)
const unsigned cLocalPlayer = 0;
//
const unsigned cServerPlayer = 1;
//
const unsigned cPlayerMaxCount = 6;
//
const unsigned cNetTargetAll = 1 << 3; //except masked, 0 - ignored
const unsigned cNetTargetOthers = 1 << 4; //except masked and local, 0 - cLocalPlayer
const unsigned cUndefPlayer = MAXUINT;

const unsigned cSyncStatePriorityDef = 0;
const unsigned cSyncStatePriorityHigh = 1;

struct NetMessage
{
private:
	mutable bool _discard;
public:
	unsigned sender;
	unsigned time;	

	NetMessage(): _discard(false), sender(cUndefPlayer), time(0) {}
	NetMessage(unsigned mSender, unsigned mTime): _discard(false), sender(mSender), time(mTime) {}

	void Discard() const
	{
		_discard = true;
	}

	bool discarded() const {return _discard;}
};

struct NetCmdHeader
{
	unsigned datagram: 1;
	unsigned id: 9;
	unsigned target: 5;
	unsigned rpc: 5;
	unsigned size: 12;

	NetCmdHeader(): datagram(0) {}
};

struct NetStateHeader
{
	unsigned time: 31;
	unsigned reserved1: 1;

	unsigned id: 9;
	unsigned sender: 3;
	unsigned size: 9;
	unsigned reserved2: 11;

	static unsigned SizeOf() {return sizeof(NetStateHeader);}
};

struct ModelHeader
{
	unsigned modelId: 7;
	unsigned id: 12;
	unsigned ownerId: 3;
	unsigned descSize: 10;
};

struct Endpoint
{
	std::string address;
	unsigned addressLong;
	unsigned port;		

	Endpoint() {}
	Endpoint(const std::string& mAddress, unsigned mPort): addressLong(0), address(mAddress), port(mPort) {}
	Endpoint(unsigned mAddress, unsigned mPort): addressLong(mAddress), port(mPort) {}

	bool operator==(const Endpoint& ref) const
	{
		return addressLong == ref.addressLong && address == ref.address && port == ref.port;
	}

	bool operator<(const Endpoint& ref) const
	{
		if (addressLong == ref.addressLong)
		{
			if (port == ref.port)
			{
				return address < ref.address;
			}
			else
			{
				return port < ref.port;
			}
		}
		else
		{
			return addressLong < ref.addressLong;
		}
	}
};

const unsigned cMaxRPC = 32;

unsigned Write(std::ostream& stream, const void* data, unsigned size);
unsigned Read(std::istream& stream, void* data, unsigned size);

unsigned Write(std::ostream& stream, BYTE value);
unsigned Read(std::istream& stream, BYTE& value);

unsigned Write(std::ostream& stream, char value);
unsigned Read(std::istream& stream, char& value);

unsigned Write(std::ostream& stream, int value);
unsigned Read(std::istream& stream, int& value);

unsigned Write(std::ostream& stream, unsigned value);
unsigned Read(std::istream& stream, unsigned& value);

unsigned Write(std::ostream& stream, float value);
unsigned Read(std::istream& stream, float& value);

unsigned Write(std::ostream& stream, double value);
unsigned Read(std::istream& stream, double& value);

unsigned Write(std::ostream& stream, bool value);
unsigned Read(std::istream& stream, bool& value);

unsigned Write(std::ostream& stream, const D3DXVECTOR2& value);
unsigned Read(std::istream& stream, D3DXVECTOR2& value);

unsigned Write(std::ostream& stream, const D3DXVECTOR3& value);
unsigned Read(std::istream& stream, D3DXVECTOR3& value);

unsigned Write(std::ostream& stream, const D3DXVECTOR4& value);
unsigned Read(std::istream& stream, D3DXVECTOR4& value);

unsigned Write(std::ostream& stream, const D3DXQUATERNION& value);
unsigned Read(std::istream& stream, D3DXQUATERNION& value);

unsigned Write(std::ostream& stream, const D3DXCOLOR& value);
unsigned Read(std::istream& stream, D3DXCOLOR& value);

unsigned Write(std::ostream& stream, const D3DXMATRIX& value);
unsigned Read(std::istream& stream, D3DXMATRIX& value);

template<class _T> unsigned Write(std::ostream& stream, const std::basic_string<_T>& value, unsigned size);
template<class _T> unsigned Read(std::istream& stream, std::basic_string<_T>& value, unsigned size);

template<class _T> unsigned Write(std::ostream& stream, const std::basic_string<_T>& value);
template<class _T> unsigned Read(std::istream& stream, std::basic_string<_T>& value);




inline unsigned Write(std::ostream& stream, const void* data, unsigned size)
{
	stream.write((const char*)data, size);
	return size;
}

inline unsigned Read(std::istream& stream, void* data, unsigned size)
{
	stream.read((char*)data, size);
	return size;
}

inline unsigned Write(std::ostream& stream, BYTE value)
{
	Write(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Read(std::istream& stream, BYTE& value)
{
	Read(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Write(std::ostream& stream, char value)
{
	Write(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Read(std::istream& stream, char& value)
{
	Read(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Write(std::ostream& stream, int value)
{
	Write(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Read(std::istream& stream, int& value)
{
	Read(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Write(std::ostream& stream, unsigned value)
{
	Write(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Read(std::istream& stream, unsigned& value)
{
	Read(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Write(std::ostream& stream, float value)
{
	Write(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Read(std::istream& stream, float& value)
{
	Read(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Write(std::ostream& stream, double value)
{
	Write(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Read(std::istream& stream, double& value)
{
	Read(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Write(std::ostream& stream, bool value)
{
	Write(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Read(std::istream& stream, bool& value)
{
	Read(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Write(std::ostream& stream, const D3DXVECTOR2& value)
{
	Write(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Read(std::istream& stream, D3DXVECTOR2& value)
{
	Read(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Write(std::ostream& stream, const D3DXVECTOR3& value)
{
	Write(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Read(std::istream& stream, D3DXVECTOR3& value)
{
	Read(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Write(std::ostream& stream, const D3DXVECTOR4& value)
{
	Write(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Read(std::istream& stream, D3DXVECTOR4& value)
{
	Read(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Write(std::ostream& stream, const D3DXQUATERNION& value)
{
	Write(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Read(std::istream& stream, D3DXQUATERNION& value)
{
	Read(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Write(std::ostream& stream, const D3DXCOLOR& value)
{
	Write(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Read(std::istream& stream, D3DXCOLOR& value)
{
	Read(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Write(std::ostream& stream, const D3DXMATRIX& value)
{
	Write(stream, &value, sizeof(value));
	return sizeof(value);
}

inline unsigned Read(std::istream& stream, D3DXMATRIX& value)
{
	Read(stream, &value, sizeof(value));
	return sizeof(value);
}

template<class _T> inline unsigned Write(std::ostream& stream, const std::basic_string<_T>& value, unsigned size)
{
	Write(stream, value.data(), sizeof(std::basic_string<_T>::value_type) * size);
	return size;
}

template<class _T> inline unsigned Read(std::istream& stream, std::basic_string<_T>& value, unsigned size)
{
	value.resize(size);	
	Read(stream, const_cast<std::basic_string<_T>::pointer>(value.data()), sizeof(std::basic_string<_T>::value_type) * size);
	return size;
}

template<class _T> inline unsigned Write(std::ostream& stream, const std::basic_string<_T>& value)
{
	unsigned size = Write(stream, (unsigned)value.length());
	size += Write(stream, value, value.length());
	return size;
}

template<class _T> inline unsigned Read(std::istream& stream, std::basic_string<_T>& value)
{
	unsigned len;
	unsigned size = Read(stream, len);
	value.resize(len);

	size += Read(stream, value, len);
	return size;
}

}