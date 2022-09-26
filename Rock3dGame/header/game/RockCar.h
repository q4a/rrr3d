#ifndef R3D_GAME_ROCKCAR
#define R3D_GAME_ROCKCAR

#include "Weapon.h"
#include "GameCar.h"

namespace r3d
{
	namespace game
	{
		class RockCar : public GameCar
		{
		private:
			using _MyBase = GameCar;
		public:
			class Weapons : public MapObjects
			{
			public:
				using _MyBase = MapObjects;
			private:
				RockCar* _owner;
				Weapon* _hyperDrive;
				Weapon* _mines;
			protected:
				void InsertItem(const Value& value) override
				{
					_MyBase::InsertItem(value);

					Weapon* weapon = &value->GetGameObj<Weapon>();

					if (weapon->GetDesc().Front().type == Proj::ptHyper && ReplaceRef(_hyperDrive, weapon))
						_hyperDrive = weapon;

					if (weapon->GetDesc().Front().type == Proj::ptMine && ReplaceRef(_mines, weapon))
						_mines = weapon;
				}

				void RemoveItem(const Value& value) override
				{
					_MyBase::RemoveItem(value);

					Weapon* weapon = &value->GetGameObj<Weapon>();

					if (_hyperDrive == weapon)
						SafeRelease(_hyperDrive);

					if (_mines == weapon)
						SafeRelease(_mines);
				}

			public:
				Weapons(RockCar* owner): _MyBase(owner), _owner(owner), _hyperDrive(nullptr), _mines(nullptr)
				{
				}

				~Weapons() override
				{
					SafeRelease(_mines);
					SafeRelease(_hyperDrive);
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
			void SaveSource(SWriter* writer) override;
			void LoadSource(SReader* reader) override;
			//
			void SaveProxy(SWriter* writer) override;
			void LoadProxy(SReader* reader) override;
		public:
			RockCar();
			~RockCar() override;

			void OnProgress(float deltaTime) override;

			Weapons& GetWeapons() const;
		};
	}
}

#endif
