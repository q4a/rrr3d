float4x4 matWVP;
float4x4 matWorldView;
float4x4 matInvProj;

float time;
float3 cloudColor;
float cloudIntens;

//x - near
//y - far
//z - switch on
float3 fogParams;
float3 fogColor;

texture depthTex;

sampler depthMap = sampler_state
{ 
    Texture = (depthTex);
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};

sampler cloudsMap: register(s0);

struct fogVSIn
{
    float4 pos: POSITION;
    float2 tex: TEXCOORD0;
};

struct fogVSOut
{
    float4 pos:      POSITION;
    float2 tex:      TEXCOORD0;
    float4 projPos:  TEXCOORD1;
};

struct fogPsIn
{
    float2 tex:  TEXCOORD0;
    float4 projPos: TEXCOORD1;
};

fogVSOut VolumeFogVS(fogVSIn input)
{
    fogVSOut output;

    output.pos = mul(input.pos, matWVP);
    output.tex = input.tex;
	output.projPos = output.pos;
	output.projPos.z = -mul(input.pos, matWorldView).z;
    
    return output;
}

float4 VolumeFogPS(fogPsIn input): COLOR
{
    float2 projTC = input.projPos.xy / input.projPos.w;
    projTC.y = -projTC.y;
    projTC.xy = 0.5f * projTC.xy + 0.5f;
	
	float scDepth = tex2D(depthMap, projTC).r;
	float2 scDepthCoord = mul(float4(0, 0, scDepth, 1), matInvProj).zw;
	scDepth = -scDepthCoord.x/scDepthCoord.y;
	
    float dist = scDepth - input.projPos.z;
    float fog = dist > 0.001f ? 1.0f / exp(dist * dist * cloudIntens) : 0.0f;
	float fog2 = saturate((abs(input.projPos.z) - fogParams.x) / (fogParams.y - fogParams.x)) * fogParams.z;	
	
    float3 cloud1 = tex2D(cloudsMap, input.tex + time).rgb;
    float3 cloud2 = tex2D(cloudsMap, input.tex - time).rgb;
    float3 cloud = (cloud1 + cloud2) / 2.0f * cloudColor;
	
	return float4(cloud.rgb * (1.0f - fog2) + fogColor * fog2, 1.0f - fog);
}

technique techVolumeFog
{
    pass p0
    {
        AlphaBlendEnable = true;
		BlendOp = Add;
		DestBlend = InvSrcAlpha;
		SrcBlend = SrcAlpha;
		
        VertexShader = compile vs_3_0 VolumeFogVS();
        PixelShader = compile ps_3_0 VolumeFogPS();
    }
}