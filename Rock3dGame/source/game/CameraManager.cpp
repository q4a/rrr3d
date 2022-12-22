#include "stdafx.h"
#include "game/World.h"

#include "game/CameraManager.h"

namespace r3d
{
	namespace game
	{
		CameraManager::CameraManager(World* world): _world(world), _style(cStyleEnd), _near(1.0f), _far(100.0f),
		                                            _clampAngle(0.0f, 0.0f, 25.0f * D3DX_PI / 180,
		                                                        80.0f * D3DX_PI / 180), _angleSpeed(D3DX_PI / 48, 0, 0),
		                                            _stableAngle(75.0f * D3DX_PI / 180, 0, 0), _flyCurTime(-1),
		                                            _player(nullptr), _secondPlr(nullptr), _light(nullptr), _target(0, 0, 0, 5.0f)
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
			SetLight(nullptr);
			SetPlayer(nullptr);
			SetSecondPlayer(nullptr);

			_world->GetControl()->RemoveEvent(_control);
			delete _control;
			_world->GetGraph()->SetCamera(nullptr);
			delete _camera;
		}

		void CameraManager::OrthoCullOffset()
		{

		}

		void CameraManager::SyncLight()
		{
			if (_style == csLights && _light)
			{
				_light->GetSource()->SetPos(_camera->GetPos());
				_light->GetSource()->SetRot(_camera->GetRot());
			}
		}

		CameraManager::Control::Control(CameraManager* manager): _manager(manager), _staticFloat1(0),
		                                                         _staticVec1(NullVector), _staticVec2(NullVector),
		                                                         _staticVec3(NullVector)
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

				if (mMove.click.key == mkRight && mMove.click.state == ksDown)
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
			if (GAME_PAUSED == false && msg.action == gaViewSwitch && msg.state == ksDown && !msg.repeat)
			{
				if (_manager->GetPlayer() && _manager->GetPlayer()->GetRace()->GetCamLock() || _manager->GetPlayer() ==
					nullptr || _manager->GetPlayer()->InRace() == false || _manager->GetPlayer()->GetFinished() || (
						_manager->GetPlayer()->GetCar().gameObj == nullptr && _manager->GetPlayer()->IsGamer()) ||
					_manager->GetPlayer()->GetRace()->GetGame()->autoCamera() || _manager->_world->GetGame()->
					IsStartRace() == false || _manager->GetPlayer()->IsBlock())
					return true;

				if (_manager->_world->GetGame()->GetRace()->GetTournament().GetCurTrack().CamLock())
				{
					return true;
				}

#if _DEBUG
			int styleMap[cStyleEnd] = {csThirdPerson, csIsometric, csLights, csIsoView, csFreeView, csFreeView, csFreeView};
#else
				int styleMap[cStyleEnd] = {
					cStyleEnd, csIsometric, csThirdPerson, cStyleEnd, cStyleEnd, cStyleEnd, cStyleEnd
				};

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
					auto camStyle = static_cast<Style>(styleMap[_manager->_style]);
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
			GameMode* game = nullptr;

			if (_manager->InFly())
			{
				_manager->_flyCurTime += deltaTime;

				float alpha = ClampValue(_manager->_flyCurTime, 0.0f, _manager->_flyTime);
				alpha = _flyAlpha = _flyAlpha + std::max((alpha - _flyAlpha) * deltaTime * 4.0f, 0.001f);
				alpha = ClampValue(alpha / _manager->_flyTime, 0.0f, 1.0f);

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

			float cIsoAngY = 15.5f * D3DX_PI / 180;
			const float cIsoAngZ = 45.0f * D3DX_PI / 180;
			const float targDist = 20.0f;
			static float cIsoBorder = 5.0f;
			if (_manager->GetPlayer() && _manager->GetPlayer()->GetRace()->GetGame()->CamProection() == 1)
			{
				cIsoAngY = 35.0f * D3DX_PI / 180;
			}
			else
			{
				cIsoAngY = 15.5f * D3DX_PI / 180;
			}


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

			D3DXVECTOR3 pos = camera->GetFirstPos();
			D3DXQUATERNION rot = camera->GetFirstRot();
			D3DXVECTOR3 dir = camera->GetDir();
			D3DXVECTOR3 right = camera->GetRight();

			D3DXVECTOR3 targetPos = NullVector;
			D3DXVECTOR3 targetDir = XVector;
			D3DXQUATERNION targetRot = NullQuaternion;
			float targetSize = 0.0f;
			D3DXVECTOR3 targetVel = NullVector;
			float targetDrivenSpeed = 0.0f;

			Player* player = _manager->_player;
			/// ////////////////////////////////////////////////////////////////////////
			D3DXVECTOR3 s_pos = camera->GetSecondPos();
			D3DXQUATERNION s_rot = camera->GetSecondRot();
			D3DXVECTOR3 s_dir = camera->GetDir();
			D3DXVECTOR3 s_right = camera->GetRight();

			Player* plr2 = _manager->_secondPlr;
			D3DXVECTOR3 s_targetPos = NullVector;
			D3DXVECTOR3 s_targetDir = XVector;
			D3DXQUATERNION s_targetRot = NullQuaternion;
			float s_targetSize = 0.0f;
			D3DXVECTOR3 s_targetVel = NullVector;
			float s_targetDrivenSpeed = 0.0f;


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

					//������������ �������� �����, ������� �� ���������� ������ ���������������� ���� ��� ������ ����. ���� ���������������, ���������� ��������, ���������� ����� � ����� ������� �� �� ������������ �������������
					if (targetDrivenSpeed < 0.1f)
					{
						//�������� ������������� ������������ �� ������������ ��������� ��� x
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


			if (plr2)
			{
				s_targetSize = plr2->GetCar().size;

				if (plr2->GetCar().grActor)
				{
					s_targetPos = plr2->GetCar().grActor->GetWorldPos();
					s_targetDir = plr2->GetCar().grActor->GetWorldDir();
					s_targetRot = plr2->GetCar().grActor->GetWorldRot();
					s_targetVel = plr2->GetCar().gameObj->GetPxVelocityLerp();
					s_targetDrivenSpeed = plr2->GetCar().gameObj->GetDrivenWheelSpeed();

					//������������ �������� �����, ������� �� ���������� ������ ���������������� ���� ��� ������ ����. ���� ���������������, ���������� ��������, ���������� ����� � ����� ������� �� �� ������������ �������������
					if (s_targetDrivenSpeed < 0.1f)
					{
						//�������� ������������� ������������ �� ������������ ��������� ��� x
						plr2->GetCar().grActor->WorldToLocalNorm(s_targetVel, s_targetVel);
						s_targetVel.x = std::max(targetVel.x, 0.0f);
						plr2->GetCar().grActor->LocalToWorldNorm(s_targetVel, s_targetVel);
					}
				}
				else
				{
					s_targetPos = plr2->GetCar().pos3;
					s_targetDir = plr2->GetCar().dir3;
					s_targetRot = plr2->GetCar().rot3;
				}
			}
			else
			{
				s_targetPos = D3DXVECTOR3(_manager->_target.x, _manager->_target.y, _manager->_target.z);
			}

			///////////////////////////////////////////////////////////////////////////////////////////////////////////

			switch (_manager->_style)
			{
			case csFreeView:
			case csLights:
			case csIsoView:
			{
				if (_manager->_style == csIsoView)
				{
				}
				break;
			}

			case csThirdPerson:
			{
				float velocityLen = D3DXVec3Length(&targetVel);
				D3DXVECTOR3 velocity = targetDir + targetVel * 0.1f;
				//������ ����������. ������ ������...
				if (_manager->GetPlayer() && _manager->GetPlayer()->GetFinished() && _manager->GetPlayer()->GetPlace() == 1)
					velocity = -targetDir + -targetVel * 0.1f;

				D3DXVec3Normalize(&velocity, &velocity);

				//������ ������� �������� ������������ ��������
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

				D3DXVECTOR3 camDir;
				Vec3Rotate(XVector, camQuat, camDir);
				/* ��������� ������ � ����������� �������� �� ���������
				const float minSpeed = 0.0f;
				const float maxSpeed = 150.0f * 1000.0f / 3600.0f;
				float speedK = lsl::ClampValue((velocityLen - minSpeed) / (maxSpeed - minSpeed), 0.0f, 1.0f);
				const float minDist = 0.0f;
				const float maxDist = 1.5f;
				float distTarget = minDist + speedK * speedK * (maxDist - minDist);
				_staticFloat1 = _staticFloat1 + (distTarget - _staticFloat1) * lsl::ClampValue(deltaTime * 5.0f, 0.0f, 1.0f);
				*/

				//���������� ������ ������� �� FOV...
				float distance = 1.2f;
				if (CAM_FOV < 85)
					distance = 2.5f;
				else if (CAM_FOV > 95)
					distance = 0.4f;

				if (SPLIT_TYPE == 1)
					distance *= 2.5f;

				D3DXVECTOR3 camOff;
				Vec3Rotate(cCamTargetOff + D3DXVECTOR3(-1.0f, 0.0f, 0.0f), camQuat, camOff);
				D3DXVECTOR3 camPos = targetPos - camDir * distance + camOff;


				D3DXQUATERNION yRot;
				D3DXQuaternionRotationAxis(&yRot, &D3DXVECTOR3(0, 1, 0), D3DX_PI * 12.0f);
				camQuat = yRot * camQuat;
				camera->FirtsViewIso(false);
				if (SPLIT_TYPE == 1)
					camera->SetUserFov(CAM_FOV / 2);
				else
					camera->SetUserFov(CAM_FOV);

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
				auto camBorder = D3DXVECTOR4(cIsoBorder, cIsoBorder, cIsoBorder / camera->GetAspect(),
					cIsoBorder / camera->GetAspect());

				D3DXQUATERNION cIsoInvRot;
				D3DXQuaternionInverse(&cIsoInvRot, &cIsoRot);

				D3DXVECTOR3 isoDir;
				Vec3Rotate(XVector, cIsoRot, isoDir);
				D3DXVECTOR3 targOff = targetDir;
				targOff.z = 0.0f;
				Vec3Rotate(targOff, cIsoInvRot, targOff);
				//���������� �� ��������� ��������
				targOff.x = targOff.y;
				targOff.y = targOff.z;
				targOff.z = 0.0f;
				D3DXVec3Normalize(&targOff, &targOff);
				//
				float yTargetDot = D3DXVec3Dot(&targOff, &D3DXVECTOR3(0.0f, 1.0f, 0.0f));
				targOff *= camSize;

				if (abs(targOff.y) > 0.1f && abs(targOff.x / targOff.y) < camera->GetAspect())
				{
					float yCoord = ClampValue(targOff.y, -camBorder.w, camBorder.z);
					float xCoord = abs(yCoord) / yTargetDot;
					xCoord = sqrt(xCoord * xCoord - yCoord * yCoord);
					if (targOff.x < 0)
						xCoord = -xCoord;
					targOff.x = xCoord;
					targOff.y = yCoord;
				}
				else
				{
					float xCoord = ClampValue(targOff.x, -camBorder.x, camBorder.y);
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

				//�������� �������������� � ������� ������������
				Vec3Rotate(targOff, cIsoRot, targOff);

				D3DXVec3Lerp(&_staticVec1, &_staticVec1, &targOff, deltaTime);
				D3DXVECTOR3 camPos = targetPos + (-isoDir) * targDist + _staticVec1;

				//������� ������������ ��� ������� ������
				//� ��������� � ����������� ����� ��������!!!
				/*
				D3DXVECTOR3 dTargetPos = targetPos - _staticVec2;
				_staticVec2 = targetPos;
				float dTargetLength = D3DXVec3Length(&dTargetPos);

				if (dTargetLength > 6.0f)
				{
					_staticFloat2 = _staticFloat1 == 0.0f ? dTargetLength / 0.5f : _staticFloat2;
					_staticFloat1 = dTargetLength;
					_staticVec3 = dTargetPos / dTargetLength;
				}

				if (_staticFloat1 > 0.0f)
				{
					_staticFloat1 = std::max(_staticFloat1 - _staticFloat2 * deltaTime, 0.0f);
					camPos = camPos - _staticFloat1 * _staticVec3;
				}*/

				if (_manager->GetPlayer()->GetRace()->GetGame()->CamProection() == 1)
					camera->FirtsViewIso(false);
				else
					camera->FirtsViewIso(true);

				
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

				_manager->GetObserverCoord(targetPos, _manager->_target.w, nullptr, _staticQuat1, dMPos, deltaTime,
					dragX, dragY, restoreY, &pos, &rot, &_staticFloat2);
				break;
			}
			}
			////////////////////////////////////////////////////////////////////////////////////////////////////////////
			if (ENABLE_SPLIT_SCREEN == true && _manager->GetSecondPlayer()) 
			{
				if (_manager->GetSecondPlayer()->GetRace()->GetGame()->CamLock() == true)
				{
					if (_manager->GetSecondPlayer()->GetRace()->GetGame()->GetPrefCamera() == 0)
						_manager->GetSecondPlayer()->SetIsometricView(false);
					else
						_manager->GetSecondPlayer()->SetIsometricView(true);
				}

				if (_manager->GetSecondPlayer()->GetIsometricView())
				//ISOMETRIC
				{
					float camWidth = camera->GetWidth() / 2;
					float camHeight = camWidth / camera->GetAspect();
					float camSize = sqrt(camWidth * camWidth + camHeight * camHeight);
					//left, right, top, bottom
					auto camBorder = D3DXVECTOR4(cIsoBorder, cIsoBorder, cIsoBorder / camera->GetAspect(),
						cIsoBorder / camera->GetAspect());

					D3DXVECTOR3 _statVec1 = NullVector;
					D3DXVECTOR3 _statVec2 = NullVector;
					if (_manager->_secondPlr)
						_statVec2 = _manager->_secondPlr && _manager->_secondPlr->GetCar().mapObj
						? _manager->_secondPlr->GetCar().grActor->GetWorldPos()
						: _manager->_secondPlr->GetCar().pos3;
					else
						_statVec2 = D3DXVECTOR3(_manager->_target);

					D3DXQUATERNION cIsoInvRot;
					D3DXQuaternionInverse(&cIsoInvRot, &cIsoRot);

					D3DXVECTOR3 isoDir;
					Vec3Rotate(XVector, cIsoRot, isoDir);
					D3DXVECTOR3 targOff = s_targetDir;
					targOff.z = 0.0f;
					Vec3Rotate(targOff, cIsoInvRot, targOff);
					//���������� �� ��������� ��������
					targOff.x = targOff.y;
					targOff.y = targOff.z;
					targOff.z = 0.0f;
					D3DXVec3Normalize(&targOff, &targOff);
					//
					float yTargetDot = D3DXVec3Dot(&targOff, &D3DXVECTOR3(0.0f, 1.0f, 0.0f));
					targOff *= camSize;

					if (abs(targOff.y) > 0.1f && abs(targOff.x / targOff.y) < camera->GetAspect())
					{
						float yCoord = ClampValue(targOff.y, -camBorder.w, camBorder.z);
						float xCoord = abs(yCoord) / yTargetDot;
						xCoord = sqrt(xCoord * xCoord - yCoord * yCoord);
						if (targOff.x < 0)
							xCoord = -xCoord;
						targOff.x = xCoord;
						targOff.y = yCoord;
					}
					else
					{
						float xCoord = ClampValue(targOff.x, -camBorder.x, camBorder.y);
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

					//�������� �������������� � ������� ������������
					Vec3Rotate(targOff, cIsoRot, targOff);

					D3DXVec3Lerp(&_statVec1, &_statVec1, &targOff, deltaTime);
					D3DXVECTOR3 camPos = s_targetPos + (-isoDir) * targDist + _statVec1;

					//���� �������� ��� ������ ������
					if (_manager->GetPlayer()->GetRace()->GetGame()->CamProection() == 1)
						camera->SecondViewIso(false);
					else
						camera->SecondViewIso(true);

					s_pos = camPos;
					s_rot = cIsoRot;
				}
				else
				//THIRD PERSON
				{
					float velocityLen = D3DXVec3Length(&s_targetVel);
					D3DXVECTOR3 velocity = s_targetDir + s_targetVel * 0.1f;
					//������ ����������. ������ ������...
					if (_manager->GetSecondPlayer() && _manager->GetSecondPlayer()->GetFinished() && _manager->GetSecondPlayer()->GetPlace() == 1)
						velocity = -s_targetDir + -s_targetVel * 0.1f;

					D3DXVec3Normalize(&velocity, &velocity);

					//������ ������� �������� ������������ ��������
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
					D3DXQUATERNION camQuat1 = s_rot;
					D3DXQUATERNION camQuat2 = velRot;
					D3DXQuaternionSlerp(&camQuat, &camQuat1, &camQuat2, 6.0f * deltaTime);

					D3DXVECTOR3 camDir;
					Vec3Rotate(XVector, camQuat, camDir);
					/* ��������� ������ � ����������� �������� �� ���������
					const float minSpeed = 0.0f;
					const float maxSpeed = 150.0f * 1000.0f / 3600.0f;
					float speedK = lsl::ClampValue((velocityLen - minSpeed) / (maxSpeed - minSpeed), 0.0f, 1.0f);
					const float minDist = 0.0f;
					const float maxDist = 1.5f;
					float distTarget = minDist + speedK * speedK * (maxDist - minDist);
					_staticFloat1 = _staticFloat1 + (distTarget - _staticFloat1) * lsl::ClampValue(deltaTime * 5.0f, 0.0f, 1.0f);
					*/

					//���������� ������ ������� �� FOV...

					float distance = 1.2f;
					if (CAM_FOV < 85)
						distance = 2.5f;
					else if (CAM_FOV > 95)
						distance = 0.4f;

					if (SPLIT_TYPE == 1)
						distance *= 2.5f;

					D3DXVECTOR3 camOff;
					Vec3Rotate(cCamTargetOff + D3DXVECTOR3(-1.0f, 0.0f, 0.0f), camQuat, camOff);
					D3DXVECTOR3 camPos = s_targetPos - camDir * distance + camOff;


					D3DXQUATERNION yRot;
					D3DXQuaternionRotationAxis(&yRot, &D3DXVECTOR3(0, 1, 0), D3DX_PI * 12.0f);
					camQuat = yRot * camQuat;

					camera->SecondViewIso(false);
					if (SPLIT_TYPE == 1)
						camera->SetUserFov(CAM_FOV / 2);
					else
						camera->SetUserFov(CAM_FOV);

					s_pos = camPos;
					s_rot = camQuat;					
				}
			}
		


			camera->SetPos(pos);
			camera->SetRot(rot);

			camera->SetFirstPos(pos);
			camera->SetFirstRot(rot);

			camera->SetSecondPos(s_pos);
			camera->SetSecondRot(s_rot);

			_manager->OrthoCullOffset();
			_manager->SyncLight();


			
			_manager->_world->GetGraph()->SetCubeViewPos(targetPos + ZVector * 1.0f);
			_manager->_world->GetGraph()->SetOrthoTarget(targetPos, targetSize);

			_manager->_world->GetGraph()->SetOrthoTargetSec(s_targetPos, s_targetSize);

			snd::Listener listener;
			listener.pos = targetPos;
			listener.rot = targetRot;
			if (_manager->GetPlayer() != nullptr && ENABLE_SPLIT_SCREEN == true)
			{
				//� ������ ��������� �������� �������� ����� � ��������� ���������������
				if (_manager->GetPlayer()->GetFinished() || _manager->GetPlayer()->GetCar().gameObj == nullptr)
				{
					listener.pos = s_targetPos;
					listener.rot = s_targetRot;
				}
			}

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
				Point pos = _manager->_world->GetControl()->GetMousePos();
				_staticVec2 = _staticVec1 = D3DXVECTOR3(static_cast<float>(pos.x), static_cast<float>(pos.y), 0);
			}
			//������ ����� ����������� ��������� �������� �������������� ������...
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
				Point pos = _manager->_world->GetControl()->GetMousePos();
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
					_staticVec2 = _manager->_player && _manager->_player->GetCar().mapObj
						              ? _manager->_player->GetCar().grActor->GetWorldPos()
						              : _manager->_player->GetCar().pos3;
				else
					_staticVec2 = D3DXVECTOR3(_manager->_target);
			}
		}

		void CameraManager::Control::SecTargetChanged()
		{
			if (_manager->_style == csIsometric)
			{
				_staticVec1 = NullVector;
				if (_manager->_secondPlr)
					_staticVec2 = _manager->_secondPlr && _manager->_secondPlr->GetCar().mapObj
					? _manager->_secondPlr->GetCar().grActor->GetWorldPos()
					: _manager->_secondPlr->GetCar().pos3;
				else
					_staticVec2 = D3DXVECTOR3(_manager->_target);
			}
		}

		D3DXVECTOR3 CameraManager::ScreenToWorld(const Point& coord, float z)
		{
			return _camera->GetContextInfo().ScreenToWorld(
				D3DXVECTOR2(static_cast<float>(coord.x), static_cast<float>(coord.y)), z,
				_world->GetView()->GetVPSize());
		}

		D3DXVECTOR2 CameraManager::WorldToScreen(const D3DXVECTOR3& coord)
		{
			return _camera->GetContextInfo().WorldToScreen(coord, _world->GetView()->GetVPSize());
		}

		void CameraManager::ScreenToRay(const Point& coord, D3DXVECTOR3& rayStart, D3DXVECTOR3& rayVec)
		{
			rayStart = ScreenToWorld(coord, 0);
			rayVec = ScreenToWorld(coord, 1) - rayStart;
			D3DXVec3Normalize(&rayVec, &rayVec);
		}

		bool CameraManager::ScreenPixelRayCastWithPlaneXY(const Point& coord, D3DXVECTOR3& outVec)
		{
			D3DXVECTOR3 rayStart;
			D3DXVECTOR3 rayVec;
			ScreenToRay(coord, rayStart, rayVec);

			return RayCastIntersectPlane(rayStart, rayVec, ZPlane, outVec);
		}

		void CameraManager::FlyTo(const D3DXVECTOR3& pos, const D3DXQUATERNION& rot, float time, bool newrot,
		                          bool newpos)
		{
			_flySPos = _camera->GetPos();
			_flySRot = _camera->GetRot();
			if (newpos == true)
				_flyPos = pos;
			else
				_flyPos = _flySPos;

			if (newrot == true)
				_flyRot = rot;
			else
				_flyRot = _flySRot;

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

			if (_style == csThirdPerson)
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
				if (_world->GetGame()->CamProection() == 1)
				{
					_camera->SetStyle(graph::csPerspective);
					_camera->SetWidth(28.0f * _world->GetGame()->GetCameraDistance());
				}
				else
				{
					_camera->SetStyle(graph::csOrtho);
					_camera->SetWidth(28.0f * _world->GetGame()->GetCameraDistance());
				}

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
				_camera->SetFov(65 * D3DX_PI / 180);

			if (_style == csIsometric && _world->GetGame()->CamProection() == 1)			
				_camera->SetFov(CAM_FOV * D3DX_PI / 180);
			
			if (ENABLE_SPLIT_SCREEN == false)
			{
				if (_style == csThirdPerson || _style == csInverseThird)
					_camera->SetFov(CAM_FOV * D3DX_PI / 180);
			}


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

			_world->GetGraph()->SetReflMappMode((_style == csIsometric || _style == csThirdPerson)
				                                    ? GraphManager::rmColorLayer
				                                    : GraphManager::rmLackLayer);
			_world->GetGraph()->SetShadowMaxFar(
				_style == csThirdPerson ? 60.0f : (_style == csIsometric ? 55.0f : 0.0f));
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
			_world->GetGame()->GetCameraDistance();
			if (Object::ReplaceRef(_player, value))
			{
				_player = value;

				_control->TargetChanged();
			}
		}

		Player* CameraManager::GetSecondPlayer()
		{
			return _secondPlr;
		}

		void CameraManager::SetSecondPlayer(Player* value)
		{
			_world->GetGame()->GetCameraDistance();
			if (Object::ReplaceRef(_secondPlr, value))
			{
				_secondPlr = value;

				_control->SecTargetChanged();
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

		void CameraManager::GetObserverCoord(const D3DXVECTOR3& targetPos, float targetDist, D3DXVECTOR3* pos,
		                                     D3DXQUATERNION& rot, const D3DXVECTOR2& dMPos, float deltaTime, bool dragX,
		                                     bool dragY, bool restoreY, D3DXVECTOR3* camPos, D3DXQUATERNION* camRot,
		                                     float* dir)
		{
			float camDist = targetDist;

			if (dragX || restoreY)
			{
				float dAngZ = deltaTime * _angleSpeed.x;
				if (dir != nullptr)
					dAngZ = dAngZ * (*dir);

				if (dragX)
				{
					dAngZ = ClampValue(dMPos.x * D3DX_PI * 0.001f, -D3DX_PI / 2, D3DX_PI / 2);
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

						if (dir != nullptr)
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
					float dAngY = ClampValue(-dMPos.y * D3DX_PI * 0.001f, -D3DX_PI / 2, D3DX_PI / 2);
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

			if (pos != nullptr)
			{
				D3DXVECTOR3 camDir;
				Vec3Rotate(XVector, rot, camDir);
				*pos = targetPos - camDir * camDist;
			}

			if (camRot != nullptr)
			{
				D3DXQuaternionSlerp(camRot, camRot, &rot, 6.0f * deltaTime);
			}

			if (camPos != nullptr && camRot != nullptr)
			{
				D3DXVECTOR3 camDir;
				Vec3Rotate(XVector, *camRot, camDir);
				*camPos = targetPos - camDir * camDist;
			}
		}
	}
}