#include "stdafx.h"

#include "graph\\Shader.h"

namespace r3d
{

namespace graph
{

Shader::Shader(): _data(0), _createData(false), _tech(""), _macro(0), _effPool(0), _curTech(""), _curMacro(0), _cntPass(0), _numPass(0), _apply(0), _applyTech(0), _applyPass(0)
{
	_include = new D3DXInclude(this);
	_texUser = new TexUser(this);
	_macros = new MacroBlocks(this);

	_defMacro = &_macros->Add(Macros());
	_defMacro->AddRef();

	SetMacro(_defMacro);
}

Shader::~Shader()
{
	Free();

	ClearTextures();

	SetMacro(0);
	SetData(0);

	_defMacro->Release();

	delete _macros;
	delete _texUser;
	delete _include;
}

Shader::D3DXInclude::D3DXInclude(Shader* owner): _owner(owner)
{
}

HRESULT Shader::D3DXInclude::Open(D3DXINCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID * ppData, UINT * pBytes)
{
	std::string path;

	switch (IncludeType)
	{
	case  D3DXINC_LOCAL:
		lsl::ExtractFilePath(path, _owner->GetData()->GetFileName());
		break;

	case D3DXINC_SYSTEM:
		path = "";
		break;

	default:
		LSL_ASSERT(false);
	}

	_buffer.LoadFromFile(path + pFileName);
	*ppData = _buffer.GetData();
	*pBytes = _buffer.GetSize();

	return S_OK;
}

HRESULT Shader::D3DXInclude::Close(LPCVOID pData)
{
	_buffer.Free();

	return S_OK;
}

void Shader::InitMacro(MacroBlock* value)
{
	LSL_ASSERT(value->_effect == 0 && IsInit());

	D3DXMACRO* macrosBuf = 0;
	const Macros& macros = value->GetMacros();
	if (!macros.empty())
	{
		macrosBuf = new D3DXMACRO[macros.size() + 1];

		unsigned i = 0;
		for (Macros::const_iterator iter = macros.begin(); iter != macros.end(); ++iter, ++i)
		{
			macrosBuf[i].Name = iter->name.c_str();
			macrosBuf[i].Definition = iter->definition.c_str();
		}
		//Завершающий ноль для массива
		macrosBuf[macros.size()].Name = macrosBuf[macros.size()].Definition = 0;
	}

	if (_macros->Size() > 1 && !_effPool)	
		D3DXCreateEffectPool(&_effPool);

	DWORD flags = 0;
#ifdef _DEBUG
	flags |= D3DXSHADER_DEBUG;
#else
	flags |= D3DXSHADER_PREFER_FLOW_CONTROL;
#endif

	ID3DXEffect* effect;
	ID3DXBuffer* bufCompErr = 0;
	HRESULT hr = D3DXCreateEffect(GetEngine()->GetDriver().GetDevice(), _data->GetData(), _data->GetSize(), macrosBuf, _include, flags, _effPool, &effect, &bufCompErr);
	if (hr != D3D_OK)
	{		
		char* bufCompErrStr = (char*)bufCompErr->GetBufferPointer();
		lsl::appLog.Append("D3DXCreateEffect compile failed.");
		lsl::appLog.Append(bufCompErrStr);

		::MessageBox(0, bufCompErrStr, "", MB_OK);

		throw lsl::Error(bufCompErrStr);
	}

	value->SetEffect(effect);

	if (macrosBuf)
		delete[] macrosBuf;
}

void Shader::FreeMacro(MacroBlock* value)
{
	LSL_ASSERT(value->_effect != 0);

	value->SetEffect(0);
}

void Shader::InsertMacroBlock(MacroBlock* value)
{
	if (IsInit())
		InitMacro(value);
}

void Shader::RemoveMacroBlock(MacroBlock* value)
{
	if (IsInit())
		FreeMacro(value);
}

void Shader::OnInitTex(TexResource* value)
{
	if (!IsInit() || GetMacro() == 0)
		return;

	for (Textures::iterator iter = _textures.begin(); iter != _textures.end(); ++iter)
		if (iter->second == value)
		{
			SetTextureDir(iter->first, iter->second);
			return;
		}
}

void Shader::OnFreeTex(TexResource* value)
{
	if (!IsInit() || GetMacro() == 0)
		return;

	for (Textures::iterator iter = _textures.begin(); iter != _textures.end(); ++iter)
		if (iter->second == value)
		{
			SetTextureDir(iter->first, static_cast<TexResource*>(0));
			return;
		}
}

void Shader::ApplyAllParams()
{
	if (GetCurMacro())
	{
		for (Values::iterator iter = _values.begin(); iter != _values.end(); ++iter)
			SetValueDir(iter->first, iter->second);

		for (Textures::iterator iter = _textures.begin(); iter != _textures.end(); ++iter)
			SetTextureDir(iter->first, iter->second);
	}
}

void Shader::BeginDraw(Engine& engine)
{
	DoBeginDraw(engine);

	if (_applyTech == 0)
	{
		LSL_ASSERT(_curMacro);

		D3DXHANDLE tech;
		HRESULT hr = _curMacro->GetEffect()->FindNextValidTechnique(0, &tech);

		LSL_ASSERT(hr == D3D_OK);

		 D3DXTECHNIQUE_DESC desc;
		_curMacro->GetEffect()->GetTechniqueDesc(tech, &desc);
		ApplyTech(desc.Name, _curMacro);
	}

	LSL_ASSERT(_applyTech > 0);

	ApplyPass(_numPass);
}

bool Shader::EndDraw(Engine& engine, bool nextPass)
{
	UnApplyPass();

	//Выполняем сначала внутрениие проходы эффекта если можно (это делается для каждого внешнего прохода)
	if (nextPass && ++_numPass < _cntPass)
	{
		return false;
	}
	//Затем приступаем к внешним проходам
	else
	{
		_numPass = 0;

		bool next = !DoEndDraw(engine, nextPass);

		return !(nextPass && next);
	}
}

void Shader::DoInit()
{
	for (MacroBlocks::iterator iter = _macros->begin(); iter != _macros->end(); ++iter)
		InitMacro(*iter);

	ApplyAllParams();
}

void Shader::DoFree()
{
	for (MacroBlocks::iterator iter = _macros->begin(); iter != _macros->end(); ++iter)
		FreeMacro(*iter);

	lsl::SafeRelease(_effPool);
}

void Shader::DoUpdate()
{
	//Nothing
}

void Shader::ApplyPass(unsigned numPass)
{
	if (_applyPass++ > 0)
		return;

	HRESULT hr = _curMacro->GetEffect()->BeginPass(numPass);

	LSL_ASSERT(hr == D3D_OK);
}

void Shader::UnApplyPass()
{
	LSL_ASSERT(_applyPass > 0);
	if (--_applyPass > 0)
		return;

	HRESULT hr = _curMacro->GetEffect()->EndPass();

	LSL_ASSERT(hr == D3D_OK);
}

void Shader::ApplyTech(const std::string& tech, MacroBlock* macro)
{
	if (_applyTech > 0)
	{
		++_applyTech;
		return;
	}

	MacroBlock* newMacro = _macro ? _macro : macro;
	if (ReplaceRef(_curMacro, newMacro))
		_curMacro = newMacro;

	_curTech = !_tech.empty() ? _tech : tech;

	if (!_curTech.empty() && _curMacro)
	{
		++_applyTech;

		_curMacro->SetTechnique(_curTech.c_str());

		HRESULT hr = _curMacro->GetEffect()->Begin(&_cntPass, 0);

		LSL_ASSERT(hr == D3D_OK);		
	}
}

void Shader::UnApplyTech()
{
	if (_applyTech == 0)
	{
		lsl::SafeRelease(_curMacro);
		return;
	}
	if (--_applyTech > 0)
		return;

	HRESULT hr = _curMacro->GetEffect()->End();
	LSL_ASSERT(hr == D3D_OK);

	lsl::SafeRelease(_curMacro);
	_curTech = "";
}

Shader::MacroBlock* Shader::GetCurMacro()
{
	return _curMacro ? _curMacro : _macro;
}

void Shader::Apply(graph::Engine& engine, const std::string& tech, MacroBlock* macro)
{
	if (_apply++ > 0)
		return;

	engine.GetContext().PushShader(this);

	ApplyTech(tech, macro);

	DoApply(engine);
}

void Shader::Apply(graph::Engine& engine)
{
	Apply(engine, "", 0);
}

void Shader::UnApply(Engine& engine)
{
	DoUnApply(engine);

	LSL_ASSERT(_apply > 0);
	if (--_apply > 0)
		return;

	UnApplyTech();

	engine.GetContext().PopShader(this);

	LSL_ASSERT(_applyTech == 0);
}

lsl::BinaryResource* Shader::GetData()
{
	return _data;
}

lsl::BinaryResource* Shader::GetOrCreateData()
{
	if (!_data)
	{
		_data = new lsl::BinaryResource();
		_data->AddRef();
		_createData = true;
	}
	return _data;
}

void Shader::SetData(lsl::BinaryResource* value)
{
	if (ReplaceRef(_data, value))
	{
		if (_createData)
		{
			delete _data;
			_createData = false;
		}
		_data = value;
	}
}

D3DXHANDLE Shader::GetParam(const std::string& name)
{
	LSL_ASSERT(IsInit() && GetCurMacro());

	return GetCurMacro()->GetParam(name);
}

void Shader::SetParam(D3DXHANDLE param, const Value& value)
{
	LSL_ASSERT(IsInit() && GetCurMacro());

	return GetCurMacro()->SetParam(param, value);
}

void Shader::SetParam(D3DXHANDLE param, const void* data, unsigned size)
{
	LSL_ASSERT(IsInit() && GetCurMacro());

	return GetCurMacro()->SetParam(param, data, size);
}

void Shader::SetTexParam(D3DXHANDLE param, IDirect3DBaseTexture9* value)
{
	LSL_ASSERT(IsInit() && GetCurMacro());

	return GetCurMacro()->SetTexParam(param, value);
}

void Shader::SetValueDir(const std::string& name, const void* data, unsigned size)
{
	LSL_ASSERT(IsInit() && GetCurMacro());

	GetCurMacro()->SetValue(name, data, size);
}

void Shader::SetValueDir(const std::string& name, const Value& value)
{
	LSL_ASSERT(IsInit() && GetCurMacro());

	GetCurMacro()->SetValue(name, value);
}

bool Shader::GetValue(const std::string& name, Value& outVal) const
{
	Values::const_iterator iter = _values.find(name);
	if (iter != _values.end())
	{
		outVal = iter->second;
		return true;
	}
	else	
		return false;
}

void Shader::SetValue(const std::string& name, const Value& value)
{
	_values[name] = value;
	if (IsInit() && GetCurMacro())
		SetValueDir(name, value);
}

void Shader::SetTextureDir(const std::string& name, IDirect3DBaseTexture9* value)
{
	LSL_ASSERT(IsInit() && GetCurMacro());

	GetCurMacro()->SetTexture(name, value);
}

void Shader::SetTextureDir(const std::string& name, TexResource* value)
{
	SetTextureDir(name, (value && value->IsInit()) ? value->GetTex() : 0);
}

TexResource* Shader::GetTexture(const std::string& name)
{
	Textures::const_iterator iter = _textures.find(name);
	if (iter != _textures.end())	
		return iter->second;
	else	
		return 0;
}

void Shader::SetTexture(const std::string& name, TexResource* value)
{
	Textures::iterator iter = _textures.find(name);

	if (iter == _textures.end())
	{
		iter = _textures.insert(iter, Textures::value_type(name, 0));
	}
	else if (!value)
	{
		iter->second->Release();
		_textures.erase(iter);
		return;
	}

	if (ReplaceRef(iter->second, value))
	{
		if (iter->second)
			iter->second->userList.Remove(_texUser);
		iter->second = value;
		if (iter->second)
		{
			iter->second->userList.Insert(_texUser);

			if (IsInit() && GetCurMacro())
				SetTextureDir(name, value);
		}
	}	
}

void Shader::ClearTextures()
{
	for (Textures::iterator iter = _textures.begin(); iter != _textures.end(); ++iter)
	{
		iter->second->userList.Remove(_texUser);
		iter->second->Release();
	}

	_textures.clear();
}

void Shader::CommitChanges()
{
	LSL_ASSERT(IsInit() && GetCurMacro());

	GetCurMacro()->GetEffect()->CommitChanges();
}

Shader::MacroBlocks& Shader::GetMacros()
{
	return *_macros;
}

const std::string& Shader::GetTech() const
{
	return _tech;
}

void Shader::SetTech(const std::string& value)
{
	_tech = value;
}

Shader::MacroBlock* Shader::GetMacro()
{
	return _macro;
}

Shader::MacroBlock* Shader::GetDefMacro()
{
	return _defMacro;
}

void Shader::SetMacro(MacroBlock* value)
{
	if (ReplaceRef(_macro, value))
	{
		//Применяем все параметры если отсутствовал предыдущий макрос
		bool applyParams = _macro == 0;

		_macro = value;

		if (applyParams)
			ApplyAllParams();
	}
}

}

}