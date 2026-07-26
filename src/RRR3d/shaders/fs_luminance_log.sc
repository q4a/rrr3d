$input v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);
// xy = source texel size.
uniform vec4 u_postParams;

void main()
{
    float logarithmic = 0.0;
    float maximum = 0.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            vec3 color = texture2D(
                s_texColor,
                v_texcoord0 + vec2(float(x), float(y)) *
                    u_postParams.xy).rgb;
            logarithmic += log(
                dot(color, vec3(0.2125, 0.7154, 0.0721)) +
                0.0001);
            maximum = max(maximum, max(color.r, max(color.g, color.b)));
        }
    }
    gl_FragColor =
        vec4(logarithmic / 9.0, maximum, 0.0, 1.0);
}
