#include "stdafx.h"

#include "graph\\SkyBox.h"

namespace r3d
{

namespace graph
{

//Схема
//+y; -y; +z; -z; +x; -x
const D3DXMATRIX skyFromLeftToRightCS(1.0f,  0.0f,  0.0f,  0.0f,
									  0.0f,  0.0f,  1.0f,  0.0f,
									  0.0f,  -1.0f, 0.0f,  0.0f,
									  0.0f,  0.0f,  0.0f,  1.0f);




SkyBox::SkyBox(): _coordSystem(csLeft)
{
	_mesh.GetOrCreateData()->SetVertexCount(6);	
	_mesh.GetData()->SetFormat(res::VertexData::vtPos3);
	_mesh.GetData()->Init();
	D3DXVECTOR3* vb = reinterpret_cast<D3DXVECTOR3*>(_mesh.GetData()->GetData());	
	vb[0] = D3DXVECTOR3(-1.0f, -1.0f, 1.0f);
	vb[1] = D3DXVECTOR3(1.0f, -1.0f, 1.0f);
	vb[2] = D3DXVECTOR3(1.0f, 1.0f, 1.0f);
	vb[3] = D3DXVECTOR3(-1.0f, -1.0f, 1.0f);
	vb[4] = D3DXVECTOR3(1.0f, 1.0f, 1.0f);
	vb[5] = D3DXVECTOR3(-1.0f, 1.0f, 1.0f);
	_mesh.GetData()->Update();

	shader.SetTech("techSkybox");
}

void SkyBox::WorldDimensions(AABB& aabb) const
{
	aabb.max.x = aabb.max.y = aabb.max.z = FLT_MAX;
	aabb.min = -aabb.max;
}

void SkyBox::Render(Engine& engine)
{
	_mesh.Init(engine);

	if (_coordSystem == csLeft)	
		engine.GetContext().SetWorldMat(skyFromLeftToRightCS);
	else
		engine.GetContext().SetWorldMat(IdentityMatrix);

	//Нужно обнулять 4-ую строку чтобы небо не растягивалось
	D3DXMATRIX skyWVP = engine.GetContext().GetCamera().GetWVP();
	skyWVP._41 = 0.0f;
	skyWVP._42 = 0.0f;
	skyWVP._43 = 0.0f;
	D3DXMatrixInverse(&skyWVP, 0, &skyWVP);
	shader.SetValueDir("matInvWVP", skyWVP);

	shader.Apply(engine);
	do
	{	
		engine.BeginDraw();
		_mesh.Draw();		
	}
	while (!engine.EndDraw(true));
	shader.UnApply(engine);
}

void SkyBox::ProgressTime(const float newTime, const float dt)
{
	//nothing
}

TexCubeResource* SkyBox::GetSkyTex()
{
	return static_cast<TexCubeResource*>(shader.GetTexture("envTex"));
}

void SkyBox::SetSkyTex(TexCubeResource* value)
{
	shader.SetTexture("envTex", value);
}

SkyBox::CoordSystem SkyBox::GetCoordSystem() const
{
	return _coordSystem;
}

void SkyBox::SetCoordSystem(CoordSystem value)
{
	_coordSystem = value;
}

}

}