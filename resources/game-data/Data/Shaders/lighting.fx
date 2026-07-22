//точечный
//#define LIGHT_TYPE_SPOT
//направленный
//#define LIGHT_TYPE_POINT
//конусный
//#define LIGHT_TYPE_DIR
//Наложение теней
//#define SHADOW
//Наложение теней
//#define FOG_ENABLE

//Диффузное освещение
//#define DIFFUSE_LIGHT
//Бликовое освещение
//#define SPECULAR_LIGHT
//
//#define SPOT_LIGHT

//При включенных тенях требуются экранные координаты
#ifdef SHADOW
    #define SCREEN_POS 1
#endif

#ifdef FOG_ENABLE
    #define VIEW_VERT 1
	#define VIEW_VERT_FOG 1
#endif

#ifdef LIGHT_TYPE_DIR  
	//lightDir нужен в tangent space
	#define LIGHT_VERT_DIR 1
	#define VIEW_VERT 1
#endif
#ifdef LIGHT_TYPE_POINT
    #define LIGHT_VERT 1
	#define VIEW_VERT 1
#endif
#ifdef LIGHT_TYPE_SPOT
    #define LIGHT_VERT 1
	#define VIEW_VERT 1
	#define SPOT_LIGHT 1
	#define WORLD_POS 1
#endif

#include "model.fx"

//light
float numLights;
float3 glAmbient;
float3 ambLight;
float3 diffLight;
float3 specLight;

//spot
#if SPOT_LIGHT
//x - spotFalloff
//y - spotPhi
//z - spotTheta
//w - spotFar
float4 spotParams;
#endif

#ifdef FOG_ENABLE
//x - near
//y - far
//z - switch on
float3 fogParams;
float3 fogColor;
#endif

//material
float3 colorMat;
float3 specMat;
float specPower;
float texDiffK;

#ifdef SHADOW
texture shadowTex;

sampler2D shadowMap = sampler_state
{
    Texture = shadowTex;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    MipFilter = NONE;
    AddressU = Border;
    AddressV = Border;
    BorderColor = 0; //Все что выходит за пределы frustum-а считается в тени
    //BorderColor = 0xFFFFFFFF; //Все что выходит за пределы frustum-а считается освещенным
};
#endif




float ComputeDiff(float3 lightVert, float3 normal)
{
    return saturate(dot(lightVert, normal));
}

float CompSpec(float3 lightVert, float4 viewVert, float3 normal)
{
    float fRdotL = max(dot(normalize(lightVert + viewVert.xyz), normal), 0);
    return pow(fRdotL, specPower);
}

#if SPOT_LIGHT
float ComputeSpot(float4 lightVert)
{
    float alpha = dot(lightDir, -lightVert.xyz);
    float cosTheta2 = cos(spotParams.z/2);
    float cosPhi2 = cos(spotParams.y/2);
	float spotK = pow((alpha - cosPhi2) / (cosTheta2 - cosPhi2), spotParams.x);
	float farAtt = 1.0f - saturate((lightVert.w - spotParams.w) / (spotParams.w * spotK));
    return (alpha >= cosPhi2) * spotK * farAtt;
}
#endif

float3 CompSpotLight(ModelPSOut modelOut, float3 texColor
#if !NORMAL    
    , float3 normal
#endif
)
{
#if NORMAL
    float3 normal = modelOut.normal;
#endif

	float3 amb = glAmbient/numLights;
	float3 diff = diffLight; 
	float3 spec = specLight; 
	
#ifdef LIGHT_TYPE_DIR
	diff *= ComputeDiff(modelOut.lightVertDir, normal);
	spec *= CompSpec(modelOut.lightVertDir.xyz, modelOut.viewVert, normal);
#endif
#ifdef LIGHT_TYPE_POINT
	diff *= ComputeDiff(modelOut.lightVert.xyz, normal);
	spec *= CompSpec(modelOut.lightVert.xyz, modelOut.viewVert, normal);
#endif
#ifdef LIGHT_TYPE_SPOT
	diff *= ComputeDiff(modelOut.lightVert.xyz, normal);
	spec *= CompSpec(modelOut.lightVert.xyz, modelOut.viewVert, normal);
#endif

#if SPOT_LIGHT
	float4 spotLightVert = float4(lightPos - modelOut.worldPos, 0);
	spotLightVert.w = length(spotLightVert.xyz);
	spotLightVert.xyz = normalize(spotLightVert.xyz);
    float spot = ComputeSpot(spotLightVert);
	diff *= spot;
	amb += ambLight * spot;
	spec *= spot;
#else
	amb += ambLight;
#endif

#ifdef SHADOW
    float shadow = tex2D(shadowMap, modelOut.screenPos).x;
	diff *= shadow;
	spec *= shadow;
#endif
	
	//texColor * texDiffK
	float3 resColor = texColor * texDiffK * colorMat * (amb + diff) + specMat * spec;	

#ifdef FOG_ENABLE	
	float fog = saturate((abs(modelOut.viewVert.w) - fogParams.x) / (fogParams.y - fogParams.x)) * fogParams.z;	
	resColor = resColor * (1.0f - fog) + fogColor * fog;	
#endif

	return resColor;
}