#pragma once

#pragma once

#include "NetConnection.h"
#include "NetChannel.h"
#include "NetPlayer.h"

namespace net
{

class NetServer: public NetPlayer, INetConnectionUser
{
private:
	struct CmdInst
	{
		NetCmdHeader header;
		shared_ptr<BYTE> data;
		unsigned size;

		CmdInst(const NetCmdHeader& mHeader, const streambuf::const_buffers_type& mBufs): header(mHeader)
		{
			size = buffer_size(mBufs);
			if (size > 0)
			{
				data.reset((BYTE*)malloc(size));
				buffer_copy(bufs(), mBufs, size);
			}
		}

		streambuf::mutable_buffers_type bufs() {return streambuf::mutable_buffers_type(data.get(), size);}
		streambuf::const_buffers_type bufs() const {return streambuf::const_buffers_type(data.get(), size);}
	};
	typedef std::vector<CmdInst> CmdList;
public:
	typedef lsl::Vector<NetConnection*> Connections;
private:	
	unsigned _port;
	bool _started;
	unsigned _lastModelId;
	NetConnection* _newConnection;
	Connections _connections;

	//streambuf _bufferedRPC;
	CmdList _processCmdList;
	unsigned _processCmdRef;

	void NewConnection(NetConnection* connection);
	void ReleaseConnection(NetConnection* connection);
protected:	
	void ProcessCmd(const NetMessage& msg, const NetCmdHeader& header, const streambuf::const_buffers_type& bufs);
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
	NetServer(NetService* net);
	virtual ~NetServer();

	void Start(unsigned port);
	void Shutdown();
	bool started() const;
	unsigned port() const;

	void Disconnect(NetConnection* connection);

	NetConnection* GetConnection(unsigned index);
	NetConnection* GetConnectionById(unsigned id);
	unsigned connectionCount() const;

	NetChannel* channel();
};

}