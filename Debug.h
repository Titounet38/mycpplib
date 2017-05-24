#include "stdio.h"
#include "string.h"
#include <exception>

#ifdef SIMULATE_NO_WIN32
#define _WIN32_BKP   _WIN32
#define _MSC_VER_BKP _MSC_VER
#undef _WIN32
#undef _MSC_VER
#endif

#ifndef _MY_DEBUG_HEADER_
#define _MY_DEBUG_HEADER_

const char * errorFileName(const char * c);
const char * errorFilePath(const char * c);

void SetErrorFunction(void(*funcPtr)(char *, char *, char *, size_t, void *, bool, long));

#define MY_ERROR(str)  \
    MY_ERROR_CORE(str, __FUNCTION__, __FILE__, __LINE__, nullptr, true, -1)

#define RECOVERABLE_ERROR(str, errorCode)  \
    MY_ERROR_CORE(str, __FUNCTION__, __FILE__, __LINE__, nullptr, true, errorCode)

#define UNRECOVERABLE_ERROR(str)  \
    MY_ERROR_CORE(str, __FUNCTION__, __FILE__, __LINE__, nullptr, false)

#define RECOVERABLE_ERROREX(str, exceptPtr, errorCode)  \
    MY_ERROR_CORE(str, __FUNCTION__, __FILE__, __LINE__, exceptPtr, true, errorCode)

#define UNRECOVERABLE_ERROREX(str, exceptPtr)  \
    MY_ERROR_CORE(str, __FUNCTION__, __FILE__, __LINE__, exceptPtr, false)

#ifdef _DEBUG
#define MY_ASSERT_DBG(a) MY_ASSERT_CORE(a != 0)
#else
#define MY_ASSERT_DBG(a)
#endif

#define MY_ASSERT(a) MY_ASSERT_CORE(a)

void MY_ASSERT_CORE(bool cond);
bool manualDbgBreak(char * errorMsg, char * funcStr, char * fileStr, size_t line, void * exceptPtr, bool allowContinue, long errCode);
//bool manualDbgBreak(char * errorMsg, char * funcStr, char * fileStr, size_t line, void * exceptPtr, bool allowContinue);

__declspec(noinline) void MY_ERROR_CORE(wchar_t * errorMsg, char * funcStr, char * fileStr, size_t line, void * exceptPtr, bool allowContinue);
__declspec(noinline) void MY_ERROR_CORE(wchar_t * errorMsg, char * funcStr, char * fileStr, size_t line, void * exceptPtr, bool allowContinue, long errCode);
__declspec(noinline) void MY_ERROR_CORE(char * errorMsg, char * funcStr, char * fileStr, size_t line, void * exceptPtr, bool allowContinue);
__declspec(noinline) void MY_ERROR_CORE(char * errorMsg, char * funcStr, char * fileStr, size_t line, void * exceptPtr, bool allowContinue, long errCode);


struct MyException : public std::exception {

	char errorMsg[1000];
	char errorDetailedMsg[5000];
	const char * funcStr;
	const char * fileStr;
	size_t line;
	void * exceptPtr;
	bool allowContinue;
	long errCode;

	MyException(
		const char * errorMsg = nullptr,
		const char * funcStr = nullptr,
		const char * fileStr = nullptr,
		size_t line = 0,
		void * exceptPtr = nullptr,
		bool allowContinue = false,
		long errCode = 0)
	{
		if (errorMsg) strcpy_s(this->errorMsg, errorMsg);

		else this->errorMsg[0] = 0;

		sprintf_s(this->errorDetailedMsg, "\n### Error ###\n\n%s\n - Function: %s\n - File: %s\n - Path: %s\n - Line: %Id\n\n",
			errorMsg, funcStr, errorFileName(fileStr), errorFilePath(fileStr), line);

		this->funcStr = funcStr;
		this->fileStr = fileStr;
		this->line = line;
		this->exceptPtr = exceptPtr;
		this->allowContinue = allowContinue;
		this->errCode = errCode;
	}

	MyException(
		const char * errorMsg,
		long errCode) :
		MyException(errorMsg, nullptr, nullptr, 0, nullptr, false, errCode)
	{}

	const char * what() const {
		return this->errorDetailedMsg;
	}
};

void EnableMyException();

#define DBG_OPT_TRACEIN  0x1
#define DBG_STR_TRACEIN  "[=>]"
#define DBG_OPT_TRACEOUT 0x2
#define DBG_STR_TRACEOUT "[<=]"
#define DBG_OPT_INFO     0x4
#define DBG_STR_INFO     "[INFO]"
#define DBG_OPT_ALLOC    0x8
#define DBG_STR_ALLOC    "[ALLOC]"
#define DBG_OPT_FREE     0x10
#define DBG_STR_FREE     "[FREE]"
#define DBG_OPT_CTOR     0x20
#define DBG_STR_CTOR     "[CTOR]"
#define DBG_OPT_CPTOR    0x40
#define DBG_STR_CPTOR    "[CPTOR]"
#define DBG_OPT_DTOR     0x80
#define DBG_STR_DTOR     "[DTOR]"
#define DBG_OPT_REF      0x100
#define DBG_STR_REF      "[REF]"
#define DBG_OPT_MOVE     0x200
#define DBG_STR_MOVE     "[MOVE]"

#define DBG_OPT_NONE     0x0
#define DBG_OPT_TRACE    (DBG_OPT_TRACEIN | DBG_OPT_TRACEOUT)
#define DBG_OPT_ALL      0xFFFFFFFF

#define DBG_TRACEIN(f)  DBG_PRINT(DBG_OPT_TRACEIN,  "%s\n", f)
#define DBG_TRACEOUT(f) DBG_PRINT(DBG_OPT_TRACEOUT, "%s\n", f)

void Dbg_DoNothing(...);

#ifdef _DEBUG

	#ifdef _MSC_VER
		#include "windows.h"
	#endif

	void Dbg_SetOpt(long dbgOpt);
	void Dbg_UnsetOpt(long dbgOpt);
	void Dbg_Print(long dbgOpt, const char * fStr, ...);

#endif // _DEBUG

#ifdef _DEBUG
#define DBG_PRINT		 Dbg_Print
#define DBG_SET_OPTION   Dbg_SetOpt
#define DBG_UNSET_OPTION Dbg_UnsetOpt
#else
#ifdef _MSC_VER
#define DBG_PRINT		 if (false) (void**)
#define DBG_SET_OPTION   if (false) (void**)
#define DBG_UNSET_OPTION if (false) (void**)
#else
#define DBG_PRINT		 Dbg_DoNothing
#define DBG_SET_OPTION   Dbg_DoNothing
#define DBG_UNSET_OPTION Dbg_DoNothing
#endif //_MSC_VER
#endif //_DEBUG

#endif 

#ifdef _WIN32_BKP
#define _WIN32 _WIN32_BKP
#define _MSC_VER _MSC_VER_BKP
#undef _WIN32_BKP
#undef _MSC_VER_BKP
#endif

