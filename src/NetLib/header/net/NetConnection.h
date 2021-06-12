#pragma once

namespace net
{

struct NetDatagramHeader
{
	unsigned datagram: 1;
	unsigned time: 31;

	NetDatagramHeader(): datagram(1) {}
};

class NetConnection;

class INetConnectionUser
{
public:	
	virtual void OnConnected(NetConnection* sender) {}
	virtual void OnDisconnected(NetConnection* sender) {}
	virtual void OnConnectionFailed(NetConnection* sender, const error_code& error) {}
	virtual void OnReceiveCmd(const NetMessage& msg, const NetCmdHeader& header, const streambuf::const_buffers_type& bufs) {}
};

class NetConnection: public INetConnection, INetConnectionImplUser
{	
private:
	NetService* _net;
	unsigned _id;
	INetConnectionUser* _user;
	INetConnectionImpl* _impl;

	unsigned _ping;
	unsigned _pingTime;
	unsigned _bytesSend;
	unsigned _bytesReceived;

	bool _cmdIsWrite;
	bool _cmdIsRead;	
	NetDatagramHeader _inputDatagram;
	NetCmdHeader _inputCmdHeader;
	streambuf _inputCmd;
	streambuf _outputCmd;

	//async model
	//same for rpc
	
	void CmdRead();
	void CmdWrite();
	//template<typename MutableBufferSequence> bool CmdReceive(const MutableBufferSequence& bufs, bool (NetConnection::*handler)(unsigned numBytes, bool success));
	bool DataReceive(bool (NetConnection::*handler)(unsigned numBytes, bool success));

	void OnAccepted(bool success);
	void OnConnected(bool success);
	void OnDisconnected();

	//bool OnReceiveCmdHeader(unsigned numBytes, bool success);
	//bool OnReceiveCmd(unsigned numBytes, bool success);
	bool OnReceive(unsigned numBytes, bool success);
	void OnSend(unsigned numBytes, bool success);
public:
	NetConnection(NetService* net, unsigned id, INetConnectionUser* user);
	~NetConnection();

	void Accept();
	bool Connect(const Endpoint& endpoint);
	void Close();
	bool IsOpen() const;
	
	void SendCmd(const streambuf::const_buffers_type& bufs, unsigned size);
	void SendCmd(const NetCmdHeader& header, const streambuf::const_buffers_type& bufs);

	void Process(unsigned time);
	void Dispatch();

	unsigned id() const;
	void id(unsigned value);

	INetConnectionUser* user() const;
	void user(INetConnectionUser* value);

	unsigned ping() const;
	unsigned pingTime() const;
	unsigned bytesSend() const;
	unsigned bytesReceived() const;

	lsl::string userName() const;

	Endpoint localEndpoint();
	Endpoint remoteEndpoint();

	static void CommitCmd(streambuf& buf, const NetCmdHeader& header, const streambuf::const_buffers_type& bufs);
};

}