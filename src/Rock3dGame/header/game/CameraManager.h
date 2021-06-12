#pragma once

#include "ICameraManager.h"
#include "ControlManager.h"
#include "Player.h"

namespace r3d
{

namespace game
{
	
class CameraManager: public ICameraManager
{
	friend class World;
private:
	class Control: public ControlEvent
	{
	private:
		CameraManager* _manager;
		float _flyAlpha;

		float _staticFloat1;
		float _staticFloat2;
		D3DXVECTOR3 _staticVec1;
		D3DXVECTOR3 _staticVec2;
		D3DXVECTOR3 _staticVec3;
		D3DXQUATERNION _staticQuat1;		

		bool OnMouseMoveEvent(const MouseMove& mMove);
		bool OnHandleInput(const InputMessage& msg);
		void OnInputFrame(float deltaTime);
	public:
		Control(CameraManager* manager);

		void StyleChanged(Style style, Style newStyle);
		void FlyStart();
		void FlyCompleted();
		void TargetChanged();
	};
private:
	World* _world;
	Control* _control;
	Style _style;
	float _near;
	float _far;
	D3DXVECTOR4 _clampAngle;
	D3DXVECTOR3 _angleSpeed;
	D3DXVECTOR3 _stableAngle;
	D3DXVECTOR3 _lastFreePos;
	D3DXQUATERNION _lastFreeRot;

	D3DXVECTOR3 _flySPos;
	D3DXQUATERNION _flySRot;
	D3DXVECTOR3 _flyPos;
	D3DXQUATERNION _flyRot;
	float _flyCurTime;
	float _flyTime;

	graph::Camera* _camera;	
	Player* _player;
	GraphManager::LightSrc* _light;	
	D3DXVECTOR4 _target;

	void OrthoCullOffset();
	void SyncLight();	
public:
	CameraManager(World* world);
	~CameraManager();

	//z - координата глубины относительно zNear. [0..1] <--> [zNear..zFar]
	D3DXVECTOR3 ScreenToWorld(const lsl::Point& coord, float z);
	D3DXVECTOR2 WorldToScreen(const D3DXVECTOR3& coord);
	void ScreenToRay(const lsl::Point& coord, D3DXVECTOR3& rayStart, D3DXVECTOR3& rayVec);
	bool ScreenPixelRayCastWithPlaneXY(const lsl::Point& coord, D3DXVECTOR3& outVec);

	void FlyTo(const D3DXVECTOR3& pos, const D3DXQUATERNION& rot, float time);
	void StopFly();
	bool InFly();

	//x,y - minAngleZ, maxAngleZ
	//z,w - minAngleY, maxAngleY
	const D3DXVECTOR4& GetClampAngle() const;
	void SetClampAngle(const D3DXVECTOR4& value);

	const D3DXVECTOR3& GetAngleSpeed();
	void SetAngleSpeed(const D3DXVECTOR3& value);

	const D3DXVECTOR3& GetStableAngle();
	void SetStableAngle(const D3DXVECTOR3& value);

	Style GetStyle() const;	
	void ChangeStyle(Style value);

	float GetAspect() const;
	void SetAspect(float value);

	D3DXVECTOR3 GetPos() const;
	D3DXVECTOR3 GetDir() const;
	D3DXVECTOR3 GetRight() const;

	float GetNear() const;
	void SetNear(float value);

	float GetFar() const;
	void SetFar(float value);

	Player* GetPlayer();
	void SetPlayer(Player* value);

	const D3DXVECTOR4& GetTarget();
	void SetTarget(const D3DXVECTOR4& value);

	GraphManager::LightSrc* GetLight();
	void SetLight(GraphManager::LightSrc* value);

	void GetObserverCoord(const D3DXVECTOR3& targetPos, float targetDist, D3DXVECTOR3* pos, D3DXQUATERNION& rot, const D3DXVECTOR2& dMPos, float deltaTime, bool dragX, bool dragY, bool restoreY, D3DXVECTOR3* camPos, D3DXQUATERNION* camQuat, float* dir);
};

}

}