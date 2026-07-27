/*
 * The three Direct3D 12 interfaces DXVK's D3D9 front-end names, and nothing
 * else.
 *
 * d3d9_include.h pulls in <d3d12.h> for its D3D9On12 block -- the Windows
 * interop path where a D3D9 device runs on top of a D3D12 one, sharing a queue
 * and a fence. That path is unreachable here: there is no D3D12 to interop
 * with, and this build's device comes from the Metal backend.
 *
 * The front-end only ever holds these as pointers inside D3D9ON12_ARGS, so
 * incomplete types are enough. Vendoring MinGW-w64's real d3d12.h would drag in
 * several thousand lines of an API nothing calls.
 *
 * If something ever dereferences one of these, the compiler will say so, and
 * that would be worth knowing rather than papering over.
 */

#ifndef __XPLATFORM_D3D12_H__
#define __XPLATFORM_D3D12_H__

#ifdef _WIN32
#error "this d3d12.h is the non-Windows stand-in; Windows has the real one"
#endif

struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12Fence;

#endif /* __XPLATFORM_D3D12_H__ */
