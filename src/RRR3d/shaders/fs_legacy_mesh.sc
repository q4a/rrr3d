$input v_normal, v_texcoord0, v_worldPosition

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);
uniform vec4 u_sceneLightDirection;
uniform vec4 u_sceneAmbient;
uniform vec4 u_sceneFog;
uniform vec4 u_sceneCamera;

void main()
{
    vec3 normal = normalize(v_normal);
    vec3 lightDirection = normalize(u_sceneLightDirection.xyz);
    float diffuse = max(dot(normal, lightDirection), 0.0);
    vec3 lighting = u_sceneAmbient.rgb + vec3_splat(diffuse * 0.82);
    vec4 albedo = texture2D(s_texColor, v_texcoord0);
    if (albedo.a < 0.1)
        discard;
    float distanceToCamera = length(v_worldPosition - u_sceneCamera.xyz);
    float fog = clamp(distanceToCamera / 120.0 * u_sceneFog.a,
                      0.0, 0.92);
    vec3 color = mix(albedo.rgb * lighting, u_sceneFog.rgb, fog);
    gl_FragColor = vec4(color, albedo.a);
}
