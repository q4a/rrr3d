#pragma once

#include "IEdit.h"

#include "edit\DataBase.h"
#include "edit\Map.h"
#include "edit\SceneControl.h"

namespace r3d
{

namespace edit
{

class Edit: public IEdit
{
	friend game::World;
private:
	game::World* _world;

	SceneControl* _scControl;
	DataBase* _db;
	Map* _map;

	void OnUpdateLevel();
public:
	Edit(game::World* world);
	virtual ~Edit();

	game::World* GetWorld();
	IDataBase* GetDB();
	IMap* GetMap();	
	ISceneControl* GetScControl();
};

}

}