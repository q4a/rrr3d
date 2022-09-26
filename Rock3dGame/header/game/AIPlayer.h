#pragma once

#include "AICar.h"

#include "TraceGfx.h"

namespace r3d
{
	namespace game
	{
		class AIPlayer : public Object
		{
			friend class AIDebug;
		private:
			Player* _player;
			AICar* _car;
		public:
			AIPlayer(Player* player);
			~AIPlayer() override;

			void OnProgress(float deltaTime) const;

			void CreateCar();
			void FreeCar();

			World* GetWorld() const;
			Player* GetPlayer() const;
			AICar* GetCar() const;
		};

		class AISystem : public Object
		{
		public:
			using PlayerList = List<AIPlayer*>;
		private:
			Race* _race;
			PlayerList _playerList;

			AIDebug* _aiDebug;

			template <class _Iter>
			void LockChainTrack(const _Iter& iter1, const _Iter& iter2, AICar* ignoreCar, unsigned track);
			void ComputeTracks(float deltaTime);
		public:
			AISystem(Race* race);
			~AISystem() override;

			void OnProgress(float deltaTime);

			AIPlayer* AddPlayer(Player* player);
			void DelPlayer(AIPlayer* value);
			void ClearPlayerList();
			AIPlayer* FindAIPlayer(Player* player) const;
			const PlayerList& GetPlayerList() const;

			void CreateDebug(AIPlayer* player);
			void FreeDebug();
		};

		class AIDebug : public Object
		{
		private:
			class GrActor : public graph::BaseSceneNode
			{
			private:
				AIDebug* _debug;
				ID3DXFont* _font{};
			protected:
				void DoRender(graph::Engine& engine) override;
			public:
				GrActor(AIDebug* debug);
				~GrActor() override;
			};

			class Control : public ControlEvent
			{
			private:
				AIDebug* _debug;

				bool OnHandleInput(const InputMessage& msg) override;
			public:
				Control(AIDebug* debug);
			};

		private:
			AISystem* _ai;
			AIPlayer* _aiPlayer;

			TraceGfx* _traceGfx;
			GrActor* _grActor;
			Control* _control;
		public:
			AIDebug(AISystem* ai, AIPlayer* aiPlayer);
			~AIDebug() override;

			void OnProgress(float deltaTime) const;
			void TestProcess();

			World* GetWorld() const;
			AIPlayer* GetAI() const;

			using Chain = List<unsigned>;
			using ChainList = List<Chain>;
			ChainList chainList;
		};
	}
}
