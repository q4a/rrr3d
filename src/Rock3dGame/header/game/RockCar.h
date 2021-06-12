#ifndef R3D_GAME_ROCKCAR
#define R3D_GAME_ROCKCAR

#include "Weapon.h"
#include "GameCar.h"

namespace r3d
{

namespace game
{

class RockCar: public GameCar
{
private:
	typedef GameCar _MyBase;
public:
	class Weapons: public MapObjects
	{
	public:
		typedef MapObjects _MyBase;
	private:
		RockCar* _owner;
		Weapon* _hyperDrive;
		Weapon* _mines;
	protected:
		virtual void InsertItem(const Value& value)
		{
			_MyBase::InsertItem(value);

			Weapon* weapon = &value->GetGameObj<Weapon>();

			if (weapon->GetDesc().Front().type == Proj::ptHyper && ReplaceRef(_hyperDrive, weapon))
				_hyperDrive = weapon;

			if (weapon->GetDesc().Front().type == Proj::ptMine && ReplaceRef(_mines, weapon))
				_mines = weapon;
		}
		virtual void RemoveItem(const Value& value)
		{
			_MyBase::RemoveItem(value);

			Weapon* weapon = &value->GetGameObj<Weapon>();

			if (_hyperDrive == weapon)
				lsl::SafeRelease(_hyperDrive);

			if (_mines == weapon)
				lsl::SafeRelease(_mines);
		}
	public:
		Weapons(RockCar* owner): _MyBase(owner), _owner(owner), _hyperDrive(0), _mines(0) {}
		~Weapons()
		{
			lsl::SafeRelease(_mines);
			lsl::SafeRelease(_hyperDrive);
		}

		MapObj& Add(MapObjRec* record)
		{
			return _MyBase::Add(record);
		}

		Weapon* GetHyperDrive()
		{
			return _hyperDrive;
		}
		Weapon* GetMines()
		{
			return _mines;
		}
	};	
private:
	Weapons* _weapons;
protected:
	virtual void SaveSource(lsl::SWriter* writer);
	virtual void LoadSource(lsl::SReader* reader);
	//
	virtual void SaveProxy(lsl::SWriter* writer);
	virtual void LoadProxy(lsl::SReader* reader);
public:
	RockCar();
	virtual ~RockCar();
	
	virtual void OnProgress(float deltaTime);

	Weapons& GetWeapons();
};

}

}

#endif