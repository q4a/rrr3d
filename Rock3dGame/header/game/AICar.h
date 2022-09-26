#pragma once

#include "Player.h"

namespace r3d
{
	namespace game
	{
		class AICar : IGameUser
		{
			friend class AIDebug;
			friend class AISystem;
		public:
			using CarState = Player::CarState;
		private:
			using TrackVec = std::vector<bool>;

			//��������� ���������� ������ ����, ���������� ���� ��� ������
			struct PathState
			{
				PathState(unsigned trackCnt);
				~PathState();

				//����� ���������� ����������������� ������� ������� �� �������� �� track � �� target �������
				//res == true �������� �����
				//res = false �������
				bool FindFirstUnlockTrack(unsigned track, unsigned target, unsigned& res);
				bool FindLastUnlockTrack(unsigned track, unsigned target, unsigned& res);
				bool FindFirstSiblingUnlock(unsigned track, unsigned& res);
				bool FindLastSiblingUnlock(unsigned track, unsigned& res);
				//���������� movDir
				void ComputeMovDir(AICar* owner, float deltaTime, const Player::CarState& car);
				//Update - ��������� ��� ���������
				void Update(AICar* owner, float deltaTime, const Player::CarState& car);

				void SetCurTile(WayNode* value);
				void SetNextTile(WayNode* value);

				WayNode* curTile;
				WayNode* nextTile;
				WayNode* curNode;

				//��������� ������� �� ��������� ���������� ������ �������, ��� ��������
				TrackVec freeTracks;
				//��������������� �������
				TrackVec lockTracks;

				//���� ������ �� ����������� �������� ������
				float dirArea;
				//�������������� ����������� �������� ������������ ������
				D3DXVECTOR2 moveDir;
				bool _break;

				const unsigned cTrackCnt;
			};

			struct AttackState
			{
				AttackState();
				~AttackState();

				Player* TestEnemy(AICar* owner, const Player::CarState& car, int dir, Player* currentEnemy);
				static Player* FindEnemy(const AICar* owner, const Player::CarState& car, int dir,
				                         Player* currentEnemy);
				void ShotByEnemy(AICar* owner, const CarState& car, const Player* enemy) const;
				void RunHyper(const AICar* owner, const CarState& car, const PathState& path) const;
				void PlaceMine(const AICar* owner, const CarState& car, const PathState& path);

				void Update(AICar* owner, float deltaTime, const CarState& car, const PathState& path);
				void SetTarget(Player* value);
				void SetBackTarget(Player* value);

				Player* target;
				Player* backTarget;
				float placeMineRandom;
			};


			//��������� �������� �� ������� � ������ ����, ������, ��������
			struct ControlState
			{
				ControlState();

				void UpdateResetCar(AICar* owner, float deltaTime, const Player::CarState& car);
				void Update(AICar* owner, float deltaTime, const Player::CarState& car, const PathState& path);

				//���� �������� �����
				float steerAngle{};
				//����� � ��������������� ���������
				float timeBlocking;
				//�����������������
				bool blocking;
				//�������� �����
				bool backMovingMode;
				bool backMoving;
				float timeBackMoving;
				//����� �� ������ ��������������� ������
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
			~AICar() override;

			void OnProcessEvent(unsigned id, EventData* data) override;

			void OnProgress(float deltaTime);

			const CarState& GetCar() const;
			Logic* GetLogic() const;
			bool _enbAI;
		};
	}
}
