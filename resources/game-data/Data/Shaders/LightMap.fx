float numLights;

texture lightMapTex;
texture colorTex;

sampler2D lightMap = sampler_state
{
    Texture = lightMapTex;
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




float4 CombLightMapPS(in float2 screenPos: TEXCOORD0): COLOR
{
    return tex2D(lightMap, screenPos) / numLights;
}

float4 MappingLightMapPS(in float2 screenPos: TEXCOORD0): COLOR
{
    return tex2D(colorMap, screenPos) * tex2D(lightMap, screenPos);
}




technique techCombLightMap
{
    pass p0
    {
      PixelShader = compile ps_3_0 CombLightMapPS();
    }
}

technique techMappingLightMap
{
    pass p0
    {
      PixelShader = compile ps_3_0 MappingLightMapPS();
    }
}