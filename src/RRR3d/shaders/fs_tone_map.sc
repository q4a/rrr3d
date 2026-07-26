$input v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);
SAMPLER2D(s_texReflection, 1);
SAMPLER2D(s_texShadow, 2);
// x = gaussian scalar, y = exposure, z = bloom contribution.
uniform vec4 u_postParams;

void main()
{
    vec3 color = texture2D(s_texColor, v_texcoord0).rgb;
    vec3 bloom = texture2D(s_texReflection, v_texcoord0).rgb;
    vec2 luminance =
        texture2D(s_texShadow, vec2(0.5)).rg;
    color += bloom * u_postParams.z;
    float peak = max(color.r, max(color.g, color.b));
    float lp =
        max(u_postParams.y, 0.01) /
        max(luminance.r, 0.001) * peak;
    float white =
        luminance.g * (1.0 + max(u_postParams.x, 0.0));
    float whiteSquared = max(white * white, 0.0001);
    float tone =
        (lp * (1.0 + lp / whiteSquared)) / (1.0 + lp);
    gl_FragColor = vec4(clamp(color * tone, 0.0, 1.0), 1.0);
}
