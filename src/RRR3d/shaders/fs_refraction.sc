$input v_texcoord0, v_projectedPosition

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);
SAMPLER2D(s_texReflection, 1);
// x = RefrShader::vScene = 1 - owning node animation frame.
uniform vec4 u_postParameters;

void main()
{
    vec2 distortion =
        texture2D(s_texColor, v_texcoord0).xy * 2.0 - 1.0;
    vec2 projected =
        v_projectedPosition.xy /
        max(abs(v_projectedPosition.w), 0.0001);
    vec2 sceneUv =
        projected * vec2(0.5, -0.5) + vec2(0.5, 0.5);
    sceneUv += distortion * u_postParameters.x;
    gl_FragColor = vec4(
        texture2D(s_texReflection, sceneUv).rgb, 1.0);
}
