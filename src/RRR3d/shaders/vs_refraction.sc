$input a_position, a_normal, a_texcoord0, a_tangent, a_bitangent
$output v_texcoord0, v_projectedPosition

#include "bgfx_shader.sh"

uniform vec4 u_textureTransform;

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_projectedPosition = gl_Position;
    v_texcoord0 =
        a_texcoord0 * u_textureTransform.xy + u_textureTransform.zw;
}
