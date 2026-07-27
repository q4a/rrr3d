
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

class CVideoRenderer;

enum PlaybackState
{
	STATE_NO_GRAPH,
	STATE_RUNNING,
	STATE_PAUSED,
	STATE_STOPPED,
};

class IVideoGraphUser
{
public:
	virtual void OnGraphEvent(HWND hwnd, long eventCode, LONG_PTR param1, LONG_PTR param2) = 0;
};

// Everything above is plain declaration that World.h needs. Everything below is
// DirectShow -- IGraphBuilder, IMediaControl, VMR/EVR renderers -- which has no
// equivalent off Windows. video::Player holds a DShowPlayer only by pointer, so
// an incomplete type is enough there and the non-Windows build never has one.
#ifdef _WIN32

class DShowPlayer
{
public:
	DShowPlayer(HWND hwnd, UINT windowMsg);
	~DShowPlayer();

	PlaybackState State() const { return m_state; }

	HRESULT OpenFile(PCWSTR pszFileName);
	void    TearDownGraph();

	HRESULT Play();
	HRESULT Pause();
	HRESULT Stop();

	BOOL    HasVideo() const;
	HRESULT UpdateVideoWindow(const LPRECT prc);
	HRESULT Repaint(HDC hdc);
	HRESULT DisplayModeChanged();

	bool GetFullScreen() const;
	void SetFullScreen(bool value);

	HRESULT HandleGraphEvent(IVideoGraphUser* user);
private:
	HRESULT InitializeGraph();	
	HRESULT CreateVideoRenderer();
	HRESULT RenderStreams(IBaseFilter *pSource);

	PlaybackState   m_state;	

	HWND m_hwnd; // Video window. This window also receives graph events.
	UINT m_windowMsg;

	IGraphBuilder   *m_pGraph;
	IMediaControl   *m_pControl;
	IMediaEventEx   *m_pEvent;
	CVideoRenderer  *m_pVideo;
};

#else

class DShowPlayer;

#endif /* _WIN32 */