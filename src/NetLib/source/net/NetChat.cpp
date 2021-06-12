#include "stdafx.h"

#include "net\NetChat.h"

namespace net
{

NetChat* NetChat::_i;

NetChat::NetChat(const Desc& desc): NetModelRPC<NetChat>(desc), testInt(0)
{
	_i = this;

	RegRPC(&NetChat::OnPushLine);
	RegRPC(&NetChat::OnClear);

	syncState(ssDelta);
}

void NetChat::OnStateRead(const NetMessage& msg, const NetCmdHeader& header, std::istream& stream)
{
	_lines.clear();
	std::string line;
	unsigned size = 0;

	while (size < header.size)
	{
		size += Read(stream, line);
		_lines.push_back(line);

		LSL_TRACE(line);
	}
}

void NetChat::OnStateWrite(const NetMessage& msg, std::ostream& stream)
{
	for (Lines::const_iterator iter = _lines.begin(); iter != _lines.end(); ++iter)
	{
		net::Write(stream, *iter);
	}
}

void NetChat::OnPushLine(const NetMessage& msg, const NetCmdHeader& header, std::istream& stream)
{
	std::string line;
	Read(stream, line, header.size);

	LSL_TRACE(line);

	_lines.push_back(line);
}

void NetChat::OnClear(const NetMessage& msg, const NetCmdHeader& header, std::istream& stream)
{
	_lines.clear();
}

void NetChat::OnSerialize(const NetMessage& msg, BitStream& stream)
{
	int testInt2 = testInt;
	stream.Serialize(testInt2);
	testInt = testInt2;

	if (stream.isReading())
		LSL_TRACE(lsl::StrFmt("NetChat.testInt=%d", testInt));
}

void NetChat::PushLine(const std::string& text)
{
	std::ostream& stream = NewRPC(cNetTargetOthers, &NetChat::OnPushLine);
	Write(stream, text, text.length());
	CloseRPC();
}

void NetChat::Clear()
{
	_lines.clear();

	MakeRPC(cNetTargetOthers, &NetChat::OnClear);
}

}