#include "stdafx.h"
#include "game\AICar.h"

#include "game\World.h"

namespace r3d
{

namespace game
{

const float AICar::cSteerAngleBias = D3DX_PI/128.0f;
const float AICar::cMaxSpeedBlocking = 0.5f;
const float AICar::cMaxTimeBlocking = 1.0f;

const float AICar::cMaxVisibleDistShot = 100.0f;




AICar::AICar(Player* player): _path(_player->GetMap()->GetTrace().cTrackCnt), _player(player), _enbAI(true)
{
	_player->GetRace()->GetGame()->RegUser(this);
}

AICar::~AICar()
{
	_player->GetRace()->GetGame()->UnregUser(this);
}

AICar::PathState::PathState(unsigned trackCnt): cTrackCnt(trackCnt), curTile(0), nextTile(0), curNode(0)
{
	freeTracks.resize(cTrackCnt);
	lockTracks.resize(cTrackCnt);
}

AICar::PathState::~PathState()
{
	SetCurTile(0);
	SetNextTile(0);	
}

bool AICar::PathState::FindFirstUnlockTrack(unsigned track, unsigned target, unsigned& res)
{
	int inc = target > track ? +1 : -1;
	
	for (int i = 1; i < abs((int)(track - target)) + 1; ++i)
	{
		unsigned testTrack = track + i * inc;
		if (!lockTracks[testTrack])	
		{
			res = testTrack;
			return true;
		}
	}

	return false;
}

bool AICar::PathState::FindLastUnlockTrack(unsigned track, unsigned target, unsigned& res)
{
	int inc = target > track ? +1 : -1;
	
	for (int i = 1; i < abs((int)(track - target)) + 1; ++i)
	{
		unsigned testTrack = track + i * inc;
		if (lockTracks[testTrack])			
			return i > 1;

		res = testTrack;
	}

	return track != target;
}

bool AICar::PathState::FindFirstSiblingUnlock(unsigned track, unsigned& res)
{
	if (track > cTrackCnt - 1)
		return false;

	int down = track - 1;
	if (down >= 0 && !lockTracks[down])
	{
		res = down;
		return true;
	}

	unsigned up = track + 1;
	if (up < cTrackCnt && !lockTracks[up])
	{
		res = up;
		return true;
	}

	return false;
}

bool AICar::PathState::FindLastSiblingUnlock(unsigned track, unsigned& res)
{
	if (track > cTrackCnt - 1)
		return false;

	for (unsigned i = 1; i < cTrackCnt; ++i)
	{
		int down = track - i;
		if (down >= 0 && !lockTracks[down])
		{
			res = down;
			return true;
		}

		unsigned up = track + i;
		if (up < cTrackCnt && !lockTracks[up])
		{
			res = up;
			return true;
		}
	}

	return false;
}

void AICar::PathState::ComputeMovDir(AICar* owner, float deltaTime, const Player::CarState& car)
{
	LSL_ASSERT(curTile && nextTile);

	unsigned newTrack = car.track;
	//Поворот налево
	bool onLeft = D3DXVec2CCW(&curTile->GetTile().GetDir(), &nextTile->GetTile().GetDir()) > 0;
	//
	WayNode* movNode = curTile;
	//
	dirArea = 5.0f + abs(car.speed) * car.kSteerControl * 10;

	//Корректировка траектории движения относительно поворота
	if (nextTile->GetTile().GetTurnAngle() > D3DX_PI/12)
	{
		float edgeDist = Line2DistToPoint(nextTile->GetTile().GetEdgeLine(), car.pos);
		if (edgeDist < car.size)
		{
			float tileWidth = nextTile->GetPoint()->GetSize() / cTrackCnt;
			D3DXVECTOR2 targPnt = nextTile->GetPos2() + nextTile->GetTile().GetEdgeNorm() * tileWidth * static_cast<float>(cTrackCnt)/2.0f - car.pos;
			float proj = D3DXVec2Dot(&curTile->GetTile().GetDir(), &targPnt);

			if (proj < car.size)			
				movNode = nextTile;
			else
			{
				dirArea = proj;

				if (onLeft)
					newTrack = 0;
				else
					newTrack = cTrackCnt - 1;
			}
		}
	}

	//расчет притормаживания
	//Берм в качестве расчетного либо активный узел либо следующий тайл
	WayNode* curBreakTile = curNode ? curNode : nextTile;
	//
	if (curBreakTile->GetTile().GetTurnAngle() > D3DX_PI/12)
	{
		float kLong = car.kSteerControl * car.kSteerControl;
		float normDist = Line2DistToPoint(curBreakTile->GetTile().GetMidNormLine(), car.pos);
		float kBreak = -normDist + curBreakTile->GetPoint()->GetSize();
		
		float kRot = D3DXVec2Dot(&car.dir, &curBreakTile->GetTile().GetDir());
		kRot = 1.0f - std::max(kRot, 0.0f);

		//условия вхождения в торможение легче чем выхода из него
		if (!_break && car.speed * car.speed * kLong * kRot > 1.5f * kBreak)		
			_break = true;
		else if (_break && car.speed * car.speed * kLong * kRot < 1.0f * kBreak)
			_break = false;
	}
	else
		_break = false;

	//Корректировка newTrack с учетом заблокированных путей
	{
		//по умолчанию остаемся на car.track в случае неудач
		unsigned res = car.track;
		//Поиск последней досутпной дорожки в направлении маршрута
		if (FindLastUnlockTrack(car.track, newTrack, res))		
		{
			//Nothing
		}
		//Неудача, если дорожка car.track заблокирована, попытка перейти на другую
		else if (lockTracks[car.track])
		{
			//Поиск первой достижимой дорожки из car.track
			FindFirstSiblingUnlock(car.track, res);
		}

		newTrack = res;
	}

	//расчет траектории движения
	{
		D3DXVECTOR2 dir = movNode->GetTile().GetDir();
		D3DXVECTOR2 target = car.pos + dir * dirArea;
		target += movNode->GetTile().ComputeTrackNormOff(target, newTrack);
		
		moveDir = target - car.pos;
		D3DXVec2Normalize(&moveDir, &moveDir);
	}
}

void AICar::PathState::Update(AICar* owner, float deltaTime, const Player::CarState& car)
{
	const float cSecretPathChance = 0.3f;

	if (car.curTile != curTile)
	{
		if (car.lastNode &&
			(car.curTile == NULL ||
			(car.lastNode->GetNext() != car.curTile && car.lastNode != car.curTile && car.lastNode->GetTile().IsContains(car.pos3, true, NULL, 5.0f)))
		)
			SetCurTile(car.lastNode);
		else
			SetCurTile(car.curTile);
		SetNextTile(0);
	}

	if (curTile && nextTile == NULL)
	{		
		//Случайно выбираем предпочтительно нетупиковый узел принадлежайший WayPoint-у
		//Если существует следующий узел, то в первую очередь выбираем из него
		if (curTile->GetNext())
			SetNextTile(curTile->GetNext());
		//Иначе пытаемся найти ответвление от текущего узла
		else
			SetNextTile(curTile->GetPoint()->GetRandomNode(curTile, true));

		//езда по секретным путям. К сожалению сущесвтует проблема, когда  нас отсутсвует точка которую можно взять за текущую при заезде на секретный путь, поскольку в данном случае первая точка секретного пути может быть только следующей точкой, а текущую точкая также должна быть на расположена на этом пути и ее нету
		//пытаемся найти ответвление от текущего узла, если мы в тупике или если сработал шанс на секретный путь
		/*if (curTile->GetNext() == NULL || Random() < cSecretPathChance)
		{
			WayNode* nextTile = curTile->GetNext() != NULL ? curTile->GetNext()->GetPoint()->GetRandomNode(curTile->GetNext(), true) : curTile->GetPoint()->GetRandomNode(curTile, true);

			if (nextTile)
			{
				SetCurTile(nextTile);
				SetNextTile(nextTile);
			}
			else
				SetNextTile(curTile->GetNext());
		}
		else
			SetNextTile(curTile->GetNext());*/
	}

	curNode = curTile && curTile->IsContains2(car.pos) ? curTile : NULL;

	moveDir = NullVec2;
	if (nextTile)	
		ComputeMovDir(owner, deltaTime, car);
}

void AICar::PathState::SetCurTile(WayNode* value)
{
	if (lsl::Object::ReplaceRef(curTile, value))
		curTile = value;
}

void AICar::PathState::SetNextTile(WayNode* value)
{
	if (lsl::Object::ReplaceRef(nextTile, value))
		nextTile = value;
}

AICar::AttackState::AttackState(): target(0), backTarget(0), placeMineRandom(-1.0f)
{
}

AICar::AttackState::~AttackState()
{
	SetTarget(0);	
	SetBackTarget(0);
}

//поис более выгодного противника
Player* AICar::AttackState::FindEnemy(AICar* owner, const Player::CarState& car, int dir, Player* currentEnemy)
{
	//ищем новую или более доступную цель
	Player* enemy = owner->_player->FindClosestEnemy(D3DX_PI/4 * dir, true);

	if (currentEnemy && currentEnemy == enemy)
		return enemy;

	//проверяем текущую цель
	currentEnemy = currentEnemy && car.curTile && car.curTile->GetTile().IsZLevelContains(currentEnemy->GetCar().pos3) ? currentEnemy : NULL;

	//отличие от текущего target как минимум на target->GetCar().size
	bool testEnemy = enemy && (!currentEnemy || D3DXVec2Length(&(currentEnemy->GetCar().pos - enemy->GetCar().pos)) > currentEnemy->GetCar().size);

	return testEnemy ? enemy : currentEnemy;
}

void AICar::AttackState::ShotByEnemy(AICar* owner, const CarState& car, Player* enemy)
{
	//отклонение от напрваляющией в пределах enemy->GetCar().radius
	bool bShoot = enemy && abs(Line2DistToPoint(car.dirLine, enemy->GetCar().pos)) < enemy->GetCar().radius;
	//машина в передлах z достижимости
	bShoot = bShoot && abs(car.pos3.z - enemy->GetCar().pos3.z) < std::min(car.radius, enemy->GetCar().radius);
	//
	if (bShoot)
	{
		struct Weapon
		{
			Player::SlotType slot;
			WeaponItem* wpn;

			Weapon(Player::SlotType mSlot, WeaponItem* mWpn): slot(mSlot), wpn(mWpn) {}
		};
		typedef lsl::Vector<Weapon> WeaponList;

		WeaponList weaponList;

		float distToTarget = D3DXVec2Length(&(enemy->GetCar().pos - car.pos));
		int wpnCount = 0;

		for (int i = Player::stWeapon1; i <= Player::stWeapon4; ++i)
		{
			Slot* slot = owner->_player->GetSlotInst(Player::SlotType(i));
			WeaponItem* weapon = slot ? slot->GetItem().IsWeaponItem() : 0;
			if (weapon == NULL)
				continue;

			Proj::Type projType = weapon->GetDesc().Front().type;
			if (projType == Proj::ptHyper || projType == Proj::ptMine)
				continue;

			//if at least one weapon is not ready to return
			if (!weapon->GetWeapon()->IsReadyShot(std::max(weapon->GetWeapon()->GetDesc().shotDelay, 0.25f)))
				return;

			float dist = weapon->GetDesc().Front().maxDist;
			if (weapon->GetCurCharge() > 0)
				++wpnCount;

			bool tShoot = dist <= 0 || distToTarget < std::min(dist, cMaxVisibleDistShot);
			tShoot = tShoot && (weapon->GetCntCharge() == 0 || weapon->GetCurCharge() > 0);
			tShoot = tShoot && (weapon->GetDesc().Front().type == Proj::ptTorpeda || Line2DistToPoint(car.normLine, enemy->GetCar().pos) > car.radius/2.0f);

			if (tShoot)
				weaponList.push_back(Weapon((Player::SlotType)i, weapon));
		}

		struct DistSort: public std::binary_function<Weapon, Weapon, bool>
		{
			bool operator()(Weapon weapon1, Weapon weapon2)
			{
				return weapon1.wpn->GetDesc().Front().maxDist < weapon2.wpn->GetDesc().Front().maxDist;
			}
		};

		if (!weaponList.empty())
		{
			//сортируем по убыванию dist
			std::stable_sort(weaponList.begin(), weaponList.end(), DistSort());

			Weapon weapon = weaponList.front();
			if (Random() < 0.25f)
				weapon = weaponList[RandomRange(0, weaponList.size() - 1)];

			unsigned cntCharge = weapon.wpn->GetCntCharge();
			unsigned curCharge = weapon.wpn->GetCurCharge();

			const float lowP = 0.0f;
			const float highP = 0.7f;

			float roadDist = car.GetDist(true) / car.GetPathLength(true);
			//отсекаем до [lowP...highP], преобразуем к [0...1]
			float summPart = ClampValue((roadDist - lowP) / (highP - lowP), 0.0f, 1.0f);

			float wpPart = wpnCount > 0 ? 1.0f / wpnCount : 1.0f;
			float part = std::min(summPart / wpPart, 1.0f);
			float ammo = std::max(curCharge - (1.0f - part) * cntCharge, 0.0f);

			if ((cntCharge == 0 || ammo > 0.0f) && owner->_enbAI)								
				owner->GetLogic()->Shot(owner->_player, enemy->GetCar().mapObj, weapon.slot);
		}
	}
}

void AICar::AttackState::RunHyper(AICar* owner, const CarState& car, const PathState& path)
{
	Slot* hyperSlot = owner->_player->GetSlotInst(Player::stHyper);	
	WeaponItem* hyperDrive = hyperSlot ? hyperSlot->GetItem().IsWeaponItem() : 0;	
	
	float distTile = car.curTile->GetTile().ComputeLength(1.0f - car.curTile->GetTile().ComputeCoordX(car.pos));	
	float maxDistHyper = hyperDrive ? (hyperDrive->GetDesc().Front().speed + car.speed) : 0.0f;
	bool hyperDist = (path.nextTile == NULL || path.nextTile->GetTile().GetTurnAngle() < D3DX_PI/6 || distTile > maxDistHyper);
	
	if (hyperDrive && car.speed > 1.0f && hyperDist && !path._break)
	{
		const float lowP = 0.0f;
		const float highP = 0.7f;
		
		float roadDist = car.GetDist(true) / car.GetPathLength(true);
		//отсекаем до [lowP...highP], преобразуем к [0...1]
		float summPart = ClampValue((roadDist - lowP)/(highP - lowP), 0.0f, 1.0f);

		unsigned cntCharge = hyperDrive->GetCntCharge();
		unsigned curCharge = hyperDrive->GetCurCharge();		

		float ammo = std::max(curCharge - (1.0f - summPart) * cntCharge, 0.0f);
		if ((cntCharge == 0 || ammo > 0.0f) && owner->_enbAI)
			owner->GetLogic()->Shot(owner->_player, NULL, Player::stHyper);
	}

}

void AICar::AttackState::PlaceMine(AICar* owner, const CarState& car, const PathState& path)
{	
	Slot* minesSlot = owner->_player->GetSlotInst(Player::stMine);
	WeaponItem* mines = minesSlot ? minesSlot->GetItem().IsWeaponItem() : 0;

	if (mines && mines->GetWeapon() && car.speed > 5.0f)
	{
		const float lowP = 0.05f;
		const float highP = 0.95f;

		if (placeMineRandom == -1.0f)
			placeMineRandom = RandomRange(-0.5f, 0.0f);
		
		float roadDist = car.GetDist(true) / car.GetPathLength(true);		
		//отсекаем до [lowP...highP], преобразуем к [0...1]
		float summPart = ClampValue((roadDist - lowP)/(highP - lowP), 0.0f, 1.0f);
		if (summPart > 0.0f && summPart < 1.0f)
		{
			//Цель сзади, +30% мин
			if (backTarget && D3DXVec2Length(&(backTarget->GetCar().pos - car.pos)) < 30.0f)
				summPart += 0.3f;
			summPart = ClampValue(summPart + placeMineRandom, 0.0f, 1.0f);
		}

		unsigned maxCharge = 3;
		if (mines->GetWeapon()->IsMaslo())
			maxCharge = 2;

		unsigned cntCharge = std::min(mines->GetCntCharge(), maxCharge);
		unsigned curCharge = cntCharge - std::min(mines->GetCntCharge() - mines->GetCurCharge(), cntCharge);
		float ammo = std::max(curCharge - (1.0f - summPart) * cntCharge, 0.0f);

		if ((cntCharge == 0 || ammo > 0.0f) && owner->_enbAI)
		{
			owner->GetLogic()->Shot(owner->_player, NULL, Player::stMine);
			placeMineRandom = RandomRange(-0.5f, 0.0f);
		}
	}
}

void AICar::AttackState::Update(AICar* owner, float deltaTime, const Player::CarState& car, const PathState& path)
{
	if (car.curTile == NULL)
		return;

	//ищем цель спереди
	SetTarget(FindEnemy(owner, car, 1, target));
	//ищем цель сзади
	SetBackTarget(FindEnemy(owner, car, -1, backTarget));

	if (target)
		ShotByEnemy(owner, car, target);
	if (backTarget)
		ShotByEnemy(owner, car, backTarget);

	RunHyper(owner, car, path);

	PlaceMine(owner, car, path);
}

void AICar::AttackState::SetTarget(Player* value)
{
	if (Object::ReplaceRef(target, value))
		target = value;
}

void AICar::AttackState::SetBackTarget(Player* value)
{
	if (Object::ReplaceRef(backTarget, value))
		backTarget = value;
}

AICar::ControlState::ControlState(): blocking(false), timeBlocking(0.0f), backMovingMode(false), backMoving(false), timeBackMoving(0.0f), timeResetBlockCar(0.0f)
{
}

void AICar::ControlState::UpdateResetCar(AICar* owner, float deltaTime, const Player::CarState& car)
{
	//Reset заблокированной машины
	if (car.mapObj && (blocking || car.curTile == NULL || abs(car.speed) < cMaxSpeedBlocking))
	{
		timeResetBlockCar += deltaTime;
		if (timeResetBlockCar > 3.0f * cMaxTimeBlocking)
		{
			owner->_player->ResetCar();
			timeResetBlockCar = 0.0f;
		}
	}
	else	
		timeResetBlockCar = 0.0f;
}

void AICar::ControlState::Update(AICar* owner, float deltaTime, const Player::CarState& car, const PathState& path)
{
	//Вычисляем угол между направлением машины и направляющей движения
	steerAngle = abs(acos(D3DXVec2Dot(&car.dir, &path.moveDir)));
	//учет инерционности рулевого управления
	//steerAngle = std::max(0.0f, steerAngle - D3DX_PI * deltaTime * 2.0f);
	//float errorSteer = 
		
	//Угол поворота колес
	if (steerAngle > cSteerAngleBias)	
		steerAngle = D3DXVec2CCW(&car.dir, &path.moveDir) > 0 ? steerAngle : -steerAngle;
	else
		steerAngle = 0;

	//состояние заблокированности
	if (abs(car.speed) < cMaxSpeedBlocking)
	{
		timeBlocking += deltaTime;
		if (timeBlocking > cMaxTimeBlocking)
		{
			timeBlocking = 0.0f;
			blocking = true;
		}
	}
	else
	{
		timeBlocking = 0.0f;
		blocking = false;
	}

	//движение назад
	if (!backMovingMode)
	{
		backMovingMode = blocking;
		backMoving = blocking;
	}
	if (backMovingMode)
	{
		timeBackMoving += deltaTime;
		if (timeBackMoving > cMaxTimeBlocking || (backMoving && abs(steerAngle) < cSteerAngleBias && timeBackMoving > 0.5f* cMaxTimeBlocking))
		{
			backMoving = !backMoving;
			timeBackMoving = 0.0f;
			backMovingMode = blocking;
		}

		if (backMoving)		
			steerAngle = -steerAngle;
	}

	if (owner->_enbAI)
	{
		GameCar::MoveCarState moveState = backMoving ? GameCar::mcBack : GameCar::mcAccel;
		if (car.cheatSlower)
			moveState = GameCar::mcNone;

		car.gameObj->SetMoveCar(path._break ? GameCar::mcBrake : moveState);
		//Контроль за поворотом
		car.gameObj->SetSteerWheel(game::GameCar::smManual);
		//Поворот колес
		car.gameObj->SetSteerWheelAngle(steerAngle);
	}
}

void AICar::UpdateAI(float deltaTime, const Player::CarState& car)
{
	_path.Update(this, deltaTime, car);	
	_attack.Update(this, deltaTime, car, _path);
	_control.Update(this, deltaTime, car, _path);
}

void AICar::OnProcessEvent(unsigned id, EventData* data)
{
	if (id == cPlayerDispose)
	{
		if (_attack.target && _attack.target->GetId() == data->playerId)
		{
			_attack.SetTarget(NULL);
		}

		if (_attack.backTarget && _attack.backTarget->GetId() == data->playerId)
		{
			_attack.SetBackTarget(NULL);
		}

		return;
	}
}

void AICar::OnProgress(float deltaTime)
{
	if (GetCar().mapObj)
	{
		UpdateAI(deltaTime, GetCar());
	}
	
	//в обход GetCar
	if (_enbAI)
		_control.UpdateResetCar(this, deltaTime, GetCar());
}

const AICar::CarState& AICar::GetCar()
{
	return _player->GetCar();
}

Logic* AICar::GetLogic()
{
	return _player->GetRace()->GetWorld()->GetLogic();
}

}

}