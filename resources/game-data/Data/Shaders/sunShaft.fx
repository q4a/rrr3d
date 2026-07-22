#include "consts.fx"

static const float4 shaftParams = float4(0.1f, 2.0f, 0.1f, 2.0f);
static const float4 sunColor = float4(0.9f, 0.8f, 0.6f, 1.0f);

float4 sunPos;

texture depthTex;
texture colorTex;
texture colorBlurTex;

sampler2D depthMap = sampler_state
{
    Texture = depthTex;
    MinFilter = POINT;
    MagFilter = POINT;
    MipFilter = NONE;
};

sampler2D colorMap = sampler_state
{
    Texture = colorTex;
    MinFilter = POINT;
    MagFilter = POINT;
    MipFilter = NONE;
};

sampler2D colorBlurMap= sampler_state
{
    Texture = colorBlurTex;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    MipFilter = NONE;
};




float4 PrepareShaftsPS(in float2 tex: TEXCOORD0): COLOR
{
    float sceneDepth = tex2D(depthMap, tex).x;
    if (sceneDepth < 1.0f)
        sceneDepth = 0.0f;
		
    float4 sceneColor = tex2D(colorMap, tex);
    float shaftsMask = 1.0f - sceneDepth;

    //return float4(1.0f.xxx * sceneDepth, shaftsMask);
	return float4(sceneColor.rgb * sceneDepth, shaftsMask);
}




float4 lessThan(float4 v1, float4 v2)
{
    return float4(v1.x < v2.x, v1.y < v2.y, v1.z < v2.z, v1.w < v2.w);
}

float4 blendSoftLight(float4 a, float4 b)
{
    float4 c = 2.0f * a * b + a * a * (1.0f - 2.0f * b);
    float4 d = sqrt(a) * (2.0f * b - 1.0f) + 2.0f * a * (1.0f - b);

    //return (b < 0.5f) ? c : d;
    return any(lessThan(b, 0.5f.xxxx)) ? c : d;
}

float4 GenShaftsPS(float2 texCoord: TEXCOORD0): COLOR
{
    float2 sunPosProj = sunPos.xy;
    sunPosProj.y = -sunPosProj.y;
    float sign = sunPos.w;

    float2 sunVec = sunPosProj.xy - (texCoord.xy - float2(0.5f, 0.5f));
    float sunDist = saturate(sign) * saturate(1.0f - saturate(length(sunVec) * shaftParams.y));
    sunVec *= shaftParams.x * sign;

    float4 accum;
    float2 tc = texCoord;

    tc += sunVec;
    accum = tex2D(colorBlurMap, tc);
    tc += sunVec;
    accum += tex2D(colorBlurMap, tc) * 0.875f;
    tc += sunVec;
    accum += tex2D(colorBlurMap, tc) * 0.75f;
    tc += sunVec;
    accum += tex2D(colorBlurMap, tc) * 0.625f;
    tc += sunVec;
    accum += tex2D(colorBlurMap, tc) * 0.5f;
    tc += sunVec;
    accum += tex2D(colorBlurMap, tc) * 0.375f;
    tc += sunVec;
    accum += tex2D(colorBlurMap, tc) * 0.25f;
    tc += sunVec;
    accum += tex2D(colorBlurMap, tc) * 0.125f;

    accum *= 0.25f * float4(sunDist.xxx, 1.0f);
    accum.w += 1.0f - saturate(sign * 0.1f + 0.9f);

    float4 cScreen = tex2D(colorMap, texCoord);
    float4 cSunShafts = accum;
    float shatsMask = saturate(1.1f - cSunShafts.w) * shaftParams.z * 2.0f;    

    accum = cScreen + cSunShafts.xyzz * shaftParams.w * sunColor * (1.0f - cScreen);
    accum = blendSoftLight(accum, sunColor * shatsMask * 0.5f + 0.5f);    

    return accum;
}




technique techPrepareShafts
{
    pass p0
    {
        PixelShader = compile ps_3_0 PrepareShaftsPS();
    }
}

technique techGenShafts
{
    pass p0
    {
        PixelShader = compile ps_3_0 GenShaftsPS();
    }
}