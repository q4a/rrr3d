#pragma once

#include "NetConnection.h"
#include "NetChannel.h"
#include "NetPlayer.h"

namespace net
{

class NetClient: public NetPlayer, INetConnectionUser
{
private:	
	NetConnection* _connection;	
	bool _connected;
protected:	
	virtual void SendCmd(const NetCmdHeader& header, const streambuf::const_buffers_type& bufs);
	virtual void OnProcessCmd(const NetMessage& msg, const NetCmdHeader& header, streambuf& streambuf, std::istream& stream);

	virtual void SendState(const NetStateHeader& header, const streambuf::const_buffers_type& bufs, unsigned target);

	virtual void OnConnected(NetConnection* sender);
	virtual void OnDisconnected(NetConnection* sender);
	virtual void OnConnectionFailed(NetConnection* sender, const error_code& error);

	virtual void OnReceiveCmd(const NetMessage& msg, const NetCmdHeader& header, const streambuf::const_buffers_type& bufs);		

	virtual void OnProcess(unsigned time);
	virtual void OnDispatch();
public:
	NetClient(NetService* net);
	virtual ~NetClient();

	bool Connect(const Endpoint& endpoint);
	void Disconnect();
	bool isConnecting() const;
	bool connected() const;

	NetConnection* connection();
	NetChannel* channel();
};

}