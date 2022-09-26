#pragma once

#include "IEdit.h"

#include "edit/DataBase.h"
#include "edit/Map.h"
#include "edit/SceneControl.h"

namespace r3d
{
	namespace edit
	{
		class Edit final : public IEdit
		{
			friend game::World;
		private:
			game::World* _world;

			SceneControl* _scControl;
			DataBase* _db;
			Map* _map;

			void OnUpdateLevel() const;
		public:
			explicit Edit(game::World* world);
			~Edit() override;

			game::World* GetWorld() const;
			IDataBase* GetDB() override;
			IMap* GetMap() override;
			ISceneControl* GetScControl() override;
			void Destroy(Object* object) const override;
		};
	}
}
