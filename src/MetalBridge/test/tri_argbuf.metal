/*
 * The same triangle again, but with the vertex positions scaled by a value the
 * shader reaches *through an argument buffer* -- the way DXVK's generated
 * shaders read their constants.
 *
 * This is the layer above tri_stagein.metal: stage_in for vertices, plus an
 * argument buffer holding a pointer to a uniform block. If the triangle is
 * missing or the wrong size here, the argument-buffer path (its contents, or
 * the useResource residency it needs) is the fault.
 */

#include <metal_stdlib>
using namespace metal;

struct Consts
{
	float4 scale;
};

/*
 * Two members, matching the shape spirv-cross emits for DXVK: id(0) is the
 * spec-state pointer the real shaders carry (and never dereference once the
 * gate function constant is set), id(1) is the constant block. Getting this
 * shape right matters -- with a single member, [[id(1)]] lands at offset 0 and
 * the test passes for the wrong reason.
 */
struct ArgBuf
{
	constant Consts* unused  [[id(0)]];
	constant Consts* consts  [[id(1)]];
};

struct VIn
{
	float4 position [[attribute(0)]];
};

struct VOut
{
	float4 position [[position]];
};

vertex VOut vs_main(VIn in [[stage_in]],
                    constant ArgBuf& args [[buffer(0)]])
{
	VOut out;
	out.position = in.position * (*args.consts).scale;
	return out;
}

fragment float4 fs_main(VOut in [[stage_in]])
{
	return float4(0.0, 1.0, 0.0, 1.0);
}
