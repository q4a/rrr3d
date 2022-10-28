#ifndef LSL_MATH
#define LSL_MATH

#include "MathCommon.h"
#include "lslCommon.h"
#include "lslException.h"

//namespace r3d
//{

#define SAFE_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define SAFE_MAX(a, b) (((a) > (b)) ? (a) : (b))

template<class _Res> _Res Floor(float value);
template<class _Res> _Res Ceil(float value);
template<class _Res> _Res Round(_Res value);
float Random();
float RandomRange(float from, float to);
int RandomRange(int from, int to);
float NumAbsAdd(float absVal, float addVal);
float ScalarTransform(float scalar, const glm::vec3& vec, const D3DXMATRIX& mat);

void BuildWorldMatrix(const glm::vec3& pos, const glm::vec3& scale, const glm::quat& rot, D3DXMATRIX& outMat);
D3DXMATRIX BuildWorldMatrix(const glm::vec3& pos, const glm::vec3& scale, const glm::quat& rot);
void MatrixRotationFromAxis(const glm::vec3& xVec, const glm::vec3& yVec, const glm::vec3& zVec, D3DXMATRIX& matOut);
void MatrixSetTranslation(const glm::vec3& vec, D3DXMATRIX& outMat);
void MatrixTranslate(const glm::vec3& vec, D3DXMATRIX& outMat);
void MatrixSetScale(const glm::vec3& vec, D3DXMATRIX& outMat);
void MatrixScale(const glm::vec3& vec, D3DXMATRIX& outMat);
void MatGetPos(const D3DXMATRIX& mat, glm::vec3& outPos);
glm::vec3 MatGetPos(const D3DXMATRIX& mat);

//
glm::vec2 Vec2TransformCoord(const glm::vec2 &vec, const glm::mat4 &mat);
void Vec2NormCCW(const glm::vec2& vec2, glm::vec2& outVec);
glm::vec2 Vec2NormCCW(const glm::vec2& vec2);
void Vec2NormCW(const glm::vec2& vec2, glm::vec2& outVec);
//Длина проекции vec1 на vec2
float Vec2Proj(const glm::vec2& vec1, const glm::vec2& vec2);
void operator*=(glm::vec2& vec1, const glm::vec2& vec2);
glm::vec2 operator*(const glm::vec2& vec1, const glm::vec2& vec2);
void operator/=(glm::vec2& vec1, const glm::vec2& vec2);
glm::vec2 operator/(const glm::vec2& vec1, const glm::vec2& vec2);

glm::vec3 Vec3FromVec2(const glm::vec2& vec);
void Vec3Invert(const glm::vec3& vec, glm::vec3& rOut);
glm::vec3 Vec3Invert(const glm::vec3& vec);
glm::vec3 Vec3TransformCoord(const glm::vec3& vec, const D3DXMATRIX& mat);
void Vec3Abs(const glm::vec3& vec, glm::vec3& rOut);
glm::vec3 Vec3Abs(const glm::vec3& vec);
void Vec3Rotate(const glm::vec3& v, const glm::quat& quat, glm::vec3& outVec);
void operator*=(glm::vec3& vec1, const glm::vec3& vec2);
glm::vec3 operator*(const glm::vec3& vec1, const glm::vec3& vec2);
void operator/=(glm::vec3& vec1, const glm::vec3& vec2);
glm::vec3 operator/(const glm::vec3& vec1, const glm::vec3& vec2);
bool operator>=(const glm::vec3& vec1, float scalar);
bool operator<(const glm::vec3& vec1, float scalar);
bool operator>(const glm::vec3& vec1, const glm::vec3& vec2);
bool operator<(const glm::vec3& vec1, const glm::vec3& vec2);

D3DXVECTOR4 Vec4FromVec2(const glm::vec2& vec);
D3DXVECTOR4 Vec4FromVec3(const glm::vec3& vec);

//Линия из нормали и точки
void Line2FromNorm(const glm::vec2& norm, const glm::vec2& point, glm::vec3& outLine);
glm::vec3 Line2FromNorm(const glm::vec2& norm, const glm::vec2& point);
//Линия из направляющей и точки
void Line2FromDir(const glm::vec2& dir, const glm::vec2& point, glm::vec3& outLine);
glm::vec3 Line2FromDir(const glm::vec2& dir, const glm::vec2& point);
void Line2GetNorm(const glm::vec3& line, glm::vec2& norm);
void Line2GetDir(const glm::vec3& line, glm::vec2& dir);
void Line2GetRadiusVec(const glm::vec3& line, glm::vec2& outVec);
glm::vec2 Line2GetRadiusVec(const glm::vec3& line);
glm::vec2 Line2GetNorm(const glm::vec3& line);
//Расстояние от прямой до точки по направлению нормали. Если точка лежит за линией по направлению нормали то результат положительный, иначе отрицательный
float Line2DistToPoint(const glm::vec3& line, const glm::vec2& point);
//Возвращает нормальный вектор до точки, этот вектор в общем случае неединичный
void Line2NormVecToPoint(const glm::vec3& line, const glm::vec2& point, glm::vec2& outNormVec);
glm::vec2 Line2NormVecToPoint(const glm::vec3& line, const glm::vec2& point);

bool RayCastIntersectSphere(const glm::vec3& rayPos, const glm::vec3& rayVec, const glm::vec3& spherePos, float sphereRadius, float* t = 0);
float PlaneDistToPoint(const D3DXPLANE& plane, const glm::vec3& point);

const float floatErrComp = 0.00001f;
const D3DXMATRIX       IdentityMatrix(1.0f, 0.0f, 0.0f, 0.0f,
                                      0.0f, 1.0f, 0.0f, 0.0f,
                                      0.0f, 0.0f, 1.0f, 0.0f,
                                      0.0f, 0.0f, 0.0f, 1.0f);
const glm::vec3      XVector(1.0f, 0.0f, 0.0f);
const glm::vec3      YVector(0.0f, 1.0f, 0.0f);
const glm::vec3      ZVector(0.0f, 0.0f, 1.0f);
const glm::vec2        NullVec2(0.0f, 0.0f);
const glm::vec3      NullVector(0.0f, 0.0f, 0.0f);
const D3DXVECTOR4      NullVec4(0.0f, 0.0f, 0.0f, 0.0f);
const glm::vec2        IdentityVec2(1.0f, 1.0f);
const glm::vec3      IdentityVector(1.0f, 1.0f, 1.0f);
const D3DXVECTOR4      IdentityVec4(1.0f, 1.0f, 1.0f, 1.0f);
const glm::vec3      IdentityHalfVec (0.5f, 0.5f, 0.5f);
const glm::quat        NullQuaternion(1.0f, 0.0f, 0.0f, 0.0f);

template<class _Value> struct ValueRange
{
public:
	typedef ValueRange<_Value> MyClass;

	enum Distribution {vdLinear = 0, vdCubic, cDistributionEnd};
	static const char* cDistributionStr[cDistributionEnd];
private:
	_Value _min;
	_Value _max;
	Distribution _distrib;
public:
	ValueRange(): _distrib(vdLinear) {};
	ValueRange(const _Value& min, const _Value& max, Distribution distrib = vdLinear): _min(min), _max(max), _distrib(distrib) {}
	ValueRange(const _Value& value): _min(value), _max(value), _distrib(vdLinear) {}

	const _Value& GetMin() const
	{
		return _min;
	}
	void SetMin(const _Value& value)
	{
		_min = value;
	}

	const _Value& GetMax() const
	{
		return _max;
	};
	void SetMax(const _Value& value)
	{
		_max = value;
	}

	Distribution GetDistrib() const
	{
		return _distrib;
	}
	void SetDistrib(Distribution value)
	{
		_distrib = value;
	}

	//[0..1]
	_Value GetValue(float range) const
	{
		switch (_distrib)
		{
		case vdLinear:
			return _min + (_max - _min) * range;

		default:
			LSL_ASSERT(false);

			return _min;
		}
	}

	_Value GetValue() const
	{
		return GetValue(Random());
	}

	bool operator==(const MyClass& value) const
	{
		return _min == value._min && _max == value._max && _distrib == value._distrib;
	}
	bool operator!=(const MyClass& value) const
	{
		return !operator==(value);
	}
};

template<class _Value> const char* ValueRange<_Value>::cDistributionStr[ValueRange<_Value>::cDistributionEnd] = {"vdLinear", "vdCubic"};

struct Point2U
{
	Point2U() {}
	Point2U(unsigned mX, unsigned mY): x(mX), y(mY) {}

	bool operator==(const Point2U& value) const
	{
		return x == value.x && y == value.y;
	}
	bool operator!=(const Point2U& value) const
	{
		return !operator==(value);
	}

	operator unsigned*()
	{
		return &x;
	}
	operator const unsigned*() const
	{
		return &x;
	}

	unsigned x;
	unsigned y;
};
typedef Point2U UPoint;

struct Point3U
{
	Point3U() {}
	Point3U(unsigned mX, unsigned mY, unsigned mZ): x(mX), y(mY), z(mZ) {}

	bool operator==(const Point3U& value) const
	{
		return x == value.x && y == value.y && z == value.z;
	}
	bool operator!=(const Point3U& value) const
	{
		return !operator==(value);
	}

	operator unsigned*()
	{
		return &x;
	}
	operator const unsigned*() const
	{
		return &x;
	}

	unsigned x;
	unsigned y;
	unsigned z;
};

template<> struct ValueRange<glm::vec3>
{
public:
	typedef ValueRange<glm::vec3> MyClass;
	typedef glm::vec3 _Value;

	enum Distribution {vdLinear = 0, vdVolume, cDistributionEnd};
	static const char* cDistributionStr[cDistributionEnd];
private:
	_Value _min;
	_Value _max;
	Distribution _distrib;

	//Частота разделения по каждой оси (или число ячеек + 1)
	Point3U _freq;
	//Объем фигуры
	unsigned _volume;
	//Шаг в интерполирвоанном значении относительно каждой ячейки
	glm::vec3 _step;

	void CompVolume()
	{
		if (_distrib == vdVolume)
		{
			_volume = _freq.x * _freq.y * _freq.z;

			LSL_ASSERT(_volume > 0);

			glm::vec3 leng = _max - _min;
			_step.x = _freq.x > 1 ? leng.x / (_freq.x - 1) : 0;
			_step.y = _freq.y > 1 ? leng.y / (_freq.y - 1) : 0;
			_step.z = _freq.z > 1 ? leng.z / (_freq.z - 1) : 0;
		}
	}
public:
	ValueRange(): _distrib(vdLinear), _freq(100, 100, 100)
	{
	}
	ValueRange(const _Value& value): _min(value), _max(value), _distrib(vdLinear), _freq(100, 100, 100)
	{
	}
	ValueRange(const _Value& min, const _Value& max, Distribution distrib = vdLinear, const Point3U& freq = Point3U(100, 100, 100)): _min(min), _max(max), _distrib(distrib), _freq(freq)
	{
		CompVolume();
	}

	const _Value& GetMin() const
	{
		return _min;
	}
	void SetMin(const _Value& value)
	{
		_min = value;
		CompVolume();
	}

	const _Value& GetMax() const
	{
		return _max;
	};
	void SetMax(const _Value& value)
	{
		_max = value;
		CompVolume();
	}

	Distribution GetDistrib() const
	{
		return _distrib;
	}
	void SetDistrib(Distribution value)
	{
		_distrib = value;
		CompVolume();
	}

	const Point3U& GetFreq() const
	{
		return _freq;
	}
	void SetFreq(const Point3U& value)
	{
		_freq = value;
		CompVolume();
	}

	_Value GetValue(float range) const
	{
		switch (_distrib)
		{
		case vdLinear:
			return _min + (_max - _min) * range;

		case vdVolume:
		{
			//Вычисляем текущий номер ячейки
			//При range == 1 номер ячейки должен быть равным последнему
			unsigned num = range == 1 ? _volume - 1 : static_cast<unsigned>(_volume * range);

			//Разделяем номер ячейки на секции по осям
			Point3U cell;
			cell.x = num % _freq.x;
			cell.y = (num / _freq.x) % _freq.y;
			cell.z = (num / (_freq.x * _freq.y)) % _freq.z;

			//Итоговый результат
			glm::vec3 value;
			value.x = _min.x + _step.x * cell.x;
			value.y = _min.y + _step.y * cell.y;
			value.z = _min.z + _step.z * cell.z;

			return value;
		}

		default:
			LSL_ASSERT(false);

			return _min;
		}
	}
	_Value GetValue() const
	{
		return GetValue(Random());
	}

	void operator*=(float value)
	{
		_min *= value;
		_max *= value;
	}

	void operator*=(const glm::vec3& value)
	{
		_min *= value;
		_max *= value;
	}

	bool operator==(const MyClass& value) const
	{
		return _min == value._min && _max == value._max && _distrib == value._distrib && _freq == value._freq;
	}
	bool operator!=(const MyClass& value) const
	{
		return !operator==(value);
	}
};

typedef ValueRange<glm::vec3> Vec3Range;

Vec3Range operator*(const Vec3Range& val1, float val2);
Vec3Range operator*(const Vec3Range& val1, const glm::vec3& val2);

//Объемная интерполяция пока схожа к кубической, хотя на самом деле нужна сферическая. Когда поверхность сферы ограничивается окружностью(двумя радиус векторами, которые определяют димаетр окружности на манер как сделано в BB), которая разбивается на сектора через углы.
template<> struct ValueRange<glm::quat>
{
public:
	typedef ValueRange<glm::quat> MyClass;
	typedef glm::quat _Value;

	enum Distribution {vdLinear = 0, vdVolume, cDistributionEnd};
	static const char* cDistributionStr[cDistributionEnd];
private:
	_Value _min;
	_Value _max;
	Distribution _distrib;

	//Частота разделения по каждой оси (или число ячеек + 1)
	Point2U _freq;
	//Объем фигуры
	unsigned _volume;
	//Шаг в интерполирвоанном значении относительно каждой ячейки
	glm::vec2 _step;
	//
	glm::vec3 _minAngle;
	glm::vec3 _maxAngle;

	void CompVolume()
	{
		if (_distrib == vdVolume)
		{
			_volume = _freq.x * _freq.y;

			LSL_ASSERT(_volume > 0);

			glm::vec2 leng(_max.x - _min.x, _max.y - _min.y);
			_step.x = _freq.x > 1 ? leng.x / (_freq.x - 1) : 0;
			_step.y = _freq.y > 1 ? leng.y / (_freq.y - 1) : 0;

			_minAngle = glm::axis(_min);
			_minAngle.z = glm::angle(_min);
			//
			_maxAngle = glm::axis(_max);
			_maxAngle.z = glm::angle(_max);
		}
	}
public:
	ValueRange(): _distrib(vdLinear), _freq(100, 100)
	{
	}
	ValueRange(const _Value& value): _min(value), _max(value), _distrib(vdLinear), _freq(100, 100)
	{
	}
	ValueRange(const _Value& min, const _Value& max, Distribution distrib = vdLinear, const Point2U& freq = Point2U(100, 100)): _min(min), _max(max), _distrib(distrib), _freq(freq)
	{
		CompVolume();
	}

	const _Value& GetMin() const
	{
		return _min;
	}
	void SetMin(const _Value& value)
	{
		_min = value;
		CompVolume();
	}

	const _Value& GetMax() const
	{
		return _max;
	};
	void SetMax(const _Value& value)
	{
		_max = value;
		CompVolume();
	}

	Distribution GetDistrib() const
	{
		return _distrib;
	}
	void SetDistrib(Distribution value)
	{
		_distrib = value;
		CompVolume();
	}

	const Point2U& GetFreq() const
	{
		return _freq;
	}
	void SetFreq(const Point2U& value)
	{
		_freq = value;
		CompVolume();
	}

	_Value GetValue(float range) const
	{
		switch (_distrib)
		{
		case vdLinear:
		{
			_Value res = glm::mix(_min, _max, range);
			return res;
		}

		case vdVolume:
		{
			//Вычисляем текущий номер ячейки
			//При range == 1 номер ячейки должен быть равным последнему
			unsigned num = range == 1 ? _volume - 1 : static_cast<unsigned>(_volume * range);

			//Разделяем номер ячейки на секции по осям
			Point2U cell;
			cell.x = num % _freq.x;
			cell.y = (num / _freq.x) % _freq.y;

			//Итоговый результат
			glm::vec3 value;
			value.x = _min.x + _step.x * cell.x;
			value.y = _min.y + _step.y * cell.y;
			value.z = sqrt(std::max(1.0f - value.x * value.x - value.y * value.y, 0.0f));
			if (range > 0.5f)
				value.z = -value.z;

			glm::quat res = glm::angleAxis(_minAngle.z + (_maxAngle.z - _minAngle.z) * range, value);

			return res;
		}

		default:
			LSL_ASSERT(false);

			return _min;
		}
	}
	_Value GetValue() const
	{
		return GetValue(Random());
	}

	bool operator==(const MyClass& value) const
	{
		return _min == value._min && _max == value._max && _distrib == value._distrib && _freq == value._freq;
	}
	bool operator!=(const MyClass& value) const
	{
		return !operator==(value);
	}
};

typedef ValueRange<float> FloatRange;
typedef ValueRange<glm::vec2> Vec2Range;
typedef ValueRange<D3DXVECTOR4> Vec4Range;
typedef ValueRange<glm::quat> QuatRange;
typedef ValueRange<D3DXCOLOR> ColorRange;

//}

#include "lslMath.inl"

#endif