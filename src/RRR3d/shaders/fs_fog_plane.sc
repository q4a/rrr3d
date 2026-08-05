$input v_texcoord0, v_worldPosition, v_projectedPosition

#include "bgfx_shader.sh"

// FogPlane.cpp binds clouds at s0 and the captured scene depth separately.
SAMPLER2D(s_texColor, 0);
SAMPLER2D(s_texReflection, 1);
uniform vec4 u_sceneFog;
uniform vec4 u_sceneCamera;
uniform vec4 u_materialColor;
// x = FogPlane::_curTime, y = Environment cloud intensity.
uniform vec4 u_materialParams;

void main()
{
    vec2 projected =
        v_projectedPosition.xy /
        max(abs(v_projectedPosition.w), 0.0001);
    vec2 screenUv =
        projected * vec2(0.5, -0.5) + vec2(0.5, 0.5);

    float sceneDepth = texture2D(s_texReflection, screenUv).r;
    vec4 sceneView = mul(
        u_invProj, vec4(0.0, 0.0, sceneDepth, 1.0));
    float sceneViewDepth =
        abs(sceneView.z / max(abs(sceneView.w), 0.0001));
    float depthDistance =
        sceneViewDepth - v_projectedPosition.z;
    float transmittance = depthDistance > 0.001
        ? 1.0 / exp(depthDistance * depthDistance *
                    u_materialParams.y)
        : 0.0;

    vec3 cloud1 =
        texture2D(s_texColor,
                  v_texcoord0 + vec2(u_materialParams.x)).rgb;
    vec3 cloud2 =
        texture2D(s_texColor,
                  v_texcoord0 - vec2(u_materialParams.x)).rgb;
    vec3 cloud = (cloud1 + cloud2) * 0.5 * u_materialColor.rgb;

    float fogFar = max(u_sceneCamera.w, 1.0);
    float fogStart = fogFar * (1.0 - clamp(u_sceneFog.a, 0.0, 1.0));
    float fog = clamp(
        (abs(v_projectedPosition.z) - fogStart) /
            max(fogFar - fogStart, 0.0001),
        0.0, 1.0);
    cloud = mix(cloud, u_sceneFog.rgb, fog);
    gl_FragColor = vec4(cloud, clamp(1.0 - transmittance, 0.0, 1.0));
}
