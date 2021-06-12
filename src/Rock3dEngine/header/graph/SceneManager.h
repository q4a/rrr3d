#ifndef SCENE_MANAGER
#define SCENE_MANAGER

#include "lslCollection.h"
#include "lslComponent.h"
#include "graph\\Engine.h"
#include "ProgressTimer.h"
#include <bitset>

namespace r3d
{

namespace graph
{

class MaterialNode;

class BaseSceneNode: public lsl::Component, protected IProgressUser
{
	friend class SceneManager;
private:
	typedef Component _MyBase;

	enum _RotationStyle {rsEulerAngles, rsQuaternion, rsVectors, ROTATION_STYLE_END};
	enum _Change {ocTransformation, ocWorldTransformation, CHANGE_END};
	enum _BBChange {bbcStructure, bbcAbsStructure, bbcChild, bbcIncludeChild, bbcWorldIncludeChild, BB_CHANGE_END};

	typedef lsl::List<BaseSceneNode*> _ChildBBDynList;
public:
	typedef std::list<SceneManager*> SceneList;

	class Children: public lsl::Container<BaseSceneNode*>
	{
	private:
		typedef lsl::Container<BaseSceneNode*> _MyBase;
	private:
		BaseSceneNode* _owner;
	protected:
		virtual void InsertItem(Item& item)
		{
			LSL_ASSERT(!item->_parent && item != _owner);

			_MyBase::InsertItem(item);

			_owner->InsertChild(item);			
		}
		virtual void RemoveItem(Item& item)
		{
			LSL_ASSERT(item->_parent == _owner);

			_MyBase::RemoveItem(item);

			_owner->RemoveChild(item);
		}
	public:
		Children(BaseSceneNode* owner): _owner(owner) {}
	};

	class ProxyList: public lsl::Container<BaseSceneNode*>
	{
	private:
		typedef lsl::Container<BaseSceneNode*> _MyBase;
	private:
		BaseSceneNode* _owner;
	protected:
		virtual void InsertItem(Item& item)
		{
			LSL_ASSERT(!item->_proxyMaster);

			_MyBase::InsertItem(item);

			item->_proxyMaster = _owner;
		}

		virtual void RemoveItem(Item& item)
		{
			LSL_ASSERT(item->_proxyMaster == _owner);

			_MyBase::RemoveItem(item);

			item->_proxyMaster = 0;
		}
	public:
		ProxyList(BaseSceneNode* owner): _owner(owner) {}
	};

	enum AnimMode {amNone, amOnce, amRepeat, amTile, amTwoSide, amManual, amInheritance};

	static void RenderBB(graph::Engine& engine, const AABB& aabb, const D3DXCOLOR& colorBB);

	enum CombMatType {cmtScaleTrans, cmtScaleRot, cmtRotTrans};
	
	enum NodeOpt {noDynStructure, NodeOptEnd};
	typedef lsl::Bitset<NodeOptEnd> NodeOpts;
private:
	SceneList _sceneList;

	BaseSceneNode* _parent;
	Children* _children;
	BaseSceneNode* _proxyMaster;
	ProxyList* _proxyList;

	D3DXVECTOR3 _position;
	D3DXVECTOR3 _scale;
	bool _visible;
	NodeOpts _options;
	int _tag;

	//Время рендеринга в сцене
	float _time;
	AnimMode _animMode;

	//Эти переменные отображают кооректность и актуальность промежуточных полей(обычно используемых для оптимизации расчетов), и могут быть изменены даже для const.
	mutable lsl::Bitset<CHANGE_END> _changes;
	mutable lsl::Bitset<ROTATION_STYLE_END> _rotInvalidate;
	mutable bool _rotChanged;
	mutable lsl::Bitset<BB_CHANGE_END> _bbChanges;

	//Различное представление поворота, эти переменные кэшируются и в случае необходимости вычисляются из локальной матрицы(поскольку существует два слабо связных способа представления) поэтому имеют mutable для const
	mutable D3DXVECTOR3 _direction;
	mutable D3DXVECTOR3 _up;
	mutable float _rollAngle;
	mutable float _pitchAngle;
	mutable float _turnAngle;
	mutable D3DXQUATERNION _rot;

	//Матрицы являются промежуточными переменными, и хранятся в полях объекта для оптимизации расчетов. Для вывода коорректных данных(если перерасчет матриц ещё не произошел) о самих матрицах и связных данных используется mutable для const
	mutable D3DXMATRIX _rotMat;
	mutable D3DXMATRIX _localMat;
	mutable D3DXMATRIX _invLocalMat;
	mutable D3DXMATRIX _worldMat;
	mutable D3DXMATRIX _invWorldMat;	

	mutable AABB _aabbLocal;
	mutable AABB _aabbWorld;
	mutable AABB _aabbOfChildren;
	mutable AABB _aabbIncludingChildren;
	mutable AABB _aabbWorldIncludeChild;

	unsigned _nodeDynRef;
	_ChildBBDynList _childBBDynList;

	void ExtractRotation(_RotationStyle style) const;
	void ChangedRotation(_RotationStyle style);

	void AddToScene(SceneManager* scene);
	void DeleteFromScene(SceneManager* scene);

	void InsertChild(BaseSceneNode* value);
	void RemoveChild(BaseSceneNode* value);

	void InsertChildBBDyn(BaseSceneNode* value);
	void RemoveChildBBDyn(BaseSceneNode* value);
	void SetDynBB(bool value);	
protected:
	void BuildMatrix() const;
	void BuildWorldMatrix() const;
	void ApplyTransformationChanged() const;

	//Уведомления об изменениях, они же если требуется, формируют общее уведомление об изменении (NotifyChanged)
	//Изменение абсолютных(в неё вклюdчается локальная и родительская) трансформаций в абсолютной системе координат
	virtual void WorldTransformationChanged();
	//Изменение локальных трансформаций в системе координат родителя
	virtual void TransformationChanged();
	//Изменение локальной структуры(т.е. его AABB) дочери
	virtual void ChildStructureChanged(BaseSceneNode* child);

	//
	virtual AABB LocalDimensions() const;

	//
	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
	virtual void OnFixUp(const FixUpNames& fixUpNames);	
public:
	BaseSceneNode();
	virtual ~BaseSceneNode();

	void Assign(BaseSceneNode& value);

	//Рендер в локальном пространстве координат
	virtual void DoRender(graph::Engine& engine) {};
	//Рендер в глобальном пространстве координат
	void Render(graph::Engine& engine, const D3DXMATRIX& worldMat);
	//Общая функция рендера
	virtual void Render(graph::Engine& engine);
	//Событие временного прогресса
	virtual void OnProgress(float deltaTime);

	//Изменение содержимого(полигональной сетки, вершин, т.е. его AABB) в локальной системе координат
	virtual void StructureChanged();
	//
	void MoveAroundTarget(const D3DXVECTOR3& worldTarget, float pitchDelta, float turnDelta);
	void AdjustDistToTarget(const D3DXVECTOR3& worldTarget, float distance);

	void WorldToLocal(const D3DXVECTOR4& vec, D3DXVECTOR4& out) const;
	void WorldToLocalCoord(const D3DXVECTOR3& vec, D3DXVECTOR3& out) const;
	void WorldToLocalNorm(const D3DXVECTOR3& vec, D3DXVECTOR3& out) const;
	void LocalToWorld(const D3DXVECTOR4& vec, D3DXVECTOR4& out) const;
	void LocalToWorldCoord(const D3DXVECTOR3& vec, D3DXVECTOR3& out) const;
	void LocalToWorldNorm(const D3DXVECTOR3& vec, D3DXVECTOR3& out) const;
	void ParentToLocal(const D3DXVECTOR4& vec, D3DXVECTOR4& out) const;
	void LocalToParent(const D3DXVECTOR4& vec, D3DXVECTOR4& out) const;

	unsigned RayCastIntersBB(const D3DXVECTOR3& wRayPos, const D3DXVECTOR3& wRayVec, bool includeChild = false) const;
	unsigned RayCastIntersBB(const D3DXVECTOR3& wRayPos, const D3DXVECTOR3& wRayVec, D3DXVECTOR3& wNearVec, D3DXVECTOR3& wFarVec, bool includeChild = false) const;

	void InsertToScene(SceneManager* scene);
	void RemoveFromScene(SceneManager* scene);
	void ClearSceneList();

	//динамичная структура
	bool IsBBDyn() const;
	//динмачый узел, что означает что он сам или один из его потомков имеет динамичную структуру
	bool IsNodeBBDyn() const;

	const SceneList& GetSceneList() const;
	void SetSceneList(const SceneList& value);
	void SetScene(SceneManager* scene, bool set);

	BaseSceneNode* GetParent();
	const BaseSceneNode* GetParent() const;
	void SetParent(BaseSceneNode* parent);
	Children& GetChildren();
	//
	BaseSceneNode* GetProxyMaster();
	ProxyList& GetProxyList();
	//
	bool GetVisible() const;
	void SetVisible(bool value);

	//Локальные координаты
	//Позиция
	const D3DXVECTOR3& GetPos() const;
	void SetPos(const D3DXVECTOR3& value);
	//Масштаб
	const D3DXVECTOR3& GetScale() const;
	void SetScale(const D3DXVECTOR3& value);
	void SetScale(float value);
	//Поворот
	//Поворот по координатным осям
	const D3DXVECTOR3& GetDir() const;
	void SetDir(const D3DXVECTOR3& value);
	D3DXVECTOR3 GetRight() const;	
	void SetRight(const D3DXVECTOR3& value);
	const D3DXVECTOR3& GetUp() const;
	void SetUp(const D3DXVECTOR3& value);
	//Поворот по углам эйлера
	float GetRollAngle() const;
	void SetRollAngle(float value);
	float GetPitchAngle() const;
	void SetPitchAngle(float value);
	float GetTurnAngle() const;
	void SetTurnAngle(float value);
	//Поворот по кватерниону
	const D3DXQUATERNION& GetRot() const;
	void SetRot(const D3DXQUATERNION& value);

	//Локальные матрицы
	D3DXMATRIX GetScaleMat() const;
	D3DXMATRIX GetRotMat() const;
	D3DXMATRIX GetTransMat() const;
	//Рез. локальная матрица
	const D3DXMATRIX& GetMat() const;
	//Матрица не должна содержать масштабирования!
	void SetLocalMat(const D3DXMATRIX& value);
	const D3DXMATRIX& GetInvMat() const;
	//Мировая матрица
	const D3DXMATRIX& GetWorldMat() const;
	const D3DXMATRIX& GetInvWorldMat() const;
	//Комбинированные матрицы
	D3DXMATRIX GetCombMat(CombMatType type) const;
	D3DXMATRIX GetWorldCombMat(CombMatType type) const;

	//Мировые координаты
	D3DXVECTOR3 GetWorldPos() const;
	void SetWorldPos(const D3DXVECTOR3& value);
	D3DXQUATERNION GetWorldRot() const;
	void SetWorldRot(const D3DXQUATERNION& value);
	//Невозможно представить мировое масштабирование с помощью одного вектора поскольку направление масштабирования зависит от поворота. Поэтому представляется в виде матрицы
	D3DXMATRIX GetWorldScale() const;
	//
	D3DXVECTOR3 GetWorldDir() const;
	D3DXVECTOR3 GetWorldRight() const;
	D3DXVECTOR3 GetWorldUp() const;

	//Ограничительный объем
	//
	D3DXVECTOR3 GetWorldSizes(bool includeChild) const;
	//
	D3DXVECTOR3 GetWorldCenterPos(bool includeChild) const;
	//AABB в локальной системе координат
	const AABB& GetLocalAABB(bool includeChild) const;
	//AABB в абсолютной системе координат
	const AABB& GetWorldAABB(bool includeChild) const;
	//AABB только дочерей в локальной системе координат
	const AABB& GetAABBOfChildren() const;

	bool GetOpt(NodeOpt option) const;
	void SetOpt(NodeOpt option, bool value);

	virtual MaterialNode* GetMaterial() {return NULL;}

	float GetFrame(float time) const;

	int tag() const;
	void tag(int value);

	//Режим анимации
	AnimMode animMode() const;
	void animMode(AnimMode value);

	//Длительность всей анимации в сек. По умолчанию 1
	float animDuration;
	//Кадр анимации при animMode = amManual
	float frame;

	bool showBB;	
	bool showBBIncludeChild;	
	D3DXCOLOR colorBB;
	bool storeCoords;
	bool invertCullFace;	

	D3DXVECTOR3 speedPos;
	D3DXVECTOR3 speedScale;
	D3DXQUATERNION speedRot;
	bool autoRot;

#ifdef _DEBUG
	AABB* renderBB;
#endif
};

class SceneDummy: public BaseSceneNode
{
public:
	virtual void DoRender(graph::Engine& engine);
};

class Camera: public BaseSceneNode
{
private:
	typedef BaseSceneNode _MyBase;
public:
	static void RenderFrustum(graph::Engine& engine, const D3DXMATRIX& invViewProj, const D3DXCOLOR& colorBB);
private:
	mutable graph::CameraDesc _desc;
	mutable graph::CameraCI _contextInfo;
	mutable bool _changedCI;

	float _width;

	void BuildContextInfo() const;
	void ChangedCI();
protected:
	virtual void WorldTransformationChanged();

	virtual void DoRender(graph::Engine& engine);
public:
	Camera();

	void Apply(graph::Engine& engine);
	void UnApply(graph::Engine& engine);

	void AdjustNearFarPlane(const AABB& aabb, float nearDist, float farDist);

	const graph::CameraCI& GetContextInfo() const;

	//Длина стороны вида(плоскость проекции zNear), в пространстве камеры
	//Пока используется только для ортографической проекции
	float GetWidth() const;
	void SetWidth(float value);	
	//Форматное отношение сторон вида
	float GetAspect() const;
	void SetAspect(float value);
	//
	float GetFov() const;
	void SetFov(float value);
	//
	float GetNear() const;
	void SetNear(float value);
	//
	float GetFar() const;
	void SetFar(float value);
	//	
	CameraStyle GetStyle() const;
	void SetStyle(CameraStyle value);
};

class LightSource: public BaseSceneNode
{	
private:
	typedef BaseSceneNode _MyBase;
private:
	mutable graph::LightDesc _desc;
	mutable graph::LightCI _contextInfo;
	mutable bool _changedCI;

	void BuildContextInfo() const;
	void ChangedCI();
protected:
	virtual void WorldTransformationChanged();

	virtual void DoRender(graph::Engine& engine);
public:
	LightSource();
	~LightSource();

	void Apply(graph::Engine& engine, DWORD lightIndex);
	void UnApply(graph::Engine& engine, DWORD lightIndex);

	void AdjustNearFarPlane(const AABB& aabb, float nearDist, float farDist);

	const graph::LightCI& GetContextInfo() const;

	const D3DXCOLOR& GetAmbient() const;
	void SetAmbient(const D3DXCOLOR& value);

	const D3DXCOLOR& GetDiffuse() const;
	void SetDiffuse(const D3DXCOLOR& value);

	const D3DXCOLOR& GetSpecular() const;
	void SetSpecular(const D3DXCOLOR& value);

	float GetNear() const;
	void SetNear(float value);

	float GetFar() const;
	void SetFar(float value);

	D3DLIGHTTYPE GetType() const;
	void SetType(D3DLIGHTTYPE value);

	float GetFalloff() const;
	void SetFalloff(float value);

	float GetPhi() const;
	void SetPhi(float value);
	
	float GetTheta() const;
	void SetTheta(float value);

	graph::Tex2DResource* GetShadowMap();
	void SetShadowMap(graph::Tex2DResource* value);
};

class SceneManager: public lsl::Component, public graph::Renderable
{
	friend class SceneRender;
public:
	typedef std::list<BaseSceneNode*> Objects;
private:
	Objects	_objects;
	SceneRender* _sceneRender;
public:
	SceneManager();
	virtual ~SceneManager();

	virtual void Render(graph::Engine& engine);

	void InsertObject(BaseSceneNode* object);
	void RemoveObject(BaseSceneNode* object);
	
	const Objects& GetObjects() const;

	SceneRender* GetSceneRender();
	void SetSceneRender(SceneRender* value);
};

class SceneRender: public lsl::Object
{
public:
	typedef std::list<SceneManager*> SceneList;
private:
	SceneList _sceneList;
public:
	virtual ~SceneRender();

	virtual void Render(graph::Engine& engine, SceneManager* scene) = 0;

	void InsertScene(SceneManager* value);
	void RemoveScene(SceneManager* value);
	
	const SceneList& GetSceneList() const;
};

//OcreeSceneManager имеет смысл реализоввать как совокупность SceneManager (чтобы не нарушать единство разбиения). Препологается что сам по себе менеджер сцены будет всеголишь списком объектов сцены

}

}

#endif