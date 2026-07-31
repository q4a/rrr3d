// stdafx.h : include file for standard system include files,
#pragma once

#include "targetver.h"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
// boost/thread/thread.hpp was included here, but no boost::thread symbol
// appears anywhere in the tree. Dropping it keeps Boost header-only for this
// library -- Asio, bind and system all are; Thread is not.

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