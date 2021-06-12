#pragma once

#include "AICar.h"

#include "TraceGfx.h"

namespace r3d
{

namespace game
{

class AIPlayer: public Object
{
	friend class AIDebug;
private:
	Player* _player;
	AICar* _car;
public:
	AIPlayer(Player* player);
	virtual ~AIPlayer();

	void OnProgress(float deltaTime);

	void CreateCar();
	void FreeCar();

	World* GetWorld();
	Player* GetPlayer();
	AICar* GetCar();
};

class AISystem: public Object
{
public:
	typedef lsl::List<AIPlayer*> PlayerList;
private:
	Race* _race;
	PlayerList _playerList;

	AIDebug* _aiDebug;

	template<class _Iter> void LockChainTrack(const _Iter& iter1, const _Iter& iter2, AICar* ignoreCar, unsigned track);
	void ComputeTracks(float deltaTime);
public:
	AISystem(Race* race);
	virtual ~AISystem();

	void OnProgress(float deltaTime);

	AIPlayer* AddPlayer(Player* player);
	void DelPlayer(AIPlayer* value);
	void ClearPlayerList();
	AIPlayer* FindAIPlayer(Player* player);
	const PlayerList& GetPlayerList() const;

	void CreateDebug(AIPlayer* player);
	void FreeDebug();
};

class AIDebug: public Object
{
private:
	class GrActor: public graph::BaseSceneNode
	{
	private:
		AIDebug* _debug;
		ID3DXFont* _font;
	protected:
		virtual void DoRender(graph::Engine& engine);
	public:
		GrActor(AIDebug* debug);
		virtual ~GrActor();
	};

	class Control: public ControlEvent
	{
	private:
		AIDebug* _debug;

		virtual bool OnHandleInput(const InputMessage& msg);		
	public:
		Control(AIDebug* _debug);
	};
private:
	AISystem* _ai;
	AIPlayer* _aiPlayer;
	
	TraceGfx* _traceGfx;
	GrActor* _grActor;
	Control* _control;	
public:
	AIDebug(AISystem* ai, AIPlayer* aiPlayer);
	~AIDebug();

	void OnProgress(float deltaTime);
	void TestProcess();

	World* GetWorld();
	AIPlayer* GetAI();

	typedef lsl::List<unsigned> Chain;
	typedef lsl::List<Chain> ChainList;
	ChainList chainList;
};

}

}