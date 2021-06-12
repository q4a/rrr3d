#pragma once

#include "IDataBase.h"

namespace r3d
{

namespace edit
{

class IMapObj: public ExternInterf
{
public:
	virtual const std::string& GetName() const = 0;

	virtual D3DXVECTOR3 GetPos() const = 0;
	virtual void SetPos(const D3DXVECTOR3& value) = 0;

	virtual D3DXVECTOR3 GetScale() const = 0;
	virtual void SetScale(const D3DXVECTOR3& value) = 0;

	virtual D3DXQUATERNION GetRot() const = 0;
	virtual void SetRot(const D3DXQUATERNION& value) = 0;

	virtual IMapObjRecRef GetRecord() = 0;
};
typedef AutoRef<IMapObj> IMapObjRef;

}

}