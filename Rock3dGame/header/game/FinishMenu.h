#pragma once

#include "MenuSystem.h"

namespace r3d
{
	namespace game
	{
		namespace n
		{
			class FinishMenu : public MenuFrame, ControlEvent
			{
			private:
				enum Label { mlName, mlPrice, mlPriceInfo, mlPriceVal, cLabelEnd };

				struct Box
				{
					gui::PlaneFon* leftFrame;
					gui::PlaneFon* rightFrame;
					gui::PlaneFon* lineFrame;
					gui::PlaneFon* photo;
					gui::PlaneFon* cup;

					gui::Label* labels[cLabelEnd];

					float duration;
					int plrId;
				};

				static const int cBoxCount = 3;
			private:
				Box _boxes[cBoxCount]{};
				unsigned _playerCount;
				float _time;
			protected:
				void OnShow(bool value) override;
				void OnAdjustLayout(const D3DXVECTOR2& vpSize) override;
				void OnInvalidate() override;

				bool OnMouseClickEvent(const MouseClick& mClick) override;
				bool OnHandleInput(const InputMessage& msg) override;
			public:
				FinishMenu(Menu* menu, gui::Widget* parent);
				~FinishMenu() override;

				void OnProgress(float deltaTime);
			};
		}
	}
}
