#define DIFF_TEX 1
#define TANGENT_SPACE 1
#define FOG_ENABLE

#include "lighting.fx"

texture normTex;

texture diffTex;

sampler2D diffMap: register(s0) = sampler_state
{
	Texture = diffTex;
};

sampler2D normMap = sampler_state
{
    Texture = normTex;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};




float4 BumpMapPS(ModelPSIn modelIn): COLOR
{
    ModelPSOut modelOut;
    CompModelPSOut(modelIn, modelOut);

    float4 texColor = tex2D(diffMap, modelIn.diffTex);
    float3 normal = normalize(tex2D(normMap, modelIn.diffTex).xyz * 2.0f - 1.0f);
   
    float3 light = CompSpotLight(modelOut, texColor.rgb, normal);

    return float4(light, texColor.a);
}




technique techBumpMap
{
    pass P0
    {
       VertexShader = compile vs_3_0 ModelVS();
       PixelShader  = compile ps_3_0 BumpMapPS();
    }
}
