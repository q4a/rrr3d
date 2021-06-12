#pragma once

#include "INetConnection.h"
#include "INetChannel.h"
#include "INetPlayer.h"

#include "NetModel.h"

namespace net
{

class INetServiceUser
{
public:	
	virtual bool OnConnected(INetConnection* sender) {return true;}
	virtual void OnDisconnected(INetConnection* sender) {}
	virtual void OnConnectionFailed(INetConnection* sender, unsigned error) {}
	virtual void OnReceiveCmd(const NetMessage& msg, const NetCmdHeader& header, const void* data, unsigned size) {}
	virtual void OnPingComplete() {}
	virtual void OnFailed(unsigned error) {}
};

class IStreamBuf
{
public:
	virtual BYTE* Prepare(unsigned n) = 0;
	virtual void Commit(unsigned n) = 0;
	virtual void Consume(unsigned n) = 0;

	virtual const BYTE* data() const = 0;
	virtual unsigned size() const = 0;
	virtual unsigned maxSize() const = 0;
};

class INetService
{	
public:
	typedef lsl::ClassList<int, NetModel, NetModel::Desc> ModelClasses;
	typedef std::vector<Endpoint> EndpointList;
public:
	virtual void Process(unsigned time) = 0;

	virtual void Initializate() = 0;
	virtual void Finalizate() = 0;
	virtual bool IsInit() const = 0;

	virtual void StartServer(unsigned port, INetAcceptorImpl* acceptor = NULL) = 0;
	virtual bool Connect(const Endpoint& endpoint, INetAcceptorImpl* acceptor = NULL) = 0;
	virtual void Close() = 0;
	virtual bool IsClosed() const = 0;

	virtual void Ping(unsigned remotePort, unsigned time, unsigned sendPeriod) = 0;
	virtual void CancelPing() = 0;
	virtual bool IsPingProcess() const = 0;
	virtual const EndpointList& endpointList() const = 0;

	virtual void Disconnect(INetConnection* connection) = 0;

	virtual INetConnection* GetConnection(unsigned index) = 0;
	virtual INetConnection* GetConnectionById(unsigned id) = 0;
	virtual unsigned connectionCount() const = 0;
	virtual bool isConnecting() const = 0;
	virtual bool isConnected() const = 0;

	virtual INetChannel* channel() = 0;

	virtual INetPlayer* player() = 0;	
	virtual bool isServer() const = 0;
	virtual bool isClient() const = 0;

	virtual unsigned time() const = 0;
	virtual unsigned syncRate() const = 0;
	virtual void syncRate(unsigned value) = 0;

	virtual ModelClasses& modelClasses() = 0;

	virtual INetServiceUser* user() const = 0;
	virtual void user(INetServiceUser* value) = 0;

	virtual lsl::Profiler* profiler() const = 0;
	virtual void profiler(lsl::Profiler* value) = 0;

	virtual bool GetAdapterAddresses(lsl::StringVec& addrVec) const = 0;

	virtual IStreamBuf* CreateStreamBuf(unsigned maxSize = (std::numeric_limits<std::size_t>::max)(), const std::allocator<char>& allocator = std::allocator<char>()) = 0;
	virtual void ReleaseStreamBuf(IStreamBuf* inst) = 0;
};

}