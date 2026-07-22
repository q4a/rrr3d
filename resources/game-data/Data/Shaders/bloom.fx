float  lumKey           = 4.0f; //Изменение интенсивности bloom-а
float  BRIGHT_THRESHOLD = 4.5f; //Уровень обнуления цветов для bloom-а
float  BRIGHT_OFFSET    = 1.0f;
float  staticLum        = 0.55f;

float2 sampleOffsets4x4[16];
float  sampleWeights4x4[16];

texture colorTex;
texture lumTex;

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




float4 Down4x4BrightPass(in float2 vScreenPosition : TEXCOORD0, float lum) : COLOR
{
    float3 color = float3(0.0f, 0.0f, 0.0f);        
    for(int i = 0; i < 16; i++)   
        color += tex2D(colorMap, vScreenPosition + sampleOffsets4x4[i]);
    color *= 0.0625;

    color *= lumKey / lum;
    color -= BRIGHT_THRESHOLD;
    color = max(0.0f, color);
    color /= (BRIGHT_OFFSET + color);
	
    return float4(color, 1.0f);
}

float4 Down4x4BrightPassStaticPS(in float2 vScreenPosition : TEXCOORD0) : COLOR
{
    return Down4x4BrightPass(vScreenPosition, staticLum);
}

float4 Down4x4BrightPassPS(in float2 vScreenPosition : TEXCOORD0) : COLOR
{
    float fLum = tex2D(lumMap, float2(0.5f, 0.5f)).x;
    return Down4x4BrightPass(vScreenPosition, fLum + 0.001f);
}

float4 BloomPS( in float2 vScreenPosition : TEXCOORD0 ) : COLOR
{
    
    float4 vSample = 0.0f;
    float4 vColor = 0.0f;
    
    for( int iSample = 0; iSample < 15; iSample++ )
    {
        // Sample from adjacent points
        vColor = tex2D(colorMap, vScreenPosition + sampleOffsets4x4[iSample]);
        vSample += vColor * sampleWeights4x4[iSample];
    }
    
    return vSample;
} 

    


technique techDown4x4BrightPassNoLum
{
    pass p0
    {
        PixelShader = compile ps_3_0 Down4x4BrightPassStaticPS();
    }
}

technique techDown4x4BrightPass
{
    pass p0
    {
        PixelShader = compile ps_3_0 Down4x4BrightPassPS();
    }
}

technique techBloom
{
    pass p0
    {
        PixelShader = compile ps_3_0 BloomPS();
    }
}