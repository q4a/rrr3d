#include "stdafx.h"

#include "net\NetConnectionTCP.h"

namespace net
{

NetAcceptorTCP::NetAcceptorTCP(io_service& io_service): _acceptor(io_service), _isOpen(false)
{
}

NetAcceptorTCP::~NetAcceptorTCP()
{
	Close();
}
	
void NetAcceptorTCP::Listen(const Endpoint& endpoint)
{
	if (_isOpen)
		return;
	_isOpen = true;

	_acceptor.open(tcp::v4());
	_acceptor.bind(tcp::endpoint(tcp::v4(), endpoint.port));
	_acceptor.listen();
}

void NetAcceptorTCP::Close()
{
	if (!_isOpen)
		return;
	_isOpen = false;

	_acceptor.close();
}

INetConnectionImpl* NetAcceptorTCP::NewConnection()
{
	return new NetConnectionTCP(this);
}

void NetAcceptorTCP::ReleaseConnection(INetConnectionImpl* connection)
{
	delete static_cast<NetConnectionTCP*>(connection);
}

INetChannelImpl* NetAcceptorTCP::NewChannel()
{
	return new NetChannelTCP(this);
}

void NetAcceptorTCP::ReleaseChannel(INetChannelImpl* connection)
{
	delete static_cast<NetChannelTCP*>(connection);
}

tcp::acceptor& NetAcceptorTCP::acceptor()
{
	return _acceptor;
}

	


NetConnectionTCP::NetConnectionTCP(NetAcceptorTCP* owner): _owner(owner), _socket(owner->acceptor().get_io_service()), _isOpen(false), _beep(false), _beepCount(0), _isConnected(false)
{
}

NetConnectionTCP::~NetConnectionTCP()
{
	Close();
}

void NetConnectionTCP::OnAccepted(const error_code& error)
{
	if (!_isOpen)
		return;

	if (error)
	{
		LSL_LOG("OnAccepted failed " + error.message());

		NotifyAccepted(false);
	}
	else
	{
		_isConnected = true;
		NotifyAccepted(true);
	}
}

void NetConnectionTCP::OnConnected(const error_code& error)
{
	if (!_isOpen)
		return;

	_socket.non_blocking(true);

	if (error)
	{
		LSL_LOG("OnConnected failed " + error.message());

		NotifyConnected(false);
	}
	else
	{
		_isConnected = true;
		NotifyConnected(true);
	}
}

void NetConnectionTCP::OnSendCmd(const error_code& error, std::size_t numBytes)
{
	if (!_isOpen)
		return;
	
	if (error)
	{
		LSL_LOG("OnSendCmd msg=" + error.message());

		NotifySend(numBytes, false);
	}
	else
	{
		NotifySend(numBytes, true);
	}
}

void NetConnectionTCP::OnReceiveBeep(const error_code& error, std::size_t numBytes)
{
	_beep = false;

	if (error)
	{
		NotifyDisconnected();
	}
}

bool NetConnectionTCP::Send(const void* data, unsigned size)
{
	_socket.async_send(buffer(data, size), boost::bind(&NetConnectionTCP::OnSendCmd, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));

	return true;
}

bool NetConnectionTCP::Receive(void* data, unsigned size, unsigned& numBytes)
{
	error_code error;
	numBytes = _socket.receive(buffer(data, size), 0, error);

	if (error)
	{
		LSL_LOG(error.message());
		return false;
	}

	return true;
}

bool NetConnectionTCP::IsAvailable(unsigned& size)
{
	if (!_socket.is_open())
		return false;

	error_code err;
	size = _socket.available(err);
	if (err)
		return false;

	if (size == 0 && !_beep && _isConnected && (++_beepCount) > 60)
	{
		_beepCount = 0;
		_beep = true;
		_socket.async_receive(buffer(&_beep, 1), tcp::socket::message_peek, boost::bind(&NetConnectionTCP::OnReceiveBeep, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
	}

	return true;
}

void NetConnectionTCP::Accept()
{
	Close();

	_isOpen = true;
	
	_owner->acceptor().async_accept(_socket, boost::bind(&NetConnectionTCP::OnAccepted, this, boost::asio::placeholders::error));
}

bool NetConnectionTCP::Connect(const Endpoint& endpoint)
{
	tcp::endpoint endpointTCP;
	if (!GetEndpointTCP(endpoint, endpointTCP))
		return false;

	Close();

	_isOpen = true;

	_socket.async_connect(endpointTCP, boost::bind(&NetConnectionTCP::OnConnected, this, boost::asio::placeholders::error));

	return true;
}

void NetConnectionTCP::Close()
{
	if (_isOpen)
	{
		_isConnected = false;
		_isOpen = false;
		_beep = false;
		_beepCount = 0;

		if (_socket.is_open())
		{
			error_code error;
			_socket.shutdown(tcp::socket::shutdown_both, error);
			if (error)
				LSL_LOG(error.message());
		}

		_socket.close();
	}
}

bool NetConnectionTCP::isOpen() const
{
	return _isOpen;
}

Endpoint NetConnectionTCP::localEndpoint()
{
	tcp::endpoint endpoint = _socket.local_endpoint();

	return Endpoint(endpoint.address().to_v4().to_ulong(), endpoint.port());
}

Endpoint NetConnectionTCP::remoteEndpoint()
{
	tcp::endpoint endpoint = _socket.remote_endpoint();

	return Endpoint(endpoint.address().to_v4().to_ulong(), endpoint.port());
}

lsl::string NetConnectionTCP::userName() const
{
	return "";
}




NetChannelTCP::NetChannelTCP(NetAcceptorTCP* owner): _owner(owner), _socketReader(owner->acceptor().get_io_service()), _socketWriter(owner->acceptor().get_io_service()), _isOpen(false), _isBind(false), _broadcast(false)
{
}

NetChannelTCP::~NetChannelTCP()
{
	Close();
}

void NetChannelTCP::OpenWriter(bool broadcast)
{
	_socketWriter.open(udp::v4());
	_socketWriter.set_option(socket_base::broadcast(broadcast));
}

void NetChannelTCP::CloseWriter()
{
	if (_socketWriter.is_open())
	{
		error_code error;
		_socketWriter.shutdown(tcp::socket::shutdown_both, error);
		if (error)
			LSL_LOG(error.message());
	}

	_socketWriter.close();
}

void NetChannelTCP::OnSend(const error_code& error, std::size_t numBytes)
{
	if (!_isOpen)
		return;
	
	if (error)
	{
		LSL_LOG("OnSendCmd msg=" + error.message());

		NotifySend(numBytes, false);
	}
	else
	{
		NotifySend(numBytes, true);
	}
}

bool NetChannelTCP::Send(const Endpoint& endpoint, const void* data, unsigned size)
{
	tcp::endpoint tcpEndpoint;
	GetEndpointTCP(endpoint, tcpEndpoint);

	_socketWriter.async_send_to(buffer(data, size), udp::endpoint(tcpEndpoint.address(), endpoint.port), boost::bind(&NetChannelTCP::OnSend, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));

	return true;
}

bool NetChannelTCP::Receive(void* data, unsigned size, unsigned& numBytes, Endpoint& remoteEndpoint, bool fromOutput)
{
	error_code error;
	udp::endpoint udpEndpoint;
	numBytes = !fromOutput ? _socketReader.receive_from(buffer(data, size), udpEndpoint, 0, error) : _socketWriter.receive_from(buffer(data, size), udpEndpoint, 0, error);

	if (error)
	{
		LSL_LOG(error.message());
		return false;
	}

	remoteEndpoint = Endpoint(udpEndpoint.address().to_v4().to_ulong(), udpEndpoint.port());

	return true;
}

bool NetChannelTCP::IsAvailable(unsigned& size, bool fromOutput)
{
	if (!(!fromOutput ? _socketReader.is_open() : _socketWriter.is_open()))
		return false;

	error_code err;
	size = !fromOutput ? _socketReader.available(err) : _socketWriter.available(err);
	if (err)
		return false;

	return true;
}

void NetChannelTCP::Flush()
{
	Close();
	Open(_broadcast);
	Bind(_bindEndpoint);
}

void NetChannelTCP::Open(bool broadcast)
{
	if (!_isOpen)
	{
		_isOpen = true;
		
		_broadcast = broadcast;

		OpenWriter(broadcast);
	}
}

void NetChannelTCP::Bind(const Endpoint& endpoint)
{
	if (_isBind)
		Unbind();

	_isBind = true;
	_bindEndpoint = endpoint;

	_socketReader.open(udp::v4());
	_socketReader.bind(udp::endpoint(udp::v4(), endpoint.port));

	//_socket.non_blocking(true);
}

void NetChannelTCP::Unbind()
{
	if (!_isBind)
		return;

	_isBind = false;

	if (_socketReader.is_open())
	{
		error_code error;
		_socketReader.shutdown(tcp::socket::shutdown_both, error);
		if (error)
			LSL_LOG(error.message());
	}

	_socketReader.close();
}

void NetChannelTCP::Close()
{
	if (_isOpen)
	{
		_isOpen = false;

		Unbind();
		CloseWriter();
	}
}

bool NetChannelTCP::IsOpen() const
{
	return _isOpen;
}

bool NetChannelTCP::IsBind() const
{
	return _isBind;
}

bool NetChannelTCP::broadcast() const
{
	return _broadcast;
}

Endpoint NetChannelTCP::bindEndpoint() const
{
	return _bindEndpoint;
}

Endpoint NetChannelTCP::localEndpoint()
{
	udp::endpoint endpoint = _socketWriter.local_endpoint();

	return Endpoint(endpoint.address().to_v4().to_ulong(), endpoint.port());
}

}