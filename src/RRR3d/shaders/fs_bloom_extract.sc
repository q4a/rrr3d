$input v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);
// x = source bright threshold.
uniform vec4 u_postParams;

void main()
{
    vec3 color = texture2D(s_texColor, v_texcoord0).rgb;
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float weight = max(luminance - u_postParams.x, 0.0) /
                   max(luminance, 0.0001);
    gl_FragColor = vec4(color * weight, 1.0);
}
