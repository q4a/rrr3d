#include "stdafx.h"

#include "graph\\ContextInfo.h"

namespace r3d
{

namespace graph
{

const float cFNull = 0.0f;
const DWORD cFNullToDw = *((const DWORD*)(&cFNull));
const float cFOne = 1.0f;
const DWORD cFOneToDw = *((const DWORD*)(&cFOne));

const float cF64 = 64.0f;
const DWORD cF64ToTw = *((const DWORD*)(&cF64));

const TransformStateType ContextInfo::cTexTransform[8] =
{
	tstTexture0,
	tstTexture1,
	tstTexture2,
	tstTexture3,
	tstTexture4,
	tstTexture5,
	tstTexture6,
	tstTexture7
};

DWORD ContextInfo::defaultRenderStates[RENDER_STATE_END] = {D3DZB_TRUE, D3DFILL_SOLID, D3DSHADE_GOURAUD, TRUE, FALSE, FALSE, D3DBLEND_ONE, D3DBLEND_ZERO, D3DCULL_CW, D3DCMP_LESSEQUAL, 0, D3DCMP_ALWAYS, FALSE, FALSE, FALSE, FALSE, 0, D3DFOG_NONE, cFNullToDw, cFOneToDw, cFOneToDw, FALSE, FALSE, D3DSTENCILOP_KEEP, D3DSTENCILOP_KEEP, D3DSTENCILOP_KEEP, D3DCMP_ALWAYS, 0, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0, 0, 0, TRUE, TRUE, 0, D3DFOG_NONE, TRUE, TRUE, TRUE, TRUE, D3DMCS_COLOR2, D3DMCS_MATERIAL, D3DMCS_MATERIAL, D3DVBF_DISABLE, 0, 0, cFOneToDw, FALSE, FALSE, cFOneToDw, cFNullToDw, cFNullToDw, TRUE, 0xFFFFFFFF, D3DPATCHEDGE_DISCRETE, D3DDMT_ENABLE, cF64ToTw, FALSE, 0x0000000F, 0, D3DBLENDOP_ADD, D3DDEGREE_CUBIC, D3DDEGREE_LINEAR, FALSE, 0, FALSE, cFOneToDw, cFOneToDw, cFNullToDw, cFNullToDw, cFOneToDw, cFNullToDw, FALSE, FALSE, D3DSTENCILOP_KEEP, D3DSTENCILOP_KEEP, D3DSTENCILOP_KEEP, D3DCMP_ALWAYS, 0x0000000F, 0x0000000F, 0x0000000F, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, FALSE, D3DBLEND_ONE, D3DBLEND_ZERO, TRUE};

DWORD ContextInfo::defaultSamplerStates[SAMPLER_STATE_END] = {D3DTADDRESS_WRAP, D3DTADDRESS_WRAP, D3DTADDRESS_WRAP, 0, D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_NONE, 0, 0, 1, 0, 0, 0};

DWORD ContextInfo::defaultTextureStageStates[TEXTURE_STAGE_STATE_END] = {D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_CURRENT, D3DTOP_SELECTARG1, D3DTA_TEXTURE, D3DTA_CURRENT, 0, 0, 0, 0, 0, 0, 0, D3DTTFF_DISABLE,  D3DTA_CURRENT, D3DTA_CURRENT, D3DTA_CURRENT, clrWhite};

DWORD ContextInfo::GetDefTextureStageState(int stage, TextureStageState state)
{
	if (stage > 0)
		switch (state)
		{
		case tssColorOp:
			return D3DTOP_DISABLE;
		case tssAlphaOp:
			return D3DTOP_DISABLE;
		case tssTexCoordIndex:
			return stage;
		}

	return ContextInfo::defaultTextureStageStates[state];
}




CameraDesc::CameraDesc()
{
	aspect = 1.0f;
	fov = D3DX_PI/2.0f;
	nearDist = 1.0f;
	farDist = 100.0f;
	style = csPerspective;
	width = 35.0f;

	pos = NullVector;
	dir = XVector;
	up = ZVector;
}




CameraCI::CameraCI(): _idState(0), _frustChanged(true)
{
	_matChanged.set();
	_invMatChanged.set();
}

D3DXVECTOR2 CameraCI::ViewToProj(const D3DXVECTOR2& coord, const D3DXVECTOR2& viewSize)
{
	D3DXVECTOR2 projVec(coord.x / viewSize.x, coord.y / viewSize.y);
	//ѕриводим к диапазону [-1, 1]
	projVec = projVec * 2.0f - IdentityVec2;
	//ќсь Y у экрана и у заднего буфера(или иначе говор€ экранной D3D поверхности) не совпадают
	projVec.y = -projVec.y;

	return projVec;

	/*//јлгоритм с импользованием D3DXVec3Unproject
	D3DVIEWPORT9 viewPort;
	_engine->GetDriver().GetDevice()->GetViewport(&viewPort);
	float width = static_cast<float>(GetWndWidth());
	float height = static_cast<float>(GetWndHeight());

	D3DXVECTOR3 screenVec(coord.x / width * viewPort.Width, coord.y / height * viewPort.Height, z);
	
	D3DXVec3Unproject(&screenVec, &screenVec, &viewPort, &_curCamera->GetContextInfo().GetProjMat(),  &_curCamera->GetContextInfo().GetViewMat(), &IdentityMatrix);

	return screenVec;*/
}

D3DXVECTOR2 CameraCI::ProjToView(const D3DXVECTOR2& coord, const D3DXVECTOR2& viewSize)
{
	D3DXVECTOR2 projVec = coord;
	projVec.y = -projVec.y;
	projVec = projVec * 0.5f + IdentityVec2 * 0.5f;

	projVec.x = projVec.x * viewSize.x;
	projVec.y = projVec.y * viewSize.y;

	return projVec;
}

void CameraCI::StateChanged()
{
	++_idState;

	_frustChanged = true;
}

void CameraCI::WorldMatChanged(const D3DXMATRIX& worldMat)
{
	_worldMat = worldMat;

	_matChanged.set(ctWorldView);
	_matChanged.set(ctWVP);
	_invMatChanged.set(ctWorldView);
	_invMatChanged.set(ctWVP);
}

void CameraCI::CalcProjPerspective(D3DXMATRIX& mat) const
{
	D3DXMatrixPerspectiveFovRH(&mat, _desc.fov, _desc.aspect, _desc.nearDist, _desc.farDist);
}

void CameraCI::ProjMatChanged()
{
	_matChanged.set(ctViewProj);
	_matChanged.set(ctWVP);
	_invMatChanged.set(ctProj);
	_invMatChanged.set(ctViewProj);
	_invMatChanged.set(ctWVP);

	StateChanged();
}

void CameraCI::DescChanged()
{
	_matChanged.set();
	_invMatChanged.set();

	StateChanged();
}

unsigned CameraCI::IdState() const
{
	return _idState;
}

enum Intersect {fiOutside, fiIntersect, fiInside};

Intersect FrustumAABBIntersect(const Frustum& frustum, bool incZ, const AABB& aabb, D3DXVECTOR3& vMin, D3DXVECTOR3& vMax)
{
	Intersect ret = fiInside;
	int ePlane = incZ ? 6 : 4;
	
	for (int i = 0; i < ePlane; ++i)
	{
		D3DXVECTOR3 normal(frustum.planes[i].a, frustum.planes[i].b, frustum.planes[i].c);

		// X axis
		if (normal.x > 0)
		{
			vMin.x = aabb.min.x;
			vMax.x = aabb.max.x;
		} 
		else
		{
			vMin.x = aabb.max.x;
			vMax.x = aabb.min.x;
		}

		// Y axis
		if (normal.y > 0)
		{
			vMin.y = aabb.min.y;
			vMax.y = aabb.max.y;
		}
		else
		{
			vMin.y = aabb.max.y;
			vMax.y = aabb.min.y;
		}

		// Z axis
		if (normal.z > 0)
		{
			vMin.z = aabb.min.z;
			vMax.z = aabb.max.z;
		}
		else
		{
			vMin.z = aabb.max.z;
			vMax.z = aabb.min.z;
		}

		if (D3DXPlaneDotCoord(&frustum.planes[i], &vMin) > 0)
			return fiOutside;
		if (D3DXPlaneDotCoord(&frustum.planes[i], &vMax) >= 0)
			ret = fiIntersect;
	}

	return ret;
}

bool LineCastIntersPlane(const D3DXVECTOR3& rayStart, const D3DXVECTOR3& rayVec, const D3DXPLANE& plane, float& outT)
{
	const float EPSILON = 1.0e-10f;

	float d = D3DXPlaneDotNormal(&plane, &rayVec);	
	if (abs(d) > EPSILON)
	{
		outT = -D3DXPlaneDotCoord(&plane, &rayStart) / d;
		return true;
	}
	return false;
}

unsigned PlaneBBIntersect(const BoundBox& bb, const D3DXPLANE& plane, D3DXVECTOR3 points[])
{
	//конечные вершины ребер дл€ каждого вертекса
	const int lines[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

	unsigned res = 0;

	for (int i = 0; i < 12; ++i)
	{
		D3DXVECTOR3 v1 = bb.v[lines[i][0]];
		D3DXVECTOR3 v2 = bb.v[lines[i][1]];

		D3DXVECTOR3 vec = v2 - v1;
		float vec3Len = D3DXVec3Length(&vec);
		D3DXVec3Normalize(&vec, &vec);
		float dist;
		//есть пересечение
		if (LineCastIntersPlane(v1, vec, plane, dist) && dist > 0.0f && dist < vec3Len)
		{
			points[res] = v1 + vec * dist;
			//больше 4-х точек не может быть
			if (++res > 3)
				return res;
		}
	}

	return res;
}

unsigned PlaneAABBIntersect(const AABB& aabb, const D3DXPLANE& plane, D3DXVECTOR3 points[])
{
	BoundBox bb(aabb);

	return PlaneBBIntersect(bb, plane, points);
}

bool CameraCI::ComputeZBounds(const AABB& aabb, float& minZ, float& maxZ) const
{
	bool res = false;
	D3DXPLANE posNearPlane;
	D3DXPlaneFromPointNormal(&posNearPlane, &_desc.pos,  &_desc.dir);

	BoundBox box(aabb);
	BoundBox viewBox, projBox;
	BoundBox::Transform(box, GetView(), viewBox);
	BoundBox::Transform(box, GetViewProj(), projBox);

	//поиск по вершинам aabb
	for (int i = 0; i < 8; ++i)	
		//лежит ли точка в боксе
		if (abs(projBox.v[i].x) < 1.0f && abs(projBox.v[i].y) < 1.0f)
		{
			float z = -viewBox.v[i].z;

			if (z > maxZ || !res)
				maxZ = z;
			if (z < minZ || !res)			
				minZ = z;

			res = true;
		}

	//поиск через лучи из направл€ющих ребер фрустума
	D3DXVECTOR3 rayVec[4] = {D3DXVECTOR3(-1.0f, -1.0f, 1.0f), D3DXVECTOR3(1.0f, -1.0f, 1.0f), D3DXVECTOR3(-1.0f, 1.0f, 1.0f), D3DXVECTOR3(1.0f, 1.0f, 1.0f)};
	D3DXVECTOR3 rayPos[4] = {D3DXVECTOR3(-1.0f, -1.0f, 0.0f), D3DXVECTOR3(1.0f, -1.0f, 0.0f), D3DXVECTOR3(-1.0f, 1.0f, 0.0f), D3DXVECTOR3(1.0f, 1.0f, 0.0f)};
	
	for (int i = 0; i < 4; ++i)
	{
		D3DXVec3TransformCoord(&rayVec[i], &rayVec[i], &GetInvViewProj());
		D3DXVec3TransformCoord(&rayPos[i], &rayPos[i], &GetInvViewProj());
		D3DXVec3Normalize(&rayVec[i], &(rayVec[i] - rayPos[i]));

		float tNear, tFar;
		if (aabb.LineCastIntersect(rayPos[i], rayVec[i], tNear, tFar))
		{
			tNear = D3DXPlaneDotCoord(&posNearPlane, &(rayPos[i] + rayVec[i] * tNear));
			tFar = D3DXPlaneDotCoord(&posNearPlane, &(rayPos[i] + rayVec[i] * tFar));

			if (tNear < minZ || !res)
				minZ = tNear;			
			if (tFar > maxZ || !res)			
				maxZ = tFar;

			res = true;
		}
	}

	//поиск пересечений aabb с фрустумом
	/*D3DXPLANE nearPlane;
	D3DXPlaneFromPointNormal(&nearPlane, &NullVector, &ZVector);
	D3DXVECTOR3 points[4];
	unsigned numPoints = PlaneBBIntersect(projBox, nearPlane, points);
	for (unsigned i = 0; i < numPoints; ++i)
	{
		if (abs(points[i].x) < 1.1f && abs(points[i].y) < 1.1f)
		{
			D3DXVECTOR3 vec;
			D3DXVec3TransformCoord(&vec, &points[i], &GetInvProj());

			if (vec.z < minZ || !res)
				minZ = vec.z;
			if (vec.z > maxZ || !res)
				maxZ = vec.z;
			res = true;
		}
	}

	return res;*/


	const Frustum& frustum = GetFrustum();
	for (int i = 0; i < 4; ++i)
	{
		D3DXVECTOR3 points[4];
		unsigned numPnt = PlaneAABBIntersect(aabb, frustum.planes[i], points);

		for (int j = 0; j < 4; ++j)
		{
			bool contain = true;
			float fContain = 0.0f;
			for (int k = 0; k < 3; ++k)
			{
				int numPlane = (i + k + 1) % 4;
				float planeDot = D3DXPlaneDotCoord(&frustum.planes[numPlane], &points[j]);
				if (k == 0)
					fContain = planeDot;
				//лежит вне
				if (k > 0 && planeDot * fContain < 0)
				{
					contain = false;
					break;
				}
			}

			if (contain)
			{
				float z = D3DXPlaneDotCoord(&posNearPlane, &points[j]);

				if (z < minZ || !res)
					minZ = z;
				if (z > maxZ || !res)
					maxZ = z;
				
				res = true;				
			}
		}
	}

	return res;
}

void CameraCI::AdjustNearFarPlane(const AABB& aabb, float minNear, float maxFar)
{
	float fMinZ = minNear;
	float fMaxZ = maxFar;
	ComputeZBounds(aabb, fMinZ, fMaxZ);

	_desc.nearDist = std::max(fMinZ, minNear);
	_desc.farDist = std::min(fMaxZ, maxFar);
	
	DescChanged();
}

void CameraCI::GetProjPerspective(D3DXMATRIX& mat) const
{
	if (_desc.style == csPerspective)
		mat = GetProj();
	else
		return CalcProjPerspective(mat);
}

void CameraCI::GetViewProjPerspective(D3DXMATRIX& mat) const
{
	if (_desc.style == csPerspective)
		mat = GetViewProj();
	else
	{
		CalcProjPerspective(mat);
		D3DXMatrixMultiply(&mat, &GetView(), &mat);
	}
}

void CameraCI::GetWVPPerspective(D3DXMATRIX& mat) const
{
	if (_desc.style == csPerspective)
		mat = GetViewProj();
	else
	{
		CalcProjPerspective(mat);
		D3DXMatrixMultiply(&mat, &GetTransform(ctWorldView), &mat);
	}
}

void CameraCI::SetProjMat(const D3DXMATRIX& value)
{
	_matrices[ctProj] = value;

	_desc.fov = 2 * atan(1.0f / value._22);
	_desc.aspect = value._22 / value._11;
	_desc.nearDist = value._43 / value._33;
	_desc.farDist = _desc.nearDist * value._33 / (1 + value._33);

	ProjMatChanged();
}

D3DXVECTOR3 CameraCI::ScreenToWorld(const D3DXVECTOR2& coord, float z, const D3DXVECTOR2& viewSize) const 
{
	D3DXVECTOR2 projCoord = ViewToProj(coord, viewSize);
	D3DXVECTOR3 screenVec(projCoord.x, projCoord.y, z);
	//ѕереводим в мировое пространство(домножа€ на инв. матрицу), что соотв. точке на near плоскости камеры
	D3DXVec3TransformCoord(&screenVec, &screenVec, &GetInvViewProj());

	return screenVec;
}

D3DXVECTOR2 CameraCI::WorldToScreen(const D3DXVECTOR3& coord, const D3DXVECTOR2& viewSize) const
{
	D3DXVECTOR3 screenVec;
	D3DXVec3TransformCoord(&screenVec, &coord, &GetViewProj());	
	D3DXVECTOR2 vec = screenVec;

	return ProjToView(vec, viewSize);
}

const CameraDesc& CameraCI::GetDesc() const
{
	return _desc;
}

void CameraCI::SetDesc(const CameraDesc& value)
{
	_desc = value;

	DescChanged();
}

const D3DXMATRIX& CameraCI::GetTransform(Transform transform) const
{
	if (_matChanged.test(transform))
	{
		switch (transform)
		{
		case ctView:
			//
			switch (_desc.style)
			{
			case csViewPort:
			case csViewPortInv:
				_matrices[transform] = IdentityMatrix;
				break;

			default:
				//»спользуетс€ правосторонн€ система координат (как в 3dMax-e)
				D3DXMatrixLookAtRH(&_matrices[transform], &_desc.pos, &(_desc.pos + _desc.dir), &_desc.up);
			}			
			break;

		case ctProj:
		{
			//»спользуетс€ правосторонн€ система координат (как в 3dMax-e)
			switch (_desc.style)
			{
			case csPerspective:
				CalcProjPerspective(_matrices[transform]);
				break;

			case csOrtho:
				D3DXMatrixOrthoRH(&_matrices[transform], _desc.width, _desc.width / _desc.aspect, _desc.nearDist, _desc.farDist);
				break;

			case csViewPort:
			case csViewPortInv:
			{
				D3DXVECTOR2 viewSize = D3DXVECTOR2(_desc.width, _desc.width / _desc.aspect);
				D3DXMATRIX viewMat;
				D3DXMatrixTranslation(&viewMat, -1.0f, _desc.style == csViewPortInv ? -1.0f : 1.0f, 0.0f);
				D3DXMATRIX matScale;
				D3DXMatrixScaling(&matScale, 2.0f/viewSize.x, _desc.style == csViewPortInv ? 2.0f/viewSize.y : -2.0f/viewSize.y, 1.0f);
				matScale._33 = 1.0f/(-500.0f - 500.0f);
				matScale._43 = -500.0f/(-500.0f - 500.0f);
				_matrices[transform] = matScale * viewMat;
				break;
			}

			default:
				LSL_ASSERT(false);
			}

			break;
		}

		case ctWorldView:
			D3DXMatrixMultiply(&_matrices[transform], &_worldMat, &GetTransform(ctView));
			break;

		case ctViewProj:
			D3DXMatrixMultiply(&_matrices[transform], &GetTransform(ctView), &GetTransform(ctProj));
			break;

		case ctWVP:
			D3DXMatrixMultiply(&_matrices[transform], &_worldMat, &GetTransform(ctViewProj));
			break;
		}

		_matChanged.reset(transform);
	}

	return _matrices[transform];
}

const D3DXMATRIX& CameraCI::GetInvTransform(Transform transform) const
{
	if (_invMatChanged.test(transform))
	{
		D3DXMatrixInverse(&_invMatrices[transform], 0, &GetTransform(transform));
		_invMatChanged.reset(transform);
	}

	return _invMatrices[transform];
}

const Frustum& CameraCI::GetFrustum() const
{
	if (_frustChanged)
	{
		_frustum.Refresh(GetTransform(ctViewProj));
		_frustChanged = false;
	}

	return _frustum;
}

const D3DXMATRIX& CameraCI::GetView() const
{
	return GetTransform(ctView);
}

const D3DXMATRIX& CameraCI::GetProj() const
{
	return GetTransform(ctProj);
}

const D3DXMATRIX& CameraCI::GetViewProj() const
{
	return GetTransform(ctViewProj);
}

const D3DXMATRIX& CameraCI::GetWVP() const
{
	return GetTransform(ctWVP);
}

const D3DXMATRIX& CameraCI::GetInvView() const
{
	return GetInvTransform(ctView);
}

const D3DXMATRIX& CameraCI::GetInvProj() const
{
	return GetInvTransform(ctProj);
}

const D3DXMATRIX& CameraCI::GetInvViewProj() const
{
	return GetInvTransform(ctViewProj);
}

const D3DXMATRIX& CameraCI::GetInvWVP() const
{
	return GetInvTransform(ctWVP);
}




LightCI::LightCI(): _changed(true), _owner(0)
{
}

bool LightCI::IsChanged() const
{
	return _changed;
}

void LightCI::AdjustNearFarPlane(const AABB& aabb, float minNear, float maxFar)
{
	/*_camera.AdjustNearFarPlane(aabb, minNear, maxFar);

	bool res = false;

	float minZ = 0;
	float maxZ = 0;
	BoundBox box(aabb);
	BoundBox viewBox, projBox;
	BoundBox::Transform(box, _camera.GetView(), viewBox);
	BoundBox::Transform(box, _camera.GetViewProj(), projBox);
	
	for (int i = 0; i < 8; ++i)	
		{
			float z = -viewBox.v[i].z;

			if (z > maxZ || !res)			
				maxZ = z;			
			if (z < minZ || !res)			
				minZ = z;

			res = true;
		}

	_desc.nearDist = std::max(minZ, minNear);
	_desc.range = std::min(maxZ, maxFar);
	_changed = true;*/

	_camera.AdjustNearFarPlane(aabb, minNear, maxFar);
	_desc.nearDist = _camera.GetDesc().nearDist;
	_desc.range = _camera.GetDesc().farDist;
}

const LightDesc& LightCI::GetDesc() const
{
	return _desc;
}

void LightCI::SetDesc(const LightDesc& value)
{
	_changed = true;

	_desc = value;

	CameraDesc lightCam;
	lightCam.pos = _desc.pos;
	lightCam.dir = _desc.dir;
	lightCam.up = _desc.up;
	lightCam.fov = _desc.phi;
	lightCam.nearDist = _desc.nearDist;
	lightCam.farDist = _desc.range;
	lightCam.aspect = _desc.aspect;
	lightCam.width = tan(_desc.phi/2.0f) * _desc.range * 2.0f;

	switch (_desc.type)
	{
	case D3DLIGHT_POINT:
	case D3DLIGHT_SPOT:
		lightCam.style = csPerspective;
		break;
	case D3DLIGHT_DIRECTIONAL:
		lightCam.style = csOrtho;
		break;
	default:
		LSL_ASSERT(false);
	}

	_camera.SetDesc(lightCam);
}

const CameraCI& LightCI::GetCamera() const
{
	return _camera;
}




ContextInfo::ContextInfo(RenderDriver* driver): _driver(driver), _enableShadow(false), _texDiffK(1.0f), _invertingCullFace(false), _ignoreMaterial(false), _cullOpacity(1.0f), _color(clrWhite), _meshId(-1), _maxTextureStage(-1)
{
	ZeroMemory(_textures, sizeof(_textures));
	std::memcpy(_renderStates, defaultRenderStates, sizeof(defaultRenderStates));

	for (int i = 0; i < cMaxTexSamplers; ++i)
	{		
		std::memcpy(_samplerStates[i], defaultSamplerStates, sizeof(defaultSamplerStates));
		std::memcpy(_textureStageStates[i], defaultTextureStageStates, sizeof(defaultTextureStageStates));
	}

	_lastLight = _lightList.end();
}

ContextInfo::~ContextInfo()
{
	LSL_ASSERT(_cameraStack.empty() && _shaderStack.empty() && _frameStack.empty());
}

DWORD ContextInfo::InvertCullFace(DWORD curFace)
{
	switch (curFace)
	{
	case D3DCULL_CW:
		return D3DCULL_CCW;
		break;
	case D3DCULL_CCW:
		return D3DCULL_CW;
		break;	
	}
	return curFace;
}

void ContextInfo::SetCamera(CameraCI* camera)
{
	camera->WorldMatChanged(_worldMat);
	_driver->SetTransform(tstView, &camera->GetView());
	_driver->SetTransform(tstProj, &camera->GetProj());
}

void ContextInfo::SetLight(LightCI* light, DWORD lightIndex)
{
	if (!light)
	{
		_driver->GetDevice()->SetLight(0, 0);
		return;
	}

	light->_camera.WorldMatChanged(_worldMat);

	D3DLIGHT9 d3dLight;
	d3dLight.Ambient = light->_desc.ambient;
	d3dLight.Attenuation0 = light->_desc.attenuation0;
	d3dLight.Attenuation1 = light->_desc.attenuation1;
	d3dLight.Attenuation2 = light->_desc.attenuation2;
	d3dLight.Diffuse = light->_desc.diffuse;
	d3dLight.Direction = light->_desc.dir;
	d3dLight.Falloff = light->_desc.falloff;
	d3dLight.Phi = light->_desc.phi;
	d3dLight.Position = light->_desc.pos;
	d3dLight.Range = light->_desc.range;
	d3dLight.Specular = light->_desc.specular;
	d3dLight.Theta = light->_desc.theta;
	d3dLight.Type = light->_desc.type;
	
	_driver->GetDevice()->SetLight(lightIndex, &d3dLight);
}

void ContextInfo::SetLightEnable(DWORD lightIndex, bool value)
{
	_driver->GetDevice()->LightEnable(lightIndex, value);
}

void ContextInfo::SetDefaults()
{
	_invertingCullFace = false;
	_ignoreMaterial = false;
	_cullOpacity = 1.0f;
	_color = clrWhite;
	_meshId = -1;

	//Ёти умолчани€ отличаютс€ от стандартного состо€ни€ d3d9
	_driver->SetRenderState(rsCullMode, D3DCULL_CW);
	_driver->SetRenderState(rsNormalizeNormals, true);
	_driver->SetRenderState(rsZEnable, true);
	for (int  i = 0; i < cMaxTexSamplers; ++i)
	{
		//Ёти значени€ по умолчанию завис€т от установленной текстуры в данном stage, приводим их к стд виду
		_driver->SetTexture(i, 0);

		for (int j = 0; j < TEXTURE_STAGE_STATE_END; ++j)
			_driver->SetTextureStageState(i, graph::TextureStageState(j), GetDefTextureStageState(i, (TextureStageState)j));
	}

	for (int i = 0; i < RENDER_STATE_END; ++i)
		_driver->SetRenderState(RenderState(i), _renderStates[i]);
}

void ContextInfo::BeginDraw()
{
	for (int i = 0; i <= _maxTextureStage; ++i)
		if (!_textureMatStack[i].empty())
		{
			D3DXMATRIX mat = _textureMatStack[i].front();
			for (unsigned j = 1; j < _textureMatStack[i].size(); ++j)
				mat = mat * _textureMatStack[i][j];

			mat._31 = mat._41;
			mat._32 = mat._42;
			mat._33 = mat._43;
			mat._41 = mat._42 = mat._43 = 0;

			_driver->SetTransform(cTexTransform[i], &mat);

			if (GetTextureStageState(i, tssTextureTransformFlags) == D3DTTFF_DISABLE)
				SetTextureStageState(i, tssTextureTransformFlags, D3DTTFF_COUNT2);
		}
		else if (GetTextureStageState(i, tssTextureTransformFlags) != D3DTTFF_DISABLE)
			SetTextureStageState(i, tssTextureTransformFlags, D3DTTFF_DISABLE);
}

void ContextInfo::EndDraw(bool nextPass)
{
}

void ContextInfo::ApplyCamera(CameraCI* camera)
{
	LSL_ASSERT(camera);

	_cameraStack.push(camera);
	camera->AddRef();

	SetCamera(camera);
}

void ContextInfo::UnApplyCamera(CameraCI* camera)
{
	LSL_ASSERT(camera && _cameraStack.top() == camera);

	_cameraStack.top()->Release();
	_cameraStack.pop();

	if (!_cameraStack.empty())
		SetCamera(_cameraStack.top());
}

void ContextInfo::AddLight(LightCI* value)
{
	LSL_ASSERT(value && value->_owner == 0);

	unsigned id = (_lastLight == _lightList.end()) ? _lightList.size() : (*_lastLight)->_id + 1;
	
	Lights::const_iterator iter = _lastLight = _lightList.insert(_lastLight, value);
	value->_owner = this;
	value->_id = id;
	++iter;
	//ћеста нет, берем конец списка	
	if (!(iter != _lightList.end() && (*iter)->_id - id > 1))
		_lastLight = _lightList.end();

	SetLight(value, id);
}

void ContextInfo::RemoveLight(LightCI* value)
{
	LSL_ASSERT(value->_owner == this && _lightEnable.End(value) == 0);

	for (Lights::iterator iter = _lightList.begin(); iter != _lightList.end(); ++iter)
		if (*iter == value)
		{
			_lastLight = _lightList.erase(iter);
			if (_lastLight != _lightList.begin())
				--_lastLight;

			value->_owner = 0;

			return;
		}

	LSL_ASSERT(false);
}

bool ContextInfo::GetLightEnable(LightCI* light) const
{
	return _lightEnable.Back(light);
}

void ContextInfo::SetLightEnable(LightCI* light, bool value)
{
	if (_lightEnable.Push(light, value))	
		SetLightEnable(light->_id, value);
}

void ContextInfo::RestoreLightEnable(LightCI* value)
{
	if (_lightEnable.Pop(value))
	{
		const bool* val = _lightEnable.End(value);
		SetLightEnable(value->_id, val ? *val : false);
	}
}

bool ContextInfo::GetLightShadow() const
{
	return _enableShadow;
}

void ContextInfo::SetLightShadow(bool value)
{
	_enableShadow = value;
}

void ContextInfo::RestoreLightShadow()
{
	_enableShadow = false;
}

float ContextInfo::GetTexDiffK() const
{
	return _texDiffK;
}

void ContextInfo::SetTexDiffK(float value)
{
	_texDiffK = value;
}

void ContextInfo::PushShader(BaseShader* value)
{
	LSL_ASSERT(value);

	_shaderStack.push_back(value);
}

void ContextInfo::PopShader(BaseShader* value)
{
	LSL_ASSERT(value && _shaderStack.back() == value);

	_shaderStack.pop_back();
}

bool ContextInfo::IsShaderActive() const
{
	return _shaderStack.size() > 0;
}

const D3DXMATRIX& ContextInfo::GetWorldMat() const
{
	return _worldMat;
}

void ContextInfo::SetWorldMat(const D3DXMATRIX& value)
{
	_worldMat = value;
	if (!_cameraStack.empty())
		_cameraStack.top()->WorldMatChanged(_worldMat);

	for (Lights::iterator iter = _lightList.begin(); iter != _lightList.end(); ++iter)
		(*iter)->_camera.WorldMatChanged(_worldMat);

	_driver->SetTransform(tstWorld, &_worldMat);
}

void ContextInfo::PushTextureTransform(int stage, const D3DXMATRIX& value)
{
	_textureMatStack[stage].push_back(value);
}

void ContextInfo::PopTextureTransform(int stage)
{
	_textureMatStack[stage].pop_back();
}

const MaterialDesc& ContextInfo::GetMaterial() const
{
	return _material;
}

void ContextInfo::SetMaterial(const MaterialDesc& value)
{
	_material = value;

	if (!_ignoreMaterial)
	{
		D3DMATERIAL9 mat;
		mat.Ambient = _material.ambient * _color;
		mat.Diffuse = _material.diffuse * _color;
		mat.Emissive = _material.emissive * _color;
		mat.Power = _material.power;
		mat.Specular = _material.specular;
		_driver->SetMaterial(&mat);
	}
}

DWORD ContextInfo::GetRenderState(RenderState type)
{
	return _renderStates[type];
}

void ContextInfo::SetRenderState(RenderState type, DWORD value)
{
	if (type == rsCullMode && _invertingCullFace)
		value = InvertCullFace(value);
	if (_renderStates[type] != value)
	{		
		_renderStates[type] = value;
		_driver->SetRenderState(type, value);
	}
}

void ContextInfo::RestoreRenderState(RenderState type)
{
	SetRenderState(type, defaultRenderStates[type]);
}

IDirect3DBaseTexture9* ContextInfo::GetTexture(DWORD sampler)
{
	return _textures[sampler];
}

void ContextInfo::SetTexture(DWORD sampler, IDirect3DBaseTexture9* value)
{	
	if (_textures[sampler] != value)
	{
		_textures[sampler] = value;

		if (!_ignoreMaterial)
			_driver->SetTexture(sampler, _textures[sampler]);

		if (value)
			_maxTextureStage = std::max<int>(_maxTextureStage, sampler);
		else
		{
			int maxTextureStage = -1;
			for (int i = _maxTextureStage; i >= (int)sampler; --i)
				if (_textures[i] != NULL)
				{
					maxTextureStage = i;
					break;
				}
			_maxTextureStage = maxTextureStage;
		}
	}
}

DWORD ContextInfo::GetSamplerState(DWORD sampler, SamplerState type)
{
	return _samplerStates[sampler][type];
}

void ContextInfo::SetSamplerState(DWORD sampler, SamplerState type, DWORD value)
{
	if (_samplerStates[sampler][type] != value)
	{
		_samplerStates[sampler][type] = value;
		if (!_ignoreMaterial)
			_driver->SetSamplerState(sampler, type, value);
	}
}

void ContextInfo::RestoreSamplerState(DWORD sampler, SamplerState type)
{
	SetSamplerState(sampler, type, defaultSamplerStates[type]);
}

DWORD ContextInfo::GetTextureStageState(DWORD sampler, TextureStageState type)
{
	return _textureStageStates[sampler][type];
}

void ContextInfo::SetTextureStageState(DWORD sampler, TextureStageState type, DWORD value)
{
	if (_textureStageStates[sampler][type] != value)
	{
		_textureStageStates[sampler][type] = value;
		if (!_ignoreMaterial)
			_driver->SetTextureStageState(sampler, type, value);	
	}
}

void ContextInfo::RestoreTextureStageState(DWORD sampler, TextureStageState type)
{
	SetTextureStageState(sampler, type, GetDefTextureStageState(sampler, type));
}

bool ContextInfo::GetInvertingCullFace() const
{
	return _invertingCullFace;
}

void ContextInfo::SetInvertingCullFace(bool value)
{
	//ћетод не работает!!!
	LSL_ASSERT(false);

	_invertingCullFace = value;
	SetRenderState(rsCullMode, InvertCullFace(_renderStates[rsCullMode]));
}

bool ContextInfo::GetIgnoreMaterial()
{
	return _ignoreMaterial;
}

void ContextInfo::SetIgnoreMaterial(bool value)
{
	_ignoreMaterial = value;
}

const ContextInfo::ShaderStack& ContextInfo::GetShaderStack() const
{
	return _shaderStack;
}

const CameraCI& ContextInfo::GetCamera() const
{
	return *_cameraStack.top();
}

const LightCI& ContextInfo::GetLight(unsigned id) const
{
	LSL_ASSERT(_lightList.size() > id);

	return *_lightList[id];
}

const ContextInfo::Lights& ContextInfo::GetLights() const
{
	return _lightList;
}

BaseShader& ContextInfo::GetShader()
{
	return *_shaderStack.front();
}

float ContextInfo::GetFrame() const
{
	return _frameStack.top();
}

void ContextInfo::PushFrame(float value)
{
	_frameStack.push(value);
}

void ContextInfo::PopFrame()
{
	_frameStack.pop();
}

float ContextInfo::GetCullOpacity() const
{
	return _cullOpacity;
}

void ContextInfo::SetCullOpacity(float value)
{
	_cullOpacity = value;
}

void ContextInfo::RestoreCullOpacity()
{
	_cullOpacity = 1.0f;
}

bool ContextInfo::IsCullOpacity() const
{
	return _cullOpacity < 1.0f;
}

const D3DXCOLOR& ContextInfo::GetColor() const
{
	return _color;
}

void ContextInfo::SetColor(const D3DXCOLOR& value)
{
	_color = value;
}

int ContextInfo::GetMeshId() const
{
	return _meshId;
}

void ContextInfo::SetMeshId(int value)
{
	_meshId = value;
}

bool ContextInfo::IsNight() const
{
	return _lightList.size() > 0 && _lightList.front()->GetDesc().type == D3DLIGHT_SPOT;
}

}

}