#include "stdafx.h"

#include "video/VideoPlayer.h"

#ifdef _WIN32

namespace r3d
{

namespace video
{

Player::Player(IVideoGraphUser* user): _user(user), _dShowPlayer(NULL), _size(0, 0), _fullScreen(false)
{
}

Player::~Player()
{
	Finalize();
}

void CALLBACK Player::OnGraphEvent(HWND hwnd, long evCode, LONG_PTR param1, LONG_PTR param2)
{
	switch (evCode)
	{
	case EC_COMPLETE:
	case EC_USERABORT:
		//g_pPlayer->Stop();
		break;

	case EC_ERRORABORT:
		//NotifyError(hwnd, L"Playback error.");
		//g_pPlayer->Stop();
		break;
	}
}

void Player::Initialize(HWND handle)
{
	if (_dShowPlayer != NULL)
		return;

	LSL_LOG("Player::Initialize");

	_dShowPlayer = new DShowPlayer(handle, WM_GRAPH_EVENT);
}

void Player::Finalize()
{
	if (_dShowPlayer == NULL)
		return;

	LSL_LOG("Player::Finalize");

	lsl::SafeDelete(_dShowPlayer);
}

void Player::Open(const lsl::string& fileName)
{
	LSL_ASSERT(_dShowPlayer);

	LSL_LOG("Player::Open opening=" + fileName);

	HRESULT hr = _dShowPlayer->OpenFile(lsl::GetAppFilePath(fileName).c_str());

	if (hr == S_OK)
	{
		if (_fullScreen)
			_dShowPlayer->SetFullScreen(_fullScreen);
		UpdateVideoWindow(_size);
	}

	LSL_LOG(lsl::StrFmt("Player::Open complete hr=%d", hr));
}

void Player::Unload()
{
	LSL_ASSERT(_dShowPlayer);

	LSL_LOG("Player::Unload");

	_dShowPlayer->TearDownGraph();

	LSL_LOG("Player::Unload complete");
}

void Player::Play()
{
	LSL_ASSERT(_dShowPlayer);

	HRESULT hr = _dShowPlayer->Play();

	if (hr != S_OK)
	{
		LSL_LOG(lsl::StrFmt("Player::Play failed hr=%d", hr));
	}
}

void Player::Pause()
{
	LSL_ASSERT(_dShowPlayer);

	HRESULT hr = _dShowPlayer->Pause();

	if (hr != S_OK)
	{
		LSL_LOG(lsl::StrFmt("Player::Pause failed hr=%d", hr));
	}
}

void Player::Stop()
{
	LSL_ASSERT(_dShowPlayer);

	HRESULT hr = _dShowPlayer->Stop();

	if (hr != S_OK)
	{
		LSL_LOG(lsl::StrFmt("Player::Stop failed hr=%d", hr));
	}
}

PlaybackState Player::state() const
{
	LSL_ASSERT(_dShowPlayer);

	return _dShowPlayer->State();
}

bool Player::GetFullScreen() const
{
	return _fullScreen;
}

void Player::SetFullScreen(bool value)
{
	if (_fullScreen != value)
	{
		_fullScreen = value;

		if (_dShowPlayer)
			_dShowPlayer->SetFullScreen(value);
	}
}

void Player::UpdateVideoWindow(const lsl::Point& size)
{
	LSL_ASSERT(_dShowPlayer);

	_size = size;

	RECT prc;
	prc.left = 0;
	prc.top = 0;
	prc.right = size.x;
	prc.bottom = size.y;
	HRESULT hr = _dShowPlayer->UpdateVideoWindow(&prc);

	if (hr != S_OK)
	{
		LSL_LOG(lsl::StrFmt("Player::UpdateVideoWindow failed hr=%d", hr));
	}
}

bool Player::OnPaint(HWND handle)
{
	LSL_ASSERT(_dShowPlayer);

	if (_dShowPlayer->State() != STATE_NO_GRAPH && _dShowPlayer->HasVideo())
	{
		PAINTSTRUCT ps;
		HDC hdc;
		hdc = BeginPaint(handle, &ps);

		//The player has video, so ask the player to repaint. 
		HRESULT hr = _dShowPlayer->Repaint(hdc);

		if (hr != S_OK)
		{
			LSL_LOG(lsl::StrFmt("Player::Repaint failed hr=%d", hr));
		}

		EndPaint(handle, &ps);

		return true;
	}

	return false;
}

void Player::DisplayModeChanged()
{
	LSL_ASSERT(_dShowPlayer);

	HRESULT hr = _dShowPlayer->DisplayModeChanged();

	if (hr != S_OK)
	{
		LSL_LOG(lsl::StrFmt("Player::DisplayModeChanged failed hr=%d", hr));
	}
}

void Player::OnWMGraphEvent()
{
	LSL_ASSERT(_dShowPlayer);

	_dShowPlayer->HandleGraphEvent(_user);
}

}

}

#else

/*
 * Cutscene playback stands in for DirectShow until the video port lands. Every
 * call succeeds and reports STATE_NO_GRAPH, which is exactly the state
 * World::IsVideoMode() already treats as "no cutscene playing" -- so the game
 * skips cutscenes rather than stalling on them.
 */

namespace r3d
{

namespace video
{

Player::Player(IVideoGraphUser* user): _dShowPlayer(NULL), _size(0, 0), _user(user), _fullScreen(false)
{
}

Player::~Player()
{
}

void CALLBACK Player::OnGraphEvent(HWND, long, LONG_PTR, LONG_PTR)
{
}

void Player::Initialize(HWND)
{
}

void Player::Finalize()
{
}

void Player::Open(const lsl::string&)
{
}

void Player::Unload()
{
}

void Player::Play()
{
}

void Player::Pause()
{
}

void Player::Stop()
{
}

PlaybackState Player::state() const
{
	return STATE_NO_GRAPH;
}

bool Player::GetFullScreen() const
{
	return _fullScreen;
}

void Player::SetFullScreen(bool value)
{
	_fullScreen = value;
}

void Player::UpdateVideoWindow(const lsl::Point& size)
{
	_size = size;
}

bool Player::OnPaint(HWND)
{
	//False means "not handled", so the caller paints the frame itself.
	return false;
}

void Player::DisplayModeChanged()
{
}

void Player::OnWMGraphEvent()
{
}

}

}

#endif /* _WIN32 */
