$input v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);
SAMPLER2D(s_texReflection, 1);
// x = luminance key, y = source bright threshold,
// z = use the source engine's static 0.55 luminance path.
uniform vec4 u_postParams;

void main()
{
    vec3 color = texture2D(s_texColor, v_texcoord0).rgb;
    float adaptedLuminance = u_postParams.z > 0.5
        ? 0.55
        : texture2D(s_texReflection, vec2(0.5)).r + 0.001;
    color *= u_postParams.x / adaptedLuminance;
    color = max(color - vec3_splat(u_postParams.y), vec3(0.0));
    color /= vec3(1.0) + color;
    gl_FragColor = vec4(color, 1.0);
}
