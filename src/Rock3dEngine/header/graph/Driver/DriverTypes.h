#ifndef DRIVER_TYPES
#define DRIVER_TYPES

#include "windows.h"
#include "d3d9.h" //При абсолютной абстракции этого здесь быть не должно
#include <bitset>

#include "lslContainer.h"
#include "lslUtility.h"

namespace r3d
{

namespace graph
{

enum MemoryPool{mpDefault, mpManaged, mpSystem, mpScratch, MEMORY_POOL_END};

enum RenderState {rsZEnable, rsFillMode, rsShadeMode, rsZWriteEnable, rsAlphaTestEnable, rsLastPixel, rsSrcBlend, rsDestBlend, rsCullMode, rsZFunc, rsAlphaRef, rsAlphaFunc, rsDitherEnable, rsAlphaBlendEnable, rsFogEnable, rsSpecularEnable, rsFogColor, rsFogTableMode, rsFogStart, rsFogEnd, rsFogDensity, rsRangeFogEnable, rsStencilEnable, rsStencilFail, rsStencilZFail, rsStencilPass, rsStencilFunc, rsStencilRef, rsStencilMask, rsStencilWriteMask, rsTextureFactor, rsWrap0, rsWrap1, rsWrap2, rsWrap3, rsWrap4, rsWrap5, rsWrap6, rsWrap7, rsClipping, rsLighting, rsAmbient, rsFogVertexMode, rsColorVertex, rsLocalViewer, rsNormalizeNormals, rsDiffuseMaterialSource, rsSpecularMaterialSource, rsAmbientMaterialSource, rsEmissiveMaterialSource, rsVertexBlend, rsClipPlaneEnable, rsPointSize, rsPointSizeMin, rsPointSpriteEnable, rsPointScaleEnable, rsPointScaleA, rsPointScaleB, rsPointScaleC, rsMultiSampleAntialias, rsMultiSampleMask, rsPatchedStyle, rsDebugMonitorToken, rsPointSizeMax, rsIndexedVertexBlendEnable, rsColorWriteEnable, rsTweenFactor, rsBlendOp, rsPositionDegree, rsNormalDegree, rsScissorTestEnable, rsSlopesScaleDepthBias, rsAntialiasedLineEnable, rsMinTessellationLevel, rsMaxTessellationLevel, rsAdaptiveTessX, rsAdaptiveTessY, rsAdaptiveTessZ, rsAdaptiveTessW, rsEnableAdaptiveTesselation, rsTwoSideStencilMode, rsCCWStencilFail, rsCCWStencilZFail, rsCCWStencilPass, rsCCWStencilFunc, rsColorWriteEnable1, rsColorWriteEnable2, rsColorWriteEnable3, rsBlendFactor, rsSRGBWriteEnable, rsDepthBias, rsWrap8, rsWrap9, rsWrap10, rsWrap11, rsWrap12, rsWrap13, rsWrap14, rsWrap15, rsSeparateAlphaBlendEnable, rsSrcBlendAlpha, rsDestBlendAlpha, rsBlendOpAlpha, RENDER_STATE_END};

enum SamplerState {ssAddressU, ssAddressV, ssAddressW, ssBorderColor, ssMagFilter, ssMinFilter, ssMipFilter, ssMipMapLodBias, ssMaxMipLevel, ssMaxAnisotropy, ssSRGBTexture, ssElementIndex, ssDMapOffset, SAMPLER_STATE_END};

enum TextureStageState {tssColorOp, tssColorArg1, tssColorArg2, tssAlphaOp, tssAlphaArg1, tssAlphaArg2, tssBumpEnvMat00, tssBumpEnvMat01, tssBumpEnvMap10, tssBumpEnvMap11, tssTexCoordIndex, tssBumpEnvlScale, tssBumpEnvlOffset, tssTextureTransformFlags, tssColorArg0, tssAlphaArg0, tssResultArg, tssConstant, TEXTURE_STAGE_STATE_END};

enum TransformStateType {tstWorld, tstView, tstProj, tstTexture0, tstTexture1, tstTexture2, tstTexture3, tstTexture4, tstTexture5, tstTexture6, tstTexture7, TRANSFORM_STATE_TYPE_END};

struct PresentParameters
{
	UINT                BackBufferWidth;
    UINT                BackBufferHeight;
    D3DFORMAT           BackBufferFormat;
    UINT                BackBufferCount;

    D3DMULTISAMPLE_TYPE MultiSampleType;
    DWORD               MultiSampleQuality;

    D3DSWAPEFFECT       SwapEffect;
    HWND                hDeviceWindow;
    BOOL                Windowed;
    BOOL                EnableAutoDepthStencil;
    D3DFORMAT           AutoDepthStencilFormat;
    DWORD               Flags;

    /* FullScreen_RefreshRateInHz must be zero for Windowed mode */
    UINT                FullScreen_RefreshRateInHz;
    UINT                PresentationInterval;
};

struct DisplayMode
{
	UINT            width;
	UINT            height;
	UINT            refreshRate;
	D3DFORMAT       format;
};
typedef lsl::Vector<DisplayMode> DisplayModes;

}

}

#endif