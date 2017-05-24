
#pragma warning(disable : 4164)
#pragma function(strlen)
#pragma warning(4 : 4519)

#ifndef _XVECT_
#define _XVECT_

#include "stdio.h"
#include <utility>
#include <new>
#include "malloc.h"
#include <typeinfo>
#include "stdarg.h"
#include "cmath"

#ifdef XVECT_BREAK_AT_LARGE_ALLOC
#include "windows.h"
#include "psapi.h"
#endif

#ifndef XVECT_NO_STATIC_BUFFER
#include "WinCritSect.h"
#endif

#include "Debug.h"

#ifdef SIMULATE_NO_WIN32
#define _WIN32_BKP   _WIN32
#define _MSC_VER_BKP _MSC_VER
#undef _WIN32
#undef _MSC_VER
#endif

#ifndef _MyUtils_

#define SIZE_T_OVERFLOW_CHECK_AFFECT(a) (a);								\
{																			\
	if (((double)(a)) > ((double) SIZE_MAX)) {								\
		MY_ERROR(aVect<char>("unsigned overflow affectation : %f > %u",		\
		((double)(a)), SIZE_MAX));											\
	}																		\
}																			

#define SIZE_T_OVERFLOW_CHECK_MULT(a, b) {									\
	if ((a) > SIZE_MAX/(b)) {												\
		MY_ERROR(aVect<char>("unsigned overflow : %u * %u", (a), (b)));		\
	}																		\
}

#define SIZE_T_OVERFLOW_CHECK_ADD(a, b) {									\
	if (SIZE_MAX - (a) < (b)) {												\
		MY_ERROR(aVect<char>("unsigned overflow : %u + %u", (a), (b)));		\
	}																		\
}

#define SIZE_T_OVERFLOW_CHECK_SUBSTRACT(a, b) {								\
	if ((a) < (b)) {														\
		MY_ERROR(aVect<char>("unsigned overflow : %u - %u", (a), (b)));		\
	}																		\
}

#endif

#define STATIC_ASSERT_SAME_TYPE(a,b) static_assert(std::is_same<a, decltype(b)>::value, "wrong type");
#define EXPAND(X) X

#define GET_FOR_MACRO(_1,_2,_3,NAME,...) NAME
#define aVect_static_for(...) EXPAND(GET_FOR_MACRO(__VA_ARGS__, aVect_static_for3, aVect_static_for2)(__VA_ARGS__))
#define aVect_for(...) EXPAND(GET_FOR_MACRO(__VA_ARGS__, aVect_for3, aVect_for2)(__VA_ARGS__))
#define aVect_for_inv(...) EXPAND(GET_FOR_MACRO(__VA_ARGS__, aVect_for_inv3, aVect_for_inv2)(__VA_ARGS__))


#define xVect_static_for(t,i) for (size_t i=0, ___I=xVect_Count(t) ; i<___I ; i++)
#define xVect_for(t,i) for (size_t i=0 ; i<xVect_Count(t) ; i++)
#define xVect_for_inv(t,i) for (size_t ___I=xVect_Count(t), i=___I-1; i<___I ; i--)

#define aVect_static_for2(t,i) for (size_t i=0, ___I=(t).Count() ; i<___I ; i++)
#define aVect_for2(t,i) for (size_t i=0 ; i<(t).Count() ; i++)
#define aVect_for_inv2(t,i) for (size_t ___I=(t).Count(), i=___I-1; i<___I ; i--)

#define aVect_static_for3(t,i,ptr) if (size_t ___I=0); else if (size_t i=0); else if (decltype(t.GetDataPtr()) ptr = nullptr); else for (ptr = (t), i=0, ___I=(t).Count(); i<___I ; ++ptr, ++i)
#define aVect_for3(t,i,ptr) if (size_t i=0); else if (decltype(t.GetDataPtr()) ptr = nullptr); else for (ptr = (t); i<(t).Count() ; ++i)
#define aVect_for_inv3(t,i,ptr) if (size_t i=0); else if (size_t ___I=0); else if (decltype(t.GetDataPtr()) ptr = nullptr); else for (___I = (t).Count(), i=___I-1, ptr = &(t)[i]; i<___I ; --ptr, i--)

#define aVect_ranged_static_for(t,ptr) if (size_t ___I=0); else if (size_t ___i=0); else if (decltype(t.GetDataPtr()) ptr = (decltype(t.GetDataPtr()))nullptr); else for (ptr = (t), ___i=0, ___I=(t).Count(); ___i<___I ; ++ptr, ++___i)
#define aVect_ranged_for(t,ptr) if (size_t ___i=0); else if (decltype(t.GetDataPtr()) ptr = (decltype(t.GetDataPtr()))nullptr); else for (ptr = (t); ___i<(t).Count() ; ++ptr, ++___i)
#define aVect_ranged_for_inv(t,ptr) if (size_t ___i=0); else if (size_t ___I=0); else if (decltype(t.GetDataPtr()) ptr =(decltype(t.GetDataPtr()))nullptr); else for (____I = (t).Count(), ___i=___I-1, ptr = &(t)[___i]; ___i<___I ; --ptr, ___i--)

#define mVect_static_for(t,i) aVect_static_for(t,i)
#define mVect_for(t,i) aVect_for(t,i)
#define mVect_for_inv(t,i) aVect_for_inv(t,i)

typedef void * (*AllocFunc)(size_t, void*);
typedef void * (*ReallocFunc)(void*, size_t, void*);
typedef void(*FreeFunc)(void*, void*);

template <
	class T,
	bool owner = true,
	bool isMvect = false,
	AllocFunc allocFunc = nullptr,
	ReallocFunc reallocFunc = nullptr,
	FreeFunc freeFunc = nullptr,
	void* param = nullptr>
class aVect;

//DEBUG CHANGED NAME
extern unsigned long long g_nAllocation_xVect;
extern unsigned long long g_accAllocated_xVect;
extern unsigned long long g_nAllocation_yVect;
extern unsigned long long g_accAllocated_yVect;
extern unsigned long long g_allocID;
extern unsigned long long g_dgbCatcher;
//extern char g_aVect_char_sprintf_buffer[32];
//extern aVect<char> g_aVect_char_sprintf_buffer;
// extern aVect<wchar_t> g_aVect_wchar_sprintf_buffer;
extern void *** g_pOwnerToClear_xVect;
extern int g_CopyRecursion_xVect;

static unsigned long long bugCatcher;

#if defined(_DEBUG) || defined(MY_DEBUG) 
#define XVECT_COOKIE 0xC0014130
#endif

#if defined(_DEBUG) || defined(MY_DEBUG) 
#define YVECT_COOKIE 0xC0014131
#endif

#ifdef _MSC_VER
#define restrict __restrict
#else
#define restrict __restrict__
#endif

template <class T> struct PlaceHolder { char c[sizeof(T)]; };

template <class T> struct xVect_struct {
	size_t count;
	size_t allocatedCount;
#if defined(_DEBUG) || defined(MY_DEBUG) 
	unsigned long long allocID; 
	unsigned long cookie;
#endif
	T vector[1];
};

template <class T> struct yRef {
	T ** ref;
	bool weak;
};

template <class T>
struct yVect_struct {
	size_t count;
	size_t allocatedCount;
	yRef<T> * refArray;
#if defined(_DEBUG) || defined(MY_DEBUG) 
	unsigned long long allocID;
	unsigned long cookie;
#endif
	T vector[1];
};


template <class T> inline xVect_struct<T> * xVect_BasePtr(T * ptr) {

	xVect_struct<T> * t = (xVect_struct<T> *)(((char*)ptr) - ((char*)&((xVect_struct<T> *)0)->vector - (char*)&((xVect_struct<T> *)0)->count));

#if defined(_DEBUG) || defined(MY_DEBUG) 
	if (t->cookie != XVECT_COOKIE) {
		MY_ERROR("xVect_struct : Bad cookie !\n");
	}
#endif

	return t;
}

template <class T> inline yVect_struct<T> * yVect_BasePtr(T * ptr) {

	yVect_struct<T> * t = (yVect_struct<T> *)(((char*)ptr) - ((char*)&((yVect_struct<T> *)0)->vector - (char*)&((yVect_struct<T> *)0)->count));

#if defined(_DEBUG) || defined(MY_DEBUG) 
	if (t->cookie != YVECT_COOKIE) {
		MY_ERROR("yVect_struct : Bad cookie !\n");
	}
#endif

	return t;
}

typedef void * (*AllocFunc)(size_t, void*);
typedef void * (*ReallocFunc)(void*, size_t, void*);
typedef void(*FreeFunc)(void*, void*);

//Count
template <class T> size_t xVect_Count(T * ptr) {
	if (ptr) return xVect_BasePtr(ptr)->count;
	return 0;
}

//AllocatedCount
template <class T> size_t xVect_AllocatedCount(T * ptr) {
	if (ptr) return xVect_BasePtr(ptr)->allocatedCount;
	return 0;
}

//Size
template <class T> size_t xVect_DataSize(T * ptr) {
	return xVect_Count(ptr) *sizeof(T);
}

//Set
template <class T> void xVect_Set(T * ptr, T val) {

	register T * restrict const t = ptr;

	xVect_for_inv(ptr, i) {
		((PlaceHolder<T>*)t)[i] = *((PlaceHolder<T>*)&val);
	}
}

//Free
template <
	FreeFunc freeFunc = nullptr,
	void * param = nullptr,
	class T = void>

void xVect_Free(T*& t) {

	if (t) {
		xVect_struct<T> * basePtr = xVect_BasePtr(t);
#if defined(_DEBUG) || defined(MY_DEBUG) 
		g_accAllocated_xVect--;
#endif

		if ((FreeFunc)freeFunc) freeFunc(basePtr, param);

#if defined(_DEBUG) || defined(MY_DEBUG) 
		else _free_dbg(basePtr, _CLIENT_BLOCK); 
#else
		else free(basePtr);
#endif

		t = nullptr;
	}
}


//Free
template <
	FreeFunc freeFunc = nullptr,
	void * param = nullptr,
	class T = void>

void yVect_Free(T*& t) {

	if (t) {
		yVect_struct<T> * basePtr = yVect_BasePtr(t);
#if defined(_DEBUG) || defined(MY_DEBUG) 
		g_accAllocated_yVect--;
#endif

		if ((FreeFunc)freeFunc) freeFunc(basePtr, param);

#if defined(_DEBUG) || defined(MY_DEBUG) 
		else _free_dbg(basePtr, _CLIENT_BLOCK);
#else
		else free(basePtr);
#endif

		t = nullptr;
	}
}

//Create
template <
	class T,
	bool reserve = false,
	AllocFunc allocFunc = nullptr,
	void * param = nullptr>

T * xVect_Create(size_t count) {

#if defined(_DEBUG) || defined(MY_DEBUG) 
	g_accAllocated_xVect++;
	g_nAllocation_xVect++;
#endif

	SIZE_T_OVERFLOW_CHECK_MULT(count, sizeof(T));
	SIZE_T_OVERFLOW_CHECK_ADD(sizeof(xVect_struct<T>), count*sizeof(T));
	size_t size = sizeof(xVect_struct<T>) - sizeof(T) + count*sizeof(T);

#if defined(_DEBUG) || defined(MY_DEBUG) 
	xVect_struct<T> * t = (xVect_struct<T> *) ((AllocFunc)allocFunc ? allocFunc(size, param) : _malloc_dbg(size, _CLIENT_BLOCK, nullptr, 0));
#else
	xVect_struct<T> * t = (xVect_struct<T> *) ((AllocFunc)allocFunc ? allocFunc(size, param) : malloc(size));
#endif

	if (!t) MY_ERROR("malloc failed");

	t->count = reserve ? 0 : count;
	t->allocatedCount = count;

#if defined(_DEBUG) || defined(MY_DEBUG) 
	t->cookie = XVECT_COOKIE;
	t->allocID = g_allocID++;
#endif

	return &t->vector[0];
}


//Create
template <
	class T,
	bool reserve = false,
	AllocFunc allocFunc = nullptr,
	void * param = nullptr>

T * yVect_Create(size_t count) {

#if defined(_DEBUG) || defined(MY_DEBUG) 
	g_accAllocated_yVect++;
	g_nAllocation_yVect++;
#endif

	SIZE_T_OVERFLOW_CHECK_MULT(count, sizeof(T));
	SIZE_T_OVERFLOW_CHECK_ADD(sizeof(yVect_struct<T>), count*sizeof(T));
	size_t size = sizeof(yVect_struct<T>) - sizeof(T) + count*sizeof(T);

#if defined(_DEBUG) || defined(MY_DEBUG) 
	yVect_struct<T> * t = (yVect_struct<T> *) ((AllocFunc)allocFunc ? allocFunc(size, param) : _malloc_dbg(size, _CLIENT_BLOCK, nullptr, 0));
#else
	yVect_struct<T> * t = (yVect_struct<T> *) ((AllocFunc)allocFunc ? allocFunc(size, param) : malloc(size));
#endif

	if (!t) MY_ERROR("malloc failed");

	t->count = reserve ? 0 : count;
	t->allocatedCount = count;

#if defined(_DEBUG) || defined(MY_DEBUG) 
	t->cookie = YVECT_COOKIE;
	t->allocID = g_allocID++;
#endif

	t->refArray = nullptr;

	return &t->vector[0];
}


template <
	bool reserve = false,
	AllocFunc allocFunc = nullptr,
	void * param = nullptr,
	class T = void>

void xVect_Create(T*& t, size_t count) {

	if (t) xVect_Free(t);

	t = xVect_Create<T, reserve, allocFunc, param>(count);
}

//Redim
template <bool exactAlloc = false,
	bool reserve = false,
	AllocFunc allocFunc = nullptr,
	ReallocFunc reallocFunc = nullptr,
	void * param = nullptr,
	class T = void>

void xVect_Redim(T*& ptrRef, size_t newCount) {

	xVect_struct<T> * ptr;

	if (ptrRef) {

		ptr = xVect_BasePtr(ptrRef);

		if ((exactAlloc && ptr->allocatedCount != newCount) || newCount > ptr->allocatedCount)  {

			size_t finalCount;
			if (reserve) {
				finalCount = newCount;
				newCount = ptr->count;
			}
			else if (exactAlloc) {
				finalCount = newCount;
			}
			else {
				SIZE_T_OVERFLOW_CHECK_ADD(newCount, ptr->allocatedCount);
				SIZE_T_OVERFLOW_CHECK_ADD(newCount + ptr->allocatedCount, 5);
				finalCount = newCount + ptr->allocatedCount + 5;
			}

			SIZE_T_OVERFLOW_CHECK_MULT(finalCount, sizeof(T));
			SIZE_T_OVERFLOW_CHECK_ADD(sizeof(xVect_struct<T>) - sizeof(ptr->vector), finalCount*sizeof(T));

			size_t size = sizeof(xVect_struct<T>) - sizeof(ptr->vector) + finalCount*sizeof(T);

#if defined(_DEBUG) || defined(MY_DEBUG) 
			xVect_struct<T> * newPtr = (xVect_struct<T>*) ((ReallocFunc)reallocFunc ? reallocFunc(ptr, size, param) : _realloc_dbg(ptr, size, _CLIENT_BLOCK, nullptr, 0));
#else
			xVect_struct<T> * newPtr = (xVect_struct<T>*) ((ReallocFunc)reallocFunc ? reallocFunc(ptr, size, param) : realloc(ptr, size));
#endif

			if (!newPtr) MY_ERROR("realloc failure");

			if (ptr != newPtr) {
				ptr = newPtr;
				ptrRef = &ptr->vector[0];
			}
			ptr->allocatedCount = finalCount;
			ptr->count = newCount;
		}
		else if (!reserve) ptr->count = newCount;

#if defined(_DEBUG) || defined(MY_DEBUG) 
		ptr->cookie = XVECT_COOKIE;
		ptr->allocID = g_allocID++;
#endif
	}
	else {
		ptrRef = xVect_Create<T, reserve, allocFunc, param>(newCount);
	}
}

//Redim
template <bool exactAlloc = false,
	bool reserve = false,
	AllocFunc allocFunc = nullptr,
	ReallocFunc reallocFunc = nullptr,
	void * param = nullptr,
	class T = void>
	
void yVect_Redim(T*& ptrRef, size_t newCount) {

	yVect_struct<T> * ptr;

	if (ptrRef) {

		ptr = xVect_BasePtr(ptrRef);

		if ((exactAlloc && ptr->allocatedCount != newCount) || newCount > ptr->allocatedCount)  {

			size_t finalCount;
			if (reserve) {
				finalCount = newCount;
				newCount = ptr->count;
			}
			else if (exactAlloc) {
				finalCount = newCount;
			}
			else {
				SIZE_T_OVERFLOW_CHECK_ADD(newCount, ptr->allocatedCount);
				SIZE_T_OVERFLOW_CHECK_ADD(newCount + ptr->allocatedCount, 5);
				finalCount = newCount + ptr->allocatedCount + 5;
			}

			SIZE_T_OVERFLOW_CHECK_MULT(finalCount, sizeof(T));
			SIZE_T_OVERFLOW_CHECK_ADD(sizeof(xVect_struct<T>) - sizeof(ptr->vector), finalCount*sizeof(T));

			size_t size = sizeof(yVect_struct<T>) - sizeof(ptr->vector) + finalCount*sizeof(T);

#if defined(_DEBUG) || defined(MY_DEBUG) 
			yVect_struct<T> * newPtr = (yVect_struct<T>*) ((ReallocFunc)reallocFunc ? reallocFunc(ptr, size, param) : _realloc_dbg(ptr, size, _CLIENT_BLOCK, nullptr, 0));
#else
			yVect_struct<T> * newPtr = (yVect_struct<T>*) ((ReallocFunc)reallocFunc ? reallocFunc(ptr, size, param) : realloc(ptr, size));
#endif
			if (!newPtr) MY_ERROR("realloc failure");

			if (ptr != newPtr) {
				ptr = newPtr;
				ptrRef = &ptr->vector[0];
			}
			ptr->allocatedCount = finalCount;
			ptr->count = newCount;
			ptr->ref = nullptr;
		}
		else if (!reserve) ptr->count = newCount;

#if defined(_DEBUG) || defined(MY_DEBUG) 
		ptr->cookie = XVECT_COOKIE;
		ptr->allocID = g_allocID++;
#endif
	}
	else {
		ptrRef = yVect_Create<T, reserve, allocFunc, param>(newCount);
	}
}


//Redim
template <bool exactAlloc = false,
	bool reserve = false,
	AllocFunc allocFunc = nullptr,
	ReallocFunc reallocFunc = nullptr,
	void * param = nullptr,
	class T = void>

void xVect_Grow(T*& ptrRef, long long nGrow) {
		
	xVect_Redim<exactAlloc, reserve, allocFunc, reallocFunc, param>(ptrRef, (size_t)(xVect_Count(ptrRef) + nGrow));

}

//Last
template <class T> T & xVect_Last(T* ptr) {

#if defined(_DEBUG) || defined(MY_DEBUG) 
	if (!ptr) MY_ERROR("xVect_End : empty array");
#endif

	return ptr[xVect_Count(ptr) - 1];
}

//Copy
template <
	AllocFunc allocFunc = nullptr,
	void * param = nullptr,
	class T = void>

T * xVect_Copy(T * t) {

	T * retVal;

	if (t) {
		T * copy = xVect_Create<T, false, allocFunc, param>(xVect_Count(t));
		memcpy(copy, t, xVect_Count(t)*sizeof(T));
		retVal = copy;
	}
	else retVal = nullptr;

	return retVal;
}

//Copy
template <
	AllocFunc allocFunc = nullptr,
	ReallocFunc reallocFunc = nullptr,
	void * param = nullptr,
	class T = void>

void xVect_Copy(T*& t1, T * t2) {

	if (xVect_Count(t1) != xVect_Count(t2)) {
		xVect_Redim<false, false, allocFunc, reallocFunc, param>(t1, xVect_Count(t2));
	}
	memcpy(t1, t2, xVect_Count(t2)*sizeof(T));

}

//Push(byref)
template <
	AllocFunc allocFunc = nullptr,
	ReallocFunc reallocFunc = nullptr,
	void * param = nullptr,
	class T = void>

void xVect_Push_ByRef(T*& t, T& el) {

	size_t count = xVect_Count(t);

	xVect_Redim<false, false, allocFunc, reallocFunc, param>(t, count + 1);
	*(PlaceHolder<T>*)(&t[count]) = *(PlaceHolder<T>*)(&el);
}

//Push(byval)
template <
	AllocFunc allocFunc = nullptr,
	ReallocFunc reallocFunc = nullptr,
	void * param = nullptr,
	class T = void>

void xVect_Push_ByVal(T*& t, T el) {

	size_t count = xVect_Count(t);

	xVect_Redim<false, false, allocFunc, reallocFunc, param>(t, count + 1);
	*(PlaceHolder<T>*)(&t[count]) = *(PlaceHolder<T>*)(&el);
}

//Pop
template <
	AllocFunc allocFunc = nullptr,
	ReallocFunc reallocFunc = nullptr,
	void * param = nullptr,
	class T = void>

T xVect_Pop(T * t) {

	T retVal;

	if (t) {
		size_t count = xVect_Count(t);
		if (count) {
			*(PlaceHolder<T>*)(&retVal) = *(PlaceHolder<T>*)(&t[count - 1]);
			xVect_Redim<false, false, allocFunc, reallocFunc, param>(t, count - 1, );
		}
		else MY_ERROR("xVect_Pop sur vecteur vide");
	}
	else MY_ERROR("xVect_Pop sur vecteur vide");

	return retVal;
}

//Remove
template <
	AllocFunc allocFunc = nullptr,
	ReallocFunc reallocFunc = nullptr,
	void * param = nullptr,
	class T = void>

void xVect_Remove(T*& t, size_t n) {

	size_t I = xVect_Count(t);

	if (n >= I) {
		MY_ERROR("Indice en dehors des limites");
	}

	if (I > 1) {
		for (size_t i = n; i < I - 1; i++) {
			*(PlaceHolder<T>*)(&t[i]) = *(PlaceHolder<T>*)(&t[i + 1]);
		}
	}

	xVect_Redim<false, false, allocFunc, reallocFunc, param>(t, I - 1);
}

//Insert
template <
	AllocFunc allocFunc = nullptr,
	ReallocFunc reallocFunc = nullptr,
	void * param = nullptr,
	class T = void>

void xVect_Insert_ByVal(T*& t, T el, size_t n) {

	size_t I = xVect_Count(t);

	xVect_Redim<false, false, allocFunc, reallocFunc, param>(t, I + 1);

	for (size_t i = I - 1; i >= n && i < I + 1; i--) {
		*(PlaceHolder<T>*)(&t[i + 1]) = *(PlaceHolder<T>*)(&t[i]);
	}

	*(PlaceHolder<T>*)(&t[n]) = *(PlaceHolder<T>*)(&el);
}

//Insert
template <
	AllocFunc allocFunc = nullptr,
	ReallocFunc reallocFunc = nullptr,
	void * param = nullptr,
	class T = void>

void xVect_MakeRoom(T*& t, size_t n) {

	size_t I = xVect_Count(t);

	xVect_Redim<false, false, allocFunc, reallocFunc, param>(t, I + 1);

	for (size_t i = I - 1; i >= n && i < I + 1; i--) {
		*(PlaceHolder<T>*)(&t[i + 1]) = *(PlaceHolder<T>*)(&t[i]);
	}
}

//Insert
template <
	AllocFunc allocFunc = nullptr,
	ReallocFunc reallocFunc = nullptr,
	void * param = nullptr,
	class T = void>

void xVect_Insert(T*& t, T& el, size_t n) {

	size_t I = xVect_Count(t);

	xVect_Redim<false, flase, allocFunc, reallocFunc, param>(t, I + 1);

	for (size_t i = I - 1; i >= n && i < I + 1; i--) {
		*(PlaceHolder<T>*)(&t[i + 1]) = *(PlaceHolder<T>*)(&t[i]);
	}

	*(PlaceHolder<T>*)(&t[n]) = *(PlaceHolder<T>*)(&el);
}

template <
	AllocFunc allocFunc = nullptr,
	ReallocFunc reallocFunc = nullptr,
	void * param = nullptr,
	class T = void>
	
void xVect_Append(T*& t, T* tAppend) {

	size_t I;
	size_t I2;
	if (std::is_same<T, char>::value) {
		I = strlen((char*)t);
		I2 = strlen((char*)tAppend) + 1;
	}
	else if (std::is_same<T, wchar_t>::value) {
		I = wcslen((wchar_t*)t);
		I2 = wcslen((wchar_t*)tAppend) + 1;
	}
	else {
		I = xVect_Count(t);
		I2 = xVect_Count(tAppend);
	}

	xVect_Redim<false, false, allocFunc, reallocFunc, param>(t, I + I2);
	memcpy(t + I, tAppend, I2 * sizeof(T));
}


template <
	AllocFunc allocFunc = nullptr,
	ReallocFunc reallocFunc = nullptr,
	void * param = nullptr,
	class T = void >
	
void xVect_Prepend(T*& t, T* tPrepend) {

	size_t I;
	size_t I2;
	if (std::is_same<T, char>::value) {
		I = strlen((char*)t) + 1;
		I2 = strlen((char*)tPrepend);
	}
	else if (std::is_same<T, wchar_t>::value) {
		I = wcslen((wchar_t*)t) + 1;
		I2 = wcslen((wchar_t*)tPrepend);
	}
	else {
		I = xVect_Count(t);
		I2 = xVect_Count(tPrepend);
	}

	xVect_Redim<false, false, allocFunc, reallocFunc, param>(t, I + I2);
	for (size_t i = I - 1; i<I; --i) {
		t[i + I2] = t[i];
	}
	memcpy(t, tPrepend, I2 * sizeof(T));
}

template <
	AllocFunc allocFunc = nullptr,
	ReallocFunc reallocFunc = nullptr,
	FreeFunc freeFunc = nullptr,
	void * param = nullptr,
	class T>

void xVect_Append(char *& t, const char * fStr, T args, ...) {

	xVect_vAppend<allocFunc, reallocFunc, freeFunc, param>(t, fStr, (va_list)&t);
}

template <
	AllocFunc allocFunc = nullptr,
	ReallocFunc reallocFunc = nullptr,
	FreeFunc freeFunc = nullptr,
	void * param = nullptr,
	class T>

void xVect_Prepend(char *& t, const char * fStr, T args, ...) {

	xVect_vPrepend<allocFunc, reallocFunc, freeFunc, param>(t, (va_list)&args);
}

//IsFinite
template <class T> bool xVect_IsFinite(T * t) {

	bool isFinite = true;

	for (int i = 0, I = xVect_Count(t); i<I; i++) {
		if ((t[i] != t[i]) || (t[i] && t[i] == 2 * t[i])) {
			isFinite = false;
			break;
		}
	}

	return isFinite;
}

template <
	AllocFunc allocFunc = nullptr,
	ReallocFunc reallocFunc = nullptr,
	FreeFunc freeFunc = nullptr,
	void * param = nullptr>
	
void xVect_vsprintf(char *& xVect_buffer, const char * formatString, va_list args) {

	static WinCriticalSection cs;

#ifdef XVECT_NO_STATIC_BUFFER
	char * buffer = nullptr;
#else
	Critical_Section(cs) {
		static char * buffer;
#endif

		//to account for allocated static buffer 
		//if (!buffer) g_accAllocated_xVect--;

		int num = vsnprintf(NULL, 0, formatString, args);
		xVect_Redim<false, false, allocFunc, reallocFunc, param>(buffer, num + 1);
		vsnprintf(buffer, num + 1, formatString, args);

		xVect_Copy<allocFunc, reallocFunc, param>(xVect_buffer, buffer);

#ifndef XVECT_NO_STATIC_BUFFER
	}
#else
	xVect_Free<freeFunc, param>(buffer);
#endif
}

template <
	AllocFunc allocFunc = nullptr,
	void * param = nullptr>
	
char * xVect_vAsprintf(const char * formatString, va_list args) {

	int num = vsnprintf(NULL, 0, formatString, args);
	char * retVal = xVect_Create<char, false, allocFunc, param>(num + 1);
	vsnprintf(retVal, num + 1, formatString, args);

	return retVal;
}

template <
	AllocFunc allocFunc = nullptr,
	ReallocFunc reallocFunc = nullptr,
	FreeFunc freeFunc = nullptr,
	void * param = nullptr,
	class T>
	
char * xVect_sprintf(char *& xVect_buffer, const char * formatString, T args, ...) {

	xVect_vsprintf<allocFunc, reallocFunc, freeFunc, param>(xVect_buffer, formatString, (va_list)&args);
	return xVect_buffer;
}

template <
	AllocFunc allocFunc = nullptr,
	void * param = nullptr,
	class... Args>
char * xVect_Asprintf(const char * formatString, Args&&... args) {

	int num = _scprintf(formatString, std::forward<Args>(args)...);
	char * retVal = xVect_Create<char, false, allocFunc, param>(num + 1);
	sprintf(retVal, formatString, std::forward<Args>(args)...);

	return retVal;
}

template <
	AllocFunc allocFunc = nullptr,
	void * param = nullptr,
	class... Args>
wchar_t * xVect_Asprintf(const wchar_t * formatString, Args&&... args) {

	int num = _scwprintf(formatString, std::forward<Args>(args)...);
	wchar_t * retVal = xVect_Create<wchar_t, false, allocFunc, param>(num + 1);
	swprintf(retVal, num + 1, formatString, std::forward<Args>(args)...);

	return retVal;
}

template <
	AllocFunc allocFunc = nullptr,
	ReallocFunc reallocFunc = nullptr,
	FreeFunc freeFunc = nullptr,
	void * param = nullptr>
	
void xVect_vAppend(
	char *& t,
	const char * fStr,
	va_list args) 
{

	if (xVect_Count(t) == 0) {
		xVect_vsprintf<allocFunc, reallocFunc, freeFunc, param>(t, fStr, args);
	}
	else {

#ifdef XVECT_NO_STATIC_BUFFER
		char * buf = nullptr;
#else
		static WinCriticalSection cs;
		Critical_Section(cs) {
			static char * buf;
#endif

			//to account for allocated static buffer 
			//if (!buf) g_accAllocated_xVect--;

			xVect_vsprintf<allocFunc, reallocFunc, freeFunc, param>(buf, fStr, args);

			xVect_Append<allocFunc, reallocFunc, freeFunc, param>(t, buf);
#ifndef XVECT_NO_STATIC_BUFFER
		}
#else
		xVect_Free<freeFunc, param>(buf)
#endif
	}
}

template <
	AllocFunc allocFunc = nullptr,
	ReallocFunc reallocFunc = nullptr,
	FreeFunc freeFunc = nullptr,
	void * param = nullptr>

void xVect_vPrepend(
	char *& t,
	const char * fStr,
	va_list args)
{

	if (xVect_Count(t) == 0) {
		xVect_vsprintf<allocFunc, reallocFunc, freeFunc, param>(t, fStr, args);
	}
	else {
#ifdef XVECT_NO_STATIC_BUFFER
		char * buf = nullptr;
#else
		static WinCriticalSection cs;
		Critical_Section(cs) {
			static char * buf;
#endif
			xVect_vsprintf<allocFunc, reallocFunc, freeFunc, param>(buf, fStr, args);
			xVect_Prepend<allocFunc, reallocFunc, freeFunc, param>(t, buf);

#ifndef	XVECT_NO_STATIC_BUFFER
		}
#else
		xVect_Free<freeFunc, param>(buf)
#endif
	}
}

struct EmptyClass {
	template <class... Args> EmptyClass(Args&&... args) {}
	template <class... Args> void operator()(Args&&... args) {}
};

template <class...>
struct EmptyTemplateClass {
	template <class... Args> EmptyTemplateClass(Args&&... args) {}
	template <class... Args> void operator()(Args&&... args) {}
};

class CallDoNothingConstructor {};
class RawMoveable {};
class TrivialType {};

template<class A, bool B> struct TestClass2 { typedef EmptyClass type; };
template<class A> struct TestClass2 < A, true > { typedef A type; };
class NoNullCheck{};

struct Fix_mVect_Data_struct {
	void * new_t;
	void * old_t;
	void * old_t_end;
};

typedef PlaceHolder<Fix_mVect_Data_struct> * Fix_mVect_Data;

inline
void* operator new (std::size_t, void* ptr, NoNullCheck) {
	__assume(ptr != nullptr);
	return ptr;
}

inline
void operator delete (void*, void*, NoNullCheck) {}


#define CREATE_MEMBER_DETECTOR(X)                                                   \
template<typename T> class Detect_Member_##X {                                      \
    struct Fallback { int X; };                                                     \
    struct Derived : T, Fallback { };                                               \
                                                                                    \
    template<typename U, U> struct Check;                                           \
                                                                                    \
    typedef char ArrayOfOne[1];                                                     \
    typedef char ArrayOfTwo[2];                                                     \
                                                                                    \
    template<typename U> static ArrayOfOne & func(Check<int Fallback::*, &U::X> *); \
    template<typename U> static ArrayOfTwo & func(...);                             \
  public:                                                                           \
    typedef Detect_Member_##X type;                                                 \
    enum { value = sizeof(func<Derived>(0)) == 2 };                                 \
};

//CREATE_MEMBER_DETECTOR(Fix_mVect);

inline const char * StrChr(const char * str, char c) {
	return strchr(str, c);
}

inline const wchar_t * StrChr(const wchar_t * str, wchar_t c) {
	return wcschr(str, c);
}

inline char * StrChr(char * str, char c) {
	return strchr(str, c);
}

inline wchar_t * StrChr(wchar_t * str, wchar_t c) {
	return wcschr(str, c);
}

template <class T> T * FindNextToken(T * str) {

	auto nextToken = StrChr(str, '%');

	while (nextToken && nextToken[1] == '%') {
		nextToken += 2;
		nextToken = StrChr(nextToken, '%');
	}

	return nextToken;
}


template <bool cast, class Cast> struct AppendCasted{
	template <class T, class U> AppendCasted(aVect<T> & buffer, T * fStr, U&& t) {
		buffer.Append(fStr, t.GetDataPtr());
	}
};

template <class Cast> struct AppendCasted<false, Cast> {
	template <class T, class U> AppendCasted(aVect<T> & buffer, T * fStr, U&& t) {
		buffer.Append(fStr, t);
	}
};

template <class T>
inline void xFormatCore(aVect<T> & buffer, T * fStr) {
	if (fStr) MY_ERROR("Too many format specifiers");
}

template <class T, class CurArg, class... Args>
void xFormatCore(aVect<T> & buffer, T * fStr, CurArg&& curArg, Args&&... args) {

	if (!fStr) MY_ERROR("Too many parameters");

	auto nextToken = FindNextToken(fStr + 1);

	if (*fStr != '%') MY_ERROR("gotcha");

	if (nextToken) *nextToken = 0;

	AppendCasted<IsAvect<std::remove_reference<CurArg>::type>::value
		|| IsMvect<std::remove_reference<CurArg>::type>::value, T*>
		(buffer, fStr, curArg);

	if (nextToken) *nextToken = '%';

	xFormatCore(buffer, nextToken, std::forward<Args>(args)...);
}

template <class T, class... Args>
void xFormat(aVect<T> & buffer, const T * fStr, Args&&... args) {

	auto token = FindNextToken(fStr);

	if (!token) {
		buffer = fStr;
		return;
	}

#ifdef XVECT_NO_STATIC_BUFFER
	aVect<T> formatBuffer;
#else
	static WinCriticalSection cs;
	Critical_Section(cs) {
		static aVect<T> formatBuffer;
#endif

		formatBuffer = fStr;

		size_t len = token - fStr;

		buffer.Redim(len + 1).Last() = 0;
		memcpy(buffer, fStr, len * (sizeof T));

		xFormatCore(buffer, (T*)formatBuffer + len, std::forward<Args>(args)...);
#ifndef XVECT_NO_STATIC_BUFFER
	}
#endif
}

template <class T, class... Args>
aVect<T> xFormat(const T * fStr, Args&&... args) {
	aVect<T> retVal;
	xFormat(retVal, fStr, std::forward<Args>(args)...);
	return std::move(retVal);
}

template <class T, class... Args>
void xPrintf(const T * fStr, Args&&... args) {

#ifdef XVECT_NO_STATIC_BUFFER
	aVect<T> buffer;
#else
	static WinCriticalSection cs;
	Critical_Section(cs) {
		static aVect<T> buffer;
#endif

	buffer.Format(fStr, std::forward<Args>(args)...).Print();

#ifndef XVECT_NO_STATIC_BUFFER
	}
#endif
}

template <class T>
class CharPointerCore;

using CharPointer = CharPointerCore<char>;
using ConstCharPointer = CharPointerCore<const char>;

using WCharPointer = CharPointerCore<wchar_t>;
using ConstWCharPointer = CharPointerCore<const wchar_t>;

template <
	class T,
	bool owner = true,
	AllocFunc allocFunc = nullptr,
	ReallocFunc reallocFunc = nullptr,
	FreeFunc freeFunc = nullptr,
	void* param = nullptr>
class mVect;

template <
	class T,
	bool owner,
	bool isMvect,
	AllocFunc allocFunc,
	ReallocFunc reallocFunc,
	FreeFunc freeFunc,
	void* param>
class aVect;

template <bool condition, class yes, class no> struct ConditionalType {
	typedef yes type;
};

template <class yes, class no> struct ConditionalType <false, yes, no> {
	typedef no type;
};

template <class T>
struct IsTrivialType {
	enum { value = std::is_trivial<T>::value || std::is_base_of<TrivialType, T>::value };
};

template <class T>
struct IsRawMovable {
	enum { value = IsTrivialType<T>::value || std::is_base_of<RawMoveable, T>::value };
};

template <
	class T, 
	bool owner, 
	bool isMvect,
	AllocFunc allocFunc,
	ReallocFunc reallocFunc, 
	FreeFunc freeFunc, 
	void* param>

class aVect {

	T * pData;
	size_t count;
	size_t allocatedCount;

	template<class> friend class CachedVect;
	template<class, bool, bool, AllocFunc, ReallocFunc, FreeFunc, void*> friend class aVect;
	template<class, bool, AllocFunc, ReallocFunc, FreeFunc, void*> friend class mVect;

	enum { isMvect = isMvect };
	enum { moveGuaranty = IsAvect<T>::value || IsMvect<T>::value };
	//enum { isTrivialType = std::is_trivial<T>::value || std::is_base_of<TrivialType, T>::value};
	//enum { isRawMovable = isTrivialType || std::is_base_of<RawMoveable, T>::value };

	typedef void(*MoveFunc)(T * oldPtr, T * newPtr, size_t oldCount, size_t newCount, void * param);
	typedef mVect<T, owner, allocFunc, reallocFunc, freeFunc, param> mVectEq;
	typedef aVect<T, owner, isMvect, allocFunc, reallocFunc, freeFunc, param> selfType;

	template <bool initialize, class... Args> void ConstructorInitialization(size_t oldCount, size_t newCount, Args&&... args) {
		for (size_t i = oldCount; i<newCount; ++i) new(this->pData + i, NoNullCheck()) T (std::forward<Args>(args)...);
	}

	template <> void ConstructorInitialization<false>(size_t oldCount, size_t newCount) {}
	
	template <
		bool cpyCtor, 
		bool isThatmVect,
		AllocFunc thatAllocFunc, 
		ReallocFunc thatReallocFunc,
		FreeFunc thatFreeFunc,
		void * thatParam>
	aVect & CopyCore(const aVect<T, owner, isThatmVect, thatAllocFunc, thatReallocFunc, thatFreeFunc, thatParam> & that) {

		static_assert(owner, "no allocated ressource if owner = false");

		//TODO
		//if (!IsTrivialType<T>::value) g_CopyRecursion_xVect++;

		if (!cpyCtor && isMvect) mThis()->mVect_Check<isMvect>();

		size_t nT = that.count;

		if (nT) {

			if (cpyCtor) {
				this->Vect_Create<cpyCtor>(nT);
			}
			else {
				size_t n = this->count;

				if (!IsTrivialType<T>::value) {
					if (n) this->RedimCore<false, false, true, false, false>(0);//to free elements
					this->RawRedim(nT);
				} else {
					if (n != nT) this->RawRedim(nT);
				}
			}

			if (!IsTrivialType<T>::value) {
				for (size_t i = 0; i<nT; ++i) new(this->pData + i, NoNullCheck()) T (that.pData[i]);
				//TODO
//				if (g_CopyRecursion_xVect == 1) {
//#if defined(_DEBUG) || defined(MY_DEBUG) 
//					if (!g_pOwnerToClear_xVect) g_accAllocated_xVect--;//to account for static alloc
//#endif
//					size_t count = ((aVect<void***>*)&g_pOwnerToClear_xVect)->Count();
//					for (size_t i = count - 1; i < count; --i) *(g_pOwnerToClear_xVect[i]) = nullptr;
//					((aVect<void***>*)&g_pOwnerToClear_xVect)->RawRedim<false, false, false>(0);
//#if defined(_DEBUG) || defined(MY_DEBUG) 
//					((aVect<void***>*)&g_pOwnerToClear_xVect)->Vect_BasePtr()->allocID = 0x1707711E470C1E44;
//#endif
//				}
			}
			else {
				memmove(this->pData, (T*)that.GetDataPtr(), nT*sizeof(T));
			}
		}
		else {
			if (cpyCtor) {
				this->pData = nullptr;
				this->count = 0;
				this->allocatedCount = 0;
				if (isMvect) {
					mThis()->ref.pData = nullptr;
					mThis()->ref.count = 0;
					mThis()->ref.allocatedCount = 0;
				}
			}
			else this->Free();
		}

		//TODO
		//if (!IsTrivialType<T>::value) g_CopyRecursion_xVect--;

		if (isMvect) mThis()->mVect_Check<isMvect>();

		return *this;
	}

#ifdef XVECT_BREAK_AT_LARGE_ALLOC
	void CheckLargeAlloc(size_t deltaSize) {

		if (deltaSize > 1200000000) {
			__debugbreak();
		}

		PROCESS_MEMORY_COUNTERS_EX pmc;

		auto ok = GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&pmc, sizeof pmc);

		if (ok && pmc.PrivateUsage + deltaSize > 12000000000) __debugbreak();
	}
#endif

	template<
		bool reserve,
		bool exactAlloc,
		MoveFunc moveFunc> 
	__declspec(noinline) void RawRedimCore(size_t newCount, void * moveFuncParam) {

		if (!this->pData && newCount) {
			this->Vect_Create<false, reserve>(newCount);
		}
		else {
			size_t finalCount;
			if (reserve) {
				finalCount = newCount;
				newCount = this->count;
			}
			else if (exactAlloc) {
				finalCount = newCount;
			}
			else {
				SIZE_T_OVERFLOW_CHECK_ADD(newCount, this->allocatedCount);
				SIZE_T_OVERFLOW_CHECK_ADD(newCount + this->allocatedCount, 5);
				finalCount = newCount + this->allocatedCount + 5;
			}

			size_t newSize = this->ComputeSize(finalCount, 0);

#ifdef XVECT_BREAK_AT_LARGE_ALLOC
			CheckLargeAlloc(newSize - this->count);
#endif

			if (IsTrivialType<T>::value || IsAvect<T>::value || IsRawMovable<T>::value) {

#if defined(_DEBUG) || defined(MY_DEBUG) 
				T * newPtr = (T*)(reallocFunc ? reallocFunc(this->pData, newSize, param) : _realloc_dbg(this->pData, newSize, _CLIENT_BLOCK, nullptr, 0));
#else
				T * newPtr = (T*)(reallocFunc ? reallocFunc(this->pData, newSize, param) : realloc(this->pData, newSize));
#endif

				if (!newPtr) MY_ERROR("realloc failure");

				if (moveFunc) moveFunc(this->pData, newPtr, this->count, newCount, moveFuncParam);
				this->pData = newPtr;
			}
			else {

#if defined(_DEBUG) || defined(MY_DEBUG) 
				T * newPtr = (T*)(allocFunc ? allocFunc(newSize, param) : _malloc_dbg(newSize, _CLIENT_BLOCK, nullptr, 0));
#else
				T * newPtr = (T*)(allocFunc ? allocFunc(newSize, param) : malloc(newSize));
#endif

				if (!newPtr) MY_ERROR("malloc failure");

				if (moveFunc) moveFunc(this->pData, newPtr, this->count, newCount, moveFuncParam);
				else {
					for (size_t i = 0; i < this->count; ++i) {
						new (newPtr + i, NoNullCheck()) T(std::move(this->pData[i]));
						if (!moveGuaranty) this->pData[i].~T();
					}
				}

				if (freeFunc) freeFunc(this->pData, param);
#if defined(_DEBUG) || defined(MY_DEBUG) 
				else _free_dbg(this->pData, _CLIENT_BLOCK);
#else
				else free(this->pData);
#endif

				this->pData = newPtr;
			}
			this->count = newCount;
			this->allocatedCount = finalCount;
			if (isMvect && mThis()->ref) mThis()->mVect_UpdateRef<isMvect, true, true, true>();
		}
	}

	template <
		bool grow = false, 
		bool reserve = false, 
		bool exactAlloc = false,
		MoveFunc moveFunc = nullptr> 

	T * RawRedim(size_t newCount, void * moveFuncParam = nullptr) {

		T * oldPtr = this->pData;

		if (isMvect) mThis()->mVect_Check<isMvect>();

		if (grow || reserve) newCount += this->count;

		if ((exactAlloc && this->allocatedCount != newCount) || newCount > this->allocatedCount) {
			this->RawRedimCore<reserve, exactAlloc, moveFunc>(newCount, moveFuncParam);
		} else {
			if (!reserve) this->count = newCount;
			if (isMvect && mThis()->ref) mThis()->mVect_UpdateRef<isMvect, false, true, false>();
		}

		if (isMvect) mThis()->mVect_Check<isMvect>();

		return oldPtr;
	}

	template <
		bool grow = false,
		bool reserve = false, 
		bool assumeNonEmpty = false,
		bool exactAlloc = false,
		bool initialize = true,
		bool destroy = true,
		class... Args>

	T * RedimCore(size_t newCount, Args&&... args) {

		static_assert(owner, "no allocated ressource if owner = false");

		size_t oldCount = this->count;

		if (!reserve && !exactAlloc && !grow && oldCount == newCount) return nullptr;

		T * oldPtr = this->pData;

		if (IsTrivialType<T>::value || (!destroy && !initialize)) {
			this->RawRedim<grow, reserve, exactAlloc>(newCount);
			if (sizeof...(Args)) {
				this->ConstructorInitialization<initialize>(oldCount, newCount, std::forward<Args>(args)...);
			}
		}
		else if (newCount < oldCount) {
			if (destroy) for (size_t i = newCount; i<oldCount; ++i) this->pData[i].~T();
			this->RawRedim<grow, reserve, exactAlloc> (newCount);
		}
		else if (oldCount < newCount) {
			this->RawRedim<grow, reserve, exactAlloc>(newCount);
			if (initialize) this->ConstructorInitialization<initialize>(oldCount, newCount, std::forward<Args>(args)...);
		}

		if (this->pData != oldPtr) {
			return oldPtr;
		}

		return nullptr;
	}

	template <class... U> //__declspec(noinline) 
	bool CheckPtr(T * oldPtr, size_t oldCount, U&&... arg) {
		return false;
	}

	bool __declspec(noinline) CheckPtr(T * oldPtr, size_t oldCount, size_t ind, T && el) {

		if (&el >= oldPtr && &el < oldPtr + oldCount) {// check if el is (was) in the array
			T * newPtr = (T*)((char*)&el + (size_t)((char*)this->pData - (char*)oldPtr));
			new (this->pData + ind, NoNullCheck()) T(std::move(*newPtr));
			return true;
		}
		return false;
	}

	bool __declspec(noinline) CheckPtr(T * oldPtr, size_t oldCount, size_t ind, T & el) {

		if (&el >= oldPtr && &el < oldPtr + oldCount) {// check if el is (was) in the array
			T * newPtr = (T*)((char*)&el + (size_t)((char*)this->pData - (char*)oldPtr));
			new (this->pData + ind, NoNullCheck()) T(*newPtr);
			return true;
		}
		return false;
	}

	template <bool realloc, class... U> //__declspec(noinline) 
	bool CheckPtr2(T * oldPtr, size_t oldCount, size_t i, U&&... arg) {
		return false;
	}

	template <bool realloc> __declspec(noinline)
	bool CheckPtr2(T * oldPtr, size_t oldCount, size_t ind, T & el) {
		if (&el >= this->pData && &el < this->pData + oldCount) {// el is in the array
			//if el is at or after insert point, increment
			if (size_t(&el - this->pData) >= ind) {//el - this->pData cannot be negative because el is in the array
				T * newEl = &el + 1;
				this->InsertCore<realloc, false>(ind, *newEl);
				return true;
			}
		}
		return false;
	}
	
	template <bool realloc> __declspec(noinline) 
	bool CheckPtr2(T * oldPtr, size_t oldCount, size_t ind, T && el) {
		if (&el >= this->pData && &el < this->pData + oldCount) {// el is in the array
			//if el is at or after insert point, increment
			if (size_t(&el - this->pData) >= ind) {//el - this->pData cannot be negative because el is in the array
				T * newEl = &el + 1;
				this->InsertCore<realloc, false>(ind, std::move(*newEl));
				return true;
			}
		}
		return false;
	}

	template <class... Args> 
	__declspec(noinline) aVect & PushCoreRealloc(Args&&...  args) {
		return PushCoreTemplate<true>(std::forward<Args>(args)...);
	}

	template <class... Args>
	aVect & PushCoreNoRealloc(Args&&...  args) {
		return PushCoreTemplate<false>(std::forward<Args>(args)...);
	}

	template <bool realloc, class... Args> inline
	aVect & PushCoreTemplate(Args&&...  args) {

		static_assert(owner, "no allocated ressource if owner = false");
		size_t oldCount = this->count;

		if (isMvect) mThis()->mVect_Check<isMvect>();

		if (realloc) {
			T * oldPtr = this->RawRedim<true, false>(1);
			if (oldPtr && oldPtr != this->pData) {
				if (this->CheckPtr(oldPtr, oldCount, oldCount, std::forward<Args>(args)...)) return *this;
			}
		}
		 
		new (this->pData + oldCount, NoNullCheck()) T(std::forward<Args>(args)...);

		if (!realloc) {
			this->count++;
			if (isMvect && mThis()->ref) mThis()->mVect_UpdateRef<isMvect, false, true, false>();
		}

		return *this;
	}

	static __declspec(noinline)
	void InsertMoveFunc(T * oldPtr, T * newPtr, size_t oldCount, size_t newCount, void * param) {

		size_t & ind = *(size_t*)param;

		if (IsTrivialType<T>::value || IsAvect<T>::value || IsRawMovable<T>::value) {//comes from realloc : memory already has been moved
			memmove(&newPtr[ind + 1], &newPtr[ind], sizeof(T) * (newCount - ind));
		}
		else {
			if (oldPtr != newPtr) {
				for (size_t i = 0; i < ind; ++i) {
					new (&newPtr[i], NoNullCheck()) T(std::move(oldPtr[i]));
					if (!moveGuaranty) oldPtr[i].~T();
				}
			}

			for (size_t i = oldCount - 1; i >= ind && i < oldCount + 1; --i) {
				new(&newPtr[i + 1], NoNullCheck()) T (std::move(oldPtr[i]));
				if (!moveGuaranty) oldPtr[i].~T();
			}
		}
	}

	template <bool realloc, bool check, class... Args>
	aVect & InsertCore(size_t ind, Args&&... args) {

		static_assert(owner, "no allocated ressource if owner = false");

		const size_t oldCount = this->count;

		if (isMvect) mThis()->mVect_Check<isMvect>();

		if (check && this->CheckPtr2<realloc>(this->pData, oldCount, ind, std::forward<Args>(args)...)) return *this;

		T * oldPtr = nullptr;

		if (realloc){
			T * oldPtr = RawRedim<true, false, false, this->InsertMoveFunc>(1, &ind);
			if (oldPtr && oldPtr != this->pData) {
				if (this->CheckPtr(oldPtr, oldCount + 1, ind, std::forward<Args>(args)...)) return *this;
			}
		}
		
		//if oldPtr!=nullptr, RawRedim was called (with this->InsertMoveFunc), so elements are already moved
		if (!realloc && !oldPtr) {//else we need to move them
			if (IsTrivialType<T>::value || IsAvect<T>::value || IsRawMovable<T>::value) {
				memmove(this->pData + ind + 1, this->pData + ind, sizeof(T) * (oldCount - ind));
			}
			else {
				for (size_t j = oldCount - 1; j >= ind && j < oldCount + 1; --j) {
					new(this->pData + j + 1, NoNullCheck()) T (std::move(this->pData[j]));
					if (!moveGuaranty) this->pData[j].~T();
				}
			}
		}

		new (this->pData + ind, NoNullCheck()) T (std::forward<Args>(args)...);

		if (!realloc) {
			this->count++;
			if (isMvect && mThis()->ref) mThis()->mVect_UpdateRef<isMvect, false, true, false>();
		}

		if (isMvect) mThis()->mVect_Check<isMvect>();

		return *this;
	}
	
	size_t ComputeSize(size_t count, size_t add) {
		SIZE_T_OVERFLOW_CHECK_ADD(count, add);
		count += add;
		SIZE_T_OVERFLOW_CHECK_MULT(count, sizeof(T));
		return count*sizeof(T);
	}

	template <bool fromCtor, bool reserve = false> 
	void Vect_Create(size_t count) {

#if defined(_DEBUG) || defined(MY_DEBUG) 
	if (isMvect) {
		g_accAllocated_yVect++;
		g_nAllocation_yVect++;
	}
	else {
		g_accAllocated_xVect++;
		g_nAllocation_xVect++;
	}
#endif

		size_t reserveSize = reserve ? 0 : 3;

		size_t size = this->ComputeSize(count, reserveSize);

#ifdef XVECT_BREAK_AT_LARGE_ALLOC
		CheckLargeAlloc(size);
#endif

		if (size) {
#if defined(_DEBUG) || defined(MY_DEBUG) 
			//if (size == 20024) __debugbreak();
			this->pData = (T*)((AllocFunc)allocFunc ? allocFunc(size, param) : _malloc_dbg(size, _CLIENT_BLOCK, nullptr, 0)); 
#else
			this->pData = (T*)((AllocFunc)allocFunc ? allocFunc(size, param) : malloc(size));
#endif
			if (!this->pData) MY_ERROR("malloc failed");
		}
		else this->pData = nullptr;

		this->count = reserve ? 0 : count;
		this->allocatedCount = count + reserveSize;

		if (isMvect) {
			if (!fromCtor && mThis()->ref) mThis()->mVect_UpdateRef<isMvect, true, true, true>();
		}
	}

	void Free_xVect() {
		
		if (!owner) {
			this->pData = nullptr;
			this->count = 0;
			this->allocatedCount = 0;
			return;
		}

		if (!this->pData) return;

		if (!IsTrivialType<T>::value) {
			for (size_t i = this->count - 1; i < this->count; --i) {
#ifdef TMVX_BUGHUNT_1
#if defined(_DEBUG) || defined(MY_DEBUG) 
				_CrtCheckMemory();
#else
				if (!HeapValidate(GetProcessHeap(), 0, NULL)) MY_ERROR("Heap corruption");
#endif
#endif
				this->pData[i].~T();
			}
#ifdef TMVX_BUGHUNT_1
#if defined(_DEBUG) || defined(MY_DEBUG) 
			_CrtCheckMemory();
#else
			if (!HeapValidate(GetProcessHeap(), 0, NULL)) MY_ERROR("Heap corruption"); 
#endif
#endif
		}

#ifdef XVECT_DISPFREE
		printf("freeing %d bytes @ x0%p\n", this->allocatedCount * sizeof(T), this->pData);
#endif

		if (freeFunc) freeFunc(this->pData, param);

#if defined(_DEBUG) || defined(MY_DEBUG) 
		else _free_dbg(this->pData, _CLIENT_BLOCK);
#else
		else free(this->pData);
#endif

		this->pData = nullptr;
		this->count = 0;
		this->allocatedCount = 0;
	}
	
	template <
		bool moveCtor,
		bool assumeThisNonEmpty,
		bool assumeThatNonEmpty,
		bool thatOwner,
		bool isThatmVect,
		AllocFunc thatAllocFunc,
		ReallocFunc thatReallocFunc,
		FreeFunc thatFreeFunc,
		void * thatParam>
	void Move(aVect<T, thatOwner, isThatmVect, thatAllocFunc, thatReallocFunc, thatFreeFunc, thatParam> & that) {

		if (moveCtor && !assumeThatNonEmpty && that.allocatedCount == 0) {
			this->pData = nullptr;
			this->count = 0;
			this->allocatedCount = 0;
			if (isMvect) {
				if (that.isMvect) {
					mThis()->mVect_Move<isMvect>(*that.mThis());
				} else {
					mThis()->ref.pData = nullptr;
					mThis()->ref.count = 0;
					mThis()->ref.allocatedCount = 0;
				}
			}
			return;
		}

		if (!moveCtor) {
			if (isMvect) mThis()->mVect_Check<isMvect>();
			this->~aVect();//release (if mVect) or free (if aVect) 
		}

		if (isMvect) {
			if (that.isMvect) {
				mThis()->mVect_Move<isMvect>(*that.mThis());
				return;
			}
			else {
				mThis()->ref.pData = nullptr;
				mThis()->ref.count = 0;
				mThis()->ref.allocatedCount = 0;
			}

			this->pData = that.pData;
			this->count = that.count;
			this->allocatedCount = that.allocatedCount;
			that.pData = nullptr;
			that.count = 0;
			that.allocatedCount = 0;

			mThis()->mVect_Check<isMvect>();
		}
		else {
			this->pData = that.pData;
			this->count = that.count;
			this->allocatedCount = that.allocatedCount;
			that.pData = nullptr;
			that.count = 0;
			that.allocatedCount = 0;

			if (that.isMvect && that.mThis()->ref) {
				that.mThis()->mVect_UpdateRef<isMvect, true, true, true>();
				that.mThis()->mVect_Check<true>();
			}
		}
	}

	mVectEq * mThis() {
		return (mVectEq*)this;
	}

public:

	aVect() {
		this->pData = nullptr;
		this->count = 0;
		this->allocatedCount = 0;

		if (isMvect) MY_ERROR("Not allowed in constructor");
	}

	template <size_t n>
	aVect(T(&x)[n]) {
		this->Vect_Create<true>(n);
		for (size_t i = 0; i < n; ++i) new(this->pData + i, NoNullCheck()) T(x[i]);

		if (isMvect) MY_ERROR("Not allowed in constructor");
	}

	aVect(const aVect & that) {
		static_assert(owner, "no allocated ressource if owner = false");
		this->CopyCore<true>(that);

		if (isMvect) MY_ERROR("Not allowed in constructor");
	}

	aVect(aVect && that) {
		static_assert(owner, "no allocated ressource if owner = false");
		this->Move<true, false, false>(that);
	}

	aVect(CallDoNothingConstructor & that) {}

	aVect(CallDoNothingConstructor && that) {}

	template <class... Args> explicit
	aVect(size_t n, Args&&... args) {
		static_assert(owner, "no allocated ressource if owner = false");
		this->Vect_Create<true>(n);
		if (!IsTrivialType<T>::value) for (size_t i = 0; i < n; ++i) new(this->pData + i, NoNullCheck()) T (std::forward<Args>(args)...);
	}

	aVect(const char * str) { 

		static_assert(owner, "should use const char type if not owner");
		static_assert(std::is_same<T, char>::value || std::is_same<T, wchar_t>::value, 
			"only for aVect<char> or aVect<wchar_t>");

		if (!str) {
			this->pData = nullptr;
			this->count = this->allocatedCount = 0;
			return;
		}

		if (std::is_same<T, char>::value) {
			this->Vect_Create<true>(strlen(str) + 1);
			memcpy(this->pData, str, this->count);
		}
		else {
			this->pData = nullptr;
			this->count = this->allocatedCount = 0;
			this->_sprintf_if<std::is_same<T, wchar_t>::value>(L"%S", str);
		}
	}

private:
	template <class T, bool enable> struct SprintfIf {
		template <class... Args>
		static void sprintf(T& t, Args&&... args) {
			t.sprintf(std::forward<Args>(args)...);
		}
	};

	template <class T> struct SprintfIf<T, false> {
		template <class... Args>
		static void sprintf(T& t, Args&&... args) {
			__debugbreak();// MY_ERROR("not supposed to be run");
		}
	};

	template <bool enable, class... Args>
	void _sprintf_if(Args&&... args) {
		SprintfIf<decltype(*this), enable>::sprintf(*this, std::forward<Args>(args)...);
	}

public:

	aVect(const wchar_t * str) {

		static_assert(owner, "should use const char type if not owner");
		static_assert(std::is_same<T, char>::value || std::is_same<T, wchar_t>::value, 
			"only for aVect<char> or aVect<wchar_t>");

		if (!str) {
			this->pData = nullptr;
			this->count = this->allocatedCount = 0;
			return;
		}
		
		if (std::is_same<T, wchar_t>::value) {
			this->Vect_Create<true>(wcslen(str) + 1);
			memcpy(this->pData, str, this->count * sizeof(wchar_t));
		}
		else {
			this->pData = nullptr;
			this->count = this->allocatedCount = 0;
			this->_sprintf_if<std::is_same<T, char>::value>("%S", str);
		}
	}

	template <class... Args>
	aVect(const char * fStr, Args&&... args) {

		static_assert(owner, "no allocated ressource if owner = false");
		static_assert(std::is_same<T, char>::value, "only for aVect<char>");

		PrintfStaticArgChecker(std::forward<Args>(args)...);

		int num = _scprintf(fStr, std::forward<Args>(args)...);
		this->Vect_Create<true>(num + 1);
		::sprintf(this->pData, fStr, std::forward<Args>(args)...);
	}

	template <class... Args>
	aVect(const wchar_t * fStr, Args&&... args) {

		static_assert(owner, "no allocated ressource if owner = false");
		static_assert(std::is_same<T, wchar_t>::value, "only for aVect<wchar_t>");

		PrintfStaticArgChecker(std::forward<Args>(args)...);

		int num = _scwprintf(fStr, std::forward<Args>(args)...);
		this->Vect_Create<true>(num + 1);
		::swprintf(this->pData, num + 1,fStr, std::forward<Args>(args)...);
	}

	aVect(const std::initializer_list<T>& l) {

		auto size = l.size();
		this->Vect_Create<true>(size);

		auto tPtr = this->pData;
		auto lPtr = l.begin();
		for ( ; lPtr != l.end(); tPtr++, lPtr++) {
			new(tPtr, NoNullCheck()) T (*lPtr);
		}
	}

	aVect(std::initializer_list<T>&& l) {

		auto size = l.size();
		this->Vect_Create<true>(size);

		auto tPtr = this->pData;
		//T * lPtr = *(T**)(l.begin());
		auto lPtr = l.begin();
		for (; lPtr != l.end(); tPtr++, lPtr++) {
			new(tPtr, NoNullCheck()) T(std::move(*lPtr));
		}
	}

	template <
		bool thatOwner,
		bool isThatmVect,
		AllocFunc thatAllocFunc,
		ReallocFunc thatReallocFunc,
		FreeFunc thatFreeFunc,
		void * thatParam>
	aVect(aVect<T, thatOwner, isThatmVect, thatAllocFunc, thatReallocFunc, thatFreeFunc, thatParam> & that) {

		static_assert(owner, "no allocated ressource if owner = false");
		this->CopyCore<true>(that);
	}

	template <
		bool thatOwner,
		bool isThatmVect,
		AllocFunc thatAllocFunc,
		ReallocFunc thatReallocFunc,
		FreeFunc thatFreeFunc,
		void * thatParam>
	aVect(aVect<T, thatOwner, isThatmVect, thatAllocFunc, thatReallocFunc, thatFreeFunc, thatParam> && that) {

		static_assert(owner, "no allocated ressource if owner = false");

		if (that.pData == nullptr && (that.isMvect && !that.ref.pData)) {
			this->pData = nullptr;
			this->count = 0;
			this->allocatedCount = 0;
			return;
		}

		this->Move<true, false, true>(that);
	}

	aVect & sprintf(const wchar_t * str) {

		static_assert(owner, "no allocated ressource if owner = false");
		static_assert(std::is_same<T, char>::value || std::is_same<T, wchar_t>::value,
			"only for aVect<char> or aVect<wchar_t> ");

		if (!str) {
			this->Free();
			return *this;
		}

		if (std::is_same<T, wchar_t>::value) {
			this->RawRedim(wcslen(str) + 1);
			memcpy(this->pData, str, this->count * sizeof(wchar_t));
		}
		else {
			this->_sprintf_if<std::is_same<T, char>::value>("%S", str);
		}
		

		return *this;
	}

	aVect & sprintf(const char * str) {

		static_assert(owner, "no allocated ressource if owner = false");
		static_assert(std::is_same<T, char>::value || std::is_same<T, wchar_t>::value, 
			"only for aVect<char> or aVect<wchar_t> ");

		if (!str) {
			//this->Free();
			this->Redim<false>(0);
			return *this;
		}

		if (std::is_same<T, char>::value) {
			this->RawRedim(strlen(str) + 1);
			memcpy(this->pData, str, this->count);
		}
		else {
			this->_sprintf_if<std::is_same<T, wchar_t>::value>(L"%S", str);
		}

		return *this;
	}

	template <class... Args>
	aVect & sprintf(const char * fStr, Args&&... args) {

		static_assert(owner, "no allocated ressource if owner = false");
		static_assert(std::is_same<T, char>::value, "only for aVect<char>");

		PrintfStaticArgChecker(std::forward<Args>(args)...);

#if defined(_DEBUG) || defined(MY_DEBUG) 
		//to account for allocated static buffer 
		//if (!g_aVect_char_sprintf_buffer) g_accAllocated_xVect--;
#endif

#ifdef XVECT_NO_STATIC_BUFFER
		aVect<char> g_aVect_char_sprintf_buffer;
#else
		static WinCriticalSection cs;

		Critical_Section(cs) {
			static aVect<char> g_aVect_char_sprintf_buffer;
#endif

			int num = _scprintf(fStr, std::forward<Args>(args)...);

			if (num == -1) {
				this->Copy("# PRINTF ERROR #");
				return *this;
			}

			g_aVect_char_sprintf_buffer.RawRedim(num + 1);
			::sprintf(g_aVect_char_sprintf_buffer, fStr, std::forward<Args>(args)...);

			if ((void*)this == (void*)&g_aVect_char_sprintf_buffer)
				return *this;

			this->RawRedim(num + 1);
			memcpy(this->pData, g_aVect_char_sprintf_buffer.pData, num + 1);

#ifndef XVECT_NO_STATIC_BUFFER
		}
#endif

		return *this;
	}

	template <class... Args>
	aVect & sprintf(const wchar_t * fStr, Args&&... args) {

		static_assert(owner, "no allocated ressource if owner = false");
		static_assert(std::is_same<T, wchar_t>::value, "only for aVect<wchar_t>");

		PrintfStaticArgChecker(std::forward<Args>(args)...);

#if defined(_DEBUG) || defined(MY_DEBUG) 
		//to account for allocated static buffer 
		//if (!g_aVect_wchar_sprintf_buffer) g_accAllocated_xVect--;
#endif

		int num = _scwprintf(fStr, std::forward<Args>(args)...);

		if (num == -1) {
			this->Copy(L"# PRINTF ERROR #");
			return *this;
		}

#ifdef XVECT_NO_STATIC_BUFFER
		aVect<wchar_t> g_aVect_wchar_sprintf_buffer;
#else
		static WinCriticalSection cs;

		Critical_Section(cs) {
			static aVect<wchar_t> g_aVect_wchar_sprintf_buffer;
#endif

			g_aVect_wchar_sprintf_buffer.RawRedim(num + 1);
			::swprintf(g_aVect_wchar_sprintf_buffer, num + 1, fStr, std::forward<Args>(args)...);

			if ((void*)this == (void*)&g_aVect_wchar_sprintf_buffer)
				return *this;

			this->RawRedim(num + 1);
			memcpy(this->pData, g_aVect_wchar_sprintf_buffer.pData, (num + 1)*sizeof(wchar_t));

#ifndef XVECT_NO_STATIC_BUFFER
		}
#endif

		return *this;
	}

	aVect & Print() {
		if (std::is_same<T, char>::value) {
			printf("%s", (char*)this->pData);
		}
		else {
			wprintf(L"%s", (wchar_t*)this->pData);
		}
		return *this;
	}

	template <class... Args>
	aVect & Format(const char * fStr, Args&&... args) {
		::xFormat(*this, fStr, std::forward<Args>(args)...);
		return *this;
	}

	template <class... Args>
	aVect & Format(const wchar_t * fStr, Args&&... args) {
		::xFormat(*this, fStr, std::forward<Args>(args)...);
		return *this;
	}

	aVect & RemoveBlanks() {

		static_assert(owner, "no allocated ressource if owner = false");
		static_assert(std::is_same<T, char>::value, "only for aVect<char>");

		if (!*this) return *this;

		::RemoveBlanks(this->pData);

		this->Redim(strlen(this->pData) + 1);

		return *this;
	}

	aVect & operator=(aVect && that) {
		this->Steal(that);
		return *this;
	}

	aVect & operator=(const aVect & that) {
		this->Copy(that);
		return *this;
	}

	template <size_t N>
	aVect & operator=(T(&that)[N]) {

		this->Redim(N);

		//no, lets trust that compiler will be better than memcpy types because N is known at compile time
		//if (IsTrivialType<T>::value) {
		//	memcpy(this->pData, that, sizeof(T) * N);
		//} else {
			for (size_t i = 0; i < N; ++i) this->pData[i] = that[i];
		//}

		return *this;
	}

	template <size_t N>
	aVect & operator=(const T(&that)[N]) {

		this->Redim(N);

		for (size_t i = 0; i < N; ++i) this->pData[i] = that[i];

		return *this;
	}

	aVect & operator=(const char * str) {

		static_assert(std::is_same<T, char>::value || std::is_same<T, wchar_t>::value,
			"only for aVect<char> or aVect<wchar_t> ");

		if (!str) {
			this->Redim(0);
			return *this;
		}

		if (std::is_same<T, char>::value) {
			this->sprintf(str);
		}
		else {
			this->_sprintf_if<std::is_same<T, wchar_t>::value>(L"%S", str);
		}
		
		return *this;
	}

	aVect & operator=(const wchar_t * str) {

		//static_assert(std::is_same<T, wchar_t>::value, "only for aVect<wchar_t>");
		static_assert(std::is_same<T, char>::value || std::is_same<T, wchar_t>::value,
			"only for aVect<char> or aVect<wchar_t>");

		if (!str) {
			this->Redim(0);
			return *this;
		}

		if (std::is_same<T, wchar_t>::value) {
			this->sprintf(str);
		}
		else {
			this->_sprintf_if<std::is_same<T, char>::value>("%S", str);
		}

		return *this;
	}

	template <
		bool thatOwner,
		bool isThatmVect,
		AllocFunc thatAllocFunc,
		ReallocFunc thatReallocFunc,
		FreeFunc thatFreeFunc,
		void * thatParam>
	aVect & Copy(const aVect<T, thatOwner, isThatmVect, thatAllocFunc, thatReallocFunc, thatFreeFunc, thatParam> & that) {
		return this->CopyCore<false>(that);
	}

	aVect & Copy(aVect && that) {
		return this->CopyCore<false>(that);
	}

	aVect Copy() const {
		//return selfType retVal(*this);
		return *this;
	}

	aVect & Copy(const char * str) {

		static_assert(std::is_same<T, char>::value, "only for aVect<char>");

		return this->sprintf(str);
	}

	aVect & Copy(const wchar_t * str) {

		static_assert(std::is_same<T, wchar_t>::value, "only for aVect<wchar_t>");

		return this->sprintf(str);
	}

	aVect & Copy(const mVect<T, owner, allocFunc, reallocFunc, freeFunc, param> & that) {
		return this->Copy(that.ToAvect());
	}

	template <
		bool isThatmVect,
		AllocFunc thatAllocFunc,
		ReallocFunc thatReallocFunc,
		FreeFunc thatFreeFunc,
		void * thatParam>
	aVect & Steal(aVect<T, owner, isThatmVect, thatAllocFunc, thatReallocFunc, thatFreeFunc, thatParam> & that) {

		if (isMvect) mThis()->mVect_Check<isMvect>();

		this->Move<false, false, false>(that);

		if (isMvect) mThis()->mVect_Check<isMvect>();
		return *this;
	}

	aVect & Steal(mVect<T, owner, allocFunc, reallocFunc, freeFunc, param> & that) {

		return this->Steal(*(aVect<T, owner, true, allocFunc, reallocFunc, freeFunc, param>*)&that);
	}

	//operator T*() {
	//	return this->pData;
	//}

	//operator const T*() const {
	//	return this->pData;
	//}

	//template<class U>
	//void NullFirstElIfChar(U*) const {}

	//template<>
	//void NullFirstElIfChar(char*) const {
	//	*(T*)(this->pData) = 0;
	//}

	//template<>
	//void NullFirstElIfChar(wchar_t*) const {
	//	*(T*)(this->pData) = 0;
	//}

	operator T*() const {
		//if (std::is_same<T, char>::value || std::is_same<T, wchar_t>::value) {
		//	if (this->pData && this->count == 0) {
		//		this->NullFirstElIfChar(this->pData);
		//	}
		//}

		if (this->count == 0) return nullptr;
		
		return this->pData;
	}

	//operator const T*() {
	//	return this->pData;
	//}

	template <bool dummy=false>
	aVect & RedimToStrlen() {
		static_assert(std::is_same<T, char>::value || std::is_same<T, wchar_t>::value, "only for aVect<char> or aVect<wchar_t>");

		if (std::is_same<T, char>::value) {
			auto l = strlen((char*)this->pData);
			this->RawRedim(l + 1);
		} else if (std::is_same<T, wchar_t>::value) {
			auto l = wcslen((wchar_t*)this->pData);
			this->RawRedim(l + 1);
		} else {
			__debugbreak();
		}

		return *this;
	}

	template <bool initialize = true, class... Args>
	aVect & Redim(size_t newCount, Args&&... args) {
#if defined(_DEBUG) || defined(MY_DEBUG) 
		if (!initialize && newCount > this->count) MY_ERROR("Redim<false> must not grow the array");
#endif
		if (IsTrivialType<T>::value && newCount <= this->allocatedCount && !sizeof...(Args)) {
			this->count = newCount;
			return *this;
		}

		this->RedimCore<false, false, false, false, initialize>(newCount, std::forward<Args>(args)...);
		return *this;
	}

	aVect & RedimExact(size_t newCount) {
		this->RedimCore<false, false, false, true>(newCount);
		return *this;
	}

	aVect & Reserve(size_t newCount) {
		this->RedimCore<false, true, false, false, false>(newCount);
		return *this;
	}

	aVect & _RedimUnitialized(size_t newCount) {
		this->RawRedim(newCount);
		return *this;
	}

	aVect & Shrink() {
		this->RawRedim<false, false, true>(this->count);
		return *this;
	}

	aVect & Grow(ptrdiff_t nGrow_input) {

		size_t newCount;

		if (nGrow_input > 0) {
			SIZE_T_OVERFLOW_CHECK_ADD(this->count, (size_t)nGrow_input);
			newCount = this->count + (size_t)nGrow_input;
		} else {
			if (this->count < (size_t)(-nGrow_input)) MY_ERROR("negative size");
			newCount = this->count - (size_t)(-nGrow_input);
		}

		if (IsTrivialType<T>::value && newCount <= this->allocatedCount) {
			this->count = newCount;
			return *this;
		}

		this->RedimCore(newCount);
		return *this;
	}

	size_t AllocatedCount() const {
		static_assert(owner, "no allocated ressource if owner = false");
		return this->allocatedCount;
	}

	size_t SizeOf() const {
		return this->count * sizeof(T);
	}

	size_t Count() const {
		return this->count;
	}

	size_t OutOfBounds(size_t i) const {
		return i >= this->count;
	}

	aVect & Set(T & val) {
		aVect_for_inv(*this, i) this->pData[i] = val;
		return *this;
	}

	aVect & Set(T && val) {
		return this->Set(val);
	}

	T * begin() const {
		return this->pData;
	}

	T * end() const {
		return &this->pData[this->count];
	}

	T & First() {
#if defined(_DEBUG) || defined(MY_DEBUG) 
		if (!this->pData || !this->count) MY_ERROR("endBegin sur vecteur vide!");
#endif
		return this->pData[0];
	}

	T & First() const {
#if defined(_DEBUG) || defined(MY_DEBUG) 
		if (!this->pData || !this->count) MY_ERROR("endBegin sur vecteur vide!");
#endif
		return this->pData[0];
	}

	T & Last() {
#if defined(_DEBUG) || defined(MY_DEBUG) 
		if (!this->pData || !this->count) MY_ERROR("Last() sur vecteur vide!");
#endif
		return this->pData[this->count - 1];
	}

	T & Last(int dec) {
#if defined(_DEBUG) || defined(MY_DEBUG) 
		if (!this->count || !this->count) MY_ERROR("Last() sur vecteur vide!");
		if (dec > 0 || -dec > (int)this->count - 1) MY_ERROR("Last() : indice en dehors des limites !");
#endif
		return this->pData[this->count - 1 + dec];
	}

	T & Last() const {
#if defined(_DEBUG) || defined(MY_DEBUG) 
		if (!this->pData || !this->count) MY_ERROR("Last() sur vecteur vide!");
#endif
		return this->pData[this->count - 1];
	}

	T & Last(int dec) const {
#if defined(_DEBUG) || defined(MY_DEBUG) 
		if (!this->count) MY_ERROR("Last() sur vecteur vide!");
		if (dec > 0 || -dec > (int)this->count - 1) MY_ERROR("Last() : indice en dehors des limites !");
#endif
		return this->pData[this->count - 1 + dec];
	}

#ifdef _XVECT_BACKWARD_COMPATIBILTY_
	aVect & Push_ByRef(T & el) {
		return this->Push(el);
	}

	aVect & Push_ByRef(T && el) {
		return this->Push(std::move(el));
	}

	aVect & Push_ByVal(T el) {
		return this->Push(el);
	}
#endif

	template <class... Args>
	aVect & Push(Args&&... args) {
		if (this->count != this->allocatedCount)
			return this->PushCoreNoRealloc(std::forward<Args>(args)...);
		else
			return this->PushCoreRealloc(std::forward<Args>(args)...);
	}

	void Pop(T & el) {

#if defined(_DEBUG) || defined(MY_DEBUG) 
		if (!this->count) MY_ERROR("Vecteur vide");
#endif

		size_t i = this->count - 1;

		el.~T();
		new(&el, NoNullCheck()) T(std::move(this->pData[i]));

		if (IsTrivialType<T>::value || IsAvect<T>::value || IsRawMovable<T>::value) {
			this->RawRedim<false, false>(i);
		}
		else {
			this->RedimCore<false, false, true, false, false>(i);
		}
	}

	T Pop() {

#if defined(_DEBUG) || defined(MY_DEBUG) 
		if (!this->Count()) MY_ERROR("Vecteur vide");
#endif

		size_t i = this->count - 1;

		T el(std::move(this->pData[i]));

		if (IsTrivialType<T>::value || IsAvect<T>::value || IsRawMovable<T>::value) {
			this->RawRedim<false, false>(i);
		}
		else {
			this->RedimCore<false, false, true, false, false>(i);
		}

		return std::move(el);
	}

	void Pop(size_t i, T & el) {

#if defined(_DEBUG) || defined(MY_DEBUG) 
		if (!this->count) MY_ERROR("Empty vector");
		if (i >= this->count) MY_ERROR("Index out of bounds");
#endif

		el.~T();
		new(&el, NoNullCheck()) T (std::move(this->pData[i]));

		
		this->RemoveCore<!moveGuaranty>(i);

	}

	T Pop(size_t i) {

#if defined(_DEBUG) || defined(MY_DEBUG) 
		if (!this->count) MY_ERROR("Empty vector");
		if (i >= this->count) MY_ERROR("Index out of bounds");
#endif

		T el (std::move(this->pData[i]));

		this->RemoveCore<!moveGuaranty>(i);

		return std::move(el);
	}

	template <bool destroy = true, bool changeCount = true>
	aVect & RemoveCore(size_t i) {

#if defined(_DEBUG) || defined(MY_DEBUG) 
		if (i >= this->count) MY_ERROR("index out of bounds");
#endif

		static_assert(owner, "no allocated ressource if owner = false");

		if (destroy) this->pData[i].~T();

		if (IsTrivialType<T>::value || IsAvect<T>::value || IsRawMovable<T>::value) {
			memmove(this->pData + i, this->pData + i + 1, sizeof(T) * (this->count - i - 1));
		}
		else {
			for (size_t j = i; j < this->count - 1; ++j) {
				new(this->pData + j, NoNullCheck()) T(std::move(this->pData[j + 1]));
				if (!moveGuaranty) this->pData[j + 1].~T();
			}
		}

		if (changeCount) this->RawRedim<true, false>(-1);

		return *this;
	}
	
	aVect & RemoveElement(T * el) {
		size_t i = el - this->pData;
		return this->RemoveCore<true>(i);
	}

	aVect & RemoveElement(T & el) {
		size_t i = &el - this->pData;
		return this->RemoveCore<true>(i);
	}

	aVect & Remove(size_t i) {
		return this->RemoveCore<true>(i);
	}
	
	template <class U>
	T * Find(U & el) {
		aVect_static_for(*this, i) {
			if (this->pData[i] == el) {
				return &this->pData[i];
			}
		}
		return nullptr;
	}

	template <class U>
	T * Find(U & el, size_t initCount) {
		auto I = this->count;
		for (size_t i=initCount; i<I; ++i) {
			if (this->pData[i] == el) {
				return &this->pData[i];
			}
		}
		return nullptr;
	}

	template <bool errorIfNotFound = true>
	aVect & FindAndRemove(const T & el) {
		aVect_static_for(*this, i) {
			if (this->pData[i] == el) {
				this->Remove(i);
				return *this;
			}
		}
		
		if (errorIfNotFound) MY_ERROR("Element not found");
		return *this;
	}
	
	template <class... Args>
	aVect & Insert(size_t i, Args&&... args) {
		if (this->count != this->allocatedCount)
			return this->InsertCore<false, true>(i, std::forward<Args>(args)...);
		else
			return this->InsertCore<true, true>(i, std::forward<Args>(args)...);
	}

	template <class... Args>
	aVect & Append(const char * fStr, Args&&... args) {

		static_assert(std::is_same<T, char>::value, "only for aVect<char>");

#ifdef XVECT_NO_STATIC_BUFFER
		aVect<char> g_aVect_char_sprintf_buffer;
#else
		static WinCriticalSection cs;
		Critical_Section(cs) {
			static aVect<char> g_aVect_char_sprintf_buffer;
#endif
			g_aVect_char_sprintf_buffer.sprintf(fStr, std::forward<Args>(args)...);

			this->Append(g_aVect_char_sprintf_buffer);
#ifndef XVECT_NO_STATIC_BUFFER
		}
#endif

		return *this;
	}

	template <class... Args>
	aVect & Append(const wchar_t * fStr, Args&&... args) {

		static_assert(std::is_same<T, wchar_t>::value, "only for aVect<wchar_t>");

#ifdef XVECT_NO_STATIC_BUFFER
		aVect<wchar_t> g_aVect_wchar_sprintf_buffer;
#else
		static WinCriticalSection cs;
		Critical_Section(cs) {
			static aVect<wchar_t> g_aVect_wchar_sprintf_buffer;
#endif

			g_aVect_wchar_sprintf_buffer.sprintf(fStr, std::forward<Args>(args)...);

			this->Append(g_aVect_wchar_sprintf_buffer);

#ifndef XVECT_NO_STATIC_BUFFER
		}
#endif

		return *this;
	}

	template <class... Args>
	aVect & AppendFormat(const char * fStr, Args&&... args) {

#ifdef XVECT_NO_STATIC_BUFFER
		aVect<char> g_aVect_char_sprintf_buffer;
#else
		static WinCriticalSection cs;
		Critical_Section(cs) {
			static aVect<char> g_aVect_char_sprintf_buffer;
#endif
			g_aVect_char_sprintf_buffer.Format(fStr, std::forward<Args>(args)...);

			this->Append(g_aVect_char_sprintf_buffer);

#ifndef XVECT_NO_STATIC_BUFFER
		}
#endif

		return *this;
	}

	template <class... Args>
	aVect & AppendFormat(const wchar_t * fStr, Args&&... args) {

#ifdef XVECT_NO_STATIC_BUFFER
		aVect<wchar_t> g_aVect_wchar_sprintf_buffer;
#else
		static WinCriticalSection cs;
		Critical_Section(cs) {
			static aVect<wchar_t> g_aVect_wchar_sprintf_buffer;
#endif
			g_aVect_wchar_sprintf_buffer.Format(fStr, std::forward<Args>(args)...);

			this->Append(g_aVect_wchar_sprintf_buffer);
#ifndef XVECT_NO_STATIC_BUFFER
		}
#endif

		return *this;
	}

	template <class... Args>
	aVect & Append(const char * str) {

		static_assert(std::is_same<T, char>::value, "only for aVect<char>");

#ifdef XVECT_NO_STATIC_BUFFER
		aVect<char> g_aVect_char_sprintf_buffer;
#else
		static WinCriticalSection cs;
		Critical_Section(cs) {
			static aVect<char> g_aVect_char_sprintf_buffer;
#endif

			g_aVect_char_sprintf_buffer = str;

			this->Append(g_aVect_char_sprintf_buffer);
#ifndef XVECT_NO_STATIC_BUFFER
		}
#endif

		return *this;
	}

	template <class... Args>
	aVect & Append(const wchar_t * str) {

		static_assert(std::is_same<T, wchar_t>::value, "only for aVect<wchar_t>");

#ifdef XVECT_NO_STATIC_BUFFER
		aVect<wchar_t> g_aVect_wchar_sprintf_buffer;
#else
		static WinCriticalSection cs;
		Critical_Section(cs) {
			static aVect<wchar_t> g_aVect_wchar_sprintf_buffer;
#endif

			g_aVect_wchar_sprintf_buffer = str;

			this->Append(g_aVect_wchar_sprintf_buffer);
#ifndef XVECT_NO_STATIC_BUFFER
		}
#endif

		return *this;
	}

	aVect & Append(aVect && that) {
		return this->Append(that);
	}

	aVect & Append(aVect & that) {

		static_assert(owner, "no allocated ressource if owner = false");

		size_t nThat = that.count;
		size_t nThis = this->count;

		if (nThat) {

			if (std::is_same<T, char>::value || std::is_same<T, wchar_t>::value) {
				if (nThis) {

					//auto count = Strlen<false>(this->pData) + 1;

					//if (count != nThis) {
					//	nThis = count;
					//	this->RawRedim(nThis);
					//}

					//this countains a terminal null
					if (this->pData[nThis-1] == 0) --nThis;
					this->RawRedim<true>(nThat - 1);
				}
				else this->RawRedim<true>(nThat);

				memcpy(this->pData + nThis, that.pData, nThat * sizeof(T));
			}
			else {

				this->RawRedim<true>(nThat);

				if (IsTrivialType<T>::value) {
					memcpy(this->pData + nThis, that.pData, nThat * sizeof(T));
				}
				else {
					T * const restrict target = this->pData + nThis;
					const T * const restrict source = that.pData;

					for (size_t i = 0; i < nThat; ++i) {
						new (&target[i], NoNullCheck()) T(source[i]);
					}
				}
			}
		}

		return *this;
	}

	template <class... Args>
	aVect & Prepend(const char * fStr, Args&&... args) {

		static_assert(std::is_same<T, char>::value, "only for aVect<char>");

#ifdef XVECT_NO_STATIC_BUFFER
		aVect<char> g_aVect_char_sprintf_buffer;
#else
		static WinCriticalSection cs;
		Critical_Section(cs) {
			static aVect<char> g_aVect_char_sprintf_buffer;
#endif

			g_aVect_char_sprintf_buffer.sprintf(fStr, std::forward<Args>(args)...);

			this->Prepend(g_aVect_char_sprintf_buffer);
#ifndef XVECT_NO_STATIC_BUFFER
		}
#endif

		return *this;
	}

	template <class... Args>
	aVect & Prepend(const wchar_t * fStr, Args&&... args) {

		static_assert(std::is_same<T, wchar_t>::value, "only for aVect<wchar_t>");

#ifdef XVECT_NO_STATIC_BUFFER
		aVect<wchar_t> g_aVect_wchar_sprintf_buffer;
#else
		static WinCriticalSection cs;
		Critical_Section(cs) {
			static aVect<wchar_t> g_aVect_wchar_sprintf_buffer;
#endif

			g_aVect_wchar_sprintf_buffer.sprintf(fStr, std::forward<Args>(args)...);

			this->Prepend(g_aVect_wchar_sprintf_buffer);
#ifndef XVECT_NO_STATIC_BUFFER
		}
#endif
		return *this;
	}

	template <class... Args>
	aVect & PrependFormat(const char * fStr, Args&&... args) {

#ifdef XVECT_NO_STATIC_BUFFER
		aVect<char> g_aVect_char_sprintf_buffer;
#else
		static WinCriticalSection cs;
		Critical_Section(cs) {
			static aVect<char> g_aVect_char_sprintf_buffer;
#endif

			g_aVect_char_sprintf_buffer.Format(fStr, std::forward<Args>(args)...);

			this->Prepend(g_aVect_char_sprintf_buffer);

#ifndef XVECT_NO_STATIC_BUFFER
		}
#endif

		return *this;
	}

	template <class... Args>
	aVect & PrependFormat(const wchar_t * fStr, Args&&... args) {

#ifdef XVECT_NO_STATIC_BUFFER
		aVect<wchar_t> g_aVect_wchar_sprintf_buffer;
#else
		static WinCriticalSection cs;
		Critical_Section(cs) {
#endif
			static aVect<wchar_t> g_aVect_wchar_sprintf_buffer;

			g_aVect_wchar_sprintf_buffer.Format(fStr, std::forward<Args>(args)...);

			this->Prepend(g_aVect_wchar_sprintf_buffer);
#ifndef XVECT_NO_STATIC_BUFFER
		}
#endif

		return *this;
	}

	aVect & Prepend(aVect && that) {
		return this->Prepend(that);
	}

	aVect & Prepend(aVect & that) {

		size_t nThat = that.count;
		size_t nThis = this->count;

		if (!nThis) {
			return this->Copy(that);
		}

		if (nThat) {

			if (std::is_same<T, char>::value || std::is_same<T, wchar_t>::value) {

				//that countains a terminal null
				--nThat;

				if (nThis) this->RawRedim<true>(nThis + nThat);

				memmove(this->pData + nThat, this->pData, nThis * sizeof(T));
				memcpy(this->pData, that.pData, nThat * sizeof(T));
			}
			else {

				this->RawRedim<true>(nThat);

				if (IsTrivialType<T>::value) {
					memmove(this->pData + nThat, this->pData, nThis * sizeof(T));
					memcpy(this->pData, that.pData, nThat * sizeof(T));
				}
				else {

					T * const restrict target = this->pData;
					const T * const restrict source = that.pData;

					for (size_t i = this->count - 1; i < this->count; --i) {
						new (&target[nThat + i], NoNullCheck()) T(target[i]);
					}
					for (size_t i = 0; i < nThat; ++i) {
						new (&target[i], NoNullCheck()) T(source[i]);
					}
				}
			}
		}

		return *this;
	}
	
	//template <char charToTrim>
	//aVect & Trim() {
	//	return this->TrimCore<char, 1, charToTrim>();
	//}

	template <wchar_t charToTrim>
	aVect & Trim() {
		return this->TrimCore<wchar_t, 1, charToTrim, 0>();
	}

	template <wchar_t charToTrim_1, wchar_t charToTrim_2>
	aVect & Trim() {
		return this->TrimCore<wchar_t, 2, charToTrim_1, charToTrim_2>();
	}

	//template <char charToTrim>
	//aVect & Trim() {
	//	return this->TrimCore<char, 1, charToTrim, 0>();
	//}

	//template <char charToTrim_1, char charToTrim_2>
	//aVect & Trim() {
	//	return this->TrimCore<char, 2, charToTrim_1, charToTrim_2>();
	//}

	//template <class U, U charToTrim>
	//aVect & Trim() {
	//	return this->TrimCore<U, 1, charToTrim, 0>();
	//}

	aVect & Trim() {
		if (std::is_same<T, char>::value) {
			return this->TrimCore<T, 2, ' ', '\t'>();
		} else {
			return this->TrimCore<T, 2, L' ', L'\t'>();
		}
	}

	template <class U, int nCharToTrim, U charToTrim_1, U charToTrim_2>
	aVect & TrimCore() {

		static_assert(std::is_same<T, char>::value || std::is_same<T, wchar_t>::value, "only for aVect<char> or aVect<wchar_t>");

		T * t = this->pData;
		if (!t) return *this;

		for (size_t i = Strlen(t) - 1; i >= 0; --i) {
			if (nCharToTrim == 1 && t[i] == charToTrim_1) {
				*((char *)&t[i]) = 0;
			} else if (nCharToTrim == 2 && (t[i] == charToTrim_1 || t[i] == charToTrim_2)) {
				*((char *)&t[i]) = 0;
			} else {
				break;
			}
		}

		size_t i = 0;
		if (nCharToTrim == 1) while (t[i] == charToTrim_1) i++;
		else if (nCharToTrim == 2) while (t[i] == charToTrim_1 || t[i] == charToTrim_2) i++;

		if (i) {
			size_t J = Strlen(t);
			aVect_static_for(*this, j) {
				if (i + j > J) break;
				t[j] = t[j + i];
			}
		}

		if (!*t) this->Erase();

		return *this;
	}

	aVect & Free() {

		if (isMvect) mThis()->mVect_Free<isMvect>();
		else this->Free_xVect();

		return *this;
	}

	aVect & Erase() {

		if (IsTrivialType<T>::value) {
			this->count = 0;
			return *this;
		}

		return this->Redim<false>(0);
	}

	T * GetDataPtr() const {
		return this->pData;
	}

	aVect & Wrap(T * ) {
		MY_ERROR("deprecated");
		return *this;
	}

	aVect & Leak() {
		if (isMvect) mThis()->mVect_Release<isMvect, true>();
		else {
			this->pData = nullptr;
			this->count = 0;
			this->allocatedCount = 0;
		}
		return *this;
	}


	template <class U> inline T & operator[] (U i) const {
		
#if defined(_DEBUG) || defined(MY_DEBUG) 
		//if (!this->pData) MY_ERROR("empty vector");
		if ((size_t)i >= this->count) MY_ERROR("index out of bounds");
#endif

		return this->pData[i];
	}

	explicit operator bool() {
		return (bool)(this->count != 0);
	}
	
	template <bool dbg_errorIfOutside = true>
	size_t Index(T & el) const {
		size_t ind = &el - this->pData;
#if defined(_DEBUG) || defined(MY_DEBUG) 
		if (dbg_errorIfOutside && ind >= this->count) MY_ERROR("element is not in array");
#endif
		return ind;
	}

	template <bool dbg_errorIfOutside = true>
	size_t Index(T * ptr) const {
		return this->Index<dbg_errorIfOutside>(*ptr);
	}

	~aVect() {
		if (isMvect) mThis()->mVect_Release<isMvect, false>();
		else if (owner) this->Free_xVect();
	}
};

template <class T> struct mRef {
	mVect<T> * ref;
	bool weak;
	
	mRef(mVect<T> * ref, bool weak) : ref(ref), weak(weak) {}
};

template <
	class T,
	bool owner,
	AllocFunc allocFunc,
	ReallocFunc reallocFunc,
	FreeFunc freeFunc,
	void* param>

class mVect {

	T * pData;
	size_t count;
	size_t allocatedCount;

	aVect<mRef<T> > ref;

	template<class,	bool, bool, AllocFunc, ReallocFunc, FreeFunc, void*> friend class aVect;
	template<class, bool, AllocFunc, ReallocFunc, FreeFunc, void*> friend class mVect;

	typedef aVect<T, owner, true, allocFunc, reallocFunc, freeFunc, param> aVectEq;
	enum { isMvect = 1 };
	//enum { isTrivialType = std::is_trivial<T>::value || std::is_base_of<TrivialType, T>::value };

	aVectEq * aThis() {
		return (aVectEq*)this;
	}

	aVectEq * aThisConst() const {
		return (aVectEq*)this;
	}

	template <bool instantiate>
	void mVect_Check(
		//#if defined(_DEBUG) || defined(MY_DEBUG) 
		bool subCheck = true,
		bool checkRefNumber = true,
		bool checkSelfRef = true
		//#endif
		)
	{
		MY_ERROR("not supposed to be run");
	}

	template <>
	void mVect_Check<true>(
		bool subCheck,
		bool checkRefNumber,
		bool checkSelfRef) 
	{
#if defined(_DEBUG) || defined(MY_DEBUG) 

		size_t nRef = this->ref.count;

		if (checkRefNumber && nRef == 1) MY_ERROR("nRef should be 0 or > 1");

		if (nRef) {
			size_t foundMySelf = 0;

			for (size_t i = 0; i < nRef; ++i) {
				if (this->ref.pData[i].ref->pData != this->pData) MY_ERROR("Ref doesn't point to data");
				if (this->ref.pData[i].ref->count != this->count) MY_ERROR("Ref count not equal to count");
				if (this->ref.pData[i].ref->allocatedCount != this->allocatedCount) MY_ERROR("Ref allocatedCount not equal to allocatedCount");
				if (this->ref.pData[i].ref->ref.pData != this->ref.pData) MY_ERROR("Ref RefArray doesn't point to RefArray");
				if (this->ref.pData[i].ref->ref.count != this->ref.count) MY_ERROR("Ref RefArray count not equal to RefArray count");
				if (this->ref.pData[i].ref->ref.allocatedCount != this->ref.allocatedCount) MY_ERROR("Ref RefArray allocatedCount not equal to RefArray allocatedCount");
				if (this->ref.pData[i].ref == this) foundMySelf++;
				else if (subCheck) this->ref.pData[i].ref->mVect_Check<isMvect>(false, checkRefNumber, checkSelfRef);
			}
			if (checkSelfRef && foundMySelf != 1) MY_ERROR("number of ref pointing to me not = 1");
		}
#endif
	}

	template <bool instantiate, bool update_pData, bool update_count, bool update_allocatedCount, bool checkRefNumber, bool checkSelfRef>
	struct mVect_UpdateRefRef_Functor {
		template <class U>
		static void mVect_UpdateRefRef(U thisPtr) {
			MY_ERROR("Not supposed to be run");
		}
	};

	template <bool update_pData, bool update_count, bool update_allocatedCount, bool checkRefNumber, bool checkSelfRef>
	struct mVect_UpdateRefRef_Functor < true, update_pData, update_count, update_allocatedCount, checkRefNumber, checkSelfRef > {
		template <class U>
		static void mVect_UpdateRefRef(U thisPtr) {

			if (!update_pData && !update_count && !update_allocatedCount) MY_ERROR("do nothing ?");

			bool onlyOne = (!update_pData && !update_count) || (!update_pData && !update_allocatedCount) || (!update_count && !update_allocatedCount);

			for (size_t i = 0; i < thisPtr->ref.count; ++i) {
				if (!onlyOne && thisPtr->ref.pData[i].ref == (void*)thisPtr) continue;
				if (update_pData) thisPtr->ref.pData[i].ref->ref.pData = thisPtr->ref.pData;
				if (update_count) thisPtr->ref.pData[i].ref->ref.count = thisPtr->ref.count;
				if (update_allocatedCount) thisPtr->ref.pData[i].ref->ref.allocatedCount = thisPtr->ref.allocatedCount;
			}

			thisPtr->mVect_Check<isMvect>(true, checkRefNumber, checkSelfRef);
		}
	};

	template <
		bool instantiate, 
		bool update_pData, 
		bool update_count, 
		bool update_allocatedCount, 
		bool checkRefNumber = true, 
		bool checkSelfRef = true>
	void mVect_UpdateRefRef() {
		mVect_UpdateRefRef_Functor<instantiate, update_pData, update_count, 
			update_allocatedCount, checkRefNumber, checkSelfRef>::mVect_UpdateRefRef(this);
	}
	 
	template <bool instantiate, bool update_pData, bool update_count, bool update_allocatedCount>
	struct mVect_UpdateRef_Functor {
		template <class U>
		static void mVect_UpdateRef(U thisPtr) {
			MY_ERROR("Not supposed to be run");
		}
	};

	template <bool update_pData, bool update_count, bool update_allocatedCount>
	struct mVect_UpdateRef_Functor < true, update_pData, update_count, update_allocatedCount> {
		template <class U>
		static void mVect_UpdateRef(U thisPtr) {

			if (!update_pData && !update_count && !update_allocatedCount) MY_ERROR("do nothing ?");

			bool onlyOne = (!update_pData && !update_count) || (!update_pData && !update_allocatedCount) || (!update_count && !update_allocatedCount);

			for (size_t i = 0; i < thisPtr->ref.count; ++i) {
				if (!onlyOne && thisPtr->ref.pData[i].ref == (void*)thisPtr) continue;
				if (update_pData) thisPtr->ref.pData[i].ref->pData = thisPtr->pData;
				if (update_count) thisPtr->ref.pData[i].ref->count = thisPtr->count;
				if (update_allocatedCount) thisPtr->ref.pData[i].ref->allocatedCount = thisPtr->allocatedCount;
			}

			thisPtr->mVect_Check<isMvect>();
		}
	};

	template <
		bool instantiate,
		bool update_pData,
		bool update_count,
		bool update_allocatedCount>
	void mVect_UpdateRef() {
		mVect_UpdateRef_Functor<instantiate, update_pData, update_count, update_allocatedCount>::mVect_UpdateRef(this);
	}

	template <bool instantiate>
	void mVect_ReferenceCore(mVect & m, bool weak) {
		MY_ERROR("not supposed to be run");
	}

	template <>
	void mVect_ReferenceCore<true>(mVect & m, bool weak) {

		static_assert(isMvect, "only for mVect type");

		this->mVect_Check<isMvect>();
		m.mVect_Check<m.isMvect>();

		if (&m == this) MY_ERROR("Self-reference");

		this->mVect_Release<isMvect, false>();

		this->mVect_Check<isMvect>();
		m.mVect_Check<m.isMvect>();

		this->pData = m.pData;
		this->count = m.count;
		this->allocatedCount = m.allocatedCount;

		if (m.ref.count == 0) m.ref.Push((mVect<T>*)&m, false);

		m.ref.Push((mVect<T>*)this, weak);
		m.mVect_UpdateRefRef<isMvect, true, true, true>();

		this->mVect_Check<isMvect>();
		m.mVect_Check<m.isMvect>();
	}

	template <bool instantiate>
	void mVect_Free() {
		MY_ERROR("not supposed to be run");
	}

	template <>
	void mVect_Free<true>() {

		if (this->ref.count == 0) {
			this->ref.Free();
		}
		else {
			for (size_t i = 0; i < this->ref.count; ++i) {
				this->ref.pData[i].ref->pData = nullptr;
				this->ref.pData[i].ref->count = 0;
				this->ref.pData[i].ref->allocatedCount = 0;
			}
		}

		aThis()->Free_xVect();
	}

	template <bool instantiate, bool leak> struct mVect_Release_Functor {
		template <class U>
		static void mVect_Release(U thisPtr) {
			MY_ERROR("Not supposed to be run");
		}
	};

	template <bool leak> struct mVect_Release_Functor<true, leak> {
		template <class U>
		static void mVect_Release(U thisPtr) {
		
			if (!thisPtr->ref.pData) {

				if (leak) {
					thisPtr->pData = nullptr;
					thisPtr->count = 0;
					thisPtr->allocatedCount = 0;
				}
				else thisPtr->aThis()->Free_xVect();
				return;
			}

			size_t nRef = thisPtr->ref.count;

			size_t nStrongRefs = 0;
			size_t nWeakRefs = 0;

			thisPtr->mVect_Check<isMvect>();

			for (size_t i = nRef - 1; i < nRef; --i) {
				if (thisPtr->ref.pData[i].ref == (void*)thisPtr) {
					thisPtr->ref.Remove(i);
					thisPtr->mVect_UpdateRefRef<isMvect, false, true, false, false, false>();
					nRef--;
				}
				else {
					if (thisPtr->ref.pData[i].weak) nWeakRefs++;
					else nStrongRefs++;
				}
			}

			if (nStrongRefs == 0) {
				for (size_t i = nRef; i < nRef; --i) {
					thisPtr->ref.pData[i].ref->pData = nullptr;
					thisPtr->ref.pData[i].ref->count = 0;
					thisPtr->ref.pData[i].ref->allocatedCount = 0;
					thisPtr->ref.pData[i].ref->ref.pData = nullptr;
					thisPtr->ref.pData[i].ref->ref.count = 0;
					thisPtr->ref.pData[i].ref->ref.allocatedCount = 0;
				}


				if (leak) {
					thisPtr->pData = nullptr;
					thisPtr->count = 0;
					thisPtr->allocatedCount = 0;
				}
				else {
					thisPtr->aThis()->Free_xVect();
				}
				thisPtr->ref.Free();
				return;
			}
			else if (nRef == 1) {
				//equivalent to Redim(0) but works regardless of whether ref is thisPtr
				thisPtr->ref.pData[0].ref->ref.count = 0;
				thisPtr->ref.count = 0;
			}

			thisPtr->pData = nullptr;
			thisPtr->count = 0;
			thisPtr->allocatedCount = 0;
			thisPtr->ref.pData = nullptr;
			thisPtr->ref.count = 0;
			thisPtr->ref.allocatedCount = 0;

			thisPtr->mVect_Check<isMvect>();
		}
	};

	template <bool instantiate, bool leak>
	void mVect_Release() {
		mVect_Release_Functor<instantiate, leak>::mVect_Release(this);
	}

	template <bool instantiate>
	void mVect_FixRef(mVect * oldThis) {
		MY_ERROR("Not supposed to be run");
	}

	template <>
	void mVect_FixRef<true>(mVect * oldThis) {

		for (size_t i = 0; i < this->ref.count; ++i) {
			if (this->ref.pData[i].ref == oldThis) {
				this->ref.pData[i].ref = this;
				break;
			}
		}
	}

	template <bool instantiate>
	void mVect_Move(mVect & that) {
		MY_ERROR("Not supposed to be run");
	}

	template <>
	void mVect_Move<true>(mVect & that) {

		that.mVect_Check<isMvect>();

		this->ref.pData = that.ref.pData;
		this->ref.count = that.ref.count;
		this->ref.allocatedCount = that.ref.allocatedCount;
		this->pData = that.pData;
		this->count = that.count;
		this->allocatedCount = that.allocatedCount; 
		that.ref.pData = nullptr;
		that.ref.count = 0;
		that.ref.allocatedCount = 0;
		that.pData = nullptr;
		that.count = 0;
		that.allocatedCount = 0;
		
		this->mVect_FixRef<isMvect>(&that);

		this->mVect_Check<isMvect>();
	}

public:

	mVect() {
		this->pData = nullptr;
		this->count = 0;
		this->allocatedCount = 0;
	}

	template <size_t n>
	mVect(T(&x)[n]) {
		aThis()->Vect_Create<true>(n);
		for (size_t i = 0; i < n; ++i) new(this->pData + i, NoNullCheck()) T(x[i]);
	}

	mVect(const mVect & that) {
		static_assert(owner, "no allocated ressource if owner = false");
		aThis()->CopyCore<true>(*(aVectEq*)&that);
	}

	mVect(mVect && that) : ref(CallDoNothingConstructor()) {
		static_assert(owner, "no allocated ressource if owner = false");
		aThis()->Move<true, false, false>(*(aVectEq*)&that);
	}

	mVect(CallDoNothingConstructor & that) {}

	mVect(CallDoNothingConstructor && that) {}

	mVect(size_t n) {
		static_assert(owner, "no allocated ressource if owner = false");
		aThis()->Vect_Create<true>(n);
		if (!IsTrivialType<T>::value) for (size_t i = 0; i < n; ++i) new(this->pData + i, NoNullCheck()) T;
	}

	mVect(const char * str) {

		static_assert(owner, "should use const char type if not owner");
		static_assert(std::is_same<T, char>::value, "only for mVect<char>");

		aThis()->Vect_Create<true>(strlen(str) + 1);
		memcpy(this->pData, str, this->count);
	}

	mVect(const wchar_t * str) {

		static_assert(owner, "should use const wchar_t type if not owner");
		static_assert(std::is_same<T, wchar_t>::value, "only for mVect<wchar_t>");

		aThis()->Vect_Create<true>(wcslen(str) + 1);
		memcpy(this->pData, str, this->count * sizeof(wchar_t));
	}

	template <class... Args>
	mVect(const char * fStr, Args&&... args) {

		static_assert(owner, "no allocated ressource if owner = false");
		static_assert(std::is_same<T, char>::value, "only for mVect<char>");

		PrintfStaticArgChecker(std::forward<Args>(args)...);

		int num = _scprintf(fStr, std::forward<Args>(args)...);
		aThis()->Vect_Create<true>(num + 1);
		::sprintf(this->pData, fStr, std::forward<Args>(args)...);
	}

	template <class... Args>
	mVect(const wchar_t * fStr, Args&&... args) {

		static_assert(owner, "no allocated ressource if owner = false");
		static_assert(std::is_same<T, wchar_t>::value, "only for mVect<wchar_t>");

		PrintfStaticArgChecker(std::forward<Args>(args)...);

		int num = _scwprintf(fStr, std::forward<Args>(args)...);
		aThis()->Vect_Create<true>(num + 1);
		::swprintf(this->pData, fStr, std::forward<Args>(args)...);
	}

	//mVect & operator=(mVect && that) {
	//	this->Steal(that);
	//	return *this;
	//}

	//mVect & operator=(const mVect & that) {
	//	this->Copy(that);
	//	return *this;
	//}

	template <
		bool thatOwner,
		bool isThatmVect,
		AllocFunc thatAllocFunc,
		ReallocFunc thatReallocFunc,
		FreeFunc thatFreeFunc,
		void * thatParam>
	mVect(aVect<T, thatOwner, isThatmVect, thatAllocFunc, thatReallocFunc, thatFreeFunc, thatParam> & that) {

		static_assert(owner, "no allocated ressource if owner = false");
		aThis()->CopyCore<true>(that);
	}

	template <
		bool thatOwner,
		bool isThatmVect,
		AllocFunc thatAllocFunc,
		ReallocFunc thatReallocFunc,
		FreeFunc thatFreeFunc,
		void * thatParam>
	mVect(aVect<T, thatOwner, isThatmVect, thatAllocFunc, thatReallocFunc, thatFreeFunc, thatParam> && that) {

		static_assert(owner, "no allocated ressource if owner = false");

		if (that.pData == nullptr && (that.isMvect && !that.mThis()->ref.pData)) {
			this->pData = nullptr;
			this->count = 0;
			this->allocatedCount = 0;
			return;
		}

		aThis()->Move<true, false, true>(that);
	}

	template <
		bool thatOwner,
		bool isThatmVect,
		AllocFunc thatAllocFunc,
		ReallocFunc thatReallocFunc,
		FreeFunc thatFreeFunc,
		void * thatParam>
	mVect & operator=(aVect<T, thatOwner, isThatmVect, thatAllocFunc, thatReallocFunc, thatFreeFunc, thatParam> & that) {

		aThis()->CopyCore<false>(that);
		return *this;
	}

	mVect & sprintf(const char * str) {
		aThis()->sprintf(str);
		return *this;
	}

	mVect & sprintf(const wchar_t * str) {
		aThis()->sprintf(str);
		return *this;
	}

	template <class... Args>
	mVect & sprintf(const char * fStr, Args&&... args) {
		aThis()->sprintf(fStr, std::forward<Args>(args)...);
		return *this;
	}

	template <class... Args>
	mVect & sprintf(const wchar_t * fStr, Args&&... args) {
		aThis()->sprintf(fStr, std::forward<Args>(args)...);
		return *this;
	}

	template <class... Args>
	mVect & Format(const char * fStr, Args&&... args) {
		aThis()->Format(fStr, std::forward<Args>(args)...);
		return *this;
	}

	template <class... Args>
	mVect & Format(const wchar_t * fStr, Args&&... args) {
		aThis()->Format(fStr, std::forward<Args>(args)...);
		return *this;
	}

	mVect & operator=(mVect && that) {
		aThis()->Steal(that);
		return *this;
	}

	template <class>
	mVect & operator=(mVect & that) = delete;

	template <
		bool thatOwner,
		AllocFunc thatAllocFunc,
		ReallocFunc thatReallocFunc,
		FreeFunc thatFreeFunc,
		void * thatParam>
	mVect & Copy(mVect<T, thatOwner, thatAllocFunc, thatReallocFunc, thatFreeFunc, thatParam> & that) {
		aThis()->Copy(*(aVect<T, thatOwner, true, allocFunc, reallocFunc, freeFunc, param>*) &that);
		return *this;
	}

	template <
		bool thatOwner,
		AllocFunc thatAllocFunc,
		ReallocFunc thatReallocFunc,
		FreeFunc thatFreeFunc,
		void * thatParam>
	mVect & Copy(mVect<T, thatOwner, thatAllocFunc, thatReallocFunc, thatFreeFunc, thatParam> && that) {
		return aThis()->Copy(*(aVect<T, thatOwner, true, allocFunc, reallocFunc, freeFunc, param>*) &that);
	}

	mVect & Copy(const char * str) {
		aThis()->Copy(str);
		return *this;
	}

	mVect & Copy(const wchar_t * str) {
		aThis()->Copy(str);
		return *this;
	}

	//commented out because it turned out to be not necessary 
	//maybe for a later use
	//check if not this "feature" is not already there
	mVect & Copy(const aVect<T, owner, false, allocFunc, reallocFunc, freeFunc, param> & that) {
		aThis()->Copy(that);
		return *this;
	}

	const aVect<T, owner, false, allocFunc, reallocFunc, freeFunc, param> & ToAvect() const {
		return *(const aVect<T, owner, false, allocFunc, reallocFunc, freeFunc, param>*)this;
	}

	template <
		bool isThatmVect,
		AllocFunc thatAllocFunc,
		ReallocFunc thatReallocFunc,
		FreeFunc thatFreeFunc,
		void * thatParam>
	mVect & Steal(aVect<T, owner, isThatmVect, thatAllocFunc, thatReallocFunc, thatFreeFunc, thatParam> & that) {
		aThis()->Steal(that);
		return *this;
	}

	mVect & Steal(mVect & that) {
		aThis()->Steal(that);
		return *this;
	}

	operator T*() const {
		return this->pData;
	}

	template <bool initialize = true>
	mVect & Redim(size_t newCount) {
		aThis()->Redim<initialize>(newCount);
		return *this;
	}

	mVect & RedimExact(size_t newCount) {
		aThis()->RedimExact(newCount);
		return *this;
	}

	mVect & Reserve(size_t newCount) {
		aThis()->Reserve(newCount);
		return *this;
	}

	mVect & Shrink() {
		aThis()->Shrink();
		return *this;
	}

	mVect & Grow(long long nGrow) {
		aThis()->Grow(size_t(nGrow));
		return *this;
	}

	size_t AllocatedCount() const {
		return aThis()->AllocatedCount();
	}

	size_t Count() const {
		return aThisConst()->Count();
	}

	mVect & Set(T & val) {
		aThis()->Set(val);
		return *this;
	}

	mVect & Set(T && val) {
		aThis()->Set(val);
		return *this;
	}

	size_t Index(T & el) const {
		return aThisConst()->Index(el);
	}

	T * begin() {
		return this->pData;
	}

	T * end() {
		return &this->pData[this->count];
	}

	T * begin() const {
		return this->pData;
	}

	T * end() const {
		return &this->pData[this->count];
	}

	T & Last() {
		return aThis()->Last();
	}

	T & Last(int dec) {
		return aThis()->Last(dec);
	}

	template <class... Args>
	mVect & Push(Args&&... args) {
		aThis()->Push(std::forward<Args>(args)...);
		return *this;
	}

	void Pop(T & el) {
		aThis()->Pop(t);
	}

	T Pop() {
		return aThis()->Pop();
	}

	void Pop(size_t i, T & el) {
		aThis()->Pop(i, el);
	}

	T Pop(size_t i) {
		return aThis()->Pop(i);
	}

	mVect & RemoveElement(T * el) {
		size_t i = el - this->pData;
		return this->RemoveCore<true>(i);
	}

	mVect & RemoveElement(T & el) {
		size_t i = &el - this->pData;
		return this->RemoveCore<true>(i);
	}

	mVect & Remove(size_t i) {
		aThis()->Remove(i);
		return *this;
	}

	mVect & FindAndRemove(T & el) {
		aThis()->FindAndRemove(el);
		return *this;
	}

	template <class... Args>
	mVect & Insert(size_t i, Args&&... args) {
		aThis()->Insert(i, std::forward<Args>(args)...);
		return *this;
	}

	template <class... Args>
	mVect & Append(const char * fStr, Args&&... args) {
		aThis()->Append(fStr, std::forward<Args>(args)...);
		return *this;
	}

	template <class... Args>
	mVect & Append(const wchar_t * fStr, Args&&... args) {
		aThis()->Append(fStr, std::forward<Args>(args)...);
		return *this;
	}

	template <class... Args>
	mVect & AppendFormat(const char * fStr, Args&&... args) {
		aThis()->AppendFormat(fStr, std::forward<Args>(args)...);
		return *this;
	}

	template <class... Args>
	mVect & AppendFormat(const wchar_t * fStr, Args&&... args) {
		aThis()->AppendFormat(fStr, std::forward<Args>(args)...);
		return *this;
	}

	mVect & Append(mVect && that) {
		aThis()->Append(that);
		return *this;
	}

	mVect & Append(mVect & that) {
		aThis()->Append(that);
		return *this;
	}

	template <class... Args>
	mVect & Prepend(const char * fStr, Args&&... args) {
		aThis()->Prepend(fStr, std::forward<Args>(args)...);
		return *this;
	}

	template <class... Args>
	mVect & Prepend(const wchar_t * fStr, Args&&... args) {
		aThis()->Prepend(fStr, std::forward<Args>(args)...);
		return *this;
	}

	template <class... Args>
	mVect & PrependFormat(const char * fStr, Args&&... args) {
		aThis()->PrependFormat(fStr, std::forward<Args>(args)...);
		return *this;
	}

	template <class... Args>
	mVect & PrependFormat(const wchar_t * fStr, Args&&... args) {
		aThis()->PrependFormat(fStr, std::forward<Args>(args)...);
		return *this;
	}

	mVect & Prepend(mVect && that) {
		aThis()->Prepend(that);
		return *this;
	}

	mVect & Prepend(mVect & that) {
		aThis()->Append(that);
		return *this;
	}

	mVect & Trim() {
		aThis()->Trim();
		return *this;
	}

	mVect & Reference(mVect & m, bool weak = false) {
		this->mVect_ReferenceCore<isMvect>(m, weak);
		return *this;
	}

	mVect & Free() {
		aThis()->Free();
		return *this;
	}

	T * GetDataPtr() const {
		return this->pData;
	}

	mVect & Wrap(T *) {
		MY_ERROR("deprecated");
		return *this;
	}

	mVect & Leak() {
		aThis()->Leak();
		return *this;
	}

	template <class U> inline T & operator[] (U i) const {
		return aThisConst()->operator[](i);
	}

	explicit operator bool() const {
		return aThisConst()->operator bool();
	}

	~mVect() {
		aThis()->~aVectEq();
	}
};


template<class T>
void RemoveExtraBlanks(T * t) {

	T * ptr2 = (T*)t;
	T * ptrEnd = ptr2 + Strlen(t) + 1;

	bool wasBlank = true;
	bool putBlank = false;

	for (char * ptr1 = ptr2; ptr1 < ptrEnd; ++ptr1) {
		if (*ptr1 != ' ' && *ptr1 != '\t')  {
			if (putBlank && *ptr1) {
				*ptr2 = ' ';
				ptr2++;
				putBlank = false;
			}
			*ptr2 = *ptr1;
			ptr2++;
			wasBlank = false;
		}
		else if (!wasBlank) {
			wasBlank = true;
			putBlank = true;
		}
	}
}

template<class T>
void RemoveBlanks(T * t) {

	T * ptr2 = (T*)t;
	T * ptrEnd = ptr2 + strlen(t) + 1;

	for (T * ptr1 = ptr2; ptr1 < ptrEnd; ++ptr1) {
		if (*ptr1 != ' ' && *ptr1 != '\t')  {
			*ptr2 = *ptr1;
			ptr2++;
		}
	}
}

template <class T> struct IsAvect {
	enum { value = 0 };
};

template <class T> struct IsMvect {
	enum { value = 0 };
};

template <
	class T, 
	bool owner, 
	AllocFunc allocFunc, 
	ReallocFunc reallocFunc, 
	FreeFunc freeFunc, 
	void * param> 
struct IsAvect<aVect<T, owner, false, allocFunc, reallocFunc, freeFunc, param> > {
	enum { value = 1 };
};

template <
	class T,
	bool owner,
	AllocFunc allocFunc,
	ReallocFunc reallocFunc,
	FreeFunc freeFunc,
	void * param>
struct IsAvect<const aVect<T, owner, false, allocFunc, reallocFunc, freeFunc, param> > {
	enum { value = 1 };
};

template <
	class T,
	bool owner,
	AllocFunc allocFunc,
	ReallocFunc reallocFunc,
	FreeFunc freeFunc,
	void * param>
struct IsMvect<aVect<T, owner, true, allocFunc, reallocFunc, freeFunc, param> > {
	enum { value = 1 };
};


template <
	class T,
	bool owner,
	AllocFunc allocFunc,
	ReallocFunc reallocFunc,
	FreeFunc freeFunc,
	void * param>
struct IsMvect<mVect<T, owner, allocFunc, reallocFunc, freeFunc, param> > {
	enum { value = 1 };
};

template <
	class T,
		bool owner,
		AllocFunc allocFunc,
		ReallocFunc reallocFunc,
		FreeFunc freeFunc,
		void * param>
struct IsMvect<const aVect<T, owner, true, allocFunc, reallocFunc, freeFunc, param> > {
	enum { value = 1 };
};


template <
	class T,
		bool owner,
		AllocFunc allocFunc,
		ReallocFunc reallocFunc,
		FreeFunc freeFunc,
		void * param>
struct IsMvect<const mVect<T, owner, allocFunc, reallocFunc, freeFunc, param> > {
	enum { value = 1 };
};

template <class T>
class CharPointerCore {

	T * pData;

public:

	static_assert(std::is_same<T, char>::value || std::is_same<T, const char>::value ||
		std::is_same<T, wchar_t>::value || std::is_same<T, const wchar_t>::value, "char or wchar_t type only");

	CharPointerCore() {
		this->pData = nullptr;
	}

	CharPointerCore(T * str) {
		this->pData = str;
	}

	//CharPointerCore(const T * str) {
	//	this->pData = str;
	//}

	operator T *& () {
		return this->pData;
	}

	CharPointerCore & Trim() {

		T * t = this->pData;
		if (!t) return *this;

		{
			size_t i = 0;
			while (t[i] == ' ' || t[i] == '\t') i++;

			if (i) {
				t = &t[i];
				this->pData = t;
			}
		}

		for (size_t i = ::Strlen(t) - 1; i >= 0; --i) {
			if (t[i] == ' ' || t[i] == '\t') t[i] = 0;
			else break;
		}

		if (!*t) this->pData = nullptr;

		return *this;
	}

	size_t Count() {
		if (this->pData) return this->Strlen() + 1;
		else return 0;
	}

	char * begin() {
		return this->pData;
	}

	char * end() {
		return this->pData + this->Count();
	}

	char & Last(int dec) {
#if defined(_DEBUG) || defined(MY_DEBUG) 
		if (!this->pData) MY_ERROR("Last() sur vecteur vide!");
		if (dec > 0 || -dec > (int)this->Strlen()) MY_ERROR("Last() : indice en dehors des limites !");
#endif
		return this->pData[this->Strlen() + dec];
	}

	explicit operator bool() {
		return this->pData != nullptr;
	}

	template <class U> T& operator[] (U i) {
		return this->pData[i];
	}

	size_t Strlen() {
		return ::Strlen(this->pData);
	}

	CharPointerCore & Free() {
		this->pData = nullptr;
		return *this;
	}

#ifdef _XVECT_BACKWARD_COMPATIBILITY_
	CharPointerCore & Wrap(T * ptr) {
		this->pData = ptr;
		return *this;
	}
#endif

	CharPointerCore & operator=(T * str) {
		this->pData = str;
		return *this;
	}


	CharPointerCore & RemoveExtraBlanks() {
		char * t = this->pData;
		if (!t) return *this;

		::RemoveExtraBlanks(t);

		return *this;
	}

	CharPointerCore & RemoveBlanks() {
		char * t = this->pData;
		if (!t) return *this;

		::RemoveBlanks(t);

		return *this;
	}
};


inline void PrintfStaticArgChecker() {}

template <class ToCheck, class... Args> void PrintfStaticArgChecker(ToCheck toCheck, Args&&... args) {
	static_assert(!IsAvect<ToCheck>::value && !IsMvect<ToCheck>::value, "requires static cast to char *");
	PrintfStaticArgChecker(std::forward<Args>(args)...);
}

template <class... Args>
int Printf(const char * fStr, Args&&... args) {

	PrintfStaticArgChecker(std::forward<Args>(args)...);

	return printf(fStr, std::forward<Args>(args)...);
}


using CharPointer = CharPointerCore<char>;
using ConstCharPointer = CharPointerCore<const char>;

template <class T> using aVectWrapper = aVect<T, false>;


template <class T>
class CachedVect : private aVect<T> {

	size_t shadowCount;

	template <class U, class... Args>
	void CallOperatorEqualIfArgs(U&& el, Args&&... args) {
		el.operator=(std::forward<Args>(args)...);
	}

	template <class U, class Arg>
	void CallOperatorEqualIfArgs(U * el, Arg&& arg) {
		el = arg;
	}

	template <class U>
	void CallOperatorEqualIfArgs(U&& el) {
		el.Erase();
	}

public:

	CachedVect() : aVect(), shadowCount(0) {}

	template <class... Args>
	CachedVect & Redim(size_t newCount, Args&&... args) {
		if (newCount > this->shadowCount) {

			for (size_t i = this->shadowCount, I = Min(this->count, newCount); i < I; ++i) {
				auto&& el = aVect::operator[](i);
				this->CallOperatorEqualIfArgs(el, std::forward<Args>(args)...);
				//this->operator=(std::forward<Args>(args)...);
			}

			if (newCount > this->count) {
				aVect::Redim(newCount, std::forward<Args>(args)...);
			}

			this->shadowCount = newCount;
			return *this;
		}
		else {
			this->shadowCount = newCount;
		}
		return *this;
	}

	template <class... Args>
	CachedVect & Push(Args&&... args) {
		this->Redim(this->shadowCount + 1, std::forward<Args>(args)...);
		return *this;
	}

	T * begin() const {
		return aVect::begin();
	}

	T * end() const {
		return &this->pData[this->shadowCount];
	}

	template <bool dbg_errorIfOutside = true>
	size_t Index(T & el) const {
		return aVect::Index<dbg_errorIfOutside>(el);
	}

	template <bool dbg_errorIfOutside = true>
	size_t Index(T * ptr) const {
		return aVect::Index<dbg_errorIfOutside>(ptr);
	}

	T & First() const {
		return aVect::First();
	}

	T & operator[](size_t i) {

#if defined(_DEBUG) || defined(MY_DEBUG) 
		if ((size_t)i >= this->shadowCount) MY_ERROR("index out of bounds");
#endif

		return this->pData[i];
	}

	T & operator[](int i) {

#if defined(_DEBUG) || defined(MY_DEBUG) 
		if ((size_t)i >= this->shadowCount) MY_ERROR("index out of bounds");
#endif

		return this->pData[i];
	}

	explicit operator bool() {
		return this->shadowCount != 0;
	}

	size_t Count() {
		return this->shadowCount;
	}

	CachedVect & Grow(long long nGrow) {
		this->Redim(size_t(this->shadowCount + nGrow));
		return *this;
	}

	CachedVect & Remove(size_t i) {

#if defined(_DEBUG) || defined(MY_DEBUG) 
		if (i >= this->shadowCount) MY_ERROR("index out of bounds");
#endif

		if (i == this->shadowCount - 1) {
			this->shadowCount--;
			return *this;
		}

		auto bkp = std::move(this->pData[i]);
		size_t origCount = this->count;
		this->count = this->shadowCount;

		aVect::RemoveCore<false, false>(i);

		this->count = origCount;

		new (&this->pData[this->shadowCount - 1], NoNullCheck()) T(std::move(bkp));

		this->shadowCount--;

		return *this;
	}

	CachedVect & Erase() {
		this->shadowCount = 0;
		return *this;
	}

	T & Last() {
#if defined(_DEBUG) || defined(MY_DEBUG) 
		if (!this->pData) MY_ERROR("Last() sur vecteur vide!");
#endif
		return this->pData[this->shadowCount - 1];
	}
};

template <class T> struct ReverseRangeIterator {

	T ptr;

	ReverseRangeIterator(T ptr) : ptr(ptr) {}

	T operator++(int) {//postFix
		T oldVal = ptr;
		ptr--;
		return oldVal;
	}

	T operator++() {//prefix
		ptr--;
		return ptr;
	}

	operator T() {
		return ptr;
	}
};

template <class T> struct ReverseRange {

	T & ref;

	typedef decltype(T().begin()) IteratorType;
	typedef ReverseRangeIterator<IteratorType> ReversRangeType;

	ReverseRange(T & ref) : ref(ref) {}

	ReversRangeType end() {
		return ReversRangeType(this->ref.begin() - 1);
	}

	ReversRangeType begin() {
		return ReversRangeType(this->ref.end() - 1);
	}
};

template <class T> ReverseRange<T> Reverse(T& container) {
	return ReverseRange<T>(container);
}



inline aVect<double> xRamp(double start, double step, double end) {

	aVect<double> retVal;

	for (double x = start; x < end; x += step) {
		retVal.Push(x);
	}

	if (retVal) {
		if (retVal.Last()) {
			auto a = retVal.Last();
			auto b = end;
			auto _a = std::abs(a);
			auto _b = std::abs(b);
			auto _c = _a < _b ? _a : _b;
			if (std::abs(a - b) / _c > 1e-10) retVal.Push(end);
		}
		else {
			if (abs(end - retVal.Last()) < 1e-10) retVal.Push(end);
		}
	}

	return std::move(retVal);
}

#endif



#ifdef _WIN32_BKP
#define _WIN32 _WIN32_BKP
#define _MSC_VER _MSC_VER_BKP
#undef _WIN32_BKP
#undef _MSC_VER_BKP
#endif

