#pragma once

#include "ISceneControl.h"

namespace r3d
{
	namespace edit
	{
		class ScNodeCont : public IScNodeCont
		{
		private:
			graph::BaseSceneNode* _node;
		protected:
			void Reset(graph::BaseSceneNode* node);
		public:
			ScNodeCont(graph::BaseSceneNode* node);
			~ScNodeCont() override;
			D3DXVECTOR3 _lastPos;
			D3DXQUATERNION _lastRot;
			D3DXVECTOR3 _lastScale;

			bool RayCastInters(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec) const override;
			bool Compare(const IMapObjRef& node) const override;

			D3DXVECTOR3 GetPos() const override;
			void SetPos(const D3DXVECTOR3& value) override;
			//
			D3DXVECTOR3 GetLastPos() const override;
			void SetLastPos(const D3DXVECTOR3& value) override;
			//
			D3DXQUATERNION GetRot() const override;
			void SetRot(const D3DXQUATERNION& value) override;
			//
			D3DXQUATERNION GetLastRot() const override;
			void SetLastRot(const D3DXQUATERNION& value) override;
			//
			D3DXVECTOR3 GetScale() const override;
			void SetScale(const D3DXVECTOR3& value) override;
			//
			D3DXVECTOR3 GetLastScale() const override;
			void SetLastScale(const D3DXVECTOR3& value) override;
			//
			D3DXVECTOR3 GetDir() const override;
			D3DXVECTOR3 GetRight() const override;
			D3DXVECTOR3 GetUp() const override;

			D3DXMATRIX GetMat() const override;
			//В локальных координатах, включая дочерние узлы
			AABB GetAABB() const override;

			graph::BaseSceneNode* GetNode() const;
		};
	}
}
