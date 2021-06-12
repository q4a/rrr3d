#pragma once

#include "NetModel.h"

namespace net
{

class NetChat: public NetModelRPC<NetChat>
{
	NETLIB_API static NetChat* _i;

public:
	typedef lsl::List<lsl::string> Lines;
private:
	Lines _lines;

	NETLIB_API virtual void OnStateRead(const NetMessage& msg, const NetCmdHeader& header, std::istream& stream);
	NETLIB_API virtual void OnStateWrite(const NetMessage& msg, std::ostream& stream);

	NETLIB_API void OnPushLine(const NetMessage& msg, const NetCmdHeader& header, std::istream& stream);
	NETLIB_API void OnClear(const NetMessage& msg, const NetCmdHeader& header, std::istream& stream);
protected:
	NETLIB_API virtual void OnSerialize(const NetMessage& msg, BitStream& stream);
public:
	NETLIB_API NetChat(const Desc& desc);

	NETLIB_API void PushLine(const std::string& text);
	NETLIB_API void Clear();

	static NetChat* I() {return _i;}

	volatile int testInt;
};

}