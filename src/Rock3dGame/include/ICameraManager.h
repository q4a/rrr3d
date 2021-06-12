#pragma once

namespace r3d
{

namespace game
{

class ICameraManager
{
public:
	enum Style {csFreeView, csThirdPerson, csIsometric, csLights, csIsoView, csAutoObserver, csBlock, cStyleEnd};
public:
	virtual Style GetStyle() const = 0;	
	virtual void ChangeStyle(Style value) = 0;
};

}

}