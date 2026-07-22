//Возможная оптимизация, не размывать тексели которые не относятся к теням(белый цвет)
texture colorTex;

sampler2D colorMap = sampler_state
{
    Texture = colorTex;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    MipFilter = NONE;    
};

float2 colorTexSizes;
static const int blurRadius = 4;




float4 GaussianBlurPS(float2 tex: TEXCOORD0): COLOR
{
    float fMultiplier = blurRadius + 1.0f;
    float4 vColor = tex2D(colorMap, tex) * fMultiplier;

    for (int i = 1; i < blurRadius; i++)
    {
        float fStep = i;
        float fScale = (1.0f + blurRadius - fStep);
        vColor += tex2D(colorMap, tex + colorTexSizes.xy * fStep) * fScale;
        vColor += tex2D(colorMap, tex - colorTexSizes.xy * fStep) * fScale;
        fMultiplier += fScale * 2.0f;
    }
    
    return float4(vColor.xyz / fMultiplier, 1.0f);
}

technique techGaussianBlur
{
    pass p0
    {
       PixelShader = compile ps_3_0 GaussianBlurPS();
    }
}