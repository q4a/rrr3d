#ifndef DEPTH_MAP
#define DEPTH_MAP

#include "MaterialLibrary.h"
#include "RenderToTexture.h"

namespace r3d
{

namespace graph
{

class DepthMapShader: public Shader
{
private:
	typedef Shader _MyBase;
public:
	enum TechniqueType {ttDepthMap, ttDepthMapAlphaTest, cTechniqueTypeEnd};

	static const char* cTechniqueNames[cTechniqueTypeEnd];
private:
	TechniqueType _technique;
protected:
	virtual void DoBeginDraw(Engine& engine);
	virtual bool DoEndDraw(Engine& engine, bool nextPass);
public:
	TechniqueType GetTech() const;
	void SetTech(TechniqueType value);

	D3DXMATRIX viewProjMat;
};

class DepthMapRender: public GraphObjRender<Tex2DResource>
{
private:
	typedef GraphObjRender<Tex2DResource> _MyBase;
protected:
	virtual Tex2DResource* CreateRT();
public:
	virtual void BeginRT(Engine& engine, const RtFlags& flags);
	virtual void EndRT(Engine& engine);

	const D3DXMATRIX& GetViewProjMat() const;
	void SetViewProjMat(const D3DXMATRIX& value);

	DepthMapShader shader;
};

}

}

#endif