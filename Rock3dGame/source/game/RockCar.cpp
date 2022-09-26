#include "stdafx.h"

#include "game//Logic.h"
#include "game//RockCar.h"

namespace r3d
{
	namespace game
	{
		RockCar::RockCar()
		{
			_weapons = new Weapons(this);
		}

		RockCar::~RockCar()
		{
			Destroy();

			delete _weapons;
		}

		void RockCar::SaveSource(SWriter* writer)
		{
			_MyBase::SaveSource(writer);
		}

		void RockCar::LoadSource(SReader* reader)
		{
			_MyBase::LoadSource(reader);
		}

		void RockCar::SaveProxy(SWriter* writer)
		{
			_MyBase::SaveProxy(writer);

			writer->WriteValue("weapons", _weapons);
		}

		void RockCar::LoadProxy(SReader* reader)
		{
			_MyBase::LoadProxy(reader);

			reader->ReadValue("weapons", _weapons);
		}

		void RockCar::OnProgress(float deltaTime)
		{
			_MyBase::OnProgress(deltaTime);

			_weapons->OnProgress(deltaTime);
		}

		RockCar::Weapons& RockCar::GetWeapons() const
		{
			return *_weapons;
		}
	}
}
