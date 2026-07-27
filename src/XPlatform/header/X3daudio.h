/*
 * X3DAudio declarations -- the subset src/Rock3dGame/snd/Audio.cpp uses to
 * pan positional sources, and nothing else.
 *
 * Like xaudio2.h beside it this is a compile-and-link seam, not an
 * implementation: X3DAudioCalculate produces a silent output matrix. FAudio
 * ships F3DAudio with exactly this API and is the intended replacement.
 */

#ifndef XPLATFORM_X3DAUDIO_H
#define XPLATFORM_X3DAUDIO_H

#ifdef _WIN32
#error "X3daudio.h here is the non-Windows substitute; Windows has its own"
#endif

#include "windows/windows_base.h"

#define X3DAUDIO_HANDLE_BYTESIZE   20
#define X3DAUDIO_SPEED_OF_SOUND    343.5f
#define X3DAUDIO_CALCULATE_MATRIX  0x00000001

typedef BYTE X3DAUDIO_HANDLE[X3DAUDIO_HANDLE_BYTESIZE];

/*
 * The Windows headers let a D3DXVECTOR3 be assigned straight into these
 * fields, which Audio.cpp relies on. The member template reproduces that for
 * any x/y/z-shaped type without making this header depend on D3DX.
 */
struct X3DAUDIO_VECTOR
{
    FLOAT x, y, z;

    template <class _Vec> X3DAUDIO_VECTOR(const _Vec& value): x(value.x), y(value.y), z(value.z) {}

    template <class _Vec> X3DAUDIO_VECTOR& operator=(const _Vec& value)
    {
        x = value.x;
        y = value.y;
        z = value.z;
        return *this;
    }
};

typedef struct X3DAUDIO_DISTANCE_CURVE_POINT
{
    FLOAT Distance;
    FLOAT DSPSetting;
} X3DAUDIO_DISTANCE_CURVE_POINT;

typedef struct X3DAUDIO_DISTANCE_CURVE
{
    X3DAUDIO_DISTANCE_CURVE_POINT* pPoints;
    UINT32                         PointCount;
} X3DAUDIO_DISTANCE_CURVE;

typedef struct X3DAUDIO_CONE
{
    FLOAT InnerAngle, OuterAngle;
    FLOAT InnerVolume, OuterVolume;
    FLOAT InnerLPF, OuterLPF;
    FLOAT InnerReverb, OuterReverb;
} X3DAUDIO_CONE;

typedef struct X3DAUDIO_LISTENER
{
    X3DAUDIO_VECTOR OrientFront;
    X3DAUDIO_VECTOR OrientTop;
    X3DAUDIO_VECTOR Position;
    X3DAUDIO_VECTOR Velocity;
    X3DAUDIO_CONE*  pCone;
} X3DAUDIO_LISTENER;

typedef struct X3DAUDIO_EMITTER
{
    X3DAUDIO_CONE*  pCone;
    X3DAUDIO_VECTOR OrientFront;
    X3DAUDIO_VECTOR OrientTop;
    X3DAUDIO_VECTOR Position;
    X3DAUDIO_VECTOR Velocity;
    FLOAT           InnerRadius;
    FLOAT           InnerRadiusAngle;
    UINT32          ChannelCount;
    FLOAT           ChannelRadius;
    FLOAT*          pChannelAzimuths;
    X3DAUDIO_DISTANCE_CURVE* pVolumeCurve;
    X3DAUDIO_DISTANCE_CURVE* pLFECurve;
    X3DAUDIO_DISTANCE_CURVE* pLPFDirectCurve;
    X3DAUDIO_DISTANCE_CURVE* pLPFReverbCurve;
    X3DAUDIO_DISTANCE_CURVE* pReverbCurve;
    FLOAT           CurveDistanceScaler;
    FLOAT           DopplerScaler;
} X3DAUDIO_EMITTER;

typedef struct X3DAUDIO_DSP_SETTINGS
{
    FLOAT* pMatrixCoefficients;
    FLOAT* pDelayTimes;
    UINT32 SrcChannelCount;
    UINT32 DstChannelCount;
    FLOAT  LPFDirectCoefficient;
    FLOAT  LPFReverbCoefficient;
    FLOAT  ReverbLevel;
    FLOAT  DopplerFactor;
    FLOAT  EmitterToListenerAngle;
    FLOAT  EmitterToListenerDistance;
    FLOAT  EmitterVelocityComponent;
    FLOAT  ListenerVelocityComponent;
} X3DAUDIO_DSP_SETTINGS;

void X3DAudioInitialize(UINT32 SpeakerChannelMask, FLOAT SpeedOfSound, X3DAUDIO_HANDLE Instance);

void X3DAudioCalculate(const X3DAUDIO_HANDLE Instance, const X3DAUDIO_LISTENER* pListener,
    const X3DAUDIO_EMITTER* pEmitter, UINT32 Flags, X3DAUDIO_DSP_SETTINGS* pDSPSettings);

#endif /* XPLATFORM_X3DAUDIO_H */
