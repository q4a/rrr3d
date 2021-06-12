#include "stdafx.h"

#include "net\NetService.h"
#include "net\NetConnectionTCP.h"

#include <winsock2.h>
#include <Iphlpapi.h>

namespace net
{

class StreamBuf: public IStreamBuf
{
private:
	streambuf _inst;
public:
	StreamBuf(unsigned maximumSize, const std::allocator<char>& allocator): _inst(maximumSize, allocator) { }

	BYTE* Prepare(unsigned n) { return buffer_cast<BYTE*>(_inst.prepare(n)); }
	void Commit(unsigned n) { _inst.commit(n); }
	void Consume(unsigned n) { _inst.consume(n); }

	const BYTE* data() const { return buffer_cast<const BYTE*>(_inst.data()); }
	unsigned size() const { return _inst.size(); }
	unsigned maxSize() const { return _inst.max_size(); }
};




NetService::NetService(): _init(false), _ioService(0), _netChannel(NULL), _netServer(NULL), _netClient(NULL), _time(0), _syncRate(70), _lastSyncTime(0), _pingPort(0), _pingTime(0), _pingSendPeriod(0), _bufConnectionTick(0), _bufChannelTick(0), _user(NULL), _netAcceptorImpl(NULL), _netAcceptorImplCreated(false)
{
}

NetService::~NetService()
{
	Finalizate();

	ReleaseProtocols();
}

void NetService::AllocProtocols()
{
	if (_netAcceptorImpl == NULL)
	{
		_netAcceptorImpl = new NetAcceptorTCP(ioService());
		_netAcceptorImplCreated = true;
	}

	if (_netChannel == NULL)
		_netChannel = NewChannel(this);
	_netChannel->Open(true);
}

void NetService::ReleaseProtocols()
{
	_ioService.poll();
	Sleep(0);
	_ioService.poll();

	DeleteConnections();
	DeleteChannels();

	if (_netAcceptorImplCreated)
	{
		delete static_cast<NetAcceptorTCP*>(_netAcceptorImpl);
		_netAcceptorImplCreated = false;
	}

	_netAcceptorImpl = NULL;
}

void NetService::protocolImpl(INetAcceptorImpl* acceptor)
{
	ReleaseProtocols();

	_netAcceptorImpl = acceptor;

	AllocProtocols();
}

NetConnection* NetService::NewConnection(unsigned id, INetConnectionUser* user)
{
	if (_connections.size() > 0 && _bufConnectionTick > cBufTickCount)
	{
		NetConnection* connection = _connections.front();
		connection->id(id);
		connection->user(user);
		_connections.pop_front();
		_bufConnectionTick = 0;

		return connection;
	}

	return new NetConnection(this, id, user);
}

void NetService::ReleaseConnection(NetConnection* connection)
{
	connection->id(cUndefPlayer);
	connection->user(NULL);
	_connections.push_back(connection);
}

void NetService::DeleteConnections()
{
	for (Connections::const_iterator iter = _connections.begin(); iter != _connections.end(); ++iter)
		delete (*iter);
	_connections.clear();
	_bufConnectionTick = 0;
}

NetChannel* NetService::NewChannel(INetChannelUser* user)
{
	if (_channels.size() > 0 && _bufChannelTick > cBufTickCount)
	{
		NetChannel* channel = _channels.front();		
		channel->user(user);
		_channels.pop_front();
		_bufChannelTick = 0;

		return channel;
	}

	return new NetChannel(this, user);
}

void NetService::ReleaseChannel(NetChannel* channel)
{
	channel->user(NULL);
	_channels.push_back(channel);
}

void NetService::DeleteChannels()
{
	if (_netChannel)
	{
		ReleaseChannel(_netChannel);
		_netChannel = NULL;
	}

	for (Channels::const_iterator iter = _channels.begin(); iter != _channels.end(); ++iter)
		delete (*iter);
	_channels.clear();
	_bufChannelTick = 0;
}

void NetService::SendPing()
{
	NetStateHeader header;
	header.id = NetPlayer::cPing;
	header.sender = cLocalPlayer;
	header.size = 0;
	
	_netChannel->SendState(Endpoint(ip::address_v4::broadcast().to_ulong(), _pingPort), header, streambuf::const_buffers_type(NULL, 0));
}

io_service& NetService::ioService()
{
	return _ioService;
}

bool NetService::OnConnected(INetConnection* sender)
{
	if (_user)
		return _user->OnConnected(sender);

	return true;
}

bool NetService::OnDisconnected(INetConnection* sender)
{
	if (_user)
		_user->OnDisconnected(sender);

	if (isClient())
		Close();

	return !IsClosed();
}

bool NetService::OnConnectionFailed(INetConnection* sender, unsigned error)
{
	if (_user)
		_user->OnConnectionFailed(sender, error);

	if (isClient())
		Close();

	return !IsClosed();
}

void NetService::OnReceiveCmd(const NetMessage& msg, const NetCmdHeader& header, const void* data, unsigned size)
{
	if (_user)
		_user->OnReceiveCmd(msg, header, data, size);
}

void NetService::OnReceiveState(const NetMessage& msg, const NetStateHeader& header, const streambuf::const_buffers_type& bufs, const Endpoint& remoteEndpoint)
{
	//LSL_TRACE(lsl::StrFmt("NetService::OnReceiveState id=%d target=%d size=%d", header.id, header.target, header.size));
	
	if (header.id == NetPlayer::cPing)
	{
		tcp::endpoint tcpEndpoint;
		GetEndpointTCP(remoteEndpoint, tcpEndpoint);		

		LSL_TRACE(lsl::StrFmt("NetService::OnReceiveState=cPing sender=%d address=%s port=%d", header.sender, tcpEndpoint.address().to_string().c_str(), tcpEndpoint.port()));

		if (header.sender == cLocalPlayer && isServer())
		{
			NetStateHeader header;
			header.id = NetPlayer::cPing;
			header.sender = cServerPlayer;
			header.size = 0;
	
			_netChannel->SendState(remoteEndpoint, header, streambuf::const_buffers_type(NULL, 0));
		}
		else if (header.sender == cServerPlayer)
		{
			Endpoint newEndpoint(Endpoint(tcpEndpoint.address().to_string(), tcpEndpoint.port()));

			//duplicate
			for (EndpointList::const_iterator iter = _endpointList.begin(); iter != _endpointList.end(); ++iter)
				if (iter->address == newEndpoint.address)
					return;

			_endpointList.push_back(newEndpoint);
		}

		return;
	}

	if (player())
		player()->ProcessState(msg, header, bufs);
}

void NetService::OnIOFailed(const error_code& error)
{
	if (_user)
		_user->OnFailed(error.value());

	Close();
}

void NetService::Process(unsigned time)
{
	_time = time;	

	//receive async events (may immediately dispatched or collected depends from current async model)
	error_code error;
	_ioService.poll(error);
	if (error)
		lsl::appLog.Append(error.message());

	if (_connections.size() > 0)
		++_bufConnectionTick;
	if (_channels.size() > 0)
		++_bufChannelTick;

	//dispatch async events
	if (_init)
		_netChannel->Dispatch();
	if (_netClient)
		_netClient->Dispatch();
	if (_netServer)
		_netServer->Dispatch();	

	NetPlayer* player = this->player();
	if (player)
	{
		//process and dispatch new state
		bool syncRate = (_time - _lastSyncTime) > _syncRate;
		if (syncRate)
			_lastSyncTime = _time;

		player->Process(time, syncRate);
	}

	if (_init)
	{
		_netChannel->Process(time);

		if (_pingTime > 0)
		{
			if (_time - _pingSendTime > _pingSendPeriod)
			{
				_pingSendTime = _time;
				SendPing();
			}

			if (_time - _pingStartTime > _pingTime)
			{
				CancelPing();				
				if (_user)
					_user->OnPingComplete();
			}
		}
	}
}

void NetService::Initializate()
{
	if (_init)
		return;
	_init = true;

	AllocProtocols();
}

void NetService::Finalizate()
{
	if (!_init)
		return;

	_init = false;
	_endpointList.clear();
	_pingTime = 0;

	Close();

	_netChannel->Close();
}

bool NetService::IsInit() const
{
	return _init;
}

void NetService::StartServer(unsigned port, INetAcceptorImpl* acceptor)
{
	if (_netServer)
		return;
	Close();

	protocolImpl(acceptor);
	
	_netServer = new NetServer(this);
	_netServer->Start(port);
}

bool NetService::Connect(const Endpoint& endpoint, INetAcceptorImpl* acceptor)
{
	if (_netClient)
		Close();

	protocolImpl(acceptor);

	_netClient = new NetClient(this);
	return _netClient->Connect(endpoint);
}

void NetService::Close()
{
	if (!IsClosed())
	{
		_ioService.reset();

		lsl::SafeDelete(_netClient);
		lsl::SafeDelete(_netServer);
	}
}

bool NetService::IsClosed() const
{
	return _netClient == NULL && _netServer == NULL;
}

void NetService::Ping(unsigned remotePort, unsigned time, unsigned sendPeriod)
{
	_endpointList.clear();

	_pingPort = remotePort;
	_pingTime = time;
	_pingSendPeriod = sendPeriod;
	_pingSendTime = _time;
	_pingStartTime = _time;

	_netChannel->StartReceiveResponseOut();
	SendPing();	
}

void NetService::CancelPing()
{
	_pingTime = 0;
	_netChannel->StopReceiveResponseOut();
}

bool NetService::IsPingProcess() const
{
	return _pingTime > 0;
}

const NetService::EndpointList& NetService::endpointList() const
{
	return _endpointList;
}

void NetService::Disconnect(INetConnection* connection)
{
	if (_netServer)
		_netServer->Disconnect(_netServer->GetConnectionById(connection->id()));
}

INetConnection* NetService::GetConnection(unsigned index)
{
	if (_netServer)
		return _netServer->GetConnection(index);
	if (_netClient && index == 0)
		return _netClient->connection();

	return NULL;
}

INetConnection* NetService::GetConnectionById(unsigned id)
{
	if (_netServer)
		return _netServer->GetConnectionById(id);
	if (_netClient && id == cServerPlayer)
		return _netClient->connection();

	return NULL;
}

unsigned NetService::connectionCount() const
{
	if (_netServer)
		return _netServer->connectionCount();
	if (_netClient)
		return 1;

	return NULL;
}

bool NetService::isConnecting() const
{
	if (_netClient)
		return _netClient->isConnecting();

	return false;
}

bool NetService::isConnected() const
{
	if (_netClient)
		return _netClient->connected();

	return false;
}

NetChannel* NetService::channel()
{
	return _netChannel;
}

NetPlayer* NetService::player()
{
	if (_netServer)
		return _netServer;
	if (_netClient && (isConnecting() || isConnected()))
		return _netClient;

	return NULL;
}

bool NetService::isServer() const
{
	return _netServer ? true : false;
}

bool NetService::isClient() const
{
	return _netClient ? true : false;
}

unsigned NetService::time() const
{
	return _time;
}

unsigned NetService::syncRate() const
{
	return _syncRate;
}

void NetService::syncRate(unsigned value)
{
	_syncRate = value;
}

NetService::ModelClasses& NetService::modelClasses()
{
	return _modelClasses;
}

INetServiceUser* NetService::user() const
{
	return _user;
}

void NetService::user(INetServiceUser* value)
{
	_user = value;
}

lsl::Profiler* NetService::profiler() const
{
	return &lsl::Profiler::I();
}

void NetService::profiler(lsl::Profiler* value)
{
	lsl::Profiler::Init(value);
}

INetAcceptorImpl* NetService::acceptorImpl() const
{
	return _netAcceptorImpl;
}

bool NetService::GetAdapterAddresses(lsl::StringVec& addrVec) const
{
	const unsigned WORKING_BUFFER_SIZE = 15000;
	const unsigned MAX_TRIES = 3;

	IP_ADAPTER_ADDRESSES* addresses = NULL;		
    unsigned long outBufLen = WORKING_BUFFER_SIZE;
	unsigned dwRetVal = 0;
	unsigned Iterations = 0;

	// Allocate a 15 KB buffer to start with.
    do
	{
        addresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);

        if (addresses == NULL) {
			LSL_LOG("Memory allocation failed for IP_ADAPTER_ADDRESSES struct");
			return false;
        }

        dwRetVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST, NULL, addresses, &outBufLen);

        if (dwRetVal == ERROR_BUFFER_OVERFLOW)
		{
            free(addresses);
            addresses = NULL;
        }
		else
            break;

        Iterations++;

    }
	while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < MAX_TRIES));

	if (addresses == NULL)
		return false;	

	IP_ADAPTER_ADDRESSES* address = addresses;

	while (address)
	{
		IP_ADAPTER_UNICAST_ADDRESS* uniAddr = address->FirstUnicastAddress;

		if (address->IfType != IF_TYPE_SOFTWARE_LOOPBACK && address->OperStatus == IfOperStatusUp)
		{
			while (uniAddr)
			{
				if (uniAddr->Address.iSockaddrLength == sizeof(sockaddr_in))
				{
					sockaddr_in* ad = ((sockaddr_in*)uniAddr->Address.lpSockaddr);			
					addrVec.push_back(inet_ntoa(ad->sin_addr));
				}

				uniAddr = uniAddr->Next;
			}
		}

		address = address->Next;
	}

	free(addresses);

	return true;
}

IStreamBuf* NetService::CreateStreamBuf(unsigned maxSize, const std::allocator<char>& allocator)
{
	return new StreamBuf(maxSize, allocator);
}

void NetService::ReleaseStreamBuf(IStreamBuf* inst)
{
	delete inst;
}

}