$input v_normal, v_texcoord0, v_worldPosition, v_reflectionPosition, v_shadowPosition, v_linearDepth

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);
SAMPLER2D(s_texReflection, 1);
SAMPLER2D(s_texShadow, 2);
uniform vec4 u_sceneLightDirection;
uniform vec4 u_sceneAmbient;
uniform vec4 u_sceneFog;
uniform vec4 u_sceneCamera;
uniform vec4 u_materialColor;
// x = alpha reference, y = emissive mix, z = specular strength,
// w = specular exponent. This is the portable form of LibMaterial state.
uniform vec4 u_materialParams;
// x = ignore fog, y = planar reflection, z = shadow strength,
// w = source clip plane enabled.
uniform vec4 u_materialOptions;
// w = original IActor::Lighting (glNone..glPlanarRefl).
uniform vec4 u_postParams;
uniform vec4 u_clipPlane;

void main()
{
    if (u_materialOptions.w > 0.5 &&
        dot(vec4(v_worldPosition, 1.0), u_clipPlane) < 0.0)
        discard;
    float mappingMode = u_postParams.w;
    vec3 normal = normalize(v_normal);
    if (abs(mappingMode - 4.0) < 0.5)
    {
        float heightX =
            texture2D(s_texColor,
                      v_texcoord0 + vec2(0.0025, 0.0)).r -
            texture2D(s_texColor,
                      v_texcoord0 - vec2(0.0025, 0.0)).r;
        float heightY =
            texture2D(s_texColor,
                      v_texcoord0 + vec2(0.0, 0.0025)).r -
            texture2D(s_texColor,
                      v_texcoord0 - vec2(0.0, 0.0025)).r;
        normal = normalize(normal + vec3(-heightX, -heightY, 0.0));
    }
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
    float shadowFactor = 1.0;
    if (u_materialOptions.z > 0.0 &&
        v_shadowPosition.w > 0.0001)
    {
        vec3 shadowNdc =
            v_shadowPosition.xyz / v_shadowPosition.w;
        vec2 shadowUv =
            shadowNdc.xy * vec2(0.5, -0.5) + vec2(0.5, 0.5);
        if (shadowUv.x >= 0.0 && shadowUv.x <= 1.0 &&
            shadowUv.y >= 0.0 && shadowUv.y <= 1.0 &&
            shadowNdc.z >= 0.0 && shadowNdc.z <= 1.0)
        {
            float storedDepth =
                texture2D(s_texShadow, shadowUv).r;
            float visibility =
                shadowNdc.z - 0.0025 <= storedDepth ? 1.0 : 0.0;
            shadowFactor =
                mix(1.0 - u_materialOptions.z, 1.0, visibility);
        }
    }
    vec3 lit =
        mappingMode < 0.5
            ? albedo.rgb
            : albedo.rgb * lighting * shadowFactor +
                  vec3_splat(specular * shadowFactor);
    float fresnel =
        clamp(1.0 - dot(viewDirection, normal), 0.0, 1.0);
    if (abs(mappingMode - 3.0) < 0.5)
    {
        lit = mix(lit, albedo.rgb + vec3_splat(specular),
                  0.4 * fresnel);
    }
    else if (abs(mappingMode - 5.0) < 0.5)
    {
        lit = mix(lit, u_sceneFog.rgb, 0.22 * fresnel);
    }
    if (u_materialOptions.y > 0.0 &&
        v_reflectionPosition.w > 0.0001)
    {
        vec3 reflectionNdc =
            v_reflectionPosition.xyz / v_reflectionPosition.w;
        vec2 reflectionUv =
            reflectionNdc.xy * vec2(0.5, -0.5) +
            vec2(0.5, 0.5);
        if (reflectionUv.x >= 0.0 && reflectionUv.x <= 1.0 &&
            reflectionUv.y >= 0.0 && reflectionUv.y <= 1.0)
        {
            vec3 reflected =
                texture2D(s_texReflection, reflectionUv).rgb;
            lit = mix(lit, reflected,
                      clamp(u_materialOptions.y, 0.0, 0.72));
        }
    }
    lit = mix(lit, albedo.rgb, clamp(u_materialParams.y, 0.0, 1.0));
    float distanceToCamera = length(v_worldPosition - u_sceneCamera.xyz);
    float fog = clamp(distanceToCamera / 120.0 * u_sceneFog.a,
                      0.0, 0.92) *
                (1.0 - clamp(u_materialOptions.x, 0.0, 1.0));
    vec3 color = mix(lit, u_sceneFog.rgb, fog);
    gl_FragColor = vec4(color, albedo.a);
}
