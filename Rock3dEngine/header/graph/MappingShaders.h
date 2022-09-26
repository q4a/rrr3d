#ifndef MAPPING_SHADERS
#define MAPPING_SHADERS

#include "MaterialLibrary.h"

namespace r3d
{

namespace graph
{

class LightShader: public Shader
{
protected:
	enum LightType {ltPoint = 0, ltSpot, ltDir, cLightTypeEnd};
	enum LightProp {lpNoShadow = 0, lpShadow, cLightPropEnd};
	enum MyParam {numLights, glAmbient, ambLight, diffLight, specLight, colorMat, specMat, specPower, texDiffK, worldMat, worldViewMat, wvpMat, viewPos, diffTex, shadowTex, lightPos, lightDir, spotParams, fogParams, fogColor, reflectivity, envTex, alphaBlendColor, normTex, mWorldViewProj, refrTex, vScene, reflTex, cMyParamEnd};

	static const lsl::string cLightTypeStr[cLightTypeEnd];
	static const lsl::string cLightPropStr[cLightPropEnd];
	static const lsl::string cMyParamStr[cMyParamEnd];
private:
	D3DXVECTOR3 _viewPos;
	float _texDiffK;

	MacroBlock* _lightMacro[cLightTypeEnd][cLightPropEnd];	
	D3DLIGHTTYPE _paramsLight;
	bool _paramsShadow;

	unsigned _curLight;	
	D3DXCOLOR _fogColor;	

	void InvalidateParams(D3DLIGHTTYPE value, bool shadow);
protected:
	D3DXHANDLE _params[cMyParamEnd];

	virtual void DoFree();
	virtual void DoBeginDraw(Engine& engine);
	virtual bool DoEndDraw(Engine& engine, bool nextPass);
public:
	LightShader();
	virtual ~LightShader();

	const D3DXVECTOR3& GetViewPos() const;
	void SetViewPos(const D3DXVECTOR3& value);

	float GetTexDiffK() const;
	void SetTexDiffK(float value);
};

class ReflMappShader: public LightShader
{
private:
	typedef LightShader _MyBase;

	float _reflectivity;
protected:
	virtual void DoBeginDraw(Engine& engine);
public:
	ReflMappShader();

	graph::TexCubeResource* GetReflTex();
	void SetReflTex(graph::TexCubeResource* value);

	float GetReflectivity() const;
	void SetReflectivity(float value);
};

class ReflBumbMappShader: public LightShader
{
private:
	typedef LightShader _MyBase;
protected:
	virtual void DoBeginDraw(Engine& engine);
public:
	ReflBumbMappShader();

	graph::TexCubeResource* GetReflTex();
	void SetReflTex(graph::TexCubeResource* value);
};

class BumpMapShader: public LightShader
{
private:
	typedef LightShader _MyBase;
protected:
	virtual void DoBeginDraw(Engine& engine);
public:
	BumpMapShader();
};

class RefrShader: public Shader
{
protected:
	virtual void DoBeginDraw(Engine& engine); 
public:
	RefrShader();
};

class PlanarReflMappShader: public LightShader
{
private:
	typedef LightShader _MyBase;
protected:
	virtual void DoBeginDraw(Engine& engine);
public:
	PlanarReflMappShader();

	graph::Tex2DResource* GetReflTex();
	void SetReflTex(graph::Tex2DResource* value);
};

}

}

#endif