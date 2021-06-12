#include "stdafx.h"

#include "net\NetModel.h"

namespace net
{

NetModel::NetModel(const Desc& desc): _player(desc.player), _classId(desc.classId), _id(desc.id), _owner(desc.owner), _ownerId(desc.ownerId), _syncState(ssOff), _syncStateTarget(cNetTargetOthers | desc.ownerId), _syncStatePriority(cSyncStatePriorityDef)
{
}

NetModel::~NetModel()
{
	syncState(ssOff);
}

std::ostream& NetModel::NewRPC(unsigned target, unsigned rpc)
{
	return _player->NewCmd(_id, target, rpc);
}

void NetModel::CloseRPC()
{
	_player->CloseCmd();
}

void NetModel::ProcessCmd(const NetMessage& msg, const NetCmdHeader& header, std::istream& stream)
{
	if (header.id == cSyncRPC)
	{
		StateRead(msg, header, stream);
		return;
	}

	//__int64 time1, time2, cpu;
	//QueryPerformanceFrequency((LARGE_INTEGER*)&cpu);
	//QueryPerformanceCounter((LARGE_INTEGER*)&time1);	

	CallRPC(msg, header, stream);

	//QueryPerformanceCounter((LARGE_INTEGER*)&time2);
	//if (time2 - time1 > 6 * cpu / 1000)
	//{
	//	lsl::Profiler::I().Begin(lsl::StrFmt("rpc %d %d %d", (int)(1000 * (time2 - time1)/cpu), _classId, header.rpc));
	//}
}

void NetModel::Serialize(const NetMessage& msg, BitStream& stream)
{
	OnSerialize(msg, stream);
}

void NetModel::DescWrite(const NetMessage& msg, std::ostream& stream)
{
	OnDescWrite(msg, stream);
}

void NetModel::StateRead(const NetMessage& msg, const NetCmdHeader& header, std::istream& stream)
{
	OnStateRead(msg, header, stream);
}

void NetModel::StateWrite(const NetMessage& msg, std::ostream& stream)
{
	OnStateWrite(msg, stream);
}

void NetModel::Sync(unsigned target)
{
	std::ostream& stream = NewRPC(target, cSyncRPC);
	StateWrite(NetMessage(_player->id(), _player->net()->time()), stream);
	CloseRPC();
}

void NetModel::Sync()
{
	Sync(net::cNetTargetOthers);
}

NetModel::SyncState NetModel::syncState() const
{
	return _syncState;
}

void NetModel::syncState(SyncState value)
{
	if (_syncState == value)
		return;

	if (_syncState != ssOff)
		_player->UnregSyncModel(this);

	_syncState = value;	

	if (_syncState != ssOff)
		_player->RegSyncModel(this);
}

unsigned NetModel::syncStateTarget() const
{
	return _syncStateTarget;
}

void NetModel::syncStateTarget(unsigned value)
{
	_syncStateTarget = value;
}

unsigned NetModel::syncStatePriority() const
{
	return _syncStatePriority;
}

void NetModel::syncStatePriority(unsigned value)
{
	_syncStatePriority = value;
}

INetPlayer* NetModel::player()
{
	return _player;
}

INetService* NetModel::net()
{
	return _player->net();
}

unsigned NetModel::classId() const
{
	return _classId;
}

unsigned NetModel::id() const
{
	return _id;
}

bool NetModel::owner() const
{
	return _owner;
}

unsigned NetModel::ownerId() const
{
	return _ownerId;
}

}