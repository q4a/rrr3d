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
		glm::vec3 _staticVec1;
		glm::vec3 _staticVec2;
		glm::vec3 _staticVec3;
		glm::quat _staticQuat1;

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
	glm::vec4 _clampAngle;
	glm::vec3 _angleSpeed;
	glm::vec3 _stableAngle;
	glm::vec3 _lastFreePos;
	glm::quat _lastFreeRot;

	glm::vec3 _flySPos;
	glm::quat _flySRot;
	glm::vec3 _flyPos;
	glm::quat _flyRot;
	float _flyCurTime;
	float _flyTime;

	graph::Camera* _camera;
	Player* _player;
	GraphManager::LightSrc* _light;
	glm::vec4 _target;

	void OrthoCullOffset();
	void SyncLight();
public:
	CameraManager(World* world);
	~CameraManager();

	//z - координата глубины относительно zNear. [0..1] <--> [zNear..zFar]
	glm::vec3 ScreenToWorld(const lsl::Point& coord, float z);
	glm::vec2 WorldToScreen(const glm::vec3& coord);
	void ScreenToRay(const lsl::Point& coord, glm::vec3& rayStart, glm::vec3& rayVec);
	bool ScreenPixelRayCastWithPlaneXY(const lsl::Point& coord, glm::vec3& outVec);

	void FlyTo(const glm::vec3& pos, const glm::quat& rot, float time);
	void StopFly();
	bool InFly();

	//x,y - minAngleZ, maxAngleZ
	//z,w - minAngleY, maxAngleY
	const glm::vec4& GetClampAngle() const;
	void SetClampAngle(const glm::vec4& value);

	const glm::vec3& GetAngleSpeed();
	void SetAngleSpeed(const glm::vec3& value);

	const glm::vec3& GetStableAngle();
	void SetStableAngle(const glm::vec3& value);

	Style GetStyle() const;
	void ChangeStyle(Style value);

	float GetAspect() const;
	void SetAspect(float value);

	glm::vec3 GetPos() const;
	glm::vec3 GetDir() const;
	glm::vec3 GetRight() const;

	float GetNear() const;
	void SetNear(float value);

	float GetFar() const;
	void SetFar(float value);

	Player* GetPlayer();
	void SetPlayer(Player* value);

	const glm::vec4& GetTarget();
	void SetTarget(const glm::vec4& value);

	GraphManager::LightSrc* GetLight();
	void SetLight(GraphManager::LightSrc* value);

	void GetObserverCoord(const glm::vec3& targetPos, float targetDist, glm::vec3* pos, glm::quat& rot, const glm::vec2& dMPos, float deltaTime, bool dragX, bool dragY, bool restoreY, glm::vec3* camPos, glm::quat* camQuat, float* dir);
};

}

}