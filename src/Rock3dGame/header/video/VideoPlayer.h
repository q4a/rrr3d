#pragma once

#include <dshow.h>
#include "playback.h"

/* Global scope, because that is where xaudio2.h declares it -- inside
   namespace r3d this would be a different, incomplete type. */
struct IXAudio2;

namespace r3d
{

/* Likewise a forward declaration: this header is included by World.h and must
   not drag the graphics headers in behind it. */
namespace graph { class Engine; }

namespace video
{

#ifndef _WIN32
/* The macOS player's state, which has nothing to do with a filter graph. Held
   by pointer so this header stays free of CoreMedia and Direct3D. */
struct PlayerImpl;
#endif

class Player
{
private:
	DShowPlayer *_dShowPlayer;
	lsl::Point _size;
	IVideoGraphUser* _user;
	bool _fullScreen;

	static void CALLBACK OnGraphEvent(HWND hwnd, long evCode, LONG_PTR param1, LONG_PTR param2);
public:
#ifndef _WIN32
	/*
	 * The macOS player drives itself, because World::MainProgress returns early
	 * while _videoMode is set -- with DirectShow the video renderer owned the
	 * client area and painted it, so the engine deliberately stopped rendering.
	 * Nothing then calls Present, and Player::OnPaint is only reached from
	 * World::OnPaint, which on Windows came from WM_PAINT and here comes from
	 * nowhere.
	 *
	 * So World calls this once a frame instead, from exactly that early-return
	 * point, and it decodes and presents. Returns false when the film has
	 * finished, which is what stands in for EC_COMPLETE.
	 */
	bool Progress(float deltaTime);

	/*
	 * The engine's XAudio2 and its Direct3D device, handed over by World once
	 * both exist. Passed in rather than reached for so this stays a leaf: the
	 * soundtrack goes through the same output the game already uses instead of
	 * opening a second device, and the picture goes through the same D3D9
	 * device rather than contending with d9mt for the Metal layer.
	 */
	void SetOutputs(IXAudio2* xaudio, graph::Engine* engine);
#endif

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

#ifndef _WIN32
private:
	PlayerImpl* _impl = nullptr;
#endif
};

}

}