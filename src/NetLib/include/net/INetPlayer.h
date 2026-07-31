#pragma once

#include "NetModel.h"

namespace net
{

class INetPlayer
{	
	friend class NetModel;
public:
	typedef std::map<unsigned, NetModel*> Models;

	static constexpr unsigned cDefCmd = 0;	
protected:
	virtual void RegSyncModel(NetModel* model) = 0;
	virtual void UnregSyncModel(NetModel* model) = 0;
public:
	//id = cDefCmd - default, non model target
	//id > 0 - model id
	virtual std::ostream& NewCmd(unsigned id, unsigned target, unsigned rpc) = 0;
	virtual std::ostream& NewModel(unsigned modelId) = 0;
	template<class _Type> std::ostream& NewModel();
	virtual void CloseCmd() = 0;

	virtual void DeleteModel(NetModel* model, bool local) = 0;
	virtual void DeleteModels(bool local) = 0;
	virtual void DeleteModels(unsigned ownerId, bool local) = 0;

	void MakeCmd(unsigned id, unsigned target, unsigned rpc) {NewCmd(id, target, rpc); CloseCmd();}
	void MakeModel(unsigned modelId) {NewModel(modelId); CloseCmd();}
	template<class _Type> void MakeModel() {NewModel<_Type>(); CloseCmd();}

	virtual NetModel* GetModel(unsigned id) = 0;

	virtual INetService* net() = 0;
	virtual unsigned id() const = 0;
	virtual unsigned netIndex() const = 0;
};




//INetPlayer::NewModel<_Type>() is defined at the bottom of INetService.h.
//
//Its body calls net()->modelClasses(), and net() returns INetService*, which is
//only forward-declared here -- INetService.h includes this header, so it cannot
//be included back. That expression does not depend on _Type, so it is checked
//where the template is defined rather than where it is instantiated, and the
//incomplete type is an error at that point. Moving the definition to after
//INetService is complete is the whole fix.

}