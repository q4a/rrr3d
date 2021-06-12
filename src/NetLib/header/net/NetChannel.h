#pragma once

namespace net
{

typedef std::list<Endpoint> EndpointList;
typedef std::list<udp::endpoint> EndpointUdpList;

struct NetState
{
	NetStateHeader header;
	shared_ptr<BYTE> data;
	unsigned size;
	EndpointList endpointList;

	NetState(): size(0) {}

	streambuf::mutable_buffers_type Prepare(unsigned size)
	{
		if (this->size != size)
		{
			this->size = size;

			if (size > 0)
				data.reset((BYTE*)malloc(size));
			else
				Release();
		}

		return bufs();
	}

	void Release()
	{
		data.reset();
	}

	streambuf::mutable_buffers_type bufs() {return streambuf::mutable_buffers_type(data.get(), size);}
	streambuf::const_buffers_type bufs() const {return streambuf::const_buffers_type(data.get(), size);}
};

typedef std::map<unsigned, NetState> NetStates;

class INetChannelUser
{
public:	
	virtual void OnReceiveState(const NetMessage& msg, const NetStateHeader& header, const streambuf::const_buffers_type& bufs, const Endpoint& remoteEndpoint) {}
	virtual void OnSend(std::size_t numBytes) {}
	virtual void OnIOFailed(const error_code& error) {}
};

class NetChannel: public INetChannel, INetChannelImplUser
{
private:
	static const int cDatagramSize = 512;
private:
	NetService* _net;
	INetChannelUser* _user;
	INetChannelImpl* _impl;
	bool _broadcast;

	unsigned _ping;
	unsigned _pingTime;
	unsigned _bytesSend;
	unsigned _bytesReceived;

	bool _dataIsRead;
	bool _dataIsReadOutStart;
	NetStateHeader _inputHeader;
	streambuf _inputData;
	Endpoint _remoteEndpoint;

	bool _dataIsWrite;
	streambuf _outputData;
	NetStates _outputStates;
	
	void DataRead(bool fromOutput);	
	void DataWrite();
	void DataSend(const Endpoint& endpoint, const streambuf::const_buffers_type& bufs);
	void DataSend(const EndpointList& endpointList, const streambuf::const_buffers_type& bufs);
	bool DataReceive(bool fromOutput, bool (NetChannel::*handler)(unsigned numBytes, bool success));

	bool OnReceive(unsigned numBytes, bool success);
	void OnSend(unsigned numBytes, bool success);
public:
	NetChannel(NetService* net, INetChannelUser* user);
	~NetChannel();

	INetChannelUser* user() const;
	void user(INetChannelUser* value);

	void Open(bool broadcast);
	void Bind(const Endpoint& endpoint);
	void Unbind();
	void Close();
	bool IsOpen() const;
	bool IsBind() const;

	void SendState(const EndpointList& endpointList, const NetStateHeader& header, const streambuf::const_buffers_type& bufs);
	void SendState(const Endpoint& endpoint, const NetStateHeader& header, const streambuf::const_buffers_type& bufs);
	
	void StartReceiveResponseOut();
	void StopReceiveResponseOut();

	void Process(unsigned time);
	void Dispatch();	
	
	unsigned ping() const;
	unsigned pingTime() const;
	unsigned bytesSend() const;
	unsigned bytesReceived() const;

	Endpoint localEndpoint();
	Endpoint remoteEndpoint();
};

}