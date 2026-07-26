$input v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);
// xy = one-pixel blur direction, z = legacy gaussian scalar.
uniform vec4 u_postParams;

void main()
{
    vec2 direction = u_postParams.xy;
    vec3 color =
        texture2D(s_texColor, v_texcoord0).rgb * 0.227027;
    color += texture2D(
        s_texColor, v_texcoord0 + direction * 1.384615).rgb *
        0.316216;
    color += texture2D(
        s_texColor, v_texcoord0 - direction * 1.384615).rgb *
        0.316216;
    color += texture2D(
        s_texColor, v_texcoord0 + direction * 3.230769).rgb *
        0.070270;
    color += texture2D(
        s_texColor, v_texcoord0 - direction * 3.230769).rgb *
        0.070270;
    float legacyScale = clamp(u_postParams.z / 30.0, 0.35, 1.5);
    gl_FragColor = vec4(color * legacyScale, 1.0);
}
