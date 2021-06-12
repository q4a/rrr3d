#include "stdafx.h"
#include "game\World.h"

#include "game\CameraManager.h"

namespace r3d
{

namespace game
{

CameraManager::CameraManager(World* world): _world(world), _player(0), _target(0, 0, 0, 5.0f), _light(0), _style(cStyleEnd), _near(1.0f), _far(100.0f), _clampAngle(0.0f, 0.0f, 25.0f * D3DX_PI/180, 80.0f * D3DX_PI/180), _angleSpeed(D3DX_PI/48, 0, 0), _stableAngle(75.0f * D3DX_PI/180, 0, 0), _flyCurTime(-1)
{
	_camera = new graph::Camera();
	_camera->SetPos(D3DXVECTOR3(50.0f, 50.0f, 50.0f));
	_camera->SetDir(-IdentityVector);	
	_lastFreePos = _camera->GetPos();
	_lastFreeRot = _camera->GetRot();

	_world->GetGraph()->SetCamera(_camera);

	_control = new Control(this);
	_world->GetControl()->InsertEvent(_control);
}

CameraManager::~CameraManager()
{
	SetLight(0);
	SetPlayer(0);

	_world->GetControl()->RemoveEvent(_control);
	delete _control;
	_world->GetGraph()->SetCamera(0);
	delete _camera;	
}

void CameraManager::OrthoCullOffset()
{
	/*if (_style == csIsometric)
	{
		float minZ = 0;
		float maxZ = 0;
		
		if (_world->GetGraph()->ComputeViewZBound(_camera->GetContextInfo(), minZ, maxZ))
		{
			float offZ = std::min(minZ - _near, 0.0f);
			float maxNear = _near;
			float maxFar = -offZ + maxZ;

			//_camera->SetPos(_camera->GetPos() + _camera->GetDir() * offZ);
			//_camera->SetNear(maxNear);
			//_camera->SetFar(maxFar);
		}
	}*/
}

void CameraManager::SyncLight()
{
	if (_style == csLights && _light)
	{
		_light->GetSource()->SetPos(_camera->GetPos());
		_light->GetSource()->SetRot(_camera->GetRot());
	}
}

CameraManager::Control::Control(CameraManager* manager): _manager(manager), _staticVec1(NullVector), _staticVec2(NullVector), _staticVec3(NullVector), _staticFloat1(0)
{	
}

bool CameraManager::Control::OnMouseMoveEvent(const MouseMove& mMove)
{
	if (_manager->_style == csLights || _manager->_style == csFreeView)
	{
		graph::Camera* camera = _manager->_camera;
		D3DXQUATERNION rot = camera->GetRot();
		D3DXVECTOR3 right = camera->GetRight();

		if (_manager->_style == csLights && _manager->_light)
		{
			rot = _manager->_light->GetSource()->GetRot();
			right = _manager->_light->GetSource()->GetRight();
		}

		if (mMove.click.key == lsl::mkRight && mMove.click.state == lsl::ksDown)
		{
			D3DXQUATERNION quatRight, quatZ;
			D3DXQuaternionRotationAxis(&quatZ, &ZVector, 0.005f * (-mMove.dtCoord.x));
			D3DXQuaternionRotationAxis(&quatRight, &right, 0.005f * mMove.dtCoord.y);
			rot = rot * quatRight * quatZ;

			camera->SetRot(rot);
			_manager->SyncLight();

			return true;
		}
	}
	
	return false;
}

bool CameraManager::Control::OnHandleInput(const InputMessage& msg)
{
	if (msg.action == gaViewSwitch && msg.state == ksDown)
	{
		if (_manager->_world->GetGame() && _manager->_world->GetGame()->IsStartRace() && _manager->_world->GetGame()->GetRace()->GetTournament().GetCurPlanetIndex() > Race::cTournamentPlanetCount - 1)
		{
			return true;
		}

#if _DEBUG
		int styleMap[cStyleEnd] = {csThirdPerson, csIsometric, csLights, csIsoView, csFreeView, csFreeView, csFreeView};
#else
		int styleMap[cStyleEnd] = {cStyleEnd, csIsometric, csThirdPerson, cStyleEnd, cStyleEnd, cStyleEnd, cStyleEnd};

		if (_manager->_world->GetEdit())
		{
			for (int i = 0; i < cStyleEnd; ++i)
				styleMap[i] = cStyleEnd;
			styleMap[0] = csIsoView;
			styleMap[3] = csFreeView;
			styleMap[4] = csLights;			
		}
#endif

		if (_manager->_style != cStyleEnd && 
			(_manager->_world->GetEdit() ||
			_manager->_world->GetGame() && _manager->_world->GetGame()->IsStartRace()))
		{
			Style camStyle = (Style)styleMap[_manager->_style];
			if (camStyle != cStyleEnd)
				_manager->ChangeStyle(camStyle);
			return true;
		}
	}

	return false;
}

void CameraManager::Control::OnInputFrame(float deltaTime)
{
	graph::Camera* camera = _manager->_camera;

	if (_manager->InFly())
	{
		_manager->_flyCurTime += deltaTime;

		float alpha = lsl::ClampValue(_manager->_flyCurTime, 0.0f, _manager->_flyTime);
		alpha = _flyAlpha = _flyAlpha + std::max((alpha - _flyAlpha) * deltaTime * 4.0f, 0.001f);
		alpha = lsl::ClampValue(alpha / _manager->_flyTime, 0.0f, 1.0f);

		D3DXVECTOR3 pos;
		D3DXVec3Lerp(&pos, &_manager->_flySPos, &_manager->_flyPos, alpha);

		D3DXQUATERNION rot;
		D3DXQuaternionSlerp(&rot, &_manager->_flySRot, &_manager->_flyRot, alpha);

		camera->SetPos(pos);
		camera->SetRot(rot);

		if (alpha == 1.0f)
		{
			_manager->_flyCurTime = -1.0f;
			FlyCompleted();
		}
		return;
	}

	if (_manager->_style == csBlock)
		return;

	static float cIsoAngY = 15.5f * D3DX_PI / 180;
	const float cIsoAngZ = 45.0f * D3DX_PI / 180;
	const float targDist = 20.0f;
	static float cIsoBorder = 5.0f;

/*#ifndef _RETAIL
	{
		static int tmp = 0;

		if (_manager->_world->GetControl()->GetAsyncKey('Y') == akLastDown)
		{
			if (tmp != 1)
			{
				tmp = 1;
				cIsoAngY = 16.0f * D3DX_PI / 180;
				cIsoBorder = 5.5f;
			}
			else
			{
				tmp = 0;
				cIsoAngY = 15.5f * D3DX_PI / 180;
				cIsoBorder = 5.0f;
			}
		}

		if (_manager->_world->GetControl()->GetAsyncKey('T') == akLastDown)
		{
			++tmp;
			if (tmp > 3)
				tmp = 0;

			if (tmp == 3)
			{
				cIsoAngY = atan(0.4f);
				cIsoBorder = 7.5f;
			}
			else if (tmp == 2)
			{
				cIsoAngY = 16.5f * D3DX_PI / 180;
				cIsoBorder = 5.5f;
			}
			else if (tmp == 1)
			{
				cIsoAngY = 16.0f * D3DX_PI / 180;
				cIsoBorder = 5.5f;
			}
			else
			{
				cIsoAngY = 15.5f * D3DX_PI / 180;
				cIsoBorder = 5.0f;
			}
		}
	}
#endif*/

	D3DXQUATERNION isoRotY, isoRotZ;
	D3DXQuaternionRotationAxis(&isoRotY, &YVector, cIsoAngY);
	D3DXQuaternionRotationAxis(&isoRotZ, &ZVector, cIsoAngZ);
	//
	D3DXQUATERNION cIsoRot = isoRotY * isoRotZ;

	const float speedMove = 20.0f * deltaTime;
	const float angleSpeed = D3DX_PI / 2.0f * deltaTime;
	const float retAngleSpeed = angleSpeed * 2.0f;
	const float maxAngle = D3DX_PI / 6;
	const D3DXVECTOR3 cCamTargetOff(-4.6f, 0.0f, 2.4f);

	ControlManager* control = _manager->_world->GetControl();	
	
	D3DXVECTOR3 pos = camera->GetPos();
	D3DXQUATERNION rot = camera->GetRot();
	D3DXVECTOR3 dir = camera->GetDir();	
	D3DXVECTOR3 right = camera->GetRight();	

	D3DXVECTOR3 targetPos = NullVector;
	D3DXVECTOR3 targetDir = XVector;
	D3DXQUATERNION targetRot = NullQuaternion;
	float targetSize = 0.0f;
	D3DXVECTOR3 targetVel = NullVector;
	float targetDrivenSpeed = 0.0f;
	
	Player* player = _manager->_player;	
	if (player)
	{
		targetSize = player->GetCar().size;

		if (player->GetCar().grActor)
		{
			targetPos = player->GetCar().grActor->GetWorldPos();
			targetDir = player->GetCar().grActor->GetWorldDir();
			targetRot = player->GetCar().grActor->GetWorldRot();
			targetVel = player->GetCar().gameObj->GetPxVelocityLerp();
			targetDrivenSpeed = player->GetCar().gameObj->GetDrivenWheelSpeed();

			//принужденное движение назад, отвечат за блокировку камеры поворачивающейся наза при заднем ходе. Если закоментировать, блокирвока уберется, необходимо брать с неким запасом из за погрешностей синхронизации
			if (targetDrivenSpeed < 0.1f)
			{
				//отсекаем отрицательную составляющую по направляющей локальной оси x
				player->GetCar().grActor->WorldToLocalNorm(targetVel, targetVel);
				targetVel.x = std::max(targetVel.x, 0.0f);
				player->GetCar().grActor->LocalToWorldNorm(targetVel, targetVel);
			}
		}
		else
		{
			targetPos = player->GetCar().pos3;
			targetDir = player->GetCar().dir3;
			targetRot = player->GetCar().rot3;
		}
	}
	else
	{
		targetPos = D3DXVECTOR3(_manager->_target.x, _manager->_target.y, _manager->_target.z);
	}

	if (_manager->_light && _manager->_style == csLights)
	{
		pos = _manager->_light->GetSource()->GetPos();
		dir = _manager->_light->GetSource()->GetDir();
		right = _manager->_light->GetSource()->GetRight();
		rot = _manager->_light->GetSource()->GetRot();		
	}

	switch (_manager->_style)
	{
	case csFreeView:
	case csLights:
	case csIsoView:
	{
		if (_manager->_style == csIsoView)
		{
			dir = D3DXVECTOR3(1, 1, 0);
			D3DXVec3Normalize(&dir, &dir);
			right = D3DXVECTOR3(-1, 1, 0);
			D3DXVec3Normalize(&right, &right);
			rot = cIsoRot;
		}

		if (control->GetAsyncKey('W'))
			pos += dir * speedMove;
		if (control->GetAsyncKey('A'))
			pos += right * speedMove;
		if (control->GetAsyncKey('S'))	
			pos -= dir * speedMove;
		if (control->GetAsyncKey('D'))
			pos -= right * speedMove;

		/*if (GetAsyncKeyState('Q'))
			camera->SetPos(camera->GetPos() + ZVector * speedMove);
		if (GetAsyncKeyState('E'))
			camera->SetPos(camera->GetPos() - ZVector * speedMove);*/
		break;
	}

	case csThirdPerson:
	{
		float velocityLen = D3DXVec3Length(&targetVel);
		D3DXVECTOR3 velocity = targetDir + targetVel * 0.1f;
		D3DXVec3Normalize(&velocity, &velocity);

		//строим матрицу поворота относительно скорости
		D3DXVECTOR3 xVec;
		D3DXVec3Normalize(&xVec, &velocity);
		D3DXVECTOR3 yVec;
		D3DXVec3Cross(&yVec, &ZVector, &xVec);
		if (D3DXVec3Length(&yVec) < 0.001f)
		{
			//D3DXVec3Cross(&yVec, &xVec, &ZVector);
		}
		D3DXVec3Normalize(&yVec, &yVec);
		D3DXVECTOR3 zVec;
		D3DXVec3Cross(&zVec, &xVec, &yVec);
		D3DXVec3Normalize(&zVec, &zVec);

		D3DXMATRIX velMat;
		D3DXQUATERNION velRot;
		MatrixRotationFromAxis(xVec, yVec, zVec, velMat);
		D3DXQuaternionRotationMatrix(&velRot, &velMat);

		//
		D3DXQUATERNION camQuat;
		D3DXQUATERNION camQuat1 = rot;
		D3DXQUATERNION camQuat2 = velRot;
		D3DXQuaternionSlerp(&camQuat, &camQuat1, &camQuat2, 6.0f * deltaTime);

		/*D3DXVECTOR3 camPos;
		Vec3Rotate(cCamTargetOff, camQuat, camPos);
		camPos += targetPos;*/

		D3DXVECTOR3 camDir;
		Vec3Rotate(XVector, camQuat, camDir);		
		const float minSpeed = 0.0f;
		const float maxSpeed = 150.0f * 1000.0f / 3600.0f;
		float speedK = lsl::ClampValue((velocityLen - minSpeed) / (maxSpeed - minSpeed), 0.0f, 1.0f);
		const float minDist = 0.0f;
		const float maxDist = 1.5f;
		float distTarget = minDist + speedK * speedK * (maxDist - minDist);
		_staticFloat1 = _staticFloat1 + (distTarget - _staticFloat1) * lsl::ClampValue(deltaTime * 5.0f, 0.0f, 1.0f);
		
		D3DXVECTOR3 camOff;
		Vec3Rotate(cCamTargetOff + D3DXVECTOR3(-1.0f, 0.0f, 0.0f), camQuat, camOff);
		D3DXVECTOR3 camPos = targetPos - camDir * _staticFloat1 + camOff;

		/*D3DXVECTOR3 camPos;
		D3DXVECTOR3 camPos1 = pos;
		D3DXVECTOR3 camPos2;
		Vec3Rotate(cCamTargetOff, camQuat2, camPos2);
		camPos2 += targetPos;
		D3DXVec3Lerp(&camPos, &camPos1, &camPos2, 8.0f * deltaTime);*/

		D3DXQUATERNION yRot;
		D3DXQuaternionRotationAxis(&yRot, &D3DXVECTOR3(0, 1, 0), D3DX_PI * 12.0f);
		camQuat = yRot * camQuat;

		pos = camPos;
		rot = camQuat;

		break;
	}

	case csIsometric:
	{
		float camWidth = camera->GetWidth() / 2;
		float camHeight = camWidth / camera->GetAspect();
		float camSize = sqrt(camWidth * camWidth + camHeight * camHeight);
		//left, right, top, bottom
		D3DXVECTOR4 camBorder = D3DXVECTOR4(cIsoBorder, cIsoBorder, cIsoBorder/camera->GetAspect(), cIsoBorder/camera->GetAspect());

		//Обратный поворот
		D3DXQUATERNION cIsoInvRot;		
		D3DXQuaternionInverse(&cIsoInvRot, &cIsoRot);

		//Направление камеры в мировом пространстве
		D3DXVECTOR3 isoDir;
		Vec3Rotate(XVector, cIsoRot, isoDir);
		
		//Преобразуем в пространство камеры, чтобы вычислять смещение отностиельно центра экрана. Для перспективной проекции это дает артефакт удаления-приближения камеры, поэтому может быть опущено
		D3DXVECTOR3 targOff = targetDir;
		targOff.z = 0.0f;
		Vec3Rotate(targOff, cIsoInvRot, targOff);
		//Проецируем на плоскость смещения
		targOff.x = targOff.y;
		targOff.y = targOff.z;
		targOff.z = 0.0f;
		D3DXVec3Normalize(&targOff, &targOff);
		//
		float yTargetDot = D3DXVec3Dot(&targOff, &D3DXVECTOR3(0.0f, 1.0f, 0.0f));
		//Формируем вектор смещения
		targOff *= camSize;

		//Ограничиваем смещение в пределах квадрата на плоскости смещения чтобы камера не уезжала за объект
		//targOff.x = lsl::ClampValue(targOff.x, -camBorder.x, camBorder.y);
		//targOff.y = lsl::ClampValue(targOff.y, -camBorder.w, camBorder.z);
		if (abs(targOff.y) > 0.1f && abs(targOff.x/targOff.y) < camera->GetAspect())
		{
			float yCoord = lsl::ClampValue(targOff.y, -camBorder.w, camBorder.z);
			float xCoord = abs(yCoord) / yTargetDot;
			xCoord = sqrt(xCoord * xCoord - yCoord * yCoord);
			if (targOff.x < 0)
				xCoord = -xCoord;
			targOff.x = xCoord;
			targOff.y = yCoord;
		}
		else
		{
			float xCoord = lsl::ClampValue(targOff.x, -camBorder.x, camBorder.y);
			float yCoord = abs(xCoord) / sqrt(1.0f - (yTargetDot * yTargetDot));
			yCoord = sqrt(yCoord * yCoord - xCoord * xCoord);
			if (targOff.y < 0)
				yCoord = -yCoord;
			targOff.x = xCoord;
			targOff.y = yCoord;
		}

		targOff.z = targOff.y;
		targOff.y = targOff.x;
		targOff.x = 0.0f;

		//Обратное преобразование в мировое пространство
		Vec3Rotate(targOff, cIsoRot, targOff);

		//Планва интерполяция смещения		
		D3DXVec3Lerp(&_staticVec1, &_staticVec1, &targOff, deltaTime);
		//_staticVec1 = targOff;
		//Позиция камеры
		D3DXVECTOR3 camPos = targetPos + (-isoDir) * targDist + _staticVec1;

		//Плавная интерполяция при скачках машины
		D3DXVECTOR3 dTargetPos = targetPos - _staticVec2;
		_staticVec2 = targetPos;
		float dTargetLength = D3DXVec3Length(&dTargetPos);

		if (dTargetLength > 6.0f)
		{
			//только если это первый скачок
			_staticFloat2 = _staticFloat1 == 0.0f ? dTargetLength / 0.5f : _staticFloat2;
			_staticFloat1 = dTargetLength;
			_staticVec3 = dTargetPos / dTargetLength;			
		}

		if (_staticFloat1 > 0.0f)
		{
			_staticFloat1 = std::max(_staticFloat1 - _staticFloat2 * deltaTime, 0.0f);
			camPos = camPos - _staticFloat1 * _staticVec3;
		}

		pos = camPos;
		rot = cIsoRot;

		break;
	}

	case csAutoObserver:
	{
		D3DXVECTOR2 mPos = control->GetMouseVec();
		D3DXVECTOR2 dMPos = mPos - D3DXVECTOR2(_staticVec1.x, _staticVec1.y);
		_staticVec1 = D3DXVECTOR3(mPos.x, mPos.y, 0);
		bool leftDown = control->IsMouseDown(mkLeft) == akDown;

		if (leftDown && _staticVec2.z == 0.0f)
		{
			D3DXVECTOR2 dMPos2 = mPos - D3DXVECTOR2(_staticVec2.x, _staticVec2.y);
			if (D3DXVec2Length(&dMPos2) > 15.0f)
				_staticVec2.z = 1.0f;
			else
				leftDown = false;
		}
		else if (!leftDown)
		{
			_staticVec2 = D3DXVECTOR3(mPos.x, mPos.y, 0.0f);
		}

		bool dragX = leftDown && dMPos.x != 0;
		bool dragY = leftDown && dMPos.y != 0;
		bool restoreY = _staticFloat1 >= 3.0f;

		if (dragX || dragY)
			_staticFloat1 = 0.0f;
		_staticFloat1 += deltaTime;

		_manager->GetObserverCoord(targetPos, _manager->_target.w, NULL, _staticQuat1, dMPos, deltaTime, dragX, dragY, restoreY, &pos, &rot, &_staticFloat2);
		break;
	}

	}

	camera->SetPos(pos);
	camera->SetRot(rot);

	_manager->OrthoCullOffset();
	_manager->SyncLight();

	_manager->_world->GetGraph()->SetCubeViewPos(targetPos + ZVector * 1.0f);
	_manager->_world->GetGraph()->SetOrthoTarget(targetPos, targetSize);

	snd::Listener listener;
	listener.pos = targetPos;
	listener.rot = targetRot;
	_manager->_world->GetAudio()->SetListener(&listener);
}

void CameraManager::Control::StyleChanged(Style style, Style newStyle)
{
	if (newStyle == csAutoObserver)
		_staticFloat1 = 3.0f;
	else
		_staticFloat1 = 0;

	if (newStyle == csAutoObserver)
		_staticFloat2 = 1.0f;
	else
		_staticFloat2 = 0;

	if (newStyle == csAutoObserver)
	{
		lsl::Point pos = _manager->_world->GetControl()->GetMousePos();
		_staticVec2 = _staticVec1 = D3DXVECTOR3(static_cast<float>(pos.x), static_cast<float>(pos.y), 0);
	}		
	else if (newStyle == csIsometric)
	{
		TargetChanged();
	}
	else
		_staticVec2 = _staticVec1 = NullVector;

	_staticVec3 = NullVector;

	if (newStyle == csAutoObserver)
		_staticQuat1 = _manager->_camera->GetRot();
	else
		_staticQuat1 = NullQuaternion;
}

void CameraManager::Control::FlyStart()
{
	_flyAlpha = 0;
}

void CameraManager::Control::FlyCompleted()
{
	if (_manager->_style == csAutoObserver)
	{
		lsl::Point pos = _manager->_world->GetControl()->GetMousePos();
		_staticVec2 = _staticVec1 = D3DXVECTOR3(static_cast<float>(pos.x), static_cast<float>(pos.y), 0);
		_staticQuat1 = _manager->_camera->GetRot();
		_staticFloat1 = 3.0f;
	}
}

void CameraManager::Control::TargetChanged()
{
	if (_manager->_style == csIsometric)
	{
		_staticVec1 = NullVector;
		if (_manager->_player)
			_staticVec2 = _manager->_player && _manager->_player->GetCar().mapObj ? _manager->_player->GetCar().grActor->GetWorldPos() : _manager->_player->GetCar().pos3;
		else
			_staticVec2 = D3DXVECTOR3(_manager->_target);
	}
}

D3DXVECTOR3 CameraManager::ScreenToWorld(const lsl::Point& coord, float z)
{
	return _camera->GetContextInfo().ScreenToWorld(D3DXVECTOR2(static_cast<float>(coord.x), static_cast<float>(coord.y)), z, _world->GetView()->GetVPSize());
}

D3DXVECTOR2 CameraManager::WorldToScreen(const D3DXVECTOR3& coord)
{
	return _camera->GetContextInfo().WorldToScreen(coord, _world->GetView()->GetVPSize());
}

void CameraManager::ScreenToRay(const lsl::Point& coord, D3DXVECTOR3& rayStart, D3DXVECTOR3& rayVec)
{
	rayStart = ScreenToWorld(coord, 0);
	rayVec = ScreenToWorld(coord, 1) - rayStart;
	D3DXVec3Normalize(&rayVec, &rayVec);
}

bool CameraManager::ScreenPixelRayCastWithPlaneXY(const lsl::Point& coord, D3DXVECTOR3& outVec)
{
	D3DXVECTOR3 rayStart;
	D3DXVECTOR3 rayVec;
	ScreenToRay(coord, rayStart, rayVec);

	return RayCastIntersectPlane(rayStart, rayVec, ZPlane, outVec);
}

void CameraManager::FlyTo(const D3DXVECTOR3& pos, const D3DXQUATERNION& rot, float time)
{
	_flySPos = _camera->GetPos();
	_flySRot = _camera->GetRot();
	_flyPos = pos;
	_flyRot = rot;

	_flyTime = time;
	_flyCurTime = 0;
	
	_control->FlyStart();
}

void CameraManager::StopFly()
{
	_flyCurTime = -1;
}

bool CameraManager::InFly()
{
	return _flyCurTime != -1;
}

const D3DXVECTOR4& CameraManager::GetClampAngle() const
{
	return _clampAngle;
}

void CameraManager::SetClampAngle(const D3DXVECTOR4& value)
{
	_clampAngle = value;
}

const D3DXVECTOR3& CameraManager::GetAngleSpeed()
{
	return _angleSpeed;
}

void CameraManager::SetAngleSpeed(const D3DXVECTOR3& value)
{
	_angleSpeed = value;
}

const D3DXVECTOR3& CameraManager::GetStableAngle()
{
	return _stableAngle;
}

void CameraManager::SetStableAngle(const D3DXVECTOR3& value)
{
	_stableAngle = value;
}

CameraManager::Style CameraManager::GetStyle() const
{
	return _style;
}

void CameraManager::ChangeStyle(Style value)
{
	if (_style == csFreeView)
	{
		_lastFreePos = _camera->GetPos();
		_lastFreeRot = _camera->GetRot();
	}

	Style prevStyle = _style;	
	_style = value;
	_control->StyleChanged(prevStyle, value);
	_world->ResetCamera();

	if (value == csIsometric || value == csIsoView)
	{
		_camera->SetStyle(graph::csOrtho);
		_camera->SetWidth(28.0f * _world->GetGame()->GetCameraDistance());

		SetNear(1.0f);
		if (_world->GetEnv())
			SetFar(_world->GetEnv()->GetOrthoCameraFar());
	}
	else if (_style == csLights && _light)
	{
		_camera->SetStyle(graph::csPerspective);

		SetNear(_light->GetDesc().nearDist);
		SetFar(_light->GetDesc().farDist);
	}
	else
	{
		_camera->SetStyle(graph::csPerspective);

		SetNear(1.0f);
		if (_world->GetEnv())
			SetFar(_world->GetEnv()->GetPerspectiveCameraFar());
	}

	if (_style == csAutoObserver || _style == csBlock)
		_camera->SetFov(60 * D3DX_PI / 180);
	else if (_style == csThirdPerson)
		_camera->SetFov(75 * D3DX_PI / 180);
	else
		_camera->SetFov(90 * D3DX_PI / 180);

	if (_style == csFreeView)
	{
		_camera->SetPos(_lastFreePos);
		_camera->SetRot(_lastFreeRot);
	}
	else if (_style == csIsoView)
	{
		_camera->SetRot(_lastFreeRot);

		D3DXVECTOR3 pos = _lastFreePos;
		pos.z = std::max(pos.z, 10.0f);
		_camera->SetPos(pos);
	}
	else if (_style == csIsometric && prevStyle != csThirdPerson)
	{
		_camera->SetPos(NullVector);
		_camera->SetRot(NullQuaternion);
	}

	_world->GetGraph()->SetReflMappMode((_style == csIsometric || _style == csThirdPerson) ? GraphManager::rmColorLayer : GraphManager::rmLackLayer);
	_world->GetGraph()->SetShadowMaxFar(_style == csThirdPerson ? 60.0f : (_style == csIsometric ? 55.0f : 0.0f));
	
}

float CameraManager::GetAspect() const
{
	return _camera->GetAspect();
}

void CameraManager::SetAspect(float value)
{
	_camera->SetAspect(value);
}

D3DXVECTOR3 CameraManager::GetPos() const
{
	return _camera->GetWorldPos();
}

D3DXVECTOR3 CameraManager::GetDir() const
{
	return _camera->GetWorldDir();
}

D3DXVECTOR3 CameraManager::GetRight() const
{
	return _camera->GetRight();
}

float CameraManager::GetNear() const
{
	return _near;
}

void CameraManager::SetNear(float value)
{
	_near = value;
	_camera->SetNear(value);	
}

float CameraManager::GetFar() const
{
	return _far;
}

void CameraManager::SetFar(float value)
{
	_far = value;
	_camera->SetFar(value);
}

Player* CameraManager::GetPlayer()
{
	return _player;
}

void CameraManager::SetPlayer(Player* value)
{
	if (Object::ReplaceRef(_player, value))
	{
		_player = value;

		_control->TargetChanged();
	}
}

const D3DXVECTOR4& CameraManager::GetTarget()
{
	return _target;
}

void CameraManager::SetTarget(const D3DXVECTOR4& value)
{
	_target = value;
}

GraphManager::LightSrc* CameraManager::GetLight()
{
	return _light;
}

void CameraManager::SetLight(GraphManager::LightSrc* value)
{
	if (Object::ReplaceRef(_light, value))
		_light = value;
}

void CameraManager::GetObserverCoord(const D3DXVECTOR3& targetPos, float targetDist, D3DXVECTOR3* pos, D3DXQUATERNION& rot, const D3DXVECTOR2& dMPos, float deltaTime, bool dragX, bool dragY, bool restoreY, D3DXVECTOR3* camPos, D3DXQUATERNION* camRot, float* dir)
{
	float camDist = targetDist;

	if (dragX || restoreY)
	{
		float dAngZ = deltaTime * _angleSpeed.x;
		if (dir != NULL)
			dAngZ = dAngZ * (*dir);

		if (dragX)
		{
			dAngZ = lsl::ClampValue(dMPos.x * D3DX_PI * 0.001f, -D3DX_PI/2, D3DX_PI/2);
		}

		D3DXQUATERNION dRotZ;
		D3DXQuaternionRotationAxis(&dRotZ, &ZVector, dAngZ);
		rot = rot * dRotZ;

		if (_clampAngle.x != 0 || _clampAngle.y != 0)
		{
			D3DXVECTOR3 yAxis;
			QuatRotateVec3(yAxis, YVector, rot);
			D3DXVec3Normalize(&yAxis, &yAxis);

			D3DXVECTOR3 norm;
			D3DXVec3Cross(&norm, &YVector, &yAxis);
			D3DXVec3Normalize(&norm, &norm);

			float ang = acos(D3DXVec3Dot(&YVector, &yAxis));
			bool angClamp = false;
			if (norm.z > 0)
			{
				angClamp = _clampAngle.x - ang < 0.001f;
				ang = std::min(ang, _clampAngle.x);
			}
			else
			{
				angClamp = _clampAngle.y - ang < 0.001f;
				ang = std::min(ang, _clampAngle.y);
			}

			if (angClamp)
			{
				D3DXQuaternionRotationAxis(&dRotZ, &norm, ang);
				QuatRotateVec3(yAxis, YVector, dRotZ);
				D3DXVec3Normalize(&yAxis, &yAxis);

				D3DXVECTOR3 zAxis;
				QuatRotateVec3(zAxis, ZVector, rot);
				D3DXVec3Normalize(&zAxis, &zAxis);

				ang = acos(D3DXVec3Dot(&ZVector, &zAxis));
				D3DXQUATERNION dRotY;
				D3DXQuaternionRotationAxis(&dRotY, &yAxis, ang);
				QuatRotateVec3(zAxis, ZVector, dRotY);

				D3DXVECTOR3 xAxis;
				D3DXVec3Cross(&xAxis, &yAxis, &zAxis);
				D3DXVec3Normalize(&xAxis, &xAxis);

				D3DXMATRIX rotMat;
				MatrixRotationFromAxis(xAxis, yAxis, zAxis, rotMat);
				D3DXQuaternionRotationMatrix(&rot, &rotMat);

				if (dir != NULL)
				{
					if (norm.z > 0)
						*dir = -1.0f;
					else
						*dir = 1.0f;
				}
			}
		}
	}

	if (dragY || restoreY)
	{
		float angLow = _stableAngle.x;
		float angUp = _stableAngle.x;

		D3DXQUATERNION dRotY;
		D3DXVECTOR3 yAxis;
		QuatRotateVec3(yAxis, YVector, rot);

		if (dragY)
		{
			float dAngY = lsl::ClampValue(-dMPos.y * D3DX_PI * 0.001f, -D3DX_PI/2, D3DX_PI/2);
			angLow = _clampAngle.z;
			angUp = _clampAngle.w;

			D3DXQuaternionRotationAxis(&dRotY, &yAxis, dAngY);
			rot = rot * dRotY;
		}

		D3DXVECTOR3 xAxis;
		QuatRotateVec3(xAxis, XVector, rot);
		D3DXVec3Normalize(&xAxis, &xAxis);

		D3DXVECTOR3 norm;
		D3DXVec3Cross(&norm, &(-ZVector), &yAxis);
		D3DXVec3Normalize(&norm, &norm);
		float ang = 0;		
		bool angClamp = false;

		if (D3DXVec3Dot(&norm, &xAxis) < 0)
		{
			angClamp = true;
			ang = dMPos.y > 0 ? angUp : angLow;				
		}
		else
		{
			ang = acos(D3DXVec3Dot(&xAxis, &(-ZVector)));
			angClamp = ang - angLow < -0.001f || ang - angUp > 0.001f;
		}

		if (angClamp)
		{
			ang = ClampValue(ang, angLow, angUp);
			D3DXQuaternionRotationAxis(&dRotY, &yAxis, -ang);
			QuatRotateVec3(xAxis, -ZVector, dRotY);
			D3DXVec3Normalize(&xAxis, &xAxis);
			D3DXVECTOR3 zAxis;
			D3DXVec3Cross(&zAxis, &xAxis, &yAxis);
			D3DXVec3Normalize(&zAxis, &zAxis);
			D3DXMATRIX rotMat;
			MatrixRotationFromAxis(xAxis, yAxis, zAxis, rotMat);
			D3DXQuaternionRotationMatrix(&rot, &rotMat);
		}			
	}

	if (pos != NULL)
	{
		D3DXVECTOR3 camDir;
		Vec3Rotate(XVector, rot, camDir);
		*pos = targetPos - camDir * camDist;
	}

	if (camRot != NULL)
	{
		D3DXQuaternionSlerp(camRot, camRot, &rot, 6.0f * deltaTime);
	}

	if (camPos != NULL && camRot != NULL)
	{
		D3DXVECTOR3 camDir;
		Vec3Rotate(XVector, *camRot, camDir);
		*camPos = targetPos - camDir * camDist;
	}
}

}

}