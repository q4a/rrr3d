#pragma once

namespace r3d
{

namespace game
{

class NetPlayer;

struct NetEventData
{
	unsigned sender;
	bool failed;

	NetEventData() {}
	NetEventData(unsigned mSender, bool mFailed): sender(mSender), failed(mFailed) {}
};

class INetGameUser: public Object
{
public:		
	virtual bool OnConnected(net::INetConnection* sender) {return true;}
	virtual void OnDisconnected(net::INetConnection* sender) {}
	virtual void OnConnectionFailed(net::INetConnection* sender, unsigned error) {}
	virtual void OnReceiveCmd(const net::NetMessage& msg, const net::NetCmdHeader& header, const void* data, unsigned size) {}
	virtual void OnPingComplete() {}
	virtual void OnFailed(unsigned error) {}

	virtual void OnConnectedPlayer(NetPlayer* sender) {}
	virtual void OnDisconnectedPlayer(NetPlayer* sender) {}
	virtual void OnProcessNetEvent(unsigned id, NetEventData* data) {}
};

//player
enum NetEvent {
	cNetPlayerReady = 0,
	cNetPlayerGoWait,
	cNetPlayerFinish,
	cNetPlayerSetColor,
	cNetPlayerSetGamerId,
	cNetRacePushLine,
};

}

}