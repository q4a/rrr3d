#include "stdafx.h"

#include "graph\\GrassField.h"

namespace r3d
{

namespace graph
{

GrassField::GrassField(): _width(200), _height(200), _density(1), _disp(2)
{
}

GrassField::~GrassField()
{
	SetGrassList(GrassList());
}

GrassField::GrassTile::GrassTile(float mWeight, D3DXVECTOR4 mTexCoord): weight(mWeight), texCoord(mTexCoord)
{
}

GrassField::GrassDesc::GrassDesc(graph::LibMaterial* mLibMat, const GrassTile& tile): libMat(mLibMat)
{
	tiles.push_back(tile);
}

void GrassField::BuildField()
{
	_batchList.clear();
	_mesh.Free();

	//Вычисляем карту позиций
	int numX = static_cast<int>(_fieldWidth * _density);
	int numY = static_cast<int>(_fieldHeight * _density);	
	//Общее число спрайтов
	int maxSprites = numX * numY;	
	//Шаг размещения
	D3DXVECTOR3 step(_fieldWidth/numX, _fieldHeight/numY, 0.0f);
	//Заполняем
	std::vector<D3DXVECTOR3> mapPos(maxSprites);
	int ind = 0;
	for (int i = -Floor<int>(numX/2.0f); i < Ceil<int>(numX/2.0f) - 1; ++i)
		for (int j = -Floor<int>(numY/2.0f); j < Ceil<int>(numY/2.0f) - 1; ++j, ++ind)
			mapPos[ind] = D3DXVECTOR3(step.x/2 + i * step.x, step.y/2 + j * step.y, step.z);
	std::random_shuffle(mapPos.begin(), mapPos.end());

	//Суммарный вес
	float summWeight = 0;
	for (unsigned i = 0; i < _grassList.size(); ++i)	
		for (unsigned j = 0; j < _grassList[i].tiles.size(); ++j)		
			summWeight += _grassList[i].tiles[j].weight;
	float spritesPerWeight = maxSprites / summWeight;

	//Инициализируем буффер
	res::VertexData& data = *_mesh.GetOrCreateData();
	data.SetVertexCount(maxSprites * 6);
	data.SetFormat(res::VertexData::vtPos4);
	data.SetFormat(res::VertexData::vtTex0);
	data.Init();
	//Заполняем
	_batchList.resize(_grassList.size());
	int spriteOff = 0;
	for (unsigned i = 0; i < _grassList.size(); ++i)
	{
		_batchList[i].vertOffs = spriteOff * 6;
		_batchList[i].primCnt = 0;

		for (unsigned j = 0; j < _grassList[i].tiles.size(); ++j)
		{
			int numSprites = static_cast<int>(Round(_grassList[i].tiles[j].weight * spritesPerWeight));
			numSprites = std::min(maxSprites - spriteOff, numSprites);
			D3DXVECTOR4 tex = _grassList[i].tiles[j].texCoord;

			for (int k = 0; k < numSprites; ++k)
			{
				int ind = (spriteOff + k);
				int vert = ind * 6;

				D3DXVECTOR3 pos(mapPos[ind]);
				pos.x += _disp * Random();
				pos.y += _disp * Random();

				*data[vert + 0].Pos4() = D3DXVECTOR4(pos, 0);
				*data[vert + 0].Tex0() = D3DXVECTOR2(tex[0], tex[1]);

				*data[vert + 1].Pos4() = D3DXVECTOR4(pos, 1);
				*data[vert + 1].Tex0() = D3DXVECTOR2(tex[2], tex[1]);

				*data[vert + 2].Pos4() = D3DXVECTOR4(pos, 2);
				*data[vert + 2].Tex0() = D3DXVECTOR2(tex[2], tex[3]);

				//
				*data[vert + 3].Pos4() = D3DXVECTOR4(pos, 0);
				*data[vert + 3].Tex0() = D3DXVECTOR2(tex[0], tex[1]);

				*data[vert + 4].Pos4() = D3DXVECTOR4(pos, 2);
				*data[vert + 4].Tex0() = D3DXVECTOR2(tex[2], tex[3]);

				*data[vert + 5].Pos4() = D3DXVECTOR4(pos, 3);
				*data[vert + 5].Tex0() = D3DXVECTOR2(tex[0], tex[3]);
			}

			spriteOff += numSprites;
			//Т.к. плоскость образуют два треугольника...
			_batchList[i].primCnt += numSprites * 2;
		}		
	}
	data.Update();	
}

void GrassField::DrawField(graph::Engine& engine, const Field& field)
{
	AABB aabb(D3DXVECTOR3(_fieldWidth, _fieldHeight, 2.0f));
	aabb.Offset(field.pos);
	aabb.Transform(GetWorldMat());
	if (engine.GetContext().GetCamera().GetFrustum().ContainsAABB(aabb) == Frustum::scNoOverlap)
		return;

	_mesh.Init(engine);

	shader.SetValueDir("offset", field.pos);

	for (unsigned i = 0; i < _grassList.size(); ++i)
	{
		_grassList[i].libMat->Apply(engine);
		//register s0
		//shader.SetTextureDir("colorTex", engine.GetContext().GetTexture(0));

		engine.BeginDraw();
		_mesh.Draw(_batchList[i].vertOffs, _batchList[i].primCnt);
		engine.EndDraw(false);

		_grassList[i].libMat->UnApply(engine);
	}
}

void GrassField::Rebuild()
{	
	if (_width <=0 || _height <= 0 || _grassList.empty())
	{
		_fieldList.clear();
		return;
	}

	float square = _width * _height;
	float aspect = _width / _height;

	int grassSz = sizeof(D3DXVECTOR4) * sizeof(D3DXVECTOR2);
	float grassCnt = square * _density;
	float fieldSz = cMaxBufSize / static_cast<float>(grassSz);
	float fieldCnt = grassCnt / fieldSz;
	int fieldNumY = static_cast<int>(Ceil<float>(sqrt(fieldCnt / aspect)));
	int fieldNumX = static_cast<int>(Ceil<float>(fieldNumY * aspect));
	int fieldNum = fieldNumX * fieldNumY;
	_fieldWidth = _width / fieldNumX;
	_fieldHeight = _height / fieldNumY;

	_fieldList.resize(fieldNum);
	for (int i = 0; i < fieldNumX; ++i)
		for (int j = 0; j < fieldNumY; ++j)
		{
			int ind = i * fieldNumY + j;
			_fieldList[ind].pos = D3DXVECTOR3((i + 0.5f) * _fieldWidth - _width / 2.0f, (j + 0.5f) * _fieldHeight - _height / 2.0f, 0.0f);			
		}
	
	BuildField();
}

AABB GrassField::LocalDimensions() const
{
	return AABB(D3DXVECTOR3(GetWidth(), GetHeight(), 2.0f));
}
	
void GrassField::DoRender(graph::Engine& engine)
{
	shader.Init(engine);
	
	shader.SetValueDir("wvpMatrix", engine.GetContext().GetCamera().GetWVP());
	shader.SetValueDir("viewMatInv", engine.GetContext().GetCamera().GetInvView());
	shader.SetValueDir("matWorldView", engine.GetContext().GetCamera().GetTransform(CameraCI::ctWorldView));

	engine.GetContext().SetRenderState(graph::rsAlphaRef, 17);
	engine.GetContext().SetRenderState(graph::rsAlphaFunc, D3DCMP_GREATEREQUAL);
	engine.GetContext().SetRenderState(graph::rsAlphaTestEnable, true);	

	D3DXVECTOR3 fogParamsVec = D3DXVECTOR3(0, 1, (float)engine.GetContext().GetRenderState(rsFogEnable));
	if (fogParamsVec.z != 0)
	{
		DWORD dwVal = engine.GetContext().GetRenderState(rsFogStart);
		fogParamsVec.x = *(float*)(&dwVal);

		dwVal = engine.GetContext().GetRenderState(rsFogEnd);
		fogParamsVec.y = *(float*)(&dwVal);

		D3DXCOLOR fogColorVec = D3DXCOLOR(engine.GetContext().GetRenderState(rsFogColor));
		shader.SetValueDir("fogColor", fogColorVec);
	}
	shader.SetValueDir("fogParams", fogParamsVec);

	bool isNight = engine.GetContext().IsNight();
	shader.SetValueDir("diffLight", isNight ? D3DXCOLOR(engine.GetContext().GetRenderState(rsAmbient)) : clrWhite);

	shader.Apply(engine);
	for (unsigned i = 0; i < _fieldList.size(); ++i)
		DrawField(engine, _fieldList[i]);			
	shader.UnApply(engine);

	engine.GetContext().RestoreRenderState(graph::rsAlphaRef);
	engine.GetContext().RestoreRenderState(graph::rsAlphaFunc);
	engine.GetContext().RestoreRenderState(graph::rsAlphaTestEnable);
}

float GrassField::GetWidth() const
{
	return _width;
}

void GrassField::SetWidth(float value)
{
	if (_width != value)
	{
		_width = value;
		Rebuild();
	}
}

float GrassField::GetHeight() const
{
	return _height;
}

void GrassField::SetHeight(float value)
{
	if (_height != value)
	{
		_height = value;
		Rebuild();
	}
}

float GrassField::GetDensity()
{
	return _density;
}

void GrassField::SetDensity(float value)
{
	if (_density != value)
	{
		_density = value;
		Rebuild();
	}
}

float GrassField::GetDisp()
{
	return _disp;
}

void GrassField::SetDisp(float value)
{
	if (_disp != value)
	{
		_disp = value;
		Rebuild();
	}
}

const GrassField::GrassList& GrassField::GetGrassList()
{
	return _grassList;
}

void GrassField::SetGrassList(const GrassList& value)
{
	for (GrassList::iterator iter = _grassList.begin(); iter != _grassList.end(); ++iter)	
		iter->libMat->Release();
	_grassList = value;
	for (GrassList::iterator iter = _grassList.begin(); iter != _grassList.end(); ++iter)	
		iter->libMat->AddRef();

	Rebuild();
}

}

}