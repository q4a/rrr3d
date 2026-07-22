static const float3 LUMINANCE_VECTOR = float3(0.2125f, 0.7154f, 0.0721f);

float2 sampleOffsets3x3[9];
float2 sampleOffsets4x4[16];
float  dtime;

texture lumTex;
texture lumTexOld;

sampler2D lumMap = sampler_state
{
    Texture = lumTex;
    MinFilter = POINT;
    MagFilter = POINT;
    MipFilter = NONE;
};

sampler2D lumMapOld = sampler_state
{
    Texture = lumTexOld;
    MinFilter = POINT;
    MagFilter = POINT;
    MipFilter = NONE;
};




//Компиляция с профилем ps_3_0 из-за не хватки слотов для //арифмитических операций (используется 66 а максиум 56)
float4 Down3x3LumLog(in float2 vScreenPosition : TEXCOORD0): COLOR
{
    float lum = 0.0f;
    float GreyValue = 0.0f;
    float maxium = 0.0f;
    float3 color;
    
    for (int i = 0; i < 9; i++)
    {
        // Compute the sum of color values
        color = tex2D(lumMap, vScreenPosition + sampleOffsets3x3[i]);
        lum += log(dot(color, LUMINANCE_VECTOR) + 0.0001f);   
        GreyValue = max(color.r, max(color.g, color.b));
        maxium = max(maxium, GreyValue);     
    }
    lum *= 0.111111;

    return float4(lum, maxium, 0.0f, 1.0f);
}

float4 Down4x4Lum(in float2 vScreenPosition: TEXCOORD0): COLOR
{
    float lum = 0.0f;  
    float4 color;
    float maxium = 0.0f;

    for (int i = 0; i < 16; i++)
    {
        color = tex2D(lumMap, vScreenPosition + sampleOffsets4x4[i]);
        maxium = max(maxium, color.g);
        lum += color.r;
    }
    lum *= 0.0625;

    return float4(lum, maxium, 0.0f, 1.0f);
}

float4 Down4x4LumExp(in float2 vScreenPosition: TEXCOORD0): COLOR
{
    float lum = 0.0f;
    float maxium = 0.0f;
    float4 color;

    for (int i = 0; i <16; i++)
    {
        color = tex2D(lumMap, vScreenPosition + sampleOffsets4x4[i]);
        maxium = max(maxium, color.g);
        lum += color.r;
    }
    lum *= 0.0625;
    lum = exp(lum);

    return float4(lum, maxium, 0.0f, 1.0f);
}

float4 AdaptLum(in float2 vScreenPosition: TEXCOORD0): COLOR
{
    float2 Clum = tex2D(lumMap, float2(0.5f, 0.5f));
    float2 Alum = tex2D(lumMapOld, float2(0.5f, 0.5f));
    
    float2 col = Alum + (Clum - Alum) * (1.0f - pow(0.98f, 30.0f *  dtime));
    //float2 col = pow(pow(Alum, 0.25f) + (pow(Clum, 0.25f) - pow(Alum, 0.25f))*(1.0f - pow(0.98f, 30.0f * dtime)), 4.0f);

    col.x = clamp(col.x, 0.5f, 5.0f);

    return float4(col.x, col.y, 0.0f, 1.0f);
}

    


technique techDown3x3LumLog
{
    pass p0
    {
        PixelShader = compile ps_3_0 Down3x3LumLog();
    }
}

technique techDown4x4Lum
{
    pass p0
    {
        PixelShader = compile ps_3_0 Down4x4Lum();
    }
}

technique techDown4x4LumExp
{
    pass p0
    {
        PixelShader = compile ps_3_0 Down4x4LumExp();
    }
}

technique techAdaptLum
{
    pass p0
    {
        PixelShader = compile ps_3_0 AdaptLum();
    }
}