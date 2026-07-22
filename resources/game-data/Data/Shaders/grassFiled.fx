//ќтрисовка массива спрайтов трав за один батч
//-ћожно избавитьс€ от пикс. шейдера

float3 offset;
float4x4 matWorldView;
float4x4 wvpMatrix;
float4x4 viewMatInv;

//x - near
//y - far
//z - switch on
float3 fogParams;
float3 fogColor;

float3 diffLight;

sampler2D colorMap: register(s0);

//ќтносительные координаты вершин спрайта в системе координат камеры
static const float3 spriteBuf[4] = {float3(-1.0f, -1.0f, 0.0f),
                             float3( 1.0f, -1.0f, 0.0f),
                             float3( 1.0f,  1.0f, 0.0f),
                             float3(-1.0f,  1.0f, 0.0f)};

void GrassFieldVS(in float4 iPos:POSITION, in float2 iTexCoord:TEXCOORD0, out float4 oPos:POSITION, out float3 oTexCoord:TEXCOORD0)
{
    //ѕереводим вершины спрайтов в мировое пространство как векторы     
    //w компонента содержит индекс вершины заданны при построении треугольника
    oPos = mul(float4(spriteBuf[iPos.w], 0), viewMatInv);
    //xyz компоненты содержат позицию в мировом пространстве
    //ƒобавл€ем позицию в world
    oPos.xyz += iPos.xyz + offset;
    oPos.w = 1.0f;

    //—тандартные операции
    oTexCoord.xy = iTexCoord;
	oTexCoord.z = mul(oPos, matWorldView).z;
	oPos = mul(oPos, wvpMatrix);
}

float4 GrassFieldPS(float3 texCoord:TEXCOORD0): COLOR
{
	float fog = saturate((abs(texCoord.z) - fogParams.x) / (fogParams.y - fogParams.x)) * fogParams.z;	
	float4 color = tex2D(colorMap, texCoord.xy);

    return float4(color.rgb * diffLight * (1.0f - fog) + fogColor * fog, color.a);
}




technique techGrassField
{
    pass p0
    {
        cullMode = NONE;

        VertexShader = compile vs_3_0 GrassFieldVS();
        PixelShader = compile ps_3_0 GrassFieldPS();
    }
}