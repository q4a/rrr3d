//¬клад bloom-а в общую €ркость
float  blurblendFactor  = 0.5f;
//»нтенсивность размыти€ бликов
float  fGaussianScalar  = 10.0f;
//яркость HDR. »ли иначе €ркость сцены. Ѕольшие значени€ соотв. дню, меньшие < 3 - ночи
float  exposure         = 3.0f;
//÷ветова€ коррекци€
float2 colorCorrection = float2(1.0f, 0.0f);

texture colorTex;
texture lumTex;
texture bloomTex;

sampler2D colorMap = sampler_state
{
    Texture = colorTex;
    MinFilter = POINT;
    MagFilter = POINT;
    MipFilter = NONE;
};

sampler2D lumMap = sampler_state
{
    Texture = lumTex;
    MinFilter = POINT;
    MagFilter = POINT;
    MipFilter = NONE;
};

sampler2D bloomMap  = sampler_state
{
    Texture = bloomTex;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    MipFilter = NONE;
};



float4 FinalPassNoLum(float2 Tex : TEXCOORD0) : COLOR
{
    float4 col = tex2D(colorMap, Tex);
    float3 bloom = tex2D(bloomMap, Tex);
    col.rgb += blurblendFactor * bloom;

    return float4(saturate(col.rgb), 1.0f);
}

float4 FinalPass(float2 Tex : TEXCOORD0) : COLOR
{
    float4 col = tex2D(colorMap, Tex);
    float4 lum = tex2D(lumMap, float2(0.5f, 0.5f));
    float3 bloom = tex2D(bloomMap, Tex);    

    col.rgb += blurblendFactor * bloom;

    float Lp = (exposure / lum.r) * max(col.r, max(col.g, col.b));
    float LmSqr = (lum.g + fGaussianScalar * lum.g) * (lum.g + fGaussianScalar * lum.g);
    float toneScalar = (Lp * (1.0f + (Lp / LmSqr)))/(1.0f + Lp);    
    col = col * toneScalar;

    return float4(saturate(col.rgb * colorCorrection.x + colorCorrection.yyy), 1.0f);
	//return float4(saturate(col.rgb), 1.0f);
}




technique techFinalPassNoLum
{
    pass p0
    {
        PixelShader = compile ps_3_0 FinalPassNoLum();
    }
}

technique techFinalPass
{
    pass p0
    {
        PixelShader = compile ps_3_0 FinalPass();
    }
}