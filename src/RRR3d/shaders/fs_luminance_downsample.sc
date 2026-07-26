$input v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);
// xy = source texel size, z = exponentiate the final 1x1 level.
uniform vec4 u_postParams;

void main()
{
    float logarithmic = 0.0;
    float maximum = 0.0;
    for (int y = -2; y < 2; ++y)
    {
        for (int x = -2; x < 2; ++x)
        {
            vec4 sampleValue = texture2D(
                s_texColor,
                v_texcoord0 +
                    (vec2(float(x), float(y)) + vec2(0.5)) *
                        u_postParams.xy);
            logarithmic += sampleValue.r;
            maximum = max(maximum, sampleValue.g);
        }
    }
    logarithmic /= 16.0;
    if (u_postParams.z > 0.5)
        logarithmic = exp(logarithmic);
    gl_FragColor = vec4(logarithmic, maximum, 0.0, 1.0);
}
