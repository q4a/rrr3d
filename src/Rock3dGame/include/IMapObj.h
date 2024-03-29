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

	virtual glm::vec3 GetPos() const = 0;
	virtual void SetPos(const glm::vec3& value) = 0;

	virtual glm::vec3 GetScale() const = 0;
	virtual void SetScale(const glm::vec3& value) = 0;

	virtual glm::quat GetRot() const = 0;
	virtual void SetRot(const glm::quat& value) = 0;

	virtual IMapObjRecRef GetRecord() = 0;
};
typedef AutoRef<IMapObj> IMapObjRef;

}

}