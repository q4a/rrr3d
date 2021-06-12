#ifndef R3D_GRAPH_IACTOR
#define R3D_GRAPH_IACTOR

#include "lslUtility.h"

namespace r3d
{
	
namespace graph
{

class IActor
{
public:
	enum Lighting
	{
		glNone, //Без освещения
		glStd,  //Станд. техника
		glPix,  //Попиксельное освещение
		glRefl, //Отражает сцену
		glBump, //Бамп
		glRefr, //Преломление
		glPlanarRefl, //Отражает на себе

		cLightingEnd
	};
	
	enum GraphProp
	{
		gpReflWater = 0, //Отражается в воде
		gpReflScene,     //Отражается на объектах сцены(относительно камеры)
		gpShadowCast,    //Кастует тень
		gpShadowApp,     //Принимает тень		

		gpColor,         //Видим в сцене

		gpDynamic,       //Динамический, свободно перемещается в пространстве
		gpMorph,         //Морфинговый, не имеет постоянной структуры, динамически изменяется. Не куллится
		gpCullOpacity,   //Становится прозрачным при перекрытии видимости

		cGraphPropEnd
	};

	enum Order
	{
		goDefault = 0,  //Порядок не имеет значения
		goEffect,       //Является эффектом. Обычно прозрачный, тест на глубину, освещение отключены
		goOpacity,      //Прозрачный
		goFirst,
		goLast,

		cOrderEnd
	};

	typedef lsl::Bitset<cGraphPropEnd> GraphProps;

	struct GraphDesc
	{
		GraphDesc(): lighting(glStd), props(0), order(goDefault) {props.set(gpColor);}

		GraphDesc(Lighting mLighting, const GraphProps& mProps, const Order& mOrder): lighting(mLighting), props(mProps), order(mOrder) {}

		bool operator==(const GraphDesc& value) const
		{
			return lighting == value.lighting && props == value.props && order == value.order;
		}
		bool operator!=(const GraphDesc& value) const
		{
			return !operator==(value);
		}

		Lighting lighting;
		GraphProps props;
		Order order;
	};
};

static const char* cLightingStr[IActor::cLightingEnd] = {"glNone", "glStd", "glPix", "glRefl", "glBump", "glRefr", "glPlanarRefl"};

static const char* cGraphPropStr[IActor::cGraphPropEnd] = {"gpReflWater", "gpReflScene", "gpShadowCast", "gpShadowApp", "gpColor", "gpDynamic", "gpMorph", "gpCullOpacity"};

static const char* cGraphOrderStr[IActor::cOrderEnd] = {"goDefault", "goOpacity", "goEffect", "goLast"};

}

}

#endif