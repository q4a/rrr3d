#ifndef XPLATFORM_MMSYSTEM_H
#define XPLATFORM_MMSYSTEM_H

/*
 * The slice of <mmsystem.h> the game reaches for. It is three things: the two
 * timer-resolution calls in World::SetTimeResolution, and WAVEFORMATEX.
 *
 * Nothing here is a stub standing in for work that is owed. The timer calls
 * genuinely have nothing to do off Windows -- see below -- and WAVEFORMATEX is
 * a data layout, not behaviour.
 *
 * Deliberately absent: `#define PlaySound PlaySoundA`. The real header has it,
 * and it means gui Menu::PlaySound is really named PlaySoundA on Windows. That
 * is invisible here because both the declaration and every call go through the
 * same headers in any one build, and no mangled C++ name crosses platforms.
 * Nothing calls Win32's PlaySound, so the macro would be all hazard and no use.
 */

#include "xplatform.h"

typedef UINT MMRESULT;

#define TIMERR_NOERROR  0

#ifdef __cplusplus
extern "C" {
#endif

/*
 * On Windows these raise and lower the global scheduler tick so that Sleep and
 * the multimedia timers become accurate to `period` milliseconds; the default
 * is around 15.6ms, which is too coarse for a 60Hz step. World asks for it
 * around the race loop and gives it back afterwards.
 *
 * There is no equivalent knob here and none is wanted. macOS timers are
 * already fine-grained -- XPlatform's Sleep is nanosleep and GetTickCount is
 * steady_clock -- so the condition these calls exist to create is the one that
 * holds by default. Accepting and ignoring the period is the accurate
 * behaviour, not a placeholder for it.
 */
MMRESULT timeBeginPeriod(UINT period);
MMRESULT timeEndPeriod(UINT period);

#ifdef __cplusplus
}
#endif

#define WAVE_FORMAT_PCM  1

/* Packed to 2 bytes on Windows, and it matters: this is a wire format. The
   16-bit fields would otherwise be padded and cbSize would land at the wrong
   offset. Audio.cpp:1182 fills one in for a submix voice. */
#pragma pack(push, 1)
typedef struct tWAVEFORMATEX
{
	WORD  wFormatTag;
	WORD  nChannels;
	DWORD nSamplesPerSec;
	DWORD nAvgBytesPerSec;
	WORD  nBlockAlign;
	WORD  wBitsPerSample;
	WORD  cbSize;
} WAVEFORMATEX, *PWAVEFORMATEX, *LPWAVEFORMATEX;
#pragma pack(pop)

#endif
