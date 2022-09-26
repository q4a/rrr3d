#pragma once

#include "IView.h"
#include "ControlManager.h"

namespace r3d
{
	namespace game
	{
		//������������ ���� �������
		//����� �������� �� ���������� �� ��������� ��������� ����������
		class View : public IView
		{
		private:
			World* _world;
			Desc _desc;

			MouseClick _mClick;
			MouseMove _mMove;
		public:
			View(World* world, const Desc& desc);

			void Reset(const Desc& desc) override;

			bool OnMouseClickEvent(MouseKey key, KeyState state, const Point& coord, bool shift, bool ctrl) override;
			bool OnMouseMoveEvent(const Point& coord, bool shift, bool ctrl) override;
			bool OnKeyEvent(unsigned key, KeyState state, bool repeat) override;
			void OnKeyChar(unsigned key, KeyState state, bool repeat) override;

			const Desc& GetDesc() override;
			float GetAspect() const;
			Point GetWndSize() const override;
			D3DXVECTOR2 GetVPSize() const override;

			//�� ��������� � ������������ ViewPort
			//� �������� ������, ������������ ViewPort � ����� ����� �������� ��������
			Point ScreenToView(const Point& point) override;
			//�� ViewPort � ������������� ������������
			D3DXVECTOR2 ViewToProj(const Point& point) override;
			//
			D3DXVECTOR2 ProjToView(const D3DXVECTOR2& coord) override;

			float GetCameraAspect() const override;
			void SetCameraAspect(float value) override;

			//�������� ���������� ����, � ������������ ViewPort
			Point GetMousePos() const;

			static void SetWindowSize(HWND handle, const Point& size, bool fullScreen);
		};
	}
}
