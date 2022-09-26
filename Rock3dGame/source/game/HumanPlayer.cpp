#include "stdafx.h"
#include "game/World.h"

#include "game/HumanPlayer.h"

namespace r3d
{
	namespace game
	{
		extern bool UnlimitedTurn = false;
		extern float TurnForce = 1.0f;

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

		HumanPlayer::Control::Control(HumanPlayer* owner): _owner(owner), _accelDown(false), _backDown(false),
		                                                   _brakeDown(false), _rapidDown(false), _easyDown(false),
		                                                   _leftDown(false), _rightDown(false)
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
			}

			Player* player = _owner->_player;

			if (_owner->GetPlayer() != nullptr && _owner->GetPlayer()->IsSpectator() && _owner->GetPlayer()->InRace() ==
				true)
			{
				if (msg.action == gaShot1 && msg.state == ksDown && !msg.repeat)
				{
					if (_owner->GetRace()->GetWorld()->GetCamera()->InFly() == false)
					{
						_owner->GetRace()->GetWorld()->GetCamera()->ChangeStyle(CameraManager::csAutoObserver);
						return true;
					}
					return false;
				}

				if (msg.action == gaShot2 && msg.state == ksDown && !msg.repeat)
				{
					if (_owner->GetRace()->GetWorld()->GetCamera()->InFly() == false)
					{
						_owner->GetRace()->GetWorld()->GetCamera()->ChangeStyle(CameraManager::csIsometric);
						return true;
					}
					return false;
				}

				if (msg.action == gaShot3 && msg.state == ksDown && !msg.repeat)
				{
					if (_owner->GetRace()->GetWorld()->GetCamera()->InFly() == false)
					{
						_owner->GetRace()->GetWorld()->GetCamera()->ChangeStyle(CameraManager::csThirdPerson);
						return true;
					}
					return false;
				}

				if (msg.action == gaShot4 && msg.state == ksDown && !msg.repeat)
				{
					if (_owner->GetRace()->GetWorld()->GetCamera()->InFly() == false)
					{
						_owner->GetRace()->GetWorld()->GetCamera()->ChangeStyle(CameraManager::csFreeView);
						return true;
					}
					return false;
				}

				if (msg.action == gaShotAll && msg.state == ksDown && !msg.repeat)
				{
					//подтянуть свободную камеру к игроку
					if (_owner->GetRace()->GetWorld()->GetCamera()->GetStyle() == CameraManager::csFreeView)
					{
						if (_owner->GetRace()->GetWorld()->GetCamera()->GetPlayer()->GetCar().gameObj != nullptr)
						{
							D3DXQUATERNION targetRot = _owner->GetRace()->GetWorld()->GetCamera()->GetPlayer()->GetCar()
							                                 .gameObj->GetRot();
							D3DXVECTOR3 targetPos = _owner->GetRace()->GetWorld()->GetCamera()->GetPlayer()->GetCar().
							                                gameObj->GetWorldPos();
							_owner->GetRace()->GetWorld()->GetCamera()->FlyTo(targetPos, targetRot, 5.0f, true, true);
							return true;
						}
						return false;
					}
				}

				if (msg.action == gaHyper && msg.state == ksDown && !msg.repeat)
				{
					//быстро подтянуть свободную камеру к игроку
					if (_owner->GetRace()->GetWorld()->GetCamera()->GetStyle() == CameraManager::csFreeView)
					{
						if (_owner->GetRace()->GetWorld()->GetCamera()->GetPlayer()->GetCar().gameObj != nullptr)
						{
							D3DXQUATERNION targetRot = _owner->GetRace()->GetWorld()->GetCamera()->GetPlayer()->GetCar()
							                                 .gameObj->GetRot();
							D3DXVECTOR3 targetPos = _owner->GetRace()->GetWorld()->GetCamera()->GetPlayer()->GetCar().
							                                gameObj->GetWorldPos();
							_owner->GetRace()->GetWorld()->GetCamera()->FlyTo(targetPos, targetRot, 1.0f, true, true);
							return true;
						}
						return false;
					}
				}

				if (msg.action == gaResetCar && msg.state == ksDown && !msg.repeat)
				{
					//полурандомный полёт свободной камеры
					if (_owner->GetRace()->GetWorld()->GetCamera()->GetStyle() == CameraManager::csFreeView)
					{
						if (_owner->GetRace()->GetWorld()->GetCamera()->GetPlayer()->GetCar().gameObj != nullptr)
						{
							D3DXQUATERNION targetRot = _owner->GetRace()->GetWorld()->GetCamera()->GetPlayer()->GetCar()
							                                 .gameObj->GetRot();
							_owner->GetRace()->GetWorld()->GetCamera()->FlyTo(
								D3DXVECTOR3(RandomRange(-100.0f, 100.0f), RandomRange(-100.0f, 100.0f),
								            RandomRange(0.0f, 40.0f)), targetRot, 5.0f, false, true);
							return true;
						}
						return false;
					}
					return false;
				}

				if (msg.action == gaBreak && msg.state == ksDown && !msg.repeat)
				{
					//остановить полёт свободной камеры:
					if (_owner->GetRace()->GetWorld()->GetCamera()->GetStyle() == CameraManager::csFreeView)
					{
						if (_owner->GetRace()->GetWorld()->GetCamera()->InFly())
						{
							_owner->GetRace()->GetWorld()->GetCamera()->StopFly();
							return true;
						}
						return false;
					}
				}

				if (msg.action == gaRapidMode && msg.state == ksDown && !msg.repeat)
				{
					_owner->GetRace()->GetWorld()->GetGame()->gameMusic()->Next();
					return true;
				}

				if (msg.action == gaEasyMode && msg.state == ksDown && !msg.repeat)
				{
					_owner->GetRace()->GetWorld()->GetGame()->gameMusic()->Pause(true);
					return true;
				}

				//следующий бот
				if (msg.action == gaAccel && msg.state == ksDown && !msg.repeat)
				{
					Player* current = _owner->GetRace()->GetWorld()->GetCamera()->GetPlayer();
					if (current->IsComputer())
					{
						if (current->GetId() == _owner->GetRace()->GetPlayerById(Race::cComputer1)->GetId())
						{
							if (_owner->GetRace()->GetPlayerById(Race::cComputer2) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer2)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer2));
								return true;
							}
							return false;
						}

						if (current->GetId() == _owner->GetRace()->GetPlayerById(Race::cComputer2)->GetId())
						{
							if (_owner->GetRace()->GetPlayerById(Race::cComputer3) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer3)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer3));
								return true;
							}
							return false;
						}

						if (current->GetId() == _owner->GetRace()->GetPlayerById(Race::cComputer3)->GetId())
						{
							if (_owner->GetRace()->GetPlayerById(Race::cComputer4) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer4)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer4));
								return true;
							}
							return false;
						}

						if (current->GetId() == _owner->GetRace()->GetPlayerById(Race::cComputer4)->GetId())
						{
							if (_owner->GetRace()->GetPlayerById(Race::cComputer5) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer5)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer5));
								return true;
							}
							return false;
						}

						if (current->GetId() == _owner->GetRace()->GetPlayerById(Race::cComputer5)->GetId())
						{
							if (_owner->GetRace()->GetPlayerById(Race::cComputer1) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer1)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer1));
								return true;
							}
							return false;
						}
					}
					else
					{
						//по-умолчанию переключаемся на босса
						if (_owner->GetRace()->GetPlayerById(Race::cComputer1) != nullptr && _owner->GetRace()->
							GetPlayerById(Race::cComputer1)->IsGamer())
						{
							_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
								_owner->GetRace()->GetPlayerById(Race::cComputer1));
							return true;
						}
						return false;
					}
				}

				//предыдущий бот
				if (msg.action == gaBack && msg.state == ksDown && !msg.repeat)
				{
					Player* current = _owner->GetRace()->GetWorld()->GetCamera()->GetPlayer();
					if (current->IsComputer())
					{
						if (current->GetId() == _owner->GetRace()->GetPlayerById(Race::cComputer1)->GetId())
						{
							if (_owner->GetRace()->GetPlayerById(Race::cComputer5) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer5)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer5));
								return true;
							}
							return false;

							if (_owner->GetRace()->GetPlayerById(Race::cComputer4) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer4)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer4));
								return true;
							}
							else
							{
								return false;
							}

							if (_owner->GetRace()->GetPlayerById(Race::cComputer3) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer3)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer3));
								return true;
							}
							else
							{
								return false;
							}

							if (_owner->GetRace()->GetPlayerById(Race::cComputer2) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer2)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer2));
								return true;
							}
							else
							{
								return false;
							}
							return false;
						}

						if (current->GetId() == _owner->GetRace()->GetPlayerById(Race::cComputer2)->GetId())
						{
							if (_owner->GetRace()->GetPlayerById(Race::cComputer1) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer1)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer1));
								return true;
							}
							return false;

							if (_owner->GetRace()->GetPlayerById(Race::cComputer5) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer5)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer5));
								return true;
							}
							else
							{
								return false;
							}

							if (_owner->GetRace()->GetPlayerById(Race::cComputer4) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer4)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer4));
								return true;
							}
							else
							{
								return false;
							}

							if (_owner->GetRace()->GetPlayerById(Race::cComputer3) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer3)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer3));
								return true;
							}
							else
							{
								return false;
							}
							return false;
						}


						if (current->GetId() == _owner->GetRace()->GetPlayerById(Race::cComputer3)->GetId())
						{
							if (_owner->GetRace()->GetPlayerById(Race::cComputer2) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer2)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer2));
								return true;
							}
							return false;

							if (_owner->GetRace()->GetPlayerById(Race::cComputer1) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer1)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer1));
								return true;
							}
							else
							{
								return false;
							}

							if (_owner->GetRace()->GetPlayerById(Race::cComputer5) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer5)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer5));
								return true;
							}
							else
							{
								return false;
							}

							if (_owner->GetRace()->GetPlayerById(Race::cComputer4) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer4)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer4));
								return true;
							}
							else
							{
								return false;
							}

							if (_owner->GetRace()->GetPlayerById(Race::cComputer3) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer3)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer3));
								return true;
							}
							else
							{
								return false;
							}
							return false;
						}

						if (current->GetId() == _owner->GetRace()->GetPlayerById(Race::cComputer4)->GetId())
						{
							if (_owner->GetRace()->GetPlayerById(Race::cComputer3) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer3)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer3));
								return true;
							}
							return false;

							if (_owner->GetRace()->GetPlayerById(Race::cComputer2) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer2)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer2));
								return true;
							}
							else
							{
								return false;
							}

							if (_owner->GetRace()->GetPlayerById(Race::cComputer1) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer1)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer1));
								return true;
							}
							else
							{
								return false;
							}


							if (_owner->GetRace()->GetPlayerById(Race::cComputer5) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer5)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer5));
								return true;
							}
							else
							{
								return false;
							}

							return false;
						}

						if (current->GetId() == _owner->GetRace()->GetPlayerById(Race::cComputer5)->GetId())
						{
							if (_owner->GetRace()->GetPlayerById(Race::cComputer4) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer4)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer4));
								return true;
							}
							return false;

							if (_owner->GetRace()->GetPlayerById(Race::cComputer3) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer3)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer3));
								return true;
							}
							else
							{
								return false;
							}

							if (_owner->GetRace()->GetPlayerById(Race::cComputer2) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer2)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer2));
								return true;
							}
							else
							{
								return false;
							}

							if (_owner->GetRace()->GetPlayerById(Race::cComputer1) != nullptr && _owner->GetRace()->
								GetPlayerById(Race::cComputer1)->IsGamer())
							{
								_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
									_owner->GetRace()->GetPlayerById(Race::cComputer1));
								return true;
							}
							else
							{
								return false;
							}
							return false;
						}
					}
					else
					{
						//по-умолчанию переключаемся на босса
						if (_owner->GetRace()->GetPlayerById(Race::cComputer1) != nullptr && _owner->GetRace()->
							GetPlayerById(Race::cComputer1)->IsGamer())
						{
							_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
								_owner->GetRace()->GetPlayerById(Race::cComputer1));
							return true;
						}
						return false;
					}
				}

				//предыдущий актор
				if (msg.action == gaWheelLeft && msg.state == ksDown && !msg.repeat)
				{
					unsigned int curID = _owner->GetRace()->GetWorld()->GetCamera()->GetPlayer()->GetNetSlot();
					unsigned int prevID = curID - 1;

					if (_owner->GetRace()->GetWorld()->GetCamera()->GetPlayer()->IsComputer())
					{
						if (_owner->GetRace()->GetPlayerById(Race::cHuman) != nullptr && _owner->GetRace()->
							GetPlayerById(Race::cHuman)->IsGamer())
						{
							_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
								_owner->GetRace()->GetPlayerById(Race::cHuman));
							return true;
						}
						if (_owner->GetRace()->GetPlayerById(Race::cOpponent1) != nullptr && _owner->GetRace()->
							GetPlayerById(Race::cOpponent1)->IsGamer())
						{
							_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
								_owner->GetRace()->GetPlayerById(Race::cOpponent1));
							return true;
						}

						if (_owner->GetRace()->GetPlayerById(Race::cOpponent2) != nullptr && _owner->GetRace()->
							GetPlayerById(Race::cOpponent2)->IsGamer())
						{
							_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
								_owner->GetRace()->GetPlayerById(Race::cOpponent2));
							return true;
						}

						if (_owner->GetRace()->GetPlayerById(Race::cOpponent3) != nullptr && _owner->GetRace()->
							GetPlayerById(Race::cOpponent3)->IsGamer())
						{
							_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
								_owner->GetRace()->GetPlayerById(Race::cOpponent3));
							return true;
						}
						return false;
					}

					if (prevID > 0)
					{
						//если предыдущий актор существует и не является наблюдателем, то переключаемся на него.
						if (_owner->GetRace()->GetPlayerByNetSlot(prevID) != nullptr && _owner->GetRace()->
							GetPlayerByNetSlot(prevID)->IsGamer())
							_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
								_owner->GetRace()->GetPlayerByNetSlot(prevID));
						else
						{
							if (prevID > 1)
							{
								prevID -= 1;
								if (_owner->GetRace()->GetPlayerByNetSlot(prevID) != nullptr && _owner->GetRace()->
									GetPlayerByNetSlot(prevID)->IsGamer())
									_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
										_owner->GetRace()->GetPlayerByNetSlot(prevID));
								else
								{
									if (prevID > 1)
									{
										prevID -= 1;
										if (_owner->GetRace()->GetPlayerByNetSlot(prevID) != nullptr && _owner->
											GetRace()->GetPlayerByNetSlot(prevID)->IsGamer())
											_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
												_owner->GetRace()->GetPlayerByNetSlot(prevID));
										else
										{
											if (prevID > 1)
											{
												prevID -= 1;
												if (_owner->GetRace()->GetPlayerByNetSlot(prevID) != nullptr && _owner->
													GetRace()->GetPlayerByNetSlot(prevID)->IsGamer())
													_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
														_owner->GetRace()->GetPlayerByNetSlot(prevID));
												else
												{
													if (prevID > 1)
													{
														prevID -= 1;
														if (_owner->GetRace()->GetPlayerByNetSlot(prevID) != nullptr &&
															_owner->GetRace()->GetPlayerByNetSlot(prevID)->IsGamer())
															_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
																_owner->GetRace()->GetPlayerByNetSlot(prevID));
													}
												}
											}
										}
									}
								}
							}
						}
					}
					return true;
				}

				//следующий актор
				if (msg.action == gaWheelRight && msg.state == ksDown && !msg.repeat)
				{
					unsigned int curID = _owner->GetRace()->GetWorld()->GetCamera()->GetPlayer()->GetNetSlot();
					unsigned int nextID = curID + 1;

					if (nextID < 9)
					{
						//если следующий актор существует и не является наблюдателем, то переключаемся на него.
						if (_owner->GetRace()->GetPlayerByNetSlot(nextID) != nullptr && _owner->GetRace()->
							GetPlayerByNetSlot(nextID)->IsGamer())
							_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
								_owner->GetRace()->GetPlayerByNetSlot(nextID));
						else
						{
							if (nextID < 8)
							{
								nextID += 1;
								if (_owner->GetRace()->GetPlayerByNetSlot(nextID) != nullptr && _owner->GetRace()->
									GetPlayerByNetSlot(nextID)->IsGamer())
									_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
										_owner->GetRace()->GetPlayerByNetSlot(nextID));
								else
								{
									if (nextID < 8)
									{
										nextID += 1;
										if (_owner->GetRace()->GetPlayerByNetSlot(nextID) != nullptr && _owner->
											GetRace()->GetPlayerByNetSlot(nextID)->IsGamer())
											_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
												_owner->GetRace()->GetPlayerByNetSlot(nextID));
										else
										{
											if (nextID < 8)
											{
												nextID += 1;
												if (_owner->GetRace()->GetPlayerByNetSlot(nextID) != nullptr && _owner->
													GetRace()->GetPlayerByNetSlot(nextID)->IsGamer())
													_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
														_owner->GetRace()->GetPlayerByNetSlot(nextID));
												else
												{
													if (nextID < 8)
													{
														nextID += 1;
														if (_owner->GetRace()->GetPlayerByNetSlot(nextID) != nullptr &&
															_owner->GetRace()->GetPlayerByNetSlot(nextID)->IsGamer())
															_owner->GetRace()->GetWorld()->GetCamera()->SetPlayer(
																_owner->GetRace()->GetPlayerByNetSlot(nextID));
													}
												}
											}
										}
									}
								}
							}
						}
					}
					return true;
				}
			}

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

			if (_owner->GetPlayer()->GetShotFreeze() == false && GAME_PAUSED == false && msg.action == gaShotAll && msg.
				state == ksDown && !msg.repeat)
			{
				GameCar* car = _owner->GetPlayer()->GetCar().gameObj;
				//фикс для молотова в сетевой (чтобы патроны не тратились в холостую):
				if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon2) && _owner->GetPlayer()->
					GetSlotInst(Player::stWeapon2)->GetItem().GetName() == "scMolotov")
				{
					if (car->IsWheelsContact() == false)
					{
						_owner->Shot();
					}
					else
					{
						if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon1))
							_owner->Shot(stWeapon1);
						if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon3))
							_owner->Shot(stWeapon3);
					}
				}
				else
				{
					_owner->Shot();
				}
				return true;
			}

			if (msg.action == gaResetCar && msg.state == ksDown && !msg.repeat && GAME_PAUSED == false)
			{
				_owner->ResetCar();
				return true;
			}

			//second jump
			if (DOUBLE_JUMP == false && msg.action == gaHyper && msg.state == ksDown && !msg.repeat && _owner->
				GetPlayer()->GetSlotInst(Player::stHyper))
			{
				if (_owner->GetPlayer()->GetSlotInst(Player::stHyper)->GetItem().IsWeaponItem()->GetCurCharge() <= 0)
				{
					if (HUD_STYLE == 3)
					{
						_owner->GetLogic()->TakeBonus(_owner->GetPlayer()->GetCar().gameObj,
						                              _owner->GetPlayer()->GetCar().gameObj,
						                              GameObjListener::btEmpty, 1.0f, false);
					}
				}

				//второй прыжок доступен после первого прыжка с задержкой, но только если он не на рампе.
				if (_owner->GetPlayer()->inRamp() == false && _owner->GetPlayer()->GetDoubleJump() && _owner->
					GetPlayer()->GetCar().gameObj->OnJump() && _owner->GetPlayer()->GetJumpTime() > 0.2f)
				{
					if (_owner->GetPlayer()->GetSlotInst(Player::stHyper)->GetItem().IsWeaponItem()->GetCurCharge() > 0)
					{
						(_owner->GetPlayer()->GetSlotInst(Player::stHyper)->GetItem().IsWeaponItem()->
						         MinusCurCharge(1));
						//если второй прыжок делается от преграды то он сильнее, чем в воздухе:
						if (_owner->GetPlayer()->GetCar().gameObj->IsBodyContact())
							_owner->GetPlayer()->GetCar().gameObj->GetPxActor().GetNxActor()->addForce(
								NxVec3(0.0f, 0.0f, 22.0f), NX_SMOOTH_VELOCITY_CHANGE);
						else
							_owner->GetPlayer()->GetCar().gameObj->GetPxActor().GetNxActor()->addLocalForce(
								NxVec3(0.0f, 0.0f, 10.0f), NX_SMOOTH_VELOCITY_CHANGE);
						DOUBLE_JUMP = true;
					}
				}
			}

			if (_owner->GetPlayer()->GetSlotInst(Player::stMine) && msg.action == gaMine && msg.state == ksDown && !msg.
				repeat && control->GetGameActionInfo(msg.controller, gaMine).alphaMax == 0)
			{
				Weapon* wpnMine = _owner->GetWeapon(stMine) ? _owner->GetWeapon(stMine)->GetWeapon() : nullptr;
				if (wpnMine && !wpnMine->IsAutoMine())
				{
					if (_owner->GetPlayer()->GetSlotInst(Player::stMine)->GetItem().IsWeaponItem()->GetCurCharge() <= 0)
					{
						if (HUD_STYLE == 3)
						{
							_owner->GetLogic()->TakeBonus(_owner->GetPlayer()->GetCar().gameObj,
							                              _owner->GetPlayer()->GetCar().gameObj,
							                              GameObjListener::btEmpty, 1.0f, false);
						}
					}

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
				return true;
			}

			if (_owner->GetPlayer()->GetShotFreeze() == false && msg.action == gaShot && msg.state == ksDown && !msg.
				repeat)
			{
				GameCar* car = _owner->GetPlayer()->GetCar().gameObj;
				if (GAME_PAUSED == false && _owner->GetPlayer()->GetSlotInst(Player::stWeapon1) && _owner->GetPlayer()->
					GetSlotInst(Player::stWeapon1)->GetItem().GetAutoShot() == false)
				{
					if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon1)->GetItem().IsWeaponItem()->GetCurCharge() >
						0)
						_owner->SetCurWeapon(0);
					else
					{
						if (HUD_STYLE == 3)
						{
							_owner->GetLogic()->TakeBonus(_owner->GetPlayer()->GetCar().gameObj,
							                              _owner->GetPlayer()->GetCar().gameObj,
							                              GameObjListener::btEmpty, 1.0f, false);
						}
						if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon2) && _owner->GetPlayer()->
							GetSlotInst(Player::stWeapon2)->GetItem().IsWeaponItem()->GetCurCharge() > 0)
							_owner->SetCurWeapon(1);
						else
						{
							if (HUD_STYLE == 3)
							{
								_owner->GetLogic()->TakeBonus(_owner->GetPlayer()->GetCar().gameObj,
								                              _owner->GetPlayer()->GetCar().gameObj,
								                              GameObjListener::btEmpty, 1.0f, false);
							}
							if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon3) && _owner->GetPlayer()->
								GetSlotInst(Player::stWeapon3)->GetItem().IsWeaponItem()->GetCurCharge() > 0)
								_owner->SetCurWeapon(2);
							else
							{
								if (HUD_STYLE == 3)
								{
									_owner->GetLogic()->TakeBonus(_owner->GetPlayer()->GetCar().gameObj,
									                              _owner->GetPlayer()->GetCar().gameObj,
									                              GameObjListener::btEmpty, 1.0f, false);
								}

								if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon4) && _owner->GetPlayer()->
									GetSlotInst(Player::stWeapon4)->GetItem().IsWeaponItem()->GetCurCharge() > 0)
									_owner->SetCurWeapon(3);
								else
								{
									if (HUD_STYLE == 3)
									{
										_owner->GetLogic()->TakeBonus(_owner->GetPlayer()->GetCar().gameObj,
										                              _owner->GetPlayer()->GetCar().gameObj,
										                              GameObjListener::btEmpty, 1.0f, false);
									}
								}
							}
						}
					}
					if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon2) && _owner->GetPlayer()->
						GetSlotInst(Player::stWeapon2)->GetItem().GetName() == "scMolotov")
					{
						if (car->IsWheelsContact() == false)
						{
							_owner->ShotCurrent();
						}
						else
						{
							if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon1))
								_owner->Shot(stWeapon1);
						}
					}
					else
						_owner->ShotCurrent();
					return true;
				}
			}

			const unsigned weaponShotKeys[stWeapon4 - stWeapon1 + 1] = {gaShot1, gaShot2, gaShot3, gaShot4};

			for (unsigned i = stWeapon1; i <= stWeapon4; ++i)
			{
				int index = i - stWeapon1;
				if (mapObj && msg.action == weaponShotKeys[index] && msg.state == ksDown && !msg.repeat)
				{
					GameCar* car = _owner->GetPlayer()->GetCar().gameObj;
					WeaponType weapon = _owner->GetWeaponByIndex(index);
					if (weapon != cWeaponTypeEnd && _owner->GetPlayer()->GetShotFreeze() == false)
					{
						_owner->SetCurWeapon(index);

						if (weapon == stWeapon2)
						{
							if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon2)->GetItem().IsWeaponItem()->
							            GetCurCharge() > 0)
							{
								_owner->SetCurWeapon(1);
							}
							else
							{
								if (HUD_STYLE == 3)
								{
									_owner->GetLogic()->TakeBonus(_owner->GetPlayer()->GetCar().gameObj,
									                              _owner->GetPlayer()->GetCar().gameObj,
									                              GameObjListener::btEmpty, 1.0f, false);
								}
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
								else
								{
									if (HUD_STYLE == 3)
									{
										_owner->GetLogic()->TakeBonus(_owner->GetPlayer()->GetCar().gameObj,
										                              _owner->GetPlayer()->GetCar().gameObj,
										                              GameObjListener::btEmpty, 1.0f, false);
									}
								}
							}

							if (weapon == stWeapon3)
							{
								if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon3)->GetItem().IsWeaponItem()->
								            GetCurCharge() > 0)
								{
									_owner->SetCurWeapon(2);
								}
								else
								{
									if (HUD_STYLE == 3)
									{
										_owner->GetLogic()->TakeBonus(_owner->GetPlayer()->GetCar().gameObj,
										                              _owner->GetPlayer()->GetCar().gameObj,
										                              GameObjListener::btEmpty, 1.0f, false);
									}
								}
							}

							if (weapon == stWeapon4)
							{
								if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon4)->GetItem().IsWeaponItem()->
								            GetCurCharge() > 0)
								{
									_owner->SetCurWeapon(3);
								}
								else
								{
									if (HUD_STYLE == 3)
									{
										_owner->GetLogic()->TakeBonus(_owner->GetPlayer()->GetCar().gameObj,
										                              _owner->GetPlayer()->GetCar().gameObj,
										                              GameObjListener::btEmpty, 1.0f, false);
									}
								}
							}
						}
						return true;
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


			if (accelDown)
			{
				//pizda не оптимизированый фрагмент кода!!!
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

				if (!brakeDown)
					gameObj.SetMoveCar(GameCar::mcAccel);
				else
					gameObj.SetMoveCar(GameCar::mcBrake);
			}
			else if (backDown)
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
			else if (brakeDown && !accelDown)
			{
				gameObj.SetMoveCar(GameCar::mcBrake);
			}
			else
			{
				gameObj.SetMoveCar(GameCar::mcNone);
			}

			if (leftDown != 0.0f)
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
			else if (rightDown)
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
			else
			{
				gameObj.SetSteerWheel(GameCar::swNone);
			}

			bool chatMode = _owner->GetRace()->GetGame()->GetMenu()->IsChatInputVisible();

			if (chatMode)
				return;

			if (_owner->GetPlayer()->GetShotFreeze() == false && control->GetGameActionState(gaShot, false))
			{
				//Автоматическая стрельба
				if (GAME_PAUSED == false && _owner->GetPlayer()->GetSlotInst(Player::stWeapon1) && _owner->GetPlayer()->
					GetSlotInst(Player::stWeapon1)->GetItem().GetAutoShot() == true)
				{
					_owner->ShotCurrent();
				}
			}

			if (CRATER_SPAWN == true)
			{
				if (_owner->GetPlayer()->GetId() == Race::cHuman)
				{
					_owner->Shot(stWeapon4);
				}
			}

			//автоматизация прыжков
			if (control->GetGameActionState(gaHyper, false))
			{
				if (_owner->GetPlayer()->GetSlotInst(Player::stHyper) && !_owner->GetPlayer()->GetHyperDelay() && !
					_owner->GetPlayer()->GetCar().gameObj->IsShell() && !_owner->GetPlayer()->GetCar().gameObj->
					OnJump())
				{
					if (_owner->GetPlayer()->GetSlotInst(Player::stHyper)->GetItem().IsWeaponItem()->GetCurCharge() <=
						0)
					{
						if (HUD_STYLE == 3)
						{
							_owner->GetLogic()->TakeBonus(_owner->GetPlayer()->GetCar().gameObj,
							                              _owner->GetPlayer()->GetCar().gameObj,
							                              GameObjListener::btEmpty, 1.0f, false);
						}
					}

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


			//3 и 4 уровни шасси позволяют поворачивать машину с места, открывают полноценные резкие повороты
			if (_owner->GetPlayer()->GetSlotInst(Player::stTrans) && (_owner->GetPlayer()->GetSlotInst(Player::stTrans)
			                                                                ->GetItem().GetName() == "scTrans3" ||
				_owner->GetPlayer()->GetSlotInst(Player::stTrans)->GetItem().GetName() == "scTrans4"))
			{
				if (rapidDown && !backDown)
				{
					if (_owner->GetPlayer()->GetCar().gameObj->IsAnyWheelContact())
					{
						TurnForce = 1.4f;
						UnlimitedTurn = true;
					}
					else
					{
						//для сайдвиндера в полёте углы более резкие.
						if (_owner->GetPlayer()->GetCar().gameObj->InFly())
						{
							TurnForce = 1.6f;
							UnlimitedTurn = true;
						}

						//для девилдрайвера воздушные стрейфы
						if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon1) && _owner->GetPlayer()->
							GetSlotInst(Player::stWeapon1)->GetItem().GetName() == "scSonar")
						{
							UnlimitedTurn = false;
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
						TurnForce = -1.4f;
						UnlimitedTurn = true;
					}
					else
					{
						//для сайдвиндера в полёте углы более резкие.
						if (_owner->GetPlayer()->GetCar().gameObj->InFly())
						{
							TurnForce = -1.6f;
							UnlimitedTurn = true;
						}

						//для девилдрайвера воздушные стрейфы
						if (_owner->GetPlayer()->GetSlotInst(Player::stWeapon1) && _owner->GetPlayer()->
							GetSlotInst(Player::stWeapon1)->GetItem().GetName() == "scSonar")
						{
							UnlimitedTurn = false;
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
						TurnForce = 0.7f;
						UnlimitedTurn = true;
					}
					else
					{
						TurnForce = 0.5f;
						UnlimitedTurn = true;
					}
				}

				if (easyDown && backDown)
				{
					//для сайдвиндера в полёте углы более резкие.
					if (_owner->GetPlayer()->GetCar().gameObj->InFly())
					{
						TurnForce = -0.7f;
						UnlimitedTurn = true;
					}
					else
					{
						TurnForce = -0.5f;
						UnlimitedTurn = true;
					}
				}
			}

			//второй уровень шасси открывает доступ к плавным поворотам, частично резким.
			if (_owner->GetPlayer()->GetSlotInst(Player::stTrans) && (_owner->GetPlayer()->GetSlotInst(Player::stTrans)
			                                                                ->GetItem().GetName() == "scTrans2"))
			{
				if (easyDown)
				{
					TurnForce = 0.5f;
				}

				if (rapidDown)
				{
					TurnForce = 1.4f;
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
						UnlimitedTurn = true;
						TurnForce = 0.8f;
					}
					else
					{
						UnlimitedTurn = false;
						TurnForce = 0.8f;
					}
				}
				else
				{
					UnlimitedTurn = false;
					TurnForce = 1.0f;
				}
			}

			for (int i = 0; i < cControllerTypeEnd; ++i)
			{
				auto controller = static_cast<ControllerType>(i);
				float alpha = control->GetGameActionState(controller, gaMine, false);

				if (alpha && _owner->GetPlayer()->GetSlotInst(Player::stMine))
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
		}

		void HumanPlayer::Shot(WeaponType weapon, MapObj* target)
		{
			auto type = static_cast<Player::SlotType>(weapon + Player::stHyper);

			if (_player->GetSlot(type))
				GetLogic()->Shot(_player, target, type);
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

		void HumanPlayer::ResetCar()
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
