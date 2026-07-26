$input v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);
SAMPLER2D(s_texReflection, 1);
// x = elapsed frame time / 2, matching HDREffect.cpp.
uniform vec4 u_postParams;

void main()
{
    vec2 currentValue =
        texture2D(s_texColor, vec2(0.5)).rg;
    vec2 previousValue =
        texture2D(s_texReflection, vec2(0.5)).rg;
    float weight =
        1.0 - pow(0.98, 30.0 * max(u_postParams.x, 0.0));
    vec2 adapted =
        previousValue + (currentValue - previousValue) * weight;
    adapted.x = clamp(adapted.x, 0.5, 5.0);
    gl_FragColor = vec4(adapted, 0.0, 1.0);
}
