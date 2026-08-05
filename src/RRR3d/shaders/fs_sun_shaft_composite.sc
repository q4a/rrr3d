$input v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);
SAMPLER2D(s_texReflection, 1);
// xy = projected source SunShaft::sunPos, z = source front-facing sign.
uniform vec4 u_postParams;

vec4 blendSoftLight(vec4 first, vec4 second)
{
    vec4 low =
        2.0 * first * second + first * first * (1.0 - 2.0 * second);
    vec4 high =
        sqrt(first) * (2.0 * second - 1.0) +
        2.0 * first * (1.0 - second);
    // The original shader deliberately chooses one branch for the complete
    // float4 through HLSL any(), instead of selecting per component.
    bool anyLess = second.x < 0.5 || second.y < 0.5 ||
                   second.z < 0.5 || second.w < 0.5;
    return anyLess ? low : high;
}

void main()
{
    const vec4 shaftParams = vec4(0.1, 2.0, 0.1, 2.0);
    const vec4 sunColor = vec4(0.9, 0.8, 0.6, 1.0);

    vec2 projectedSun = u_postParams.xy;
    projectedSun.y = -projectedSun.y;
    float signValue = u_postParams.z;
    vec2 sunVector =
        projectedSun - (v_texcoord0 - vec2(0.5, 0.5));
    float sunDistance =
        clamp(signValue, 0.0, 1.0) *
        clamp(1.0 - clamp(length(sunVector) * shaftParams.y,
                          0.0, 1.0),
              0.0, 1.0);
    sunVector *= shaftParams.x * signValue;

    vec2 coordinate = v_texcoord0 + sunVector;
    vec4 accumulated = texture2D(s_texReflection, coordinate);
    coordinate += sunVector;
    accumulated += texture2D(s_texReflection, coordinate) * 0.875;
    coordinate += sunVector;
    accumulated += texture2D(s_texReflection, coordinate) * 0.75;
    coordinate += sunVector;
    accumulated += texture2D(s_texReflection, coordinate) * 0.625;
    coordinate += sunVector;
    accumulated += texture2D(s_texReflection, coordinate) * 0.5;
    coordinate += sunVector;
    accumulated += texture2D(s_texReflection, coordinate) * 0.375;
    coordinate += sunVector;
    accumulated += texture2D(s_texReflection, coordinate) * 0.25;
    coordinate += sunVector;
    accumulated += texture2D(s_texReflection, coordinate) * 0.125;

    accumulated *=
        0.25 * vec4(sunDistance, sunDistance, sunDistance, 1.0);
    accumulated.w +=
        1.0 - clamp(signValue * 0.1 + 0.9, 0.0, 1.0);

    vec4 screenColor = texture2D(s_texColor, v_texcoord0);
    float shaftsMask =
        clamp(1.1 - accumulated.w, 0.0, 1.0) *
        shaftParams.z * 2.0;
    vec4 result =
        screenColor + accumulated.xyzz * shaftParams.w * sunColor *
                          (1.0 - screenColor);
    result = blendSoftLight(
        result, sunColor * shaftsMask * 0.5 + 0.5);
    gl_FragColor = result;
}
