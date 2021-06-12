#pragma once

#include "NetServer.h"
#include "NetClient.h"
#include "NetMasterServer.h"

namespace net
{

class NetService: public INetService, INetChannelUser
{
	friend class NetConnection;
	friend NetChannel;
	friend class NetServer;
	friend class NetClient;
	friend class NetMasterServer;
private:
	typedef lsl::List<NetConnection*> Connections;
	typedef lsl::List<NetChannel*> Channels;

	static const unsigned cBufTickCount = 5;
private:
	INetServiceUser* _user;	
	INetAcceptorImpl* _netAcceptorImpl;
	bool _netAcceptorImplCreated;

	bool _init;	
	unsigned _time;
	unsigned _lastSyncTime;
	unsigned _syncRate;

	unsigned _pingPort;
	unsigned _pingTime;
	unsigned _pingSendPeriod;
	unsigned _pingStartTime;
	unsigned _pingSendTime;

	unsigned _bufConnectionTick;
	unsigned _bufChannelTick;
	EndpointList _endpointList;
	Connections _connections;
	Channels _channels;

	NetChannel* _netChannel;
	NetServer* _netServer;
	NetClient* _netClient;
	ModelClasses _modelClasses;

	io_service _ioService;

	void AllocProtocols();
	void ReleaseProtocols();
	void protocolImpl(INetAcceptorImpl* acceptor);

	NetConnection* NewConnection(unsigned id, INetConnectionUser* user);
	void ReleaseConnection(NetConnection* connection);
	void DeleteConnections();

	NetChannel* NewChannel(INetChannelUser* user);
	void ReleaseChannel(NetChannel* channel);
	void DeleteChannels();
	void SendPing();

	io_service& ioService();
	
	virtual bool OnConnected(INetConnection* sender);
	virtual bool OnDisconnected(INetConnection* sender);
	virtual bool OnConnectionFailed(INetConnection* sender, unsigned error);
	virtual void OnReceiveCmd(const NetMessage& msg, const NetCmdHeader& header, const void* data, unsigned size);

	virtual void OnReceiveState(const NetMessage& msg, const NetStateHeader& header, const streambuf::const_buffers_type& bufs, const Endpoint& remoteEndpoint);
	virtual void OnIOFailed(const error_code& error);
public:
	NetService();
	~NetService();

	void Process(unsigned time);

	void Initializate();
	void Finalizate();
	bool IsInit() const;

	void StartServer(unsigned port, INetAcceptorImpl* acceptor);
	bool Connect(const Endpoint& endpoint, INetAcceptorImpl* acceptor);	
	void Close();
	bool IsClosed() const;

	void Ping(unsigned remotePort, unsigned time, unsigned sendPeriod);
	void CancelPing();
	bool IsPingProcess() const;
	const EndpointList& endpointList() const;

	void Disconnect(INetConnection* connection);

	INetConnection* GetConnection(unsigned index);
	INetConnection* GetConnectionById(unsigned id);
	unsigned connectionCount() const;
	bool isConnecting() const;
	bool isConnected() const;

	NetChannel* channel();

	NetPlayer* player();
	bool isServer() const;
	bool isClient() const;

	unsigned time() const;

	unsigned syncRate() const;
	void syncRate(unsigned value);

	ModelClasses& modelClasses();

	INetServiceUser* user() const;
	void user(INetServiceUser* value);

	lsl::Profiler* profiler() const;
	void profiler(lsl::Profiler* value);

	INetAcceptorImpl* acceptorImpl() const;

	bool GetAdapterAddresses(lsl::StringVec& addrVec) const;

	IStreamBuf* CreateStreamBuf(unsigned maxSize, const std::allocator<char>& allocator);
	void ReleaseStreamBuf(IStreamBuf* inst);
};

}