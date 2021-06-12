#include "stdafx.h"
#include "game\World.h"
#include "edit\Edit.h"

namespace r3d
{

namespace edit
{

Edit::Edit(game::World* world): _world(world)
{
	_scControl = new SceneControl(this);
	_db = new DataBase(this);
	_map = new Map(this);
}

Edit::~Edit()
{
	delete _map;
	delete _db;
	delete _scControl;
}

void Edit::OnUpdateLevel()
{
	_map->OnUpdateLevel();
}

game::World* Edit::GetWorld()
{
	return _world;
}

IDataBase* Edit::GetDB()
{
	return _db;
}

IMap* Edit::GetMap()
{
	return _map;
}

ISceneControl* Edit::GetScControl()
{
	return _scControl;
}

}

}