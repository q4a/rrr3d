$input v_normal, v_texcoord0, v_worldPosition, v_reflectionPosition, v_shadowPosition, v_linearDepth, v_tangent, v_bitangent, v_shadowPositionFar

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);
SAMPLER2D(s_texReflection, 1);
SAMPLER2D(s_texShadow, 2);
SAMPLERCUBE(s_texEnvironment, 3);
SAMPLER2D(s_texNormal, 4);
SAMPLER2D(s_texShadowFar, 5);
uniform vec4 u_sceneLightDirection;
uniform vec4 u_sceneLampPositions[12];
uniform vec4 u_sceneLampDirections[12];
uniform vec4 u_sceneLampColors[12];
// x = cos(phi/2), y = cos(theta/2), matching D3DLIGHT9.
uniform vec4 u_sceneLampCones[12];
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
// x = source split distance, y = 2048 map size, z = depth bias.
uniform vec4 u_shadowParams;

void main()
{
    if (u_materialOptions.w > 0.5 &&
        dot(vec4(v_worldPosition, 1.0), u_clipPlane) < 0.0)
        discard;
    float mappingMode = u_postParams.w;
    vec3 normal = normalize(v_normal);
    if (abs(mappingMode - 4.0) < 0.5)
    {
        vec3 tangent = normalize(v_tangent);
        vec3 bitangent = normalize(v_bitangent);
        vec3 tangentNormal =
            texture2D(s_texNormal, v_texcoord0).xyz * 2.0 - 1.0;
        normal = normalize(
            tangent * tangentNormal.x +
            bitangent * tangentNormal.y +
            normal * tangentNormal.z);
    }
    vec3 lightDirection = normalize(u_sceneLightDirection.xyz);
    float directionalEnabled = u_sceneLightDirection.w;
    float diffuse =
        max(dot(normal, lightDirection), 0.0) *
        directionalEnabled;
    vec3 lighting = u_sceneAmbient.rgb + vec3_splat(diffuse * 0.82);
    vec4 albedo = texture2D(s_texColor, v_texcoord0) * u_materialColor;
    if (u_materialParams.x > 0.0 &&
        albedo.a <= u_materialParams.x)
        discard;
    vec3 viewDirection =
        normalize(u_sceneCamera.xyz - v_worldPosition);
    if (u_postParams.x > 0.5)
    {
        // Bonus\\maslo's second fixed-function stage uses
        // D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR, SELECTARG1 for RGB and
        // keeps the default alpha modulation with the first oil sampler.
        vec3 reflectionVector =
            reflect(-viewDirection, normal);
        vec2 reflectionUv =
            reflectionVector.xy * 0.5 + vec2(0.5);
        vec4 reflectionLayer =
            texture2D(s_texNormal, reflectionUv);
        albedo = vec4(
            reflectionLayer.rgb * u_materialColor.rgb,
            albedo.a * reflectionLayer.a);
    }
    vec3 halfDirection = normalize(lightDirection + viewDirection);
    float specular = pow(max(dot(normal, halfDirection), 0.0),
                         max(u_materialParams.w, 1.0)) *
                     u_materialParams.z * directionalEnabled;
    // Both Environment::wtGarage lamps and Player::SetHeadlight use the
    // source D3DLIGHT_SPOT attenuation0=1/falloff=1 model.  Their theta/phi
    // differ, so the exact source cone cosines are supplied per light.
    for (int lamp = 0; lamp < 12; ++lamp)
    {
        float enabled = u_sceneLampDirections[lamp].w;
        vec3 fromLamp =
            v_worldPosition - u_sceneLampPositions[lamp].xyz;
        float distanceToLamp = length(fromLamp);
        float range = u_sceneLampPositions[lamp].w;
        if (enabled > 0.5 && distanceToLamp > 0.0001 &&
            distanceToLamp < range)
        {
            vec3 lampRay = fromLamp / distanceToLamp;
            vec3 lampDirection =
                normalize(u_sceneLampDirections[lamp].xyz);
            float coneCosine = dot(lampDirection, lampRay);
            float outerCosine = u_sceneLampCones[lamp].x;
            float innerCosine = u_sceneLampCones[lamp].y;
            float spot = clamp(
                (coneCosine - outerCosine) /
                    max(innerCosine - outerCosine, 0.0001),
                0.0, 1.0);
            vec3 toLamp = -lampRay;
            float lampDiffuse =
                max(dot(normal, toLamp), 0.0) * spot;
            lighting +=
                u_sceneLampColors[lamp].rgb * lampDiffuse;
            vec3 lampHalf = normalize(toLamp + viewDirection);
            specular +=
                pow(max(dot(normal, lampHalf), 0.0),
                    max(u_materialParams.w, 1.0)) *
                u_materialParams.z * spot *
                max(max(u_sceneLampColors[lamp].r,
                        u_sceneLampColors[lamp].g),
                    u_sceneLampColors[lamp].b);
        }
    }
    float shadowFactor = 1.0;
    if (u_materialOptions.z > 0.0 &&
        v_shadowPosition.w > 0.0001)
    {
        bool farSplit = v_linearDepth > u_shadowParams.x;
        vec4 shadowPosition =
            farSplit ? v_shadowPositionFar : v_shadowPosition;
        vec3 shadowNdc =
            shadowPosition.xyz / shadowPosition.w;
        vec2 shadowUv =
            shadowNdc.xy * vec2(0.5, -0.5) + vec2(0.5, 0.5);
        if (shadowUv.x >= 0.0 && shadowUv.x <= 1.0 &&
            shadowUv.y >= 0.0 && shadowUv.y <= 1.0 &&
            shadowNdc.z >= 0.0 && shadowNdc.z <= 1.0)
        {
            float texel = 1.0 / max(u_shadowParams.y, 1.0);
            float depth = shadowNdc.z - u_shadowParams.z;
            float depth00;
            float depth10;
            float depth01;
            float depth11;
            if (farSplit)
            {
                depth00 = texture2D(s_texShadowFar, shadowUv).r;
                depth10 = texture2D(
                    s_texShadowFar, shadowUv + vec2(texel, 0.0)).r;
                depth01 = texture2D(
                    s_texShadowFar, shadowUv + vec2(0.0, texel)).r;
                depth11 = texture2D(
                    s_texShadowFar, shadowUv + vec2(texel, texel)).r;
            }
            else
            {
                depth00 = texture2D(s_texShadow, shadowUv).r;
                depth10 = texture2D(
                    s_texShadow, shadowUv + vec2(texel, 0.0)).r;
                depth01 = texture2D(
                    s_texShadow, shadowUv + vec2(0.0, texel)).r;
                depth11 = texture2D(
                    s_texShadow, shadowUv + vec2(texel, texel)).r;
            }
            vec2 frame = fract(shadowUv * u_shadowParams.y);
            float visibility0 = mix(
                depth <= depth00 ? 1.0 : 0.0,
                depth <= depth10 ? 1.0 : 0.0, frame.x);
            float visibility1 = mix(
                depth <= depth01 ? 1.0 : 0.0,
                depth <= depth11 ? 1.0 : 0.0, frame.x);
            float visibility = mix(visibility0, visibility1, frame.y);
            shadowFactor =
                mix(1.0 - u_materialOptions.z, 1.0, visibility);
        }
    }
    vec3 lit =
        mappingMode < 0.5
            ? albedo.rgb
            : albedo.rgb * lighting * shadowFactor +
                  vec3_splat(specular * shadowFactor);
    if (abs(mappingMode - 3.0) < 0.5)
    {
        vec3 reflected = textureCube(
            s_texEnvironment,
            reflect(-viewDirection, normal)).rgb;
        lit = mix(lit, reflected, 0.4);
    }
    else if (abs(mappingMode - 5.0) < 0.5)
    {
        float fresnel =
            clamp(1.0 - dot(viewDirection, normal), 0.0, 1.0);
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
