#pragma once

#include "MenuSystem.h"

namespace r3d
{
	namespace game
	{
		class FinalMenu : public MenuFrame
		{
		private:
			enum Label { mlCredits, cLabelEnd };

			enum MenuItem { miOk, cMenuItemEnd };

			struct LineBox
			{
				gui::Label* caption;
				gui::Label* text;
			};

			using LineBoxes = Vector<LineBox>;

			struct Slide
			{
				gui::PlaneFon* plane;
			};

			using Slides = Vector<Slide>;
		private:
			gui::Button* _menuItems[cMenuItemEnd]{};
			gui::Widget* _linesRoot;
			LineBoxes _lineBoxes;
			Slides _slides;

			float _time;
			float _linesSizeY;

			void AddLineBox(const string& caption, const string& text);
			void AddSlide(const string& image);
			void DeleteAll();
		protected:
			void OnShow(bool value) override;
			void OnAdjustLayout(const D3DXVECTOR2& vpSize) override;
			void OnInvalidate() override;

			bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick) override;
		public:
			FinalMenu(Menu* menu, gui::Widget* parent);
			~FinalMenu() override;

			void OnProgress(float deltaTime);
		};

		/*class FinalMenu: public MenuFrame
		{
		private:
			enum Label {mlCredits, cLabelEnd};
			enum MenuItem {miOk, cMenuItemEnd};
		private:
			gui::Label* _labels[cLabelEnd];
			gui::Button* _menuItems[cMenuItemEnd];
			gui::PlaneFon* _bg;
		
			float _scrollTime;
		protected:
			virtual void OnShow(bool value);
			virtual void OnAdjustLayout(const D3DXVECTOR2& vpSize);
			virtual void OnInvalidate();
		
			bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);
		public:
			FinalMenu(Menu* menu, gui::Widget* parent);
			virtual ~FinalMenu();
		
			void OnProgress(float deltaTime);
		};*/
	}
}
