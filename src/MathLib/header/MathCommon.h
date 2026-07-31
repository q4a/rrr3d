#pragma once

// windows.h first: the DirectX headers name the Windows scalar types and do not
// include anything themselves, which is the ordering the SDK assumes on Windows
// too. The D3DX headers live in XPlatform now rather than beside this one --
// there is one vendored copy for the whole tree.
#include <windows.h>
#include <d3dx9math.h>
