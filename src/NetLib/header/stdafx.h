// stdafx.h : include file for standard system include files,
#pragma once

#include "targetver.h"

#include <boost/asio.hpp>

#include "net/NetLib.h"

namespace net
{

using namespace boost;
using namespace boost::system;
using namespace boost::asio;
using namespace boost::asio::ip;

class NetService;

bool GetEndpointTCP(const Endpoint& ref, tcp::endpoint& endpoint);

}
