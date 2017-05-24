#define _CRT_SECURE_NO_WARNINGS

#include "windows.h"
#include "WinUtils.h"
#include "ExcelUtils.h"

BSTR CharPtrToBSTR(const char * charPtr) {

	if (!charPtr) return nullptr;

	size_t len = charPtr ? strlen(charPtr) : 0;
	return SysAllocStringByteLen(charPtr, (UINT)len); //SysAllocStringByteLen appends a 1-byte null terminator
}

BSTR WcharPtrToBSTR(const wchar_t * charPtr) {

	if (!charPtr) return nullptr;

	size_t len = charPtr ? wcslen(charPtr) : 0;
	//return SysAllocStringByteLen((char*)charPtr, (UINT)(2*(len + 1)));//SysAllocStringByteLen appends a 1-byte null terminator
	return SysAllocStringByteLen((char*)charPtr, (UINT)(2 * len));//SysAllocStringByteLen appends a 1-byte null terminator
}

//VARIANT MakeVariant_Double(double value) {
//
//	VARIANT retVal;
//
//	retVal.vt = VT_R8;
//	retVal.dblVal = value;
//
//	return retVal;
//}
//
//VARIANT MakeVariant_Error(int errorCode) {
//
//	VARIANT retVal;
//
//	retVal.vt = VT_ERROR;
//	retVal.scode = errorCode;
//
//	return retVal;
//}
//
//VARIANT MakeVariant_String(wchar_t * str) {
//
//	VARIANT retVal;
//
//	retVal.vt = VT_BSTR;
//	retVal.bstrVal = str ? SysAllocString(str) : str;
//
//	return retVal;
//}

void ExcelUtils_SEH_Translate(unsigned int u, PEXCEPTION_POINTERS pExp) {

	const char * errorMsg = "An error occured.";
	bool critical = false;

	switch (pExp->ExceptionRecord->ExceptionCode) {
		case EXCEPTION_ACCESS_VIOLATION:
			critical = true;
			errorMsg = "A serious error occured (memory access violation).\nExcel may crash at any moment.\n\nConsider saving now.\n";
			break;
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
			errorMsg = "An integer divide by zero error occured.";
			break;
		case EXCEPTION_INT_OVERFLOW:
			critical = true;
			errorMsg = "A serious error occured (stack overflow).\nExcel may crash at any moment.\n\nConsider saving now.\n";
			break;
	}

	CoreDump(errorMsg, pExp, GetCurrentThreadId(), "", 0, "", false);

	throw MyException(errorMsg, critical);
}

VARIANT ExcelUtils_ExceptionHandler(std::exception_ptr eptr) {

	try {
		if (eptr) {
			std::rethrow_exception(eptr);
		}
	}
	catch (const std::exception& e) {
		return ExcelUtils::MakeVariant_String(xFormat(L"# %S #", e.what()));
	}
	catch (...) {
		return ExcelUtils::MakeVariant_String(L"# CRITICAL ERROR #");
	}

	return ExcelUtils::MakeVariant_String(L"???");
}

