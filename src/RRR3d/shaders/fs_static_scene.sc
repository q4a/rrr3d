$input v_color0, v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);
uniform vec4 u_materialColor;

void main()
{
    gl_FragColor =
        texture2D(s_texColor, v_texcoord0) * v_color0 * u_materialColor;
}
