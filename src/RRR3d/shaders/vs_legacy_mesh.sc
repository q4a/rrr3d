$input a_position, a_normal, a_texcoord0, a_tangent, a_bitangent, a_color0
$output v_normal, v_texcoord0, v_worldPosition, v_reflectionPosition, v_shadowPosition, v_linearDepth, v_tangent, v_bitangent, v_shadowPositionFar, v_color0

#include "bgfx_shader.sh"

uniform vec4 u_textureTransform;
uniform mat4 u_reflectionViewProj;
uniform mat4 u_shadowViewProj;
uniform mat4 u_shadowViewProjFar;

void main()
{
    vec4 worldPosition = mul(u_model[0], vec4(a_position, 1.0));
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_normal = mul(u_model[0], vec4(a_normal, 0.0)).xyz;
    v_color0 = a_color0;
    v_tangent = mul(u_model[0], vec4(a_tangent, 0.0)).xyz;
    v_bitangent = mul(u_model[0], vec4(a_bitangent, 0.0)).xyz;
    v_texcoord0 =
        a_texcoord0 * u_textureTransform.xy + u_textureTransform.zw;
    v_worldPosition = worldPosition.xyz;
    v_reflectionPosition = mul(u_reflectionViewProj, worldPosition);
    v_shadowPosition = mul(u_shadowViewProj, worldPosition);
    v_shadowPositionFar = mul(u_shadowViewProjFar, worldPosition);
    v_linearDepth = -mul(u_view, worldPosition).z;
}
