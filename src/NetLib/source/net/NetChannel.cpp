#include "stdafx.h"

#include "net\NetChannel.h"
#include "net\NetService.h"

#include "net\NetConnectionTCP.h"

namespace net
{

NetChannel::NetChannel(NetService* net, INetChannelUser* user): _net(net), _user(user), _ping(0), _pingTime(0), _bytesSend(0), _bytesReceived(0), _dataIsWrite(false), _dataIsRead(false), _broadcast(false), _dataIsReadOutStart(false)
{
	_impl = _net->acceptorImpl()->NewChannel();
	_impl->RegUser(this);
}

NetChannel::~NetChannel()
{
	_impl->UnRegUser(this);
	_user = NULL;

	Close();

	_net->acceptorImpl()->ReleaseChannel(_impl);
}

void NetChannel::DataRead(bool fromOutput)
{
	if (!_dataIsRead)
	{
		int packetCount = 0;

		while (packetCount < 128)
		{
			if (!DataReceive(fromOutput, &NetChannel::OnReceive))
				break;

			++packetCount;
		}
	}
}

void NetChannel::DataWrite()
{
	if (_outputStates.size() > 0 && !_dataIsWrite)
	{
		_dataIsWrite = true;

		struct Output
		{
			NetStates::const_iterator state;
		};
		typedef std::list<Output> OutputList;
		typedef std::map<Endpoint, OutputList> OutputMap;

		OutputMap outputMap;

		for (NetStates::iterator iter = _outputStates.begin(); iter != _outputStates.end(); ++iter)
		{
			NetState& state = iter->second;
			state.header.time = _net->time();

			for (EndpointList::const_iterator iter2 = state.endpointList.begin(); iter2 != state.endpointList.end(); ++iter2)
			{
				OutputMap::iterator iter3 = outputMap.find(*iter2);
				if (iter3 == outputMap.end())
					iter3 = outputMap.insert(iter3, OutputMap::value_type(*iter2, OutputList()));

				Output output;
				output.state = iter;
				iter3->second.push_back(output);
			}
		}

		unsigned headerSize = NetStateHeader::SizeOf();
		unsigned offset = 0;		

		for (OutputMap::const_iterator iter = outputMap.begin(); iter != outputMap.end(); ++iter)
		{
			unsigned size = 0;

			for (OutputList::const_iterator iter2 = iter->second.begin(); iter2 != iter->second.end(); ++iter2)
			{
				const NetState& state = iter2->state->second;
				unsigned dataSize = state.header.size;
				unsigned totalSize = headerSize + dataSize;

				LSL_ASSERT(totalSize <= cDatagramSize);

				if (totalSize > cDatagramSize)
				{
					LSL_LOG("NetChannel::DataWrite udp packet is very large");
					continue;
				}

				buffer_copy(_outputData.prepare(headerSize), buffer(&state.header, headerSize), headerSize);
				_outputData.commit(headerSize);

				buffer_copy(_outputData.prepare(dataSize), state.bufs(), dataSize);
				_outputData.commit(dataSize);

				size += totalSize;

				if (size >= cDatagramSize)
				{
					unsigned packetSize = size <= cDatagramSize ? size : size - totalSize;

					const_buffer bufs = _outputData.data();
					DataSend(iter->first, buffer(bufs + offset, packetSize));
					offset += packetSize;
					size -= packetSize;
				}
			}

			if (size > 0)
			{
				const_buffer bufs = _outputData.data();
				DataSend(iter->first, buffer(bufs + offset, size));
			}
		}

		_outputStates.clear();
	}
}

void NetChannel::DataSend(const Endpoint& endpoint, const streambuf::const_buffers_type& bufs)
{
	_impl->Send(endpoint, buffer_cast<const void*>(bufs), buffer_size(bufs));
}

void NetChannel::DataSend(const EndpointList& endpointList, const streambuf::const_buffers_type& bufs)
{
	for (EndpointList::const_iterator iter = endpointList.begin(); iter != endpointList.end(); ++iter)
		DataSend(*iter, bufs);
}

bool NetChannel::DataReceive(bool fromOutput, bool (NetChannel::*handler)(unsigned numBytes, bool success))
{
	unsigned headerSize = NetStateHeader::SizeOf();
	unsigned avSize = 0;

	if (!_impl->IsAvailable(avSize, fromOutput))
	{
		if (_user)
		{
			//error_code error;
			//_user->OnIOFailed(error);
		}
		return false;
	}

	if (avSize >= headerSize)
	{
		unsigned numBytes;
		bool success = _impl->Receive(buffer_cast<void*>(_inputData.prepare(avSize)), avSize, numBytes, _remoteEndpoint, fromOutput);
		
		bool handleSuccess = (this->*handler)(numBytes, success);

		if (success && !handleSuccess)
		{
			_impl->Flush();
		}

		return success && handleSuccess;
	}
	else
		return false;
}

bool NetChannel::OnReceive(unsigned numBytes, bool success)
{
	if (!success)
	{
		//for crit errors (buffer overflow for example)
		//if (_user)
		//	_user->OnIOFailed(error);
		return false;
	}

	_bytesReceived += numBytes;	
	unsigned headerSize = NetStateHeader::SizeOf();	

	while (numBytes >= headerSize)
	{
		_inputData.commit(headerSize);
		buffer_copy(buffer(&_inputHeader, headerSize), _inputData.data(), headerSize);
		_inputData.consume(headerSize);

		numBytes -= headerSize;
		unsigned dataSize = _inputHeader.size;

		if (_pingTime != _net->time())
		{
			_ping = _net->time() - _pingTime;
			_pingTime = _net->time();
		}

		if (dataSize > numBytes)
			return false;

		_inputData.commit(dataSize);

		if (_user)
			_user->OnReceiveState(NetMessage(_inputHeader.sender, _inputHeader.time), _inputHeader, _inputData.data(), _remoteEndpoint);

		_inputData.consume(dataSize);
		numBytes -= dataSize;
	}

	return true;
}

void NetChannel::OnSend(unsigned numBytes, bool success)
{
	//LSL_LOG("NetChannel::OnSend msg=" + error.message() + lsl::StrFmt(" numBytes=%d", numBytes));
	//LSL_LOG(lsl::StrFmt("DDD local add=%s port=%d", _socketWriter.local_endpoint().address().to_string().c_str(), _socketWriter.local_endpoint().port()));

	_bytesSend += numBytes;

	if (_dataIsWrite)
		_outputData.consume(_outputData.size());
	_dataIsWrite = false;

	if (!success)
	{
		//for crit errors (buffer overflow for example)
		//if (_user)
		//	_user->OnIOFailed(error);
		return;
	}

	if (_user)
		_user->OnSend(numBytes);
}

INetChannelUser* NetChannel::user() const
{
	return _user;
}

void NetChannel::user(INetChannelUser* value)
{
	_user = value;
}

void NetChannel::Open(bool broadcast)
{
	_impl->Open(broadcast);
}

void NetChannel::Bind(const Endpoint& endpoint)
{
	_impl->Bind(endpoint);
}

void NetChannel::Unbind()
{
	_impl->Unbind();
}

void NetChannel::Close()
{
	_impl->Close();
}

bool NetChannel::IsOpen() const
{
	return _impl->IsOpen();
}

bool NetChannel::IsBind() const
{
	return _impl->IsBind();
}

void NetChannel::SendState(const EndpointList& endpointList, const NetStateHeader& header, const streambuf::const_buffers_type& bufs)
{
	NetStates::iterator iter = _outputStates.find(header.id);
	if (iter == _outputStates.end())
		iter = _outputStates.insert(iter, NetStates::value_type(header.id, NetState()));

	iter->second.header = header;
	iter->second.endpointList = endpointList;
	buffer_copy(iter->second.Prepare(header.size), bufs);
}

void NetChannel::SendState(const Endpoint& endpoint, const NetStateHeader& header, const streambuf::const_buffers_type& bufs)
{
	EndpointList endpointList;
	endpointList.push_back(endpoint);

	SendState(endpointList, header, bufs);
}

void NetChannel::StartReceiveResponseOut()
{
	if (_dataIsReadOutStart)
		return;
	_dataIsReadOutStart = true;
}

void NetChannel::StopReceiveResponseOut()
{
	if (_dataIsReadOutStart)
	{
		_dataIsReadOutStart = false;
		bool broadcast = _impl->broadcast();

		Close();
		Open(broadcast);
	}
}

void NetChannel::Process(unsigned time)
{	
	if (IsOpen())
	{
		bool isBind = IsBind();

		if (isBind)
			DataRead(false);

		if (isBind || _dataIsReadOutStart)
			DataWrite();

		if (_dataIsReadOutStart)
			DataRead(true);
	}
}

void NetChannel::Dispatch()
{
	//for (NetStates::iterator iter = _inputState.begin(); iter != _inputState.end(); ++iter)
	//if (_user)
	//	_user->OnReceiveState(NetMessage(_id, _inputDatagram.time), iter->second.header, iter->second.Data());
	//_inputState.clear();
}

unsigned NetChannel::ping() const
{
	return _ping;
}

unsigned NetChannel::pingTime() const
{
	return _pingTime;
}

unsigned NetChannel::bytesSend() const
{
	return _bytesSend;
}

unsigned NetChannel::bytesReceived() const
{
	return _bytesReceived;
}

Endpoint NetChannel::localEndpoint()
{
	return _impl->localEndpoint();
}

Endpoint NetChannel::remoteEndpoint()
{
	return _remoteEndpoint;
}

}