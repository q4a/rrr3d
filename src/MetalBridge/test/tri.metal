/*
 * The smallest shader pair that proves a draw works: positions straight from a
 * bound buffer, a constant colour out. No uniforms, no argument buffers, no
 * function constants, no vertex descriptor -- so nothing here can fail for the
 * reasons the D3D9 path might.
 */

#include <metal_stdlib>
using namespace metal;

struct VOut
{
	float4 position [[position]];
};

vertex VOut vs_main(uint vid [[vertex_id]],
                    const device float4* verts [[buffer(0)]])
{
	VOut out;
	out.position = verts[vid];
	return out;
}

fragment float4 fs_main(VOut in [[stage_in]])
{
	return float4(0.0, 1.0, 0.0, 1.0);
}
