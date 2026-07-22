//Ёмул€ци€ ALPHATEST в шейдере
//#define ALPHATEST

#define DIFF_TEX 1
#define NORMAL 1
#define SCREEN_POS 1
#define FOG_ENABLE

#include "lighting.fx"

float reflectivity;

texture reflTex;

texture diffTex;

sampler2D diffMap: register(s0) = sampler_state
{
	Texture = diffTex;
};

sampler2D reflMap = sampler_state
{
    Texture = reflTex;
    AddressU = MIRROR;
    AddressV = MIRROR; 
    MipFilter = NONE;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};




float4 ReflMappPS(ModelPSIn modelIn): COLOR
{
    //“.к. альфа блендинг просто так не симулировать, 
    //то используем только те форматы в которых он работает
    //Ёмулирование альфа теста в шейдере
#ifdef ALPHATEST
    if (tex2D(diffMap, modelIn.diffTex).a < 0.5f)
       discard;
#endif

    ModelPSOut modelOut;
    CompModelPSOut(modelIn, modelOut);

    float4 texColor = tex2D(diffMap, modelIn.diffTex);
	float4 reflColor = tex2D(reflMap, float2(modelOut.screenPos.x, 1.0f - modelOut.screenPos.y));
	
	float kRefl = reflectivity * reflColor.a;
	texColor.rgb = texColor.rgb * (1 - kRefl) + reflColor.rgb * kRefl;
	
    float3 light = CompSpotLight(modelOut, texColor);

    return float4(light, texColor.a);
}




technique techReflMapp
{
    pass p0
    {

//»спользуем эмул€цию альфа теста
#ifdef ALPHATEST
       AlphaTestEnable = false;
#endif

       VertexShader = compile vs_3_0 ModelVS();
       PixelShader = compile ps_3_0 ReflMappPS();
    }
}