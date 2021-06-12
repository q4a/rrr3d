#ifndef R3D_GAME_IGAMECAR
#define R3D_GAME_IGAMECAR

#include "lslMath.h"
#include <vector>

namespace r3d
{

namespace game
{

class CarMotorDesc
{
public:
	static const int cNeutralGear = -1;
	static const int cBackGear = 0;

	CarMotorDesc();

	//Об/мин
	float CalcRPM(float wheelAxleSpeed, unsigned curGear) const;
	//Нм
	float CalcTorque(float rpm, unsigned curGear) const;

	//макс. число оборотов двигателя, об/мин
	unsigned maxRPM; 
	//обороты холостого хода, об/мин
	unsigned idlingRPM;
	//макс. момент двигателя при maxRPM, Н*м
	float maxTorque;
	//system effectiveness measure, КПД то есть, доли
	float SEM;
	//разностный коэффициент передачи
	float gearDiff;
	//тип коробки передач
	bool autoGear;
	//коэффициенты передач
	//[0] - задняя
	std::vector<float> gears;
	//тормозной момент
	float brakeTorque;
	//момент сопротивления движению (потери за счет трения и т.п.)
	float restTorque;

};

}

}

#endif