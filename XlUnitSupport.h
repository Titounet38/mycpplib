

#ifndef _UnitSupport_
#define _UnitSupport_

#include "ExcelUtilsX.h"

DllExport VARIANT __stdcall Unit_ConvertFromSI(VARIANT & v, VARIANT & v_To, VBA_BOOL strict);
DllExport VARIANT __stdcall Unit_ConvertToSI(VARIANT & v, VARIANT & v_From, VBA_BOOL strict);
DllExport VARIANT __stdcall Unit_GetType(VARIANT & v_Unit);
DllExport VARIANT __stdcall Unit_GetInterpretation(VARIANT & v_Unit);
DllExport VARIANT __stdcall Unit_GetDimensions(VARIANT & v_Unit);
DllExport VARIANT __stdcall Unit_Convert(VARIANT & v, VARIANT & v_From, VARIANT & v_To, VBA_BOOL strict);

namespace XlUnitSupport {
	VARIANT ConvertFromSI(VARIANT & v, VARIANT & v_To = ExcelUtils::MakeVariant_Missing(), bool strict = false);
	VARIANT ConvertToSI(VARIANT & v, VARIANT & v_From = ExcelUtils::MakeVariant_Missing(), bool strict = false);
	VARIANT GetType(VARIANT & v_Unit);
	VARIANT GetInterpretation(VARIANT & v_Unit);
	VARIANT GetDimensions(VARIANT & v_Unit);
	enum Convert_MissingBehavior {
		missing_is_SI, missing_is_specified_unit, missing_is_adimensional
	};
	VARIANT Convert(
		VARIANT & v,
		VARIANT & v_From,
		VARIANT & v_To,
		bool strict = false,
		Convert_MissingBehavior missingBehavior_from = Convert_MissingBehavior::missing_is_adimensional,
		Convert_MissingBehavior missingBehavior_to = Convert_MissingBehavior::missing_is_adimensional);
}
#endif

