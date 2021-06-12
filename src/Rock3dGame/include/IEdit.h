#pragma once

#include "IDataBase.h"
#include "IMap.h"
#include "ISceneControl.h"

namespace r3d
{

namespace edit
{

class IEdit: public Object
{
public:
	virtual IDataBase* GetDB() = 0;
	virtual IMap* GetMap() = 0;
	virtual ISceneControl* GetScControl() = 0;
};

}

}