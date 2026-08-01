/*
 * video::Player with no filter graph.
 *
 * The three files that build a real DirectShow graph -- video.cpp,
 * playback.cpp, VideoPlayer.cpp -- are excluded from the non-MSVC build (see
 * this directory's parent CMakeLists), because they need the real dshow.h plus
 * Vmr9.h and Evr.h. This supplies the class they define so the game links.
 *
 * It is not a placeholder that pretends: STATE_NO_GRAPH is a state the game
 * already knows how to be in. World::IsVideoPlaying() is
 *
 *     _videoMode && _videoPlayer->state() != STATE_NO_GRAPH
 *
 * so reporting it means every cutscene is treated as finished the moment it
 * starts, the game moves straight on, and no branch anywhere is skipped or
 * short-circuited. Video is the one subsystem where "absent" is a configuration
 * the game shipped able to handle, which is why this is a stub and the audio
 * seam next door had to be a silent implementation instead.
 *
 * Phase 11 deletes this file and puts the three real ones back. The interface
 * does not move: World.h keeps its video::Player member and World.cpp keeps its
 * EC_* handler either way.
 */

#include "stdafx.h"
#include "video/VideoPlayer.h"

namespace r3d
{

namespace video
{

Player::Player(IVideoGraphUser* user): _dShowPlayer(0), _size(0, 0), _user(user), _fullScreen(false)
{
}

Player::~Player()
{
}

void Player::Initialize(HWND handle)
{
	LSL_LOG("video: no DirectShow graph in this build; cutscenes are skipped");
}

void Player::Finalize()
{
}

void Player::Open(const lsl::string& fileName)
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

/* The whole point of this file -- see the header comment. */
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

/*
 * False, meaning "not handled".
 *
 * World::OnPaint passes this on to the caller, and the caller repaints the
 * scene when the video did not. With a real graph the video renderer owns the
 * client area during playback and answers true; here nothing does, so the
 * engine must keep drawing.
 */
bool Player::OnPaint(HWND handle)
{
	return false;
}

void Player::DisplayModeChanged()
{
}

void Player::OnWMGraphEvent()
{
}

void CALLBACK Player::OnGraphEvent(HWND hwnd, long evCode, LONG_PTR param1, LONG_PTR param2)
{
}

}

}
