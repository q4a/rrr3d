$input a_position, a_normal, a_texcoord0
$output v_texcoord0, v_worldPosition, v_linearDepth

#include "bgfx_shader.sh"

uniform vec4 u_textureTransform;

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_texcoord0 =
        a_texcoord0 * u_textureTransform.xy + u_textureTransform.zw;
    v_worldPosition =
        mul(u_model[0], vec4(a_position, 1.0)).xyz;
    v_linearDepth =
        gl_Position.z / max(abs(gl_Position.w), 0.0001);
}
