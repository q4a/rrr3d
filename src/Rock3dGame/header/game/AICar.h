#pragma once

#include "Player.h"

namespace r3d
{

namespace game
{

class AICar: IGameUser
{
	friend class AIDebug;
	friend class AISystem;
public:
	typedef Player::CarState CarState;
private:
	typedef std::vector<bool> TrackVec;

	//Описывает результаты поиска пути, показатели пути для машины
	struct PathState
	{
		PathState(unsigned trackCnt);
		~PathState();

		//Поиск достижимой незаблокированной дорожки начиная со следющей за track и до target включая
		//res == true успешный поиск
		//res = false неудача
		bool FindFirstUnlockTrack(unsigned track, unsigned target, unsigned& res);
		bool FindLastUnlockTrack(unsigned track, unsigned target, unsigned& res);
		bool FindFirstSiblingUnlock(unsigned track, unsigned& res);
		bool FindLastSiblingUnlock(unsigned track, unsigned& res);
		//Вычисление movDir
		void ComputeMovDir(AICar* owner, float deltaTime, const Player::CarState& car);
		//Update - обновляет все состояние
		void Update(AICar* owner, float deltaTime, const Player::CarState& car);

		void SetCurTile(WayNode* value);
		void SetNextTile(WayNode* value);	

		WayNode* curTile;
		WayNode* nextTile;
		WayNode* curNode;

		//Свободные дорожки на некотором расстояние обгона впереди, для движения
		TrackVec freeTracks;
		//Заблокированные дорожки
		TrackVec lockTracks;
		
		//зона охвата по направлению движения машины
		float dirArea;
		//результирующее направление движения относительно машины
		D3DXVECTOR2 moveDir;
		bool _break;

		const unsigned cTrackCnt;
	};

	struct AttackState
	{
		AttackState();
		~AttackState();

		Player* TestEnemy(AICar* owner, const Player::CarState& car, int dir, Player* currentEnemy);
		Player* FindEnemy(AICar* owner, const Player::CarState& car, int dir, Player* currentEnemy);
		void ShotByEnemy(AICar* owner, const CarState& car, Player* enemy);
		void RunHyper(AICar* owner, const CarState& car, const PathState& path);
		void PlaceMine(AICar* owner, const CarState& car, const PathState& path);

		void Update(AICar* owner, float deltaTime, const CarState& car, const PathState& path);
		void SetTarget(Player* value);
		void SetBackTarget(Player* value);

		Player* target;
		Player* backTarget;
		float placeMineRandom;
	};


	//Состояние контроля за машиной с учетом пути, обгона, стрельбы
	struct ControlState
	{
		ControlState();

		void UpdateResetCar(AICar* owner, float deltaTime, const Player::CarState& car);
		void Update(AICar* owner, float deltaTime, const Player::CarState& car, const PathState& path);

		//угол поворота колес
		float steerAngle;
		//время в заблокирвоанном состоянии
		float timeBlocking;
		//заблокированность
		bool blocking;
		//движение назад
		bool backMovingMode;
		bool backMoving;
		float timeBackMoving;
		//время до сброса заблокирвоанной машины
		float timeResetBlockCar;
	};

	static const float cSteerAngleBias;
	static const float cMaxSpeedBlocking;
	static const float cMaxTimeBlocking;

	static const float cMaxVisibleDistShot;
private:
	Player* _player;

	AttackState _attack;
	PathState _path;
	ControlState _control;	

	void UpdateAI(float deltaTime, const Player::CarState& car);	
public:
	AICar(Player* player);
	virtual ~AICar();

	virtual void OnProcessEvent(unsigned id, EventData* data);

	void OnProgress(float deltaTime);

	const CarState& GetCar();
	Logic* GetLogic();
	bool _enbAI;
};

}

}