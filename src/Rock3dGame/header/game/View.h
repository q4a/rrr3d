#pragma once

#include "IView.h"
#include "ControlManager.h"

namespace r3d
{

namespace game
{

//Представляет окно рендера
//Также отвечает за увеомления об изменении экранного разрешения
class View: public IView
{
private:
	World* _world;
	Desc _desc;	

	MouseClick _mClick;
	MouseMove _mMove;
public:
	View(World* world, const Desc& desc);

	void Reset(const Desc& desc);	

	bool OnMouseClickEvent(MouseKey key, KeyState state, const Point& coord, bool shift, bool ctrl);
	bool OnMouseMoveEvent(const Point& coord, bool shift, bool ctrl);
	bool OnKeyEvent(unsigned key, KeyState state, bool repeat);
	void OnKeyChar(unsigned key, lsl::KeyState state, bool repeat);

	const Desc& GetDesc();
	float GetAspect() const;
	lsl::Point GetWndSize() const;	
	D3DXVECTOR2 GetVPSize() const;

	//из экранного в пространство ViewPort
	//в терминах движка, пространство ViewPort и будет далее являться экранным
	lsl::Point ScreenToView(const lsl::Point& point);
	//из ViewPort в проекционноое пространство
	D3DXVECTOR2 ViewToProj(const lsl::Point& point);
	//
	D3DXVECTOR2 ProjToView(const D3DXVECTOR2& coord);

	float GetCameraAspect() const;
	void SetCameraAspect(float value);

	//получить координаты мыши, в пространство ViewPort
	lsl::Point GetMousePos() const;

	static void SetWindowSize(HWND handle, const lsl::Point& size, bool fullScreen);
};

}

}