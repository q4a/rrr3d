$input v_texcoord0, v_worldPosition, v_projectedPosition

#include "bgfx_shader.sh"

// The source WaterPlane binds scene depth, water00.png and the planar
// reflection in this order. Reusing the engine's three portable samplers
// keeps that contract explicit without introducing a backend-only API.
SAMPLER2D(s_texColor, 0);
SAMPLER2D(s_texReflection, 1);
SAMPLER2D(s_texShadow, 2);
uniform vec4 u_sceneLightDirection;
uniform vec4 u_sceneFog;
uniform vec4 u_sceneCamera;
uniform vec4 u_materialColor;
// x = source time (dt * 0.15), y = cloud intensity.
uniform vec4 u_materialParams;

void main()
{
    vec2 projected =
        v_projectedPosition.xy /
        max(abs(v_projectedPosition.w), 0.0001);
    vec2 screenUv =
        projected * vec2(0.5, -0.5) + vec2(0.5, 0.5);

    float time = u_materialParams.x;
    vec3 normal1 =
        texture2D(s_texReflection, v_texcoord0 + vec2(time)).xyz *
            2.0 - 1.0;
    vec3 normal2 =
        texture2D(s_texReflection,
                  v_texcoord0 * 0.5 + vec2(time)).xyz *
            2.0 - 1.0;
    vec3 normal = normalize(normal1 + normal2 + vec3(0.0, 0.0, 1.0));

    vec3 viewDirection =
        normalize(u_sceneCamera.xyz - v_worldPosition);
    vec3 lightDirection =
        normalize(u_sceneLightDirection.xyz);
    float fresnel =
        clamp(1.0 - dot(viewDirection, normal), 0.0, 1.0);
    float highlight = pow(
        max(dot(normalize(lightDirection + viewDirection), normal),
            0.0),
        128.0);

    vec2 distortion = normal.xy * 0.07;
    vec3 reflection =
        texture2D(s_texShadow, screenUv + distortion).rgb;
    vec3 color =
        u_materialColor.rgb * u_materialColor.a * (1.0 - fresnel) +
        reflection * fresnel + vec3_splat(highlight * 0.5);

    float cameraDistance =
        length(v_worldPosition - u_sceneCamera.xyz);
    float fog = clamp(cameraDistance / 120.0 * u_sceneFog.a,
                      0.0, 0.92);
    color = mix(color, u_sceneFog.rgb, fog);

    float sceneDepth = texture2D(s_texColor, screenUv).r;
    float surfaceDepth =
        v_projectedPosition.z /
        max(abs(v_projectedPosition.w), 0.0001);
    float depthDistance =
        max(sceneDepth - surfaceDepth, 0.0) * 120.0;
    float waterAlpha =
        1.0 - exp(-depthDistance * depthDistance *
                  max(u_materialParams.y, 0.001));
    gl_FragColor =
        vec4(color, clamp(waterAlpha, 0.03, 0.96));
}
