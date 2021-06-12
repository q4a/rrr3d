#include "stdafx.h"

#include "net\NetPlayer.h"
#include "net\NetService.h"

namespace net
{

NetPlayer::NetPlayer(NetService* net, unsigned id): _net(net), _id(id), _cmdIndex(0)
{
}

NetPlayer::~NetPlayer()
{
	DeleteModels(true);
}

NetModel* NetPlayer::NewModel(unsigned modelId, unsigned id, bool owner, unsigned ownerId, std::istream& stream)
{
	NetModel* model = _net->modelClasses().CreateInst(modelId, NetModel::Desc(this, modelId, id, owner, ownerId, stream));
	_models.insert(Models::value_type(id, model));

	OnNewModel(model);

	return model;
}

void NetPlayer::RegSyncModel(NetModel* model)
{
	_syncModels.insert(SyncModels::value_type(model->id(), SyncModel(model)));
}

void NetPlayer::UnregSyncModel(NetModel* model)
{
	_syncModels.erase(model->id());
}

NetPlayer::Cmd& NetPlayer::DoNewCmd(unsigned id, unsigned target, unsigned rpc)
{
	LSL_ASSERT(_cmdIndex < cCmdMax);

	Cmd& cmd = _cmdBuf[_cmdIndex];
	++_cmdIndex;	

	cmd.header.size = 0;
	cmd.header.id = id;	
	cmd.header.target = target;
	cmd.header.rpc = rpc;

	return cmd;
}

void NetPlayer::ProcessCmd(const NetMessage& msg, const NetCmdHeader& header, const streambuf::const_buffers_type& bufs)
{
	streambuf streambuf;
	std::istream stream(&streambuf);

	if (header.size > 0)
	{
		streambuf.prepare(header.size);
		buffer_copy(streambuf.prepare(header.size), bufs, header.size);
		streambuf.commit(header.size);
	}

	OnProcessCmd(msg, header, streambuf, stream);

	if (header.id == cDefCmd)
	{
		switch (header.rpc)
		{
			case cNewModelRPC:
			{
				ModelHeader header2;
				Read(stream, &header2, sizeof(header2));

				LSL_LOG(lsl::StrFmt("cNewModelRPC modelId=%d id=%d, ownerId=%d", header2.modelId, header2.id, header2.ownerId));
				
				NetModel* model = NewModel(header2.modelId, header2.id, header2.ownerId == id(), header2.ownerId, stream);
				if (streambuf.size() > 0)
				{
					NetCmdHeader header3 = header;
					header3.size = streambuf.size();
					model->StateRead(msg, header3, stream);
				}
				break;
			}

			case cDelModelRPC:
			{
				ModelHeader header;
				Read(stream, &header, sizeof(header));

				LSL_LOG(lsl::StrFmt("cDelModelRPC modelId=%d id=%d, ownerId=%d", header.modelId, header.id, header.ownerId));

				NetModel* model = GetModel(header.id);
				if (model == NULL)
				{
					LSL_LOG(lsl::StrFmt("NetPlayer::ProcessCmd-cDelModelRPC model id=%d not found", header.id));
				}
				else
					DeleteModel(model, true);
				break;
			}
		}	

		return;
	}

	NetModel* model = GetModel(header.id);
	if (model == NULL)
	{
		LSL_LOG(lsl::StrFmt("model with id=%d not found.", header.id));
		return;
	}

	model->ProcessCmd(msg, header, stream);
}

void NetPlayer::ProcessState(const NetMessage& msg, const NetStateHeader& header, const streambuf::const_buffers_type& bufs)
{
	streambuf streambuf;
	std::istream stream(&streambuf);

	if (header.size > 0)
	{
		streambuf.prepare(header.size);
		buffer_copy(streambuf.prepare(header.size), bufs, header.size);
		streambuf.commit(header.size);
	}

	SyncModels::iterator syncModelIter = _syncModels.find(header.id);
	if (syncModelIter == _syncModels.end())
	{
		LSL_LOG(lsl::StrFmt("sync model with id=%d not found.", header.id));
		return;
	}

	SyncModel& syncModel = syncModelIter->second;
	if (syncModel.bitStream.time() < msg.time)
	{
		syncModel.bitStream.Reset(false, true, msg.time);
		syncModel.bitStream.Read(stream);
		syncModel.msg = msg;
	}
}

const NetPlayer::SyncModels& NetPlayer::syncModels() const
{
	return _syncModels;
}

void NetPlayer::id(unsigned value)
{
	_id = value;
}

bool NetPlayer::TestTarget(unsigned target, unsigned id, unsigned sender) const
{
	return target == id ||
		((target & cNetTargetAll) && (target & ~cNetTargetAll) != id) ||
		((target & cNetTargetOthers) && !((target & ~cNetTargetOthers) == id || id == sender));
}

std::ostream& NetPlayer::NewCmd(unsigned id, unsigned target, unsigned rpc)
{
	return DoNewCmd(id, target, rpc).stream;
}

void NetPlayer::CloseCmd()
{
	LSL_ASSERT(_cmdIndex > 0);

	Cmd& cmd = _cmdBuf[_cmdIndex - 1];
	unsigned size = cmd.streambuf.size();
	cmd.header.size = size;	

	SendCmd(cmd.header, cmd.streambuf.data());
	cmd.streambuf.consume(size);
	--_cmdIndex;
}

std::ostream& NetPlayer::NewModel(unsigned modelId)
{
	std::ostream& stream = NewCmd(cDefCmd, cServerPlayer, cAllocateModelRPC);
	Write(stream, (BYTE)modelId);

	return stream;
}

void NetPlayer::DeleteModel(NetModel* model, bool local)
{
	if (local)
	{
		OnDeleteModel(model);
		_models.erase(model->id());
		delete model;
	}
	else
	{
		std::ostream& stream = NewCmd(cDefCmd, cNetTargetAll, cDelModelRPC);

		ModelHeader header;
		header.modelId = 0;
		header.id = model->id();
		Write(stream, &header, sizeof(header));

		CloseCmd();
	}
}

void NetPlayer::DeleteModels(bool local)
{
	while (_models.size() > 0)
		DeleteModel(_models.begin()->second, local);
}

void NetPlayer::DeleteModels(unsigned ownerId, bool local)
{
	Models list = _models;

	for (Models::const_iterator iter = list.begin(); iter != list.end(); ++iter)
		if (iter->second->ownerId() == ownerId)
			DeleteModel(iter->second, local);
}

NetModel* NetPlayer::GetModel(unsigned id)
{
	Models::iterator iter = _models.find(id);
	if (iter != _models.end())
		return iter->second;

	return NULL;
}

void NetPlayer::Process(unsigned time, bool syncRate)
{
	streambuf streambuf;
	std::ostream stream(&streambuf);

	for (SyncModels::iterator iter = _syncModels.begin(); iter != _syncModels.end(); ++iter)
	{	
		SyncModel& syncModel = iter->second;
		if (!syncModel.model->owner() || (syncModel.model->syncStatePriority() == cSyncStatePriorityDef && !syncRate))
			continue;

		syncModel.bitStream.Reset(true, false, time);
		syncModel.model->Serialize(NetMessage(id(), time), syncModel.bitStream);

		if (syncModel.bitStream.isChanged())
		{
			syncModel.bitStream.Write(stream, true, false, false);

			NetStateHeader header;
			header.id = iter->first;
			header.sender = _id;
			header.size = streambuf.size();
			SendState(header, streambuf.data(), syncModel.model->syncStateTarget());

			streambuf.consume(streambuf.size());
		}
		else if (syncModel.bitStream.isPutSleep())
		{
			//rpc
		}
	}

	OnProcess(time);
}

void NetPlayer::Dispatch()
{
	OnDispatch();

	for (SyncModels::iterator iter = _syncModels.begin(); iter != _syncModels.end(); ++iter)
	{
		SyncModel& synModel = iter->second;		
		if (synModel.model->owner() || !synModel.bitStream.isUpdated())
			continue;

		synModel.model->Serialize(synModel.msg, synModel.bitStream);
		synModel.isUpdated = true;
	}
}

INetService* NetPlayer::net()
{
	return _net;
}

unsigned NetPlayer::id() const
{
	return _id;
}

unsigned NetPlayer::netIndex() const
{
	return _id - cServerPlayer;
}

}