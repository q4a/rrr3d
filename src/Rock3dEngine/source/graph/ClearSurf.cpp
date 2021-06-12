#include "stdafx.h"

#include "graph\\ClearSurf.h"

namespace r3d
{

namespace graph
{

ClearSurf::ClearSurf(): _mode(cmColor), _color(0, 0, 0, 0)
{
	ApplyMode();
}

void ClearSurf::ApplyMode()
{
	const std::string techNames[] = {"techClearSurf", "techFillMaxDepth"};

	shader.SetTech(techNames[_mode]);
}

void ClearSurf::Render(Engine& engine)
{
	shader.SetValueDir("color", _color);	
	
	ApplyRT(engine, RtFlags(0, 0));

	shader.Apply(engine);
	DrawScreenQuad(engine);
	shader.UnApply(engine);

	UnApplyRT(engine);
}

ClearSurf::Mode ClearSurf::GetMode() const
{
	return _mode;
}

void ClearSurf::SetMode(Mode value)
{
	if (_mode != value)
	{
		_mode = value;
		ApplyMode();
	}
}

const D3DXCOLOR& ClearSurf::GetColor() const
{
	return _color;
}

void ClearSurf::SetColor(const D3DXCOLOR& value)
{
	_color = value;
}

}

}