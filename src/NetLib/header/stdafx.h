// stdafx.h : include file for standard system include files,
#pragma once

#include "targetver.h"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>

#include "net/NetLib.h"

namespace net
{

using namespace boost;
using namespace boost::system;
using namespace boost::asio;
using namespace boost::asio::ip;

// boost::asio::buffer_cast was removed in Boost 1.87. This is what it did:
// take the first buffer of a sequence and expose its pointer. Constness of
// _Ptr must match the sequence (prepare() is mutable, data() is const).
template<class _Ptr, class _Buffers> inline _Ptr buffer_cast(const _Buffers& buffers)
{
	return static_cast<_Ptr>(boost::asio::buffer(buffers).data());
}

class NetService;

bool GetEndpointTCP(const Endpoint& ref, tcp::endpoint& endpoint);

}