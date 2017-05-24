#define _CRT_SECURE_NO_WARNINGS

#include "windows.h"
#include "shlwapi.h"
#include "ExcelUtils.h"

namespace ExcelUtils {

	extern const wchar_t g_valueName[] = L"Value";
	extern const wchar_t g_numberFormatName[] = L"NumberFormat";

	VARIANT MakeVariant_Double(double value) {

		VARIANT retVal;
		VariantInit(&retVal);

		retVal.vt = VT_R8;
		retVal.dblVal = value;

		return retVal;
	}

	VARIANT MakeVariant_Error(int errorCode) {

		VARIANT retVal;
		VariantInit(&retVal);

		retVal.vt = VT_ERROR;
		retVal.scode = errorCode;

		return retVal;
	}

	VARIANT MakeVariant_Empty() {

		VARIANT retVal;
		VariantInit(&retVal);

		return retVal;
	}

	VARIANT MakeVariant_FakeString(const wchar_t * str) {

		VARIANT retVal;

		retVal.vt = VT_BSTR;
		retVal.bstrVal = (BSTR)str;

		return retVal;
	}

	VARIANT MakeVariant_String(const wchar_t * str) {

		VARIANT retVal;
		VariantInit(&retVal);

		retVal.vt = VT_BSTR;
		retVal.bstrVal = str ? SysAllocString(str) : (wchar_t*)str;

		return retVal;
	}

	VARIANT MakeVariant_Bool(bool b) {

		VARIANT retVal;
		VariantInit(&retVal);

		retVal.vt = VT_BOOL;
		retVal.boolVal = b ? VARIANT_TRUE : VARIANT_FALSE;

		return retVal;
	}

	VARIANT GetRangeNumberFormat(const VARIANT & v) {
		return GetDispatchProperty<g_numberFormatName>(v);
	}

	VARIANT ConvertToTypeOrError(const VARIANT & v, VARENUM type) {

		if (v.vt == VT_ERROR) return v;

		VARIANT vConv;
		VariantInit(&vConv);

		auto vt = v.vt & VT_TYPEMASK;

		if (vt == type) {
			switch (vt) {
			case VT_EMPTY:
			case VT_NULL:
			case VT_I2:
			case VT_I4:
			case VT_R4:
			case VT_R8:
			case VT_CY:
			case VT_DATE:
			case VT_ERROR:
			case VT_BOOL:
			case VT_I1:
			case VT_UI1:
			case VT_UI2:
			case VT_UI4:
			case VT_I8:
			case VT_UI8:
			case VT_INT:
			case VT_UINT:
			case VT_HRESULT:
			case VT_FILETIME: {
				struct PlaceHolder {
					char bytes[8];
				};
				auto pDest = (PlaceHolder*)&vConv.intVal;
				PlaceHolder* pSrc;
				if (v.vt & VT_BYREF) pSrc = (PlaceHolder*)v.pintVal;
				else pSrc = (PlaceHolder*)&v.intVal;
				vConv.vt = vt;
				*pDest = *pSrc;
				return vConv;
			}
			}

			auto hr = VariantCopyInd(&vConv, &v);

			if (hr == S_OK) {
				return vConv;
			}
		}

		if (vt == VT_DISPATCH) {
			vConv = GetDispatchProperty<g_valueName>(v);
			if (vConv.vt == type) return vConv;
		}

		auto hr = VariantChangeType(&vConv, &v, 0, type);

		if (hr == S_OK) {
			return vConv;
		}

		return MakeVariant_Error(VARIANT_ERROR_NA);
	}

	VARIANT ConvertToTypeOrErrorOrStringError(const VARIANT & v, VARENUM type) {

		VARIANT vConv = ConvertToTypeOrError(v, type);

		if (vConv.vt == VT_ERROR) {
			VariantString v_str = v;
			if (v_str.AvailableString()) {
				auto s = v_str.GetString();
				if (s && s[0] == L'#') {
					return v_str.Detach();
				}
			}
		}

		return vConv;
	}

	VARIANT ConvertToBoolOrError(const VARIANT & v) {
		return ConvertToTypeOrError(v, VT_BOOL);
	}

	VARIANT ConvertToDoubleOrError(const VARIANT & v) {
		return ConvertToTypeOrError(v, VT_R8);
	}

	VARIANT ConvertToStringOrError(const VARIANT & v) {
		return ConvertToTypeOrError(v, VT_BSTR);
	}

	VARIANT ConvertToLongOrError(const VARIANT & v) {
		return ConvertToTypeOrError(v, VT_I4);
	}

	VARIANT ConvertToDoubleOrErrorOrStringError(VARIANT & v) {
		return ConvertToTypeOrErrorOrStringError(v, VT_R8);
	}

	VARIANT ConvertToBoolOrErrorOrStringError(VARIANT & v) {
		return ConvertToTypeOrErrorOrStringError(v, VT_BOOL);
	}

	VARIANT ConvertToLongOrErrorOrStringError(VARIANT & v) {
		return ConvertToTypeOrErrorOrStringError(v, VT_I4);
	}

	VARIANT MakeVariant_Missing() {
		return MakeVariant_Error(DISP_E_PARAMNOTFOUND);
	}

	bool IsMissing(const VARIANT & v) {
		return v.vt == VT_ERROR && v.scode == DISP_E_PARAMNOTFOUND;
	}

	bool IsError(const VARIANT & v) {
		return v.vt == VT_ERROR;
	}

	bool IsDouble(const VARIANT & v) {
		return v.vt == VT_R8;
	}

	bool IsString(const VARIANT & v) {
		return v.vt == VT_BSTR;
	}

	bool IsBool(const VARIANT & v) {
		return v.vt == VT_BOOL;
	}

	bool IsLong(const VARIANT & v) {
		return v.vt == VT_I4;
	}

	bool IsError(const VariantString & v) {
		return v.GetType() == VT_ERROR;
	}

	bool IsDouble(const VariantString & v) {
		return v.GetType() == VT_R8;
	}

	bool IsEmpty(const VARIANT & v) {
		return v.vt == VT_EMPTY;
	}

	bool IsNumeric(const VARIANT & v) {

		auto vt = v.vt & VT_TYPEMASK;

		switch (vt) {
		case VT_I2:
		case VT_I4:
		case VT_R4:
		case VT_R8:
		case VT_CY:
		case VT_DATE:
		case VT_BOOL:
		case VT_I1:
		case VT_UI1:
		case VT_UI2:
		case VT_UI4:
		case VT_I8:
		case VT_UI8:
		case VT_INT:
		case VT_UINT:
			return true;
		}

		return false;
	}

	bool SameString(const wchar_t * str1, const wchar_t * str2, bool caseSensitive) {
		if (!str1 && !str2) return true;
		if (!str1 || !str2) return false;

		return caseSensitive ? wcscmp(str1, str2) == 0 : _wcsicmp(str1, str2) == 0;
	}

	bool SameString(wchar_t * str1, wchar_t * str2, bool caseSensitive) {
		return SameString((const wchar_t *)str1, (const wchar_t *)str2, caseSensitive);
	}

	bool SameString(const wchar_t * str1, wchar_t * str2, bool caseSensitive) {
		return SameString((const wchar_t *)str1, (const wchar_t *)str2, caseSensitive);
	}

	bool SameString(wchar_t * str1, const wchar_t * str2, bool caseSensitive) {
		return SameString((const wchar_t *)str1, (const wchar_t *)str2, caseSensitive);
	}

	__declspec(noinline)
		bool Convert1stArgAndRecurse(const VARIANT & v1, const VARIANT & v2, bool caseSensitive) {
		VARIANT vConv = ConvertToStringOrError(v1);
		bool retVal = SameString(static_cast<const VARIANT>(vConv), v2, caseSensitive);
		VariantClear(&vConv);
		return retVal;
	}

	bool SameString(const VARIANT & v1, const VARIANT & v2, bool caseSensitive) {

		if (v1.vt == VT_DISPATCH) return Convert1stArgAndRecurse(v1, v2, caseSensitive);
		if (v2.vt == VT_DISPATCH) return Convert1stArgAndRecurse(v2, v1, caseSensitive);

		if (v1.vt != VT_BSTR || v2.vt != VT_BSTR) return false;

		return SameString(v1.bstrVal, v2.bstrVal, caseSensitive);
	}

	bool SameString(const VARIANT & v, const wchar_t * str, bool caseSensitive) {

		VARIANT v2;
		v2.vt = VT_BSTR;
		v2.bstrVal = (wchar_t*)str;

		return SameString(v, static_cast<const VARIANT>(v2), caseSensitive);
	}

	bool SameString(const wchar_t * str, const VARIANT & v, bool caseSensitive) {
		return SameString(v, str, caseSensitive);
	}

	__declspec(dllexport)
		void __stdcall Dll_SwapVariant(VARIANT & v1, VARIANT & v2) {
		VARIANT tmp = v1;
		v1 = v2;
		v2 = tmp;
	}
}
