#pragma once

#include "BitStream.h"

namespace net
{

class INetPlayer;

class NetModel
{
public:
	struct Desc
	{
		INetPlayer* player;
		unsigned classId;
		unsigned id;
		bool owner;
		unsigned ownerId;
		std::istream& stream;

		Desc(INetPlayer* mPlayer, unsigned mClassId, unsigned mId, bool mOwner, unsigned mOwnerId, std::istream& mStream): player(mPlayer), classId(mClassId), id(mId), owner(mOwner), ownerId(mOwnerId), stream(mStream) {}
	};

	enum SyncState {ssOff = 0, ssDelta};

	static const unsigned cSyncRPC = 0;
	static const unsigned cSysRPCEnd = 1;
private:
	INetPlayer* _player;
	unsigned _classId;
	unsigned _id;
	bool _owner;
	unsigned _ownerId;

	SyncState _syncState;
	unsigned _syncStateTarget;
	unsigned _syncStatePriority;
protected:
	NETLIB_API virtual void OnDescWrite(const NetMessage& msg, std::ostream& stream) {}
	NETLIB_API virtual void OnStateRead(const NetMessage& msg, const NetCmdHeader& header, std::istream& stream) {}		
	NETLIB_API virtual void OnStateWrite(const NetMessage& msg, std::ostream& stream) {}

	NETLIB_API std::ostream& NewRPC(unsigned target, unsigned rpc);
	NETLIB_API void CloseRPC();
	NETLIB_API virtual void CallRPC(const NetMessage& msg, const NetCmdHeader& header, std::istream& stream) = 0;
	
	NETLIB_API virtual void OnSerialize(const NetMessage& msg, BitStream& stream) {}
public:
	NETLIB_API NetModel(const Desc& desc);
	NETLIB_API virtual ~NetModel();

	NETLIB_API void ProcessCmd(const NetMessage& msg, const NetCmdHeader& header, std::istream& stream);
	NETLIB_API void Serialize(const NetMessage& msg, BitStream& stream);
	
	NETLIB_API void DescWrite(const NetMessage& msg, std::ostream& stream);
	NETLIB_API void StateRead(const NetMessage& msg, const NetCmdHeader& header, std::istream& stream);
	NETLIB_API void StateWrite(const NetMessage& msg, std::ostream& stream);	

	NETLIB_API void Sync(unsigned target);
	NETLIB_API void Sync();

	NETLIB_API SyncState syncState() const;
	NETLIB_API void syncState(SyncState value);

	NETLIB_API unsigned syncStateTarget() const;
	NETLIB_API void syncStateTarget(unsigned value);

	NETLIB_API unsigned syncStatePriority() const;
	NETLIB_API void syncStatePriority(unsigned value);

	NETLIB_API INetPlayer* player();
	NETLIB_API INetService* net();
	NETLIB_API unsigned classId() const;
	NETLIB_API unsigned id() const;
	//id == ownerId
	NETLIB_API bool owner() const;
	//corresponds to id of logical connection (or NetPlayer::id) that owns the object
	NETLIB_API unsigned ownerId() const;
};

template<class _Class> class NetModelRPC: public NetModel
{
protected:
	typedef void (_Class::*RPC)(const NetMessage& msg, const NetCmdHeader& header, std::istream& stream);
private:
	RPC _rpcList[cMaxRPC];
	unsigned _rpcCount;
protected:
	NetModelRPC(const Desc& desc);

	unsigned RegRPC(RPC rpc);
	using NetModel::NewRPC;
	std::ostream& NewRPC(unsigned target, RPC rpc);
	void MakeRPC(unsigned target, RPC rpc);

	virtual void CallRPC(const NetMessage& msg, const NetCmdHeader& header, std::istream& stream);
};




template<class _Class> NetModelRPC<_Class>::NetModelRPC(const Desc& desc): NetModel(desc), _rpcCount(0)
{
}

template<class _Class> unsigned NetModelRPC<_Class>::RegRPC(RPC rpc)
{
	unsigned index = _rpcCount++;
	_rpcList[index] = rpc;
	return index;
}

template<class _Class> std::ostream& NetModelRPC<_Class>::NewRPC(unsigned target, RPC rpc)
{
	unsigned index = cMaxRPC;
	for (unsigned i = 0; i < _rpcCount; ++i)
		if (_rpcList[i] == rpc)
		{
			index = i;
			break;
		}

	LSL_ASSERT(index != cMaxRPC);

	return NewRPC(target, cSysRPCEnd + index);
}

template<class _Class> void NetModelRPC<_Class>::MakeRPC(unsigned target, RPC rpc)
{
	NewRPC(target, rpc);
	CloseRPC();
}

template<class _Class> void NetModelRPC<_Class>::CallRPC(const NetMessage& msg, const NetCmdHeader& header, std::istream& stream)
{
	RPC method = _rpcList[header.rpc - cSysRPCEnd];
	if (method)
		((_Class*)this->*method)(msg, header, stream);
}


}