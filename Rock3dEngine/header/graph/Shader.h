#ifndef R3D_GRAPH_SHADER
#define R3D_GRAPH_SHADER

#include "Engine.h"

namespace r3d
{

namespace graph
{

class Shader: public VideoResource, public BaseShader
{
private:
	class D3DXInclude: public ID3DXInclude
	{
	private:
		Shader* _owner;
		lsl::BinaryResource _buffer;
	protected:
		virtual HRESULT STDMETHODCALLTYPE Open(D3DXINCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID * ppData, UINT * pBytes);
		virtual HRESULT STDMETHODCALLTYPE Close(LPCVOID pData);
	public:
		D3DXInclude(Shader* owner);
	};
public:
	typedef lsl::VariantVec Value;
	typedef std::map<std::string, Value> Values;
	typedef std::map<std::string, TexResource*> Textures;

	struct Macro
	{
		std::string name;
		std::string definition;

		Macro() {}
		Macro(const std::string& mName, const std::string& mDefinition): name(mName), definition(mDefinition) {}

		bool operator==(const Macro& value) const
		{
			return name == value.name && definition == value.definition;
		}
	};
	typedef std::list<Macro> Macros;

	class MacroBlock: public lsl::Object
	{
		friend Shader;
		friend lsl::Collection<MacroBlock, void, void, void>;
	private:
		typedef std::map<std::string, D3DXHANDLE> Handles;
	private:
		Macros _macros;
		ID3DXEffect* _effect;
		Handles _handles;

		ID3DXEffect* GetEffect()
		{
			return _effect;
		}

		void SetEffect(ID3DXEffect* value)
		{
			if (_effect != value)
			{
				_handles.clear();
				if (_effect)
					_effect->Release();

				_effect = value;				
			}
		}

		D3DXHANDLE GetParam(const std::string& name)
		{
			Handles::iterator iter = _handles.find(name);
			if (iter == _handles.end())
				iter = _handles.insert(iter, Handles::value_type(name, _effect->GetParameterByName(NULL, name.c_str())));

			return iter->second;
		}

		void SetParam(D3DXHANDLE param, const void* data, unsigned size)
		{
			if (param)
				_effect->SetValue(param, data, size);
		}

		void SetParam(D3DXHANDLE param, const Value& value)
		{
			if (param)
				_effect->SetValue(param, value.GetData(), value.GetSize());
		}

		void SetTexParam(D3DXHANDLE param, IDirect3DBaseTexture9* texture)
		{
			if (param)
				_effect->SetTexture(param, texture);
		}		

		D3DXHANDLE GetTech(const std::string& name)
		{
			Handles::iterator iter = _handles.find(name);
			if (iter == _handles.end())
				iter = _handles.insert(iter, Handles::value_type(name, _effect->GetTechniqueByName(name.c_str())));

			return iter->second;
		}
	protected:
		MacroBlock() {}
		MacroBlock(const Macros& macros): _macros(macros), _effect(0) {}

		const Macros& GetMacros() const
		{
			return _macros;
		}

		bool operator==(const MacroBlock& value) const
		{
			return _effect == value._effect;
		}
		bool operator==(const Macros& value) const
		{
			return _macros == value;
		}

		void SetValue(const std::string& name, const void* data, unsigned size)
		{
			SetParam(GetParam(name), data, size);
		}

		void SetValue(const std::string& name, const Value& value)
		{
			SetParam(GetParam(name), value);
		}

		void SetTexture(const std::string& name, IDirect3DBaseTexture9* texture)
		{
			SetTexParam(GetParam(name), texture);
		}

		void SetTechnique(const std::string& name)
		{
			D3DXHANDLE tech = GetTech(name);
			if (tech)
				_effect->SetTechnique(tech);
		}
	};

	class MacroBlocks: public lsl::Collection<MacroBlock, void, void, void>
	{
	private:
		typedef lsl::Collection<MacroBlock, void, void, void> _MyBase;
	private:
		Shader* _owner;
	protected:
		virtual void InsertItem(const Value& value)
		{
			_MyBase::InsertItem(value);

			_owner->InsertMacroBlock(value);
		}
		virtual void RemoveItem(const Value& value)
		{
			_MyBase::RemoveItem(value);

			_owner->RemoveMacroBlock(value);
		}

		MacroBlock* Find(const Macros& macros)
		{
			for (iterator iter = begin(); iter != end(); ++iter)
				if ((**iter) == macros)
					return *iter;

			return 0;
		}
	public:
		MacroBlocks(Shader* owner): _owner(owner) {}

		MacroBlock& Add(const Macros& macros)
		{
			MacroBlock* res = Find(macros);

			if (!res)
			{
				Engine* engine = _owner->GetEngine();
				bool reload = Size() == 1 && engine;
				if (reload)
					_owner->Free();
				
				res = &_MyBase::AddItem(new MacroBlock(macros));
				
				if (reload)
					_owner->Init(*engine);
			}

			return *res;
		}
	};

	class TexUser: public lsl::ResourceUser
	{
	private:
		Shader* _owner;
	protected:
		virtual void OnInitResource(Resource* sender)
		{
			_owner->OnInitTex(lsl::StaticCast<TexResource*>(sender));
		}
		virtual void OnFreeResource(Resource* sender)
		{
			_owner->OnFreeTex(lsl::StaticCast<TexResource*>(sender));
		}
	public:
		TexUser(Shader* owner): _owner(owner) {}
	};
private:
	//Данные шейдера
	lsl::BinaryResource* _data;	
	bool _createData;
	//Параметры
	Values _values;
	//Текстуры
	Textures _textures;
	//Макрос по умолчанию (дефаултный эффект без макроопределений). Наименьший приоритет.
	MacroBlock* _defMacro;
	//Список эффектов с макроопределениями
	MacroBlocks* _macros;

	//Заданная извне техника (наивысшый приоритет)
	std::string _tech;
	//Заданный извне макрос (наивысшый приоритет)
	MacroBlock* _macro;

	//Реализация включения
	D3DXInclude* _include;
	//Обновление внешних текстур
	TexUser* _texUser;
	//Текущая техника прохода, задается только при применении прохода и может быть временной, заданной через параметр
	std::string _curTech;
	//Текущий макрос прохода, задается только при применении прохода и может быть временным, заданной через параметр
	MacroBlock* _curMacro;
	//Общее число внтуренних проходов текущей техники
	unsigned _cntPass;
	//Текущий внутренний проход
	unsigned _numPass;	
	//Применение эффекта
	unsigned _apply;
	//Применение техники
	unsigned _applyTech;	
	//Применение внутреннего прохода
	unsigned _applyPass;
	//Пул эффектов
	ID3DXEffectPool* _effPool;

	void InitMacro(MacroBlock* value);
	void FreeMacro(MacroBlock* value);

	void InsertMacroBlock(MacroBlock* value);
	void RemoveMacroBlock(MacroBlock* value);

	void OnInitTex(TexResource* value);
	void OnFreeTex(TexResource* value);

	void ApplyAllParams();

	//Пара BeginDraw/EndDraw вызывается каждый проход при отрисовке
	virtual void BeginDraw(Engine& engine);
	//Если nextPass равен false то следующий вызов BeginDraw/EndDraw будет новой транзакцией
	virtual bool EndDraw(Engine& engine, bool nextPass);
protected:
	virtual void DoInit();
	virtual void DoFree();
	virtual void DoUpdate();

	//Применить numPass проход
	void ApplyPass(unsigned numPass);
	void UnApplyPass();
	//Применить tech технику. Параметры соответствующие заданным аргументам могут быть перегружены если они имеют нулевое значение
	void ApplyTech(const std::string& tech, MacroBlock* macro);
	void UnApplyTech();

	virtual void DoBeginDraw(Engine& engine) {}
	virtual bool DoEndDraw(Engine& engine, bool nextPass) {return true;}

	virtual void DoApply(graph::Engine& engine) {}
	virtual void DoUnApply(Engine& engine) {}

	MacroBlock* GetCurMacro();
public:
	Shader();
	virtual ~Shader();

	//Установить шейдер
	void Apply(graph::Engine& engine, const std::string& tech, MacroBlock* macro);
	void Apply(graph::Engine& engine);
	void UnApply(Engine& engine);

	lsl::BinaryResource* GetData();
	lsl::BinaryResource* GetOrCreateData();
	void SetData(lsl::BinaryResource* value);

	D3DXHANDLE GetParam(const std::string& name);
	void SetParam(D3DXHANDLE param, const Value& value);
	void SetParam(D3DXHANDLE param, const void* data, unsigned size);
	void SetTexParam(D3DXHANDLE param, IDirect3DBaseTexture9* value);

	void SetValueDir(const std::string& name, const void* data, unsigned size);
	void SetValueDir(const std::string& name, const Value& value);
	bool GetValue(const std::string& name, Value& outVal) const;
	template<class _Type> bool GetValue(const std::string& name, _Type& outVal) const;
	void SetValue(const std::string& name, const Value& value);
	//
	void SetTextureDir(const std::string& name, IDirect3DBaseTexture9* value);
	void SetTextureDir(const std::string& name, TexResource* value);
	TexResource* GetTexture(const std::string& name);
	void SetTexture(const std::string& name, TexResource* value);
	void ClearTextures();

	void CommitChanges();

	MacroBlocks& GetMacros();

	const std::string& GetTech() const;
	void SetTech(const std::string& value);

	MacroBlock* GetDefMacro();
	MacroBlock* GetMacro();
	void SetMacro(MacroBlock* value);
};




template<class _Type> bool Shader::GetValue(const std::string& name, _Type& outVal) const
{
	Value tmp;
	if (GetValue(name, tmp))
	{
		tmp.Convert(&tmp);
		return true;
	}
	else
		return false;
}

}

}

#endif