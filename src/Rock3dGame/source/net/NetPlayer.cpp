#include "stdafx.h"

#include "net\NetPlayer.h"
#include "game\World.h"

namespace r3d
{

namespace game
{

/*NetPlayerResponse::NetPlayerResponse(const Desc& desc): NetModelRPC(desc), _player(NULL)
{
	DescData data;
	data.Read(desc.stream);

	_player = NetGame::I()->GetPlayer(data.playerId);
	_player->_response = this;

	syncState(ssDelta);
	syncStateTarget(net::cNetTargetOthers | _player->ownerId());
}

void NetPlayerResponse::OnDescWrite(const net::NetMessage& msg, std::ostream& stream)
{
	DescData(_player->id()).Write(stream);
}

void NetPlayerResponse::OnSerialize(const net::NetMessage& msg, net::BitStream& stream)
{
	_player->ResponseStream(msg, stream);
}*/




NetPlayer::NetPlayer(const Desc& desc): NetModelRPC(desc), _net(NetGame::I()), _player(NULL), _human(NULL), _aiPlayer(NULL), _modelOwner(false), _dAlpha(0), _raceReady(false), _raceGoWait(false), _raceFinish(false) //, _response(NULL)
{	
	RegRPC(&NetPlayer::OnSetGamerId);
	RegRPC(&NetPlayer::OnSetColor);
	RegRPC(&NetPlayer::OnSetCar);
	RegRPC(&NetPlayer::OnCarSlotsChanged);
	RegRPC(&NetPlayer::OnRaceReady);
	RegRPC(&NetPlayer::OnRaceGoWait);
	RegRPC(&NetPlayer::OnRaceFinish);
	RegRPC(&NetPlayer::OnShot);
	RegRPC(&NetPlayer::OnTakeBonus);
	RegRPC(&NetPlayer::OnMineContact1);
	RegRPC(&NetPlayer::OnMineContact2);

	DescData data;
	data.Read(desc.stream);

	if (Race::IsHumanId(data.playerId) || Race::IsOpponentId(data.playerId))
	{
		unsigned netSlot = data.netSlot;
		int opponentId = netSlot << Race::cOpponentBit;

		_player = _net->game()->GetRace()->GetPlayerByNetSlot(netSlot);
		
		if (_player == NULL)
		{
			_player = race()->AddPlayer(opponentId, GenerateGamerId(), netSlot, GenerateColor());
			_modelOwner = true;
		}

		_player->AddRef();

		if (desc.owner)
		{
			_player->SetId(Race::cHuman);
			_human = race()->CreateHuman(_player);
			_human->AddRef();
		}
		else
			_player->SetId(opponentId);
	}
	else
	{
		_player = _net->game()->GetRace()->GetPlayerById(data.playerId);

		if (_player == NULL)
		{
			_player = race()->AddPlayer(data.playerId);
			_modelOwner = true;
		}

		_player->AddRef();

		if (desc.owner)
		{
			_aiPlayer = race()->AddAIPlayer(_player);
			_aiPlayer->AddRef();
		}
	}	

	//if ((desc.owner && _net->isClient()) || (!desc.owner && _net->isHost()))
	//{
		syncState(ssDelta);
		//syncStateTarget(net::cServerPlayer);
		//syncStatePriority(net::cSyncStatePriorityHigh);
	//}

	net::INetConnection* connection = _net->netService().GetConnectionById(ownerId());
	if (connection)	
		_player->SetNetName(connection->userName());

	_net->RegPlayer(this);

	if (_net->isHost())
	{
		//std::ostream& stream = net::GetNetService().player()->NewModel<NetPlayerResponse>();
		//NetPlayerResponse::DescData data(desc.id);
		//data.Write(stream);
		//net::GetNetService().player()->CloseCmd();
	}
}

NetPlayer::~NetPlayer()
{
	_net->UnregPlayer(this);

	if (_aiPlayer)
	{
		_aiPlayer->Release();
		race()->DelAIPlayer(_aiPlayer);
	}

	if (_human)
	{
		_human->Release();

		if (!owner())
			race()->FreeHuman();
	}

	_player->Release();
	_player->FreeCar(true);

	if (_modelOwner || game()->GetRace()->IsSkirmish())
	{
		if (!owner())
			race()->DelPlayer(_player);
	}

	//if (_response)
	//	net::GetNetService().player()->DeleteModel(_response, true);
}

void NetPlayer::DoRaceReady(bool ready)
{
	if (_raceReady == ready)
		return;

	_raceReady = ready;
	_net->SendEvent(cNetPlayerReady);
}

void NetPlayer::DoRaceGoWait(bool goWait)
{
	if (_raceGoWait == goWait)
		return;

	_raceGoWait = goWait;
	_net->SendEvent(cNetPlayerGoWait);
}

void NetPlayer::DoRaceFinish(bool finish)
{
	if (_raceFinish == finish)
		return;

	_raceFinish = finish;
	_net->SendEvent(cNetPlayerFinish);
}

void NetPlayer::DoShot(MapObj* target, ShotSlots& slots, unsigned projId, CoordList* coordList, bool readMode)
{
	if (_player->GetCar().mapObj == NULL)
	{
		LSL_LOG("NetPlayer::DoShot car=NULL");
	}

	Weapon::ProjList projList;
	unsigned coordIndex = 0;
	NxMat34 nxMat(true);	

	Proj::ShotContext ctx;
	ctx.shot.SetTargetMapObj(target);
	ctx.logic = _net->game()->GetWorld()->GetLogic();	

	for (unsigned i = 0; i < slots.count(); ++i)
	{
		Player::SlotType type = slots.AsWpn(i); 
		Slot* slot = _player->GetSlotInst(type);		
		WeaponItem* wpn = slot ? slot->GetItem().IsWeaponItem() : NULL;		

		if (slots.Get(i) && wpn)
		{
			projList.clear();

			if (coordList && coordIndex < coordList->size()) 
			{
				nxMat.t = NxVec3((*coordList)[coordIndex]);
				ctx.projMat = &nxMat;
			}
			else
				ctx.projMat = NULL;

			if (!_player->Shot(ctx, type, projId, wpn->GetCurCharge() - 1, &projList))
			{
				slots.Set(i, false);

				if (readMode)
					++coordIndex;

				continue;
			}

			LSL_ASSERT(projList.size() > 0);

			Proj* proj = projList.front();
			D3DXVECTOR3 worldPos = proj->GetWorldPos();

			if (!readMode)
			{
				if (coordList && coordIndex < coordList->size())
					(*coordList)[coordIndex] = worldPos;
				else if (coordList)
					coordList->push_back(worldPos);
			}

			++coordIndex;
		}
	}	
}

void NetPlayer::SendColor(const D3DXCOLOR& value, bool failed, unsigned target)
{
	std::ostream& stream = NewRPC(target, &NetPlayer::OnSetColor);
	net::Write(stream, value);
	net::Write(stream, failed);
	CloseRPC();
}

void NetPlayer::SendGamerId(int value, bool failed, unsigned target)
{
	std::ostream& stream = NewRPC(target, &NetPlayer::OnSetGamerId);
	net::Write(stream, value);
	net::Write(stream, failed);
	CloseRPC();
}

void NetPlayer::SlotsWrite(std::ostream& stream)
{
	for (int i = 0; i < Player::cSlotTypeEnd; ++i)
	{
		Slot* slot = _player->GetSlotInst(Player::SlotType(i));
		WeaponItem* wpn = slot ? slot->GetItem().IsWeaponItem() : 0;		
		ArmorItem* armor = slot && slot->GetType() == Slot::stArmor ? &slot->GetItem<ArmorItem>() : NULL;

		std::string recName = slot && slot->GetRecord() ? slot->GetRecord()->GetName() : "";
		unsigned chargeCnt = wpn ? wpn->GetCntCharge() : 0;
		bool armor4 = armor && armor->CheckArmor4();

		net::Write(stream, recName);
		net::Write(stream, chargeCnt);
		net::Write(stream, armor4);
	}
}

void NetPlayer::SlotsRead(std::istream& stream)
{
	for (int i = 0; i < Player::cSlotTypeEnd; ++i)
	{
		std::string recName;
		unsigned chargeCnt;
		bool armor4;

		net::Read(stream, recName);
		net::Read(stream, chargeCnt);
		net::Read(stream, armor4);

		if (GetCar())
		{
			Slot* slot = recName != "" ? &race()->GetWorkshop().GetSlot(recName) : NULL;
			race()->GetGarage().InstalSlot(_player, (Player::SlotType)i, GetCar(), slot, chargeCnt);

			slot = _player->GetSlotInst(Player::SlotType(i));
			ArmorItem* armor = slot && slot->GetType() == Slot::stArmor ? &slot->GetItem<ArmorItem>() : NULL;
			if (armor)
				armor->InstalArmor4(armor4);
		}
	}
}

void NetPlayer::OnSetGamerId(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	int gamerId;
	bool failed;

	net::Read(stream, gamerId);
	net::Read(stream, failed);

	if (_net->isHost())
	{
		if (!CheckGamerId(gamerId))
		{
			msg.Discard();
			gamerId = _player->GetGamerId();
			failed = true;

			if (player()->id() != msg.sender)
			{
				SendGamerId(gamerId, failed, msg.sender);
				return;
			}
		}
	}

	_player->SetGamerId(gamerId);

	_net->SendEvent(cNetPlayerSetGamerId, &NetEventData(id(), failed));
}

void NetPlayer::OnSetColor(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	D3DXCOLOR color;
	bool failed;

	net::Read(stream, color);
	net::Read(stream, failed);

	if (_net->isHost())
	{
		if (!CheckColor(color))
		{
			msg.Discard();
			SendColor(_player->GetColor(), true, msg.sender);
			return;
		}
	}

	_player->SetColor(color);

	_net->SendEvent(cNetPlayerSetColor, &NetEventData(id(), failed));
}

void NetPlayer::OnSetCar(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	std::string carName;
	net::Read(stream, carName);

	Garage::Car* car = race()->GetGarage().FindCar(carName);
	if (car)
		race()->GetGarage().BuyCar(_player, car);
}

void NetPlayer::OnCarSlotsChanged(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	int money;

	net::Read(stream, money);	
	SlotsRead(stream);

	model()->SetMoney(money);
}

void NetPlayer::OnRaceReady(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	bool raceReady;
	net::Read(stream, raceReady);

	DoRaceReady(raceReady);
}

void NetPlayer::OnRaceGoWait(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	bool raceGoWait;
	net::Read(stream, raceGoWait);

	DoRaceGoWait(raceGoWait);
}

void NetPlayer::OnRaceFinish(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	bool raceFinish;
	net::Read(stream, raceFinish);

	DoRaceFinish(raceFinish);
}

void NetPlayer::OnShot(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	unsigned target;	
	ShotSlots slots;
	unsigned projId;
	CoordList coordList;

	net::Read(stream, target);
	net::Read(stream, &slots, sizeof(slots));
	net::Read(stream, &projId, sizeof(projId));

	unsigned coordCount = 0;
	for (unsigned i = 0; i < slots.count(); ++i)
		if (slots.Get(i))
			++coordCount;

	for (unsigned i = 0; i < coordCount; ++i)
	{
		D3DXVECTOR3 worldPos;
		net::Read(stream, &worldPos, sizeof(worldPos));

		coordList.push_back(worldPos);
	}

	MapObj* mapObj = map()->GetMapObj(target);

	DoShot(mapObj, slots, projId, &coordList, true);
}

void NetPlayer::OnTakeBonus(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	unsigned bonusId;
	int type;
	float value;

	net::Read(stream, bonusId);
	net::Read(stream, type);
	net::Read(stream, value);

	MapObj* bonus = map()->GetMapObj(bonusId);

	if (bonus == NULL)		
	{
		LSL_LOG("NetPlayer::OnTakeBonus bonus == NULL");
		msg.Discard();
		return;
	}

	_player->TakeBonus(&bonus->GetGameObj(), (Player::BonusType)type, value);
}

void NetPlayer::OnMineContact1(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{	
	unsigned plrId;
	unsigned projId;	
	D3DXVECTOR3 point;

	net::Read(stream, plrId);
	net::Read(stream, projId);	
	net::Read(stream, point);

	NetPlayer* bonusPlr = _net->GetPlayer(plrId);
	Proj* bonus = bonusPlr ? bonusPlr->model()->GetBonusProj(projId) : NULL;	

	if (bonusPlr == NULL)		
	{
		LSL_LOG("NetRace::OnMineContact1 bonusPlr == NULL");
		msg.Discard();
		return;
	}

	if (bonus == NULL)
	{
		LSL_LOG("NetRace::OnMineContact1 bonus == NULL");
		msg.Discard();
		return;
	}

	bonus->MineContact(_player->GetCar().gameObj, point);
}

void NetPlayer::OnMineContact2(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	unsigned projId;	

	D3DXVECTOR3 point;

	net::Read(stream, projId);	
	net::Read(stream, point);

	MapObj* bonus = map()->GetMapObj(projId);	

	if (bonus == NULL || bonus->GetGameObj().IsProj() == NULL)
	{
		LSL_LOG("NetRace::OnMineContact2 bonus == NULL");
		msg.Discard();
		return;
	}

	bonus->GetGameObj().IsProj()->MineContact(_player->GetCar().gameObj, point);
}

void NetPlayer::ControlStream(const net::NetMessage& msg, net::BitStream& stream)
{
	ResponseStream(msg, stream);

	//GameCar* car = _player->GetCar().gameObj;
	//if (car == NULL)
	//	return;
	//
	//BYTE moveState = car->GetMoveCar();
	//BYTE steerState = car->GetSteerWheel();
	//float steerWheelsAngle = car->GetSteerWheelAngle();
	//
	//stream.Serialize(moveState);
	//stream.Serialize(steerState);
	//stream.Serialize(steerWheelsAngle);
	//
	//if (stream.isReading())
	//{
	//	car->SetMoveCar((GameCar::MoveCarState)moveState);
	//	//car->SetSteerWheel(GameCar::smManual);
	//	car->SetSteerWheel((GameCar::SteerWheelState)steerState);
	//	car->SetSteerWheelAngle(steerWheelsAngle);
	//}
}

void NetPlayer::ResponseStream(const net::NetMessage& msg, net::BitStream& stream)
{
	GameCar* car = _player->GetCar().gameObj;
	if (car == NULL)
		return;

	D3DXVECTOR3 pos = car->GetPxActor().GetPos();
	D3DXQUATERNION rot = car->GetPxActor().GetRot();	
	D3DXVECTOR3 linVel = car->GetNxActor()->getLinearMomentum().get();
	D3DXVECTOR3 angVel = car->GetNxActor()->getAngularMomentum().get();
	BYTE moveState = car->GetMoveCar();
	BYTE steerState = car->GetSteerWheel();
	float steerWheelsAngle = car->GetSteerWheelAngle();

	stream.Serialize(pos);
	stream.Serialize(rot);
	stream.Serialize(linVel);
	stream.Serialize(angVel);
	stream.Serialize(moveState);
	stream.Serialize(steerState);
	stream.Serialize(steerWheelsAngle);

	if (stream.isReading() && !(owner() && _net->isClient()) && !_player->GetFinished())
	{
		_dAlpha = 1.0f;

		D3DXVECTOR3 dPos = pos - car->GetPxActor().GetPos();		
		float dPosLength = D3DXVec3Length(&dPos);
		if (dPosLength > 4.0f)
		{
			car->SetPosSync(pos - car->GetGrActor().GetPos());
			car->GetPxActor().SetPos(pos);
		}
		else if (dPosLength > 0.1f)
			linVel += dPos * 2.0f * car->GetNxActor()->getMass();

		/*D3DXVECTOR3 dPos = pos - car->GetPxActor().GetPos();
		float dPosLength = D3DXVec3Length(&dPos);
		if (dPosLength > 4.0f) //>2.0f
		{
			car->SetPosSync2(car->GetGrActor().GetPos() - pos, NullVector);
			car->GetPxActor().SetPos(pos);			
		}
		else
		{
			D3DXVECTOR3 dPos1 = car->GetGrActor().GetPos() - car->GetPxPosLerp();
			D3DXVECTOR3 dPos2 = pos - car->GetPxPosLerp();
			if (D3DXVec3Length(&(dPos1 - dPos2)) > 0.5f)
				car->SetPosSync2(dPos1, dPos2);

			if (dPosLength > 0.1f)
			{
				linVel += dPos * 2.0f * car->GetNxActor()->getMass();
			}
		}*/

		/*D3DXVECTOR3 dPos = pos - car->GetGrActor().GetPos();
		float dPosLength = D3DXVec3Length(&dPos);
		if (D3DXVec3Length(&(car->GetPxActor().GetPos() - pos)) > 1.0f)
		{
			car->GetPxActor().SetPos(pos);
			car->SetPosSync(dPos);
		}*/

		D3DXQUATERNION dRot;
		QuatRotation(dRot, car->GetGrActor().GetRot(), rot);
		D3DXVECTOR3 dRotAxis;
		float dRotAngle;
		D3DXQuaternionToAxisAngle(&dRot, &dRotAxis, &dRotAngle);
		if (abs(dRotAngle) > D3DX_PI/24)
		{
			car->GetPxActor().SetRot(rot);
			car->SetRotSync(dRot);
		}

		car->GetNxActor()->setLinearMomentum(NxVec3(linVel));
		car->GetNxActor()->setAngularMomentum(NxVec3(angVel));
		
		car->SetMoveCar((GameCar::MoveCarState)moveState);		
		car->SetSteerWheel((GameCar::SteerWheelState)steerState);
		car->SetSteerWheelAngle(steerWheelsAngle);
	}
}

void NetPlayer::OnDescWrite(const net::NetMessage& msg, std::ostream& stream)
{
	DescData(_player->GetId(), _player->GetNetSlot()).Write(stream);
}

void NetPlayer::OnStateRead(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream)
{
	D3DXCOLOR color;
	std::string carName;
	int gamerId;

	net::Read(stream, color);
	net::Read(stream, carName);
	net::Read(stream, _raceReady);
	net::Read(stream, _raceGoWait);
	net::Read(stream, _raceFinish);
	net::Read(stream, gamerId);

	_player->SetColor(color);
	_player->SetGamerId(gamerId);

	Garage::Car* car = race()->GetGarage().FindCar(carName);
	if (car)
		race()->GetGarage().BuyCar(_player, car);

	SlotsRead(stream);
}

void NetPlayer::OnStateWrite(const net::NetMessage& msg, std::ostream& stream)
{
	net::Write(stream, _player->GetColor());
	net::Write(stream, _player->GetCar().record ? _player->GetCar().record->GetName() : "");	
	net::Write(stream, _raceReady);
	net::Write(stream, _raceGoWait);
	net::Write(stream, _raceFinish);
	net::Write(stream, _player->GetGamerId());

	SlotsWrite(stream);
}

void NetPlayer::OnSerialize(const net::NetMessage& msg, net::BitStream& stream)
{
	ControlStream(msg, stream);	
}

void NetPlayer::Process(float deltaTime)
{
	GameCar* car = _player->GetCar().gameObj;
	if (car == NULL)
		return;

	if (_dAlpha > 0.0f && (_dAlpha -= deltaTime) <= 0)
	{
		_dAlpha = 0.0f;
		car->SetMoveCar(GameCar::mcNone);
		car->SetSteerWheel(GameCar::swNone);		
	}	
}

void NetPlayer::Shot(MapObj* target, Player::SlotType type)
{
	ShotSlots slots;
	unsigned shotCnt = 0;
	if (type != Player::cSlotTypeEnd)
	{
		if (_player->GetSlotInst(type)->GetItem().IsWeaponItem()->IsReadyShot())
		{
			slots.Set(type, true);
			shotCnt = 1;
		}
	}
	else
		for (unsigned i = 0; i < slots.count(); ++i)		
		{
			Player::SlotType type = slots.AsWpn(i);
			if (type == Player::stMine || type == Player::stHyper)
				continue;

			WeaponItem* wpn = _player->GetSlotInst(type) ? _player->GetSlotInst(type)->GetItem().IsWeaponItem() : NULL;
			if (wpn && wpn->IsReadyShot())
			{
				slots.Set(i, true);
				++shotCnt;
			}
		}

	if (shotCnt > 0)
	{
		unsigned targetId = target ? target->GetId() : Map::cDefMapObjId;
		unsigned netTarget = net::cNetTargetAll;
		unsigned projId = _player->GetNextBonusProjId();
		CoordList coordList;

		//any shot goes from host
		//if (_net->isHost() || type == Player::stHyper || type == Player::stMine)
		{
			netTarget = net::cNetTargetOthers;
			DoShot(target, slots, projId, &coordList, false);

			if (coordList.size() == 0)
				return;
		}

		std::ostream& stream = NewRPC(netTarget, &NetPlayer::OnShot);
		net::Write(stream, targetId);
		net::Write(stream, &slots, sizeof(slots));
		net::Write(stream, &projId, sizeof(projId));

		for (unsigned i = 0; i < coordList.size(); ++i)
			net::Write(stream, &coordList[i], sizeof(coordList[i]));

		CloseRPC();
	}
}

void NetPlayer::Shot(MapObj* target)
{
	Shot(target, Player::cSlotTypeEnd);
}

void NetPlayer::TakeBonus(MapObj* bonus, GameObject::BonusType type, float value)
{
	if (owner())
	{
		LSL_ASSERT(bonus);

		std::ostream& stream = NewRPC(net::cNetTargetAll, &NetPlayer::OnTakeBonus);
		net::Write(stream, bonus->GetId());
		net::Write(stream, (int)type);
		net::Write(stream, value);
		CloseRPC();
	}
}

void NetPlayer::MineContact(Proj* sender, const D3DXVECTOR3& point)
{
	if (owner())
	{
		LSL_ASSERT(sender);

		unsigned id = Player::cBonusProjUndef;

		NetGame::NetPlayers::const_iterator iter = _net->players().begin();
		for (; iter != _net->players().end(); ++iter)
		{
			id = (*iter)->model()->GetBonusProjId(sender);
			if (id != Player::cBonusProjUndef)
				break;
		}

		if (id != Player::cBonusProjUndef)
		{
			std::ostream& stream = NewRPC(net::cNetTargetAll, &NetPlayer::OnMineContact1);
			net::Write(stream, (*iter)->id());
			net::Write(stream, id);			
			net::Write(stream, point);
			CloseRPC();
		}
		else
		{
			LSL_ASSERT(sender->GetMapObj());

			id = sender->GetMapObj()->GetId();

			std::ostream& stream = NewRPC(net::cNetTargetAll, &NetPlayer::OnMineContact2);
			net::Write(stream, id);			
			net::Write(stream, point);
			CloseRPC();
		}
	}
}

int NetPlayer::GetGamerId() const
{
	return _player->GetGamerId();
}

void NetPlayer::SetGamerId(int value)
{
	SendGamerId(value, false, net::cNetTargetAll);
}

bool NetPlayer::CheckGamerId(int value) const
{
	for (NetGame::NetPlayers::const_iterator iter = _net->netPlayers().begin(); iter != _net->netPlayers().end(); ++iter)
	{
		if (*iter == this)
			continue;

		if ((*iter)->GetGamerId() == value)
			return false;
	}

	return true;
}

int NetPlayer::GenerateGamerId() const
{
	Tournament& tournament = _net->game()->GetRace()->GetTournament();

	for (Tournament::Planets::const_iterator iter = tournament.GetGamers().begin(); iter != tournament.GetGamers().end(); ++iter)
		if (CheckGamerId((*iter)->GetId()))
			return (*iter)->GetId();

	return -1;
}

const D3DXCOLOR& NetPlayer::GetColor() const
{
	return _player->GetColor();
}

void NetPlayer::SetColor(const D3DXCOLOR& value)
{
	_player->SetColor(value);

	SendColor(value, false, net::cNetTargetOthers);
}

bool NetPlayer::CheckColor(const D3DXCOLOR& value) const
{
	for (NetGame::NetPlayers::const_iterator iter = _net->netPlayers().begin(); iter != _net->netPlayers().end(); ++iter)
	{
		if (*iter == this)
			continue;

		D3DXCOLOR diff = (*iter)->GetColor() - value;
		if (abs(diff.r) < 0.001f && abs(diff.g) < 0.001f && abs(diff.b) < 0.001f && abs(diff.a) < 0.001f)
			return false;
	}

	return true;
}

D3DXCOLOR NetPlayer::GenerateColor() const
{
	for (int i = 0; i < Player::cColorsCount; ++i)
		if (CheckColor(Player::cLeftColors[i]))
			return Player::cLeftColors[i];

	for (int i = 0; i < Player::cColorsCount; ++i)
		if (CheckColor(Player::cRightColors[i]))
			return Player::cRightColors[i];

	return clrWhite;
}

Garage::Car* NetPlayer::GetCar()
{
	return race()->GetGarage().FindCar(_player->GetCar().record);
}

bool NetPlayer::BuyCar(Garage::Car* car)
{
	if (race()->GetGarage().BuyCar(_player, car))
	{
		std::ostream& stream = NewRPC(net::cNetTargetOthers, &NetPlayer::OnSetCar);
		net::Write(stream, car->GetRecord() ? car->GetRecord()->GetName() : "");
		CloseRPC();

		return true;
	}
	return false;
}

void NetPlayer::CarSlotsChanged()
{
	LSL_ASSERT(GetCar());

	std::ostream& stream = NewRPC(net::cNetTargetOthers, &NetPlayer::OnCarSlotsChanged);

	net::Write(stream, model()->GetMoney());
	SlotsWrite(stream);	

	CloseRPC();
}

void NetPlayer::RaceStarted()
{
	_raceReady = false;
	_raceGoWait = false;
	_raceFinish = false;
	_modelOwner = false;
}

bool NetPlayer::IsRaceReady()
{
	return _raceReady;
}

void NetPlayer::RaceReady(bool ready)
{
	if (_raceReady == ready)
		return;

	DoRaceReady(ready);
	
	std::ostream& stream = NewRPC(net::cNetTargetOthers, &NetPlayer::OnRaceReady);
	net::Write(stream, _raceReady);
	CloseRPC();
}

bool NetPlayer::IsRaceGoWait()
{
	return _raceGoWait;
}

void NetPlayer::RaceGoWait(bool goWait)
{
	if (_raceGoWait == goWait)
		return;
	
	DoRaceGoWait(goWait);

	std::ostream& stream = NewRPC(net::cNetTargetOthers, &NetPlayer::OnRaceGoWait);
	net::Write(stream, _raceGoWait);
	CloseRPC();
}

bool NetPlayer::IsRaceFinish()
{
	return _raceFinish;
}

void NetPlayer::RaceFinish(bool finish)
{
	if (_raceFinish == finish)
		return;

	DoRaceFinish(finish);

	std::ostream& stream = NewRPC(net::cNetTargetOthers, &NetPlayer::OnRaceFinish);
	net::Write(stream, _raceFinish);
	CloseRPC();
}

GameMode* NetPlayer::game()
{
	return _net->game();
}

Race* NetPlayer::race()
{
	return game()->GetRace();
}

Map* NetPlayer::map()
{
	return race()->GetMap();
}

Player* NetPlayer::model()
{
	return _player;
}

}

}