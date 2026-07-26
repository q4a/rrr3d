$input v_normal, v_texcoord0, v_worldPosition

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);
uniform vec4 u_sceneLightDirection;
uniform vec4 u_sceneAmbient;
uniform vec4 u_sceneFog;
uniform vec4 u_sceneCamera;
uniform vec4 u_materialColor;
// x = alpha reference, y = emissive mix, z = specular strength,
// w = specular exponent. This is the portable form of LibMaterial state.
uniform vec4 u_materialParams;
// x = ignore scene fog. Original sprite/effect materials set this state.
uniform vec4 u_materialOptions;

void main()
{
    vec3 normal = normalize(v_normal);
    vec3 lightDirection = normalize(u_sceneLightDirection.xyz);
    float diffuse = max(dot(normal, lightDirection), 0.0);
    vec3 lighting = u_sceneAmbient.rgb + vec3_splat(diffuse * 0.82);
    vec4 albedo = texture2D(s_texColor, v_texcoord0) * u_materialColor;
    if (u_materialParams.x > 0.0 &&
        albedo.a <= u_materialParams.x)
        discard;
    vec3 viewDirection =
        normalize(u_sceneCamera.xyz - v_worldPosition);
    vec3 halfDirection = normalize(lightDirection + viewDirection);
    float specular = pow(max(dot(normal, halfDirection), 0.0),
                         max(u_materialParams.w, 1.0)) *
                     u_materialParams.z;
    vec3 lit = albedo.rgb * lighting + vec3_splat(specular);
    lit = mix(lit, albedo.rgb, clamp(u_materialParams.y, 0.0, 1.0));
    float distanceToCamera = length(v_worldPosition - u_sceneCamera.xyz);
    float fog = clamp(distanceToCamera / 120.0 * u_sceneFog.a,
                      0.0, 0.92) *
                (1.0 - clamp(u_materialOptions.x, 0.0, 1.0));
    vec3 color = mix(lit, u_sceneFog.rgb, fog);
    gl_FragColor = vec4(color, albedo.a);
}
