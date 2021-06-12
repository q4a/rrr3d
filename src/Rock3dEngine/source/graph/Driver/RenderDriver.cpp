#include "stdafx.h"

#ifndef RENDER_DRIVER_INL
#define RENDER_DRIVER_INL

#include "graph\driver\RenderDriver.h"

namespace r3d
{

namespace graph
{

const unsigned RenderDriver::cMinResWidth = 1280;
const unsigned RenderDriver::cMinResHeight = 720;
//
const unsigned RenderDriver::cMaxResWidth = 1920;
const unsigned RenderDriver::cMaxResHeight = 1080;
//
const unsigned RenderDriver::cOptimalResWidth = 1366; //1280; //1600;
const unsigned RenderDriver::cOptimalResHeight = 768; //720; //900;




float RenderDriver::CalcAspect()
{
	IDirect3DSurface9* pBackBuffer;
	GetDevice()->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);
	D3DSURFACE_DESC desc;
	pBackBuffer->GetDesc(&desc);
	pBackBuffer->Release();
	return static_cast<float>(desc.Width) / desc.Height;
}

bool RenderDriver::FindNearMode(const lsl::Point& resolution, DisplayMode& mode, float aspect) const
{
	if (GetDisplayModes().empty())
		return false;

	if (aspect != 0)
	{
		float minAspect = 0.0f;

		for (unsigned i = 0; i < GetDisplayModes().size(); ++i)
		{
			DisplayMode dispMode = GetDisplayModes()[i];			
			float newAspect = dispMode.height != 0 ? dispMode.width /(float)dispMode.height : 0.0f;

			if (newAspect != 0 && (abs(minAspect - aspect) > abs(aspect - newAspect) || minAspect == 0.0f))
				minAspect = newAspect;
		}

		aspect = minAspect;
	}

	int minD = -1;
	int minDAsp = -1;
	DisplayMode resMode = mode;
	DisplayMode resModeAsp = mode;

	for (unsigned i = 0; i < GetDisplayModes().size(); ++i)
	{
		DisplayMode dispMode = GetDisplayModes()[i];
		int dist = abs(static_cast<int>(dispMode.width * dispMode.height - resolution.x * resolution.y));

		if (aspect != 0.0f && dispMode.height != 0 && abs(aspect - dispMode.width/(float)dispMode.height) < 0.001f)
		{
			if (minDAsp > dist || minDAsp == -1)
			{
				minDAsp = dist;
				resModeAsp = dispMode;
			}
		}
		else
		{
			if (minD > dist || minD == -1)
			{
				minD = dist;
				resMode = dispMode;
			}
		}
	}

	mode = minDAsp != -1 ? resModeAsp : resMode;

	return true;
}

}

}

#endif