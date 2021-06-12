#include "stdafx.h"

#include "net\NetClient.h"
#include "net\NetService.h"

namespace net
{

NetClient::NetClient(NetService* net): NetPlayer(net, cUndefPlayer), _connection(0), _connected(false)
{
}

NetClient::~NetClient()
{
	Disconnect();
}

void NetClient::SendCmd(const NetCmdHeader& header, const streambuf::const_buffers_type& bufs)
{
	_connection->SendCmd(header, bufs);
}

void NetClient::OnProcessCmd(const NetMessage& msg, const NetCmdHeader& header, streambuf& streambuf, std::istream& stream)
{
	if (header.id == cDefCmd)
	{
		switch (header.rpc)
		{
		case cSetPlayerId:
			unsigned id;
			Read(stream, id);

			LSL_LOG(lsl::StrFmt("NetClient SetPlayerId id=%d", id));
			
			this->id(id);
			_connected = true;

			_net->OnConnected(_connection);
			break;
		}
	}
}

void NetClient::SendState(const NetStateHeader& header, const streambuf::const_buffers_type& bufs, unsigned target)
{
	channel()->SendState(_connection->remoteEndpoint(), header, bufs);
}

void NetClient::OnConnected(NetConnection* sender)
{
	tcp::endpoint local, remote;
	GetEndpointTCP(_connection->localEndpoint(), local);
	GetEndpointTCP(_connection->remoteEndpoint(), remote);

	LSL_LOG(lsl::StrFmt("NetClient connected local=%s:%d remote=%s:%d", local.address().to_string().c_str(), local.port(), remote.address().to_string().c_str(), remote.port()));

	_net->channel()->Bind(_connection->localEndpoint());
}

void NetClient::OnDisconnected(NetConnection* sender)
{
	LSL_LOG("NetClient disconnected");

	if (!_net->OnDisconnected(sender))
		return;

	Disconnect();
}

void NetClient::OnConnectionFailed(NetConnection* sender, const error_code& error)
{
	LSL_LOG("NetClient conection failed " + error.message());

	if (!_net->OnConnectionFailed(sender, error.value()))
		return;

	Disconnect();
}

void NetClient::OnReceiveCmd(const NetMessage& msg, const NetCmdHeader& header, const streambuf::const_buffers_type& bufs)
{
	LSL_TRACE(lsl::StrFmt("NetClient OnReceiveCmd sender=%d id=%d rpc=%d size=%d", msg.sender, header.id, header.rpc, header.size));

	ProcessCmd(msg, header, bufs);

	_net->OnReceiveCmd(msg, header, buffer_cast<const void*>(bufs), buffer_size(bufs));
}

void NetClient::OnProcess(unsigned time)
{
	_connection->Process(time);	
}

void NetClient::OnDispatch()
{
	_connection->Dispatch();	
}

bool NetClient::Connect(const Endpoint& endpoint)
{
	_connection = _net->NewConnection(cServerPlayer, this);

	return _connection->Connect(endpoint);
}

void NetClient::Disconnect()
{
	DeleteModels(true);

	NetConnection* connection = _connection;
	_connection = NULL;

	//lsl::SafeDelete(connection);

	if (connection)
	{
		connection->Close();
		_net->ReleaseConnection(connection);
	}
	connection = NULL;

	channel()->Unbind();
}

bool NetClient::isConnecting() const
{
	return _connection != NULL && !_connected;
}

bool NetClient::connected() const
{
	return _connected;
}

NetConnection* NetClient::connection()
{
	return _connection;
}

NetChannel* NetClient::channel()
{
	return _net->channel();
}

}