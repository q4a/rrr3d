#ifndef LSL_MATH
#define LSL_MATH

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
float ScalarTransform(float scalar, const D3DXVECTOR3& vec, const D3DXMATRIX& mat);

void BuildWorldMatrix(const D3DXVECTOR3& pos, const D3DXVECTOR3& scale, const D3DXQUATERNION& rot, D3DXMATRIX& outMat);
D3DXMATRIX BuildWorldMatrix(const D3DXVECTOR3& pos, const D3DXVECTOR3& scale, const D3DXQUATERNION& rot);
void MatrixRotationFromAxis(const D3DXVECTOR3& xVec, const D3DXVECTOR3& yVec, const D3DXVECTOR3& zVec, D3DXMATRIX& matOut);
void MatrixSetTranslation(const D3DXVECTOR3& vec, D3DXMATRIX& outMat);
void MatrixTranslate(const D3DXVECTOR3& vec, D3DXMATRIX& outMat);
void MatrixSetScale(const D3DXVECTOR3& vec, D3DXMATRIX& outMat);
void MatrixScale(const D3DXVECTOR3& vec, D3DXMATRIX& outMat);
void MatGetPos(const D3DXMATRIX& mat, D3DXVECTOR3& outPos);
D3DXVECTOR3 MatGetPos(const D3DXMATRIX& mat);

//
D3DXVECTOR2 Vec2TransformCoord(const D3DXVECTOR2& vec, const D3DXMATRIX& mat);
void Vec2NormCCW(const D3DXVECTOR2& vec2, D3DXVECTOR2& outVec);
D3DXVECTOR2 Vec2NormCCW(const D3DXVECTOR2& vec2);
void Vec2NormCW(const D3DXVECTOR2& vec2, D3DXVECTOR2& outVec);
//Длина проекции vec1 на vec2
float Vec2Proj(const D3DXVECTOR2& vec1, const D3DXVECTOR2& vec2);
void operator*=(D3DXVECTOR2& vec1, const D3DXVECTOR2& vec2);
D3DXVECTOR2 operator*(const D3DXVECTOR2& vec1, const D3DXVECTOR2& vec2);
void operator/=(D3DXVECTOR2& vec1, const D3DXVECTOR2& vec2);
D3DXVECTOR2 operator/(const D3DXVECTOR2& vec1, const D3DXVECTOR2& vec2);

D3DXVECTOR3 Vec3FromVec2(const D3DXVECTOR2& vec);
void Vec3Invert(const D3DXVECTOR3& vec, D3DXVECTOR3& rOut);
D3DXVECTOR3 Vec3Invert(const D3DXVECTOR3& vec);
D3DXVECTOR3 Vec3TransformCoord(const D3DXVECTOR3& vec, const D3DXMATRIX& mat);
void Vec3Abs(const D3DXVECTOR3& vec, D3DXVECTOR3& rOut);
D3DXVECTOR3 Vec3Abs(const D3DXVECTOR3& vec);
void Vec3Rotate(const D3DXVECTOR3& v, const D3DXQUATERNION& quat, D3DXVECTOR3& outVec);
void operator*=(D3DXVECTOR3& vec1, const D3DXVECTOR3& vec2);
D3DXVECTOR3 operator*(const D3DXVECTOR3& vec1, const D3DXVECTOR3& vec2);
void operator/=(D3DXVECTOR3& vec1, const D3DXVECTOR3& vec2);
D3DXVECTOR3 operator/(const D3DXVECTOR3& vec1, const D3DXVECTOR3& vec2);
bool operator>=(const D3DXVECTOR3& vec1, float scalar);
bool operator<(const D3DXVECTOR3& vec1, float scalar);
bool operator>(const D3DXVECTOR3& vec1, const D3DXVECTOR3& vec2);
bool operator<(const D3DXVECTOR3& vec1, const D3DXVECTOR3& vec2);

D3DXVECTOR4 Vec4FromVec2(const D3DXVECTOR2& vec);
D3DXVECTOR4 Vec4FromVec3(const D3DXVECTOR3& vec);

//Линия из нормали и точки
void Line2FromNorm(const D3DXVECTOR2& norm, const D3DXVECTOR2& point, D3DXVECTOR3& outLine);
D3DXVECTOR3 Line2FromNorm(const D3DXVECTOR2& norm, const D3DXVECTOR2& point);
//Линия из направляющей и точки
void Line2FromDir(const D3DXVECTOR2& dir, const D3DXVECTOR2& point, D3DXVECTOR3& outLine);
D3DXVECTOR3 Line2FromDir(const D3DXVECTOR2& dir, const D3DXVECTOR2& point);
void Line2GetNorm(const D3DXVECTOR3& line, D3DXVECTOR2& norm);
void Line2GetDir(const D3DXVECTOR3& line, D3DXVECTOR2& dir);
void Line2GetRadiusVec(const D3DXVECTOR3& line, D3DXVECTOR2& outVec);
D3DXVECTOR2 Line2GetRadiusVec(const D3DXVECTOR3& line);
D3DXVECTOR2 Line2GetNorm(const D3DXVECTOR3& line);
//Расстояние от прямой до точки по направлению нормали. Если точка лежит за линией по направлению нормали то результат положительный, иначе отрицательный
float Line2DistToPoint(const D3DXVECTOR3& line, const D3DXVECTOR2& point);
//Возвращает нормальный вектор до точки, этот вектор в общем случае неединичный
void Line2NormVecToPoint(const D3DXVECTOR3& line, const D3DXVECTOR2& point, D3DXVECTOR2& outNormVec);
D3DXVECTOR2 Line2NormVecToPoint(const D3DXVECTOR3& line, const D3DXVECTOR2& point);

bool RayCastIntersectSphere(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec, const D3DXVECTOR3& spherePos, float sphereRadius, float* t = 0);
float PlaneDistToPoint(const D3DXPLANE& plane, const D3DXVECTOR3& point);

const float floatErrComp = 0.00001f;
const D3DXMATRIX       IdentityMatrix(1.0f, 0.0f, 0.0f, 0.0f,
                                      0.0f, 1.0f, 0.0f, 0.0f,
                                      0.0f, 0.0f, 1.0f, 0.0f,
                                      0.0f, 0.0f, 0.0f, 1.0f);
const D3DXVECTOR3      XVector(1.0f, 0.0f, 0.0f);
const D3DXVECTOR3      YVector(0.0f, 1.0f, 0.0f);
const D3DXVECTOR3      ZVector(0.0f, 0.0f, 1.0f);
const D3DXVECTOR2      NullVec2(0.0f, 0.0f);
const D3DXVECTOR3      NullVector(0.0f, 0.0f, 0.0f);
const D3DXVECTOR4      NullVec4(0.0f, 0.0f, 0.0f, 0.0f);
const D3DXVECTOR2      IdentityVec2(1.0f, 1.0f);
const D3DXVECTOR3      IdentityVector(1.0f, 1.0f, 1.0f);
const D3DXVECTOR4      IdentityVec4(1.0f, 1.0f, 1.0f, 1.0f);
const D3DXVECTOR3      IdentityHalfVec (0.5f, 0.5f, 0.5f);
const D3DXQUATERNION   NullQuaternion(0.0f, 0.0f, 0.0f, 1.0f);

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

template<> struct ValueRange<D3DXVECTOR3>
{
public:
	typedef ValueRange<D3DXVECTOR3> MyClass;
	typedef D3DXVECTOR3 _Value;

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
	D3DXVECTOR3 _step;

	void CompVolume()
	{
		if (_distrib == vdVolume)
		{
			_volume = _freq.x * _freq.y * _freq.z;
			
			LSL_ASSERT(_volume > 0);
			
			D3DXVECTOR3 leng = _max - _min;
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
			D3DXVECTOR3 value;
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

	void operator*=(const D3DXVECTOR3& value)
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

typedef ValueRange<D3DXVECTOR3> Vec3Range;

Vec3Range operator*(const Vec3Range& val1, float val2);
Vec3Range operator*(const Vec3Range& val1, const D3DXVECTOR3& val2);

//Объемная интерполяция пока схожа к кубической, хотя на самом деле нужна сферическая. Когда поверхность сферы ограничивается окружностью(двумя радиус векторами, которые определяют димаетр окружности на манер как сделано в BB), которая разбивается на сектора через углы. 
template<> struct ValueRange<D3DXQUATERNION>
{
public:
	typedef ValueRange<D3DXQUATERNION> MyClass;
	typedef D3DXQUATERNION _Value;

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
	D3DXVECTOR2 _step;
	//
	D3DXVECTOR3 _minAngle;
	D3DXVECTOR3 _maxAngle;

	void CompVolume()
	{
		if (_distrib == vdVolume)
		{
			_volume = _freq.x * _freq.y;
			
			LSL_ASSERT(_volume > 0);
			
			D3DXVECTOR2 leng = _max - _min;
			_step.x = _freq.x > 1 ? leng.x / (_freq.x - 1) : 0;
			_step.y = _freq.y > 1 ? leng.y / (_freq.y - 1) : 0;

			float tmp;
			D3DXQuaternionToAxisAngle(&_min, &_minAngle, &tmp);
			_minAngle.z = tmp;
			//
			D3DXQuaternionToAxisAngle(&_max, &_maxAngle, &tmp);
			_maxAngle.z = tmp;
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
			_Value res;
			D3DXQuaternionSlerp(&res, &_min, &_max, range);
			
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
			D3DXVECTOR3 value;
			value.x = _min.x + _step.x * cell.x;
			value.y = _min.y + _step.y * cell.y;
			value.z = sqrt(std::max(1.0f - value.x * value.x - value.y * value.y, 0.0f));
			if (range > 0.5f)
				value.z = -value.z;

			D3DXQUATERNION res;
			D3DXQuaternionRotationAxis(&res, &value, _minAngle.z + (_maxAngle.z - _minAngle.z) * range);

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
typedef ValueRange<D3DXVECTOR2> Vec2Range;
typedef ValueRange<D3DXVECTOR4> Vec4Range;
typedef ValueRange<D3DXQUATERNION> QuatRange;
typedef ValueRange<D3DXCOLOR> ColorRange;

//}

#include "lslMath.inl"

#endif