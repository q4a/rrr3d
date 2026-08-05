$input v_texcoord0, v_worldPosition

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);
uniform vec4 u_sceneFog;
uniform vec4 u_sceneCamera;
uniform vec4 u_materialColor;
uniform vec4 u_materialParams;

void main()
{
    vec4 color = texture2D(s_texColor, v_texcoord0);
    if (color.a <= u_materialParams.x)
        discard;

    float fogFar = max(u_sceneCamera.w, 1.0);
    float fogStart = fogFar * (1.0 - clamp(u_sceneFog.a, 0.0, 1.0));
    float fog = clamp(
        (length(v_worldPosition - u_sceneCamera.xyz) - fogStart) /
            max(fogFar - fogStart, 0.0001),
        0.0, 1.0);
    gl_FragColor = vec4(
        mix(color.rgb * u_materialColor.rgb, u_sceneFog.rgb, fog),
        color.a);
}
