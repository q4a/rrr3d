//-----------------------------------------------------------------------------
// File: SkyBox.fx
//
// Desc: 
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
float4x4 matInvWVP;

texture envTex;

sampler envSampler = sampler_state
{ 
    Texture = (envTex);
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};


//-----------------------------------------------------------------------------
// Skybox stuff
//-----------------------------------------------------------------------------
struct SkyboxVS_Input
{
    float4 Pos : POSITION;
};

struct SkyboxVS_Output
{
    float4 Pos : POSITION;
    float3 Tex : TEXCOORD0;
};

SkyboxVS_Output SkyboxVS( SkyboxVS_Input Input )
{
    SkyboxVS_Output Output;
    
    Output.Pos = Input.Pos;  
    //Output.Tex = Output.Pos;

    //float4 pos = Input.Pos;
    //pos.z = -pos.z;
    Output.Tex = normalize(mul(Input.Pos, matInvWVP));
    
    return Output;
}

float4 SkyboxPS( SkyboxVS_Output Input ) : COLOR
{
    //float tmp = Input.Tex.x;
    //Input.Tex.x = Input.Tex.y;
    //Input.Tex.y = tmp;
    //Input.Tex.x = Input.Tex.y;

    //return float4(Input.Tex.zzz, 1.0f);

    //Input.Tex.z = -Input.Tex.z;

    float4 color = texCUBE( envSampler, Input.Tex );
    return color;
}

technique techSkybox
{
    pass p0
    {
        //SkyBox рисуется в постпроективном пространстве, поэтому порядок обхода вершин всегда постоянный(т.е. он не меняется при отражении камеры) 
        CullMode = NONE;
        //Запись в буффер глубины ни к чему
        ZWriteEnable = false;
        VertexShader = compile vs_3_0 SkyboxVS();
        PixelShader = compile ps_3_0 SkyboxPS();
    }
}




