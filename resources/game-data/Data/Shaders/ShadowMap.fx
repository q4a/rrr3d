//ѕроблемы, которые были:
//1. двойна€ нормализаци€ (при вычислении глубины делить на w не нужно, это уже сделно в матрице
//2.¬от эти элементы:
//_splitLightProjMat[numSplit]._33 /= fLightFar;
//_splitLightProjMat[numSplit]._43 /= fLightFar;
//ѕри больших размерах уровн€ принимают маленькие значени€ и обрезаютс€ (т.к. и fLightFar большой и _33, _43 маленькие)
//Ќужно следить за near plane ист. света
//

const float SHADOW_EPS = 0.000f;

float4x4 matWVP;
float4x4 matShadow;

texture shadowTex;

//x - размер карты теней
//y - размер тексел€ карты теней
float2 sizeShadow;

sampler2D shadowMap = sampler_state
{
    Texture = shadowTex;
    MinFilter = POINT;
    MagFilter = POINT;
    MipFilter = NONE;
    AddressU = Border;
    AddressV = Border;
    BorderColor = 0; //¬се что выходит за пределы frustum-а считаетс€ в тени
    //BorderColor = 0xFFFFFFFF; //¬се что выходит за пределы frustum-а считаетс€ освещенным
};




void LightMapVS(in float4 pos : POSITION, out float4 oPos : POSITION, out float4 vShadowTex: TEXCOORD0)
{
    oPos = mul(pos, matWVP);
    
    vShadowTex = mul(pos, matShadow);
}

float4 LightMapPS(float4 shadowTex: TEXCOORD0): COLOR
{
    //¬ершина в пространстве ист. света может уехать за еЄ nearPlane, что приведет к 
    //белым полосам за пределами пирамиды ист. света на карте теней, поэтому 
    //отрицательные значени€ обнул€ем. ƒелаетс€ в пиксельном шейдере после 
    //интерпол€ции!
    shadowTex.z = max(shadowTex.z, 0);
    shadowTex.xy /= shadowTex.w;

    float fShadow[4];
    fShadow[0] = (shadowTex.z - SHADOW_EPS < tex2D(shadowMap, shadowTex).x);
    fShadow[1] = (shadowTex.z - SHADOW_EPS < tex2D(shadowMap, shadowTex + float2(sizeShadow.y, 0)).x);
    fShadow[2] = (shadowTex.z - SHADOW_EPS < tex2D(shadowMap, shadowTex + float2(0, sizeShadow.y)).x);
    fShadow[3] = (shadowTex.z - SHADOW_EPS < tex2D(shadowMap, shadowTex + float2(sizeShadow.y, sizeShadow.y)).x);
  
    float2 vLerpFactor = frac(sizeShadow.x * shadowTex);
    float fLightingFactor = lerp(lerp(fShadow[0], fShadow[1], vLerpFactor.x),
                                 lerp(fShadow[2], fShadow[3], vLerpFactor.x),
                                 vLerpFactor.y);

    return float4(fLightingFactor, fLightingFactor, fLightingFactor, 1.0f);
}




technique techLightMap
{  

    pass p0
    {
       VertexShader = compile vs_3_0 LightMapVS();
       PixelShader = compile ps_3_0 LightMapPS();
    }
}