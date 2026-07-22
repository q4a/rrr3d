//В будущем добавить кодирование глубины в 32 битный ARGB, запись в обычные floating point (16 и 32) оставить
//Возможно небходимо убрать рендер backfaces

float4x4 depthMatrix;

texture opacityTex;

sampler2D opacityMap = sampler_state
{
    Texture = opacityTex;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    MipFilter = LINEAR;
};




void DepthMapVS(in float4 Pos : POSITION, out float4 oPos : POSITION, out float2 Depth : TEXCOORD0)
{
    oPos = mul(Pos, depthMatrix);
    Depth = oPos.zw;
}

float4 DepthMapPS(in float2 Depth: TEXCOORD0): COLOR
{
    return Depth.x/Depth.y;
}




void DepthMapAlphaVS(in float4 Pos: POSITION, in float2 tex: TEXCOORD0, out float4 oPos : POSITION, out float2  texObj: TEXCOORD0, out float2 Depth : TEXCOORD1)
{
    oPos = mul(Pos, depthMatrix);
    texObj = tex;
    Depth = oPos.zw;
}

float4 DepthMapAlphaTestPS(in float2 texObj: TEXCOORD0, in float2 Depth: TEXCOORD1): COLOR
{
    //Пиксель текстуры используется по максиуму поэтому альфа тест здесь. Также альфа тест не работает с floating текстурами (такие форматы могут использоваться)
    if (tex2D(opacityMap, texObj).a < 0.5f)
        discard;
	//return Depth.x;	
    return Depth.x/Depth.y;
}




technique techDepthMap
{
    pass p0
    {
        VertexShader = compile vs_3_0 DepthMapVS();
        PixelShader = compile ps_3_0 DepthMapPS();
    }
}

technique techDepthMapAlphaTest
{
    pass p0
    {
        //Нельзя чтобы альфа тест накладывался на альфа тест в шейдере
        AlphaTestEnable = false;

        VertexShader = compile vs_3_0 DepthMapAlphaVS();
        PixelShader = compile ps_3_0 DepthMapAlphaTestPS();
    }
}
