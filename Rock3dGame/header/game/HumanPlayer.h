#pragma once

#include "Player.h"

namespace r3d
{
	namespace game
	{
		extern bool UnlimitedTurn;
		extern float TurnForce;

		class HumanPlayer : public Object
		{
		private:
			class Control : public ControlEvent
			{
			private:
				HumanPlayer* _owner;

				bool _accelDown;
				bool _backDown;
				bool _brakeDown;
				bool _rapidDown;
				bool _easyDown;
				bool _leftDown;
				bool _rightDown;

				bool OnHandleInput(const InputMessage& msg) override;
				void OnInputProgress(float deltaTime) override;
			public:
				Control(HumanPlayer* owner);
			};

		public:
			enum WeaponType { stHyper = 0, stMine, stWeapon1, stWeapon2, stWeapon3, stWeapon4, cWeaponTypeEnd };

		private:
			Player* _player;
			Control* _control;
			int _curWeapon;
		public:
			HumanPlayer(Player* player);
			~HumanPlayer() override;

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
