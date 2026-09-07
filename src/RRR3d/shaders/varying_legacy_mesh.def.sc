vec3 v_normal    : NORMAL    = vec3(0.0, 0.0, 1.0);
vec2 v_texcoord0 : TEXCOORD0 = vec2(0.0, 0.0);
vec3 v_worldPosition : TEXCOORD1 = vec3(0.0, 0.0, 0.0);
vec4 v_reflectionPosition : TEXCOORD2 = vec4(0.0, 0.0, 0.0, 1.0);
vec4 v_shadowPosition : TEXCOORD3 = vec4(0.0, 0.0, 0.0, 1.0);
float v_linearDepth : TEXCOORD4 = 0.0;
vec3 v_tangent : TEXCOORD5 = vec3(1.0, 0.0, 0.0);
vec3 v_bitangent : TEXCOORD6 = vec3(0.0, 1.0, 0.0);
vec4 v_shadowPositionFar : TEXCOORD7 = vec4(0.0, 0.0, 0.0, 1.0);
vec4 v_color0 : COLOR0 = vec4(1.0, 1.0, 1.0, 1.0);

vec3 a_position  : POSITION;
vec3 a_normal    : NORMAL;
vec2 a_texcoord0 : TEXCOORD0;
vec3 a_tangent   : TANGENT;
vec3 a_bitangent : BITANGENT;
vec4 a_color0    : COLOR0;
