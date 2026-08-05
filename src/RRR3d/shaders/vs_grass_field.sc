$input a_position, a_normal, a_texcoord0
$output v_texcoord0, v_worldPosition

#include "bgfx_shader.sh"

void main()
{
    // GrassFieldVS stores the sprite corner in POSITION.w. The portable
    // static layout retains that corner in NORMAL.xy and obtains the same
    // camera-facing axes from bgfx's inverse view matrix.
    vec3 cameraRight = normalize(u_invView[0].xyz);
    vec3 cameraUp = normalize(u_invView[1].xyz);
    vec3 localPosition =
        a_position + cameraRight * a_normal.x + cameraUp * a_normal.y;
    vec4 worldPosition = mul(u_model[0], vec4(localPosition, 1.0));
    gl_Position = mul(u_viewProj, worldPosition);
    v_texcoord0 = a_texcoord0;
    v_worldPosition = worldPosition.xyz;
}
