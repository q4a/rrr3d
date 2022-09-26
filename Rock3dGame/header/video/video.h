// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#include <windows.h>
#include <dshow.h>
#include <d3d9.h>
#include <Vmr9.h>
#include <Evr.h>

template <class T>
void SafeRelease(T** ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
}

HRESULT RemoveUnconnectedRenderer(IGraphBuilder* pGraph, IBaseFilter* pRenderer, BOOL* pbRemoved);
HRESULT AddFilterByCLSID(IGraphBuilder* pGraph, REFGUID clsid, IBaseFilter** ppF, LPCWSTR wszName);

// Abstract class to manage the video renderer filter.
// Specific implementations handle the VMR-7, VMR-9, or EVR filter.

class CVideoRenderer
{
public:
	virtual ~CVideoRenderer()
	{
	};
	virtual BOOL HasVideo() const = 0;
	virtual HRESULT AddToGraph(IGraphBuilder* pGraph, HWND hwnd) = 0;
	virtual HRESULT FinalizeGraph(IGraphBuilder* pGraph) = 0;
	virtual HRESULT UpdateVideoWindow(HWND hwnd, LPRECT prc) = 0;
	virtual HRESULT Repaint(HWND hwnd, HDC hdc) = 0;
	virtual HRESULT DisplayModeChanged() = 0;

	virtual bool GetFullScreen() const = 0;
	virtual void SetFullScreen(bool value) = 0;
};

// Manages the VMR-7 video renderer filter.

class CVMR7 : public CVideoRenderer
{
	IVMRWindowlessControl* m_pWindowless;

public:
	CVMR7();
	~CVMR7() override;
	BOOL HasVideo() const override;
	HRESULT AddToGraph(IGraphBuilder* pGraph, HWND hwnd) override;
	HRESULT FinalizeGraph(IGraphBuilder* pGraph) override;
	HRESULT UpdateVideoWindow(HWND hwnd, LPRECT prc) override;
	HRESULT Repaint(HWND hwnd, HDC hdc) override;
	HRESULT DisplayModeChanged() override;

	bool GetFullScreen() const override;
	void SetFullScreen(bool value) override;
};


// Manages the VMR-9 video renderer filter.

class CVMR9 : public CVideoRenderer
{
	IVMRWindowlessControl9* m_pWindowless;
	IVideoWindow* _videoWindow;
public:
	CVMR9();
	~CVMR9() override;
	BOOL HasVideo() const override;
	HRESULT AddToGraph(IGraphBuilder* pGraph, HWND hwnd) override;
	HRESULT FinalizeGraph(IGraphBuilder* pGraph) override;
	HRESULT UpdateVideoWindow(HWND hwnd, LPRECT prc) override;
	HRESULT Repaint(HWND hwnd, HDC hdc) override;
	HRESULT DisplayModeChanged() override;

	bool GetFullScreen() const override;
	void SetFullScreen(bool value) override;
};


// Manages the EVR video renderer filter.

class CEVR : public CVideoRenderer
{
	IBaseFilter* m_pEVR;
	IMFVideoDisplayControl* m_pVideoDisplay;

public:
	CEVR();
	~CEVR() override;
	BOOL HasVideo() const override;
	HRESULT AddToGraph(IGraphBuilder* pGraph, HWND hwnd) override;
	HRESULT FinalizeGraph(IGraphBuilder* pGraph) override;
	HRESULT UpdateVideoWindow(HWND hwnd, LPRECT prc) override;
	HRESULT Repaint(HWND hwnd, HDC hdc) override;
	HRESULT DisplayModeChanged() override;

	bool GetFullScreen() const override;
	void SetFullScreen(bool value) override;
};
