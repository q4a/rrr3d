/*
 * The same triangle again, but the shader is *specialised* by function
 * constants the way every real DXVK shader is.
 *
 * This mirrors the shape spirv-cross emits: a gate constant says "the spec data
 * is baked into this function, use the baked value"; when the gate is 0 the
 * shader instead dereferences a pointer in the argument buffer which the real
 * path never binds. So if the gate is not actually applied, the shader reads a
 * null pointer and every vertex comes out garbage -- silently, with no
 * validation error. That is exactly the failure being hunted, so it is worth
 * proving the gate reaches the GPU.
 */

#include <metal_stdlib>
using namespace metal;

struct Consts
{
	float4 scale;
};

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

/* index 12 is the gate, index 1 the baked value -- the same indices d9mt uses. */
constant uint gate_tmp   [[function_constant(12)]];
constant uint gate       = is_function_constant_defined(gate_tmp) ? gate_tmp : 0u;
constant uint baked_tmp  [[function_constant(1)]];
constant uint baked      = is_function_constant_defined(baked_tmp) ? baked_tmp : 0u;

vertex VOut vs_main(VIn in [[stage_in]],
                    constant ArgBuf& args [[buffer(0)]])
{
	/* Gate set -> use the baked constant. Gate clear -> read the null pointer. */
	uint flags = (gate != 0u) ? baked : as_type<uint>((*args.unused).scale.x);

	VOut out;
	out.position = in.position * (*args.consts).scale;
	/* Keep `flags` live so the compiler cannot fold the branch away. */
	if (flags == 0xffffffffu)
		out.position = float4(0.0);
	return out;
}

fragment float4 fs_main(VOut in [[stage_in]])
{
	return float4(0.0, 1.0, 0.0, 1.0);
}
