#include "stdafx.h"

#include "net\NetServer.h"
#include "net\NetService.h"

namespace net
{
	
NetServer::NetServer(NetService* net): NetPlayer(net, cServerPlayer), _started(false), _port(0), _lastModelId(0), _processCmdRef(0), _newConnection(NULL)
{
}

NetServer::~NetServer()
{
	Shutdown();
}

void NetServer::NewConnection(NetConnection* connection)
{
	_newConnection = connection;
	if (_newConnection == NULL)
		_newConnection = _net->NewConnection(cUndefPlayer, this);

	_newConnection->Accept();	
}

void NetServer::ReleaseConnection(NetConnection* connection)
{
	_connections.Remove(connection);

	//delete connection;
	connection->Close();
	_net->ReleaseConnection(connection);
	connection = NULL;
}

void NetServer::ProcessCmd(const NetMessage& msg, const NetCmdHeader& header, const streambuf::const_buffers_type& bufs)
{	
	++_processCmdRef;
	int pos = _processCmdList.size();

	if (TestTarget(header.target, id(), msg.sender))
		NetPlayer::ProcessCmd(msg, header, bufs);

	LSL_ASSERT(_processCmdRef > 0);

	--_processCmdRef;

	if (!msg.discarded())
	{
		if (_processCmdRef == 0)
		{
			for (Connections::iterator iter = _connections.begin(); iter != _connections.end(); ++iter)
			{
				if (TestTarget(header.target, (*iter)->id(), msg.sender))
					(*iter)->SendCmd(header, bufs);
			}
		}
		else
		{
			_processCmdList.insert(_processCmdList.begin() + pos, CmdInst(header, bufs));
		}
	}

	if (_processCmdRef == 0)
	{
		for (CmdList::const_iterator iter = _processCmdList.begin(); iter != _processCmdList.end(); ++iter)
		{
			for (Connections::iterator iter2 = _connections.begin(); iter2 != _connections.end(); ++iter2)
			{
				if (TestTarget(iter->header.target, (*iter2)->id(), id()))
					(*iter2)->SendCmd(iter->header, iter->bufs());
			}
		}

		_processCmdList.clear();
	}
}

void NetServer::SendCmd(const NetCmdHeader& header, const streambuf::const_buffers_type& bufs)
{	
	ProcessCmd(NetMessage(id(), _net->time()), header, bufs);
}

void NetServer::OnProcessCmd(const NetMessage& msg, const NetCmdHeader& header, streambuf& streambuf, std::istream& stream)
{
	if (header.id == cDefCmd)
	{
		switch (header.rpc)
		{
			case cAllocateModelRPC:
			{
				BYTE modelId;
				net::Read(stream, modelId);
				unsigned dataSize = streambuf.size();
				const void* data = dataSize > 0 ? buffer_cast<const void*>(streambuf.data()) : NULL;				

				ModelHeader header2;
				header2.id = ++_lastModelId;
				header2.modelId = modelId;
				header2.ownerId = msg.sender;
				header2.descSize = dataSize;

				LSL_LOG(lsl::StrFmt("cAllocateModelRPC modelId=%d id=%d ownerId=%d", header2.modelId, header2.id, header2.ownerId));

				std::ostream& stream2 = NewCmd(cDefCmd, cNetTargetAll, cNewModelRPC);
				Write(stream2, &header2, sizeof(header2));
				if (data)
					Write(stream2, data, dataSize);
				CloseCmd();

				streambuf.consume(dataSize);
				break;
			}
		}
	}
}

void NetServer::SendState(const NetStateHeader& header, const streambuf::const_buffers_type& bufs, unsigned target)
{
	std::list<Endpoint> endpointList;

	for (Connections::iterator iter = _connections.begin(); iter != _connections.end(); ++iter)
	{
		NetConnection* connection = *iter;
		if (!connection->IsOpen() || !TestTarget(target, connection->id(), id()))
			continue;
		
		endpointList.push_back(connection->remoteEndpoint());
	}

	if (endpointList.size() > 0)
		channel()->SendState(endpointList, header, bufs);
}

void NetServer::OnConnected(NetConnection* sender)
{
	tcp::endpoint local, remote;
	GetEndpointTCP(sender->localEndpoint(), local);
	GetEndpointTCP(sender->remoteEndpoint(), remote);

	LSL_LOG(lsl::StrFmt("NetServer connected local=%s:%d remote=%s:%d", local.address().to_string().c_str(), local.port(), remote.address().to_string().c_str(), remote.port()));

	unsigned newId = _connections.size() > 0 ? _connections[_connections.size() - 1]->id() + 1 : cServerPlayer + 1;
	Connections::const_iterator posIter = _connections.end();

	for (unsigned i = 0; i < _connections.size(); ++i)
	{
		unsigned prevId = i > 0 ? _connections[i - 1]->id() : cServerPlayer;
		if (_connections[i]->id() - prevId >= 2)
		{
			newId = prevId + 1;
			posIter = _connections.begin() + i;
			break;
		}
	}
	
	sender->id(newId);

	if (_connections.size() < cPlayerMaxCount && _net->OnConnected(sender))
	{
		_connections.insert(posIter, sender);

		//send id
		{
			std::ostream& stream = NewCmd(cDefCmd, sender->id(), cSetPlayerId);
			Write(stream, sender->id());
			CloseCmd();
		}

		//models
		for (Models::const_iterator iter = _models.begin(); iter != _models.end(); ++iter)
		{
			ModelHeader header;
			header.id = iter->second->id();
			header.modelId = iter->second->classId();
			header.ownerId = iter->second->ownerId();

			Cmd& cmd = DoNewCmd(cDefCmd, sender->id(), cNewModelRPC);
			Write(cmd.stream, &header, sizeof(header));

			header.descSize = cmd.streambuf.size();
			iter->second->DescWrite(NetMessage(sender->id(), net()->time()), cmd.stream);
			header.descSize = cmd.streambuf.size() - header.descSize;

			iter->second->StateWrite(NetMessage(sender->id(), net()->time()), cmd.stream);
			CloseCmd();
		}

		//state
		streambuf streambuf;
		std::ostream stream(&streambuf);
		for (SyncModels::const_iterator iter = syncModels().begin(); iter != syncModels().end(); ++iter)
		{
			const SyncModel& syncModel = iter->second;
			if (syncModel.bitStream.isEmpty() || !TestTarget(syncModel.model->syncStateTarget(), sender->id(), this->id()))
				continue;

			syncModel.bitStream.Write(stream, false, false, false);
		
			NetStateHeader header;
			header.id = iter->first;
			header.size = streambuf.size();
			SendState(header, streambuf.data(), sender->id());
		
			streambuf.consume(streambuf.size());
		}

		NewConnection(NULL);
	}
	else
	{
		sender->Close();
		NewConnection(sender);
	}
}

void NetServer::OnDisconnected(NetConnection* sender)
{
	LSL_LOG("NetServer disconnected");

	if (!_net->OnDisconnected(sender))
		return;

	Disconnect(sender);
}

void NetServer::OnConnectionFailed(NetConnection* sender, const error_code& error)
{
	LSL_LOG("NetServer connection failed " + error.message());

	if (!_net->OnConnectionFailed(sender, error.value()))
		return;

	ReleaseConnection(sender);
}

void NetServer::OnReceiveCmd(const NetMessage& msg, const NetCmdHeader& header, const streambuf::const_buffers_type& bufs)
{
	LSL_TRACE(lsl::StrFmt("NetServer OnReceiveCmd sender=%d id=%d rpc=%d size=%d", msg.sender, header.id, header.rpc, header.size));

	ProcessCmd(msg, header, bufs);

	_net->OnReceiveCmd(msg, header, buffer_cast<const void*>(bufs), buffer_size(bufs));
}

void NetServer::OnProcess(unsigned time)
{
	streambuf streambuf;
	std::ostream stream(&streambuf);

	for (SyncModels::iterator iter = _syncModels.begin(); iter != _syncModels.end(); ++iter)
	{	
		SyncModel& syncModel = iter->second;
		if (syncModel.model->owner())
			continue;

		if (syncModel.isUpdated)
		{
			syncModel.bitStream.Reset(true, false, syncModel.bitStream.time());
			syncModel.model->Serialize(NetMessage(id(), time), syncModel.bitStream);
			syncModel.isUpdated = false;

			syncModel.bitStream.Write(stream, true, false, false);

			NetStateHeader header;
			header.id = iter->first;
			header.sender = id();
			header.size = streambuf.size();
			SendState(header, streambuf.data(), syncModel.model->syncStateTarget());

			streambuf.consume(streambuf.size());
		}
	}

	for (int i = _connections.size() - 1; i >= 0; --i)
		_connections[i]->Process(time);
}

void NetServer::OnDispatch()
{
	for (int i = _connections.size() - 1; i >= 0; --i)
		_connections[i]->Dispatch();
}

void NetServer::Start(unsigned port)
{
	if (_started)
		return;
	_started = true;

	_port = port;

	_net->acceptorImpl()->Listen(Endpoint(0, port));
	channel()->Bind(Endpoint(0, port));

	NewConnection(NULL);
}

void NetServer::Shutdown()
{
	if (!_started)
		return;
	_started = false;

	DeleteModels(true);

	channel()->Unbind();

	for (Connections::const_iterator iter = _connections.begin(); iter != _connections.end(); ++iter)
	{
		(*iter)->Close();
		_net->ReleaseConnection(*iter);
	}
	_connections.clear();

	if (_newConnection)
	{
		_newConnection->Close();
		_net->ReleaseConnection(_newConnection);
		_newConnection = NULL;
	}

	_net->acceptorImpl()->Close();
}

bool NetServer::started() const
{
	return _started;
}

unsigned NetServer::port() const
{
	return _port;
}

void NetServer::Disconnect(NetConnection* connection)
{
	connection->Close();
	DeleteModels(connection->id(), false);	

	ReleaseConnection(connection);
}

NetConnection* NetServer::GetConnection(unsigned index)
{
	return _connections[index];
}

NetConnection* NetServer::GetConnectionById(unsigned id)
{
	for (Connections::const_iterator iter = _connections.begin(); iter != _connections.end(); ++iter)
		if ((*iter)->id() == id)
			return *iter;
	return NULL;
}

unsigned NetServer::connectionCount() const
{
	return _connections.size();
}

NetChannel* NetServer::channel()
{
	return _net->channel();
}

}