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

void MatrixRotationFromAxis(const D3DXVECTOR3& xVec, const D3DXVECTOR3& yVec, const D3DXVECTOR3& zVec, glm::mat4& matOut);
void MatrixSetTranslation(const D3DXVECTOR3& vec, glm::mat4& outMat);
void MatrixTranslate(const D3DXVECTOR3& vec, glm::mat4& outMat);
void MatrixSetScale(const D3DXVECTOR3& vec, glm::mat4& outMat);
void MatrixScale(const D3DXVECTOR3& vec, glm::mat4& outMat);
glm::vec2 MatGetPos(const glm::mat4 &mat);
void BuildWorldMatrix(const D3DXVECTOR3 &pos, const D3DXVECTOR3 &scale, const glm::quat &rot, glm::mat4 &outMat);
glm::mat4 BuildWorldMatrix(const D3DXVECTOR3 &pos, const D3DXVECTOR3 &scale, const glm::quat &rot);

glm::vec3 Vec3DxToGlm(D3DXVECTOR3 v3);
D3DXVECTOR3 Vec3GlmToDx(glm::vec3 v3);

//
glm::vec2 Vec2TransformCoord(const glm::vec2 &vec, const glm::mat4 &mat);
void Vec2NormCCW(const glm::vec2& vec2, glm::vec2& outVec);
glm::vec2 Vec2NormCCW(const glm::vec2& vec2);
void Vec2NormCW(const glm::vec2& vec2, glm::vec2& outVec);
//����� �������� vec1 �� vec2
float Vec2Proj(const glm::vec2& vec1, const glm::vec2& vec2);
void operator*=(glm::vec2& vec1, const glm::vec2& vec2);
glm::vec2 operator*(const glm::vec2& vec1, const glm::vec2& vec2);
void operator/=(glm::vec2& vec1, const glm::vec2& vec2);
glm::vec2 operator/(const glm::vec2& vec1, const glm::vec2& vec2);

D3DXVECTOR3 Vec3FromVec2(const glm::vec2& vec);
void Vec3Invert(const D3DXVECTOR3& vec, D3DXVECTOR3& rOut);
D3DXVECTOR3 Vec3Invert(const D3DXVECTOR3& vec);
void Vec3TransformNormal(const D3DXVECTOR3 &vec, const glm::mat4 &mat, D3DXVECTOR3 &outVec);
void Vec3TransformCoord(const D3DXVECTOR3 &vec, const glm::mat4 &mat, D3DXVECTOR3 &outVec);
D3DXVECTOR3 Vec3TransformCoord(const D3DXVECTOR3& vec, const glm::mat4& mat);
void Vec3Abs(const D3DXVECTOR3& vec, D3DXVECTOR3& rOut);
D3DXVECTOR3 Vec3Abs(const D3DXVECTOR3& vec);
void Vec3Rotate(const D3DXVECTOR3& v, const glm::quat& quat, D3DXVECTOR3& outVec);
void operator*=(D3DXVECTOR3& vec1, const D3DXVECTOR3& vec2);
D3DXVECTOR3 operator*(const D3DXVECTOR3& vec1, const D3DXVECTOR3& vec2);
void operator/=(D3DXVECTOR3& vec1, const D3DXVECTOR3& vec2);
D3DXVECTOR3 operator/(const D3DXVECTOR3& vec1, const D3DXVECTOR3& vec2);
bool operator>=(const D3DXVECTOR3& vec1, float scalar);
bool operator<(const D3DXVECTOR3& vec1, float scalar);
bool operator>(const D3DXVECTOR3& vec1, const D3DXVECTOR3& vec2);
bool operator<(const D3DXVECTOR3& vec1, const D3DXVECTOR3& vec2);

D3DXVECTOR4 Vec4FromVec2(const glm::vec2& vec);
D3DXVECTOR4 Vec4FromVec3(const D3DXVECTOR3& vec);

//����� �� ������� � �����
void Line2FromNorm(const glm::vec2& norm, const glm::vec2& point, D3DXVECTOR3& outLine);
D3DXVECTOR3 Line2FromNorm(const glm::vec2& norm, const glm::vec2& point);
//����� �� ������������ � �����
void Line2FromDir(const glm::vec2& dir, const glm::vec2& point, D3DXVECTOR3& outLine);
D3DXVECTOR3 Line2FromDir(const glm::vec2& dir, const glm::vec2& point);
void Line2GetNorm(const D3DXVECTOR3& line, glm::vec2& norm);
void Line2GetDir(const D3DXVECTOR3& line, glm::vec2& dir);
void Line2GetRadiusVec(const D3DXVECTOR3& line, glm::vec2& outVec);
glm::vec2 Line2GetRadiusVec(const D3DXVECTOR3& line);
glm::vec2 Line2GetNorm(const D3DXVECTOR3& line);
//���������� �� ������ �� ����� �� ����������� �������. ���� ����� ����� �� ������ �� ����������� ������� �� ��������� �������������, ����� �������������
float Line2DistToPoint(const D3DXVECTOR3& line, const glm::vec2& point);
//���������� ���������� ������ �� �����, ���� ������ � ����� ������ �����������
void Line2NormVecToPoint(const D3DXVECTOR3& line, const glm::vec2& point, glm::vec2& outNormVec);
glm::vec2 Line2NormVecToPoint(const D3DXVECTOR3& line, const glm::vec2& point);

bool RayCastIntersectSphere(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec, const D3DXVECTOR3& spherePos, float sphereRadius, float* t = 0);
float PlaneDistToPoint(const D3DXPLANE& plane, const D3DXVECTOR3& point);

const float floatErrComp = 0.00001f;
const glm::mat4       IdentityMatrix(1.0f, 0.0f, 0.0f, 0.0f,
                                      0.0f, 1.0f, 0.0f, 0.0f,
                                      0.0f, 0.0f, 1.0f, 0.0f,
                                      0.0f, 0.0f, 0.0f, 1.0f);
const D3DXVECTOR3      XVector(1.0f, 0.0f, 0.0f);
const D3DXVECTOR3      YVector(0.0f, 1.0f, 0.0f);
const D3DXVECTOR3      ZVector(0.0f, 0.0f, 1.0f);
const glm::vec2        NullVec2(0.0f, 0.0f);
const D3DXVECTOR3      NullVector(0.0f, 0.0f, 0.0f);
const D3DXVECTOR4      NullVec4(0.0f, 0.0f, 0.0f, 0.0f);
const glm::vec2        IdentityVec2(1.0f, 1.0f);
const D3DXVECTOR3      IdentityVector(1.0f, 1.0f, 1.0f);
const D3DXVECTOR4      IdentityVec4(1.0f, 1.0f, 1.0f, 1.0f);
const D3DXVECTOR3      IdentityHalfVec (0.5f, 0.5f, 0.5f);
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

	//������� ���������� �� ������ ��� (��� ����� ����� + 1)
	Point3U _freq;
	//����� ������
	unsigned _volume;
	//��� � ����������������� �������� ������������ ������ ������
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
			//��������� ������� ����� ������
			//��� range == 1 ����� ������ ������ ���� ������ ����������
			unsigned num = range == 1 ? _volume - 1 : static_cast<unsigned>(_volume * range);

			//��������� ����� ������ �� ������ �� ����
			Point3U cell;
			cell.x = num % _freq.x;
			cell.y = (num / _freq.x) % _freq.y;
			cell.z = (num / (_freq.x * _freq.y)) % _freq.z;

			//�������� ���������
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

//�������� ������������ ���� ����� � ����������, ���� �� ����� ���� ����� �����������. ����� ����������� ����� �������������� �����������(����� ������ ���������, ������� ���������� ������� ���������� �� ����� ��� ������� � BB), ������� ����������� �� ������� ����� ����.
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

	//������� ���������� �� ������ ��� (��� ����� ����� + 1)
	Point2U _freq;
	//����� ������
	unsigned _volume;
	//��� � ����������������� �������� ������������ ������ ������
	glm::vec2 _step;
	//
	D3DXVECTOR3 _minAngle;
	D3DXVECTOR3 _maxAngle;

	void CompVolume()
	{
		if (_distrib == vdVolume)
		{
			_volume = _freq.x * _freq.y;

			LSL_ASSERT(_volume > 0);

			glm::vec2 leng(_max.x - _min.x, _max.y - _min.y);
			_step.x = _freq.x > 1 ? leng.x / (_freq.x - 1) : 0;
			_step.y = _freq.y > 1 ? leng.y / (_freq.y - 1) : 0;

			_minAngle = Vec3GlmToDx(glm::axis(_min));
			_minAngle.z = glm::angle(_min);
			//
			_maxAngle = Vec3GlmToDx(glm::axis(_max));
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
			//��������� ������� ����� ������
			//��� range == 1 ����� ������ ������ ���� ������ ����������
			unsigned num = range == 1 ? _volume - 1 : static_cast<unsigned>(_volume * range);

			//��������� ����� ������ �� ������ �� ����
			Point2U cell;
			cell.x = num % _freq.x;
			cell.y = (num / _freq.x) % _freq.y;

			//�������� ���������
			D3DXVECTOR3 value;
			value.x = _min.x + _step.x * cell.x;
			value.y = _min.y + _step.y * cell.y;
			value.z = sqrt(std::max(1.0f - value.x * value.x - value.y * value.y, 0.0f));
			if (range > 0.5f)
				value.z = -value.z;

			glm::quat res = glm::angleAxis(_minAngle.z + (_maxAngle.z - _minAngle.z) * range, Vec3DxToGlm(value));

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