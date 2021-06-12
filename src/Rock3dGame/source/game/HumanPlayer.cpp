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

HumanPlayer::Control::Control(HumanPlayer* owner): _owner(owner), _accelDown(false), _backDown(false), _leftDown(false), _rightDown(false)
{	
}

bool HumanPlayer::Control::OnHandleInput(const InputMessage& msg)
{
	ControlManager* control = _owner->GetRace()->GetWorld()->GetControl();

	if (msg.controller == ctKeyboard)
	{
		if (msg.action == gaAccel)
			_accelDown = msg.state == lsl::ksDown;
		if (msg.action == gaBreak)
			_backDown = msg.state == lsl::ksDown;

		if (msg.action == gaWheelLeft)
			_leftDown = msg.state == lsl::ksDown;	
		if (msg.action == gaWheelRight)
			_rightDown = msg.state == lsl::ksDown;
	}

	Player* player = _owner->_player;

	if (player->IsBlock())
		return false;
	
	game::MapObj* mapObj = _owner->_player->GetCar().mapObj;
	if (mapObj == NULL)
		return false;

	game::GameCar& gameObj = mapObj->GetGameObj<game::GameCar>();
	bool chatMode = _owner->GetRace()->GetGame()->GetMenu()->IsChatInputVisible();

	if (chatMode)
		return false;

	if (msg.action == gaShotAll && msg.state == ksDown && !msg.repeat)
	{
		_owner->Shot();
		return true;
	}

	if (msg.action == gaResetCar && msg.state == ksDown && !msg.repeat)
	{
		_owner->ResetCar();
		return true;
	}

	if (msg.action == gaMine && msg.state == ksDown && !msg.repeat && control->GetGameActionInfo(msg.controller, gaMine).alphaMax == 0)
	{
		Weapon* wpnMine = _owner->GetWeapon(stMine) ? _owner->GetWeapon(stMine)->GetWeapon() : NULL;
		if (wpnMine && !wpnMine->IsMaslo())
			_owner->Shot(stMine);
		return true;
	}

	if (msg.action == gaShot && msg.state == ksDown && !msg.repeat)
	{
		_owner->ShotCurrent();
		return true;
	}

	if (msg.action == gaWeaponDown && msg.state == ksDown && !msg.repeat)
	{
		_owner->SetCurWeapon(std::max(_owner->GetCurWeapon() - 1, 0));
		return true;
	}

	if (msg.action == gaWeaponUp && msg.state == ksDown && !msg.repeat)
	{
		_owner->SetCurWeapon(std::min(_owner->GetCurWeapon() + 1, _owner->GetWeaponCount() - 1));
		return true;
	}

	const unsigned weaponShotKeys[stWeapon4 - stWeapon1 + 1] = {gaShot1, gaShot2, gaShot3, gaShot4};

	for (unsigned i = stWeapon1; i <= stWeapon4; ++i)
	{
		int index = i - stWeapon1;
		if (mapObj && msg.action == weaponShotKeys[index] && msg.state == lsl::ksDown && !msg.repeat)
		{
			WeaponType weapon = _owner->GetWeaponByIndex(index);
			if (weapon != cWeaponTypeEnd)
			{
				_owner->Shot(weapon);
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
	game::MapObj* mapObj = player->GetCar().mapObj;

	if (mapObj == NULL)
		return;

	game::GameCar& gameObj = mapObj->GetGameObj<game::GameCar>();

	if (player->IsBlock())
		return;

#ifdef _DEBUG
	Race* race = _owner->_player->GetRace();

	for (Race::AIPlayers::const_iterator iter = race->GetAIPlayers().begin(); iter != race->GetAIPlayers().end(); ++iter)
		//не мешаем АИ
		if ((*iter)->GetCar() && (*iter)->GetCar()->_enbAI && (*iter)->GetPlayer() == _owner->_player)
			return;
#endif

	bool accelDown = _accelDown || control->GetGameActionState(ctGamepad, gaAccel);
	bool backDown = _backDown || control->GetGameActionState(ctGamepad, gaBreak);
	float leftDown = _leftDown ? 1.0f : control->GetGameActionState(ctGamepad, gaWheelLeft, false);
	float rightDown = _rightDown ? 1.0f : control->GetGameActionState(ctGamepad, gaWheelRight, false);
	int leftAlphaMax = _leftDown ? 0 : control->GetGameActionInfo(ctGamepad, gaWheelLeft).alphaMax;
	int rightAlphaMax = _rightDown ? 0 : control->GetGameActionInfo(ctGamepad, gaWheelRight).alphaMax;

	if (accelDown)
		gameObj.SetMoveCar(game::GameCar::mcAccel);
	else if (backDown)
		gameObj.SetMoveCar(game::GameCar::mcBack);
	else
		gameObj.SetMoveCar(game::GameCar::mcNone);

	if (leftDown != 0.0f)
	{
		if (leftAlphaMax != 0)
		{
			gameObj.SetSteerWheel(game::GameCar::smManual);
			gameObj.SetSteerWheelAngle(-GameCar::cMaxSteerAngle * leftDown);
		}
		else
			gameObj.SetSteerWheel(game::GameCar::swOnLeft);
	}
	else if (rightDown)
	{
		if (rightAlphaMax != 0)
		{
			gameObj.SetSteerWheel(game::GameCar::smManual);
			gameObj.SetSteerWheelAngle(-GameCar::cMaxSteerAngle * rightDown);
		}
		else
			gameObj.SetSteerWheel(game::GameCar::swOnRight);
	}
	else
		gameObj.SetSteerWheel(game::GameCar::swNone);

	bool chatMode = _owner->GetRace()->GetGame()->GetMenu()->IsChatInputVisible();

	if (chatMode)
		return;

	if (control->GetGameActionState(gaHyper, false))
	{
		_owner->Shot(stHyper);
	}

	for (int i = 0; i < cControllerTypeEnd; ++i)
	{
		ControllerType controller = (ControllerType)i;
		float alpha = control->GetGameActionState(controller, gaMine, false);

		if (alpha)
		{
			VirtualKeyInfo info = control->GetGameActionInfo(controller, gaMine);
			Weapon* wpnMine = _owner->GetWeapon(stMine) ? _owner->GetWeapon(stMine)->GetWeapon() : NULL;

			if (wpnMine && (info.alphaMax != 0 || wpnMine->IsMaslo()) && wpnMine->IsReadyShot((1.0f - alpha) * 0.6f))
			{
				_owner->Shot(stMine);
			}

			break;
		}
	}
}

void HumanPlayer::Shot(WeaponType weapon, MapObj* target)
{
	Player::SlotType type = Player::SlotType(weapon + Player::stHyper);

	if (_player->GetSlot(type))
		GetLogic()->Shot(_player, target, type);
}

void HumanPlayer::Shot(WeaponType weapon)
{
	float viewAngle = D3DX_PI/5.5f;
	WeaponItem* wpn = GetWeapon(weapon);
	if (wpn && wpn->GetSlot()->GetRecord() && wpn->GetSlot()->GetRecord()->GetName() == "sphereGun")
		viewAngle = 0;

	Player* enemy = _player->FindClosestEnemy(viewAngle, false);
	Shot(weapon, enemy ? enemy->GetCar().mapObj : NULL);
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

	Player* enemy = _player->FindClosestEnemy(D3DX_PI/4, false);
	Shot(enemy ? enemy->GetCar().mapObj : 0);
}

void HumanPlayer::ResetCar()
{
	if (_player->GetCar().gameObj && (_player->GetCar().gameObj->IsAnyWheelContact() || _player->GetCar().gameObj->IsBodyContact()))
		_player->ResetCar();
}

HumanPlayer::WeaponType HumanPlayer::GetWeaponByIndex(int number)
{
	for (int i = stWeapon1; i <= stWeapon4; ++i)
	{
		WeaponType type = (WeaponType)i;

		WeaponItem* wpn = GetWeapon(type);
		if (wpn && (--number) < 0)
			return type;
	}

	return cWeaponTypeEnd;
}

WeaponItem* HumanPlayer::GetWeapon(WeaponType weapon)
{
	Player::SlotType type = Player::SlotType(weapon + Player::stHyper);

	Slot* slot = _player->GetSlotInst(type);
	WeaponItem* wpn = slot ? slot->GetItem().IsWeaponItem() : NULL;

	if (wpn && wpn->GetMapObj())
		return wpn;

	return NULL;
}

int HumanPlayer::GetWeaponCount()
{
	int count = 0;

	for (int i = stWeapon1; i <= stWeapon4; ++i)
	{
		if (GetWeapon((WeaponType)i) == NULL)
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
		WeaponType type = (WeaponType)(stWeapon1 + index);
		WeaponItem* weapon = GetWeapon(type);

		if (weapon != NULL && weapon->GetCurCharge() > 0)
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