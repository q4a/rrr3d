#pragma once

namespace r3d
{
	namespace game
	{
		class IView
		{
		public:
			struct Desc
			{
				HWND handle;
				Point resolution;
				bool fullscreen;
			};

		public:
			virtual void Reset(const Desc& desc) = 0;

			virtual D3DXVECTOR2 ViewToProj(const Point& point) = 0;
			virtual D3DXVECTOR2 ProjToView(const D3DXVECTOR2& coord) = 0;

			virtual bool OnMouseClickEvent(MouseKey key, KeyState state, const Point& coord, bool shift, bool ctrl) = 0;
			virtual bool OnMouseMoveEvent(const Point& coord, bool shift, bool ctrl) = 0;
			virtual bool OnKeyEvent(unsigned key, KeyState state, bool repeat) = 0;
			virtual void OnKeyChar(unsigned key, KeyState state, bool repeat) = 0;

			virtual const Desc& GetDesc() = 0;
			virtual Point GetWndSize() const = 0;
			virtual D3DXVECTOR2 GetVPSize() const = 0;

			//Часть интерфейса камеры. Только для пользователей
			virtual float GetCameraAspect() const = 0;
			virtual void SetCameraAspect(float value) = 0;

			virtual Point ScreenToView(const Point& point) = 0;
		};
	}
}
