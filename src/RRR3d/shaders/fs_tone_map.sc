$input v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);
SAMPLER2D(s_texReflection, 1);
// x = luminance key, y = exposure, z = bloom contribution.
uniform vec4 u_postParams;

void main()
{
    vec3 hdr = texture2D(s_texColor, v_texcoord0).rgb;
    vec3 bloom = texture2D(s_texReflection, v_texcoord0).rgb;
    float exposure = max(u_postParams.y, 0.01) * 0.08;
    vec3 mapped =
        vec3(1.0) - exp(-(hdr + bloom * u_postParams.z) * exposure);
    float luminance = dot(mapped, vec3(0.2126, 0.7152, 0.0722));
    mapped *= u_postParams.x /
              max(u_postParams.x + luminance, 0.0001) + 0.45;
    mapped = pow(max(mapped, vec3(0.0)), vec3_splat(1.0 / 2.2));
    gl_FragColor = vec4(clamp(mapped, 0.0, 1.0), 1.0);
}
