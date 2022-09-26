// stdafx.cpp : source file that includes just the standard includes
// NetServer.pch will be the pre-compiled header
// stdafx.obj will contain the pre-compiled type information

#include "stdafx.h"

namespace net
{

bool GetEndpointTCP(const Endpoint& ref, tcp::endpoint& endpoint)
{
	if (!ref.address.empty())
	{
		error_code code;
		ip::address addr = address::from_string(ref.address, code);

		if (code)
		{
			LSL_LOG("NetClient::Connect error address=" + ref.address + " message=" + code.message());
			return false;
		}

		endpoint = tcp::endpoint(addr, ref.port);

		return true;
	}
	else
	{
		endpoint = tcp::endpoint(ip::address_v4(ref.addressLong), ref.port);

		return true;
	}
}

}
