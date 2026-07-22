#include "consts.fx"

float4 color;




float4 ClearSurf(): COLOR
{
    return color;
}

float4 FillMaxDepth(): COLOR
{
    return float4(cMaxDepth, cMaxDepth, cMaxDepth, cMaxDepth);
}




technique techClearSurf
{
    pass p0
    {
        PixelShader = compile ps_3_0 ClearSurf();
    }
}

technique techFillMaxDepth
{
    pass p0
    {
        PixelShader = compile ps_3_0 FillMaxDepth();
    }
}