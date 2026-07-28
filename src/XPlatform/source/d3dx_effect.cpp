/*
 * The D3DX effects framework, over vkd3d-shader.
 *
 * This is the last large piece of D3DX the game needs. It compiles the 23 .fx
 * files at runtime through D3DXCreateEffect, once per macro permutation --
 * Shader::MacroBlock drives DIFF_TEX / NORMAL / TANGENT_SPACE and friends -- so
 * a compiler has to be in the process. tools/setup-vkd3d-macos.sh provides one,
 * and tools/shader-pipeline/vkd3d_hlsl_check.py records that all 48 entry
 * points compile.
 *
 * What this implements is only what the engine calls: roughly sixteen methods
 * out of ID3DXEffect's eighty-odd. The rest return E_NOTIMPL rather than
 * pretending, so an unimplemented path stops somewhere named.
 *
 * How it works, and why not more:
 *
 *   The .fx files are HLSL with technique/pass blocks appended. Plain HLSL
 *   profiles reject `technique`, so the source is preprocessed once, the
 *   technique blocks are lifted out and parsed, and what remains is handed to
 *   the compiler once per entry point. That is a real parser for the ~20 lines
 *   of technique syntax these files use and no parser at all for the HLSL,
 *   which vkd3d already understands.
 *
 *   Parameters are bound through the CTAB constant table that vkd3d embeds in
 *   the bytecode -- the same table D3DX itself reads. So "matWorldViewProj"
 *   finds its register because the compiler said where it is, not because
 *   anything here parsed a declaration.
 *
 * Verified against the real data rather than assumed: register counts are not
 * always the full type (a float4x4 the shader only partly uses is allocated 3
 * registers, not 4), and matrices come back as MATRIX_COLUMNS, so a row-major
 * D3DXMATRIX has to be transposed on the way to the registers.
 */

#include "xplatform.h"
#include "directx/d3dx9.h"

#include <map>
#include <string>
#include <vector>

#include <vkd3d_shader.h>

namespace
{

/* ------------------------------------------------------------------ */
/* CTAB: the constant table vkd3d embeds, in D3DX's own format.        */
/* ------------------------------------------------------------------ */

/* D3DXREGISTER_SET */
const uint16_t cRegSetBool    = 0;
const uint16_t cRegSetInt4    = 1;
const uint16_t cRegSetFloat4  = 2;
const uint16_t cRegSetSampler = 3;

/* D3DXPARAMETER_CLASS */
const uint16_t cClassMatrixRows    = 2;
const uint16_t cClassMatrixColumns = 3;

struct Constant
{
	std::string name;
	uint16_t registerSet;
	uint16_t registerIndex;
	uint16_t registerCount;
	uint16_t paramClass;
	uint16_t rows;
	uint16_t columns;
};

/*
 * The bytecode's comment blocks, one of which is CTAB. Layout is the documented
 * D3DXSHADER_CONSTANTTABLE followed by `Constants` records of five DWORDs, all
 * offsets relative to the start of the table. Confirmed against vkd3d's writer
 * in libs/vkd3d-shader/hlsl_codegen.c and against real output.
 */
bool ParseConstantTable(const void* bytecode, size_t size, std::vector<Constant>& out)
{
	const uint32_t* token = static_cast<const uint32_t*>(bytecode);
	const size_t words = size / sizeof(uint32_t);

	/* Word 0 is the version token; comments follow, before the first instruction. */
	for (size_t i = 1; i < words; )
	{
		if ((token[i] & 0xffff) != 0xfffe)
			break;

		const uint32_t length = (token[i] >> 16) & 0x7fff;
		if (length == 0 || i + 1 + length > words)
			break;

		if (token[i + 1] == 0x42415443)      /* "CTAB" */
		{
			const char* table = reinterpret_cast<const char*>(&token[i + 2]);
			const uint32_t* header = reinterpret_cast<const uint32_t*>(table);

			const uint32_t count = header[3];
			const uint32_t infoOffset = header[4];

			for (uint32_t c = 0; c < count; ++c)
			{
				const uint32_t* record = reinterpret_cast<const uint32_t*>(table + infoOffset) + c * 5;
				const uint16_t* type = reinterpret_cast<const uint16_t*>(table + record[3]);

				Constant constant;
				constant.name = table + record[0];
				constant.registerSet = static_cast<uint16_t>(record[1] & 0xffff);
				constant.registerIndex = static_cast<uint16_t>(record[1] >> 16);
				constant.registerCount = static_cast<uint16_t>(record[2] & 0xffff);
				constant.paramClass = type[0];
				constant.rows = type[2];
				constant.columns = type[3];

				out.push_back(constant);
			}
			return true;
		}

		i += length + 1;
	}

	/* A shader with no uniforms has no CTAB, which is not an error. */
	return true;
}

/* ------------------------------------------------------------------ */
/* Compiling one entry point.                                          */
/* ------------------------------------------------------------------ */

/*
 * Bridges vkd3d's include callbacks to the ID3DXInclude the caller supplied.
 * Shader::D3DXInclude resolves paths relative to the .fx being compiled, which
 * is why this has to be the engine's handler and not a local file read.
 */
struct IncludeBridge
{
	ID3DXInclude* handler;
};

int OpenInclude(const char* filename, bool local, const char* parentData, void* context, struct vkd3d_shader_code* out)
{
	IncludeBridge* bridge = static_cast<IncludeBridge*>(context);
	if (!bridge || !bridge->handler)
		return VKD3D_ERROR;

	const void* data = NULL;
	UINT bytes = 0;
	const D3DXINCLUDE_TYPE type = local ? D3DXINC_LOCAL : D3DXINC_SYSTEM;

	if (FAILED(bridge->handler->Open(type, filename, parentData, &data, &bytes)))
		return VKD3D_ERROR;

	/*
	 * Copied rather than referenced. The engine's handler owns a single reusable
	 * buffer and frees it on Close, but vkd3d holds every open include until the
	 * whole compile finishes -- so handing back its pointer would alias.
	 */
	void* copy = std::malloc(bytes ? bytes : 1);
	if (!copy)
	{
		bridge->handler->Close(data);
		return VKD3D_ERROR;
	}
	std::memcpy(copy, data, bytes);
	bridge->handler->Close(data);

	out->code = copy;
	out->size = bytes;
	return VKD3D_OK;
}

void CloseInclude(const struct vkd3d_shader_code* code, void* context)
{
	std::free(const_cast<void*>(code->code));
}

void AppendMessages(std::string& log, const char* what, char* messages)
{
	if (messages && *messages)
	{
		log += what;
		log += ": ";
		log += messages;
		log += "\n";
	}
	vkd3d_shader_free_messages(messages);
}

/* Preprocess only -- resolves #include and the macro permutation, leaves the rest. */
bool PreprocessSource(const void* source, size_t size, const D3DXMACRO* macros,
	ID3DXInclude* include, const char* name, std::string& out, std::string& log)
{
	std::vector<vkd3d_shader_macro> macroBuf;
	for (const D3DXMACRO* macro = macros; macro && macro->Name; ++macro)
	{
		vkd3d_shader_macro entry;
		entry.name = macro->Name;
		entry.value = macro->Definition ? macro->Definition : "";
		macroBuf.push_back(entry);
	}

	IncludeBridge bridge = {include};

	struct vkd3d_shader_preprocess_info preprocess = {};
	preprocess.type = VKD3D_SHADER_STRUCTURE_TYPE_PREPROCESS_INFO;
	preprocess.macros = macroBuf.empty() ? NULL : macroBuf.data();
	preprocess.macro_count = static_cast<unsigned>(macroBuf.size());
	preprocess.pfn_open_include = include ? OpenInclude : NULL;
	preprocess.pfn_close_include = include ? CloseInclude : NULL;
	preprocess.include_context = &bridge;

	struct vkd3d_shader_compile_info info = {};
	info.type = VKD3D_SHADER_STRUCTURE_TYPE_COMPILE_INFO;
	info.next = &preprocess;
	info.source.code = source;
	info.source.size = size;
	info.source_type = VKD3D_SHADER_SOURCE_HLSL;
	info.target_type = VKD3D_SHADER_TARGET_NONE;
	info.log_level = VKD3D_SHADER_LOG_ERROR;
	info.source_name = name;

	struct vkd3d_shader_code result = {};
	char* messages = NULL;
	const int ret = vkd3d_shader_preprocess(&info, &result, &messages);
	AppendMessages(log, "preprocess", messages);

	if (ret != VKD3D_OK)
		return false;

	out.assign(static_cast<const char*>(result.code), result.size);
	vkd3d_shader_free_shader_code(&result);
	return true;
}

/* Compile one entry point of already-preprocessed source to D3D9 bytecode. */
bool CompileEntryPoint(const std::string& source, const char* profile, const char* entryPoint,
	const char* name, std::vector<char>& out, std::string& log)
{
	struct vkd3d_shader_hlsl_source_info hlsl = {};
	hlsl.type = VKD3D_SHADER_STRUCTURE_TYPE_HLSL_SOURCE_INFO;
	hlsl.entry_point = entryPoint;
	hlsl.profile = profile;

	struct vkd3d_shader_compile_info info = {};
	info.type = VKD3D_SHADER_STRUCTURE_TYPE_COMPILE_INFO;
	info.next = &hlsl;
	info.source.code = source.data();
	info.source.size = source.size();
	info.source_type = VKD3D_SHADER_SOURCE_HLSL;
	info.target_type = VKD3D_SHADER_TARGET_D3D_BYTECODE;
	info.log_level = VKD3D_SHADER_LOG_ERROR;
	info.source_name = name;

	struct vkd3d_shader_code result = {};
	char* messages = NULL;
	const int ret = vkd3d_shader_compile(&info, &result, &messages);
	AppendMessages(log, entryPoint, messages);

	if (ret != VKD3D_OK)
		return false;

	const char* bytes = static_cast<const char*>(result.code);
	out.assign(bytes, bytes + result.size);
	vkd3d_shader_free_shader_code(&result);
	return true;
}

/* ------------------------------------------------------------------ */
/* The technique/pass grammar.                                         */
/* ------------------------------------------------------------------ */

struct RenderState
{
	D3DRENDERSTATETYPE state;
	DWORD value;
};

struct Pass
{
	std::string name;
	std::vector<char> vertexBytecode;
	std::vector<char> pixelBytecode;
	std::vector<Constant> vertexConstants;
	std::vector<Constant> pixelConstants;
	IDirect3DVertexShader9* vertexShader;
	IDirect3DPixelShader9* pixelShader;
	std::vector<RenderState> renderStates;

	Pass(): vertexShader(NULL), pixelShader(NULL) {}
};

struct Technique
{
	std::string name;
	std::vector<Pass> passes;
	bool valid;

	Technique(): valid(true) {}
};

bool IsIdentChar(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

void SkipSpace(const std::string& text, size_t& pos)
{
	while (pos < text.size())
	{
		if (std::isspace(static_cast<unsigned char>(text[pos])))
		{
			++pos;
		}
		else if (text.compare(pos, 2, "//") == 0)
		{
			pos = text.find('\n', pos);
			if (pos == std::string::npos)
				pos = text.size();
		}
		else if (text.compare(pos, 2, "/*") == 0)
		{
			const size_t end = text.find("*/", pos + 2);
			pos = end == std::string::npos ? text.size() : end + 2;
		}
		else if (text[pos] == '#')
		{
			/* The preprocessor leaves #line directives behind. */
			pos = text.find('\n', pos);
			if (pos == std::string::npos)
				pos = text.size();
		}
		else
		{
			break;
		}
	}
}

std::string ReadIdent(const std::string& text, size_t& pos)
{
	SkipSpace(text, pos);
	const size_t start = pos;
	while (pos < text.size() && IsIdentChar(text[pos]))
		++pos;
	return text.substr(start, pos - start);
}

/* Position just past the '}' matching the '{' at `pos`. */
size_t SkipBraceBlock(const std::string& text, size_t pos)
{
	int depth = 0;
	for (; pos < text.size(); ++pos)
	{
		if (text[pos] == '{')
			++depth;
		else if (text[pos] == '}' && --depth == 0)
			return pos + 1;
	}
	return text.size();
}

/*
 * The render states these shaders assign, and the only values they use. Ten
 * assignments across all 23 files -- deliberately a closed list rather than the
 * whole of D3DRENDERSTATETYPE, so an .fx that starts using something new fails
 * loudly here instead of silently rendering wrong.
 */
bool LookupRenderState(const std::string& name, D3DRENDERSTATETYPE& state)
{
	std::string key;
	for (size_t i = 0; i < name.size(); ++i)
		key += static_cast<char>(std::tolower(static_cast<unsigned char>(name[i])));

	if (key == "zenable")           { state = D3DRS_ZENABLE;          return true; }
	if (key == "zwriteenable")      { state = D3DRS_ZWRITEENABLE;     return true; }
	if (key == "cullmode")          { state = D3DRS_CULLMODE;         return true; }
	if (key == "alphatestenable")   { state = D3DRS_ALPHATESTENABLE;  return true; }
	if (key == "alphablendenable")  { state = D3DRS_ALPHABLENDENABLE; return true; }
	if (key == "srcblend")          { state = D3DRS_SRCBLEND;         return true; }
	if (key == "destblend")         { state = D3DRS_DESTBLEND;        return true; }
	if (key == "blendop")           { state = D3DRS_BLENDOP;          return true; }

	return false;
}

bool LookupRenderStateValue(const std::string& text, DWORD& value)
{
	std::string key;
	for (size_t i = 0; i < text.size(); ++i)
		key += static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));

	if (key == "true")          { value = TRUE;                    return true; }
	if (key == "false")         { value = FALSE;                   return true; }
	if (key == "none")          { value = D3DCULL_NONE;            return true; }
	if (key == "cw")            { value = D3DCULL_CW;              return true; }
	if (key == "ccw")           { value = D3DCULL_CCW;             return true; }
	if (key == "one")           { value = D3DBLEND_ONE;            return true; }
	if (key == "zero")          { value = D3DBLEND_ZERO;           return true; }
	if (key == "srcalpha")      { value = D3DBLEND_SRCALPHA;       return true; }
	if (key == "invsrcalpha")   { value = D3DBLEND_INVSRCALPHA;    return true; }
	if (key == "srccolor")      { value = D3DBLEND_SRCCOLOR;       return true; }
	if (key == "invsrccolor")   { value = D3DBLEND_INVSRCCOLOR;    return true; }
	if (key == "destcolor")     { value = D3DBLEND_DESTCOLOR;      return true; }
	if (key == "invdestcolor")  { value = D3DBLEND_INVDESTCOLOR;   return true; }
	if (key == "add")           { value = D3DBLENDOP_ADD;          return true; }
	if (key == "subtract")      { value = D3DBLENDOP_SUBTRACT;     return true; }
	if (key == "revsubtract")   { value = D3DBLENDOP_REVSUBTRACT;  return true; }
	if (key == "min")           { value = D3DBLENDOP_MIN;          return true; }
	if (key == "max")           { value = D3DBLENDOP_MAX;          return true; }

	/* Plain numbers, which the syntax allows even though this data does not use them. */
	if (!key.empty() && (std::isdigit(static_cast<unsigned char>(key[0]))))
	{
		value = static_cast<DWORD>(std::strtoul(key.c_str(), NULL, 0));
		return true;
	}

	return false;
}

struct ShaderRef
{
	std::string profile;
	std::string entryPoint;
};

/*
 * One pass body: shader assignments and render states. Everything is
 * `Name = value;`, with `compile <profile> <entry>(...)` as the value for the
 * two shader slots.
 */
void ParsePassBody(const std::string& text, size_t pos, size_t end,
	Pass& pass, ShaderRef& vertex, ShaderRef& pixel, std::string& log)
{
	while (pos < end)
	{
		SkipSpace(text, pos);
		if (pos >= end || text[pos] == '}')
			break;

		const std::string name = ReadIdent(text, pos);
		if (name.empty())
		{
			++pos;
			continue;
		}

		SkipSpace(text, pos);
		if (pos >= end || text[pos] != '=')
			continue;
		++pos;

		SkipSpace(text, pos);
		const size_t valueStart = pos;
		const size_t semi = text.find(';', pos);
		const size_t valueEnd = (semi == std::string::npos || semi > end) ? end : semi;
		std::string value = text.substr(valueStart, valueEnd - valueStart);
		pos = valueEnd < end ? valueEnd + 1 : end;

		/* Trim */
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
			value.erase(value.size() - 1);

		size_t vpos = 0;
		const std::string first = ReadIdent(value, vpos);

		if (first == "compile")
		{
			ShaderRef ref;
			ref.profile = ReadIdent(value, vpos);
			ref.entryPoint = ReadIdent(value, vpos);

			std::string lower;
			for (size_t i = 0; i < name.size(); ++i)
				lower += static_cast<char>(std::tolower(static_cast<unsigned char>(name[i])));

			if (lower == "vertexshader")
				vertex = ref;
			else if (lower == "pixelshader")
				pixel = ref;
			continue;
		}

		D3DRENDERSTATETYPE state;
		DWORD stateValue;
		if (LookupRenderState(name, state) && LookupRenderStateValue(value, stateValue))
		{
			RenderState entry;
			entry.state = state;
			entry.value = stateValue;
			pass.renderStates.push_back(entry);
		}
		else
		{
			log += "unsupported pass state: " + name + " = " + value + "\n";
		}
	}
}

/* ------------------------------------------------------------------ */
/* The effect pool: a shared parameter store.                          */
/* ------------------------------------------------------------------ */

struct Parameter
{
	std::string name;
	std::vector<char> data;
	IDirect3DBaseTexture9* texture;

	Parameter(): texture(NULL) {}
};

typedef std::map<std::string, Parameter> Parameters;

/*
 * Deliberate divergence from D3DX, and the one place this is not a faithful
 * reimplementation.
 *
 * D3DX shares a parameter through a pool only when it is declared `shared`.
 * None of this game's 23 .fx files uses that keyword -- so strictly, nothing
 * would be shared. But Shader::SetMacro only re-applies parameters when there
 * was no previous macro block, so on switching permutations the engine expects
 * values set against one effect to be visible from another.
 *
 * Every effect sharing a pool here is a macro permutation of the same .fx, with
 * the same parameter names meaning the same things, so sharing all of them is
 * behaviourally equivalent to the engine re-applying and strictly safer than
 * leaving the second permutation with stale values.
 */
class EffectPool: public ID3DXEffectPool
{
public:
	EffectPool(): _refCount(1) {}

	Parameters& GetParameters() { return _parameters; }

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override
	{
		if (out)
			*out = this;
		AddRef();
		return S_OK;
	}

	ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	ULONG STDMETHODCALLTYPE Release() override
	{
		const ULONG count = --_refCount;
		if (!count)
			delete this;
		return count;
	}

private:
	ULONG _refCount;
	Parameters _parameters;
};

/* ------------------------------------------------------------------ */
/* Handles.                                                            */
/* ------------------------------------------------------------------ */

/*
 * D3DXHANDLE is an opaque pointer, and the engine passes technique handles and
 * parameter handles to different methods without any way for us to tell them
 * apart. Each is therefore tagged, rather than assuming a caller never mixes
 * them up.
 */
enum HandleKind
{
	hkParameter,
	hkTechnique
};

struct HandleTag
{
	HandleKind kind;
};

/*
 * Carries the name, not a Parameter*. The parameter store lives in the pool and
 * is shared between effects, so another effect building against the same pool
 * can insert into it while these handles are live. Resolving by name at use is
 * cheap and cannot dangle.
 */
struct ParameterHandle: HandleTag
{
	std::string name;
};

struct TechniqueHandle: HandleTag
{
	Technique* technique;
};

/* ------------------------------------------------------------------ */
/* The effect.                                                         */
/* ------------------------------------------------------------------ */

class Effect: public ID3DXEffect
{
public:
	Effect(IDirect3DDevice9* device, EffectPool* pool)
		: _refCount(1), _device(device), _pool(pool),
		  _currentTechnique(NULL), _activePass(NULL), _began(false)
	{
		_device->AddRef();
		if (_pool)
			_pool->AddRef();
	}

	bool Build(const void* source, size_t size, const D3DXMACRO* macros,
		ID3DXInclude* include, std::string& log);

	/* ---- IUnknown ---- */

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override
	{
		if (out)
			*out = this;
		AddRef();
		return S_OK;
	}

	ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	ULONG STDMETHODCALLTYPE Release() override
	{
		const ULONG count = --_refCount;
		if (!count)
			delete this;
		return count;
	}

	/* ---- what the engine uses ---- */

	D3DXHANDLE STDMETHODCALLTYPE GetParameterByName(D3DXHANDLE parent, const char* name) override;
	D3DXHANDLE STDMETHODCALLTYPE GetTechniqueByName(const char* name) override;
	D3DXHANDLE STDMETHODCALLTYPE GetTechnique(UINT index) override;
	HRESULT STDMETHODCALLTYPE GetTechniqueDesc(D3DXHANDLE technique, D3DXTECHNIQUE_DESC* desc) override;
	HRESULT STDMETHODCALLTYPE FindNextValidTechnique(D3DXHANDLE technique, D3DXHANDLE* next) override;
	HRESULT STDMETHODCALLTYPE ValidateTechnique(D3DXHANDLE technique) override;
	HRESULT STDMETHODCALLTYPE SetTechnique(D3DXHANDLE technique) override;
	D3DXHANDLE STDMETHODCALLTYPE GetCurrentTechnique() override;
	HRESULT STDMETHODCALLTYPE SetValue(D3DXHANDLE parameter, const void* data, UINT bytes) override;
	HRESULT STDMETHODCALLTYPE SetFloatArray(D3DXHANDLE parameter, const FLOAT* values, UINT count) override;
	HRESULT STDMETHODCALLTYPE SetTexture(D3DXHANDLE parameter, IDirect3DBaseTexture9* texture) override;
	HRESULT STDMETHODCALLTYPE Begin(UINT* passes, DWORD flags) override;
	HRESULT STDMETHODCALLTYPE BeginPass(UINT pass) override;
	HRESULT STDMETHODCALLTYPE CommitChanges() override;
	HRESULT STDMETHODCALLTYPE EndPass() override;
	HRESULT STDMETHODCALLTYPE End() override;
	HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** device) override;
	HRESULT STDMETHODCALLTYPE OnLostDevice() override { return S_OK; }
	HRESULT STDMETHODCALLTYPE OnResetDevice() override { return S_OK; }
	HRESULT STDMETHODCALLTYPE GetPool(ID3DXEffectPool** pool) override;

	/* ---- the rest ---- */

	HRESULT STDMETHODCALLTYPE GetDesc(D3DXEFFECT_DESC*) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetParameterDesc(D3DXHANDLE, D3DXPARAMETER_DESC*) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetPassDesc(D3DXHANDLE, D3DXPASS_DESC*) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetFunctionDesc(D3DXHANDLE, D3DXFUNCTION_DESC*) override { return E_NOTIMPL; }
	D3DXHANDLE STDMETHODCALLTYPE GetParameter(D3DXHANDLE, UINT) override { return NULL; }
	D3DXHANDLE STDMETHODCALLTYPE GetParameterBySemantic(D3DXHANDLE, const char*) override { return NULL; }
	D3DXHANDLE STDMETHODCALLTYPE GetParameterElement(D3DXHANDLE, UINT) override { return NULL; }
	D3DXHANDLE STDMETHODCALLTYPE GetPass(D3DXHANDLE, UINT) override { return NULL; }
	D3DXHANDLE STDMETHODCALLTYPE GetPassByName(D3DXHANDLE, const char*) override { return NULL; }
	D3DXHANDLE STDMETHODCALLTYPE GetFunction(UINT) override { return NULL; }
	D3DXHANDLE STDMETHODCALLTYPE GetFunctionByName(const char*) override { return NULL; }
	D3DXHANDLE STDMETHODCALLTYPE GetAnnotation(D3DXHANDLE, UINT) override { return NULL; }
	D3DXHANDLE STDMETHODCALLTYPE GetAnnotationByName(D3DXHANDLE, const char*) override { return NULL; }
	HRESULT STDMETHODCALLTYPE GetValue(D3DXHANDLE, void*, UINT) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE SetBool(D3DXHANDLE, WINBOOL) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetBool(D3DXHANDLE, WINBOOL*) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE SetBoolArray(D3DXHANDLE, const WINBOOL*, UINT) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetBoolArray(D3DXHANDLE, WINBOOL*, UINT) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE SetInt(D3DXHANDLE, INT) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetInt(D3DXHANDLE, INT*) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE SetIntArray(D3DXHANDLE, const INT*, UINT) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetIntArray(D3DXHANDLE, INT*, UINT) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE SetFloat(D3DXHANDLE, FLOAT) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetFloat(D3DXHANDLE, FLOAT*) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetFloatArray(D3DXHANDLE, FLOAT*, UINT) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE SetVector(D3DXHANDLE, const D3DXVECTOR4*) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetVector(D3DXHANDLE, D3DXVECTOR4*) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE SetVectorArray(D3DXHANDLE, const D3DXVECTOR4*, UINT) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetVectorArray(D3DXHANDLE, D3DXVECTOR4*, UINT) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE SetMatrix(D3DXHANDLE, const D3DXMATRIX*) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetMatrix(D3DXHANDLE, D3DXMATRIX*) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE SetMatrixArray(D3DXHANDLE, const D3DXMATRIX*, UINT) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetMatrixArray(D3DXHANDLE, D3DXMATRIX*, UINT) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE SetMatrixPointerArray(D3DXHANDLE, const D3DXMATRIX**, UINT) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetMatrixPointerArray(D3DXHANDLE, D3DXMATRIX**, UINT) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE SetMatrixTranspose(D3DXHANDLE, const D3DXMATRIX*) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetMatrixTranspose(D3DXHANDLE, D3DXMATRIX*) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE SetMatrixTransposeArray(D3DXHANDLE, const D3DXMATRIX*, UINT) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetMatrixTransposeArray(D3DXHANDLE, D3DXMATRIX*, UINT) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE SetMatrixTransposePointerArray(D3DXHANDLE, const D3DXMATRIX**, UINT) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetMatrixTransposePointerArray(D3DXHANDLE, D3DXMATRIX**, UINT) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE SetString(D3DXHANDLE, const char*) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetString(D3DXHANDLE, const char**) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetTexture(D3DXHANDLE, IDirect3DBaseTexture9**) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetPixelShader(D3DXHANDLE, IDirect3DPixelShader9**) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetVertexShader(D3DXHANDLE, IDirect3DVertexShader9**) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE SetArrayRange(D3DXHANDLE, UINT, UINT) override { return E_NOTIMPL; }
	WINBOOL STDMETHODCALLTYPE IsParameterUsed(D3DXHANDLE, D3DXHANDLE) override { return TRUE; }
	HRESULT STDMETHODCALLTYPE SetStateManager(ID3DXEffectStateManager*) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetStateManager(ID3DXEffectStateManager**) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE BeginParameterBlock() override { return E_NOTIMPL; }
	D3DXHANDLE STDMETHODCALLTYPE EndParameterBlock() override { return NULL; }
	HRESULT STDMETHODCALLTYPE ApplyParameterBlock(D3DXHANDLE) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE DeleteParameterBlock(D3DXHANDLE) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE CloneEffect(IDirect3DDevice9*, ID3DXEffect**) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE SetRawValue(D3DXHANDLE, const void*, UINT, UINT) override { return E_NOTIMPL; }

private:
	~Effect();

	Parameters& GetParameters() { return _pool ? _pool->GetParameters() : _ownParameters; }
	Parameter* FindParameter(const std::string& name);
	void UploadConstants(const std::vector<Constant>& constants, bool vertex);
	void ApplyPass(Pass& pass);

	ULONG _refCount;
	IDirect3DDevice9* _device;
	EffectPool* _pool;

	std::vector<Technique*> _techniques;
	Parameters _ownParameters;

	/* Handles are handed out by pointer, so they have to outlive every call. */
	std::map<std::string, ParameterHandle> _parameterHandles;
	std::map<std::string, TechniqueHandle> _techniqueHandles;

	Technique* _currentTechnique;
	Pass* _activePass;
	bool _began;

	/* Saved on Begin, restored on End -- Begin's contract is that it preserves state. */
	std::vector<RenderState> _savedStates;
};

Effect::~Effect()
{
	for (size_t t = 0; t < _techniques.size(); ++t)
	{
		Technique* technique = _techniques[t];
		for (size_t p = 0; p < technique->passes.size(); ++p)
		{
			if (technique->passes[p].vertexShader)
				technique->passes[p].vertexShader->Release();
			if (technique->passes[p].pixelShader)
				technique->passes[p].pixelShader->Release();
		}
		delete technique;
	}

	if (_pool)
		_pool->Release();
	_device->Release();
}

/*
 * Parse, compile, and create the device shaders.
 *
 * A technique whose shaders fail to compile is kept but marked invalid, rather
 * than failing the whole effect. That is what D3DX does, and it is what makes
 * the engine's caps-based FindNextValidTechnique selection work: several .fx
 * files carry a cheaper fallback technique for exactly this.
 */
bool Effect::Build(const void* source, size_t size, const D3DXMACRO* macros,
	ID3DXInclude* include, std::string& log)
{
	std::string text;
	if (!PreprocessSource(source, size, macros, include, "effect.fx", text, log))
		return false;

	/* Lift out the technique blocks; what is left is the HLSL to compile. */
	std::string hlsl;
	struct ParsedPass
	{
		Technique* technique;
		size_t index;
		ShaderRef vertex;
		ShaderRef pixel;
	};
	std::vector<ParsedPass> parsedPasses;

	size_t pos = 0;
	size_t copied = 0;

	while (pos < text.size())
	{
		const size_t found = text.find("technique", pos);
		if (found == std::string::npos)
			break;

		/* Only a standalone word, not a substring of a longer identifier. */
		const bool leftOk = found == 0 || !IsIdentChar(text[found - 1]);
		const bool rightOk = found + 9 >= text.size() || !IsIdentChar(text[found + 9]);
		if (!leftOk || !rightOk)
		{
			pos = found + 9;
			continue;
		}

		size_t cursor = found + 9;
		const std::string techniqueName = ReadIdent(text, cursor);
		SkipSpace(text, cursor);
		if (cursor >= text.size() || text[cursor] != '{')
		{
			pos = found + 9;
			continue;
		}

		const size_t blockEnd = SkipBraceBlock(text, cursor);

		hlsl.append(text, copied, found - copied);
		copied = blockEnd;
		pos = blockEnd;

		Technique* technique = new Technique();
		technique->name = techniqueName;

		/* Passes inside the technique block. */
		size_t inner = cursor + 1;
		while (inner < blockEnd)
		{
			const size_t passAt = text.find("pass", inner);
			if (passAt == std::string::npos || passAt >= blockEnd)
				break;

			const bool passLeft = passAt == 0 || !IsIdentChar(text[passAt - 1]);
			const bool passRight = passAt + 4 >= text.size() || !IsIdentChar(text[passAt + 4]);
			if (!passLeft || !passRight)
			{
				inner = passAt + 4;
				continue;
			}

			size_t passCursor = passAt + 4;
			const std::string passName = ReadIdent(text, passCursor);
			SkipSpace(text, passCursor);
			if (passCursor >= text.size() || text[passCursor] != '{')
			{
				inner = passAt + 4;
				continue;
			}

			const size_t passEnd = SkipBraceBlock(text, passCursor);

			Pass pass;
			pass.name = passName;
			ParsedPass parsed;
			parsed.technique = technique;
			parsed.index = technique->passes.size();

			ParsePassBody(text, passCursor + 1, passEnd - 1, pass, parsed.vertex, parsed.pixel, log);

			technique->passes.push_back(pass);
			parsedPasses.push_back(parsed);

			inner = passEnd;
		}

		_techniques.push_back(technique);
	}

	hlsl.append(text, copied, std::string::npos);

	if (_techniques.empty())
	{
		log += "no techniques found\n";
		return false;
	}

	/* Compile every referenced entry point. */
	for (size_t i = 0; i < parsedPasses.size(); ++i)
	{
		ParsedPass& parsed = parsedPasses[i];
		Pass& pass = parsed.technique->passes[parsed.index];

		if (!parsed.vertex.entryPoint.empty())
		{
			if (CompileEntryPoint(hlsl, parsed.vertex.profile.c_str(), parsed.vertex.entryPoint.c_str(),
					"effect.fx", pass.vertexBytecode, log))
			{
				ParseConstantTable(pass.vertexBytecode.data(), pass.vertexBytecode.size(), pass.vertexConstants);
				if (FAILED(_device->CreateVertexShader(
						reinterpret_cast<const DWORD*>(pass.vertexBytecode.data()), &pass.vertexShader)))
				{
					log += "CreateVertexShader failed for " + parsed.vertex.entryPoint + "\n";
					parsed.technique->valid = false;
				}
			}
			else
			{
				parsed.technique->valid = false;
			}
		}

		if (!parsed.pixel.entryPoint.empty())
		{
			if (CompileEntryPoint(hlsl, parsed.pixel.profile.c_str(), parsed.pixel.entryPoint.c_str(),
					"effect.fx", pass.pixelBytecode, log))
			{
				ParseConstantTable(pass.pixelBytecode.data(), pass.pixelBytecode.size(), pass.pixelConstants);
				if (FAILED(_device->CreatePixelShader(
						reinterpret_cast<const DWORD*>(pass.pixelBytecode.data()), &pass.pixelShader)))
				{
					log += "CreatePixelShader failed for " + parsed.pixel.entryPoint + "\n";
					parsed.technique->valid = false;
				}
			}
			else
			{
				parsed.technique->valid = false;
			}
		}
	}

	/*
	 * Every constant any shader declares becomes a parameter, so
	 * GetParameterByName answers for anything actually used. Names the shaders
	 * never mention correctly return NULL, and the engine skips them.
	 */
	Parameters& parameters = GetParameters();
	for (size_t t = 0; t < _techniques.size(); ++t)
	{
		Technique* technique = _techniques[t];
		for (size_t p = 0; p < technique->passes.size(); ++p)
		{
			const Pass& pass = technique->passes[p];
			for (int which = 0; which < 2; ++which)
			{
				const std::vector<Constant>& constants = which ? pass.pixelConstants : pass.vertexConstants;
				for (size_t c = 0; c < constants.size(); ++c)
				{
					Parameters::iterator iter = parameters.find(constants[c].name);
					if (iter == parameters.end())
					{
						Parameter parameter;
						parameter.name = constants[c].name;
						parameters.insert(Parameters::value_type(constants[c].name, parameter));
					}

					if (_parameterHandles.find(constants[c].name) == _parameterHandles.end())
					{
						ParameterHandle handle;
						handle.kind = hkParameter;
						handle.name = constants[c].name;
						_parameterHandles.insert(std::map<std::string, ParameterHandle>::value_type(
							constants[c].name, handle));
					}
				}
			}
		}
	}

	for (size_t t = 0; t < _techniques.size(); ++t)
	{
		TechniqueHandle handle;
		handle.kind = hkTechnique;
		handle.technique = _techniques[t];
		_techniqueHandles.insert(std::map<std::string, TechniqueHandle>::value_type(
			_techniques[t]->name, handle));
	}

	return true;
}

Parameter* Effect::FindParameter(const std::string& name)
{
	Parameters& parameters = GetParameters();
	Parameters::iterator iter = parameters.find(name);
	return iter == parameters.end() ? NULL : &iter->second;
}

D3DXHANDLE STDMETHODCALLTYPE Effect::GetParameterByName(D3DXHANDLE parent, const char* name)
{
	if (!name)
		return NULL;

	std::map<std::string, ParameterHandle>::iterator iter = _parameterHandles.find(name);
	if (iter == _parameterHandles.end())
		return NULL;

	return reinterpret_cast<D3DXHANDLE>(&iter->second);
}

D3DXHANDLE STDMETHODCALLTYPE Effect::GetTechniqueByName(const char* name)
{
	if (!name)
		return NULL;

	std::map<std::string, TechniqueHandle>::iterator iter = _techniqueHandles.find(name);
	return iter == _techniqueHandles.end() ? NULL : reinterpret_cast<D3DXHANDLE>(&iter->second);
}

D3DXHANDLE STDMETHODCALLTYPE Effect::GetTechnique(UINT index)
{
	if (index >= _techniques.size())
		return NULL;

	return GetTechniqueByName(_techniques[index]->name.c_str());
}

HRESULT STDMETHODCALLTYPE Effect::GetTechniqueDesc(D3DXHANDLE handle, D3DXTECHNIQUE_DESC* desc)
{
	if (!desc)
		return D3DERR_INVALIDCALL;

	Technique* technique = _currentTechnique;
	if (handle)
	{
		HandleTag* tag = reinterpret_cast<HandleTag*>(const_cast<char*>(handle));
		if (tag->kind != hkTechnique)
			return D3DERR_INVALIDCALL;
		technique = reinterpret_cast<TechniqueHandle*>(const_cast<char*>(handle))->technique;
	}

	if (!technique)
		return D3DERR_INVALIDCALL;

	desc->Name = technique->name.c_str();
	desc->Passes = static_cast<UINT>(technique->passes.size());
	desc->Annotations = 0;
	return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Effect::ValidateTechnique(D3DXHANDLE handle)
{
	if (!handle)
		return D3DERR_INVALIDCALL;

	HandleTag* tag = reinterpret_cast<HandleTag*>(const_cast<char*>(handle));
	if (tag->kind != hkTechnique)
		return D3DERR_INVALIDCALL;

	return reinterpret_cast<TechniqueHandle*>(const_cast<char*>(handle))->technique->valid ? D3D_OK : E_FAIL;
}

/*
 * The first technique at or after `handle` whose shaders all compiled. Passing
 * NULL means "start from the beginning", which is how the engine calls it.
 */
HRESULT STDMETHODCALLTYPE Effect::FindNextValidTechnique(D3DXHANDLE handle, D3DXHANDLE* next)
{
	if (!next)
		return D3DERR_INVALIDCALL;

	size_t start = 0;
	if (handle)
	{
		HandleTag* tag = reinterpret_cast<HandleTag*>(const_cast<char*>(handle));
		if (tag->kind != hkTechnique)
			return D3DERR_INVALIDCALL;

		Technique* from = reinterpret_cast<TechniqueHandle*>(const_cast<char*>(handle))->technique;
		for (size_t i = 0; i < _techniques.size(); ++i)
		{
			if (_techniques[i] == from)
			{
				start = i + 1;
				break;
			}
		}
	}

	for (size_t i = start; i < _techniques.size(); ++i)
	{
		if (_techniques[i]->valid)
		{
			*next = GetTechniqueByName(_techniques[i]->name.c_str());
			return D3D_OK;
		}
	}

	*next = NULL;
	return E_FAIL;
}

HRESULT STDMETHODCALLTYPE Effect::SetTechnique(D3DXHANDLE handle)
{
	if (!handle)
		return D3DERR_INVALIDCALL;

	HandleTag* tag = reinterpret_cast<HandleTag*>(const_cast<char*>(handle));
	if (tag->kind != hkTechnique)
		return D3DERR_INVALIDCALL;

	_currentTechnique = reinterpret_cast<TechniqueHandle*>(const_cast<char*>(handle))->technique;
	return D3D_OK;
}

D3DXHANDLE STDMETHODCALLTYPE Effect::GetCurrentTechnique()
{
	return _currentTechnique ? GetTechniqueByName(_currentTechnique->name.c_str()) : NULL;
}

/*
 * Stores the bytes as given. D3DX does the same for SetValue -- it is the raw
 * path, with no type conversion -- and the layout fix-up that matters, matrix
 * transposition, happens on the way to the registers where the shader's
 * declared class is known.
 */
HRESULT STDMETHODCALLTYPE Effect::SetValue(D3DXHANDLE handle, const void* data, UINT bytes)
{
	if (!handle || !data)
		return D3DERR_INVALIDCALL;

	HandleTag* tag = reinterpret_cast<HandleTag*>(const_cast<char*>(handle));
	if (tag->kind != hkParameter)
		return D3DERR_INVALIDCALL;

	Parameter* parameter = FindParameter(reinterpret_cast<ParameterHandle*>(const_cast<char*>(handle))->name);
	if (!parameter)
		return D3DERR_INVALIDCALL;

	const char* bytesIn = static_cast<const char*>(data);
	parameter->data.assign(bytesIn, bytesIn + bytes);
	return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Effect::SetFloatArray(D3DXHANDLE handle, const FLOAT* values, UINT count)
{
	return SetValue(handle, values, count * sizeof(FLOAT));
}

HRESULT STDMETHODCALLTYPE Effect::SetTexture(D3DXHANDLE handle, IDirect3DBaseTexture9* texture)
{
	if (!handle)
		return D3DERR_INVALIDCALL;

	HandleTag* tag = reinterpret_cast<HandleTag*>(const_cast<char*>(handle));
	if (tag->kind != hkParameter)
		return D3DERR_INVALIDCALL;

	Parameter* parameter = FindParameter(reinterpret_cast<ParameterHandle*>(const_cast<char*>(handle))->name);
	if (!parameter)
		return D3DERR_INVALIDCALL;

	/*
	 * Not reference counted. The engine owns every texture it sets here through
	 * its own resource manager and outlives the binding; taking a reference
	 * would keep dead textures alive across a device reset, which is the bug
	 * this shape avoids.
	 */
	parameter->texture = texture;
	return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Effect::Begin(UINT* passes, DWORD flags)
{
	if (!_currentTechnique)
		return D3DERR_INVALIDCALL;

	if (passes)
		*passes = static_cast<UINT>(_currentTechnique->passes.size());

	_began = true;
	return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Effect::BeginPass(UINT index)
{
	if (!_began || !_currentTechnique || index >= _currentTechnique->passes.size())
		return D3DERR_INVALIDCALL;

	_activePass = &_currentTechnique->passes[index];
	ApplyPass(*_activePass);
	return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Effect::CommitChanges()
{
	if (!_activePass)
		return D3D_OK;

	UploadConstants(_activePass->vertexConstants, true);
	UploadConstants(_activePass->pixelConstants, false);
	return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Effect::EndPass()
{
	if (!_activePass)
		return D3DERR_INVALIDCALL;

	/* Restore whatever this pass overwrote, in reverse. */
	for (size_t i = _savedStates.size(); i-- > 0; )
		_device->SetRenderState(_savedStates[i].state, _savedStates[i].value);
	_savedStates.clear();

	_activePass = NULL;
	return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Effect::End()
{
	_began = false;
	return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Effect::GetDevice(IDirect3DDevice9** device)
{
	if (!device)
		return D3DERR_INVALIDCALL;

	*device = _device;
	_device->AddRef();
	return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Effect::GetPool(ID3DXEffectPool** pool)
{
	if (!pool)
		return D3DERR_INVALIDCALL;

	*pool = _pool;
	if (_pool)
		_pool->AddRef();
	return D3D_OK;
}

void Effect::ApplyPass(Pass& pass)
{
	_device->SetVertexShader(pass.vertexShader);
	_device->SetPixelShader(pass.pixelShader);

	_savedStates.clear();
	for (size_t i = 0; i < pass.renderStates.size(); ++i)
	{
		RenderState saved;
		saved.state = pass.renderStates[i].state;
		saved.value = 0;
		_device->GetRenderState(saved.state, &saved.value);
		_savedStates.push_back(saved);

		_device->SetRenderState(pass.renderStates[i].state, pass.renderStates[i].value);
	}

	UploadConstants(pass.vertexConstants, true);
	UploadConstants(pass.pixelConstants, false);
}

/*
 * Push every parameter a shader declares into its registers.
 *
 * Three things here were measured against real vkd3d output rather than assumed
 * (see tools/shader-pipeline/vkd3d_hlsl_check.py and the notes at the top):
 *
 *   - registerCount can be smaller than the type. A float4x4 the shader only
 *     partly uses is allocated 3 registers, not 4, so the count drives the
 *     upload, never sizeof(D3DXMATRIX).
 *   - matrices come back classed MATRIX_COLUMNS. A D3DXMATRIX is row-major, so
 *     register i must be column i -- transposed on the way in.
 *   - samplers live in their own register set, indexed by stage.
 */
void Effect::UploadConstants(const std::vector<Constant>& constants, bool vertex)
{
	for (size_t i = 0; i < constants.size(); ++i)
	{
		const Constant& constant = constants[i];
		const Parameter* parameter = FindParameter(constant.name);
		if (!parameter)
			continue;

		if (constant.registerSet == cRegSetSampler)
		{
			if (!parameter->texture)
				continue;

			/*
			 * Vertex textures occupy a separate range on the device; D3D9 spells
			 * that as D3DVERTEXTEXTURESAMPLER0 rather than a second namespace.
			 */
			const DWORD stage = vertex
				? D3DVERTEXTEXTURESAMPLER0 + constant.registerIndex
				: constant.registerIndex;

			_device->SetTexture(stage, parameter->texture);
			continue;
		}

		if (parameter->data.empty() || constant.registerCount == 0)
			continue;

		const float* source = reinterpret_cast<const float*>(parameter->data.data());
		const size_t available = parameter->data.size() / sizeof(float);

		std::vector<float> registers(constant.registerCount * 4, 0.0f);

		if (constant.paramClass == cClassMatrixColumns && constant.rows && constant.columns)
		{
			/* Register r holds column r: element (row, r) of a row-major source. */
			for (unsigned r = 0; r < constant.registerCount; ++r)
			{
				for (unsigned row = 0; row < constant.rows && row < 4; ++row)
				{
					const size_t index = row * constant.columns + r;
					if (index < available)
						registers[r * 4 + row] = source[index];
				}
			}
		}
		else if (constant.paramClass == cClassMatrixRows && constant.columns)
		{
			for (unsigned r = 0; r < constant.registerCount; ++r)
			{
				for (unsigned column = 0; column < constant.columns && column < 4; ++column)
				{
					const size_t index = r * constant.columns + column;
					if (index < available)
						registers[r * 4 + column] = source[index];
				}
			}
		}
		else
		{
			/* Scalars, vectors and arrays are already in register order. */
			const size_t count = registers.size() < available ? registers.size() : available;
			std::memcpy(registers.data(), source, count * sizeof(float));
		}

		if (constant.registerSet == cRegSetFloat4)
		{
			if (vertex)
				_device->SetVertexShaderConstantF(constant.registerIndex, registers.data(), constant.registerCount);
			else
				_device->SetPixelShaderConstantF(constant.registerIndex, registers.data(), constant.registerCount);
		}
		else if (constant.registerSet == cRegSetInt4)
		{
			std::vector<INT> ints(registers.size());
			for (size_t v = 0; v < registers.size(); ++v)
				ints[v] = static_cast<INT>(registers[v]);

			if (vertex)
				_device->SetVertexShaderConstantI(constant.registerIndex, ints.data(), constant.registerCount);
			else
				_device->SetPixelShaderConstantI(constant.registerIndex, ints.data(), constant.registerCount);
		}
		else if (constant.registerSet == cRegSetBool)
		{
			/*
			 * Bools occupy one register each, not four, so the source is read
			 * per-element rather than out of the float4 staging buffer.
			 */
			std::vector<WINBOOL> bools(constant.registerCount, FALSE);
			for (unsigned b = 0; b < constant.registerCount && b < available; ++b)
				bools[b] = source[b] != 0.0f ? TRUE : FALSE;

			if (vertex)
				_device->SetVertexShaderConstantB(constant.registerIndex, bools.data(), constant.registerCount);
			else
				_device->SetPixelShaderConstantB(constant.registerIndex, bools.data(), constant.registerCount);
		}
	}
}

}

/* ------------------------------------------------------------------ */
/* Entry points.                                                       */
/* ------------------------------------------------------------------ */

HRESULT WINAPI D3DXCreateEffectPool(ID3DXEffectPool** pool)
{
	if (!pool)
		return D3DERR_INVALIDCALL;

	*pool = new EffectPool();
	return D3D_OK;
}

HRESULT WINAPI D3DXCreateEffect(IDirect3DDevice9* device, const void* srcData, UINT srcDataSize,
	const D3DXMACRO* defines, ID3DXInclude* include, DWORD flags,
	ID3DXEffectPool* pool, ID3DXEffect** effect, ID3DXBuffer** compilationErrors)
{
	if (compilationErrors)
		*compilationErrors = NULL;

	if (!device || !srcData || !effect)
		return D3DERR_INVALIDCALL;

	*effect = NULL;

	Effect* built = new Effect(device, static_cast<EffectPool*>(pool));

	std::string log;
	if (!built->Build(srcData, srcDataSize, defines, include, log))
	{
		std::fprintf(stderr, "rrr3d: D3DXCreateEffect failed:\n%s", log.c_str());
		built->Release();
		return E_FAIL;
	}

	if (!log.empty())
		std::fprintf(stderr, "rrr3d: D3DXCreateEffect warnings:\n%s", log.c_str());

	*effect = built;
	return D3D_OK;
}
