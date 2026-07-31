#ifndef XPLATFORM_X3DAUDIO_H
#define XPLATFORM_X3DAUDIO_H

/*
 * X3DAudio, declared to the surface src/Rock3dGame/source/snd/Audio.cpp uses.
 *
 * Declarations only. Nothing is implemented yet -- phase 11 supplies that over
 * FAudio's F3DAudio, whose F3DAudioInitialize and F3DAudioCalculate correspond
 * one for one with the two functions below. Until then the missing symbols at
 * link time are the accurate statement of where the port is.
 *
 * The layouts here are not free choices. X3DAudioCalculate fills structures the
 * game reads, so a field in the wrong place is silently wrong audio rather than
 * a compile error, and phase 11's implementation has to agree with whatever is
 * written here.
 */

#include "xplatform.h"

/* d3d9types.h, for D3DVECTOR. See the X3DAUDIO_VECTOR note below. */
#include "directx/d3d9types.h"

typedef float FLOAT32;

/*
 * A deviation from Wine, and the game is what settles it.
 *
 * Wine's include/x3daudio.h declares X3DAUDIO_VECTOR as its own struct
 * { float x, y, z; }, which is reasonable for Wine -- it keeps x3daudio.h from
 * depending on d3d9types.h. Microsoft's SDK header typedefs it to D3DVECTOR
 * instead, so that it interoperates with D3DX math without conversions.
 *
 * Audio.cpp requires the SDK's choice. At :1691-1695 and :1699-1701 it assigns
 * D3DXVECTOR3 values -- listener->pos, _pos3d, XVector, ZVector -- directly
 * into Position, OrientFront and OrientTop. D3DXVECTOR3 derives from D3DVECTOR,
 * so those are base-class assignments and compile; against a standalone struct
 * there is no conversion at all and they would not. That code compiles on
 * Windows today, which makes this a fact about the header rather than a
 * preference. Same three floats either way, so the ABI is unaffected.
 */
typedef D3DVECTOR X3DAUDIO_VECTOR;

#define X3DAUDIO_HANDLE_BYTESIZE  20
typedef BYTE X3DAUDIO_HANDLE[X3DAUDIO_HANDLE_BYTESIZE];

/* Metres per second, dry air at 20C. Passed to X3DAudioInitialize at
   Audio.cpp:2061 and not otherwise used. */
#define X3DAUDIO_SPEED_OF_SOUND  343.5f

#define X3DAUDIO_CALCULATE_MATRIX          0x00000001
#define X3DAUDIO_CALCULATE_DELAY           0x00000002
#define X3DAUDIO_CALCULATE_LPF_DIRECT      0x00000004
#define X3DAUDIO_CALCULATE_LPF_REVERB      0x00000008
#define X3DAUDIO_CALCULATE_REVERB          0x00000010
#define X3DAUDIO_CALCULATE_DOPPLER         0x00000020
#define X3DAUDIO_CALCULATE_EMITTER_ANGLE   0x00000040

typedef struct X3DAUDIO_DISTANCE_CURVE_POINT
{
	FLOAT32 Distance;
	FLOAT32 DSPSetting;
} X3DAUDIO_DISTANCE_CURVE_POINT, *LPX3DAUDIO_DISTANCE_CURVE_POINT;

typedef struct X3DAUDIO_DISTANCE_CURVE
{
	X3DAUDIO_DISTANCE_CURVE_POINT* pPoints;
	UINT32 PointCount;
} X3DAUDIO_DISTANCE_CURVE, *LPX3DAUDIO_DISTANCE_CURVE;

typedef struct X3DAUDIO_CONE
{
	FLOAT32 InnerAngle;
	FLOAT32 OuterAngle;
	FLOAT32 InnerVolume;
	FLOAT32 OuterVolume;
	FLOAT32 InnerLPF;
	FLOAT32 OuterLPF;
	FLOAT32 InnerReverb;
	FLOAT32 OuterReverb;
} X3DAUDIO_CONE, *LPX3DAUDIO_CONE;

typedef struct X3DAUDIO_LISTENER
{
	X3DAUDIO_VECTOR OrientFront;
	X3DAUDIO_VECTOR OrientTop;
	X3DAUDIO_VECTOR Position;
	X3DAUDIO_VECTOR Velocity;
	X3DAUDIO_CONE* pCone;
} X3DAUDIO_LISTENER, *LPX3DAUDIO_LISTENER;

/* Audio.cpp zeroes the whole struct at :1631 and then sets ChannelCount,
   CurveDistanceScaler, Position, OrientFront, OrientTop and pChannelAzimuths.
   Everything else stays zero, which is why the full layout matters even though
   the game never names most of it. */
typedef struct X3DAUDIO_EMITTER
{
	X3DAUDIO_CONE* pCone;
	X3DAUDIO_VECTOR OrientFront;
	X3DAUDIO_VECTOR OrientTop;
	X3DAUDIO_VECTOR Position;
	X3DAUDIO_VECTOR Velocity;
	FLOAT32 InnerRadius;
	FLOAT32 InnerRadiusAngle;
	UINT32 ChannelCount;
	FLOAT32 ChannelRadius;
	FLOAT32* pChannelAzimuths;
	X3DAUDIO_DISTANCE_CURVE* pVolumeCurve;
	X3DAUDIO_DISTANCE_CURVE* pLFECurve;
	X3DAUDIO_DISTANCE_CURVE* pLPFDirectCurve;
	X3DAUDIO_DISTANCE_CURVE* pLPFReverbCurve;
	X3DAUDIO_DISTANCE_CURVE* pReverbCurve;
	FLOAT32 CurveDistanceScaler;
	FLOAT32 DopplerScaler;
} X3DAUDIO_EMITTER, *LPX3DAUDIO_EMITTER;

typedef struct X3DAUDIO_DSP_SETTINGS
{
	FLOAT32* pMatrixCoefficients;
	FLOAT32* pDelayTimes;
	UINT32 SrcChannelCount;
	UINT32 DstChannelCount;
	FLOAT32 LPFDirectCoefficient;
	FLOAT32 LPFReverbCoefficient;
	FLOAT32 ReverbLevel;
	FLOAT32 DopplerFactor;
	FLOAT32 EmitterToListenerAngle;
	FLOAT32 EmitterToListenerDistance;
	FLOAT32 EmitterVelocityComponent;
	FLOAT32 ListenerVelocityComponent;
} X3DAUDIO_DSP_SETTINGS, *LPX3DAUDIO_DSP_SETTINGS;

#ifdef __cplusplus
extern "C" {
#endif

void X3DAudioInitialize(UINT32 SpeakerChannelMask, FLOAT32 SpeedOfSound,
                        X3DAUDIO_HANDLE Instance);

void X3DAudioCalculate(const X3DAUDIO_HANDLE Instance,
                       const X3DAUDIO_LISTENER* pListener,
                       const X3DAUDIO_EMITTER* pEmitter,
                       UINT32 Flags,
                       X3DAUDIO_DSP_SETTINGS* pDSPSettings);

#ifdef __cplusplus
}
#endif

#endif
