#pragma once

#include <dshow.h>
#include "playback.h"

namespace r3d
{

namespace video
{

class Player
{
private:
	DShowPlayer *_dShowPlayer;
	lsl::Point _size;
	IVideoGraphUser* _user;
	bool _fullScreen;

	static void CALLBACK OnGraphEvent(HWND hwnd, long evCode, LONG_PTR param1, LONG_PTR param2);
public:
	Player(IVideoGraphUser* user);
	~Player();

	void Initialize(HWND handle);
	void Finalize();

	void Open(const lsl::string& fileName);
	void Unload();

	void Play();
	void Pause();
	void Stop();
	PlaybackState state() const;

	bool GetFullScreen() const;
	void SetFullScreen(bool value);

	void UpdateVideoWindow(const lsl::Point& size);
	bool OnPaint(HWND handle);
	void DisplayModeChanged();
	void OnWMGraphEvent();
};

}

}