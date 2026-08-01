/*
 * The same triangle, but fed through [[stage_in]] and a vertex descriptor --
 * the way DXVK's generated shaders take their vertices.
 *
 * Paired with tri.metal (which indexes a buffer by vertex_id) this isolates one
 * variable: whether the vertex-descriptor path works. Attribute 0 and buffer
 * index 14 mirror what d9mt binds for a real draw.
 */

#include <metal_stdlib>
using namespace metal;

struct VIn
{
	float4 position [[attribute(0)]];
};

struct VOut
{
	float4 position [[position]];
};

vertex VOut vs_main(VIn in [[stage_in]])
{
	VOut out;
	out.position = in.position;
	return out;
}

fragment float4 fs_main(VOut in [[stage_in]])
{
	return float4(0.0, 1.0, 0.0, 1.0);
}
