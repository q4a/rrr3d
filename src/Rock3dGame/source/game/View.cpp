#include "stdafx.h"
#include "game\View.h"

#include "game\World.h"

namespace r3d
{

namespace game
{
	
View::View(World* world, const Desc& desc): _world(world), _desc(desc)
{
}

void View::Reset(const Desc& desc2)
{
	if (!_world->GetGraph())
		return;

	_desc = desc2;
	lsl::Point resolution = _desc.resolution;
	bool fullscreen = _desc.fullscreen;

	if (resolution.x == 0 && resolution.y == 0)
	{
		graph::DisplayMode dispMode;
		graph::DisplayMode screenMode = _world->GetGraph()->GetScreenMode();

		if (_world->GetGraph()->FindNearMode(lsl::Point(graph::RenderDriver::cOptimalResWidth, graph::RenderDriver::cOptimalResHeight), dispMode, screenMode.height != 0 ? screenMode.width/(float)screenMode.height : 0.0f))
		{
			_desc.resolution.x = resolution.x = dispMode.width;
			_desc.resolution.y = resolution.y = dispMode.height;
		}
	}

	if (_world->GetVideoMode() && fullscreen)
	{
		graph::DisplayMode screenMode = _world->GetGraph()->GetScreenMode();
		resolution = lsl::Point(screenMode.width, screenMode.height);
		fullscreen = false;
	}
	else if (fullscreen)
	{
		graph::DisplayMode mode;
		if (_world->GetGraph()->FindNearMode(resolution, mode))
			_desc.resolution = resolution = lsl::Point(mode.width, mode.height);
	}
	
	if (_world->GetEdit() == NULL)
	{
		SetWindowLong(_desc.handle, GWL_STYLE, _desc.fullscreen ? WS_POPUP | WS_VISIBLE : WS_OVERLAPPEDWINDOW | WS_VISIBLE);
		SetWindowLong(_desc.handle, GWL_EXSTYLE, _desc.fullscreen ? WS_EX_TOPMOST : 0);
	}

	if (_world->GetEdit() == NULL)
	{
		SetWindowSize(_desc.handle, resolution, _desc.fullscreen);
	}

	_world->OnReset(_desc.handle, resolution, fullscreen);
}

bool View::OnMouseClickEvent(MouseKey key, KeyState state, const Point& coord, bool shift, bool ctrl)
{
	if (!_world->GetGraph())
		return false;

	_mClick.key = key;
	_mClick.state = state;
	_mClick.shift1 = shift;
	_mClick.coord = ScreenToView(coord);
	_mClick.projCoord = ViewToProj(_mClick.coord);
	_world->GetCamera()->ScreenToRay(coord, _mClick.scrRayPos, _mClick.scrRayVec);

	return _world->OnMouseClickEvent(_mClick);
}

bool View::OnMouseMoveEvent(const Point& coord, bool shift, bool ctrl)
{
	if (!_world->GetGraph())
		return false;

	lsl::Point newCoord = ScreenToView(coord);

	_mMove.shift1 = shift;
	_mMove.dtCoord = newCoord - _mMove.coord;
	_mMove.offCoord = newCoord - _mClick.coord;
	_mMove.coord = newCoord;
	_mMove.projCoord = ViewToProj(_mMove.coord);

	_mMove.click = _mClick;
	_world->GetCamera()->ScreenToRay(coord, _mMove.scrRayPos, _mMove.scrRayVec);	

	return _world->OnMouseMoveEvent(_mMove);
}
	
bool View::OnKeyEvent(unsigned key, KeyState state, bool repeat)
{
	if (!_world->GetGraph())
		return false;

	return _world->OnKeyEvent(key, state, repeat);
}

void View::OnKeyChar(unsigned key, lsl::KeyState state, bool repeat)
{
	if (!_world->GetGraph())
		return;

	_world->OnKeyChar(key, state, repeat);
}

const View::Desc& View::GetDesc()
{
	return _desc;
}

float View::GetAspect() const
{
	return _desc.resolution.x / static_cast<float>(_desc.resolution.y);
}

lsl::Point View::GetWndSize() const
{
	RECT rc;
	::GetClientRect(_desc.handle, &rc);
	//::GetWindowRect(_desc.handle, &rc);

	return lsl::Point(rc.right - rc.left, rc.bottom - rc.top);
}

D3DXVECTOR2 View::GetVPSize() const
{
	graph::Engine& engine = _world->GetGraph()->GetEngine();

	return D3DXVECTOR2(static_cast<float>(engine.GetParams().BackBufferWidth), static_cast<float>(engine.GetParams().BackBufferHeight));
}

lsl::Point View::ScreenToView(const lsl::Point& point)
{
	lsl::Point wndRc = GetWndSize();
	const D3DPRESENT_PARAMETERS& params = _world->GetGraph()->GetEngine().GetParams();
	D3DXVECTOR2 wndSize = D3DXVECTOR2(static_cast<float>(wndRc.x), static_cast<float>(wndRc.y));	

	D3DXVECTOR2 viewSize = GetVPSize();

	return Point(static_cast<int>(Round(point.x * viewSize.x / wndSize.x)), static_cast<int>(Round(point.y * viewSize.y / wndSize.y)));
}

D3DXVECTOR2 View::ViewToProj(const lsl::Point& point)
{
	return graph::CameraCI::ViewToProj(D3DXVECTOR2(static_cast<float>(point.x), static_cast<float>(point.y)), GetVPSize());
}

D3DXVECTOR2 View::ProjToView(const D3DXVECTOR2& coord)
{
	return graph::CameraCI::ProjToView(coord, GetVPSize());
}

float View::GetCameraAspect() const
{
	return _world->GetCamera()->GetAspect();
}

void View::SetCameraAspect(float value)
{
	_world->GetCamera()->SetAspect(value);
}

lsl::Point View::GetMousePos() const
{
	return _mMove.coord;
}

void View::SetWindowSize(HWND handle, const lsl::Point& size, bool fullScreen)
{
	lsl::Point wndSize = size;

	if (!fullScreen)
	{
		WINDOWINFO wndInfo;
		GetWindowInfo(handle, &wndInfo);		

		RECT rect;
		rect.left = rect.top = 0;
		rect.right = wndSize.x;
		rect.bottom = wndSize.y;
		AdjustWindowRect(&rect, wndInfo.dwStyle, false);

		wndSize.x = rect.right - rect.left;
		wndSize.y = rect.bottom - rect.top;
	}

	LSL_LOG(lsl::StrFmt("res %d %d", wndSize.x, wndSize.y));

	SetWindowPos(handle, 0, 0, 0, wndSize.x, wndSize.y, 0);
}

}

}