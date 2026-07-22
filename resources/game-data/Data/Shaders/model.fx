//#define DIFF_TEX 1
//#define NORMAL 1
//#define VIEW_VERT 1
//#define VIEW_VERT_FOG 1
//#define LIGHT_VERT 1
//#define LIGHT_VERT_DIR 1
//#define REFL_VECTOR 1
//#define SCREEN_POS 1
//#define WORLD_POS 1
//#define TANGENT_SPACE 1

float4x4 worldMat;
float4x4 worldViewMat;
float4x4 wvpMat;

float3 viewPos;
float3 lightDir;
#if LIGHT_VERT
float3 lightPos;
#endif

struct ModelVSIn
{
    float4 pos: POSITION;

#if DIFF_TEX
    float2 diffTex: TEXCOORD0;
#endif

#if NORMAL || REFL_VECTOR || TANGENT_SPACE
    float3 normal: NORMAL0;
#endif

#if TANGENT_SPACE
    float3 tangent: TEXCOORD1;
#endif

#if TANGENT_SPACE
    float3 binormal: TEXCOORD2;
#endif
};

struct ModelVSOut
{
    float4 pos: POSITION;

#if DIFF_TEX
    float2 diffTex: TEXCOORD0;
#endif

#if NORMAL
    float3 normal: TEXCOORD1;
#endif

#if VIEW_VERT
    float4 viewVert: TEXCOORD2;
#endif

#if LIGHT_VERT	
    float3 lightVert: TEXCOORD3;
#endif

#if REFL_VECTOR
    float3 reflVec: TEXCOORD4;
#endif

#if SCREEN_POS
    float4 screenPos: TEXCOORD5;
#endif

#if WORLD_POS
    float3 worldPos: TEXCOORD6;
#endif

#if LIGHT_VERT_DIR
    float3 lightVertDir: TEXCOORD7;
#endif
};

struct ModelPSIn
{
#if DIFF_TEX
    float2 diffTex: TEXCOORD0;
#endif

#if NORMAL
    float3 normal: TEXCOORD1;
#endif

#if VIEW_VERT
    float4 viewVert: TEXCOORD2;
#endif

#if LIGHT_VERT
    float3 lightVert: TEXCOORD3;
#endif

#if REFL_VECTOR
    float3 reflVec: TEXCOORD4;
#endif

#if SCREEN_POS
    float4 screenPos: TEXCOORD5;
#endif

#if WORLD_POS
    float3 worldPos: TEXCOORD6;
#endif

#if LIGHT_VERT_DIR
    float3 lightVertDir: TEXCOORD7;
#endif
};

struct ModelPSOut
{
#if NORMAL
    float3 normal;
#endif

#if VIEW_VERT
    float4 viewVert;
#endif

#if LIGHT_VERT
	//xyz - vec
	//w - length
    float4 lightVert;
#endif

#if SCREEN_POS
    float2 screenPos;
#endif

#if WORLD_POS
    float3 worldPos;
#endif

#if LIGHT_VERT_DIR
	float3 lightVertDir;
#endif
};




void CompModelVSOut(ModelVSIn modelIn, out ModelVSOut modelOut)
{
    float3 worldPos = mul(modelIn.pos, worldMat).xyz;	

#if NORMAL || REFL_VECTOR || TANGENT_SPACE
    float3 normal = mul(modelIn.normal, (float3x3)worldMat);
#endif

#if TANGENT_SPACE
    float3x3 tangSpaceMat; 
    tangSpaceMat[0] = mul(modelIn.tangent, (float3x3)worldMat); // x
    tangSpaceMat[1] = mul(modelIn.binormal, (float3x3)worldMat);// y
    tangSpaceMat[2] = normal; //z
#endif

#if VIEW_VERT || REFL_VECTOR
	float3 viewVert = viewPos - worldPos;
	#if TANGENT_SPACE
		viewVert = mul(tangSpaceMat, viewVert);
	#endif	
#endif

	modelOut.pos = mul(modelIn.pos, wvpMat);

#if VIEW_VERT
	modelOut.viewVert = float4(viewVert, 0.0f);
	#if VIEW_VERT_FOG
	modelOut.viewVert.w = mul(modelIn.pos, worldViewMat).z;
	#endif
#endif

#if LIGHT_VERT
	modelOut.lightVert = lightPos - worldPos;
	#if TANGENT_SPACE
		modelOut.lightVert = mul(tangSpaceMat, modelOut.lightVert);
	#endif
#endif

#if LIGHT_VERT_DIR
	modelOut.lightVertDir = -lightDir;
	#if TANGENT_SPACE
		modelOut.lightVertDir = mul(tangSpaceMat, modelOut.lightVertDir);
	#endif
#endif    

#if DIFF_TEX
    modelOut.diffTex = modelIn.diffTex;
#endif

#if NORMAL
    modelOut.normal = normal;
#endif

#if REFL_VECTOR
    modelOut.reflVec = reflect(normalize(viewVert), normal);
#endif

#if SCREEN_POS
    modelOut.screenPos = modelOut.pos;
#endif

#if WORLD_POS
    modelOut.worldPos = worldPos;
#endif
}

void CompModelPSOut(ModelPSIn modelIn, out ModelPSOut modelOut)
{
#if NORMAL
    modelOut.normal = normalize(modelIn.normal);
#endif

#if VIEW_VERT
    modelOut.viewVert.xyz = normalize(modelIn.viewVert.xyz);
	modelOut.viewVert.w = modelIn.viewVert.w;
#endif

#if LIGHT_VERT
    modelOut.lightVert.xyz = normalize(modelIn.lightVert);
	modelOut.lightVert.w = length(modelIn.lightVert);
#endif

#if LIGHT_VERT_DIR
    modelOut.lightVertDir = normalize(modelIn.lightVertDir);
#endif

#if SCREEN_POS
    modelOut.screenPos = 0.5f * modelIn.screenPos.xy / modelIn.screenPos.w + 0.5f;
    modelOut.screenPos.y = 1.0f - modelOut.screenPos.y;
#endif

#if WORLD_POS
    modelOut.worldPos = modelIn.worldPos;
#endif
}

void ModelVS(ModelVSIn modelIn, out ModelVSOut modelOut)
{
    CompModelVSOut(modelIn, modelOut);
}