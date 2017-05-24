
#define _CRT_SECURE_NO_WARNINGS

#include "ExcelUtils.h"
#include "ExcelUtilsX.h"
#include "XlUnitSupport.h"
#include "WinUtils.h"
#include "Converter.h"
#include "Oleacc.h"
#include "comutil.h"
#include "XLCALL.H"

namespace XlUnitSupport {

	struct VariantHolder {

		VARIANT v;
		bool initialized = false;

		VariantHolder(bool init = true) {
			VariantInit(&this->v);
			this->initialized = true;
		}

		~VariantHolder() {
			if (this->initialized) VariantClear(&this->v);
		}
	};

	bool IsNumber(ExcelUtils::VariantString & vs) {
		if (vs.IsError()) return false;
		if (vs.AvailableString() && ::IsNumber(vs.GetString())) return true;
		return false;
	}

	template <bool strict, bool ignoreFromCellAndGetCallerRange = false, int N>
	VARIANT GetUnitFromInputs(char(&c)[N], VARIANT & vFromCell, VARIANT * vUnit = nullptr) {

		VariantHolder tmp(false);
		
		VARIANT * vPtr = nullptr;
		ExcelUtils::VariantString vs_unit;

		bool cond = false;

		if (!vUnit) {
			cond = true;
		} else {
			if (vUnit->vt == VT_DISPATCH) {
				tmp.v = ExcelUtils::GetDispatchProperty<ExcelUtils::g_valueName>(*vUnit);
				tmp.initialized = true;
				vPtr = &tmp.v;
			} else {
				vPtr = vUnit;
			}

			if (strict) {
				cond = ExcelUtils::IsMissing(*vPtr);
			} else {
				cond = ExcelUtils::IsError(*vPtr) || ExcelUtils::IsNumeric(*vPtr);
			}
		}

		const wchar_t * vs_unit_ptr;

		if (cond) {
			VARIANT numFormat;
			if (ignoreFromCellAndGetCallerRange) {
				auto callerRange = GetCallerRange();
				numFormat = ExcelUtils::GetRangeNumberFormat(callerRange);
				VariantClear(&callerRange);
			} else {
				if ((vFromCell.vt & VT_TYPEMASK) == VT_DISPATCH) {
					numFormat = ExcelUtils::GetRangeNumberFormat(vFromCell);
				} else {
					numFormat.vt = VT_BSTR;
					numFormat.bstrVal = NULL;
				}
			}

			if (numFormat.vt == VT_BSTR) {
				WCharPointer str = numFormat.bstrVal;
				//RetrieveStr<L'"', L' '>(str);
				RetrieveStr<L'"'>(str);
				WCharPointer str2 = RetrieveStr<L'"', L' '>(str);
				str2.Trim();
				vs_unit_ptr = str2;
				vs_unit.TakeOwnership(numFormat);
			} else if (numFormat.vt == VT_NULL) {
				vs_unit_ptr = nullptr;
			} else {
				VariantClear(&numFormat);
				if (ExcelUtils::IsError(numFormat)) return numFormat;
				return ExcelUtils::MakeVariant_Error(VARIANT_ERROR_NA);
			}
		} else {
			vs_unit = *vPtr;
			if (vs_unit.AvailableString()) {
				vs_unit_ptr = vs_unit.GetString();
			} else {
				if (vs_unit.IsError()) return vs_unit.GetVariant();
				return ExcelUtils::MakeVariant_Error(VARIANT_ERROR_NA);
			}
		}

		if (vs_unit_ptr) sprintf_s(c, "%S", vs_unit_ptr);
		else c[0] = 0;

		return ExcelUtils::MakeVariant_Empty();
	}

	LPDISPATCH GetXlAppPtr() {

		static bool done;
		static VariantHolder xlAppGuard;

		LPDISPATCH retVal = nullptr;

		if (done) {
			retVal = xlAppGuard.v.pdispVal;
		} else {
			XLOPER12 xRes;
			auto r = Excel12(xlGetHwnd, &xRes, 0);

			if (r == xlretSuccess) {

				if (xRes.xltype == xltypeInt) {
					auto h = (HWND)xRes.val.w;
					auto hDesk = FindWindowExA(h, NULL, "XLDESK", NULL);
					auto hXl = FindWindowExA(hDesk, NULL, "EXCEL7", NULL);

					if (hXl) {

						LPDISPATCH xlPtr;
						auto ridd = __uuidof(IDispatch);
						auto hr = AccessibleObjectFromWindow(hXl, OBJID_NATIVEOM, ridd, (void**)&xlPtr);

						if (SUCCEEDED(hr)) {

							BSTR name = L"Application";
							DISPID dispid;
							DISPPARAMS dispparamsNoArgs = { NULL, NULL, 0, 0 };

							hr = xlPtr->GetIDsOfNames(IID_NULL, &name, 1, NULL, &dispid);

							if (SUCCEEDED(hr)) {

								auto&& xlApp = xlAppGuard.v;

								hr = xlPtr->Invoke(dispid, IID_NULL, NULL, DISPATCH_PROPERTYGET, &dispparamsNoArgs, &xlApp, NULL, NULL);

								if (SUCCEEDED(hr)) {

									if (xlApp.vt == VT_DISPATCH) {
										done = true;
										retVal = xlApp.pdispVal;
									}
								}
							}
							xlPtr->Release();
						}
					}
					Excel12(xlFree, 0, 1, &xRes);
				}
			}
		}
		return retVal;
	}

	VARIANT GetCallerRange() {

		VARIANT retVal;

		VariantInit(&retVal);

		if (auto xlApp = GetXlAppPtr()) {

			static DISPID dispid;
			static bool once;
			static HRESULT once_hr;
			BSTR name = L"Caller";

			if (!once || FAILED(once_hr)) {
				once = true;
				once_hr = xlApp->GetIDsOfNames(IID_NULL, &name, 1, NULL, &dispid);
			}

			if (SUCCEEDED(once_hr)) {
				VARIANT xlRange;
				VariantInit(&xlRange);

				DISPPARAMS dispparamsNoArgs = { NULL, NULL, 0, 0 };
				auto hr = xlApp->Invoke(dispid, IID_NULL, NULL, DISPATCH_PROPERTYGET, &dispparamsNoArgs, &xlRange, NULL, NULL);
				if (SUCCEEDED(hr)) {
					if (xlRange.vt == VT_DISPATCH) {
						retVal = xlRange;
					}
				}
			}
		}

		return retVal;
	}

	VARIANT ConvertFromSI(VARIANT & v, VARIANT & v_To, bool strict) {

		VARIANT vConv = ExcelUtils::ConvertToDoubleOrErrorOrStringError(v);
		if (!ExcelUtils::IsDouble(vConv)) return vConv;

		auto val = vConv.dblVal;

		char unit[500];
		auto ok = GetUnitFromInputs<true, true>(unit, v, &v_To);
		if (ExcelUtils::IsError(ok)) return ok;

		if (unit[0] == 0) return ExcelUtils::MakeVariant_Double(val);

		static UnitConverter converter;

		auto convertedValue = converter.To(unit, strict).FromSI().Convert(val);
		if (auto error = converter.GetError()) {
			return ExcelUtils::MakeVariant_String(xFormat(L"# %S #", error));
		} else {
			return ExcelUtils::MakeVariant_Double(convertedValue);
		}
	} 

	 VARIANT ConvertToSI(VARIANT & v, VARIANT & v_From, bool strict) {

		VARIANT vConv = ExcelUtils::ConvertToDoubleOrErrorOrStringError(v);
		if (vConv.vt != VT_R8) return vConv;

		auto val = vConv.dblVal;

		char unit[500];
		auto ok = GetUnitFromInputs<true>(unit, v, &v_From);
		if (ExcelUtils::IsError(ok)) return ok;

		if (unit[0] == 0) return ExcelUtils::MakeVariant_Double(val);

		static UnitConverter converter;

		auto convertedValue = converter.From(unit, strict != 0).ToSI().Convert(val);
		if (auto error = converter.GetError()) {
			return ExcelUtils::MakeVariant_String(xFormat(L"# %S #", error));
		} else {
			return ExcelUtils::MakeVariant_Double(convertedValue);
		}
	}

	VARIANT GetType(VARIANT & v_Unit) {

		char unit[500];
		auto ok = GetUnitFromInputs<false>(unit, v_Unit, &v_Unit);
		if (ExcelUtils::IsError(ok)) return ok;

		static UnitConverterCore converter;

		auto pv = converter.ToSI(ValueAndUnit(1, unit));
		if (pv.error) {
			return ExcelUtils::MakeVariant_String(L"# unrecognized unit #");
		} else {
			DimensionTypesEx dimTypes;
			auto dimInfo = dimTypes.GetDimensionName(pv.dim);
			if (dimInfo.type) {
				return ExcelUtils::MakeVariant_String(xFormat(L"%S", dimInfo.type));
			} else {
				return ExcelUtils::MakeVariant_String(nullptr);
			}
		}
	}

	VARIANT GetInterpretation(VARIANT & v_Unit) {

		char unit[500];
		auto ok = GetUnitFromInputs<false>(unit, v_Unit, &v_Unit);
		if (ExcelUtils::IsError(ok)) return ok;

		static UnitConverterCore converter;

		auto pv = converter.ToSI(ValueAndUnit(1, unit));
		if (pv.error) {
			return ExcelUtils::MakeVariant_String(L"# unrecognized unit #");
		} else {
			return ExcelUtils::MakeVariant_String(xFormat(L"%S", pv.orig_unitStrFullNamePlural));
		}
	}

	VARIANT GetDimensions(VARIANT & v_Unit) {

		char unit[500];
		auto ok = GetUnitFromInputs<false>(unit, v_Unit, &v_Unit);
		if (ExcelUtils::IsError(ok)) return ok;

		static UnitConverterCore converter;

		auto pv = converter.ToSI(ValueAndUnit(1, unit));
		if (pv.error) {
			return ExcelUtils::MakeVariant_String(L"# unrecognized unit #");
		} else {
			return ExcelUtils::MakeVariant_String(xFormat(L"%S", pv.dim.ToString()));
		}
	}

	VARIANT Convert(
		VARIANT & v, 
		VARIANT & v_From, 
		VARIANT & v_To,
		bool strict,
		Convert_MissingBehavior missingBehavior_from,
		Convert_MissingBehavior missingBehavior_to)
	{

		VARIANT vConv = ExcelUtils::ConvertToDoubleOrErrorOrStringError(v);
		if (vConv.vt != VT_R8) return vConv;

		auto val = vConv.dblVal;

		VARIANT ok;
		char unit_from[500];
		ok = GetUnitFromInputs<true>(unit_from, v, &v_From);
		if (ExcelUtils::IsError(ok)) return ok;

		char unit_to[500];
		ok = GetUnitFromInputs<true, true>(unit_to, v, &v_To);
		if (ExcelUtils::IsError(ok)) return ok;

		static UnitConverter converter;

		double convertedValue;

		bool missing_to   = unit_to[0] == 0;
		bool missing_from = unit_from[0] == 0;

		if (!missing_to && !missing_from) {
do_full_conversion:
			convertedValue = converter.SetUnits(unit_from, unit_to, strict != 0).Convert(val);
		} else {
			if (missing_to && missing_from) {
				/* no conversion */
				return ExcelUtils::MakeVariant_Double(val);
			} else if (missing_to) {
				if (missingBehavior_to == XlUnitSupport::missing_is_adimensional) {
					goto do_full_conversion;
				} else if (missingBehavior_to == XlUnitSupport::missing_is_SI) {
					convertedValue = converter.From(unit_from, strict != 0).ToSI().Convert(val);
				} else if (missingBehavior_to == XlUnitSupport::missing_is_specified_unit) {
					/* no conversion */
					return ExcelUtils::MakeVariant_Double(val);
				} else {
					MY_ASSERT(false);
				}
			} else if (missing_from) {
				if (missingBehavior_from == XlUnitSupport::missing_is_adimensional) {
					goto do_full_conversion;
				} else if (missingBehavior_from == XlUnitSupport::missing_is_SI) {
					convertedValue = converter.To(unit_to, strict).FromSI().Convert(val);
				} else if (missingBehavior_from == XlUnitSupport::missing_is_specified_unit) {
					/* no conversion */
					return ExcelUtils::MakeVariant_Double(val);
				} else {
					MY_ASSERT(false);
				}
			} else {
				MY_ASSERT(false);
			}
		}
		
		if (auto error = converter.GetError()) {
			return ExcelUtils::MakeVariant_String(xFormat(L"%S", error));
		} else {
			return ExcelUtils::MakeVariant_Double(convertedValue);
		}
	}

	/* must clear retVal if not returned */
	VARIANT ConvertCheck(VARIANT & retVal, const wchar_t * unit, bool noThrow) {

		if (ExcelUtils::IsString(retVal)) {
			if (retVal.bstrVal && retVal.bstrVal[0] == L'#') {
				if (noThrow) {
					return retVal;
				}
				else {
					auto ex = std::runtime_error(xFormat("%S", retVal.bstrVal));
					VariantClear(&retVal);
					throw ex;
				}
			}
			auto type = XlUnitSupport::GetType(ExcelUtils::MakeVariant_FakeString(unit));
			if (noThrow) {
				auto retVal2 = ExcelUtils::MakeVariant_String((const wchar_t*)xFormat(L"# Wrong unit for %s: %s #", type.bstrVal, retVal.bstrVal));
				VariantClear(&type);
				VariantClear(&retVal);
				return retVal2;
			}
			else {
				auto ex = std::runtime_error(xFormat("Wrong unit for %S: %S", type.bstrVal, retVal.bstrVal));
				VariantClear(&type);
				VariantClear(&retVal);
				throw ex;
			}
		}

		if (!ExcelUtils::IsDouble(retVal)) {
			auto type = XlUnitSupport::GetType(ExcelUtils::MakeVariant_FakeString(unit));
			if (noThrow) {
				auto retVal2 = ExcelUtils::MakeVariant_String((const wchar_t*)xFormat(L"# Wrong input for %s #", type.bstrVal));
				VariantClear(&type);
				VariantClear(&retVal);
				return retVal2;
			}
			else {
				auto ex = std::runtime_error(xFormat("Wrong input for %S", type.bstrVal));
				VariantClear(&type);
				VariantClear(&retVal);
				throw ex;
			}
		}

		return retVal;
	}

	VARIANT ConvertInputCore(VARIANT & v, const wchar_t * unit, bool noThrow) {
		auto retVal = Convert(v, ExcelUtils::MakeVariant_Missing(), ExcelUtils::MakeVariant_FakeString(unit), false, XlUnitSupport::missing_is_SI);
		return ConvertCheck(retVal, unit, noThrow);
	}

	//VARIANT ConvertOutputCore(VARIANT & v, const wchar_t * unit, bool noThrow) {
	//	auto retVal = XlUnitSupport::Convert(v, ExcelUtils::MakeVariant_FakeString(unit), ExcelUtils::MakeVariant_Missing(), false, XlUnitSupport::missing_is_adimensional, XlUnitSupport::missing_is_specified_unit);
	//	return ConvertCheck(retVal, unit, noThrow);
	//}

	double ConvertInput(VARIANT & v, const wchar_t * unit) {
		return ConvertInputCore(v, unit, false).dblVal;
	}

	//double ConvertOutput(VARIANT & v, const wchar_t * unit) {
	//	return ConvertOutputCore(v, unit, false).dblVal;
	//}

	VARIANT ConvertInput_noThrow(VARIANT & v, const wchar_t * unit) {
		return ConvertInputCore(v, unit, true);
	}

	//VARIANT ConvertOutput_noThrow(VARIANT & v, const wchar_t * unit) {
	//	return ConvertOutputCore(v, unit, true);
	//}

	VARIANT ConvertOutput(VARIANT & v, const wchar_t * unit) {
		auto retVal = XlUnitSupport::Convert(v, ExcelUtils::MakeVariant_FakeString(unit), ExcelUtils::MakeVariant_Missing(), false, XlUnitSupport::missing_is_adimensional, XlUnitSupport::missing_is_specified_unit);
		return ConvertCheck(retVal, unit, true);
	}
}
