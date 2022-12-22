#include "stdafx.h"
#include "game\World.h"

#include "game\HumanPlayer.h"

namespace r3d
{
	namespace game
	{

		HumanPlayer::HumanPlayer(Player* player): _player(player), _curWeapon(0)
		{
			LSL_ASSERT(_player);
			_player->AddRef();

			_control = new Control(this);
			GetRace()->GetWorld()->GetControl()->InsertEvent(_control);

			player->SetReflScene(false);

			if (player->GetRace()->GetGame()->netGame()->isStarted())
				player->SetCheat(Player::cCheatEnableFaster);
		}

		HumanPlayer::~HumanPlayer()
		{
			GetRace()->GetWorld()->GetControl()->RemoveEvent(_control);
			delete _control;

			_player->SetCheat(Player::cCheatDisable);
			_player->Release();
		}

		HumanPlayer::Control::Control(HumanPlayer* owner): _owner(owner), _accelDown(false), _backDown(false), _brakeDown(false), _rapidDown(false), _easyDown(false), _leftDown(false), _rightDown(false), _accelDownSec(false), _backDownSec(false), _brakeDownSec(false), _rapidDownSec(false), _easyDownSec(false), _leftDownSec(false), _rightDownSec(false)
		{
		}

		bool HumanPlayer::Control::OnHandleInput(const InputMessage& msg)
		{
			ControlManager* control = _owner->GetRace()->GetWorld()->GetControl();

			if (msg.controller == ctKeyboard)
			{
				if (msg.action == gaAccel)
					_accelDown = msg.state == ksDown;
				if (msg.action == gaBack)
					_backDown = msg.state == ksDown;
				if (msg.action == gaBreak)
					_brakeDown = msg.state == ksDown;

				if (msg.action == gaRapidMode)
					_rapidDown = msg.state == ksDown;
				if (msg.action == gaEasyMode)
					_easyDown = msg.state == ksDown;


				if (msg.action == gaWheelLeft)
					_leftDown = msg.state == ksDown;
				if (msg.action == gaWheelRight)
					_rightDown = msg.state == ksDown;
				
				/// //////////////////////
				if (msg.action == gaAccelSec)
					_accelDownSec = msg.state == ksDown;
				if (msg.action == gaBackSec)
					_backDownSec = msg.state == ksDown;
				if (msg.action == gaBreakSec)
					_brakeDownSec = msg.state == ksDown;

				if (msg.action == gaRapidModeSec)
					_rapidDownSec = msg.state == ksDown;
				if (msg.action == gaEasyModeSec)
					_easyDownSec = msg.state == ksDown;


				if (msg.action == gaWheelLeftSec)
					_leftDownSec = msg.state == ksDown;
				if (msg.action == gaWheelRightSec)
					_rightDownSec = msg.state == ksDown;
			}

			Player* player = _owner->_player;
				
			MapObj* mapObj = _owner->_player->GetCar().mapObj;
			if (mapObj == nullptr)
				return false;

			GameCar& gameObj = mapObj->GetGameObj<GameCar>();
			bool chatMode = _owner->GetRace()->GetGame()->GetMenu()->IsChatInputVisible();

			if (chatMode)
				return false;

			if (player->IsBlock())
				return false;

			if (!player->InRace())
				return false;


			if (msg.action == gaResetCar && msg.state == ksDown && !msg.repeat && GAME_PAUSED == false)
			{				
				_owner->ResetCar();
				return true;
			}

			if (msg.action == gaResetCarSec && msg.state == ksDown && !msg.repeat && GAME_PAUSED == false)
			{
				_owner->ResetCarSec();
				return true;
			}

			if (msg.action == gaViewSwitchSec && msg.state == ksDown && !msg.repeat && GAME_PAUSED == false)
			{

				if (_owner->GetRace()->IsFriendship())
				{
					Player* _SecondPlayer = _owner->GetRace()->GetPlayerById(Race::cOpponent1);
					if (_SecondPlayer->InRace())
					{
						//если включена блокировка камеры, то ставим второму игроку такую же камеру, как у первого игрока...
						if (_SecondPlayer->GetRace()->GetGame()->CamLock() == true)
						{
							if (_owner->GetPlayer()->GetRace()->GetGame()->GetPrefCamera() == 0)
								_SecondPlayer->SetIsometricView(false);							
							else							
								_SecondPlayer->SetIsometricView(true);							
						}
						else
						{
							//смена видов второй камеры
							if (_SecondPlayer->GetIsometricView())
								_SecondPlayer->SetIsometricView(false);
							else
								_SecondPlayer->SetIsometricView(true);
						}
					}
				}
				return true;
			}

			//second jump
			if (msg.action == gaHyper && msg.state == ksDown && !msg.repeat && _owner->GetPlayer()->GetSlotInst(Player::stHyper) && _owner->GetPlayer()->GetFinished() == false)
			{
				//второй прыжок доступен после первого прыжка с задержкой, но только если он не на рампе.
				if (_owner->GetPlayer()->GetCar().gameObj->DoubleJumpIsActive() == false && _owner->GetPlayer()->inRamp() == false && _owner->GetPlayer()->GetDoubleJump() && _owner->GetPlayer()->GetCar().gameObj->OnJump() && _owner->GetPlayer()->GetJumpTime() > 0.2f)
				{
					if (_owner->GetPlayer()->GetSlotInst(Player::stHyper)->GetItem().IsWeaponItem()->GetCurCharge() > 0)
					{
						(_owner->GetPlayer()->GetSlotInst(Player::stHyper)->GetItem().IsWeaponItem()->MinusCurCharge(1));
						//если второй прыжок делается от преграды то он сильнее, чем в воздухе:
						if (_owner->GetPlayer()->GetCar().gameObj->IsBodyContact())
							_owner->GetPlayer()->GetCar().gameObj->GetPxActor().GetNxActor()->addForce(
								NxVec3(0.0f, 0.0f, 22.0f), NX_SMOOTH_VELOCITY_CHANGE);
						else
							_owner->GetPlayer()->GetCar().gameObj->GetPxActor().GetNxActor()->addLocalForce(
								NxVec3(0.0f, 0.0f, 10.0f), NX_SMOOTH_VELOCITY_CHANGE);
						_owner->GetPlayer()->GetCar().gameObj->DoubleJumpSetActive(true);
					}
				}
			}

			if (_owner->GetPlayer()->GetSlotInst(Player::stMine) && msg.action == gaMine && msg.state == ksDown && !msg.repeat && control->GetGameActionInfo(msg.controller, gaMine).alphaMax == 0)
			{
				Weapon* wpnMine = _owner->GetWeapon(stMine) ? _owner->GetWeapon(stMine)->GetWeapon() : nullptr;
				if (wpnMine && !wpnMine->IsAutoMine())
				{
					if (GAME_PAUSED == false && _owner->GetPlayer()->GetMineFreeze() == false && _owner->GetPlayer()->GetFinished() == false)
					{
						if (_owner->GetPlayer()->GetSlotInst(Player::stMine)->GetItem().GetAutoShot() == true)
						{
							if (_owner->GetPlayer()->GetMasloDrop() || (_owner->GetPlayer()->GetCar().gameObj->
								IsWheelsContact()))
								_owner->Shot(stMine);
						}
						else
							_owner->Shot(stMine);
					}
				}
				return true;
			}


			const unsigned weaponShotKeys[stWeapon4 - stWeapon1 + 1] = { gaShot1, gaShot2, gaShot3, gaShot4 };

			for (unsigned i = stWeapon1; i <= stWeapon4; ++i)
			{
				int index = i - stWeapon1;
				if (mapObj && msg.action == weaponShotKeys[index] && msg.state == ksDown && !msg.repeat)
				{
					GameCar* car = _owner->GetPlayer()->GetCar().gameObj;
					WeaponType weapon = _owner->GetWeaponByIndex(index);
					if (weapon != cWeaponTypeEnd && _owner->GetPlayer()->GetShotFreeze() == false && _owner->GetPlayer()->GetFinished() == false)
					{
						_owner->SetCurWeapon(index);

						if (weapon == stWeapon2)
						{
							if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon2)->GetItem().IsWeaponItem()->
								GetCurCharge() > 0)
							{
								_owner->SetCurWeapon(1);
							}


							//фикс для молотова в сетевой (чтобы патроны не тратились в холостую):
							if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon2)->GetItem().GetName() == "scMolotov")
							{
								if (car->IsWheelsContact() == false)
								{
									_owner->Shot(weapon);
								}
							}
							else
							{
								if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon2)->GetItem().GetName() !=
									"scFireGun")
									_owner->Shot(weapon);
							}
						}
						else
						{
							_owner->Shot(weapon);
							if (weapon == stWeapon1)
							{
								if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon1)->GetItem().IsWeaponItem()->
									GetCurCharge() > 0)
								{
									_owner->SetCurWeapon(0);
								}
							}

							if (weapon == stWeapon3)
							{
								if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon3)->GetItem().IsWeaponItem()->
									GetCurCharge() > 0)
								{
									_owner->SetCurWeapon(2);
								}
							}

							if (weapon == stWeapon4)
							{
								if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon4)->GetItem().IsWeaponItem()->
									GetCurCharge() > 0)
								{
									_owner->SetCurWeapon(3);
								}
							}
						}
						return true;
					}
				}
			}

			if (_owner->GetRace()->IsFriendship())
			{
				Player* _SecondPlayer = _owner->GetRace()->GetPlayerById(Race::cOpponent1);

				if (_SecondPlayer == nullptr)
					return false;

				MapObj* mapObjS = _SecondPlayer->GetCar().mapObj;

				if (mapObjS == nullptr)
					return false;

				if (_SecondPlayer->IsBlock())
					return false;

				if (!_SecondPlayer->InRace())
					return false;

				GameCar& gameObjS = mapObjS->GetGameObj<GameCar>();
				if (_SecondPlayer->GetCar().gameObj && _SecondPlayer->GetFinished() == false)
				{
					if (msg.action == gaHyperSec && msg.state == ksDown && !msg.repeat && _SecondPlayer->GetSlotInst(Player::stHyper) && _SecondPlayer->GetFinished() == false)
					{
						if (_SecondPlayer->GetSlotInst(Player::stHyper)->GetItem().IsWeaponItem()->GetCurCharge() <= 0)
							_owner->GetLogic()->TakeBonus(_SecondPlayer->GetCar().gameObj, _SecondPlayer->GetCar().gameObj, GameObjListener::btEmpty, 1.0f, false);

						//второй прыжок доступен после первого прыжка с задержкой, но только если он не на рампе.
						if (_SecondPlayer->GetCar().gameObj->DoubleJumpIsActive() == false && _SecondPlayer->inRamp() == false && _SecondPlayer->GetDoubleJump() && _SecondPlayer->GetCar().gameObj->OnJump() && _SecondPlayer->GetJumpTime() > 0.2f)
						{
							if (_SecondPlayer->GetSlotInst(Player::stHyper)->GetItem().IsWeaponItem()->GetCurCharge() > 0)
							{
								(_SecondPlayer->GetSlotInst(Player::stHyper)->GetItem().IsWeaponItem()->MinusCurCharge(1));
								//если второй прыжок делается от преграды то он сильнее, чем в воздухе:
								if (_SecondPlayer->GetCar().gameObj->IsBodyContact())
									_SecondPlayer->GetCar().gameObj->GetPxActor().GetNxActor()->addForce(
										NxVec3(0.0f, 0.0f, 22.0f), NX_SMOOTH_VELOCITY_CHANGE);
								else
									_SecondPlayer->GetCar().gameObj->GetPxActor().GetNxActor()->addLocalForce(
										NxVec3(0.0f, 0.0f, 10.0f), NX_SMOOTH_VELOCITY_CHANGE);
								_SecondPlayer->GetCar().gameObj->DoubleJumpSetActive(true);
							}
						}
					}
				
					if (_SecondPlayer->GetSlotInst(Player::stMine) && msg.action == gaMineSec && msg.state == ksDown && !msg.repeat && control->GetGameActionInfo(msg.controller, gaMineSec).alphaMax == 0)
					{

						Weapon* wpnMine = _owner->GetWeapon(stMine) ? _owner->GetWeapon(stMine)->GetWeapon() : nullptr;
						if (wpnMine && !wpnMine->IsAutoMine())
						{
							if (GAME_PAUSED == false && _SecondPlayer->GetMineFreeze() == false && _SecondPlayer->GetFinished() == false)
							{
								if (_SecondPlayer->GetSlotInst(Player::stMine)->GetItem().GetAutoShot() == true)
								{
									if (_SecondPlayer->GetMasloDrop() || (_SecondPlayer->GetCar().gameObj->
										IsWheelsContact()))
										_owner->Shot2(stMine);
								}
								else
									_owner->Shot2(stMine);
							}
						}
						return true;
					}


					if (msg.action == gaShot1Sec && msg.state == ksDown && !msg.repeat && _SecondPlayer->GetSlotInst(Player::stWeapon1) && _SecondPlayer->GetFinished() == false)
					{
						if (_SecondPlayer->GetSlotInst(Player::stWeapon1)->GetItem().IsWeaponItem()->GetCurCharge() > 0 && _SecondPlayer->GetSlotInst(Player::stWeapon1)->GetItem().GetAutoShot() == false)
							_owner->Shot2(stWeapon1);
					}


					if (msg.action == gaShot2Sec && msg.state == ksDown && !msg.repeat && _SecondPlayer->GetSlotInst(Player::stWeapon2) && _SecondPlayer->GetFinished() == false)
					{

						if (_SecondPlayer->GetCar().gameObj && _SecondPlayer->GetSlotInst(Player::stWeapon2)->GetItem().IsWeaponItem()->GetCurCharge() > 0 && _SecondPlayer->GetSlotInst(Player::stWeapon2)->GetItem().GetAutoShot() == false)
						{
							//фикс для молотова в сетевой (чтобы патроны не тратились в холостую):
							if (_SecondPlayer->GetSlotInst(Player::stWeapon2)->GetItem().GetName() == "scMolotov")
							{
								if (_SecondPlayer->GetCar().gameObj->IsWheelsContact() == false)								
									_owner->Shot2(stWeapon2);								
							}
							else
							{
								if (_SecondPlayer->GetSlotInst(Player::stWeapon2)->GetItem().GetName() != "scFireGun")
									_owner->Shot2(stWeapon2);
								
							}
						}		
					}


					if (msg.action == gaShot3Sec && msg.state == ksDown && !msg.repeat && _SecondPlayer->GetSlotInst(Player::stWeapon3) && _SecondPlayer->GetFinished() == false)
					{
						if (_SecondPlayer->GetSlotInst(Player::stWeapon3)->GetItem().IsWeaponItem()->GetCurCharge() > 0 && _SecondPlayer->GetSlotInst(Player::stWeapon3)->GetItem().GetAutoShot() == false)
							_owner->Shot2(stWeapon3);
					}


					if (msg.action == gaShot4Sec && msg.state == ksDown && !msg.repeat && _SecondPlayer->GetSlotInst(Player::stWeapon4) && _SecondPlayer->GetFinished() == false)
					{
						if (_SecondPlayer->GetSlotInst(Player::stWeapon4)->GetItem().IsWeaponItem()->GetCurCharge() > 0 && _SecondPlayer->GetSlotInst(Player::stWeapon4)->GetItem().GetAutoShot() == false)
							_owner->Shot2(stWeapon4);
					}				
				}
			}
					
			return false;
		}


		void HumanPlayer::Control::OnInputProgress(float deltaTime)
		{
			ControlManager* control = _owner->GetRace()->GetWorld()->GetControl();
			Player* player = _owner->_player;
			MapObj* mapObj = player->GetCar().mapObj;


			if (mapObj == nullptr)
				return;

			if (player->IsBlock())
				return;


#ifdef _DEBUG
	Race* race = _owner->_player->GetRace();

	for (Race::AIPlayers::const_iterator iter = race->GetAIPlayers().begin(); iter != race->GetAIPlayers().end(); ++iter)
		//не мешаем АИ
		if ((*iter)->GetCar() && (*iter)->GetCar()->_enbAI && (*iter)->GetPlayer() == _owner->_player)
			return;
#endif

			GameCar& gameObj = mapObj->GetGameObj<GameCar>();
			bool accelDown = _accelDown || control->GetGameActionState(ctGamepad, gaAccel);
			bool backDown = _backDown || control->GetGameActionState(ctGamepad, gaBack);
			bool brakeDown = _brakeDown || control->GetGameActionState(ctGamepad, gaBreak);
			bool rapidDown = _rapidDown || control->GetGameActionState(ctGamepad, gaRapidMode);
			bool easyDown = _easyDown || control->GetGameActionState(ctGamepad, gaEasyMode);
			float leftDown = _leftDown ? 1.0f : control->GetGameActionState(ctGamepad, gaWheelLeft, false);
			float rightDown = _rightDown ? 1.0f : control->GetGameActionState(ctGamepad, gaWheelRight, false);
			int leftAlphaMax = _leftDown ? 0 : control->GetGameActionInfo(ctGamepad, gaWheelLeft).alphaMax;
			int rightAlphaMax = _rightDown ? 0 : control->GetGameActionInfo(ctGamepad, gaWheelRight).alphaMax;

			bool accelDownS = _accelDownSec || control->GetGameActionState(ctGamepad, gaAccelSec);
			bool backDownS = _backDownSec || control->GetGameActionState(ctGamepad, gaBackSec);
			bool brakeDownS = _brakeDownSec || control->GetGameActionState(ctGamepad, gaBreakSec);
			bool rapidDownS = _rapidDownSec || control->GetGameActionState(ctGamepad, gaRapidModeSec);
			bool easyDownS = _easyDownSec || control->GetGameActionState(ctGamepad, gaEasyModeSec);
			float leftDownS = _leftDownSec ? 1.0f : control->GetGameActionState(ctGamepad, gaWheelLeftSec, false);
			float rightDownS = _rightDownSec ? 1.0f : control->GetGameActionState(ctGamepad, gaWheelRightSec, false);
			int leftAlphaMaxS = _leftDownSec ? 0 : control->GetGameActionInfo(ctGamepad, gaWheelLeftSec).alphaMax;
			int rightAlphaMaxS = _rightDownSec ? 0 : control->GetGameActionInfo(ctGamepad, gaWheelRightSec).alphaMax;


			if (accelDown)
			{				
				if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon1) != nullptr)
				{
					if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon1) && _owner->GetPlayer()->
						GetSlotInst(Player::stWeapon1)->GetItem().IsWeaponItem()->GetCurCharge() < 1)
						_owner->GetPlayer()->IsEmptyWpn(true);
					else
						_owner->GetPlayer()->IsEmptyWpn(false);
				}

				if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon2) != nullptr)
				{
					if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon2)->GetItem().IsWeaponItem()->GetCurCharge() <
						1)
						_owner->GetPlayer()->IsEmptyWpn(true);
					else
						_owner->GetPlayer()->IsEmptyWpn(false);
				}

				if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon3) != nullptr)
				{
					if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon3)->GetItem().IsWeaponItem()->GetCurCharge() <
						1)
						_owner->GetPlayer()->IsEmptyWpn(true);
					else
						_owner->GetPlayer()->IsEmptyWpn(false);
				}

				if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon4) != nullptr)
				{
					if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon4)->GetItem().IsWeaponItem()->GetCurCharge() <
						1)
						_owner->GetPlayer()->IsEmptyWpn(true);
					else
						_owner->GetPlayer()->IsEmptyWpn(false);
				}

				if (_owner->GetPlayer()->GetFinished() == false)
				{
					if (!brakeDown)
						gameObj.SetMoveCar(GameCar::mcAccel);
					else
						gameObj.SetMoveCar(GameCar::mcBrake);
				}
			}
			else if (backDown)
			{
				if (_owner->GetPlayer()->GetFinished() == false)
				{
					if (!brakeDown)
						gameObj.SetMoveCar(GameCar::mcBack);
					else
						gameObj.SetMoveCar(GameCar::mcBrake);

					//быстрый задний ход, если полностью прокачено шасси.
					if (_owner->GetPlayer()->GetSlotInst(Player::stTrans) && _owner->GetPlayer()->
						GetSlotInst(Player::stTrans)->GetItem().GetName() == "scTrans4")
						gameObj.BackSpeedK(1.0f);
					else
						gameObj.BackSpeedK(0.2f);
				}
			}
			else if (brakeDown && !accelDown)
			{
				if (_owner->GetPlayer()->GetFinished() == false)
					gameObj.SetMoveCar(GameCar::mcBrake);
			}
			else
			{
				if (_owner->GetPlayer()->GetFinished() == false)
					gameObj.SetMoveCar(GameCar::mcNone);
			}

			if (leftDown != 0.0f)
			{
				if (_owner->GetPlayer()->GetFinished() == false)
				{
					if (leftAlphaMax != 0)
					{
						gameObj.SetSteerWheel(GameCar::smManual);
						gameObj.SetSteerWheelAngle(-GameCar::cMaxSteerAngle * leftDown);
					}
					else
					{
						gameObj.SetSteerWheel(GameCar::swOnLeft);
					}
				}
			}
			else if (rightDown)
			{
				if (_owner->GetPlayer()->GetFinished() == false)
				{
					if (rightAlphaMax != 0)
					{
						gameObj.SetSteerWheel(GameCar::smManual);
						gameObj.SetSteerWheelAngle(-GameCar::cMaxSteerAngle * rightDown);
					}
					else
					{
						gameObj.SetSteerWheel(GameCar::swOnRight);
					}
				}
			}
			else
			{
				gameObj.SetSteerWheel(GameCar::swNone);
			}
			/////////////////////////////////////////////////////////////////////
			if (_owner->GetRace()->IsFriendship())
			{
				Player* _SecondPlayer = _owner->GetRace()->GetPlayerById(Race::cOpponent1);
								
				if (_SecondPlayer == nullptr)
					return;

				MapObj* mapObjS = _SecondPlayer->GetCar().mapObj;

				if (mapObjS == nullptr)
					return;

				if (_SecondPlayer->IsBlock())
					return;

				if (!_SecondPlayer->InRace())
					return;

				GameCar& gameObjS = mapObjS->GetGameObj<GameCar>();

				if (_SecondPlayer->GetCar().gameObj && _SecondPlayer->GetFinished() == false)
				{
					if (accelDownS)
					{
						if (_SecondPlayer->GetSlotInst(Player::stWeapon1) != nullptr)
						{
							if (_SecondPlayer->GetSlotInst(Player::stWeapon1) && _SecondPlayer->GetSlotInst(Player::stWeapon1)->GetItem().IsWeaponItem()->GetCurCharge() < 1)
								_SecondPlayer->IsEmptyWpn(true);
							else
								_SecondPlayer->IsEmptyWpn(false);
						}

						if (_SecondPlayer->GetSlotInst(Player::stWeapon2) != nullptr)
						{
							if (_SecondPlayer->GetSlotInst(Player::stWeapon2)->GetItem().IsWeaponItem()->GetCurCharge() < 1)
								_SecondPlayer->IsEmptyWpn(true);
							else
								_SecondPlayer->IsEmptyWpn(false);
						}

						if (_SecondPlayer->GetSlotInst(Player::stWeapon3) != nullptr)
						{
							if (_SecondPlayer->GetSlotInst(Player::stWeapon3)->GetItem().IsWeaponItem()->GetCurCharge() < 1)
								_SecondPlayer->IsEmptyWpn(true);
							else
								_SecondPlayer->IsEmptyWpn(false);
						}

						if (_SecondPlayer->GetSlotInst(Player::stWeapon4) != nullptr)
						{
							if (_SecondPlayer->GetSlotInst(Player::stWeapon4)->GetItem().IsWeaponItem()->GetCurCharge() < 1)
								_SecondPlayer->IsEmptyWpn(true);
							else
								_SecondPlayer->IsEmptyWpn(false);
						}

						if (!brakeDownS)
							gameObjS.SetMoveCar(GameCar::mcAccel);
						else
							gameObjS.SetMoveCar(GameCar::mcBrake);
					}
					else if (backDownS)
					{
						if (!brakeDownS)
							gameObjS.SetMoveCar(GameCar::mcBack);
						else
							gameObjS.SetMoveCar(GameCar::mcBrake);

						//быстрый задний ход, если полностью прокачено шасси.
						if (_SecondPlayer->GetSlotInst(Player::stTrans) && _SecondPlayer->GetSlotInst(Player::stTrans)->GetItem().GetName() == "scTrans4")
							gameObjS.BackSpeedK(1.0f);
						else
							gameObjS.BackSpeedK(0.2f);
					}
					else if (brakeDownS && !accelDownS)
					{
						gameObjS.SetMoveCar(GameCar::mcBrake);
					}
					else
					{
						gameObjS.SetMoveCar(GameCar::mcNone);
					}

					if (leftDownS != 0.0f)
					{
						if (leftAlphaMaxS != 0)
						{
							gameObjS.SetSteerWheel(GameCar::smManual);
							gameObjS.SetSteerWheelAngle(-GameCar::cMaxSteerAngle * leftDownS);
						}
						else
						{
							gameObjS.SetSteerWheel(GameCar::swOnLeft);
						}
					}
					else if (rightDownS)
					{
						if (rightAlphaMaxS != 0)
						{
							gameObjS.SetSteerWheel(GameCar::smManual);
							gameObjS.SetSteerWheelAngle(-GameCar::cMaxSteerAngle * rightDownS);
						}
						else
						{
							gameObjS.SetSteerWheel(GameCar::swOnRight);
						}
					}
					else
					{
						gameObjS.SetSteerWheel(GameCar::swNone);
					}
				}
			} 

			bool chatMode = _owner->GetRace()->GetGame()->GetMenu()->IsChatInputVisible();

			if (chatMode)
				return;


			if (CRATER_SPAWN == true)
			{
				if (_owner->GetPlayer()->GetId() == Race::cHuman)
				{
					_owner->Shot(stWeapon4);
				}
			}

			if (S_CRATER_SPAWN == true)
			{
				Player* _SecondPlayer = _owner->GetRace()->GetPlayerById(Race::cOpponent1);
				if (_SecondPlayer)
				{
					_owner->Shot2(stWeapon4);
				}
			}

			
			/// ////////////////////////////////////////////////////''''''''''''
			//автоматизация оружия

			if (control->GetGameActionState(gaShot1, false) && _owner->GetPlayer()->GetSlotInst(Player::stWeapon1) && _owner->GetPlayer()->GetFinished() == false)
			{
				if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon1)->GetItem().IsWeaponItem()->GetCurCharge() > 0 && _owner->GetPlayer()->GetSlotInst(Player::stWeapon1)->GetItem().GetAutoShot())
					_owner->Shot(stWeapon1);
			}


			if (control->GetGameActionState(gaShot2, false) && _owner->GetPlayer()->GetSlotInst(Player::stWeapon2) && _owner->GetPlayer()->GetFinished() == false)
			{

				if (_owner->GetPlayer()->GetCar().gameObj && _owner->GetPlayer()->GetSlotInst(Player::stWeapon2)->GetItem().IsWeaponItem()->GetCurCharge() > 0 && _owner->GetPlayer()->GetSlotInst(Player::stWeapon2)->GetItem().GetAutoShot())
				{
					//фикс для молотова в сетевой (чтобы патроны не тратились в холостую):
					if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon2)->GetItem().GetName() == "scMolotov")
					{
						if (_owner->GetPlayer()->GetCar().gameObj->IsWheelsContact() == false)
							_owner->Shot(stWeapon2);
					}
					else
					{
						if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon2)->GetItem().GetName() != "scFireGun")
							_owner->Shot(stWeapon2);

					}
				}
			}


			if (control->GetGameActionState(gaShot3, false) && _owner->GetPlayer()->GetSlotInst(Player::stWeapon3) && _owner->GetPlayer()->GetFinished() == false)
			{
				if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon3)->GetItem().IsWeaponItem()->GetCurCharge() > 0 && _owner->GetPlayer()->GetSlotInst(Player::stWeapon3)->GetItem().GetAutoShot())
					_owner->Shot(stWeapon3);
			}


			if (control->GetGameActionState(gaShot4, false) && _owner->GetPlayer()->GetSlotInst(Player::stWeapon4) && _owner->GetPlayer()->GetFinished() == false)
			{
				if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon4)->GetItem().IsWeaponItem()->GetCurCharge() > 0 && _owner->GetPlayer()->GetSlotInst(Player::stWeapon4)->GetItem().GetAutoShot())
					_owner->Shot(stWeapon4);
			}

			///////////////////////////////////////////////////////////////////////////////

			//автоматизация прыжков
			if (control->GetGameActionState(gaHyper, false))
			{
				if (_owner->GetPlayer()->GetSlotInst(Player::stHyper) && !_owner->GetPlayer()->GetHyperDelay() && !
					_owner->GetPlayer()->GetCar().gameObj->IsShell() && !_owner->GetPlayer()->GetCar().gameObj->
					OnJump() && _owner->GetPlayer()->GetFinished() == false)
				{
					//фикс чтобы прыжки с рампы не тратились все за раз.
					if (_owner->GetPlayer()->GetSlotInst(Player::stHyper)->GetItem().GetAutoShot() == true)
					{
						if (_owner->GetPlayer()->GetCar().gameObj->IsWheelsContact())
							_owner->Shot(stHyper);
					}
					else
						_owner->Shot(stHyper);
				}
			}

			if (_owner->GetRace()->IsFriendship())
			{
				Player* _SecondPlayer = _owner->GetRace()->GetPlayerById(Race::cOpponent1);

				if (_SecondPlayer == nullptr)
					return;

				MapObj* mapObjS = _SecondPlayer->GetCar().mapObj;

				if (mapObjS == nullptr)
					return;

				if (_SecondPlayer->IsBlock())
					return;

				if (!_SecondPlayer->InRace())
					return;

				GameCar& gameObjS = mapObjS->GetGameObj<GameCar>();

				if (_SecondPlayer->GetCar().gameObj)
				{
					if (control->GetGameActionState(gaHyperSec, false))
					{
						if (_SecondPlayer->GetSlotInst(Player::stHyper) && !_SecondPlayer->GetHyperDelay() && !
							_SecondPlayer->GetCar().gameObj->IsShell() && !_SecondPlayer->GetCar().gameObj->
							OnJump() && _SecondPlayer->GetFinished() == false)
						{
							//фикс чтобы прыжки с рампы не тратились все за раз.
							if (_SecondPlayer->GetSlotInst(Player::stHyper)->GetItem().GetAutoShot() == true)
							{
								if (_SecondPlayer->GetCar().gameObj->IsWheelsContact())
									_owner->Shot2(stHyper);
							}
							else
								_owner->Shot2(stHyper);
						}
					}


					if (control->GetGameActionState(gaShot1Sec, false) && _SecondPlayer->GetSlotInst(Player::stWeapon1) && _SecondPlayer->GetFinished() == false)
					{
						if (_SecondPlayer->GetSlotInst(Player::stWeapon1)->GetItem().IsWeaponItem()->GetCurCharge() > 0 && _SecondPlayer->GetSlotInst(Player::stWeapon1)->GetItem().GetAutoShot())
							_owner->Shot2(stWeapon1);
					}


					if (control->GetGameActionState(gaShot2Sec, false) && _SecondPlayer->GetSlotInst(Player::stWeapon2) && _SecondPlayer->GetFinished() == false)
					{
						if (_SecondPlayer->GetSlotInst(Player::stWeapon2)->GetItem().IsWeaponItem()->GetCurCharge() > 0 && _SecondPlayer->GetSlotInst(Player::stWeapon2)->GetItem().GetAutoShot())
						{
							//фикс для молотова в сетевой (чтобы патроны не тратились в холостую):
							if (_SecondPlayer->GetSlotInst(Player::stWeapon2)->GetItem().GetName() == "scMolotov")
							{
								if (_SecondPlayer->GetCar().gameObj->IsWheelsContact() == false)
									_owner->Shot2(stWeapon2);
							}
							else
							{
								if (_SecondPlayer->GetSlotInst(Player::stWeapon2)->GetItem().GetName() != "scFireGun")
									_owner->Shot2(stWeapon2);

							}
						}
					}


					if (control->GetGameActionState(gaShot3Sec, false) && _SecondPlayer->GetSlotInst(Player::stWeapon3) && _SecondPlayer->GetFinished() == false)
					{
						if (_SecondPlayer->GetSlotInst(Player::stWeapon3)->GetItem().IsWeaponItem()->GetCurCharge() > 0 && _SecondPlayer->GetSlotInst(Player::stWeapon3)->GetItem().GetAutoShot())
							_owner->Shot2(stWeapon3);
					}


					if (control->GetGameActionState(gaShot4Sec, false) && _SecondPlayer->GetSlotInst(Player::stWeapon4) && _SecondPlayer->GetFinished() == false)
					{
						if (_SecondPlayer->GetSlotInst(Player::stWeapon4)->GetItem().IsWeaponItem()->GetCurCharge() > 0 && _SecondPlayer->GetSlotInst(Player::stWeapon4)->GetItem().GetAutoShot())
							_owner->Shot2(stWeapon4);
					}




				}
			}


			//3 и 4 уровни шасси позволяют поворачивать машину с места, открывают полноценные резкие повороты
			if (_owner->GetPlayer()->GetSlotInst(Player::stTrans) && (_owner->GetPlayer()->GetSlotInst(Player::stTrans)
			                                                                ->GetItem().GetName() == "scTrans3" ||
				_owner->GetPlayer()->GetSlotInst(Player::stTrans)->GetItem().GetName() == "scTrans4"))
			{
				if (rapidDown && !backDown)
				{
					if (_owner->GetPlayer()->GetCar().gameObj->IsAnyWheelContact())
					{
						_owner->GetPlayer()->GetCar().gameObj->SetTurnForce(1.4f);
						_owner->GetPlayer()->GetCar().gameObj->SetUnlimitedTurn(true);
					}
					else
					{
						//для сайдвиндера в полёте углы более резкие.
						if (_owner->GetPlayer()->GetCar().gameObj->InFly())
						{
							_owner->GetPlayer()->GetCar().gameObj->SetTurnForce(1.6f);
							_owner->GetPlayer()->GetCar().gameObj->SetUnlimitedTurn(true);
						}

						//для девилдрайвера воздушные стрейфы
						if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon1) && _owner->GetPlayer()->
							GetSlotInst(Player::stWeapon1)->GetItem().GetName() == "scSonar")
						{
							_owner->GetPlayer()->GetCar().gameObj->SetUnlimitedTurn(false);
							float magnitude = _owner->GetPlayer()->GetCar().gameObj->GetPxActor().GetNxActor()->
							                          getLinearVelocity().magnitude();

							if (gameObj.GetSteerWheel() == GameCar::swOnLeft)
								_owner->GetPlayer()->GetCar().gameObj->GetPxActor().GetNxActor()->addLocalForce(
									NxVec3(YVector * (100 - magnitude)), NX_ACCELERATION);
							if (gameObj.GetSteerWheel() == GameCar::swOnRight)
								_owner->GetPlayer()->GetCar().gameObj->GetPxActor().GetNxActor()->addLocalForce(
									NxVec3(-YVector * (100 - magnitude)), NX_ACCELERATION);
						}
					}
				}

				if (rapidDown && backDown)
				{
					if (_owner->GetPlayer()->GetCar().gameObj->IsAnyWheelContact())
					{
						_owner->GetPlayer()->GetCar().gameObj->SetTurnForce(-1.4f);
						_owner->GetPlayer()->GetCar().gameObj->SetUnlimitedTurn(true);
					}
					else
					{
						//для сайдвиндера в полёте углы более резкие.
						if (_owner->GetPlayer()->GetCar().gameObj->InFly())
						{
							_owner->GetPlayer()->GetCar().gameObj->SetTurnForce(-1.6f);
							_owner->GetPlayer()->GetCar().gameObj->SetUnlimitedTurn(true);
						}

						//для девилдрайвера воздушные стрейфы
						if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon1) && _owner->GetPlayer()->
							GetSlotInst(Player::stWeapon1)->GetItem().GetName() == "scSonar")
						{
							_owner->GetPlayer()->GetCar().gameObj->SetUnlimitedTurn(false);
							float magnitude = _owner->GetPlayer()->GetCar().gameObj->GetPxActor().GetNxActor()->
							                          getLinearVelocity().magnitude();

							if (gameObj.GetSteerWheel() == GameCar::swOnLeft)
								_owner->GetPlayer()->GetCar().gameObj->GetPxActor().GetNxActor()->addLocalForce(
									NxVec3(YVector * (100 - magnitude)), NX_ACCELERATION);
							if (gameObj.GetSteerWheel() == GameCar::swOnRight)
								_owner->GetPlayer()->GetCar().gameObj->GetPxActor().GetNxActor()->addLocalForce(
									NxVec3(-YVector * (100 - magnitude)), NX_ACCELERATION);
						}
					}
				}

				if (easyDown && !backDown)
				{
					//для сайдвиндера в полёте углы более резкие.
					if (_owner->GetPlayer()->GetCar().gameObj->InFly())
					{
						_owner->GetPlayer()->GetCar().gameObj->SetTurnForce(0.7f);
						_owner->GetPlayer()->GetCar().gameObj->SetUnlimitedTurn(true);
					}
					else
					{
						_owner->GetPlayer()->GetCar().gameObj->SetTurnForce(0.5f);
						_owner->GetPlayer()->GetCar().gameObj->SetUnlimitedTurn(true);
					}
				}

				if (easyDown && backDown)
				{
					//для сайдвиндера в полёте углы более резкие.
					if (_owner->GetPlayer()->GetCar().gameObj->InFly())
					{
						_owner->GetPlayer()->GetCar().gameObj->SetTurnForce(-0.7f);
						_owner->GetPlayer()->GetCar().gameObj->SetUnlimitedTurn(true);
					}
					else
					{
						_owner->GetPlayer()->GetCar().gameObj->SetTurnForce(-0.5);
						_owner->GetPlayer()->GetCar().gameObj->SetUnlimitedTurn(true);
					}
				}
			}

			//второй уровень шасси открывает доступ к плавным поворотам, частично резким.
			if (_owner->GetPlayer()->GetSlotInst(Player::stTrans) && (_owner->GetPlayer()->GetSlotInst(Player::stTrans)
			                                                                ->GetItem().GetName() == "scTrans2"))
			{
				if (easyDown)
				{
					_owner->GetPlayer()->GetCar().gameObj->SetTurnForce(0.5);
				}

				if (rapidDown)
				{
					_owner->GetPlayer()->GetCar().gameObj->SetTurnForce(1.4);
				}
			}

			if (!easyDown && !rapidDown)
			{
				if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon1) && _owner->GetPlayer()->
					GetSlotInst(Player::stWeapon1)->GetItem().GetName() == "scSonar")
				{
					if (_owner->GetPlayer()->GetSlotInst(Player::stTrans) && _owner->GetPlayer()->
						GetSlotInst(Player::stTrans)->GetItem().GetName() != "scTrans1")
					{
						//для девилдрайвера уже со 2 уровня шасси можно разворачиваться на 360 с места.
						_owner->GetPlayer()->GetCar().gameObj->SetUnlimitedTurn(true);
						_owner->GetPlayer()->GetCar().gameObj->SetTurnForce(0.8f);
					}
					else
					{
						_owner->GetPlayer()->GetCar().gameObj->SetUnlimitedTurn(false);
						_owner->GetPlayer()->GetCar().gameObj->SetTurnForce(0.8f);
					}
				}
				else
				{
					_owner->GetPlayer()->GetCar().gameObj->SetUnlimitedTurn(false);
					_owner->GetPlayer()->GetCar().gameObj->SetTurnForce(1.0f);
				}
			}

			for (int i = 0; i < cControllerTypeEnd; ++i)
			{
				auto controller = static_cast<ControllerType>(i);
				float alpha = control->GetGameActionState(controller, gaMine, false);

				if (alpha && _owner->GetPlayer()->GetSlotInst(Player::stMine) && _owner->GetPlayer()->GetFinished() == false)
				{
					VirtualKeyInfo info = control->GetGameActionInfo(controller, gaMine);
					Weapon* wpnMine = _owner->GetWeapon(stMine) ? _owner->GetWeapon(stMine)->GetWeapon() : nullptr;


					if (wpnMine && (info.alphaMax != 0 || wpnMine->IsAutoMine()) && wpnMine->IsReadyShot(
						(1.0f - alpha) * 0.6f))
					{
						if (GAME_PAUSED == false && _owner->GetPlayer()->GetMineFreeze() == false)
						{
							if (_owner->GetPlayer()->GetSlotInst(Player::stMine)->GetItem().GetAutoShot() == true)
							{
								if (_owner->GetPlayer()->GetMasloDrop() || (_owner->GetPlayer()->GetCar().gameObj->
									IsWheelsContact()))
									_owner->Shot(stMine);
							}
							else
								_owner->Shot(stMine);
						}
					}

					break;
				}
			}
		


			/////////////////////////////
			if (_owner->GetRace()->IsFriendship())
			{
				Player* _SecondPlayer = _owner->GetRace()->GetPlayerById(Race::cOpponent1);

				if (_SecondPlayer == nullptr)
					return;

				MapObj* mapObjS = _SecondPlayer->GetCar().mapObj;

				if (mapObjS == nullptr)
					return;

				if (_SecondPlayer->IsBlock())
					return;

				if (!_SecondPlayer->InRace())
					return;

				GameCar& gameObjS = mapObjS->GetGameObj<GameCar>();

				if (_SecondPlayer->GetCar().gameObj)
				{

					if (_SecondPlayer->GetSlotInst(Player::stTrans) && (_SecondPlayer->GetSlotInst(Player::stTrans)
						->GetItem().GetName() == "scTrans3" ||
						_SecondPlayer->GetSlotInst(Player::stTrans)->GetItem().GetName() == "scTrans4"))
					{
						if (rapidDownS && !backDownS)
						{
							if (_SecondPlayer->GetCar().gameObj->IsAnyWheelContact())
							{
								_SecondPlayer->GetCar().gameObj->SetTurnForce(1.4f);
								_SecondPlayer->GetCar().gameObj->SetUnlimitedTurn(true);
							}
							else
							{
								//для сайдвиндера в полёте углы более резкие.
								if (_SecondPlayer->GetCar().gameObj->InFly())
								{
									_SecondPlayer->GetCar().gameObj->SetTurnForce(1.6f);
									_SecondPlayer->GetCar().gameObj->SetUnlimitedTurn(true);
								}

								//для девилдрайвера воздушные стрейфы
								if (_SecondPlayer->GetSlotInst(Player::stWeapon1) && _SecondPlayer->
									GetSlotInst(Player::stWeapon1)->GetItem().GetName() == "scSonar")
								{
									_SecondPlayer->GetCar().gameObj->SetUnlimitedTurn(false);
									float magnitude = _SecondPlayer->GetCar().gameObj->GetPxActor().GetNxActor()->
										getLinearVelocity().magnitude();

									if (gameObj.GetSteerWheel() == GameCar::swOnLeft)
										_SecondPlayer->GetCar().gameObj->GetPxActor().GetNxActor()->addLocalForce(
											NxVec3(YVector * (100 - magnitude)), NX_ACCELERATION);
									if (gameObj.GetSteerWheel() == GameCar::swOnRight)
										_SecondPlayer->GetCar().gameObj->GetPxActor().GetNxActor()->addLocalForce(
											NxVec3(-YVector * (100 - magnitude)), NX_ACCELERATION);
								}
							}
						}

						if (rapidDownS && backDownS)
						{
							if (_SecondPlayer->GetCar().gameObj->IsAnyWheelContact())
							{
								_SecondPlayer->GetCar().gameObj->SetTurnForce(-1.4f);
								_SecondPlayer->GetCar().gameObj->SetUnlimitedTurn(true);
							}
							else
							{
								//для сайдвиндера в полёте углы более резкие.
								if (_SecondPlayer->GetCar().gameObj->InFly())
								{
									_SecondPlayer->GetCar().gameObj->SetTurnForce(-1.6f);
									_SecondPlayer->GetCar().gameObj->SetUnlimitedTurn(true);
								}

								//для девилдрайвера воздушные стрейфы
								if (_SecondPlayer->GetSlotInst(Player::stWeapon1) && _SecondPlayer->
									GetSlotInst(Player::stWeapon1)->GetItem().GetName() == "scSonar")
								{
									_SecondPlayer->GetCar().gameObj->SetUnlimitedTurn(false);
									float magnitude = _SecondPlayer->GetCar().gameObj->GetPxActor().GetNxActor()->
										getLinearVelocity().magnitude();

									if (gameObj.GetSteerWheel() == GameCar::swOnLeft)
										_SecondPlayer->GetCar().gameObj->GetPxActor().GetNxActor()->addLocalForce(
											NxVec3(YVector * (100 - magnitude)), NX_ACCELERATION);
									if (gameObj.GetSteerWheel() == GameCar::swOnRight)
										_SecondPlayer->GetCar().gameObj->GetPxActor().GetNxActor()->addLocalForce(
											NxVec3(-YVector * (100 - magnitude)), NX_ACCELERATION);
								}
							}
						}

						if (easyDownS && !backDownS)
						{
							//для сайдвиндера в полёте углы более резкие.
							if (_SecondPlayer->GetCar().gameObj->InFly())
							{
								_SecondPlayer->GetCar().gameObj->SetTurnForce(0.7f);
								_SecondPlayer->GetCar().gameObj->SetUnlimitedTurn(true);
							}
							else
							{
								_SecondPlayer->GetCar().gameObj->SetTurnForce(0.5f);
								_SecondPlayer->GetCar().gameObj->SetUnlimitedTurn(true);
							}
						}

						if (easyDownS && backDownS)
						{
							//для сайдвиндера в полёте углы более резкие.
							if (_SecondPlayer->GetCar().gameObj->InFly())
							{
								_SecondPlayer->GetCar().gameObj->SetTurnForce(-0.7f);
								_SecondPlayer->GetCar().gameObj->SetUnlimitedTurn(true);
							}
							else
							{
								_SecondPlayer->GetCar().gameObj->SetTurnForce(-0.5);
								_SecondPlayer->GetCar().gameObj->SetUnlimitedTurn(true);
							}
						}
					}

					//второй уровень шасси открывает доступ к плавным поворотам, частично резким.
					if (_SecondPlayer->GetSlotInst(Player::stTrans) && (_SecondPlayer->GetSlotInst(Player::stTrans)
						->GetItem().GetName() == "scTrans2"))
					{
						if (easyDownS)
						{
							_SecondPlayer->GetCar().gameObj->SetTurnForce(0.5);
						}

						if (rapidDownS)
						{
							_SecondPlayer->GetCar().gameObj->SetTurnForce(1.4);
						}
					}

					if (!easyDownS && !rapidDownS)
					{
						if (_SecondPlayer->GetSlotInst(Player::stWeapon1) && _SecondPlayer->
							GetSlotInst(Player::stWeapon1)->GetItem().GetName() == "scSonar")
						{
							if (_SecondPlayer->GetSlotInst(Player::stTrans) && _SecondPlayer->
								GetSlotInst(Player::stTrans)->GetItem().GetName() != "scTrans1")
							{
								//для девилдрайвера уже со 2 уровня шасси можно разворачиваться на 360 с места.
								_SecondPlayer->GetCar().gameObj->SetUnlimitedTurn(true);
								_SecondPlayer->GetCar().gameObj->SetTurnForce(0.8f);
							}
							else
							{
								_SecondPlayer->GetCar().gameObj->SetUnlimitedTurn(false);
								_SecondPlayer->GetCar().gameObj->SetTurnForce(0.8f);
							}
						}
						else
						{
							_SecondPlayer->GetCar().gameObj->SetUnlimitedTurn(false);
							_SecondPlayer->GetCar().gameObj->SetTurnForce(1.0f);
						}
					}

					for (int i = 0; i < cControllerTypeEnd; ++i)
					{
						auto controller = static_cast<ControllerType>(i);
						float alpha = control->GetGameActionState(controller, gaMineSec, false);

						if (alpha && _SecondPlayer->GetSlotInst(Player::stMine))
						{
							VirtualKeyInfo info = control->GetGameActionInfo(controller, gaMineSec);
							Weapon* wpnMine = _owner->GetWeapon(stMine) ? _owner->GetWeapon(stMine)->GetWeapon() : nullptr;


							if (wpnMine && (info.alphaMax != 0 || wpnMine->IsAutoMine()) && wpnMine->IsReadyShot(
								(1.0f - alpha) * 0.6f))
							{
								if (GAME_PAUSED == false && _SecondPlayer->GetMineFreeze() == false && _SecondPlayer->GetFinished() == false)
								{
									if (_SecondPlayer->GetSlotInst(Player::stMine)->GetItem().GetAutoShot() == true)
									{
										if (_SecondPlayer->GetMasloDrop() || (_SecondPlayer->GetCar().gameObj->
											IsWheelsContact()))
											_owner->Shot2(stMine);
									}
									else
										_owner->Shot2(stMine);
								}
							}

							break;
						}
					}

				}
			} 

		}

		void HumanPlayer::Shot(WeaponType weapon, MapObj* target)
		{
			auto type = static_cast<Player::SlotType>(weapon + Player::stHyper);

			if (_player->GetSlot(type))
				GetLogic()->Shot(_player, target, type);
		}

		void HumanPlayer::SecShot(WeaponType weapon, MapObj* target)
		{
			Player* _SecondPlayer = this->GetRace()->GetPlayerById(Race::cOpponent1);
			auto type = static_cast<Player::SlotType>(weapon + Player::stHyper);

			if (_SecondPlayer->GetSlot(type))
				GetLogic()->Shot(_SecondPlayer, target, type);
		}

		void HumanPlayer::Shot(WeaponType weapon)
		{
			float viewAngle = D3DX_PI / 5.5f;
			WeaponItem* wpn = GetWeapon(weapon);
			if (wpn && wpn->GetSlot()->GetRecord() && wpn->GetSlot()->GetRecord()->GetName() == "sphereGun")
				viewAngle = 0;

				Player* enemy = _player->FindClosestEnemy(viewAngle, false);
				Shot(weapon, enemy ? enemy->GetCar().mapObj : nullptr);			
		}

		void HumanPlayer::Shot2(WeaponType weapon)
		{
			float viewAngle = D3DX_PI / 5.5f;
			WeaponItem* wpn = GetWeapon(weapon);
			if (wpn && wpn->GetSlot()->GetRecord() && wpn->GetSlot()->GetRecord()->GetName() == "sphereGun")
				viewAngle = 0;

			Player* _SecondPlayer = this->GetRace()->GetPlayerById(Race::cOpponent1);
			Player* enemy = _SecondPlayer->FindClosestEnemy(viewAngle, false);
			SecShot(weapon, enemy ? enemy->GetCar().mapObj : nullptr);
		}

		void HumanPlayer::ShotCurrent()
		{
			SelectWeapon(true);
		}

		void HumanPlayer::Shot(MapObj* target)
		{
			GetLogic()->Shot(_player, target);
		}

		void HumanPlayer::Shot()
		{
			LSL_ASSERT(_player->GetCar().mapObj);

			Player* enemy = _player->FindClosestEnemy(D3DX_PI / 4, false);
			Shot(enemy ? enemy->GetCar().mapObj : nullptr);
		}

		void HumanPlayer::ShotSec()
		{
			Player* _SecondPlayer = this->GetRace()->GetPlayerById(Race::cOpponent1);
			LSL_ASSERT(_SecondPlayer->GetCar().mapObj);

			Player* enemy = _SecondPlayer->FindClosestEnemy(D3DX_PI / 4, false);
			Shot(enemy ? enemy->GetCar().mapObj : nullptr);
		}

		void HumanPlayer::ResetCar()
		{
			if (GetPlayer()->GetFinished() == false)
			{
				GameCar* plrcar = _player->GetCar().gameObj;
				if (plrcar && (plrcar->IsAnyWheelContact() || plrcar->IsBodyContact()))
				{
					//звук респавна только для респавна человека.
					if (plrcar->GetGhostEff() == false && plrcar->GetRespBlock() == false)
						plrcar->GetLogic()->Damage(plrcar, _player->GetId(), plrcar, 0, plrcar->dtResp);

					_player->ResetCar();
				}
			}
		}

		void HumanPlayer::ResetCarSec()
		{
			Player* _SecondPlayer = this->GetRace()->GetPlayerById(Race::cOpponent1);
			if (_SecondPlayer->GetFinished() == false)
			{
				if (this->GetRace()->IsFriendship())
				{					
					if (_SecondPlayer == nullptr || !_SecondPlayer->InRace())
						return;


					GameCar* plrcar = _SecondPlayer->GetCar().gameObj;

					if (plrcar && (plrcar->IsAnyWheelContact() || plrcar->IsBodyContact()))
					{
						//звук респавна только для респавна человека.
						if (plrcar->GetGhostEff() == false && plrcar->GetRespBlock() == false)
							plrcar->GetLogic()->Damage(plrcar, _SecondPlayer->GetId(), plrcar, 0, plrcar->dtResp);

						_SecondPlayer->ResetCar();
					}
				}
			}
		}

		HumanPlayer::WeaponType HumanPlayer::GetWeaponByIndex(int number)
		{
			for (int i = stWeapon1; i <= stWeapon4; ++i)
			{
				auto type = static_cast<WeaponType>(i);

				WeaponItem* wpn = GetWeapon(type);
				if (wpn && (--number) < 0)
					return type;
			}

			return cWeaponTypeEnd;
		}

		WeaponItem* HumanPlayer::GetWeapon(WeaponType weapon)
		{
			auto type = static_cast<Player::SlotType>(weapon + Player::stHyper);

			Slot* slot = _player->GetSlotInst(type);
			WeaponItem* wpn = slot ? slot->GetItem().IsWeaponItem() : nullptr;

			if (wpn && wpn->GetMapObj())
				return wpn;

			return nullptr;
		}

		WeaponItem* HumanPlayer::GetWeaponSec(WeaponType weapon)
		{
			Player* _SecondPlayer = this->GetRace()->GetPlayerById(Race::cOpponent1);
			auto type = static_cast<Player::SlotType>(weapon + Player::stHyper);

			Slot* slot = _SecondPlayer->GetSlotInst(type);
			WeaponItem* wpn = slot ? slot->GetItem().IsWeaponItem() : nullptr;

			if (wpn && wpn->GetMapObj())
				return wpn;

			return nullptr;
		}

		int HumanPlayer::GetWeaponCount()
		{
			int count = 0;

			for (int i = stWeapon1; i <= stWeapon4; ++i)
			{
				if (GetWeapon(static_cast<WeaponType>(i)) == nullptr)
					break;
				++count;
			}

			return count;
		}

		int HumanPlayer::GetCurWeapon()
		{
			return _curWeapon;
		}

		void HumanPlayer::SetCurWeapon(int index)
		{
			_curWeapon = index;
		}

		void HumanPlayer::SelectWeapon(bool shot)
		{
			int count = stWeapon4 - stWeapon1 + 1;

			for (int i = 0; i < count; ++i)
			{
				int index = (_curWeapon + i) % count;
				auto type = static_cast<WeaponType>(stWeapon1 + index);
				WeaponItem* weapon = GetWeapon(type);

				if (weapon != nullptr && weapon->GetCurCharge() > 0)
				{
					_curWeapon = index;
					if (shot)
						Shot(type);

					if (shot && weapon->GetCurCharge() == 0)
						SelectWeapon(false);
					return;
				}
			}
			_curWeapon = 0;
		}

		Race* HumanPlayer::GetRace()
		{
			return _player->GetRace();
		}

		Player* HumanPlayer::GetPlayer()
		{
			return _player;
		}

		Logic* HumanPlayer::GetLogic()
		{
			return GetRace()->GetWorld()->GetLogic();
		}
	}
}
