float4x4 matWorld;
float4x4 matWorldView;
float4x4 matWVP;
float4x4 matInvProj;

float time;
float normalScale;
float4 waterColor;
float3 lightPos;
float3 viewPos;
float cloudIntens;

//x - near
//y - far
//z - switch on
float3 fogParams;
float3 fogColor;

texture depthTex;
texture normalTex;
texture reflTex;

sampler depthMap = sampler_state
{ 
    Texture = depthTex;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};

sampler normalMap = sampler_state
{ 
    Texture = normalTex;
    AddressU = WRAP;
    AddressV = WRAP;  
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};

sampler reflMap = sampler_state
{ 
    Texture = reflTex; 
    AddressU = MIRROR;
    AddressV = MIRROR;  
    MipFilter = NONE;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};

struct VS_Input
{
    float4 pos: POSITION;
    float2 tex: TEXCOORD0;
};

struct VS_Output
{
    float4 pos:      POSITION;
    float2 tex:      TEXCOORD0;
    float3 worldpos: TEXCOORD1;
    float4 projPos:  TEXCOORD2;
};

struct PS_Input
{
    float2 tex:  TEXCOORD0;
    float3 worldpos: TEXCOORD1;
    float4 projPos: TEXCOORD2;
};

VS_Output WaterVS(VS_Input input)
{
    VS_Output output;

    output.worldpos = mul(input.pos, matWorld);	
    output.pos = mul(input.pos, matWVP);
    output.projPos = output.pos;
	output.projPos.z = -mul(input.pos, matWorldView).z;
    output.tex = input.tex;
    
    return output;
}

float4 WaterPS(PS_Input input): COLOR
{
    float2 normalMapTex = input.tex;
    float3 normal1 = 2.0f * tex2D(normalMap, normalMapTex * normalScale + time).xyz - 1.0f; 
    float3 normal2 = 2.0f * tex2D(normalMap, normalMapTex * normalScale * 0.5f + time).xyz - 1.0f;   
    float3 normal = normalize(normal1 + normal2 + float3(0.0f, 0.0f, 1.0f));
  
    float3 lpnorm = normalize(lightPos - input.worldpos);
    float3 vpnorm = normalize(viewPos - input.worldpos);
    float2 proj_tc = 0.5f * input.projPos.xy / input.projPos.w + 0.5f;
    float frensel = 1.0f - dot(vpnorm, normal);

    float fRdotL = max(dot(normalize(lpnorm + vpnorm), normal), 0);    
    float VdotR = pow(fRdotL, 128.0f);
    float3 specular = float3(0.5f, 0.5f, 0.5f) * VdotR;
	
	float scDepth = tex2D(depthMap, float2(proj_tc.x, 1.0f - proj_tc.y)).r;
	float2 scDepthCoord = mul(float4(0, 0, scDepth, 1), matInvProj).zw;
	scDepth = -scDepthCoord.x/scDepthCoord.y;
	float dist = scDepth - input.projPos.z;
    float fog2 = dist > 0.001f ? 1.0f / exp(dist * dist * cloudIntens) : 0.0f;

    float2 vDistort = normal.xy * 0.07f;
    float3 reflection = tex2D(reflMap, proj_tc.xy + vDistort).xyz;
	float fog = saturate((abs(input.projPos.z) - fogParams.x) / (fogParams.y - fogParams.x)) * fogParams.z;	
	
	float3 color = waterColor * waterColor.w * (1.0f - frensel) + reflection * frensel + specular;
	color = color * (1.0f - fog) + fogColor * fog;
	
    return float4(color, 1.0f - fog2);
	//return float4(tex2D(depthMap, float2(proj_tc.x, 1.0f - proj_tc.y)).xxx, 1.0f);
}

technique techWater
{
    pass p0
    {
		AlphaBlendEnable = true;
		BlendOp = Add;
		DestBlend = InvSrcAlpha;
		SrcBlend = SrcAlpha;		
        
        VertexShader = compile vs_3_0 WaterVS();
        PixelShader = compile ps_3_0 WaterPS();
    }
}