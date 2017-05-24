
#ifdef _WIN32 
#define _CRT_SECURE_NO_WARNINGS
#include "WinUtils.h"
#endif

#include "Debug.h"
#include "stdio.h"
//#include "windows.h"

#ifdef SIMULATE_NO_WIN32
#define SIMULATE_NO_WIN32_ENABLED
#undef _WIN32
#endif

const char * errorFileName(const char * c) {

	if (!c) return nullptr;

	size_t len = strlen(c);
	for (size_t i=len; i <= len; --i) {
		if (i < len && c[i] == '\\') return &c[i+1];
	}
	return c;
}

const char * errorFilePath(const char * c) {

	if (!c) return nullptr;

	const size_t bufLen = 500;
	static char buffer[bufLen];

	for (size_t i=0; i<bufLen; ++i) {
		buffer[i] = c[i];
		if (!c[i]) break;
	}

	buffer[bufLen-1] = 0;

	size_t len = strlen(buffer);
	for (size_t i=len; i <= len; --i) {
		if (buffer[i] == '\\') {
			buffer[i] = 0;
			break;
		}
	}

	return buffer;
}

void Dbg_DoNothing(...) {}

#ifdef _DEBUG

	long g_Dbg_Options = DBG_OPT_NONE;

	void Dbg_SetOpt(long dbgOpt) {
		g_Dbg_Options |= dbgOpt;
	}

	void Dbg_UnsetOpt(long dbgOpt) {
		g_Dbg_Options &= ~dbgOpt;
	}

	void Dbg_Print(long dbgOpt, const char * fStr, ...) {

		//static int lvl = 0;
		int lvl = 0;
		if (!(dbgOpt & g_Dbg_Options)) return;

		if (dbgOpt & DBG_OPT_TRACEOUT) lvl--;

		va_list args;
		va_start(args, fStr);

	#ifdef _MSC_VER
		#define OUT_FUNC OutputDebugStringA
	#else
		#define OUT_FUNC printf
	#endif

	#define OUT_PREFIX(opt) if (dbgOpt & DBG_OPT_##opt) OUT_FUNC(DBG_STR_##opt)

	for (int i=0; i<lvl; ++i) OUT_FUNC("  ");
	OUT_PREFIX(TRACEIN);
	OUT_PREFIX(TRACEOUT);
	OUT_PREFIX(INFO);
	OUT_PREFIX(ALLOC);
	OUT_PREFIX(FREE);
	OUT_PREFIX(CTOR);
	OUT_PREFIX(CPTOR);
	OUT_PREFIX(DTOR);
	OUT_PREFIX(REF);
	OUT_PREFIX(MOVE);

	#undef OUT_FUNC
	#undef OUT_PREFIX

	#ifdef _MSC_VER
		long n = vsnprintf(NULL, 0, fStr, args);
		char buffer[1000];
		//static long last_n = 0;
		long last_n = 0;

		vsprintf(buffer, fStr, args);

		OutputDebugStringA(buffer);
	#else
		vfprintf(stdout, fStr, args);

	#endif // _MSC_VER

		va_end(args);

		if (dbgOpt & DBG_OPT_TRACEIN)  lvl++;
	}

#endif // _DEBUG

void(*g_MyErrorCore_ptr)(char *, char *, char *, size_t, void *, bool, long);

void SetErrorFunction(void(*funcPtr)(char *, char *, char *, size_t, void *, bool, long)) {
	g_MyErrorCore_ptr = funcPtr;
}

__declspec(noinline) void MY_ERROR_CORE(
	wchar_t * errorMsg,
	char * funcStr,
	char * fileStr,
	size_t line,
	void * exceptPtr,
	bool allowContinue,
	long errCode)
{
	MY_ERROR_CORE(aVect<char>(errorMsg), funcStr, fileStr, line, exceptPtr, allowContinue, errCode);
}

__declspec(noinline) void MY_ERROR_CORE(
	char * errorMsg, 
	char * funcStr, 
	char * fileStr, 
	size_t line, 
	void * exceptPtr, 
	bool allowContinue,
	long errCode)
{

#ifdef _WINUTILS_
	static WinCriticalSection cs;

	Critical_Section(cs) {
#endif

		if (g_MyErrorCore_ptr) {
			g_MyErrorCore_ptr(errorMsg, funcStr, fileStr, line, exceptPtr, allowContinue, errCode);
		}
		else {
			printf("\n### Error ###\n\n%s\n - Function: %s\n - File: %s\n - Path: %s\n - Line: %Id\n\n",
				errorMsg, funcStr, errorFileName(fileStr), errorFilePath(fileStr), line);

#ifdef _WINUTILS_
			StopAllOtherThreads();
#endif

			if (manualDbgBreak(errorMsg, funcStr, fileStr, line, exceptPtr, allowContinue, errCode)) {
				getchar();
				//exit(-1);
				TerminateProcess(GetCurrentProcess(), -1);
			}

			StopAllOtherThreads(true);
		}
#ifdef _WINUTILS_
	}
#endif
}

void MyErrorFunc_(
	char * errorMsg,
	char * funcStr,
	char * fileStr,
	size_t line,
	void * exceptPtr,
	bool allowContinue,
	long errCode)
{
	throw MyException(errorMsg, funcStr, fileStr, line, exceptPtr, allowContinue, errCode);
}

__declspec(noinline) 
void MY_ERROR_CORE(
	wchar_t * errorMsg,
	char * funcStr,
	char * fileStr,
	size_t line,
	void * exceptPtr,
	bool allowContinue)
{
	MY_ERROR_CORE(aVect<char>(errorMsg), funcStr, fileStr, line, exceptPtr, allowContinue, -1);
}

__declspec(noinline)
void MY_ERROR_CORE(
	char * errorMsg,
	char * funcStr,
	char * fileStr,
	size_t line,
	void * exceptPtr,
	bool allowContinue)
{
	MY_ERROR_CORE(errorMsg, funcStr, fileStr, line, exceptPtr, allowContinue, -1);
}

void MY_ASSERT_CORE(bool cond) {
	if (!cond) MY_ERROR("Assertion failed");
}

void EnableMyException() {
	SetErrorFunction(MyErrorFunc_);
};

#pragma optimize("", off)
bool manualDbgBreak(
	char * errorMsg, 
	char * funcStr, 
	char * fileStr, 
	size_t line, 
	void * exceptPtr, 
	bool allowContinue,
	long errCode)
{
	
	char msg[5000];

	size_t strLen = strlen(errorMsg);
	if (strLen > 5000-1) errorMsg[5000-2] = 0;

	sprintf(msg, "Error :\n\n%s\n%s", errorMsg, allowContinue ? "\nTerminate program (recommanded) ?" : "");
	int result = MessageBoxA(nullptr, msg, "DbgBreak", MB_ICONERROR | (allowContinue ? MB_YESNO : 0) | MB_SERVICE_NOTIFICATION);

#ifdef _WIN32 

	if (IsDebuggerPresent()) {
		__debugbreak();
	} else {
		CoreDump(errorMsg, exceptPtr, GetCurrentThreadId(), fileStr, line, funcStr);
	}

#endif

	if (!allowContinue || result == IDYES) {
		printf("_STOP_\n");
		return true;
	}
	else {
		printf("_Continue_\n");
		return false;
	}
}
#pragma optimize("", on)


