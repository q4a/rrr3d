#pragma once

#include "graph\\Engine.h"
#include "graph\\MaterialLibrary.h"
#include "ProgressTimer.h"

namespace r3d
{

namespace graph {class Camera;}

namespace gui
{

using namespace lsl;

class Context;
class Manager;

struct MouseClick
{
	MouseClick(): key(mkLeft), state(ksUp), shift1(false), coord(0, 0) {}

	MouseKey key;
	KeyState state;
	bool shift1;
	
	//локальные координаты
	D3DXVECTOR2 coord;
	//мировые координаты
	D3DXVECTOR2 worldCoord;
};

struct MouseMove
{
	MouseMove(): shift1(false), coord(0, 0) {}

	bool shift1;
	//локальные координаты
	D3DXVECTOR2 coord;
	//Разность между текущим и предыдущим значением координаты
	D3DXVECTOR2 dtCoord;
	//Разность между текущим значением координаты и координатой при клике
	D3DXVECTOR2 offCoord;
	//мировые координаты
	D3DXVECTOR2 worldCoord;

	//Состояние последнего клика мыши
	MouseClick click;
};

class Material: public Object
{
public:
	enum Blending {bmOpaque, bmTransparency, bmAdditive};
	enum AlphaTest {asNone, asLessOrEq};
private:
	D3DXCOLOR _color;
	Blending _blending;
	AlphaTest _alphaTest;
	
	graph::Sampler2d _sampler;
public:
	Material();

	D3DXVECTOR2 GetImageSize();
	
	const D3DXCOLOR& GetColor() const;
	void SetColor(const D3DXCOLOR& value);

	float GetAlpha() const;
	void SetAlpha(float value);

	Blending GetBlending() const;
	void SetBlending(Blending value);

	AlphaTest GetAlphaTest() const;
	void SetAlphaTest(AlphaTest value);

	graph::Sampler2d& GetSampler();
};

//Абстракция базового графического элемента, с настройками визуализации, к которому может применяться шаблон оформления (шаблон конфигурируется вне)
class Graphic: public Object
{
	friend Context;
private:
	Context* _context;
	D3DXVECTOR2 _pos;
	D3DXVECTOR2 _size;
	bool _active;
	float _alpha;

	Material* _material;
	bool _createMat;
protected:
	Graphic(Context* context);
	virtual ~Graphic();

	virtual void TransformChanged() {};
public:
	virtual void Draw() = 0;

	Context& GetContext();

	const D3DXVECTOR2& GetPos() const;
	void SetPos(const D3DXVECTOR2& value);
	//
	const D3DXVECTOR2& GetSize() const;
	void SetSize(const D3DXVECTOR2& value);

	//Без материала Graphic не рисуется
	Material* GetOrCreateMaterial();
	Material* GetMaterial();
	void SetMaterial(Material* value);

	bool GetActive() const;
	void SetActive(bool value);

	float GetAlpha() const;
	void SetAlpha(float value);
};

class Plane: public Graphic
{
	friend Context;
	typedef Graphic _MyBase;
protected:
	Plane(Context* context);
public:
	virtual void Draw();
};

//Пока не введена операция вычисления Rect текста, может пригодятся для определения точного размера, должна быть типа AABB
class BaseText: public Graphic
{
	friend Context;
	typedef Graphic _MyBase;
public:
	enum HorAlign {haLeft = 0, haCenter, haRight, cHorAlignEnd};
	enum VertAlign {vaTop = 0, vaBottom, vaCenter, cVertAlignEnd};
private:
	graph::TextFont* _font;

	HorAlign _horAlign;
	VertAlign _vertAlign;
	bool _wordWrap;
	bool _clipEnable;
	float _vScroll;

	AABB2 _textAABB;
	bool _textAABBChanged;

	void BuildTextAABB();	
protected:
	BaseText(Context* context);
	virtual ~BaseText();

	void TextAABBChanged();

	virtual void DrawText(AABB2* aabb) = 0;
	virtual void Draw();
	virtual void TransformChanged();
public:
	graph::TextFont* GetFont();
	void SetFont(graph::TextFont* value);

	HorAlign GetHorAlign() const;
	void SetHorAlign(HorAlign value);

	VertAlign GetVertAlign() const;
	void SetVertAlign(VertAlign value);

	//перенос по словам
	bool GetWordWrap() const;
	void SetWordWrap(bool value);

	bool GetClipEnable() const;
	void SetClipEnable(bool value);

	//Область в которой текст форматируется и рендерится, тип двухмерной вектор, введение типа AABB2 излишне
	using _MyBase::GetSize;
	using _MyBase::SetSize;

	const AABB2& GetTextAABB();

	float GetVScroll() const;
	void SetVScroll(float value);
};

class Text: public BaseText
{
	friend Context;
	typedef BaseText _MyBase;
private:
	std::string _text;
protected:
	Text(Context* context);
	
	virtual void DrawText(AABB2* aabb);
public:
	const std::string& GetText() const;
	void SetText(const std::string& value);
};

class TextW: public BaseText
{
	friend Context;
	typedef BaseText _MyBase;
private:
	std::wstring _text;
protected:
	TextW(Context* context);
	
	virtual void DrawText(AABB2* aabb);
public:
	const std::wstring& GetText() const;
	void SetText(const std::wstring& value);
};

class Graphic3d: public Object
{
	friend Context;
public:
	typedef lsl::List<Graphic3d*> Children;
private:
	Context* _context;
	Graphic3d* _parent;
	Children _children;
	bool _active;

	D3DXVECTOR3 _pos;
	D3DXQUATERNION _rot;
	D3DXVECTOR3 _scale;

	Material* _material;
	bool _createMat;
protected:
	Graphic3d(Context* context);
	virtual ~Graphic3d();

	virtual AABB LocalAABB() const = 0;

	virtual void TransformChanged() {}
	virtual void StructureChanged() {}
public:
	virtual void Draw() = 0;

	void InsertChild(Graphic3d* value);
	void RemoveChild(Children::const_iterator iter);
	void RemoveChild(Graphic3d* value);
	void MoveChildTo(Graphic3d* graphic, Children::const_iterator pos);
	void DeleteChildren();

	Context& GetContext();

	const D3DXVECTOR3& GetPos() const;
	void SetPos(const D3DXVECTOR3& value);

	D3DXQUATERNION GetRot() const;
	void SetRot(const D3DXQUATERNION& value);

	const D3DXVECTOR3& GetScale() const;
	void SetScale(const D3DXVECTOR3& value);

	D3DXMATRIX GetMat();
	D3DXMATRIX GetWorldMat();
	AABB GetChildAABB();
	AABB GetLocalAABB(bool includeChild);
	AABB GetWorldAABB(bool includeChild);

	Graphic3d* GetParent();
	void SetParent(Graphic3d* value);

	const Children& GetChildren() const;

	//Без материала Graphic не рисуется
	Material* GetOrCreateMaterial();
	Material* GetMaterial();
	void SetMaterial(Material* value);

	bool GetActive() const;
	void SetActive(bool value);

	bool invertCullFace;
};

class Dummy3d: public Graphic3d
{
	friend Context;
	typedef Graphic3d _MyBase;
protected:
	Dummy3d(Context* context);
	virtual ~Dummy3d();

	virtual AABB LocalAABB() const;
public:
	virtual void Draw();
};

class VBuf3d: public Graphic3d
{
	friend Context;
	typedef Graphic3d _MyBase;
private:
	graph::VBMesh* _mesh;
	bool _createMesh;
protected:
	VBuf3d(Context* context);
	virtual ~VBuf3d();

	virtual AABB LocalAABB() const;
public:
	virtual void Draw();

	graph::VBMesh* GetOrCreateMesh();
	graph::VBMesh* GetMesh();
	void SetMesh(graph::VBMesh* value);
};

class Mesh3d: public Graphic3d
{
	friend Context;
	typedef Graphic3d _MyBase;
private:
	graph::IndexedVBMesh* _mesh;
	int _meshId;
protected:
	Mesh3d(Context* context);
	virtual ~Mesh3d();

	virtual AABB LocalAABB() const;
public:
	virtual void Draw();

	graph::IndexedVBMesh* GetMesh();
	void SetMesh(graph::IndexedVBMesh* value);

	int GetMeshId() const;
	void SetMeshId(int value);
};

class Plane3d: public Graphic3d
{
	friend Context;
	typedef Graphic3d _MyBase;
private:
	D3DXVECTOR2 _size;
protected:
	Plane3d(Context* context);
	
	virtual AABB LocalAABB() const;
public:
	virtual void Draw();

	const D3DXVECTOR2& GetSize();
	void SetSize(const D3DXVECTOR2 value);
};

class View3d: public Graphic
{
	friend Context;
	typedef Graphic _MyBase;
private:
	Dummy3d* _box;
	bool _align;
protected:
	View3d(Context* context);
	virtual ~View3d();
public:
	virtual void Draw();

	Graphic3d* GetBox();

	//выравнивание по центру
	bool GetAlign() const;
	void SetAlign(bool value);
};

//контекст в котором отрисовывается Graphic
//Система координат экранная, единицы пиксели, начало в левом нижнем углу экрана
class Context: public Object
{
private:
	typedef lsl::List<Graphic*> Graphics;
	typedef lsl::List<Graphic3d*> Graphics3d;
private:
	graph::Engine* _engine;
	Graphics _graphics;
	Graphics3d _graphics3d;

	graph::CameraCI _camera;
	bool _invertY;
	D3DXVECTOR2 _vpSize;

	void InsertGraphic(Graphic* value);
	void RemoveGraphic(Graphic* value);
	void DeleteAllGraphics();

	void InsertGraphic3d(Graphic3d* value);
	void RemoveGraphic3d(Graphic3d* value);
	void DeleteAllGraphics3d();
	
	void ApplyMaterial(Material& material, float alpha);
	void UnApplyMaterial(Material& material);

	void BeginDrawGraphic(Graphic& graphic);
	void EndDrawGraphic(Graphic& graphic);

	void DrawGraphic3d(Graphic3d* graphic, const D3DXMATRIX& worldMat);
	template<class _Text> void DrawBaseText(_Text& text, AABB2* aabb);

	graph::Engine& GetEngine();
	graph::RenderDriver& GetDriver();
	graph::ContextInfo& GetCI();	
public:
	Context(graph::Engine* engine);
	virtual ~Context();

	void BeginDraw();
	void EndDraw();

	void SetTransform(const D3DXMATRIX& value);

	void DrawPlane(Plane& plane);
	void DrawText(Text& text, AABB2* aabb = 0);
	void DrawText(TextW& text, AABB2* aabb = 0);
	void DrawView3d(View3d& view3d);
	void DrawVBuf3d(VBuf3d& vBuf3d);
	void DrawPlane3d(Plane3d& plane3d);
	void DrawMesh3d(Mesh3d& mesh3d);

	Plane* CreatePlane();
	Text* CreateText();
	TextW* CreateTextW();
	Dummy3d* CreateDummy3d();
	VBuf3d* CreateVBuf3d();
	Mesh3d* CreateMesh3d();
	Plane3d* CreatePlane3d();
	View3d* CreateView3d();

	void ReleaseGraphic(Graphic* value);
	void ReleaseGraphic(Graphic3d* value);

	bool GetInvertY() const;
	void SetInvertY(bool value);

	const D3DXVECTOR2& GetVPSize();
	void SetVPSize(const D3DXVECTOR2& value);
};

//элемент управления включающий контейнер Graphic, обработку событий, ввода, связь родитель-потомок
class Widget: public Object, protected graph::IProgressUser
{
	friend Manager;
protected:	
	enum MatrixChange {mcLocal = 0, mcWorld, mcInvLocal, mcInvWorld, cMatrixChangeEnd};
	typedef lsl::Bitset<cMatrixChangeEnd> MatrixChanges;

	enum AABBChange {acLocalAABB = 0, acWorldAABB, acChildAABB, acLocalChildAABB, acWorldChildAABB, cAABBChangeEnd};
	typedef lsl::Bitset<cAABBChangeEnd> AABBChanges;

	enum StructChange {scLocal = 0, scWorld, scChild};
public:	
	typedef lsl::List<Graphic*> Graphics;
	typedef lsl::List<Widget*> Children;	

	enum Flag 
	{
		//Всегда перехватывать сообщения от мыши если они зоне охвата. Установлен по умлочанию
		wfCathMouseMessages = 0,
		//события OnClick срабатывает при всяком клике мыши
		wfMouseOnClick,

		//На переднем плане
		wfTopmost,

		//выравнивание по краям родителя
		wfAlignLeft,
		wfAlignTop,
		wfAlignRight,
		wfAlignBottom,

		wfClientClip,

		cFlagEnd
	};
	typedef lsl::Bitset<cFlagEnd> Flags;

	enum Anchor {waNone, waCenter, waLeft, waRight, waTop, waBottom, waLeftTop, waLeftBottom, waRightTop, waRightBottom};	
public:
	class Event: public lsl::ObjReference
	{
	public:
		virtual void OnStructureChanged(Widget* sender, StructChange change) {};

		virtual bool OnMouseClick(Widget* sender, const MouseClick& mClick){return false;}
		virtual bool OnMouseEnter(Widget* sender, const MouseMove& mMove) {return false;}
		virtual void OnMouseLeave(Widget* sender, bool wasReset) {}
		virtual bool OnMouseOver(Widget* sender, const MouseMove& mMove) {return false;}
		virtual bool OnClick(Widget* sender, const MouseClick& mClick) {return false;}
		//пользователем, неявно!, выбран item
		virtual bool OnSelect(Widget* sender, Object* item) {return false;}
		//
		virtual void OnFocusChanged(Widget* sender) {}
		//выделение изменилось, явно или неявно
		//virtual bool OnChangedSelection(Widget* sender, Object* item) {return false;}
		virtual bool OnDrag(Widget* sender, const MouseMove& mMove) {return false;}

		virtual bool OnMouseDown(Widget* sender, const MouseClick& mClick) {return false;}
		virtual bool OnMouseMove(Widget* sender, const MouseMove& mMove) {return false;}
	};
	
	typedef lsl::Container<Event*> EventList;
private:
	Manager* _manager;
	bool _disposed;
	Graphics _graphics;
	Flags _flags;
	int _topmostLevel;
	bool _modal;
	bool _visible;
	bool _enabled;
	EventList _events;
	int _tag;
	Object* _data;

	Widget* _parent;
	Children _children;

	Anchor _anchor;
	Anchor _align;
	mutable D3DXVECTOR2 _anchorVP;
	mutable D3DXVECTOR2 _pos;
	D3DXVECTOR2 _scale;
	float _rot;	

	bool _coord3d;
	D3DXVECTOR3 _pos3d;

	mutable D3DXMATRIX _matrix[cMatrixChangeEnd];
	mutable MatrixChanges _matrixChanges;
	
	D3DXVECTOR2 _size;
	mutable AABB2 _localAABB;
	mutable AABB2 _worldAABB;
	mutable AABB2 _childAABB;
	mutable AABB2 _localChildAABB;
	mutable AABB2 _worldChildAABB;
	mutable AABBChanges _aabbChanges;

	mutable int _alignChanged;	
	bool _isMouseDown;
	bool _isMouseOver;
	bool _isMouseEnter;
	bool _focused;

	void DoRemoveChild(Widget* child);

	void BuildMatrix(MatrixChange change) const;

	void BuildLocalAABB() const;
	void BuildWorldAABB() const;
	void BuildChildAABB() const;
	void BuildLocalChildAABB() const;
	void BuildWorldChildAABB() const;
	//изменение aabb, в зависимости от структуры и трансформации, аргумент change описывает оба случая
	void AABBChanged(StructChange change = scLocal);

	bool ApplyMouseEnter(bool wasReset);
	
	//выравнивание
	//условия выравнивания: локальные координаты, локальный AABB, родительский AABB
	//корректировка только локальных координат согласно выравниванию
	void ApplyAlign() const;
	//условия для выравнивания изменились
	void AlignChanged();
	//
	bool IsAligned() const;	
protected:
	//Уведомления об изменениях
	//Локальные координаты
	virtual void TransformChanged();
	//Мировые координаты
	virtual void WorldTransformChanged();
	//Только структура, под которой можно понимать внешнее состояние которое влияет на локальный AABB - локальная, мировая, дочерняя
	virtual void StructureChanged(StructChange change = scLocal);
	//
	virtual void FlagChanged(Flag flag, bool value);

	//События от мыши
	//return true если оно было обработано, false в ином случае
	//дочерние виджеты полностью перехватывают сообщения если в его зоне и обработали
	//виджету можно ввести флаг чтобы допустим он всегда перехватывал сообшения если он в его зоне
	virtual bool OnMouseClick(const MouseClick& mClick);
	virtual bool OnMouseOver(const MouseMove& mMove);
	virtual bool OnMouseEnter(const MouseMove& mMove);
	virtual void OnMouseLeave(bool wasReset);

	//временной прогресс, для анимации
	virtual void OnProgress(float deltaTime) {}
	
	//События от менеджера
	//return true если событие обрабатывается
	//перемещение мыши, 
	virtual bool OnMouseDown(const MouseClick& mClick);
	virtual bool OnMouseMove(const MouseMove& mMove);

	virtual bool OnClick(const MouseClick& mClick);
	virtual bool OnSelect(Object* item);
	virtual void OnFocusChanged();
	virtual bool OnDrag(const MouseMove& mMove);
	virtual void OnEnabled(bool value) {}

	Widget(Manager* manager);
	virtual ~Widget();
	//Вызвается при особождении manager->ReleaseWidget(). Здесь необходимо очищать все внешнее состояние
	virtual void Dispose();
public:
	bool disposed() const;

	void InsertGraphic(Graphic* graphic, Graphics::const_iterator place);
	void InsertGraphic(Graphic* graphic);
	void RemoveGraphic(Graphics::const_iterator iter);
	void RemoveGraphic(Graphic* graphic);
	void MoveGraphicTo(Graphic* graphic, Graphics::const_iterator pos);
	void ClearGraphics();
	void DeleteAllGraphics();

	D3DXVECTOR2 LocalToWorldCoord(const D3DXVECTOR2& value) const;
	D3DXVECTOR2 WorldToLocalCoord(const D3DXVECTOR2& value) const;
	//
	D3DXVECTOR2 LocalToWorldNorm(const D3DXVECTOR2& value) const;
	D3DXVECTOR2 WorldToLocalNorm(const D3DXVECTOR2& value) const;

	Manager& GetManager();
	Context& GetContext();

	bool GetFlag(Flag flag) const;
	void SetFlag(Flag flag, bool value);

	int GetTopmostLevel() const;
	void SetTopmostLevel(int value);

	bool GetVisible() const;
	void SetVisible(bool value);

	void ShowModal(bool show);
	bool modal() const;
	
	void RegEvent(Event* value);
	void UnregEvent(Event* value);
	void SetEvent(Event* value);
	void ClearEvents();

	void InsertChild(Widget* child);
	void RemoveChild(Widget* child);
	void ClearChildren();
	void DeleteAllChildren();

	bool IsMouseDown() const;
	bool IsMouseOver() const;
	bool IsMouseEnter() const;

	void SetFocused(bool value, bool wasReset = false);
	bool IsFocused() const;

	void Press();

	const Graphics& GetGraphics() const;

	Widget* GetParent();
	void SetParent(Widget* value);
	const Children& GetChildren() const;

	Anchor GetAnchor() const;
	void SetAnchor(Anchor value);

	Anchor GetAlign() const;
	void SetAlign(Anchor value);

	bool GetEnabled() const;
	void SetEnabled(bool value);

	D3DXVECTOR2 GetAlignPos() const;

	const D3DXVECTOR2& GetPos() const;
	void SetPos(const D3DXVECTOR2& value);
	void SetPos(float x, float y);

	const D3DXVECTOR2& GetScale() const;
	void SetScale(const D3DXVECTOR2& value);
	void SetScale(float x, float y);

	float GetRot() const;
	void SetRot(float value);

	bool GetCoord3d() const;
	void SetCoord3d(bool value);

	const D3DXVECTOR3& GetPos3d() const;
	void SetPos3d(const D3DXVECTOR3& value);

	D3DXVECTOR2 GetWorldPos() const;
	void SetWorldPos(const D3DXVECTOR2& value);

	const D3DXMATRIX& GetMat() const;
	const D3DXMATRIX& GetWorldMat() const;
	const D3DXMATRIX& GetInvMat() const;
	const D3DXMATRIX& GetInvWorldMat() const;

	//размер относительно центра localAABB
	const D3DXVECTOR2& GetSize() const;
	void SetSize(const D3DXVECTOR2& value);
	void SetSize(float szX, float szY);

	const AABB2& GetLocalAABB(bool includeChild) const;
	const AABB2& GetWorldAABB(bool includeChild) const;
	const AABB2& GetChildAABB() const;

	int GetTag() const;
	void SetTag(int value);

	//внешние данные, связанные с виджетом
	Object* GetData();
	void SetData(Object* value);

	static D3DXVECTOR2 GetAlignPos(const D3DXVECTOR2& size, Anchor align);
};

class Dummy: public Widget
{
	friend Manager;
	typedef Widget _MyBase;
protected:
	Dummy(Manager* manager);
};

//контейнер Widget-ов. Вспомогательный класс, добавляет вложенный dummy выступающий в роли контейнера
class WidgetCont: public Dummy
{
	friend Manager;
	typedef Dummy _MyBase;
private:
	Dummy* _cont;
protected:
	WidgetCont(Manager* manager);
	virtual ~WidgetCont();
public:
	Dummy* GetCont();
};

class PlaneFon: public Widget
{
	friend Manager;
	typedef Widget _MyBase;
private:
	Plane* _plane;
protected:
	PlaneFon(Manager* manager);
	virtual ~PlaneFon();

	virtual void StructureChanged(StructChange change = scLocal);
public:
	Material& GetMaterial();
	void SetMaterial(Material* value);
};

class Label: public Widget
{
	friend Manager;
	typedef Widget _MyBase;
private:
	graph::TextFont* _font;
	Material* _material;
	Text::HorAlign _horAlign;
	Text::VertAlign _vertAlign;
	bool _wordWrap;
	float _vScroll;

	BaseText* _baseText;
	Text* _text;
	TextW* _textW;	

	void CreateText();
	void CreateTextW();
	void DeleteText();
	void ApplyText();
protected:
	Label(Manager* manager);
	virtual ~Label();

	virtual void StructureChanged(StructChange change = scLocal);	
public:
	AABB2 GetTextAABB();
	void AdjustSizeByText();

	graph::TextFont* GetFont();
	void SetFont(graph::TextFont* value);

	Material& GetMaterial();

	std::string GetText() const;
	void SetText(const std::string& value);

	std::wstring GetTextW() const;
	void SetTextW(const std::wstring& value);

	Text::HorAlign GetHorAlign() const;
	void SetHorAlign(Text::HorAlign value);

	Text::VertAlign GetVertAlign() const;
	void SetVertAlign(Text::VertAlign value);

	bool GetWordWrap() const;
	void SetWordWrap(bool value);

	float GetVScroll() const;
	void SetVScroll(float value);
};

class Button: public Widget
{
	friend Manager;
	typedef Widget _MyBase;
public:
	enum Style {bsSimple, bsSelAnim};
private:
	Style _style;
	D3DXVECTOR2 _selSize;
	bool _selected;

	Plane* _fon;
	Plane* _sel;
	Text* _text;

	Material* _fonMaterial;
	Material* _selMaterial;
	Material* _textMaterial;
	Material* _textSelMaterial;
	bool _createFon;
	bool _createSel;
	bool _createText;
	bool _createTextSel;

	bool _selection;
	float _fadeIn;
	float _fadeOut;

	void ApplySelection(bool instant);
	void UpdateSelection(bool instant);
protected:
	Button(Manager* manager);
	virtual ~Button();	

	virtual void StructureChanged(StructChange change = scLocal);
	virtual void OnProgress(float deltaTime);

	virtual bool OnMouseEnter(const MouseMove& mMove);
	virtual void OnMouseLeave(bool wasReset);
public:
	Material* GetOrCreateFon();
	Material* GetFon();
	void SetFon(Material* value);

	Material* GetOrCreateSel();
	Material* GetSel();
	void SetSel(Material* value);

	Material* GetOrCreateTextMaterial();
	Material* GetTextMaterial();
	void SetTextMaterial(Material* value);

	Material* GetOrCreateTextSelMaterial();
	Material* GetTextSelMaterial();
	void SetTextSelMaterial(Material* value);

	graph::TextFont* GetFont();
	void SetFont(graph::TextFont* value);

	const std::string& GetText() const;
	void SetText(const std::string& value);

	const D3DXVECTOR2& GetSelSize() const;
	void SetSelSize(const D3DXVECTOR2& value);

	Style GetStyle() const;
	void SetStyle(Style value);

	void Select(bool value, bool instant);
	bool IsSelected() const;
};

class StepperBox: public Widget
{
	friend Manager;
private:
	class ChildEvent: public Event
	{
	private:
		StepperBox* _owner;
	public:
		ChildEvent(StepperBox* owner);

		virtual bool OnClick(Widget* sender, const MouseClick& mClick);
	};
private:
	StringList _items;
	int _selIndex;
	bool _itemsLoop;
	ChildEvent* _childEvent;

	Button* _left;
	Button* _right;
	Plane* _planeBg;
	Text* _textField;
protected:
	StepperBox(Manager* manager);
	virtual ~StepperBox();

	void ApplySelection();

	virtual void OnEnabled(bool value);
	virtual void StructureChanged(StructChange change = scLocal);
public:
	void Invalidate();

	Button* GetLeft();
	Button* GetRight();

	Material* GetOrCreateBg();
	Material* GetBg();
	void SetBg(Material* value);

	Material* GetOrCreateArrow();
	Material* GetArrow();
	void SetArrow(Material* value);

	Material* GetOrCreateArrowSel();
	Material* GetArrowSel();
	void SetArrowSel(Material* value);

	Material* GetOrCreateText();
	Material* GetText();
	void SetText(Material* value);

	graph::TextFont* GetFont();
	void SetFont(graph::TextFont* value);

	const StringList& GetItems() const;
	void SetItems(const StringList& value);

	int GetSelIndex() const;
	void SetSelIndex(int index);

	bool GetItemsLoop() const;
	void SetItemsLoop(bool value);
};

class DropBox: public Widget
{
	friend Manager;
	typedef Widget _MyBase;
private:
	typedef lsl::List<Text*> TextItems;

	class ItemsEvent: public Event
	{
	private:
		DropBox* _dropBox;
	public:
		ItemsEvent(DropBox* dropBox);

		virtual bool OnMouseOver(Widget* sender, const MouseMove& mMove);
		virtual bool OnMouseDown(Widget* sender, const MouseClick& mClick);
	};
private:
	StringList _items;
	ItemsEvent* _itemsEvent;

	Plane* _fon;
	Plane* _button;
	int _selInd;
	Text* _selItem;
	
	PlaneFon* _itemsFon;
	TextItems _textItems;

	Material* _fonMaterial;
	Material* _textMaterial;
	Material* _selMaterial;

	bool _showItems;

	void ShowItems(bool value);
	void SelectItem(const std::string& item);

	Text* FindSelTextItem(const D3DXVECTOR2& point, bool deselect);
protected:
	DropBox(Manager* manager);
	virtual ~DropBox();

	virtual void StructureChanged(StructChange change = scLocal);

	virtual bool OnClick(const MouseClick& mClick);	
public:
	const StringList& GetItems() const;
	void SetItems(const StringList& value);

	int GetSelInd() const;
	void SetSelInd(int value);

	const std::string& GetSelItem() const;
	void SetSelItem(const std::string& value);

	graph::TextFont* GetFont();
	void SetFont(graph::TextFont* value);

	Material& GetFonMaterial();
	Material& GetButMaterial();
	Material& GetTextMaterial();
	Material& GetSelMaterial();
};

class TrackBar: public Widget
{
	friend Manager;
	typedef Widget _MyBase;
private:
	Plane* _bar;
	Plane* _button;

	bool _grag;
	float _dragOff;

	float ComputeBarPos(const D3DXVECTOR2& point) const;
protected:
	TrackBar(Manager* manager);
	virtual ~TrackBar();

	virtual void StructureChanged(StructChange change = scLocal);

	virtual bool OnMouseDown(const MouseClick& mClick);
	virtual bool OnMouseMove(const MouseMove& mMove);
public:
	float GetBarPos() const;
	void SetBarPos(float value);

	Material& GetBarMaterial();
	Material& GetButMaterial();
};

class ViewPort3d: public Widget
{
	friend Manager;
	typedef Widget _MyBase;
public:
	enum Style {msStatic, msAnim, msMouseOverAnim, msViewer};
private:
	View3d* _view3d;
	Style _style;
	D3DXQUATERNION _rot3dSpeed;
	bool _setProgress;

	void AnimProgress(float deltaTime);
	void SetProgress(bool enable);
protected:
	ViewPort3d(Manager* manager);
	virtual ~ViewPort3d();

	virtual void StructureChanged(StructChange change = scLocal);

	virtual bool OnMouseOver(const MouseMove& mMove);
	virtual void OnProgress(float deltaTime);
public:
	Graphic3d* GetBox();

	const D3DXQUATERNION& GetRot3dSpeed() const;
	void SetRot3dSpeed(const D3DXQUATERNION& value);

	Style GetStyle() const;
	void SetStyle(Style value);

	//выравнивание по центру
	bool GetAlign() const;
	void SetAlign(bool value);
};

class ScrollBox: public Widget
{
	friend Manager;
	typedef Widget _MyBase;
private:
	enum ScrollDir {sdLeft, sdDown, sdRight, sdUp, cScrollDirEnd};

	class MyEvent: public Event
	{
	private:
		ScrollBox* _scrollBox;
	public:
		MyEvent(ScrollBox* scrollBox);

		virtual void OnStructureChanged(Widget* sender, StructChange change);
		virtual bool OnClick(Widget* sender, const MouseClick& mClick);
	};
public:
	enum Option {soVert, soHor, cOptionEnd};
	typedef lsl::Bitset<cOptionEnd> Options;
private:
	MyEvent* _myEvent;
	Options _options;
	Dummy* _clip;
	Dummy* _box;

	D3DXVECTOR2 _arrowSize;
	D3DXVECTOR2 _arrowSpace;
	D3DXVECTOR2 _arrowScrollStep;
	Material* _arrowMat;
	Material* _arrowSelMat;
	Button* _scrollBut[cScrollDirEnd];

	Button* NewArrow();
	void FreeArrows();
	void ApplyOptions();
	void AdjustLayout();
	void ApplyScroll();
protected:
	ScrollBox(Manager* manager);
	virtual ~ScrollBox();

	virtual void StructureChanged(StructChange change = scLocal);
public:
	//область отсечения
	Dummy* GetClip();
	//box с элементами
	Widget* GetBox();

	bool GetOption(Option option) const;
	void SetOption(Option option, bool value);

	const D3DXVECTOR2& GetArrowSize() const;
	void SetArrowSize(const D3DXVECTOR2& value);

	const D3DXVECTOR2& GetArrowSpace() const;
	void SetArrowSpace(const D3DXVECTOR2& value);

	const D3DXVECTOR2& GetArrowScrollStep() const;
	void SetArrowScrollStep(const D3DXVECTOR2& value);

	D3DXVECTOR2 GetMaxScroll() const;

	D3DXVECTOR2 GetScroll();
	void SetScroll(const D3DXVECTOR2& value);

	Material& GetArrowMaterial();
	Material& GetArrowSelMaterial();
};

class ListBox: public Widget
{
	friend Manager;
	typedef Widget _MyBase;
public:
	class Item: public Object
	{
		friend ListBox;
	private:
		ListBox* _listBox;
		Widget* _data;
		Graphic* _frame;

		Item(ListBox* listBox);
		virtual ~Item();

		void ApplyFrame();
		void RemoveFrame();
	public:		
		ListBox* GetListBox();

		Widget* GetData();
		void SetData(Widget* value);

		Graphic* GetFrame();
		void SetFrame(Graphic* value);
	};
	typedef lsl::List<Item*> Items;

	class MyEvent: public Event
	{
	private:
		ListBox* _listBox;
	public:
		MyEvent(ListBox* listBox);

		virtual bool OnClick(Widget* sender, const MouseClick& mClick);
	};
private:
	Items _items;
	MyEvent* _event;

	D3DXVECTOR2 _itemSize;
	D3DXVECTOR2 _itemSpace;	
	Item* _selItem;

	Plane* _fon;
	ScrollBox* _scrollBox;
	Material* _frameMaterial;

	Graphic* NewFrame();
	void FreeItemFrame(Item* item);
	void FreeItem(Item* item);

	void AlignItems();
protected:
	ListBox(Manager* manager);
	virtual ~ListBox();

	virtual void StructureChanged(StructChange change = scLocal);
public:
	void AddItem(Widget* data);
	void DelItem(Items::const_iterator iter);
	void DelItem(Items::const_iterator sIter, Items::const_iterator eIter);
	void DelItem(Item* item);
	void ClearItems();

	Item* PickItem(const D3DXVECTOR2& worldCoord);
	bool PickItems(const D3DXVECTOR2& worldCoord);
	Item* FindItemByData(Widget* data);

	Item* GetSelItem();
	void SelectItem(Item* item);
	void AlignSizeByItems(const D3DXVECTOR2& size);

	const Items& GetItems() const;

	const D3DXVECTOR2& GetItemSize() const;
	void SetItemSize(const D3DXVECTOR2& value);

	const D3DXVECTOR2& GetItemSpace() const;
	void SetItemSpace(const D3DXVECTOR2& value);

	Material& GetOrCreateFon();
	void FreeFon();

	Material& GetOrCreateFrame();
	void FreeFrame();

	Material& GetArrowMaterial();
	Material& GetArrowSelMaterial();

	const D3DXVECTOR2& GetArrowSize() const;
	void SetArrowSize(const D3DXVECTOR2& value);

	D3DXVECTOR2 GetScroll() const;
	void SetScroll(const D3DXVECTOR2& value);

	D3DXVECTOR2 GetMaxScroll() const;

	D3DXVECTOR2 GetItemPlaceSize() const;
	D3DXVECTOR2 GetScrollSpace() const;
};

class ProgressBar: public Widget
{
	friend Manager;
	typedef Widget _MyBase;
public:
	enum Style {psHorizontal, psVertical};
private:
	Style _style;
	float _progress;
	bool _backEnabled;

	Plane* _front;
	Plane* _back;
protected:
	ProgressBar(Manager* manager);
	virtual ~ProgressBar();

	virtual void StructureChanged(StructChange change = scLocal);
public:
	Style GetStyle() const;
	void SetStyle(Style value);

	Material& GetFront();
	void SetFront(Material* value);

	Material& GetBack();
	void SetBack(Material* value);

	float GetProgress() const;
	void SetProgress(float value);

	bool GetBackEnaled() const;
	void SetBackEnabled(bool value);
};

class StreakBar: public Widget
{
	friend Manager;
	typedef Widget _MyBase;
private:
	typedef lsl::List<Plane*> Streaks;
private:
	Streaks _streaks;
	Material* _streak;
	bool _createStreak;

	unsigned _streakMax;
	unsigned _streakCount;

	void UpdateStreaks();
	void ClearStreaks();
protected:
	StreakBar(Manager* manager);
	virtual ~StreakBar();

	virtual void StructureChanged(StructChange change = scLocal);
public:
	Material* GetOrCreateStreak();
	Material* GetStreak();
	void SetStreak(Material* value);

	unsigned GetStreakMax();
	void SetStreakMax(unsigned value);

	unsigned GetStreakCount();
	void SetStreakCount(unsigned value);
};

class ChargeBar: public Widget
{
	friend Manager;
	typedef Widget _MyBase;
private:
	class MyEvent: public Event
	{
	private:
		ChargeBar* _bar;
	public:
		MyEvent(ChargeBar* bar);

		virtual bool OnClick(Widget* sender, const MouseClick& mClick);
	};
private:
	MyEvent* _myEvent;

	Plane* _frame;
	Button* _up;
	StreakBar* _bar;

	D3DXVECTOR2 _space;

	void AdjustLayout();
protected:
	ChargeBar(Manager* manager);
	virtual ~ChargeBar();

	virtual void StructureChanged(StructChange change = scLocal);
public:
	Material& GetStreak();
	Material& GetFrame();
	Material& GetUp();
	Material& GetUpSel();

	Button* GetUpButton();

	const D3DXVECTOR2& GetSpace() const;
	void SetSpace(const D3DXVECTOR2& value);

	unsigned GetChargeMax();
	void SetChargeMax(unsigned value);

	unsigned GetCharge();
	void SetCharge(unsigned value);
};

class VolumeBar: public Widget
{
	friend Manager;
private:
	class ChildEvent: public Event
	{
	private:
		VolumeBar* _owner;
	public:
		ChildEvent(VolumeBar* owner);

		virtual bool OnClick(Widget* sender, const MouseClick& mClick);
	};
private:	
	float _volume;
	int _stepCount;
	ChildEvent* _childEvent;

	Button* _left;
	Button* _right;
	Plane* _planeBg;
	ProgressBar* _bar;
protected:
	VolumeBar(Manager* manager);
	virtual ~VolumeBar();

	void ApplyVolume();

	virtual void StructureChanged(StructChange change = scLocal);
public:
	void Invalidate();

	Button* GetLeft();
	Button* GetRight();

	Material* GetOrCreateBg();
	Material* GetBg();
	void SetBg(Material* value);

	Material* GetOrCreateArrow();
	Material* GetArrow();
	void SetArrow(Material* value);

	Material* GetOrCreateArrowSel();
	Material* GetArrowSel();
	void SetArrowSel(Material* value);

	Material& GetFront();
	void SetFront(Material* value);

	Material& GetBack();
	void SetBack(Material* value);

	float GetVolume() const;
	void SetVolume(float value);

	int GetStep() const;
	void SetStep(int value);

	int GetStepCount() const;
	void SetStepCount(int value);
};

class ColorBox: public Widget
{
	friend Manager;
	typedef Widget _MyBase;
private:
	Plane* _frame;
	Plane* _box;
	Plane* _check;
	Plane* _sel;

	D3DXCOLOR _color;
	bool _select;

	D3DXVECTOR2 WinToLocal(const D3DXVECTOR2& vec) const;

	void ApplyColor();
	void ApplySelect();
	void AdjustLayout();
protected:
	ColorBox(Manager* manager);
	virtual ~ColorBox();

	virtual void StructureChanged(StructChange change = scLocal);
public:
	Material& GetFrame();
	void SetFrame(Material* value);

	Material& GetBox();
	Material& GetCheck();
	
	const D3DXCOLOR& GetColor() const;
	void SetColor(const D3DXCOLOR& value);

	bool GetSelect() const;
	void SetSelect(bool value);

	D3DXVECTOR2 GetColorBoxSize() const;
	AABB2 GetBoxAABB() const;
	AABB2 GetCheckAABB() const;
};

class ColorList: public Widget
{
	friend Manager;
	typedef Widget _MyBase;
private:
	struct MyCol
	{
		MyCol(): box(0) {}

		const D3DXCOLOR& GetVal() const {return box->GetColor();}
		void SetVal(const D3DXCOLOR& value) {box->SetColor(value);}

		operator bool() {return box != 0;}
		bool operator==(const MyCol& val) const {return box == val.box;}
		bool operator!=(const MyCol& val) const {return box != val.box;}

		ColorBox* box;
	};	
public:
	typedef lsl::List<MyCol> Colors;

	class MyEvent: public Event
	{
	private:
		ColorList* _list;
	public:
		MyEvent(ColorList* list);

		virtual bool OnClick(Widget* sender, const MouseClick& mClick);
	};
private:
	MyEvent* _myEvent;
	Colors _colors;
	MyCol _select;
	D3DXVECTOR2 _space;

	Material* _frame;
	Material* _box;
	Material* _check;

	void ApplyChanges();
protected:
	ColorList(Manager* manager);
	virtual ~ColorList();

	virtual void StructureChanged(StructChange change = scLocal);
	void SelectColor(const MyCol& value);
public:
	Material& GetFrame();
	Material& GetBox();
	Material& GetCheck();

	void InsertColor(const D3DXCOLOR& value);
	void RemoveColor(Colors::const_iterator iter);
	void RemoveColor(const D3DXCOLOR& value);
	void ClearColors();
	Colors::const_iterator FindColor(const D3DXCOLOR& value) const;

	const D3DXCOLOR* GetSelColor();
	void SelectColor(const D3DXCOLOR* value);

	const Colors& GetColors() const;

	const D3DXVECTOR2& GetSpace() const;
	void SetSpace(const D3DXVECTOR2& value);
};

class Grid: public Widget
{
	friend Manager;
	typedef Widget _MyBase;
public:
	enum Style {gsHorizontal, gsVertical};
private:
	Style _style;
	D3DXVECTOR2 _cellSize;
	int _maxCellsOnLine;
	bool _hideInvisible;
protected:
	Grid(Manager* manager);
	virtual ~Grid();
public:
	void Reposition();

	Style style() const;
	void style(Style value);

	D3DXVECTOR2 cellSize() const;
	void cellSize(const D3DXVECTOR2& value);

	int maxCellsOnLine() const;
	void maxCellsOnLine(int value);

	bool hideInvisible() const;
	void hideInvisible(bool value);
};

//главный класс, фасад, фабрика элементов Widget
class Manager
{
	friend Widget;
public:
	typedef lsl::Container<Widget*> WidgetList;
	typedef lsl::List<Widget*> Widgets;
private:
	graph::Engine* _engine;
	bool _invertY;
	graph::Camera* _camera3d;
	Context* _context;
	WidgetList _widgetList;
	Widgets _topmostWidgets;
	Widgets _modalWidgets;
	Dummy* _root;	

	MouseClick _mClick;
	MouseMove _mMove;
	Widget* _clipWidget;

	bool _safeMode;
	WidgetList _safeList;	

	void BeginSafeMode();
	void EndSafeMode();
	bool IsSafeMode() const;

	void InsertWidget(Widget* widget);

	void InsertTopmostWidget(Widget* value);
	void RemoveTopmostWidget(Widget* value);
	void PushModalWidget(Widget* value);
	void PopModalWidget(Widget* value);

	void DoDrawWidget(Widget* widget);
	void DrawWidget(Widget* widget);

	bool OnWidgetMouseDown(Widget* widget, const MouseClick& mClick);
	bool OnWidgetMouseMove(Widget* widget, const MouseMove& mMove);
public:
	Manager(graph::Engine* engine);
	virtual ~Manager();

	void Draw();

	bool GetInvertY() const;
	void SetInvertY(bool value);

	graph::Camera* GetCamera3d();
	void SetCamera3d(graph::Camera* value);

	D3DXVECTOR2 ScreenToView(const Point& point);
	D3DXVECTOR2 WorldToView( const D3DXVECTOR3& coord);

	MouseClick GetMouseClick(Widget* widget) const;
	MouseMove GetMouseMove(Widget* widget) const;

	bool OnMouseClickEvent(MouseKey key, KeyState state, const Point& coord, bool shift, bool ctrl);
	bool OnMouseMoveEvent(const Point& coord, bool shift, bool ctrl);

	Dummy* CreateDummy();
	WidgetCont* CreateWidgetCont();
	PlaneFon* CreatePlaneFon();
	Label* CreateLabel();
	Button* CreateButton();
	StepperBox* CreateStepperBox();
	VolumeBar* CreateVolumeBar();
	DropBox* CreateDropBox();
	TrackBar* CreateTrackBar();
	ViewPort3d* CreateViewPort3d();
	ScrollBox* CreateScrollBox();
	ListBox* CreateListBox();
	ProgressBar* CreateProgressBar();
	StreakBar* CreateStreakBar();
	ChargeBar* CreateChargeBar();
	ColorBox* CreateColorBox();
	ColorList* CreateColorList();
	Grid* CreateGrid();

	void ReleaseWidget(Widget* widget);

	Context& GetContext();
	Dummy* GetRoot();

	D3DXVECTOR2 GetVPSize();
	D3DXVECTOR2 GetMousePos();
};

}

}