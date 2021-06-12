#ifndef LSL_SERIALVALUE
#define LSL_SERIALVALUE

#include "lslCommon.h"
#include "lslSerialization.h"
#include "lslMath.h"

namespace lsl
{

template<class _Value> SWriter* SWriteValueRange(SWriter* writer, const char* name, const ValueRange<_Value>& value);
template<class _Value> SReader* SReadValueRange(SReader* reader, const char* name, ValueRange<_Value>& value);

template<class _Value> struct SerialValue
{
	static void Write(SWriter* writer, const char* name, const _Value& value)
	{
		writer->WriteValue(name, value);
	}
	static SReader* Read(SReader* reader, const char* name, _Value& value)
	{
		return reader->ReadValue(name, value);
	}
};

template<> struct SerialValue<D3DXVECTOR2>
{
	typedef D3DXVECTOR2 _Value;

	static void Write(SWriter* writer, const char* name, const _Value& value)
	{
		writer->WriteValue(name, value, 2);
	}
	static SReader* Read(SReader* reader, const char* name, _Value& value)
	{
		return reader->ReadValue(name, value, 2);
	}
};

template<> struct SerialValue<D3DXVECTOR3>
{
	typedef D3DXVECTOR3 _Value;

	static void Write(SWriter* writer, const char* name, const _Value& value)
	{
		writer->WriteValue(name, value, 3);
	}
	static SReader* Read(SReader* reader, const char* name, _Value& value)
	{
		return reader->ReadValue(name, value, 3);
	}
};

template<> struct SerialValue<D3DXVECTOR4>
{
	typedef D3DXVECTOR4 _Value;

	static void Write(SWriter* writer, const char* name, const _Value& value)
	{
		writer->WriteValue(name, value, 4);
	}
	static SReader* Read(SReader* reader, const char* name, _Value& value)
	{
		return reader->ReadValue(name, value, 4);
	}
};

template<> struct SerialValue<D3DXQUATERNION>
{
	typedef D3DXQUATERNION _Value;

	static void Write(SWriter* writer, const char* name, const _Value& value)
	{
		writer->WriteValue(name, value, 4);
	}
	static SReader* Read(SReader* reader, const char* name, _Value& value)
	{
		return reader->ReadValue(name, value, 4);
	}
};

template<> struct SerialValue<D3DXCOLOR>
{
	typedef D3DXCOLOR _Value;

	static void Write(SWriter* writer, const char* name, const _Value& value)
	{
		writer->WriteValue(name, value, 4);
	}
	static SReader* Read(SReader* reader, const char* name, _Value& value)
	{
		return reader->ReadValue(name, value, 4);
	}
};

template<> struct SerialValue<Point2U>
{
	typedef Point2U _Value;

	static void Write(SWriter* writer, const char* name, const _Value& value)
	{
		writer->WriteValue(name, value, 2);
	}
	static SReader* Read(SReader* reader, const char* name, _Value& value)
	{
		return reader->ReadValue(name, value, 2);
	}
};

template<> struct SerialValue<Point>
{
	typedef Point _Value;

	static void Write(SWriter* writer, const char* name, const _Value& value)
	{
		writer->WriteValue(name, value, 2);
	}
	static SReader* Read(SReader* reader, const char* name, _Value& value)
	{
		return reader->ReadValue(name, value, 2);
	}
};

template<> struct SerialValue<Point3U>
{
	typedef Point3U _Value;

	static void Write(SWriter* writer, const char* name, const _Value& value)
	{
		writer->WriteValue(name, value, 3);
	}
	static SReader* Read(SReader* reader, const char* name, _Value& value)
	{
		return reader->ReadValue(name, value, 3);
	}
};

template<class _Value> struct SerialValue<ValueRange<_Value>>
{
	typedef ValueRange<_Value> MyRange;
	typedef SerialValue<_Value> MyVal;

	static void Write(SWriter* writer, const char* name, const MyRange& value)
	{
		SWriteValueRange(writer, name, value);
	}

	static SReader* Read(SReader* reader, const char* name, MyRange& value)
	{
		return SReadValueRange(reader, name, value);
	}
};

template<> struct SerialValue<ValueRange<D3DXVECTOR3>>
{
	typedef D3DXVECTOR3 _Value;
	typedef ValueRange<_Value> MyRange;
	typedef SerialValue<Point3U> MyPoint3U;

	static void Write(SWriter* writer, const char* name, const MyRange& value)
	{
		SWriter* child = SWriteValueRange(writer, name, value);

		MyPoint3U::Write(child, "freq", value.GetFreq());
	}

	static SReader* Read(SReader* reader, const char* name, MyRange& value)
	{
		SReader* child = SReadValueRange(reader, name, value);

		if (child)
		{
			Point3U tmp;
			MyPoint3U::Read(child, "freq", tmp);
			value.SetFreq(tmp);
		}

		return child;
	}
};

template<> struct SerialValue<ValueRange<D3DXQUATERNION>>
{
	typedef D3DXQUATERNION _Value;
	typedef ValueRange<_Value> MyRange;
	typedef SerialValue<Point2U> MyPoint2U;

	static void Write(SWriter* writer, const char* name, const MyRange& value)
	{
		SWriter* child = SWriteValueRange(writer, name, value);

		MyPoint2U::Write(child, "freq", value.GetFreq());
	}

	static SReader* Read(SReader* reader, const char* name, MyRange& value)
	{
		SReader* child = SReadValueRange(reader, name, value);

		if (child)
		{
			Point2U tmp;
			MyPoint2U::Read(child, "freq", tmp);
			value.SetFreq(tmp);
		}

		return child;
	}
};




inline lsl::SWriter* SWriteEnum(lsl::SWriter* writer, const char* name, int enumVal, const char* enumStr[], int enumEnd)
{
	LSL_ASSERT(enumVal < enumEnd);

	return writer->WriteValue(name, enumStr[enumVal]);
}

inline lsl::SWriter* SWriteEnum(lsl::SWriter* writer, const char* name, int enumVal, const lsl::string enumStr[], int enumEnd)
{
	LSL_ASSERT(enumVal < enumEnd);

	return writer->WriteValue(name, enumStr[enumVal]);
}

template<class _Enum> lsl::SReader* SReadEnum(lsl::SReader* reader, const char* name, _Enum& enumVal, const char* enumStr[], int enumEnd)
{
	std::string str;
	if (reader->ReadValue(name, str))
	{
		int val = lsl::ConvStrToEnum(str.c_str(), enumStr, enumEnd);
		if (val > -1)
		{
			LSL_ASSERT(val < enumEnd);

			enumVal = _Enum(val);

			return reader;
		}
	}

	return 0;
}

template<class _Enum> lsl::SReader* SReadEnum(lsl::SReader* reader, const char* name, _Enum& enumVal, const lsl::string enumStr[], int enumEnd)
{
	std::string str;
	if (reader->ReadValue(name, str))
	{
		int val = lsl::ConvStrToEnum(str.c_str(), enumStr, enumEnd);
		if (val > -1)
		{
			LSL_ASSERT(val < enumEnd);

			enumVal = _Enum(val);

			return reader;
		}
	}

	return 0;
}

template<class _Value> SWriter* SWriteValueRange(SWriter* writer, const char* name, const ValueRange<_Value>& value)
{
	typedef ValueRange<_Value> MyRange;
	typedef SerialValue<_Value> MyVal;

	SWriter* child = writer->NewDummyNode(name);
		
	MyVal::Write(child, "min", value.GetMin());
	MyVal::Write(child, "max", value.GetMax());
	child->WriteValue("distrib", MyRange::cDistributionStr[value.GetDistrib()]);

	return child;
}

template<class _Value> SReader* SReadValueRange(SReader* reader, const char* name, ValueRange<_Value>& value)
{
	typedef ValueRange<_Value> MyRange;
	typedef SerialValue<_Value> MyVal;

	SReader* child = reader->ReadValue(name);
	if (child)
	{
		_Value tmp;
		std::string str;
		
		if (MyVal::Read(child, "min", tmp))
			value.SetMin(tmp);
		if (MyVal::Read(child, "max", tmp))
			value.SetMax(tmp);
		
		if (child->ReadValue("distrib", str))
		{
			int res = lsl::ConvStrToEnum(str.c_str(), MyRange::cDistributionStr, MyRange::cDistributionEnd);
			if (res > -1)
				value.SetDistrib(MyRange::Distribution(res));
		}
		return child;
	}
	return 0;
}

template<class _Value> void SWriteValue(SWriter* writer, const char* name, const _Value& value)
{
	SerialValue<_Value>::Write(writer, name, value);
}

template<class _Value> SReader* SReadValue(SReader* reader, const char* name, _Value& value)
{
	return SerialValue<_Value>::Read(reader, name, value);
}

}

#endif