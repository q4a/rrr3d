#include "stdafx.h"
#include "game\AIPLayer.h"

#include "game\World.h"

namespace r3d
{

namespace game
{

AIPlayer::AIPlayer(Player* player): _player(player), _car(0)
{
	LSL_ASSERT(_player);

	_player->AddRef();	

	if (!_player->IsHuman())
		_player->SetCheat(Player::cCheatEnableFaster | Player::cCheatEnableSlower);
}

AIPlayer::~AIPlayer()
{
	FreeCar();

	if (!_player->IsHuman())
		_player->SetCheat(Player::cCheatDisable);

	_player->Release();
}

void AIPlayer::OnProgress(float deltaTime)
{
	if (_car)
		_car->OnProgress(deltaTime);
}

void AIPlayer::CreateCar()
{
	if (!_car)
		_car = new AICar(_player);
}

void AIPlayer::FreeCar()
{
	if (_car && _car->GetCar().mapObj)
		_car->GetCar().gameObj->SetMoveCar(GameCar::mcNone);

	lsl::SafeDelete(_car);
}

World* AIPlayer::GetWorld()
{
	return _player->GetRace()->GetWorld();
}

Player* AIPlayer::GetPlayer()
{
	return _player;
}

AICar* AIPlayer::GetCar()
{
	return _car;
}




AISystem::AISystem(Race* race): _race(race), _aiDebug(0)
{
}

AISystem::~AISystem()
{
	FreeDebug();
	ClearPlayerList();
}

template<class _Iter> void AISystem::LockChainTrack(const _Iter& iter1, const _Iter& iter2, AICar* ignoreCar, unsigned track)
{
	for (_Iter iter = iter1; iter != iter2; ++iter)
		if ((*iter)->car != ignoreCar)
		{
			(*iter)->car->_path.freeTracks[track] = false;
			(*iter)->car->_path.lockTracks[track] = true;
		}
}

void AISystem::ComputeTracks(float deltaTime)
{
	struct Link;
	typedef lsl::List<Link*> Chain;
	struct Link
	{
		AICar* car;
		float dirDist;
		Chain* chain;

		Link(AICar* mCar, float mDirDist, Chain* mChain): car(mCar), dirDist(mDirDist), chain(mChain) {}
		
		bool operator==(const Link& link) const
		{
			return car == link.car;
		}
		bool operator<(const Link& tie) const
		{
			return dirDist < tie.dirDist;
		}
	};
	typedef lsl::List<Link*> CarStore;
	typedef lsl::Container<Link*> CarList;
	typedef lsl::List<Chain*> ChainList;

	CarStore carStore;
	CarList carList;
	ChainList chainList;
	unsigned cTrackCnt = _race->GetMap()->GetTrace().cTrackCnt;
	if (_aiDebug)
		_aiDebug->chainList.clear();

	//Заполняем список
	for (PlayerList::const_iterator iter = _playerList.begin(); iter != _playerList.end(); ++iter)
	{
		if ((*iter)->GetCar() && (*iter)->GetCar()->GetCar().mapObj && (*iter)->GetCar()->GetCar().curTile)
		{
			const AICar::CarState& car = (*iter)->GetCar()->GetCar();

			D3DXVECTOR3 dirLine;
			Line2FromDir(car.curTile->GetTile().GetDir(), car.curTile->GetPos2(), dirLine);
			Link* link = new Link((*iter)->GetCar(), Line2DistToPoint(dirLine, car.pos), 0);

			carStore.push_back(link);
			carList.Insert(link);
		}
	}

	//
	for (CarList::Position pos = carList.First(); Link** iter = carList.Current(pos); carList.Next(pos))
	{
		//опорный элемент
		Link* link = *iter;
		AICar* aiCar = link->car;
		Chain* chain = link->chain;
		const AICar::CarState& car = aiCar->GetCar();
		WayNode* curNode = car.curNode;

		//по умолчанию дорожки свободные
		for (unsigned i = 0; i < cTrackCnt; ++i)
		{
			aiCar->_path.freeTracks[i] = true;
			aiCar->_path.lockTracks[i] = false;
		}
		//
		CarList::Position tPos = carList.First();
		//поиск целевых элементов цепи
		for (CarList::Position tPos = carList.First(); Link** iter = carList.Current(tPos); carList.Next(tPos))
		{
			//целевой элемент
			Link* tLink = *iter;
			AICar* tAICar = tLink->car;
			const AICar::CarState& tCar = tAICar->GetCar();
			Chain* tChain = tLink->chain;
			
			//1. Совпадающие элементы
			//2. Элементы уже входят в одну и ту же цепь
			if (link == tLink || (tChain && tChain == chain))
				continue;

			float normDist = Line2DistToPoint(car.trackNormLine, tCar.pos);
			bool lowRange = normDist + tCar.radius + car.radius > 0;
			bool heighRange = normDist - tCar.radius - car.radius < 0;

			if (curNode->GetTile().IsContains(tCar.pos3, false) && lowRange && heighRange)
			{
				//цепи нет, следовательно создаем и обрабаываем опорный элемент
				if (!chain)
				{
					if (tChain)
						chain = tChain;
					else
					{
						chain = new Chain();
						chainList.push_back(chain);			
					}

					LSL_ASSERT(link->chain == 0);

					//вставляем опорный элемент
					link->chain = chain;
					chain->push_back(link);
				}

				//обрабатываем целевой элемент
				if (chain != tChain)
					//цель в другой цепи, необходимо объеденить её с chain
					if (tChain)
					{
						for (Chain::iterator iter = tChain->begin(); iter != tChain->end(); ++iter)		
							(*iter)->chain = chain;
						chain->insert(chain->end(), tChain->begin(), tChain->end());
						chainList.Remove(tChain);
						delete tChain;
					}
					//добавление нового целевого элемента в цепь
					else
					{	
						LSL_ASSERT(tLink->chain == 0);

						tLink->chain = chain;
						chain->push_back(tLink);
					}
			}
		}

		//удаляем опорный элемент, который не может встречаться дважды в одной цепи
		carList.Remove(pos);
	}

	//Обрабатываем цепи
	for (ChainList::iterator iter = chainList.begin(); iter != chainList.end(); ++iter)
	{
		Chain* tChain = *iter;
		//LSL_ASSERT(!tChain->empty() && tChain->size() <= cTrackCnt);
		LSL_ASSERT(!tChain->empty());

		
		struct Pred: public std::binary_function<Link*, Link*, bool>
		{
			bool operator()(Link* link1, Link* link2)
			{
				return link1->dirDist < link2->dirDist;
			}
		};
		//сортируем по убыванию dirDist, т.е. слева на право в направлении дорожек
		tChain->sort(Pred());

		AIDebug::Chain* debChain = 0;
		if (_aiDebug)
		{
			_aiDebug->chainList.push_back(AIDebug::Chain());
			debChain = &_aiDebug->chainList.back();
		}

		//общее количество требуемых дорожек
		unsigned tCount = tChain->size();
		unsigned sTrack = 0;
		unsigned i = 0;
		//
		AICar* lastAICar = 0;
		unsigned lastSTrack = 0;
		for (Chain::iterator tIter = tChain->begin(); tIter != tChain->end(); ++tIter, ++sTrack, ++i)
		{
			//
			AICar* tAICar = (*tIter)->car;
			AICar::CarState& tCar = const_cast<AICar::CarState&>(tAICar->GetCar());

			//максимально допустимая дорожка с учетом требуемых для оставшихся целей мест			
			sTrack = ClampValue(tCar.track, std::min(sTrack, cTrackCnt - 1), cTrackCnt - (std::min(tCount, cTrackCnt) - i % cTrackCnt));

			//
			LockChainTrack(tChain->begin(), tChain->end(), tAICar, sTrack);
			//Если соседние машины на одинаковых треках, необходимо заблокировать левый трек для правой машины чтобы избежать неопределнности при вычислении маршрута
			if (lastAICar && lastAICar->GetCar().track == tCar.track && lastSTrack > 0)
			{
				tAICar->_path.freeTracks[lastSTrack - 1] = false;
				tAICar->_path.lockTracks[lastSTrack - 1] = true;
			}
			lastAICar = tAICar;
			lastSTrack = sTrack;

			if (_aiDebug)
				debChain->push_back(tCar.track);
		}
	}

	for (ChainList::iterator iter = chainList.begin(); iter != chainList.end(); ++iter)
		delete *iter;
	for (CarStore::iterator iter = carStore.begin(); iter != carStore.end(); ++iter)
		delete *iter;
}

void AISystem::OnProgress(float deltaTime)
{
	ComputeTracks(deltaTime);
	for (PlayerList::iterator iter = _playerList.begin(); iter != _playerList.end(); ++iter)
		(*iter)->OnProgress(deltaTime);

	if (_aiDebug)
		_aiDebug->OnProgress(deltaTime);
}

AIPlayer* AISystem::AddPlayer(Player* player)
{
	_playerList.push_back(new AIPlayer(player));
	return _playerList.back();
}

void AISystem::DelPlayer(AIPlayer* value)
{
	_playerList.Remove(value);
	delete value;
}

void AISystem::ClearPlayerList()
{
	for (PlayerList::iterator iter = _playerList.begin(); iter != _playerList.end(); ++iter)
		delete (*iter);
	_playerList.clear();
}

AIPlayer* AISystem::FindAIPlayer(Player* player)
{
	for (PlayerList::iterator iter = _playerList.begin(); iter != _playerList.end(); ++iter)
		if ((*iter)->GetPlayer() == player)
			return *iter;

	return 0;
}

const AISystem::PlayerList& AISystem::GetPlayerList() const
{
	return _playerList;
}

void AISystem::CreateDebug(AIPlayer* player)
{
	FreeDebug();

	_aiDebug = new AIDebug(this, player);
}

void AISystem::FreeDebug()
{
	lsl::SafeDelete(_aiDebug);
}




AIDebug::AIDebug(AISystem* ai, AIPlayer* aiPlayer): _ai(ai), _aiPlayer(aiPlayer)
{
	_aiPlayer->AddRef();

	_traceGfx = new TraceGfx(&aiPlayer->GetWorld()->GetMap()->GetTrace());
	GetWorld()->GetGraph()->InsertScNode(_traceGfx);

	_grActor = new GrActor(this);
	GetWorld()->GetGraph()->InsertScNode(_grActor);

	_control = new Control(this);
	GetWorld()->GetControl()->InsertEvent(_control);	

	if (aiPlayer->_car)
		aiPlayer->_car->_enbAI = false;
	_traceGfx->SetVisible(false);
}

AIDebug::~AIDebug()
{
	GetWorld()->GetControl()->RemoveEvent(_control);
	delete _control;

	GetWorld()->GetGraph()->RemoveScNode(_grActor);
	delete _grActor;

	GetWorld()->GetGraph()->RemoveScNode(_traceGfx);
	delete _traceGfx;

	_aiPlayer->Release();
}

AIDebug::GrActor::GrActor(AIDebug* debug): _debug(debug)
{
	HDC hDC;
    //HFONT hFont;
    int nHeight;
    int nPointSize = 9;
    //char strFontName[] = "Arial";
    hDC = GetDC(NULL);
    nHeight = -( MulDiv( nPointSize, GetDeviceCaps(hDC, LOGPIXELSY), 72 ) );
    ReleaseDC( NULL, hDC );
    // Create a font for statistics and help output
	HRESULT hr = D3DXCreateFont(_debug->GetWorld()->GetGraph()->GetEngine().GetDriver().GetDevice(), nHeight, 0, FW_BOLD, 0, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("Arial"), &_font);	
}

AIDebug::GrActor::~GrActor()
{
	_font->Release();
}

void AIDebug::GrActor::DoRender(graph::Engine& engine)
{
	if (!_debug->_aiPlayer->_car)
		return;




	AISystem* ai = _debug->_ai;

	engine.GetContext().SetRenderState(graph::rsZWriteEnable, false);
	engine.GetContext().SetRenderState(graph::rsZEnable, false);
	engine.GetContext().SetRenderState(graph::rsLighting, false);

	/*for (AISystem::PlayerList::const_iterator iter = ai->GetPlayerList().begin(); iter != ai->GetPlayerList().end(); ++iter)
	{
		AIPlayer* aiPlayer = (*iter);
		MapObj* car = aiPlayer->GetPlayer()->GetCar().mapObj;
		RockCar* gameObj = _debug->_aiPlayer->GetPlayer()->GetCar().gameObj;
		
		if (car)
		{
			D3DXVECTOR2 moveDir = aiPlayer->GetCar()->_path.moveDir;

			res::VertexPD lines[2];
			D3DXVECTOR3 worldPos = car->GetGameObj().GetWorldPos();
			D3DXVECTOR3 newPos = worldPos + D3DXVECTOR3(moveDir.x, moveDir.y, 0.0f) * aiPlayer->GetCar()->_path.dirArea;
			lines[0].pos = worldPos;
			lines[1].pos = newPos;
			lines[0].diffuse = lines[1].diffuse = clrWhite;
			engine.GetDriver().GetDevice()->SetFVF(res::VertexPD::fvf);
			engine.GetDriver().GetDevice()->DrawPrimitiveUP(D3DPT_LINELIST, 1, lines, sizeof(res::VertexPD));
		}
	}*/

	if (_debug->_aiPlayer->GetPlayer()->GetCar().mapObj)
	{
		MapObj* car = _debug->_aiPlayer->GetPlayer()->GetCar().mapObj;
		RockCar* gameObj = _debug->_aiPlayer->GetPlayer()->GetCar().gameObj;
		WayNode* nextNode = _debug->_aiPlayer->_car->_path.nextTile;

		if (car && nextNode)
		{
			D3DXVECTOR2 moveDir = _debug->_aiPlayer->_car->_path.moveDir;			

			float dist = Line2DistToPoint(nextNode->GetTile().GetEdgeLine(), _debug->_aiPlayer->_car->GetCar().pos);
			D3DXVECTOR2 dir = nextNode->GetTile().GetEdgeNorm();

			res::VertexPD lines[2];
			D3DXVECTOR3 worldPos = car->GetGameObj().GetWorldPos();
			D3DXVECTOR3 newPos = worldPos + (-D3DXVECTOR3(dir.x, dir.y, 0.0f)) * dist;
			lines[0].pos = worldPos;
			lines[1].pos = newPos;
			lines[0].diffuse = lines[1].diffuse = clrRed;
			engine.GetDriver().GetDevice()->SetFVF(res::VertexPD::fvf);
			engine.GetDriver().GetDevice()->DrawPrimitiveUP(D3DPT_LINELIST, 1, lines, sizeof(res::VertexPD));
		}

		/*for (GameCar::ContactList::iterator iter = gameObj->contactList.begin(); iter != gameObj->contactList.end(); ++iter)
		{
			res::VertexPD lines[2];
			game::GameCar::PxContactCallbackData contData = iter->data;
			NxTriangle triData = iter->tri;

			NxVec3 pos0 = contData.localpos0;
			NxQuat rot0 = contData.localorientation0;
			NxMat33 mat0(rot0);
			if (iter->shape0->getActor().isDynamic())
			{
				NxMat34 bodyMat = iter->shape0->getActor().getCMassGlobalPose();				
				bodyMat.multiply(pos0, pos0);				
				mat0.multiply(bodyMat.M, mat0);
			}
			
			D3DXVECTOR3 error(contData.error.get());
			D3DXVECTOR3 errorNorm;
			D3DXVec3Normalize(&errorNorm, &error);
			//Отрисовка error вектора
			NxVec3 nxErrNorm = error;
			//nxErrNorm.normalize();
			gameObj->GetPxActor().GetNxActor()->getCMassGlobalPose().M.multiply(nxErrNorm, nxErrNorm);
			errorNorm = D3DXVECTOR3(nxErrNorm.get());
			lines[0].pos = D3DXVECTOR3(pos0.get());
			lines[0].diffuse = clrBlack;
			lines[1].pos = D3DXVECTOR3(pos0.get()) + errorNorm * 50.0f;
			lines[1].diffuse = clrBlack;
			engine.GetDriver().GetDevice()->SetFVF(res::VertexPD::fvf);
			engine.GetDriver().GetDevice()->DrawPrimitiveUP(D3DPT_LINELIST, 1, lines, sizeof(res::VertexPD));

			//Отрисовка X оси базиса
			lines[0].pos = D3DXVECTOR3(pos0.get());
			lines[1].pos = D3DXVECTOR3((pos0 + mat0.getColumn(0) * 5.0f).get());
			lines[0].diffuse = lines[1].diffuse = clrYellow;
			engine.GetDriver().GetDevice()->SetFVF(res::VertexPD::fvf);
			engine.GetDriver().GetDevice()->DrawPrimitiveUP(D3DPT_LINELIST, 1, lines, sizeof(res::VertexPD));
			//Отрисовка Y оси базиса
			lines[0].pos = D3DXVECTOR3(pos0.get());
			lines[1].pos = D3DXVECTOR3((pos0 + mat0.getColumn(1) * 5.0f).get());
			lines[0].diffuse = lines[1].diffuse = clrGreen;
			engine.GetDriver().GetDevice()->SetFVF(res::VertexPD::fvf);
			engine.GetDriver().GetDevice()->DrawPrimitiveUP(D3DPT_LINELIST, 1, lines, sizeof(res::VertexPD));
			//Отрисовка Z оси базиса
			lines[0].pos = D3DXVECTOR3(pos0.get());
			lines[1].pos = D3DXVECTOR3((pos0 + mat0.getColumn(2) * 5.0f).get());
			lines[0].diffuse = lines[1].diffuse = clrRed;
			engine.GetDriver().GetDevice()->SetFVF(res::VertexPD::fvf);
			engine.GetDriver().GetDevice()->DrawPrimitiveUP(D3DPT_LINELIST, 1, lines, sizeof(res::VertexPD));

			//Отрисовка треугольника
			NxVec3 myNorm;
			triData.normal(myNorm);
			res::VertexPD triLines[4];
			triLines[0].pos = D3DXVECTOR3(triData.verts[0].get());
			triLines[1].pos = D3DXVECTOR3(triData.verts[1].get());
			triLines[2].pos = D3DXVECTOR3(triData.verts[2].get());
			triLines[0].diffuse = triLines[1].diffuse = triLines[2].diffuse = abs(myNorm.x) > 0.5f ? clrRed : clrBlue;
			triLines[3] = triLines[0];
			engine.GetDriver().GetDevice()->SetFVF(res::VertexPD::fvf);
			engine.GetDriver().GetDevice()->DrawPrimitiveUP(D3DPT_LINESTRIP, 3, triLines, sizeof(res::VertexPD));
			//break;
		}
		gameObj->contactList.clear();
		gameObj->GetGrActor().showBB = true;
		gameObj->GetGrActor().colorBB = clrWhite;*/
	}
	engine.GetContext().RestoreRenderState(graph::rsZWriteEnable);
	engine.GetContext().RestoreRenderState(graph::rsZEnable);
	engine.GetContext().RestoreRenderState(graph::rsLighting);
	 



	std::stringstream sstream;
	std::stringstream sstream2;
	
	sstream << "Chains \n";
	unsigned i = 0;
	for (ChainList::iterator iter = _debug->chainList.begin(); iter != _debug->chainList.end(); ++iter, ++i)
	{
		sstream << "Chain " << i << " = ";

		const Chain& tChain = *iter;
		for (Chain::const_iterator tIter = tChain.begin(); tIter != tChain.end(); ++tIter)
		{
			sstream << *tIter << " ";
		}
		sstream << '\n';
	}







	static bool speed100 = false;
	static bool speed150 = false;
	static bool speed200 = false;
	static float speed100Time = 0;
	static float speed150Time = 0;
	static float speed200Time = 0;

	AICar* aiCar = _debug->_aiPlayer->GetCar();
	const AICar::CarState& car = aiCar->GetCar();

	if (!car.gameObj)
		return;

	game::RockCar* gameObj = car.gameObj;
	NxActor* nxActor = gameObj->GetPxActor().GetNxActor();

	if (car.speed <= 0)
	{
		speed100 = false;
		speed100Time = 0;
	}
	else if (car.speed >= 100 * 1000 / 3600.0f)
		speed100 = true;
	else if (!speed100)
		speed100Time += engine.GetDt();

	if (car.speed <= 0)
	{
		speed150 = false;
		speed150Time = 0;
	}
	else if (car.speed >= 150 * 1000 / 3600.0f)
		speed150 = true;
	else if (!speed150)
		speed150Time += engine.GetDt();

	if (car.speed <= 0)
	{
		speed200 = false;
		speed200Time = 0;
	}
	else if (car.speed >= 200 * 1000 / 3600.0f)
		speed200 = true;
	else if (!speed200)
		speed200Time += engine.GetDt();

	sstream << "RPM = " << gameObj->GetRPM() << '\n';
	sstream << "Gear = " << gameObj->GetCurGear() << '\n';	
	sstream << "Speed = " << Round(car.speed * 3600.0f / 1000.0f * 100.0f) / 100.0f << '\n';
	sstream << "AxleSpeed = " << Round(gameObj->GetDrivenWheelSpeed() * 3600.0f / 1000.0f * 100.0f) / 100.0f << '\n';
	sstream << "Break = " << _debug->_aiPlayer->GetCar()->_path._break << '\n';
	sstream << "Speed100 = " << Round(speed100Time * 100.0f) / 100.0f << '\n';
	sstream << "Speed150 = " << Round(speed150Time * 100.0f) / 100.0f << '\n';
	sstream << "Speed200 = " << Round(speed200Time * 100.0f) / 100.0f << '\n';
	if (car.curTile)
	{	
		WayNode* curTile = _debug->_aiPlayer->GetCar()->GetCar().curTile;
		sstream << "Finish = " << curTile->GetTile().GetFinishDist()/curTile->GetPath()->GetLength() << '\n';		
	}
	sstream << '\n';

	static float dbgDumpTime = 0;
	static lsl::string strNormReaction;
	static float minNormReaction = 0;
	static float maxNormReaction = 0;

	float normReaction = gameObj->GetWheels().front()._nReac;
	if (minNormReaction > normReaction)
		minNormReaction = normReaction;
	if (maxNormReaction < normReaction)
		maxNormReaction = normReaction;
	sstream << strNormReaction << '\n';

	if (dbgDumpTime > 0.5f)
	{
		dbgDumpTime = 0.0f;

		std::stringstream sstream;
		sstream << "nReac=" << Round(normReaction * 100.0f) / 100.0f << "\nnReacMin=" << Round(minNormReaction * 100.0f) / 100.0f << "\nnReacMax=" << Round(maxNormReaction * 100.0f) / 100.0f;
		strNormReaction = sstream.str();

		minNormReaction = 999999.0f;
		maxNormReaction = 0;
	}
	dbgDumpTime += engine.GetDt();	

	game::CarWheel* wheel = &gameObj->GetWheels().front();
	px::WheelShape* pxWheel =  wheel->GetShape();
	NxTireFunctionDesc longFunc = pxWheel->GetLongitudalTireForceFunction();
	NxTireFunctionDesc latFunc = pxWheel->GetLateralTireForceFunction();
	NxSpringDesc suspension = pxWheel->GetSuspension();
	float suspensionTravel = pxWheel->GetSuspensionTravel();
	float mass = nxActor->getMass();
	NxVec3 cMassPos = nxActor->getCMassLocalPosition();
	CarMotorDesc motor = gameObj->GetMotorDesc();
	float steerSpeed = gameObj->GetSteerSpeed();
	float steerRot = gameObj->GetSteerRot();
	float kSteer = car.kSteerControl;
	float maxSpeed = gameObj->GetMaxSpeed();
	float tireSpring = gameObj->GetTireSpring();

	static int _numParam = 0;
	static int numPage = 0;
	sstream2 << "numParam = " << _numParam << '\n';
	sstream2 << "numPage = " << numPage << '\n';
	sstream2 << "\n";
	sstream2 << "long tire func" << '\n';
	sstream2 << "0. extremumSlip = " << longFunc.extremumSlip << '\n';
	sstream2 << "1. extremumValue = " << longFunc.extremumValue << '\n';
	sstream2 << "2. asymptoteSlip = " << longFunc.asymptoteSlip << '\n';
	sstream2 << "3. asymptoteValue = " << longFunc.asymptoteValue << '\n';
	sstream2 << '\n';
	sstream2 << "lat tire func" << '\n';
	sstream2 << "4. extremumSlip = " << latFunc.extremumSlip << '\n';
	sstream2 << "5. extremumValue = " << latFunc.extremumValue << '\n';
	sstream2 << "6. asymptoteSlip = " << latFunc.asymptoteSlip << '\n';
	sstream2 << "7. asymptoteValue = " << latFunc.asymptoteValue << '\n';
	sstream2 << "8. maxSpeed = " << maxSpeed << '\n';
	sstream2 << "9. tireSpring = " << tireSpring << '\n';
	sstream2 << '\n';
	sstream2 << "suspension" << '\n';
	//Упругость, тем больше значение тем система жестче
	sstream2 << "0. spring = " << suspension.spring << '\n';
	//Затухание колебаний
	sstream2 << "1. damper = " << suspension.damper << '\n';
	//Точка где сила равняется нулю
	sstream2 << "2. targetValue = " << suspension.targetValue << '\n';
	//Высота подвески
	sstream2 << "3. suspensionTravel = " << suspensionTravel << '\n';
	//масса
	sstream2 << "4. mass = " << mass << '\n';
	//центр тяжести
	sstream2 << "5. cMass = (" << cMassPos.x << ", " << cMassPos.y << ", " << cMassPos.z << ")" << '\n';	
	//motor
	sstream2 << "6. torque = " << motor.maxTorque << '\n';		
	//steerSpeed
	sstream2 << "7. steerSpeed = " << steerSpeed << '\n';
	//steerRot
	sstream2 << "8. steerRot = " << steerRot << '\n';
	//kSteer
	sstream2 << "9. kSteer = " << kSteer << '\n';	
	sstream2 << '\n';
	
	
	//
	sstream2 << "slip wheels" << '\n';
	int ind = 0;
	for (game::CarWheels::const_iterator iter = gameObj->GetWheels().begin(); iter != gameObj->GetWheels().end(); ++iter, ++ind)
		sstream2 << "wheel" << ind << " lat=" << Round((*iter)->GetLatSlip() * 10.0f)/10.0f <<  " long=" << Round((*iter)->GetLongSlip() * 10.0f)/10.0f << '\n';
	sstream2 << '\n';	

	RECT destRect;
	RECT destRect2;
	SetRect(&destRect, 5, 200, 0, 0);	
	SetRect(&destRect2, 150, 160, 0, 0);	
	_font->OnResetDevice();
	_font->DrawText(0 , sstream.str().c_str(), -1, &destRect, DT_NOCLIP, D3DXCOLOR(1.0f, 0.0f, 0.0f, 1.0f));
	_font->DrawText(0 , sstream2.str().c_str(), -1, &destRect2, DT_NOCLIP, D3DXCOLOR(1.0f, 0.0f, 0.0f, 1.0f));
	_font->OnLostDevice();

	const int cParamKeysEnd = 10;
	const int paramKeys[cParamKeysEnd] = {VK_NUMPAD0, VK_NUMPAD1, VK_NUMPAD2, VK_NUMPAD3, VK_NUMPAD4, VK_NUMPAD5, VK_NUMPAD6, VK_NUMPAD7, VK_NUMPAD8, VK_NUMPAD9};

	for (int i = 0; i < cParamKeysEnd; ++i)
		if (GetAsyncKeyState(paramKeys[i]))
		{
			_numParam = i;
			break;
		}
	if (GetAsyncKeyState(VK_PRIOR))
		numPage = std::min(numPage + 1, 1);
	if (GetAsyncKeyState(VK_NEXT))
		numPage = std::max(numPage - 1, 0);



	int addVal = 0;
	if (GetAsyncKeyState(VK_ADD))
		addVal = 1;
	if (GetAsyncKeyState(VK_SUBTRACT))
		addVal = -1;

	if (numPage == 0)
	switch (_numParam)	
	{
	case 0:
		longFunc.extremumSlip += 0.1f * engine.GetDt() * addVal;
		break;
	case 1:
		longFunc.extremumValue += 1.0f * engine.GetDt() * addVal;
		break;
	case 2:
		longFunc.asymptoteSlip += 1.0f * engine.GetDt() * addVal;
		break;
	case 3:
		longFunc.asymptoteValue += 1.0f * engine.GetDt() * addVal;
		break;
	case 4:
		latFunc.extremumSlip += 0.1f * engine.GetDt() * addVal;
		break;
	case 5:
		latFunc.extremumValue += 1.0f * engine.GetDt() * addVal;
		break;
	case 6:
		latFunc.asymptoteSlip += 1.0f * engine.GetDt() * addVal;
		break;
	case 7:
		latFunc.asymptoteValue += 1.0f * engine.GetDt() * addVal;
		break;
	case 8:
		maxSpeed += 5.0f * engine.GetDt() * addVal;
		break;
	case 9:
		tireSpring += 2.0f * engine.GetDt() * addVal;
		break;
	}
	if (numPage == 1)
	switch (_numParam)	
	{
	case 0:
		suspension.spring += 100000.0f * engine.GetDt() * addVal;
		break;
	case 1:
		suspension.damper += 1000.0f * engine.GetDt() * addVal;
		break;
	case 2:
		suspension.targetValue += 1.0f * engine.GetDt() * addVal;
		break;
	case 3:
		suspensionTravel += 0.1f * engine.GetDt() * addVal;
		break;
	case 4:
		mass += 1000.0f * engine.GetDt() * addVal;
		break;
	case 5:
		cMassPos.z += 0.1f * engine.GetDt() * addVal;
		break;
	case 6:
		motor.maxTorque += 1000.0f * engine.GetDt() * addVal;
		break;
	case 7:
		steerSpeed += 1.0f * engine.GetDt() * addVal;
		break;
	case 8:
		steerRot += 1.0f * engine.GetDt() * addVal;
		break;
	case 9:
		kSteer += 0.25f * engine.GetDt() * addVal;
		break;
	}

	for (game::CarWheels::const_iterator iter = gameObj->GetWheels().begin(); iter != gameObj->GetWheels().end(); ++iter)
	{
		game::CarWheel* wheel = *iter;

		wheel->GetShape()->SetLongitudalTireForceFunction(longFunc);
		wheel->GetShape()->SetLateralTireForceFunction(latFunc);
		wheel->GetShape()->SetSuspension(suspension);
		wheel->GetShape()->SetSuspensionTravel(suspensionTravel);
	}
	nxActor->setMass(mass);
	nxActor->setCMassOffsetLocalPosition(cMassPos);
	gameObj->SetMotorDesc(motor);
	gameObj->SetSteerSpeed(steerSpeed);
	gameObj->SetSteerRot(steerRot);
	gameObj->SetMaxSpeed(maxSpeed);
	gameObj->SetTireSpring(tireSpring);
	const_cast<Player::CarState&>(car).kSteerControl = kSteer;
}

AIDebug::Control::Control(AIDebug* debug): _debug(debug)
{
}

bool AIDebug::Control::OnHandleInput(const InputMessage& msg)
{
#ifndef _RETAIL
	if (msg.action == gaDebug6 && msg.state == ksDown && !msg.repeat)
	{
		_debug->_traceGfx->SetVisible(!_debug->_traceGfx->GetVisible());
		return true;
	}

	if (msg.action == gaDebug7 && msg.state == ksDown && !msg.repeat)
	{
		_debug->_aiPlayer->_car->_enbAI = !_debug->_aiPlayer->_car->_enbAI;
		return true;
	}
#endif

	return false;
}

void AIDebug::OnProgress(float deltaTime)
{
	_traceGfx->SetSelNode(_aiPlayer->GetPlayer()->GetCar().curNode);	
}

World* AIDebug::GetWorld()
{
	return _aiPlayer->GetWorld();
}

AIPlayer* AIDebug::GetAI()
{
	return _aiPlayer;
}

}

}