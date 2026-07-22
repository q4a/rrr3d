#define DIFF_TEX 1
#define NORMAL 1
#define REFL_VECTOR 1
#define FOG_ENABLE

#include "lighting.fx"

float reflectivity;
float3 alphaBlendColor;

texture envTex;

texture diffTex;

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

float4 ReflMappPS(ModelPSIn modelIn): COLOR
{
    ModelPSOut modelOut;
    CompModelPSOut(modelIn, modelOut);

    float4 texColor = tex2D(diffMap, modelIn.diffTex);
    float3 difColor = texColor.rgb * texColor.a + alphaBlendColor * (1 - texColor.a);
    difColor *= (1 - reflectivity);

    float3 reflColor = texCUBE(envMap, modelIn.reflVec) * reflectivity;

    float3 light = CompSpotLight(modelOut, difColor + reflColor);

    return float4(light, texColor.a);
}

float4 ReflMappMenuPS(ModelPSIn modelIn): COLOR
{
    ModelPSOut modelOut;
    CompModelPSOut(modelIn, modelOut);
	
	float4 texColor = tex2D(diffMap, modelIn.diffTex);
	float3 reflColor = alphaBlendColor * (1 - reflectivity) + texCUBE(envMap, modelIn.reflVec) * reflectivity;
    float3 diffColor = texColor.rgb * texColor.a + reflColor * (1 - texColor.a);
    float3 light = CompSpotLight(modelOut, diffColor);

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

technique techReflMappMenu
{
    pass P0
    {
        VertexShader = compile vs_3_0 ModelVS();
        PixelShader  = compile ps_3_0 ReflMappMenuPS();
    }
}
