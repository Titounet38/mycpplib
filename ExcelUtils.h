#ifndef _ExcelUtils_
#define _ExcelUtils_

#include "windows.h"
#include <new>

#ifndef DllExport
#define DllExport   __declspec( dllexport )   
#endif

#define VBA_TRUE  ((short)(size_t)-1)
#define VBA_FALSE (0)
#define VBA_BOOL short

#define VARIANT_ERROR_NUL    2148141008		//#NULL!
#define VARIANT_ERROR_DIV0   2148141015		//#DIV/0!
#define VARIANT_ERROR_VALUE  2148141023		//#VALUE! 
#define VARIANT_ERROR_REF    2148141031		//#REF!
#define VARIANT_ERROR_NAME   2148141037		//#NAME?
#define VARIANT_ERROR_NUM    2148141044		//#NUM!
#define VARIANT_ERROR_NA     2148141050		//#N/A

namespace ExcelUtils {

	VARIANT MakeVariant_Double(double value);
	VARIANT MakeVariant_Error(int errorCode);
	VARIANT MakeVariant_String(const wchar_t * str);
	VARIANT MakeVariant_FakeString(const wchar_t * str);
	VARIANT MakeVariant_Bool(bool b);
	VARIANT MakeVariant_Missing();
	VARIANT MakeVariant_Empty();

	VARIANT ConvertToDoubleOrError(const VARIANT & v);
	VARIANT ConvertToBoolOrError(const VARIANT & v);
	VARIANT ConvertToLongOrError(const VARIANT & v);

	//VariantClear must be called on returned value before leaving the function! (except if the function returns it)
	//Use the VariantString class to have automatic Variant management for string conversion
	VARIANT ConvertToStringOrError(const VARIANT & v);
	VARIANT GetRangeNumberFormat(const VARIANT & v);
	VARIANT ConvertToDoubleOrErrorOrStringError(VARIANT & v);
	VARIANT ConvertToBoolOrErrorOrStringError(VARIANT & v);
	VARIANT ConvertToLongOrErrorOrStringError(VARIANT & v);

	bool IsError(const VARIANT & v);
	bool IsDouble(const VARIANT & v);
	bool IsString(const VARIANT & v);
	bool IsEmpty(const VARIANT & v);
	bool IsMissing(const VARIANT & v);
	bool IsLong(const VARIANT & v);
	bool IsBool(const VARIANT & v);
	bool IsNumeric(const VARIANT & v);

	bool SameString(const VARIANT & v1, const VARIANT & v2, bool caseSensitive = false);
	bool SameString(const VARIANT & v, const wchar_t * str, bool caseSensitive = false);
	bool SameString(const wchar_t * str, const VARIANT & v, bool caseSensitive = false);
	bool SameString(const wchar_t * str1, const wchar_t * str2, bool caseSensitive = false);

	class VariantString {
		VARIANT v;
		bool owner;

	public:

		~VariantString() {
			if (this->owner && this->v.vt != VT_EMPTY) VariantClear(&this->v);
		}

		VariantString() : owner(false) {
			VariantInit(&this->v);
		}

		VariantString& operator=(const VARIANT& v) {
			this->~VariantString();
			new (this) VariantString(v);
			return *this;
		}

		VariantString(const VARIANT& v) {
			if (v.vt == VT_DISPATCH) {
				this->v = ConvertToStringOrError(v);
				this->owner = true;
			}
			else if (v.vt & VT_BSTR) {
				this->v = v;
				this->owner = false;
			}
			else if (v.vt == VT_EMPTY) {
				this->v = v;
				this->v.vt = VT_BSTR;
				this->v.bstrVal = nullptr;
				this->owner = false;
			}
			else {
				this->v = MakeVariant_Error(VARIANT_ERROR_NA);
				this->owner = false;
			}
		}

		VARTYPE GetType() const {
			return this->v.vt;
		}

		bool IsError() const {
			return ExcelUtils::IsError(this->v);
		}

		bool IsEmpty() const {
			return ExcelUtils::IsEmpty(this->v);
		}

		bool IsMissing() const {
			return ExcelUtils::IsMissing(this->v);
		}

		bool AvailableString() {
			if (this->v.vt == VT_BSTR) return true;
			if (this->v.vt == (VT_BYREF | VT_BSTR)) return true;
			return false;
		}

		const wchar_t * GetString() const {
			if (this->v.vt == VT_BSTR) return this->v.bstrVal;
			if (this->v.vt == (VT_BYREF | VT_BSTR)) return *this->v.pbstrVal;
			else return L"# string conversion error #";
		}

		const VARIANT & GetVariant() const {
			return this->v;
		}

		VARIANT Detach() {

			VARIANT retVal;
			VariantInit(&retVal);

			if (this->v.vt & VT_BYREF) {
				auto r = VariantCopyInd(&retVal, &this->v);
				if (r != S_OK) {
					/* ? */
				}
				if (this->owner) {
					VariantClear(&this->v);
					this->owner = false;
				}
			}
			else {
				if (this->owner) {
					//Swap(this->v, retVal);
					auto tmp_swap = retVal;
					retVal = this->v;
					this->v = tmp_swap;

					this->owner = false;
				}
				else {
					auto r = VariantCopyInd(&retVal, &this->v);
					if (r != S_OK) {
						/* ? */
					}
				}
			}

			return retVal;
		}

		VariantString& WeakRef(VARIANT & v) {
			this->~VariantString();
			this->owner = false;
			this->v = v;
			return *this;
		}

		VariantString& TakeOwnership(VARIANT & v) {
			this->~VariantString();
			this->owner = true;
			this->v = v;
			return *this;
		}
	};

	inline const VARIANT & GetVariantOrString(const VARIANT & v) {
		return v;
	}

	inline const VARIANT & GetVariantOrString(const VariantString & v) {
		return v.GetVariant();
	}

	inline const wchar_t * GetVariantOrString(const wchar_t * str) {
		return str;
	}

	template <class T1, class T2>
	bool SameString(const T1& v1, const T2& v2, bool caseSensitive = false) {
		return SameString(GetVariantOrString(v1), GetVariantOrString(v2), caseSensitive);
	}

	bool IsDouble(const VariantString & v);
	bool IsError(const VariantString & v);

	extern const wchar_t g_valueName[];
	extern const wchar_t g_numberFormatName[];

	template <const wchar_t * str>
	VARIANT GetDispatchProperty(const VARIANT & v) {

		VARIANT retVal;
		VariantInit(&retVal);
		retVal.vt = VT_NULL;

		if ((v.vt & VT_TYPEMASK) == VT_DISPATCH) {

			static DISPID dispid;
			static bool once;
			static HRESULT hr_dispid;

			auto pdispVal = (v.vt & VT_BYREF) ? *v.ppdispVal : v.pdispVal;

			DISPPARAMS dispparamsNoArgs = { NULL, NULL, 0, 0 };
			BSTR name = (BSTR)str;

			if (!once) {
				hr_dispid = pdispVal->GetIDsOfNames(IID_NULL, &name, 1, NULL, &dispid);
				once = true;
			}

			HRESULT hr = hr_dispid;
			if (hr == S_OK) {
				hr = pdispVal->Invoke(dispid, IID_NULL, NULL, DISPATCH_PROPERTYGET, &dispparamsNoArgs, &retVal, NULL, NULL);
			}
		}

		return retVal;//MakeVariant_Error(VARIANT_ERROR_NA);
	}
}

#endif
