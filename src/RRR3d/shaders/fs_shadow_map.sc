$input v_texcoord0, v_worldPosition, v_linearDepth

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);
uniform vec4 u_materialParams;
uniform vec4 u_materialOptions;
uniform vec4 u_clipPlane;

void main()
{
    if (u_materialOptions.w > 0.5 &&
        dot(vec4(v_worldPosition, 1.0), u_clipPlane) < 0.0)
        discard;
    float alpha = texture2D(s_texColor, v_texcoord0).a;
    if (u_materialParams.x > 0.0 &&
        alpha <= u_materialParams.x)
        discard;
    gl_FragColor = vec4(
        clamp(v_linearDepth, 0.0, 1.0),
        0.0, 0.0, 1.0);
}
