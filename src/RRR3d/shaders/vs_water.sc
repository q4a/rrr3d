$input a_position, a_normal, a_texcoord0
$output v_texcoord0, v_worldPosition, v_projectedPosition

#include "bgfx_shader.sh"

uniform vec4 u_textureTransform;

void main()
{
    vec4 worldPosition = mul(u_model[0], vec4(a_position, 1.0));
    vec4 projectedPosition =
        mul(u_modelViewProj, vec4(a_position, 1.0));
    vec4 viewPosition = mul(u_view, worldPosition);
    gl_Position = projectedPosition;
    v_texcoord0 =
        a_texcoord0 * u_textureTransform.xy + u_textureTransform.zw;
    v_worldPosition = worldPosition.xyz;
    // Windows stores positive view-space Z in projPos.z after preserving
    // clip-space XY/W for screen-coordinate lookup.
    v_projectedPosition = vec4(
        projectedPosition.xy, abs(viewPosition.z), projectedPosition.w);
}
