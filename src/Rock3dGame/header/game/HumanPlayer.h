#pragma once

#include "Player.h"

namespace r3d
{

namespace game
{

class HumanPlayer: public Object
{
private:
	class Control: public ControlEvent
	{
	private:
		HumanPlayer* _owner;		

		bool _accelDown;
		bool _backDown;
		bool _leftDown;
		bool _rightDown;

		virtual bool OnHandleInput(const InputMessage& msg);
		virtual void OnInputProgress(float deltaTime);
	public:
		Control(HumanPlayer* owner);
	};
public:
	enum WeaponType {stHyper = 0, stMine, stWeapon1, stWeapon2, stWeapon3, stWeapon4, cWeaponTypeEnd};
private:
	Player* _player;
	Control* _control;	
	int _curWeapon;
public:
	HumanPlayer(Player* player);
	virtual ~HumanPlayer();

	void Shot(WeaponType weapon, MapObj* target);
	void Shot(WeaponType weapon);
	void ShotCurrent();
	void Shot(MapObj* target);	
	void Shot();	

	void ResetCar();

	WeaponType GetWeaponByIndex(int number);
	WeaponItem* GetWeapon(WeaponType weapon);
	int GetWeaponCount();

	int GetCurWeapon();
	void SetCurWeapon(int index);
	void SelectWeapon(bool shot);

	Race* GetRace();
	Player* GetPlayer();
	Logic* GetLogic();
};

}

}