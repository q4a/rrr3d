$input v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);
SAMPLER2D(s_texReflection, 1);

void main()
{
    float sceneDepth = texture2D(s_texReflection, v_texcoord0).r;
    if (sceneDepth < 1.0)
        sceneDepth = 0.0;

    vec4 sceneColor = texture2D(s_texColor, v_texcoord0);
    float shaftsMask = 1.0 - sceneDepth;
    gl_FragColor = vec4(sceneColor.rgb * sceneDepth, shaftsMask);
}
