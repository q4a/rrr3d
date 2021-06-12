#include "stdafx.h"

#include "net\NetConnection.h"
#include "net\NetService.h"

namespace net
{

NetConnection::NetConnection(NetService* net, unsigned id, INetConnectionUser* user): _net(net), _id(id), _user(user), _ping(0), _pingTime(0), _bytesSend(0), _bytesReceived(0), _cmdIsWrite(false), _cmdIsRead(false)
{
	_impl = _net->acceptorImpl()->NewConnection();
	_impl->RegUser(this);
}

NetConnection::~NetConnection()
{
	_impl->UnRegUser(this);
	_user = NULL;	
	Close();

	_net->acceptorImpl()->ReleaseConnection(_impl);
}

void NetConnection::CmdRead()
{
	if (!_cmdIsRead)
	{
		_cmdIsRead = true;

		DataReceive(&NetConnection::OnReceive);

		_cmdIsRead = false;
	}
}

void NetConnection::CmdWrite()
{
	if (_outputCmd.size() > 0 && !_cmdIsWrite)
	{
		_cmdIsWrite = true;

		NetDatagramHeader datagramHeader;		
		datagramHeader.time = _net->time();
		unsigned datagramSize = sizeof(datagramHeader);
		buffer_copy(_outputCmd.prepare(datagramSize), buffer(&datagramHeader, datagramSize), datagramSize);
		_outputCmd.commit(datagramSize);

		_impl->Send(buffer_cast<const void*>(_outputCmd.data()), _outputCmd.size());
	}
}

bool NetConnection::DataReceive(bool (NetConnection::*handler)(unsigned numBytes, bool success))
{
	unsigned headerSize = sizeof(NetCmdHeader);
	unsigned avSize = 0;

	if (!_impl->IsAvailable(avSize))
	{
		OnDisconnected();
		return false;
	}

	if (avSize >= headerSize)
	{
		unsigned numBytes;
		bool success = _impl->Receive(buffer_cast<void*>(_inputCmd.prepare(avSize)), avSize, numBytes);

		bool handleSuccess = (this->*handler)(numBytes, success);

		if (success && !handleSuccess)
		{
			//_impl->Flush();
		}

		return success && handleSuccess && avSize > numBytes;
	}
	else
		return false;
}

void NetConnection::OnAccepted(bool success)
{
	OnConnected(success);
}

void NetConnection::OnConnected(bool success)
{
	if (success)
	{
		if (_user)
			_user->OnConnected(this);
	}
	else
	{
		if (_user)
		{
			error_code error;
			_user->OnConnectionFailed(this, error);
		}
	}
}

void NetConnection::OnDisconnected()
{
	if (_user)
		_user->OnDisconnected(this);
}

bool NetConnection::OnReceive(unsigned numBytes, bool success)
{
	if (!success)
	{
		OnDisconnected();
		return false;
	}

	_bytesReceived += numBytes;
	_inputCmd.commit(numBytes);
	unsigned dataSize = _inputCmd.size();
	unsigned headerSize = sizeof(NetCmdHeader);

	while (dataSize >= headerSize)
	{
		buffer_copy(buffer(&_inputCmdHeader, headerSize), _inputCmd.data(), headerSize);
		dataSize -= headerSize;

		if (_inputCmdHeader.datagram)
		{
			_inputDatagram = reinterpret_cast<NetDatagramHeader&>(_inputCmdHeader);
			_inputCmd.consume(headerSize);

			if (_pingTime != _net->time())
			{
				_ping = _net->time() - _pingTime;
				_pingTime = _net->time();
			}
		}
		else if (_inputCmdHeader.size > 0)
		{
			if (dataSize < _inputCmdHeader.size)
				return true;

			_inputCmd.consume(headerSize);

			if (_user)
				_user->OnReceiveCmd(NetMessage(_id, _inputDatagram.time), _inputCmdHeader, _inputCmd.data());

			_inputCmd.consume(_inputCmdHeader.size);
			dataSize -= _inputCmdHeader.size;
		}
		else
		{
			_inputCmd.consume(headerSize);

			if (_user)
				_user->OnReceiveCmd(NetMessage(_id, _inputDatagram.time), _inputCmdHeader, _inputCmd.data());
		}
	}

	return true;
}

void NetConnection::OnSend(unsigned numBytes, bool success)
{
	_bytesSend += numBytes;
	_outputCmd.consume(numBytes);
	_cmdIsWrite = false;
	
	if (!success)
	{		
		OnDisconnected();
		return;
	}
}

void NetConnection::Accept()
{
	Close();

	_impl->Accept();
}

bool NetConnection::Connect(const Endpoint& endpoint)
{
	Close();

	return _impl->Connect(endpoint);
}

void NetConnection::Close()
{
	_impl->Close();
}

bool NetConnection::IsOpen() const
{
	return _impl->isOpen();
}

void NetConnection::SendCmd(const streambuf::const_buffers_type& bufs, unsigned size)
{
	buffer_copy(_outputCmd.prepare(size), bufs, size);
	_outputCmd.commit(size);
}

void NetConnection::SendCmd(const NetCmdHeader& header, const streambuf::const_buffers_type& bufs)
{
	CommitCmd(_outputCmd, header, bufs);
}

void NetConnection::Process(unsigned time)
{	
	if (IsOpen())
	{
		CmdRead();
		CmdWrite();
	}
}

void NetConnection::Dispatch()
{
	//for (NetStates::iterator iter = _inputState.begin(); iter != _inputState.end(); ++iter)
	//if (_user)
	//	_user->OnReceiveState(NetMessage(_id, _inputDatagram.time), iter->second.header, iter->second.Data());
	//_inputState.clear();
}

unsigned NetConnection::id() const
{
	return _id;
}

void NetConnection::id(unsigned value)
{
	_id = value;
}

INetConnectionUser* NetConnection::user() const
{
	return _user;
}

void NetConnection::user(INetConnectionUser* value)
{
	_user = value;
}

unsigned NetConnection::ping() const
{
	return _ping;
}

unsigned NetConnection::pingTime() const
{
	return _pingTime;
}

unsigned NetConnection::bytesSend() const
{
	return _bytesSend;
}

unsigned NetConnection::bytesReceived() const
{
	return _bytesReceived;
}

lsl::string NetConnection::userName() const
{
	return _impl->userName();
}

Endpoint NetConnection::localEndpoint()
{
	return _impl->localEndpoint();
}

Endpoint NetConnection::remoteEndpoint()
{
	return _impl->remoteEndpoint();
}

void NetConnection::CommitCmd(streambuf& buf, const NetCmdHeader& header, const streambuf::const_buffers_type& bufs)
{
	unsigned sizeHeader = sizeof(NetCmdHeader);
	buffer_copy(buf.prepare(sizeHeader), buffer(&header, sizeHeader), sizeHeader);
	buf.commit(sizeHeader);

	if (header.size > 0)
	{
		buffer_copy(buf.prepare(header.size), bufs, header.size);
		buf.commit(header.size);
	}
}

}