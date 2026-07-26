$input v_direction

#include "bgfx_shader.sh"

SAMPLERCUBE(s_texEnvironment, 3);

void main()
{
    gl_FragColor =
        vec4(textureCube(s_texEnvironment, normalize(v_direction)).rgb,
             1.0);
}
