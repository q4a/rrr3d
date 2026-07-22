#define DIFF_TEX 1
#define TANGENT_SPACE 1
#define VIEW_VERT 1
#define FOG_ENABLE

#include "lighting.fx"

float reflectivity;
float3 alphaBlendColor;

texture envTex;
texture diffTex;
texture normTex;

sampler2D diffMap: register(s0) = sampler_state
{
	Texture = diffTex;
};

samplerCUBE envMap = sampler_state
{
    Texture = envTex;
    MipFilter = Linear;
    MinFilter = Linear;
    MagFilter = Linear;
};

sampler2D normMap = sampler_state
{
    Texture = normTex;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};




float4 ReflMappPS(ModelPSIn modelIn): COLOR
{
    ModelPSOut modelOut;
    CompModelPSOut(modelIn, modelOut);

    float4 texColor = tex2D(diffMap, modelIn.diffTex);
	float3 normal = normalize(tex2D(normMap, modelIn.diffTex).xyz * 2.0f - 1.0f);
	float reflVec = reflect(normalize(modelOut.viewVert), normal);
	
    float3 difColor = texColor.rgb * texColor.a + alphaBlendColor * (1 - texColor.a);
    difColor *= (1 - reflectivity);
	
    float3 reflColor = texCUBE(envMap, reflVec) * reflectivity;

    float3 light = CompSpotLight(modelOut, difColor + reflColor, normal);

    return float4(light, texColor.a);
}

technique techReflMapp
{
    pass P0
    {
        VertexShader = compile vs_3_0 ModelVS();
        PixelShader  = compile ps_3_0 ReflMappPS();
    }
}
