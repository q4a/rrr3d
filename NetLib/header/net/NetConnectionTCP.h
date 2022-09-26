#pragma once

namespace net
{

class NetAcceptorTCP: public INetAcceptorImpl
{
private:
	tcp::acceptor _acceptor;
	bool _isOpen;
public:
	NetAcceptorTCP(io_service& io_service);
	~NetAcceptorTCP();

	void Listen(const Endpoint& endpoint);
	void Close();

	INetConnectionImpl* NewConnection();
	void ReleaseConnection(INetConnectionImpl* connection);

	INetChannelImpl* NewChannel();
	void ReleaseChannel(INetChannelImpl* connection);

	tcp::acceptor& acceptor();
};
	
class NetConnectionTCP: public INetConnectionImpl
{
private:
	NetAcceptorTCP* _owner;
	tcp::socket _socket;
	bool _isOpen;
	bool _beep;
	int _beepCount;
	bool _isConnected;

	void OnAccepted(const error_code& error);
	void OnConnected(const error_code& error);
	void OnSendCmd(const error_code& error, std::size_t numBytes);
	void OnReceiveBeep(const error_code& error, std::size_t numBytes);
public:
	NetConnectionTCP(NetAcceptorTCP* owner);
	~NetConnectionTCP();

	virtual bool Send(const void* data, unsigned size);
	virtual bool Receive(void* data, unsigned size, unsigned& numBytes);
	virtual bool IsAvailable(unsigned& size);	

	virtual void Accept();
	virtual bool Connect(const Endpoint& endpoint);
	virtual void Close();
	virtual bool isOpen() const;

	virtual Endpoint localEndpoint();
	virtual Endpoint remoteEndpoint();
	virtual lsl::string userName() const;
};

//udp::socket::protocol_type protocol = udp::v4()

class NetChannelTCP: public INetChannelImpl
{
private:
	NetAcceptorTCP* _owner;

	udp::socket _socketReader;
	udp::socket _socketWriter;
	bool _isOpen;
	bool _isBind;
	bool _broadcast;
	Endpoint _bindEndpoint;

	void OpenWriter(bool broadcast);
	void CloseWriter();

	void OnSend(const error_code& error, std::size_t numBytes);
public:
	NetChannelTCP(NetAcceptorTCP* owner);
	~NetChannelTCP();

	virtual bool Send(const Endpoint& endpoint, const void* data, unsigned size);
	virtual bool Receive(void* data, unsigned size, unsigned& numBytes, Endpoint& remoteEndpoint, bool fromOutput);
	virtual bool IsAvailable(unsigned& size, bool fromOutput);
	virtual void Flush();

	virtual void Open(bool broadcast);
	virtual void Bind(const Endpoint& endpoint);
	virtual void Unbind();
	virtual void Close();
	virtual bool IsOpen() const;
	virtual bool IsBind() const;
	virtual bool broadcast() const;
	virtual Endpoint bindEndpoint() const;

	virtual Endpoint localEndpoint();
};

}