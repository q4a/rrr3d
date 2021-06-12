#pragma once

namespace net
{

class NetPlayer: public INetPlayer
{
	friend NetService;
protected:
	struct Cmd
	{
		NetCmdHeader header;
		streambuf streambuf;
		std::ostream stream;		

		Cmd(): stream(&streambuf) {}
	};

	struct SyncModel
	{
		NetModel* model;
		BitStream bitStream;
		NetMessage msg;
		bool isUpdated;

		SyncModel(NetModel* mModel): model(mModel), isUpdated(false) {}
	};

	typedef std::map<unsigned, SyncModel> SyncModels;
public:
	static const unsigned cCmdMax = 8;

	static const unsigned cSetPlayerId = 0;
	static const unsigned cAllocateModelRPC = 1;
	static const unsigned cNewModelRPC = 2;
	static const unsigned cDelModelRPC = 3;

	static const unsigned cPing = 0;
private:
	unsigned _id;
	Cmd _cmdBuf[cCmdMax];
	unsigned _cmdIndex;	

	NetModel* NewModel(unsigned modelId, unsigned id, bool owner, unsigned ownerId, std::istream& stream);	
protected:
	NetService* _net;
	Models _models;
	SyncModels _syncModels;
	
	void RegSyncModel(NetModel* model);
	void UnregSyncModel(NetModel* model);

	virtual void OnNewModel(NetModel* model) {}
	virtual void OnDeleteModel(NetModel* model) {}

	Cmd& DoNewCmd(unsigned id, unsigned target, unsigned rpc);
	void ProcessCmd(const NetMessage& msg, const NetCmdHeader& header, const streambuf::const_buffers_type& bufs);
	virtual void SendCmd(const NetCmdHeader& header, const streambuf::const_buffers_type& bufs) = 0;
	virtual void OnProcessCmd(const NetMessage& msg, const NetCmdHeader& header, streambuf& streambuf, std::istream& stream) {}

	void ProcessState(const NetMessage& msg, const NetStateHeader& header, const streambuf::const_buffers_type& bufs);
	virtual void SendState(const NetStateHeader& header, const streambuf::const_buffers_type& bufs, unsigned target) = 0;

	virtual void OnProcess(unsigned time) {}
	virtual void OnDispatch() {}

	const SyncModels& syncModels() const;
	void id(unsigned value);
public:
	NetPlayer(NetService* net, unsigned id);
	virtual ~NetPlayer();

	//target - filter
	//id - receiver
	//sender - sender
	bool TestTarget(unsigned target, unsigned id, unsigned sender) const;

	//id = cDefCmd - default, non model target
	//id > 0 - model id	
	std::ostream& NewCmd(unsigned id, unsigned target, unsigned rpc);
	std::ostream& NewModel(unsigned modelId);
	void CloseCmd();

	void DeleteModel(NetModel* model, bool local);
	void DeleteModels(bool local);
	void DeleteModels(unsigned ownerId, bool local);

	NetModel* GetModel(unsigned id);

	void Process(unsigned time, bool syncRate);
	void Dispatch();

	INetService* net();

	unsigned id() const;
	unsigned netIndex() const;
};

}