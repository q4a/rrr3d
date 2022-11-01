#include "stdafx.h"
#include "game\World.h"

#include "game\CameraManager.h"

namespace r3d
{

namespace game
{

CameraManager::CameraManager(World* world): _world(world), _player(0), _target(0, 0, 0, 5.0f), _light(0), _style(cStyleEnd), _near(1.0f), _far(100.0f), _clampAngle(0.0f, 0.0f, glm::radians(25.0f), glm::radians(80.0f)), _angleSpeed(glm::pi<float>()/48, 0, 0), _stableAngle(glm::radians(75.0f), 0, 0), _flyCurTime(-1)
{
	_camera = new graph::Camera();
	_camera->SetPos(glm::vec3(50.0f, 50.0f, 50.0f));
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
		glm::quat rot = camera->GetRot();
		glm::vec3 right = camera->GetRight();

		if (_manager->_style == csLights && _manager->_light)
		{
			rot = _manager->_light->GetSource()->GetRot();
			right = _manager->_light->GetSource()->GetRight();
		}

		if (mMove.click.key == lsl::mkRight && mMove.click.state == lsl::ksDown)
		{
			glm::quat quatZ = glm::angleAxis(0.005f * (-mMove.dtCoord.x), ZVector);
			glm::quat quatRight = glm::angleAxis(0.005f * mMove.dtCoord.y, right);
			rot = quatZ * quatRight * rot;

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

		glm::vec3 pos = Vec3Lerp(_manager->_flySPos, _manager->_flyPos, alpha);

		glm::quat rot = glm::slerp(_manager->_flySRot, _manager->_flyRot, alpha);

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

	static float cIsoAngY = glm::radians(15.5f);
	const float cIsoAngZ = glm::radians(45.0f);
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
				cIsoAngY = glm::radians(16.0f);
				cIsoBorder = 5.5f;
			}
			else
			{
				tmp = 0;
				cIsoAngY = glm::radians(15.5f);
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
				cIsoAngY = glm::radians(16.5f);
				cIsoBorder = 5.5f;
			}
			else if (tmp == 1)
			{
				cIsoAngY = glm::radians(16.0f);
				cIsoBorder = 5.5f;
			}
			else
			{
				cIsoAngY = glm::radians(15.5f);
				cIsoBorder = 5.0f;
			}
		}
	}
#endif*/

	glm::quat isoRotY = glm::angleAxis(cIsoAngY, YVector);
	glm::quat isoRotZ = glm::angleAxis(cIsoAngZ, ZVector);
	//
	glm::quat cIsoRot = isoRotZ * isoRotY;

	const float speedMove = 20.0f * deltaTime;
	const float angleSpeed = glm::half_pi<float>() * deltaTime;
	const float retAngleSpeed = angleSpeed * 2.0f;
	const float maxAngle = glm::pi<float>() / 6;
	const glm::vec3 cCamTargetOff(-4.6f, 0.0f, 2.4f);

	ControlManager* control = _manager->_world->GetControl();

	glm::vec3 pos = camera->GetPos();
	glm::quat rot = camera->GetRot();
	glm::vec3 dir = camera->GetDir();
	glm::vec3 right = camera->GetRight();

	glm::vec3 targetPos = NullVector;
	glm::vec3 targetDir = XVector;
	glm::quat targetRot = NullQuaternion;
	float targetSize = 0.0f;
	glm::vec3 targetVel = NullVector;
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
		targetPos = glm::vec3(_manager->_target.x, _manager->_target.y, _manager->_target.z);
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
			dir = glm::vec3(1, 1, 0);
			dir = glm::normalize(dir);
			right = glm::vec3(-1, 1, 0);
			right = glm::normalize(right);
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
		float velocityLen = glm::length(targetVel);
		glm::vec3 velocity = targetDir + targetVel * 0.1f;
		velocity = glm::normalize(velocity);

		//строим матрицу поворота относительно скорости
		glm::vec3 xVec;
		xVec = glm::normalize(velocity);
		glm::vec3 yVec;
		yVec = glm::cross(ZVector, xVec);
		if (glm::length(yVec) < 0.001f)
		{
			//yVec = glm::cross(xVec, ZVector);
		}
		yVec = glm::normalize(yVec);
		glm::vec3 zVec;
		zVec = glm::cross(xVec, yVec);
		zVec = glm::normalize(zVec);

		D3DXMATRIX velMat;
		MatrixRotationFromAxis(xVec, yVec, zVec, velMat);
		glm::quat velRot = glm::quat_cast(Matrix4DxToGlm(velMat));
		velRot = glm::quat(-velRot.w, velRot.x, velRot.y, velRot.z);

		//
		glm::quat camQuat1 = rot;
		glm::quat camQuat2 = velRot;
		glm::quat camQuat = glm::slerp(camQuat1, camQuat2, 6.0f * deltaTime);

		/*glm::vec3 camPos;
		Vec3Rotate(cCamTargetOff, camQuat, camPos);
		camPos += targetPos;*/

		glm::vec3 camDir;
		Vec3Rotate(XVector, camQuat, camDir);
		const float minSpeed = 0.0f;
		const float maxSpeed = 150.0f * 1000.0f / 3600.0f;
		float speedK = lsl::ClampValue((velocityLen - minSpeed) / (maxSpeed - minSpeed), 0.0f, 1.0f);
		const float minDist = 0.0f;
		const float maxDist = 1.5f;
		float distTarget = minDist + speedK * speedK * (maxDist - minDist);
		_staticFloat1 = _staticFloat1 + (distTarget - _staticFloat1) * lsl::ClampValue(deltaTime * 5.0f, 0.0f, 1.0f);

		glm::vec3 camOff;
		Vec3Rotate(cCamTargetOff + glm::vec3(-1.0f, 0.0f, 0.0f), camQuat, camOff);
		glm::vec3 camPos = targetPos - camDir * _staticFloat1 + camOff;

		/*glm::vec3 camPos;
		glm::vec3 camPos1 = pos;
		glm::vec3 camPos2;
		Vec3Rotate(cCamTargetOff, camQuat2, camPos2);
		camPos2 += targetPos;
		camPos = Vec3Lerp(camPos1, camPos2, 8.0f * deltaTime);*/

		glm::quat yRot = glm::angleAxis(glm::pi<float>() * 12.0f, glm::vec3(0, 1, 0));
		camQuat = camQuat * yRot;

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
		glm::vec4 camBorder = glm::vec4(cIsoBorder, cIsoBorder, cIsoBorder/camera->GetAspect(), cIsoBorder/camera->GetAspect());

		//Обратный поворот
		glm::quat cIsoInvRot = glm::inverse(cIsoRot);

		//Направление камеры в мировом пространстве
		glm::vec3 isoDir;
		Vec3Rotate(XVector, cIsoRot, isoDir);

		//Преобразуем в пространство камеры, чтобы вычислять смещение отностиельно центра экрана. Для перспективной проекции это дает артефакт удаления-приближения камеры, поэтому может быть опущено
		glm::vec3 targOff = targetDir;
		targOff.z = 0.0f;
		Vec3Rotate(targOff, cIsoInvRot, targOff);
		//Проецируем на плоскость смещения
		targOff.x = targOff.y;
		targOff.y = targOff.z;
		targOff.z = 0.0f;
		targOff = glm::normalize(targOff);
		//
		float yTargetDot = glm::dot(targOff, glm::vec3(0.0f, 1.0f, 0.0f));
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
		_staticVec1 = Vec3Lerp(_staticVec1, targOff, deltaTime);
		//_staticVec1 = targOff;
		//Позиция камеры
		glm::vec3 camPos = targetPos + (-isoDir) * targDist + _staticVec1;

		//Плавная интерполяция при скачках машины
		glm::vec3 dTargetPos = targetPos - _staticVec2;
		_staticVec2 = targetPos;
		float dTargetLength = glm::length(dTargetPos);

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
		glm::vec2 mPos = control->GetMouseVec();
		glm::vec2 dMPos = mPos - glm::vec2(_staticVec1.x, _staticVec1.y);
		_staticVec1 = glm::vec3(mPos.x, mPos.y, 0);
		bool leftDown = control->IsMouseDown(mkLeft) == akDown;

		if (leftDown && _staticVec2.z == 0.0f)
		{
			glm::vec2 dMPos2 = mPos - glm::vec2(_staticVec2.x, _staticVec2.y);
			if (glm::length(dMPos2) > 15.0f)
				_staticVec2.z = 1.0f;
			else
				leftDown = false;
		}
		else if (!leftDown)
		{
			_staticVec2 = glm::vec3(mPos.x, mPos.y, 0.0f);
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
		_staticVec2 = _staticVec1 = glm::vec3(static_cast<float>(pos.x), static_cast<float>(pos.y), 0);
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
		_staticVec2 = _staticVec1 = glm::vec3(static_cast<float>(pos.x), static_cast<float>(pos.y), 0);
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
			_staticVec2 = glm::vec3(_manager->_target);
	}
}

glm::vec3 CameraManager::ScreenToWorld(const lsl::Point& coord, float z)
{
	return _camera->GetContextInfo().ScreenToWorld(glm::vec2(static_cast<float>(coord.x), static_cast<float>(coord.y)), z, _world->GetView()->GetVPSize());
}

glm::vec2 CameraManager::WorldToScreen(const glm::vec3& coord)
{
	return _camera->GetContextInfo().WorldToScreen(coord, _world->GetView()->GetVPSize());
}

void CameraManager::ScreenToRay(const lsl::Point& coord, glm::vec3& rayStart, glm::vec3& rayVec)
{
	rayStart = ScreenToWorld(coord, 0);
	rayVec = ScreenToWorld(coord, 1) - rayStart;
	rayVec = glm::normalize(rayVec);
}

bool CameraManager::ScreenPixelRayCastWithPlaneXY(const lsl::Point& coord, glm::vec3& outVec)
{
	glm::vec3 rayStart;
	glm::vec3 rayVec;
	ScreenToRay(coord, rayStart, rayVec);

	return RayCastIntersectPlane(rayStart, rayVec, ZPlane, outVec);
}

void CameraManager::FlyTo(const glm::vec3& pos, const glm::quat& rot, float time)
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

const glm::vec4& CameraManager::GetClampAngle() const
{
	return _clampAngle;
}

void CameraManager::SetClampAngle(const glm::vec4& value)
{
	_clampAngle = value;
}

const glm::vec3& CameraManager::GetAngleSpeed()
{
	return _angleSpeed;
}

void CameraManager::SetAngleSpeed(const glm::vec3& value)
{
	_angleSpeed = value;
}

const glm::vec3& CameraManager::GetStableAngle()
{
	return _stableAngle;
}

void CameraManager::SetStableAngle(const glm::vec3& value)
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
		_camera->SetFov(glm::radians(60.0f));
	else if (_style == csThirdPerson)
		_camera->SetFov(glm::radians(75.0f));
	else
		_camera->SetFov(glm::radians(90.0f));

	if (_style == csFreeView)
	{
		_camera->SetPos(_lastFreePos);
		_camera->SetRot(_lastFreeRot);
	}
	else if (_style == csIsoView)
	{
		_camera->SetRot(_lastFreeRot);

		glm::vec3 pos = _lastFreePos;
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

glm::vec3 CameraManager::GetPos() const
{
	return _camera->GetWorldPos();
}

glm::vec3 CameraManager::GetDir() const
{
	return _camera->GetWorldDir();
}

glm::vec3 CameraManager::GetRight() const
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

const glm::vec4& CameraManager::GetTarget()
{
	return _target;
}

void CameraManager::SetTarget(const glm::vec4& value)
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

void CameraManager::GetObserverCoord(const glm::vec3& targetPos, float targetDist, glm::vec3* pos, glm::quat& rot, const glm::vec2& dMPos, float deltaTime, bool dragX, bool dragY, bool restoreY, glm::vec3* camPos, glm::quat* camRot, float* dir)
{
	float camDist = targetDist;

	if (dragX || restoreY)
	{
		float dAngZ = deltaTime * _angleSpeed.x;
		if (dir != NULL)
			dAngZ = dAngZ * (*dir);

		if (dragX)
		{
			dAngZ = lsl::ClampValue(dMPos.x * glm::pi<float>() * 0.001f, -glm::half_pi<float>(), glm::half_pi<float>());
		}

		glm::quat dRotZ = glm::angleAxis(dAngZ, ZVector);
		rot = dRotZ * rot;

		if (_clampAngle.x != 0 || _clampAngle.y != 0)
		{
			glm::vec3 yAxis;
			QuatRotateVec3(yAxis, YVector, rot);
			yAxis = glm::normalize(yAxis);

			glm::vec3 norm;
			norm = glm::cross(YVector, yAxis);
			norm = glm::normalize(norm);

			float ang = acos(glm::dot(YVector, yAxis));
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
				dRotZ = glm::angleAxis(ang, norm);
				QuatRotateVec3(yAxis, YVector, dRotZ);
				yAxis = glm::normalize(yAxis);

				glm::vec3 zAxis;
				QuatRotateVec3(zAxis, ZVector, rot);
				zAxis = glm::normalize(zAxis);

				ang = acos(glm::dot(ZVector, zAxis));
				glm::quat dRotY = glm::angleAxis(ang, yAxis);
				QuatRotateVec3(zAxis, ZVector, dRotY);

				glm::vec3 xAxis;
				xAxis = glm::cross(yAxis, zAxis);
				xAxis = glm::normalize(xAxis);

				D3DXMATRIX rotMat;
				MatrixRotationFromAxis(xAxis, yAxis, zAxis, rotMat);
				rot = glm::quat_cast(Matrix4DxToGlm(rotMat));
				rot = glm::quat(-rot.w, rot.x, rot.y, rot.z);

				if (dir != NULL)
				{
					if (norm.z > 0)
					{
						*dir = -1.0f;
					}
					else
					{
						*dir = 1.0f;
					}
				}
			}
		}
	}

	if (dragY || restoreY)
	{
		float angLow = _stableAngle.x;
		float angUp = _stableAngle.x;

		glm::quat dRotY;
		glm::vec3 yAxis;
		QuatRotateVec3(yAxis, YVector, rot);

		if (dragY)
		{
			float dAngY = lsl::ClampValue(-dMPos.y * glm::pi<float>() * 0.001f, -glm::half_pi<float>(), glm::half_pi<float>());
			angLow = _clampAngle.z;
			angUp = _clampAngle.w;

			dRotY = glm::angleAxis(dAngY, yAxis);
			rot = dRotY * rot;
		}

		glm::vec3 xAxis;
		QuatRotateVec3(xAxis, XVector, rot);
		xAxis = glm::normalize(xAxis);

		glm::vec3 norm;
		norm = glm::cross((-ZVector), yAxis);
		norm = glm::normalize(norm);
		float ang = 0;
		bool angClamp = false;

		if (glm::dot(norm, xAxis) < 0)
		{
			angClamp = true;
			ang = dMPos.y > 0 ? angUp : angLow;
		}
		else
		{
			ang = acos(glm::dot(xAxis, (-ZVector)));
			angClamp = ang - angLow < -0.001f || ang - angUp > 0.001f;
		}

		if (angClamp)
		{
			ang = ClampValue(ang, angLow, angUp);
			dRotY = glm::angleAxis(-ang, yAxis);
			QuatRotateVec3(xAxis, -ZVector, dRotY);
			xAxis = glm::normalize(xAxis);
			glm::vec3 zAxis;
			zAxis = glm::cross(xAxis, yAxis);
			zAxis = glm::normalize(zAxis);
			D3DXMATRIX rotMat;
			MatrixRotationFromAxis(xAxis, yAxis, zAxis, rotMat);
			rot = glm::quat_cast(Matrix4DxToGlm(rotMat));
			rot = glm::quat(-rot.w, rot.x, rot.y, rot.z);
		}
	}

	if (pos != NULL)
	{
		glm::vec3 camDir;
		Vec3Rotate(XVector, rot, camDir);
		*pos = targetPos - camDir * camDist;
	}

	if (camRot != NULL)
	{
		*camRot = glm::slerp(*camRot, rot, 6.0f * deltaTime);
	}

	if (camPos != NULL && camRot != NULL)
	{
		glm::vec3 camDir;
		Vec3Rotate(XVector, *camRot, camDir);
		*camPos = targetPos - camDir * camDist;
	}
}

}

}