#ifndef XPLATFORM_DSHOW_H
#define XPLATFORM_DSHOW_H

/*
 * DirectShow, to the depth the game's *headers* need -- which is much less than
 * the depth its .cpp files need, and that asymmetry is the whole design here.
 *
 * src/Rock3dGame/header/video/playback.h holds IGraphBuilder, IMediaControl and
 * IMediaEventEx only as pointer members, so incomplete types are enough. The
 * files that actually build a filter graph -- video.cpp, playback.cpp,
 * VideoPlayer.cpp -- need the real interfaces plus Vmr9.h and Evr.h, and they
 * are excluded from the non-MSVC build until phase 11 rather than being given
 * declarations that lead nowhere.
 *
 * What that buys: World.h can keep `video::Player* _videoPlayer`, World.cpp and
 * GameMode.cpp keep their EC_* switch arms, and nothing about the game's video
 * plumbing has to be edited or #ifdef'd out. Phase 11 fills in the
 * implementation behind an interface that never moved.
 *
 * The game tolerates video being absent -- World::IsVideoMode() treats a
 * missing graph as no cutscene -- so this is the one subsystem where "declared
 * but not linked" is a state the game can actually run in.
 */

#include "xplatform.h"

typedef const wchar_t* PCWSTR;

typedef LONG_PTR OAHWND;
typedef double REFTIME;

/*
 * Filter graph event codes. World.cpp:383-389 and GameMode.cpp:1931-1933 switch
 * on these three and no others, so those three are what is defined -- a code
 * nothing handles should stay a compile error rather than become a number.
 * Values from the DirectShow event-code list.
 */
#define EC_COMPLETE     0x01
#define EC_USERABORT    0x02
#define EC_ERRORABORT   0x03

/* Held only as pointers by playback.h, so incomplete is enough and correct:
   it keeps every COM vtable out of the 52 translation units that include
   World.h, which is the same reason the PhysX 2.8 shim keeps its backend out
   of Rock3dGame's. */
struct IGraphBuilder;
struct IMediaControl;
struct IMediaEventEx;
struct IVideoWindow;
struct IBaseFilter;
struct IPin;
struct IEnumPins;

#endif
