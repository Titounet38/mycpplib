#ifndef _ExcelUtilsX_
#define _ExcelUtilsX_

#include "ExcelUtils.h"
#include "xVect.h"

namespace ExcelUtils {
	BSTR CharPtrToBSTR(const char * charPtr);
	BSTR WcharPtrToBSTR(const wchar_t * charPtr);

#pragma pack(push, 4)

	template <class T>
	struct SAFEARRAY {
		USHORT         cDims;
		USHORT         fFeatures;
		ULONG          cbElements;
		ULONG          cLocks;
		T *            pvData;
		SAFEARRAYBOUND rgsabound[1];

		T * GetEl(ptrdiff_t i, bool errorIfOutOfRange = true) {

			if (!this) return nullptr;

			if (this->cDims != 1) {
				RECOVERABLE_ERROR("array dimension is not 1", -1);
				return nullptr;
			}

			if (this->cbElements != sizeof(T)) {
				RECOVERABLE_ERROR("Wrong element size", -1);
				return nullptr;
			}

			i -= this->rgsabound[0].lLbound;

			if ((size_t)i >= this->rgsabound[0].cElements) {
				if (errorIfOutOfRange) RECOVERABLE_ERROR("Index out of range", -1);
				return nullptr;
			}

			return &((T*)this->pvData)[i];
		}
	};

#pragma pack(pop)

	template <class T>
	void aVectToSafeArray(::SAFEARRAY *& sa, const aVect<T> & t) {

		if (SafeArrayDestroy(sa) != S_OK) MY_ERROR("SafeArrayDestroy failed");

		SAFEARRAYBOUND bounds;

#ifdef _WIN64
		if (t.Count() >= UINT_MAX) MY_ERROR("vector too large for SAFEARRAY");
#endif

		bounds.cElements = (ULONG)t.Count();
		bounds.lLbound = 1;

		VARENUM varType;

		if (std::is_same<T, double>::value) {
			varType = VT_R8;
		} else {
			MY_ERROR("TODO");
		}

		sa = SafeArrayCreate(varType, 1, &bounds);

		auto ptr = GetEl((ExcelUtils::SAFEARRAY<double>*)sa, 1);

		memcpy(ptr, t, t.Count() * sizeof(T));
	}


	inline void MySysFreeString(BSTR & str) {
		SysFreeString(str);
		str = NULL;
	}

	template <class T>
	T * GetEl(SAFEARRAY<T> * ptr, ptrdiff_t i, bool errorIfOutOfRange = true) {

		if (!ptr) return nullptr;

		return ptr->GetEl(i, errorIfOutOfRange);
	}

	template<bool insertLineFeed>
	void AppendWcharToBSTR(BSTR & str1, const wchar_t * str2) {

		size_t oldLen = str1 ? wcslen(str1) : 0;
		size_t addLen = str2 ? wcslen(str2) : 0;
		size_t newLen = oldLen + addLen + insertLineFeed;// + 1 for the line feed

		if (oldLen > 0) {
			//static aVect<char> buf;
			aVect<char> buf;
			buf.Redim(2 * (newLen + 1));						// + 1 for the null terminator
			memcpy(buf, str1, 2 * oldLen);						//copy old string
			if (insertLineFeed) buf[2 * oldLen] = 10; buf[2 * oldLen + 1] = 0;		//line feed
			memcpy((char*)buf + 2 * (oldLen + insertLineFeed), str2, 2 * addLen);   //append additional string
			buf[2 * newLen] = buf[2 * newLen + 1] = 0;		//null terminator

															//logFile.Printf("AppendWcharToBSTR: MySysFreeString(0x%p)\n", str1);
			MySysFreeString(str1);
			str1 = SysAllocStringByteLen(buf, (UINT)buf.Count() - 2);		//SysAllocStringByteLen appends a 1-byte null terminator
		}
		else {
			MySysFreeString(str1);
			if (str2) str1 = SysAllocStringByteLen((char*)str2, (UINT)(2 * (addLen)));//SysAllocStringByteLen appends 1-byte a null terminator
		}
	}

	void SEH_Translate(unsigned int u, PEXCEPTION_POINTERS pExp);
	VARIANT ExceptionHandler(std::exception_ptr eptr);
}

#endif
