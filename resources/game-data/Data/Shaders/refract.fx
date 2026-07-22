float4x4	mWorldViewProj;  // World * View * Projection transformation
float 	vScene;

texture refrTex;
texture sceneTex;

sampler2D refraction = sampler_state
{
    Texture = refrTex;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    MipFilter = LINEAR;
};

sampler2D scene = sampler_state
{
    Texture = sceneTex;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    MipFilter = NONE;

    AddressU = MIRROR;
    AddressV = MIRROR;  
};




struct VS_INPUT
{
	float4 Position  : POSITION;
	float2 Texcoord0 : TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 Position : POSITION;
	float2 Texcoord0 : TEXCOORD0;
	float4 PositionScreen : TEXCOORD1;
};

VS_OUTPUT RefractVS(VS_INPUT In)
{
	VS_OUTPUT Out;

	Out.Position       = mul(In.Position, mWorldViewProj);
	Out.Texcoord0      = In.Texcoord0;
	Out.PositionScreen = Out.Position;

	return Out;
}




struct PS_INPUT
{
	float2 Texcoord0 : TEXCOORD0;
	float4 PositionScreen : TEXCOORD1;
};

struct PS_OUTPUT
{
	float4 Color : COLOR;
};

PS_OUTPUT RefractPS(PS_INPUT In)
{
	PS_OUTPUT Out;
	
	float4 vRefraction = tex2D(refraction, In.Texcoord0.xy);
	vRefraction.xy = vRefraction.xy * 2.0f - 1.0f;
	
	float2 vSceneTexcoord = 0.5f * In.PositionScreen.xy / In.PositionScreen.w + 0.5f;
      vSceneTexcoord.y = 1.0f - vSceneTexcoord.y;
	vSceneTexcoord += vRefraction.xy * vScene;
	
	Out.Color = tex2D(scene, vSceneTexcoord);
      Out.Color.a = 1.0f;
	
	return Out;
}




technique techRefract
{
    pass p0
    {
		ZWriteEnable = false;
		ZEnable = true;
	
        VertexShader = compile vs_3_0 RefractVS();
        PixelShader = compile ps_3_0 RefractPS();
    }
}