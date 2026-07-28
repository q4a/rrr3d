/*
 * A triangle whose fragments are half-transparent, for testing blending.
 *
 * The game's sprites draw opaque -- the regions that should be masked out come
 * out white -- and every link in engine -> DXVK -> d9mt -> Metal checks out
 * when read individually. So this asks the question directly: with
 * blendingEnabled and SourceAlpha / OneMinusSourceAlpha set on the pipeline,
 * does a fragment with alpha 0.5 actually blend with what is already there?
 *
 * Red at half alpha over a blue clear should read back as half red, half blue.
 * Fully red means the alpha was ignored, which is the game's symptom.
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
	return float4(1.0, 0.0, 0.0, 0.5);
}
