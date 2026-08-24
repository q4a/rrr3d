$input v_texcoord0, v_worldPosition, v_projectedPosition

#include "bgfx_shader.sh"

// The source WaterPlane binds scene depth, water00.png and the planar
// reflection in this order. Reusing the engine's three portable samplers
// keeps that contract explicit without introducing a backend-only API.
SAMPLER2D(s_texColor, 0);
SAMPLER2D(s_texReflection, 1);
SAMPLER2D(s_texShadow, 2);
uniform vec4 u_sceneSunPosition;
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
        normalize(u_sceneSunPosition.xyz - v_worldPosition);
    float fresnel = 1.0 - dot(viewDirection, normal);
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

    float cameraDistance = length(v_worldPosition - u_sceneCamera.xyz);
    float fogFar = max(u_sceneCamera.w, 1.0);
    float fogStart = fogFar * (1.0 - clamp(u_sceneFog.a, 0.0, 1.0));
    float fog = clamp(
        (cameraDistance - fogStart) /
            max(fogFar - fogStart, 0.0001),
        0.0, 1.0);
    color = mix(color, u_sceneFog.rgb, fog);

    float sceneDepth = texture2D(s_texColor, screenUv).r;
    vec4 sceneView = mul(
        u_invProj, vec4(0.0, 0.0, sceneDepth, 1.0));
    float sceneViewDepth =
        abs(sceneView.z / max(abs(sceneView.w), 0.0001));
    float depthDistance =
        sceneViewDepth - v_projectedPosition.z;
    // The Windows WaterPlane is submitted into the scene render target and
    // therefore still has the opaque scene depth attached.  The Metal port
    // copies the scene into a second color target before this pass, so its
    // depth attachment is empty.  Reproduce the source depth test explicitly:
    // a car or track fragment in front of the water must reject this water
    // fragment instead of being covered by the reflected scene.
    if (depthDistance <= 0.001)
        discard;
    float transmittance = depthDistance > 0.001
        ? 1.0 / exp(depthDistance * depthDistance *
                    u_materialParams.y)
        : 0.0;
    gl_FragColor = vec4(color, 1.0 - transmittance);
}
