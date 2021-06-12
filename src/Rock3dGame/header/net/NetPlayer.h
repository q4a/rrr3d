#pragma once

#include "game\Race.h"
#include "NetBase.h"

namespace r3d
{

namespace game
{

class GameMode;
class NetGame;
class NetPlayer;
class Map;

/*class NetPlayerResponse: public net::NetModelRPC<NetPlayerResponse>
{
public:
	struct DescData
	{
		BYTE playerId;

		DescData() {}
		DescData(BYTE mPlayerId): playerId(mPlayerId) {}

		void Read(std::istream& stream)
		{
			net::Read(stream, this, sizeof(DescData));
		}

		void Write(std::ostream& stream)
		{
			net::Write(stream, this, sizeof(DescData));
		}
	};
private:
	NetPlayer* _player;
protected:
	virtual void OnDescWrite(const net::NetMessage& msg, std::ostream& stream);
	virtual void OnSerialize(const net::NetMessage& msg, net::BitStream& stream);
public:
	NetPlayerResponse(const Desc& desc);
};*/

class NetPlayer: public net::NetModelRPC<NetPlayer>
{
	//friend NetPlayerResponse;
private:
	struct ShotSlots
	{
		BYTE hyperdrive: 1;
		BYTE mine: 1;
		BYTE wpn1: 1;
		BYTE wpn2: 1;
		BYTE wpn3: 1;
		BYTE wpn4: 1;

		BYTE reserved: 2;

		ShotSlots(): hyperdrive(0), mine(0), wpn1(0), wpn2(0), wpn3(0), wpn4(4), reserved(0) {}

		bool Get(unsigned index) const
		{
			switch (index)
			{
			case 0:
				return hyperdrive;
			case 1:
				return mine;
			case 2:
				return wpn1;
			case 3:
				return wpn2;
			case 4:
				return wpn3;
			case 5:
				return wpn4;
			}

			return false;
		}

		void Set(unsigned index, bool val)
		{
			switch (index)
			{
			case 0:
				hyperdrive = val;
				break;
			case 1:
				mine = val;
				break;
			case 2:
				wpn1 = val;
				break;
			case 3:
				wpn2 = val;
				break;
			case 4:
				wpn3 = val;
				break;
			case 5:
				wpn4 = val;
				break;
			}
		}

		bool Get(Player::SlotType slot) const
		{
			return Get((unsigned)(slot - Player::stHyper));
		}

		void Set(Player::SlotType slot, bool val)
		{
			Set((unsigned)(slot - Player::stHyper), val);
		}

		Player::SlotType AsWpn(unsigned i) const
		{
			return (Player::SlotType)(Player::stHyper + i);
		}

		unsigned count() const
		{
			return 6;
		}
	};

	typedef std::vector<D3DXVECTOR3> CoordList;
public:
	struct DescData
	{
		BYTE playerId;
		unsigned netSlot;

		DescData() {}
		DescData(BYTE mPlayerId, unsigned mNetSlot): playerId(mPlayerId), netSlot(mNetSlot) {}

		void Read(std::istream& stream)
		{
			net::Read(stream, this, sizeof(DescData));
		}

		void Write(std::ostream& stream)
		{
			net::Write(stream, this, sizeof(DescData));
		}
	};
private:
	NetGame* _net;
	Player* _player;
	HumanPlayer* _human;
	AIPlayer* _aiPlayer;
	//NetPlayerResponse* _response;

	//refactor, flag for save
	bool _modelOwner;
	float _dAlpha;
	bool _raceReady;
	bool _raceGoWait;
	bool _raceFinish;

	void DoRaceReady(bool ready);
	void DoRaceGoWait(bool goWait);
	void DoRaceFinish(bool finish);
	
	void DoShot(MapObj* target, ShotSlots& slots, unsigned projId, CoordList* coordList, bool readMode);
	void SendColor(const D3DXCOLOR& value, bool failed, unsigned target);
	void SendGamerId(int value, bool failed, unsigned target);

	void SlotsWrite(std::ostream& stream);
	void SlotsRead(std::istream& stream);	

	void OnSetGamerId(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	void OnSetColor(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	void OnSetCar(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	void OnCarSlotsChanged(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	void OnRaceReady(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	void OnRaceGoWait(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	void OnRaceFinish(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	void OnShot(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	void OnTakeBonus(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	void OnMineContact1(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	void OnMineContact2(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);

	void ControlStream(const net::NetMessage& msg, net::BitStream& stream);
	void ResponseStream(const net::NetMessage& msg, net::BitStream& stream);
protected:
	virtual void OnDescWrite(const net::NetMessage& msg, std::ostream& stream);
	virtual void OnStateRead(const net::NetMessage& msg, const net::NetCmdHeader& header, std::istream& stream);
	virtual void OnStateWrite(const net::NetMessage& msg, std::ostream& stream);
	virtual void OnSerialize(const net::NetMessage& msg, net::BitStream& stream);
public:
	NetPlayer(const Desc& desc);
	virtual ~NetPlayer();

	void Process(float deltaTime);

	void Shot(MapObj* target, Player::SlotType type);
	void Shot(MapObj* target);

	void TakeBonus(MapObj* bonus, GameObject::BonusType type, float value);
	void MineContact(Proj* sender, const D3DXVECTOR3& point);

	int GetGamerId() const;
	void SetGamerId(int value);
	bool CheckGamerId(int value) const;
	int GenerateGamerId() const;

	const D3DXCOLOR& GetColor() const;	
	void SetColor(const D3DXCOLOR& value);	
	bool CheckColor(const D3DXCOLOR& value) const;
	D3DXCOLOR GenerateColor() const;

	Garage::Car* GetCar();
	bool BuyCar(Garage::Car* car);

	void CarSlotsChanged();
	void RaceStarted();

	bool IsRaceReady();
	void RaceReady(bool ready);

	bool IsRaceGoWait();
	void RaceGoWait(bool goWait);

	bool IsRaceFinish();
	void RaceFinish(bool finish);

	GameMode* game();
	Race* race();
	Map* map();
	Player* model();
};

}

}