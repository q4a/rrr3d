$input v_normal, v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);

void main()
{
    vec3 normal = normalize(v_normal);
    vec3 lightDirection = normalize(vec3(-0.45, -0.35, 0.82));
    float diffuse = max(dot(normal, lightDirection), 0.0);
    vec3 lighting = vec3_splat(0.28 + diffuse * 0.72);
    vec4 albedo = texture2D(s_texColor, v_texcoord0);
    gl_FragColor = vec4(albedo.rgb * lighting, albedo.a);
}
