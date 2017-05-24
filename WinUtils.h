
#ifndef _WINUTILS_
#define _WINUTILS_

#include "xVect.h"
#include "WinCritSect.h"
#include "MyUtils.h"
#include "windows.h"
#include "windowsx.h"
#include "float.h"
#include <stdexcept>
#include "Shobjidl.h"
#include "Shlwapi.h"


template <class T, class U> T WinSafeCall(T retVal, U retErrorValue, char * errMsg, char * funcStr, char * fileStr, int lineStr, bool continueIfNoError = false) {

	if (retVal == (T)retErrorValue && !(continueIfNoError && GetLastError() == NO_ERROR)) {
		auto msg = xFormat(L"%S failed\n\n - Error code: %d\n - Description: \"%s\"", errMsg, GetLastError(), GetWin32ErrorDescription(GetLastError(), false));
		MY_ERROR_CORE(msg, funcStr, fileStr, lineStr, nullptr, true);
		return retVal;
	}
	else {
		return retVal;
	}
}

template <class T> T WinSafeCall(T retVal, char * errMsg, char * funcStr, char * fileStr, int lineStr, bool continueIfNoError = false) {

	if (!retVal && !(continueIfNoError && GetLastError() == NO_ERROR)) {
		auto msg = xFormat(L"%S failed\n\n - Error code: %d\n - Description: \"%s\"", errMsg, GetLastError(), GetWin32ErrorDescription(GetLastError(), false));
		MY_ERROR_CORE(msg, funcStr, fileStr, lineStr, nullptr, true);
		return retVal;
	}
	else {
		return retVal;
	}
}

#define EXPAND(X) X

#define GET_MACRO(_1,_2,NAME,...) NAME
#define SAFE_CALL_CORE(expr, ...) EXPAND(GET_MACRO(__VA_ARGS__, SAFE_CALL2, SAFE_CALL1)(expr, __VA_ARGS__))
#define SAFE_CALL_CORE_NO_ERROR_OK(expr, ...) EXPAND(GET_MACRO(__VA_ARGS__, SAFE_CALL2_NO_ERROR_OK, SAFE_CALL1_NO_ERROR_OK)(expr, __VA_ARGS__))

#define SAFE_CALL(expr, ...) SAFE_CALL_CORE(#expr, expr, __VA_ARGS__)
#define SAFE_CALL_NO_ERROR_OK(expr, ...) SAFE_CALL_CORE_NO_ERROR_OK(#expr, expr, __VA_ARGS__)

#define SAFE_CALL1(textExpr, expr) WinSafeCall(expr, #textExpr, __FUNCTION__, __FILE__, __LINE__)
#define SAFE_CALL2(textExpr, expr, retErrorValue) WinSafeCall(expr, retErrorValue, #textExpr, __FUNCTION__, __FILE__, __LINE__)

#define SAFE_CALL1_NO_ERROR_OK(textExpr, expr) WinSafeCall(expr, #textExpr, __FUNCTION__, __FILE__, __LINE__, true)
#define SAFE_CALL2_NO_ERROR_OK(textExpr, expr, retErrorValue) WinSafeCall(expr, retErrorValue, #textExpr, __FUNCTION__, __FILE__, __LINE__, true)

extern "C" {
	WINUSERAPI int WINAPI MessageBoxTimeoutA(IN HWND hWnd,IN LPCSTR lpText, IN LPCSTR lpCaption, 
        IN UINT uType, IN WORD wLanguageId, IN DWORD dwMilliseconds);
	WINUSERAPI int WINAPI MessageBoxTimeoutW(IN HWND hWnd,IN LPCWSTR lpText, IN LPCWSTR lpCaption, 
        IN UINT uType, IN WORD wLanguageId, IN DWORD dwMilliseconds);
}
 
#define ScopeVerboseLevel(a, b) if (auto&& __guard = a.SetScopeVerbose(b)) 

#ifdef UNICODE
#define MessageBoxTimeout MessageBoxTimeoutW
#else
#define MessageBoxTimeout MessageBoxTimeoutA
#endif 

#define ENUM_ALL	0x1
#define ENUM_CHILD	0x2
#define ENUM_LB		0x4

struct ArgGroup {
	mVect<char> stringToFindInWinTitle;
	mVect< mVect<char> > listBoxContents;
};

struct EnumFuncArg {
	mVect<char> winClassName;
	mVect<ArgGroup> argGroup;
	size_t currentArgGroup;
	int enumOption;
};


void SplitPath(const char * src, char ** pDrive, char ** pPath, char ** pFName, char ** pExt);
void SplitPathW(const wchar_t * src, wchar_t ** pDrive, wchar_t ** pPath, wchar_t ** pFName, wchar_t ** pExt);

void RemoveFiles(aVect< aVect<char> > & files2remove, char * introMsg = nullptr, LogFile & logFile = LogFile());

char WinGetChar();

aVect<char> WinGetChars(DWORD nCharMaxToRead);

void MyTerminateProcess(PROCESS_INFORMATION & pi, STARTUPINFO & si);

DWORD WinKbHit();

BOOL CALLBACK EnumFunc(HWND hWnd, LPARAM lParam);

mVect< mVect<wchar_t> > WalkForwardPathForFile(
	const aVect<wchar_t>& path,
	const aVect< aVect<wchar_t> >& fileNames,
	mVect< mVect<wchar_t> > * folderEnds = nullptr,
	bool stopAtFirst = false,
	bool includeAll = false);

mVect< mVect<wchar_t> > WalkForwardPathForFile(
	const aVect<wchar_t>& path,
	const aVect<wchar_t>& fileName,
	mVect< mVect<wchar_t> > * folderEnds = nullptr,
	bool stopAtFirst = false,
	bool includeAll = false);

mVect< mVect<wchar_t> > WalkForwardPathForFile(
	const aVect<wchar_t>& path,
	const aVect<wchar_t>& fileName1,
	const aVect<wchar_t>& fileName2,
	mVect< mVect<wchar_t> > * folderEnds = nullptr,
	bool stopAtFirst = false,
	bool includeAll = false);

template <class T1, class T2, class T3, class T4, class T5> 
void SplitPath(T1 & src, T2 * pDrive, T3 * pPath, T4 * pFName, T5 * pExt) {

	static WinCriticalSection cs;

	Critical_Section(cs) {
		char * _pDrive = nullptr;
		char * _pPath = nullptr;
		char * _pFName = nullptr;
		char * _pExt = nullptr;

		SplitPath((const char*)src,
			(char**)(pDrive ? &_pDrive : nullptr),
			(char**)(pPath ? &_pPath : nullptr),
			(char**)(pFName ? &_pFName : nullptr),
			(char**)(pExt ? &_pExt : nullptr));

		if (pDrive)
			if (std::is_same<T2, CharPointer >::value)
				pDrive->Wrap(_pDrive);
			else
				pDrive->Copy(_pDrive);

		if (pPath)
			if (std::is_same<T3, CharPointer >::value)
				pPath->Wrap(_pPath);
			else
				pPath->Copy(_pPath);

		if (pFName)
			if (std::is_same<T4, CharPointer >::value)
				pFName->Wrap(_pFName);
			else
				pFName->Copy(_pFName);

		if (pExt)
			if (std::is_same<T5, CharPointer >::value)
				pExt->Wrap(_pExt);
			else
				pExt->Copy(_pExt);
	}
}

#define DEFINE_SPLITPATH_1(a, b, c, d, a2, b2, c2, d2, a3, b3, c3, d3)					   \
template <class T, class T1>															   \
void SplitPath(T & src, a * pDrive, b * pPath, c * pFName, d * pExt) {					   \
	SplitPath<T, a2, b2, c2, d2> (src, (a2*)pDrive, (b2*)pPath, (c2*) pFName, (d2*) pExt); \
}																						   \
template <bool useless>																	   \
void SplitPath(char * src, a3 * pDrive, b3 * pPath, c3 * pFName, d3 * pExt) {			   \
	SplitPath((char*)src, (char**)pDrive, (char**)pPath, (char**)pFName, (char**)pExt);	   \
}																						   \
template <class T>																		   \
void SplitPath(T & src, a3 * pDrive, b3 * pPath, c3 * pFName, d3 * pExt) {				   \
	SplitPath((char*)src, (char**)pDrive, (char**)pPath, (char**)pFName, (char**)pExt);	   \
}	

#define DEFINE_SPLITPATH_2(a, b, c, d, a2, b2, c2, d2, a3, b3, c3, d3)					   \
template <class T, class T1, class T2>													   \
void SplitPath(T & src, a * pDrive, b * pPath, c * pFName, d * pExt) {					   \
	SplitPath<T, a2, b2, c2, d2> (src, (a2*)pDrive, (b2*)pPath, (c2*) pFName, (d2*) pExt); \
}

#define DEFINE_SPLITPATH_3(a, b, c, d, a2, b2, c2, d2, a3, b3, c3, d3)					   \
template <class T, class T1, class T2, class T3>										   \
void SplitPath(T & src, a * pDrive, b * pPath, c * pFName, d * pExt) {					   \
	SplitPath<T, a2, b2, c2, d2> (src, (a2*)pDrive, (b2*)pPath, (c2*) pFName, (d2*) pExt); \
}

DEFINE_SPLITPATH_1(T1, void, void, void, T1, aVect<char>, aVect<char>, aVect<char>, char*, void, void, void)
DEFINE_SPLITPATH_1(void, T1, void, void, aVect<char>, T1, aVect<char>, aVect<char>, void, char*, void, void)
DEFINE_SPLITPATH_1(void, void, T1, void, aVect<char>, aVect<char>, T1, aVect<char>, void, void, char*, void)
DEFINE_SPLITPATH_1(void, void, void, T1, aVect<char>, aVect<char>, aVect<char>, T1, void, void, void, char*)

DEFINE_SPLITPATH_2(T1, T2, void, void, T1, T2, aVect<char>, aVect<char>, char*, char*, void, void)
DEFINE_SPLITPATH_2(void, T1, T2, void, aVect<char>, T1, T2, aVect<char>, void, char*, char*, void)
DEFINE_SPLITPATH_2(void, T1, void, T2, aVect<char>, T1, aVect<char>, T2, void, char*, void, char*)

//DEFINE_SPLITPATH_2(void, T1, T2, void, CharPointer, T1, T2, CharPointer, void, const char*, const char*, void)

DEFINE_SPLITPATH_3(void, T1, T2, T3, aVect<char>, T1, T2, T3, void, char*, char*, char*)

#undef DEFINE_SPLITPATH_1
#undef DEFINE_SPLITPATH_2
#undef DEFINE_SPLITPATH_3

extern WinCriticalSection g_SplitPathW_cs;

template <class T1, class T2, class T3, class T4, class T5>
void SplitPathW(T1 & src, T2 * pDrive, T3 * pPath, T4 * pFName, T5 * pExt) {

	Critical_Section(g_SplitPathW_cs) {

		wchar_t * _pDrive = nullptr, *_pPath = nullptr, *_pFName = nullptr, *_pExt = nullptr;

		SplitPathW((const wchar_t*)src,
			(wchar_t**)(pDrive ? &_pDrive : nullptr),
			(wchar_t**)(pPath ? &_pPath : nullptr),
			(wchar_t**)(pFName ? &_pFName : nullptr),
			(wchar_t**)(pExt ? &_pExt : nullptr));

		if (pDrive)
			if (std::is_same<T2, CharPointer >::value)
				pDrive->Wrap(_pDrive);
			else
				pDrive->Copy(_pDrive);

		if (pPath)
			if (std::is_same<T3, CharPointer >::value)
				pPath->Wrap(_pPath);
			else
				pPath->Copy(_pPath);

		if (pFName)
			if (std::is_same<T4, CharPointer >::value)
				pFName->Wrap(_pFName);
			else
				pFName->Copy(_pFName);

		if (pExt)
			if (std::is_same<T5, CharPointer >::value)
				pExt->Wrap(_pExt);
			else
				pExt->Copy(_pExt);
	}
}

#define DEFINE_SPLITPATH_1(a, b, c, d, a2, b2, c2, d2, a3, b3, c3, d3)					   \
template <class T, class T1>															   \
void SplitPathW(T & src, a * pDrive, b * pPath, c * pFName, d * pExt) {					   \
	SplitPathW<T, a2, b2, c2, d2> (src, (a2*)pDrive, (b2*)pPath, (c2*) pFName, (d2*) pExt); \
}																						   \
template <bool useless>																	   \
void SplitPathW(wchar_t * src, a3 * pDrive, b3 * pPath, c3 * pFName, d3 * pExt) {			   \
	SplitPathW((wchar_t*)src, (wchar_t**)pDrive, (wchar_t**)pPath, (wchar_t**)pFName, (wchar_t**)pExt);	   \
}																						   \
template <class T>																		   \
void SplitPathW(T & src, a3 * pDrive, b3 * pPath, c3 * pFName, d3 * pExt) {				   \
	SplitPathW((wchar_t*)src, (wchar_t**)pDrive, (wchar_t**)pPath, (wchar_t**)pFName, (wchar_t**)pExt);	   \
}	

#define DEFINE_SPLITPATH_2(a, b, c, d, a2, b2, c2, d2, a3, b3, c3, d3)					   \
template <class T, class T1, class T2>													   \
void SplitPathW(T & src, a * pDrive, b * pPath, c * pFName, d * pExt) {					   \
	SplitPathW<T, a2, b2, c2, d2> (src, (a2*)pDrive, (b2*)pPath, (c2*) pFName, (d2*) pExt); \
}

#define DEFINE_SPLITPATH_3(a, b, c, d, a2, b2, c2, d2, a3, b3, c3, d3)					   \
template <class T, class T1, class T2, class T3>										   \
void SplitPathW(T & src, a * pDrive, b * pPath, c * pFName, d * pExt) {					   \
	SplitPathW<T, a2, b2, c2, d2> (src, (a2*)pDrive, (b2*)pPath, (c2*) pFName, (d2*) pExt); \
}

DEFINE_SPLITPATH_1(T1, void, void, void, T1, aVect<wchar_t>, aVect<wchar_t>, aVect<wchar_t>, wchar_t*, void, void, void)
DEFINE_SPLITPATH_1(void, T1, void, void, aVect<wchar_t>, T1, aVect<wchar_t>, aVect<wchar_t>, void, wchar_t*, void, void)
DEFINE_SPLITPATH_1(void, void, T1, void, aVect<wchar_t>, aVect<wchar_t>, T1, aVect<wchar_t>, void, void, wchar_t*, void)
DEFINE_SPLITPATH_1(void, void, void, T1, aVect<wchar_t>, aVect<wchar_t>, aVect<wchar_t>, T1, void, void, void, wchar_t*)

DEFINE_SPLITPATH_2(T1, T2, void, void, T1, T2, aVect<wchar_t>, aVect<wchar_t>, wchar_t*, wchar_t*, void, void)
DEFINE_SPLITPATH_2(void, T1, T2, void, aVect<wchar_t>, T1, T2, aVect<wchar_t>, void, wchar_t*, wchar_t*, void)
DEFINE_SPLITPATH_2(void, T1, void, T2, aVect<wchar_t>, T1, aVect<wchar_t>, T2, void, wchar_t*, void, wchar_t*)

//DEFINE_SPLITPATH_2(void, T1, T2, void, CharPointer, T1, T2, CharPointer, void, const wchar_t*, const wchar_t*, void)

DEFINE_SPLITPATH_3(void, T1, T2, T3, aVect<wchar_t>, T1, T2, T3, void, wchar_t*, wchar_t*, wchar_t*)

#pragma comment ( lib, "Gdi32.lib" )

HWND PutThere(HWND hParent, const wchar_t * className, const wchar_t * str, int width, int height,
int decX, int decY, INT_PTR index, int myFlag, DWORD additionalStyle = NULL, DWORD additionalStyleEx = NULL, void * ptr = NULL);
HWND PutBottom(HWND hParent, const wchar_t * className, const wchar_t * str, int width, int height, int marge = 0, INT_PTR index = 0, DWORD additionalStyle = NULL, DWORD additionalStyleEx = NULL, void * ptr = NULL);
HWND PutBottomEx(HWND hParent, const wchar_t * className, const wchar_t * str, int width, int height, int margeX = 0, int margeY = 0, INT_PTR index = 0, DWORD additionalStyle = NULL, DWORD additionalStyleEx = NULL, void * ptr = NULL);
HWND PutRight(HWND hParent, const wchar_t * className, const wchar_t * str, int width, int height, int marge = 0, INT_PTR index = 0, DWORD additionalStyle = NULL, DWORD additionalStyleEx = NULL, void * ptr = NULL);
HWND PutRightEx(HWND hParent, const wchar_t * className, const wchar_t * str, int width, int height, int margeX = 0, int margeY = 0, INT_PTR index = 0, DWORD additionalStyle = NULL, DWORD additionalStyleEx = NULL, void * ptr = NULL);

HWND PutTopLeft(HWND hParent, const wchar_t * className, const wchar_t * str, int width, int height, int margeX = 0, int margeY = 0, INT_PTR index = 0, DWORD additionalStyle = NULL, DWORD additionalStyleEx = NULL, void * ptr = NULL);
RECT GetRect(HWND hListePlats);
void MyPrintWindow(HWND h);
HWND MyGetDlgItem(HWND hWnd, int id);

HWND CreateToolTipWindow(int additionalStyles);
void AddToolToToolTip(HWND toolTipHwnd, HWND ctrlHwnd, char * toolTipText);

void DoEvents(int MsgMax = 0, HWND hWnd = NULL);

BOOL BrowseForFolderCore(char *path, const char *title, RECT *placement);
aVect<char> BrowseForFolder_old(const char * path = "", const char *title = "", RECT *placement = NULL);

aVect<char> BrowseForFile(const char * filters = "", const char * path = "", const char *title = "", RECT *placement = NULL);

void DbgStr(char* formatString, ...);
void DbgStr(wchar_t* formatString, ...);
int MyMessageBox(char* formatString, ...);
int MyMessageBox(wchar_t* formatString, ...);
int MyMessageBox(DWORD style, char* formatString, ...);
int MyMessageBox(HWND hWnd, DWORD style, char* formatString, ...);

int TextWidth(HWND hWnd, const wchar_t * str, bool accountForLineFeeds = true);
int TextHeight(HWND hWnd, const wchar_t * str, bool accountForLineFeeds = true);

void StopAllOtherThreads(bool resume = false);
void StopAllThreadsBut(DWORD threadId, bool resume = false);
	
void CoreDump(const char * str, void * pEx, int threadID, 
	char * file, size_t line, char * func, bool reportAndExit = false);

bool IsFileReadable(const char * fPath, DWORD * errCode = nullptr);
bool IsFileWritable(const char * fPath, DWORD * errCode = nullptr);
bool IsFileReadable(const wchar_t * fPath, DWORD * errCode = nullptr);
bool IsFileWritable(const wchar_t * fPath, DWORD * errCode = nullptr);

bool FileExists(const wchar_t * szPath);
bool FileExists(const char * szPath);

double tic(void);
double toc(double t);
double CPU_KernelTic(void);
double CPU_KernelToc(double kernelTic);
double CPU_UserTic(void);
double CPU_UserToc(double userTic);

double ComputationStartTime(void);

LOGFONTW CreateLogFont(
	wchar_t * fontName,
	LONG height = 0,
	LONG avWidth = 0,
	LONG escapement = 0,
	LONG orientation = 0,
	LONG weight = 0,
	BYTE italic = 0,
	BYTE underline = 0,
	BYTE strikeOut = 0,
	BYTE charSet = ANSI_CHARSET,
	BYTE outPrecision = OUT_DEFAULT_PRECIS,
	BYTE clipPrecision = CLIP_DEFAULT_PRECIS,
	BYTE quality = DEFAULT_QUALITY,
	BYTE pitchAndFamily = DEFAULT_PITCH | FF_DONTCARE);

template <class T> T * AlloCopy(T * el, size_t count, HANDLE heap) {

	size_t n = count*sizeof(T);
	T * retVal = (T*)HeapAlloc(heap, 0, n);
	if (!retVal) MY_ERROR("malloc = NULL");
	memcpy(retVal, el, n);
	return retVal;
}

template <class T> T * AlloCopy(T * el, HANDLE heap) {

	return  AlloCopy(el, 1, heap);
}

#define sign(a) (a >= 0 ? 1 : -1)

aVect<wchar_t> GetWin32ErrorDescription(DWORD systemErrorCode, bool appendErrorCode = true);
aVect<wchar_t> GetWin32ErrorDescriptionLocal(DWORD systemErrorCode, bool appendErrorCode = true);

WNDCLASSW MyRegisterClass(LRESULT(CALLBACK *WinProc)(HWND, UINT, WPARAM, LPARAM), const wchar_t * classTitle);
HINSTANCE GetCurrentModuleHandle();
HWND MyCreateWindow(const char * classTitle, const char * strTitle,
	int x = CW_USEDEFAULT, int y = CW_USEDEFAULT,
	int nX = CW_USEDEFAULT, int nY = CW_USEDEFAULT,
	DWORD style = WS_OVERLAPPEDWINDOW,
	HWND parent = NULL,
	HMENU menu = NULL,
	void * data = NULL,
	int SWP_FLAGS = SW_SHOW);

HWND MyCreateWindowEx(
	DWORD exStyle,
	const char * classTitle, const char * strTitle,
	int x = CW_USEDEFAULT, int y = CW_USEDEFAULT,
	int nX = CW_USEDEFAULT, int nY = CW_USEDEFAULT,
	DWORD style = WS_OVERLAPPEDWINDOW,
	HWND parent = NULL,
	HMENU menu = NULL,
	void * data = NULL,
	int SWP_FLAGS = SW_SHOW);

HWND MyCreateWindow(const wchar_t * classTitle, const wchar_t * strTitle,
	int x = CW_USEDEFAULT, int y = CW_USEDEFAULT,
	int nX = CW_USEDEFAULT, int nY = CW_USEDEFAULT,
	DWORD style = WS_OVERLAPPEDWINDOW,
	HWND parent = NULL,
	HMENU menu = NULL,
	void * data = NULL,
	int SWP_FLAGS = SW_SHOW);

HWND MyCreateWindowEx(
	DWORD exStyle,
	const wchar_t * classTitle, const wchar_t * strTitle,
	int x = CW_USEDEFAULT, int y = CW_USEDEFAULT,
	int nX = CW_USEDEFAULT, int nY = CW_USEDEFAULT,
	DWORD style = WS_OVERLAPPEDWINDOW,
	HWND parent = NULL,
	HMENU menu = NULL,
	void * data = NULL,
	int SWP_FLAGS = SW_SHOW);
#ifdef _DEBUG
#define GDI_SAFE_CALL(a) {												\
	unsigned short count = 0;											\
	char tmp[5000];														\
while (!(a)) {															\
if (count++>5) 	{														\
	sprintf(tmp, "GDI call returned 0, error %S",						\
	(wchar_t*)GetWin32ErrorDescriptionLocal(GetLastError()));			\
	MY_ERROR(tmp);														\
}																		\
	sprintf(tmp, "GDI call returned 0 (error %S) ! Retry...\n",			\
	(wchar_t*)GetWin32ErrorDescriptionLocal(GetLastError()));			\
	OutputDebugStringA(tmp);											\
	Sleep(1);															\
}																		\
}

#define WIN32_SAFE_CALL(a) if(!(a)) {									\
	char tmp[5000];														\
	sprintf(tmp, "WIN32 call returned 0, error %S",						\
	(wchar_t*)GetWin32ErrorDescriptionLocal(GetLastError()));			\
	MY_ERROR(tmp);														\
	}

#define EVENT_SAFE_CALL(a) if(!(a)) {									\
	char tmp[5000];														\
	sprintf(tmp, "Event call returned 0, error %S",						\
	(wchar_t*)GetWin32ErrorDescriptionLocal(GetLastError()));			\
	MY_ERROR(tmp);														\
}
#else
#define GDI_SAFE_CALL(a) (a)
#define WIN32_SAFE_CALL(a) (a)
#define EVENT_SAFE_CALL(a) (a)
#endif

#define PUTTHERE_INTO		1
#define PUTTHERE_RIGHT		2
#define PUTTHERE_BOTTOM		3

void * aVectEx_malloc(size_t s, void * heap);
void * aVectEx_realloc(void * ptr, size_t s, void * heap);
void aVectEx_free(void * ptr, void * heap);

void(__stdcall * const g_EnterCriticalSection)(LPCRITICAL_SECTION) = EnterCriticalSection;
void(__stdcall * const g_LeaveCriticalSection)(LPCRITICAL_SECTION) = LeaveCriticalSection;

class WinFile : public File {

protected:

	bool eof;
	HANDLE hFile;
	long long basePtr;
	bool console = false;

	aVect<wchar_t> filePath;

	virtual void CloseCore();
	virtual void OpenCore(const char * fp, const char * om);
	virtual void OpenCore(const wchar_t * fp, const char * om);
	virtual void FlushCore();
	virtual size_t ReadCore(void* ptr, size_t s, size_t n);
	virtual size_t WriteCore(void* ptr, size_t s, size_t n);
	virtual bool EofCore();

	HANDLE FileReadHandle();
	HANDLE FileWriteHandle();

public:

	virtual operator bool();
	long long GetCurrentPosition();

	WinFile::~WinFile();
	WinFile();
	WinFile(const WinFile&) = delete;
	WinFile(WinFile&&);
	WinFile& operator=(const WinFile&) = delete;
	WinFile& operator=(WinFile&&);
	WinFile(const char * fn);
	WinFile(const wchar_t * fn);
	WinFile(const char * fn, const char * om);
	WinFile(const wchar_t * fn, const char * om);
	WinFile& SetFile(const wchar_t * fn);
	WinFile& SetFile(const char * fn);
	const aVect<wchar_t>& GetFilePath() const;
	virtual WinFile& Open();
	WinFile& OpenConsole();
	WinFile& Open(const char * fn);
	WinFile& Open(const wchar_t * fn);
	WinFile& Open(const char * fn, const char * om);
	WinFile& Open(const wchar_t * fn, const char * om);
};

class WinCurrentConsole : private WinFile {

	public:
	WinCurrentConsole() {
		this->console = true;
		this->openMode = "rb";
	}

	//template <class T, class... Args>
	//size_t Write(T* format, Args&&... args) {
	//	return File::Write(format, std::forward<Args>(args)...);
	//}

	//size_t Write(const char * str) {
	//	return File::Write(str);
	//}

	template <class... Args>
	size_t Write(Args&&... args) {
		return File::Write(std::forward<Args>(args)...);
	}

	size_t Read(aVect<char> & data, size_t n) {
		return File::Read(data, n);
	}

	aVect<char> GetLine() {
		return (char*)File::GetLine();
	}

};

class MMFile : public WinFile {
private:

	HANDLE hFileMapping;
	char * pFileStart, * pFilePos, * pFileEnd;

protected:
	
	virtual void CloseCore();
	virtual void OpenCore(char * fp, char * om);
	virtual void FlushCore();
	virtual size_t ReadCore(void* ptr, size_t s, size_t n);
	virtual size_t WriteCore(void* ptr, size_t s, size_t n);

public:
	
	long long GetCurrentPosition();

	MMFile::~MMFile();
	MMFile();
	MMFile(const char * fn);
	MMFile(const wchar_t * fn);
	MMFile(const char * fn, const char * om);
	MMFile(const wchar_t * fn, const char * om);
};


mVect< mVect<wchar_t> > WalkBackPathForFile(
	const aVect<wchar_t>& path,
	const aVect<wchar_t>& fileName,
	aVect<WinFile>* pFiles = nullptr,
	bool stopAtFirst = false);

mVect< mVect<wchar_t> > WalkBackPathForFile(
	const aVect<wchar_t>& path,
	const aVect<wchar_t>& fileName1,
	const aVect<wchar_t>& fileName2,
	aVect<WinFile>* pFiles = nullptr,
	bool stopAtFirst = false);

mVect< mVect<wchar_t> > WalkBackPathForFile(
	const aVect<wchar_t>& path,
	const aVect< aVect<wchar_t> >& fileNames,
	aVect<WinFile>* pFiles = nullptr,
	bool stopAtFirst = false);


template <class T, HANDLE & heap> class aVectEx : public aVect<T, true, false, aVectEx_malloc, aVectEx_realloc, aVectEx_free, &heap> {
private:
	//HANDLE heap;
	CRITICAL_SECTION * pCs;
	unsigned int cs_spinCount;

	aVectEx& Copy(aVectEx & t);
	aVectEx& Copy(aVect& t);
	aVectEx(aVectEx& t);
	aVectEx(aVect& t);

public:

	aVectEx() {
		pCs = nullptr;
		cs_spinCount = 0;
	}
	//aVectEx(HANDLE hHeap) {
	//	heap = hHeap;
	//	pCs = nullptr;
	//	cs_spinCount = 0;
	//}
	aVectEx(size_t n) : aVect(n) {
		pCs = nullptr;
		cs_spinCount = 0;
		//this->Redim(n);
		////this->EnterCriticalSection();
		////g_dirtyHeapHandle = this->heap;
		//aVect<T>::pData = xVect_Create<T, false, aVectEx_malloc, &g_dirtyHeapHandle>(n);
		////this->LeaveCriticalSection();
		//if (!std::is_trivial<T>::value) xVect_for_inv(aVect<T>::pData, i) new(aVect<T>::pData + i) T;
	}
	//~aVectEx() {
	//	Free();
	//}

	CRITICAL_SECTION * CriticalSection() {
		return pCs;
	}

	//aVectEx& SetHeap(HANDLE hHeap) {
	//	if (aVect::pData) MY_ERROR("Can't set heap after allocation");
	//	heap = hHeap;
	//	return *this;
	//}

	aVectEx& CreateCriticalSection(unsigned int spinCount = 0) {

		if (pCs) MY_ERROR("Non impl\E9ment\E9");
		pCs = (CRITICAL_SECTION *)HeapAlloc(heap, 0, sizeof *pCs);
		if (!pCs) MY_ERROR("HeapAlloc failed");
		InitializeCriticalSectionAndSpinCount(pCs, spinCount);
		cs_spinCount = spinCount;
		return *this;
	}

	aVectEx& SetSpinCount(unsigned int spinCount) {
		if (spinCount != cs_spinCount) {
			cs_spinCount = spinCount;
			SetCriticalSectionSpinCount(pCs, spinCount);
		}
		return *this;
	}

	aVectEx& EnterCriticalSection(unsigned int spinCount) {
		if (!pCs) {
			CreateCriticalSection(cs_spinCount);
		}
		else SetSpinCount(spinCount);

		g_EnterCriticalSection(pCs);
		return *this;
	}

	aVectEx& EnterCriticalSection() {
		if (!pCs) CreateCriticalSection(cs_spinCount);
		g_EnterCriticalSection(pCs);
		return *this;
	}

	aVectEx& LeaveCriticalSection() {
		if (!pCs) MY_ERROR("Critical section not initialized");
		g_LeaveCriticalSection(pCs);
		return *this;
	}

	////Redim
	//aVectEx& Redim(size_t newCount) {
	//	//this->EnterCriticalSection();
	//	//g_dirtyHeapHandle = this->heap;
	//	aVect<T>::Redim_Core<false, false, true, true, aVectEx_malloc, aVectEx_realloc, &g_dirtyHeapHandle>(newCount);
	//	//this->LeaveCriticalSection();
	//	return *this;
	//}

	////RedimExact
	//aVectEx& RedimExact(size_t newCount) {
	//	this->EnterCriticalSection();
	//	g_dirtyHeapHandle = this->heap;
	//	aVect<T>::Redim_Core<true, false, true, true, aVectEx_malloc, aVectEx_realloc, &g_dirtyHeapHandle>(newCount);
	//	this->LeaveCriticalSection();
	//	return *this;
	//}

	////Push(byref)
	//aVectEx & Push(T & t) {

	//	if (!std::is_trivial<T>::value) {
	//		PlaceHolder<T> cpy;
	//		new(&cpy) T(t);
	//		xVect_Push_ByRef(*(PlaceHolder<T>**)(&pData), cpy, aVectEx_malloc, aVectEx_realloc, heap);
	//	}
	//	else xVect_Push_ByRef(pData, t, aVectEx_malloc, aVectEx_realloc, heap);

	//	return *this;
	//}

	////Push(byval)
	//aVectEx& Push_ByVal(T t) {

	//	if (!std::is_trivial<T>::value) {
	//		PlaceHolder<T> cpy;
	//		new(&cpy) T(t);
	//		xVect_Push_ByVal(*(PlaceHolder<T>**)&pData, cpy, aVectEx_malloc, aVectEx_realloc, heap);
	//	}
	//	else xVect_Push_ByVal(pData, t, aVectEx_malloc, aVectEx_realloc, heap);

	//	return *this;
	//}

	////Pop
	//T Pop(void) {

	//	if (!std::is_trivial<T>::value) {
	//		if (!xVect_Count(pData)) MY_ERROR("Vecteur vide");
	//		size_t i = xVect_Count(pData) - 1;
	//		PlaceHolder<T> el = ((PlaceHolder<T>*)pData)[i];
	//		xVect_Remove(pData, i, aVectEx_malloc, aVectEx_realloc, heap);
	//		return *((T*)&el);
	//	}
	//	else return xVect_Pop(pData, aVectEx_malloc, aVectEx_realloc, heap);
	//}

	////Pop
	//void Pop(T & t) {

	//	if (!std::is_trivial<T>::value) {
	//		if (!xVect_Count(pData)) MY_ERROR("Vecteur vide");
	//		size_t i = xVect_Count(pData) - 1;
	//		*(PlaceHolder<T>*)(&t) = ((PlaceHolder<T>*)pData)[i];
	//		xVect_Remove(pData, i, aVectEx_malloc, aVectEx_realloc, heap);
	//	}
	//	else t = xVect_Pop(pData, aVectEx_malloc, aVectEx_realloc, heap);
	//}

	////Remove
	//aVectEx&  Remove(size_t n) {
	//	if (!std::is_trivial<T>::value) {
	//		(pData + n)->~T();
	//	}
	//	xVect_Remove(pData, n, aVectEx_malloc, aVectEx_realloc, heap);
	//	return *this;
	//}

	////Insert
	//aVectEx& Insert_ByVal(T t, size_t n) {
	//	if (!std::is_trivial<T>::value) {
	//		PlaceHolder<T> cpy;
	//		new(&cpy) T(t);
	//		xVect_Insert_ByVal(*(PlaceHolder<T>**)&pData, cpy, n, aVectEx_malloc, aVectEx_realloc, heap);
	//	}
	//	xVect_Insert_ByVal(pData, t, n, aVectEx_malloc, aVectEx_realloc, heap);
	//	return *this;
	//}

	////Insert
	//aVectEx& Insert(T& t, size_t n) {
	//	if (!std::is_trivial<T>::value) {
	//		PlaceHolder<T> cpy;
	//		new(&cpy) T(t);
	//		xVect_Insert(*(PlaceHolder<T>**)&pData, cpy, n, aVectEx_malloc, aVectEx_realloc, heap);
	//	}
	//	xVect_Insert(pData, t, n, aVectEx_malloc, aVectEx_realloc, heap);
	//	return *this;
	//}

	//Free
	void Free(void) {
		aVect::Free();
		DeleteCriticalSection(pCs);
		//if (aVect<T>::pData) {
		//	xVect_Free(aVect<T>::pData, aVectEx_free, heap);
		//}
		//if (pCs) {
		//	DeleteCriticalSection(pCs);
		//	HeapFree(heap, 0, pCs);
		//}
	}

	//aVect<char> & sprintf(const char* fStr, ...) {
	//	va_list args;
	//	va_start(args, fStr);

	//	vsprintf(fStr, args);
	//	
	//	va_end(args);
	//	return *this;
	//}

	//aVect<char> & vsprintf(const char* fStr, va_list args) {
	//	
	//	static char * buffer;
	//	
	//	xVect_vsprintf(buffer, fStr, args);

	//	xVect_Copy(this->pData, buffer, aVectEx_malloc, aVectEx_realloc, heap);

	//	return *this;
	//}

	/*aVectEx& operator=(const aVect<T>& t) {
		aVect<T>::operator=(t);
		return *this;
	}
	*/

	aVectEx& Leak() {
		aVect::Leak();
		pCs = nullptr;
		return *this;
	}
};

struct InputBoxParams {
	aVect<wchar_t> * pBuffer;
	const wchar_t * title;
	const wchar_t * question;
	const wchar_t * suggestion;
	int length;
	bool editable;
	bool OkButton;
	bool CancelButton;
	bool CloseButton;
	HWND parentWindow;
	int left;
	int top;
	LOGFONTW * pLogFont;
	const wchar_t * additionalButton;
	bool (*additionalButtonCallback)(InputBoxParams * params);
	void * additionalButtonCallbackParams;
};

void InputBoxCore(
	aVect<wchar_t> & retVal,
	const wchar_t * title = nullptr,
	const wchar_t * question = nullptr,
	const wchar_t * suggestion = nullptr,
	int boxLength = -1,
	bool editable = true,
	bool OkButton = true,
	bool CancelButton = true,
	bool CloseButton = false,
	HWND parentWindow = NULL,
	int left = 0,
	int top = 0,
	LOGFONTW * pLogFont = nullptr,
	const wchar_t * additionalButton = nullptr,
	bool (*additionalButtonCallback)(InputBoxParams * params) = nullptr,
	void * additionalButtonCallbackParams = nullptr
	);

aVect<wchar_t> InputBox(
	const wchar_t * title = nullptr,
	const wchar_t * question = nullptr,
	const wchar_t * suggestion = nullptr,
	int boxLength = -1);

aVect<wchar_t> InputBox(
	LOGFONTW & logFont,
	const wchar_t * title = nullptr,
	const wchar_t * question = nullptr,
	const wchar_t * suggestion = nullptr,
	int boxLength = -1);

aVect<wchar_t> InputBox(
	HWND parentWindow,
	int left = 0,
	int top = 0,
	const wchar_t * title = nullptr,
	const wchar_t * question = nullptr,
	const wchar_t * suggestion = nullptr,
	int boxLength = -1);

aVect<wchar_t> InputBox(
	LOGFONTW & logFont,
	HWND parentWindow,
	int left = 0,
	int top = 0,
	const wchar_t * title = nullptr,
	const wchar_t * question = nullptr,
	const wchar_t * suggestion = nullptr,
	int boxLength = -1);

aVect<wchar_t> TextBox(
	const wchar_t * title = nullptr,
	const wchar_t * subTitle = nullptr,
	const wchar_t * text = nullptr,
	int boxLength = -1);

aVect<wchar_t> TextBox(
	LOGFONTW & logFont,
	const wchar_t * title = nullptr,
	const wchar_t * subTitle = nullptr,
	const wchar_t * text = nullptr,
	int boxLength = -1);

aVect<wchar_t> TextBox(
	HWND parentWindow,
	int left = 0,
	int top = 0,
	const wchar_t * title = nullptr,
	const wchar_t * question = nullptr,
	const wchar_t * suggestion = nullptr,
	int boxLength = -1);

aVect<wchar_t> TextBox(
	LOGFONTW & logFont,
	HWND parentWindow,
	int left = 0,
	int top = 0,
	const wchar_t * title = nullptr,
	const wchar_t * question = nullptr,
	const wchar_t * suggestion = nullptr,
	int boxLength = -1);

int MsgBoxEx(HWND hWnd, TCHAR *szText, TCHAR *szCaption, UINT uType);
void SetDumpOnUnhandledException(bool enable = true);
bool TestDumpOnUnhandledException();

class WinProcess {

	static const PROCESS_INFORMATION pi_zero;
	static const STARTUPINFOW si_zero;

	PROCESS_INFORMATION pi;
	STARTUPINFOW si;
	BOOL processCreated;

	void Destroy() {

		if (!this->free && this->IsRunning()) {
			SAFE_CALL(TerminateProcess(this->pi.hProcess, 0));
			SAFE_CALL(WaitForSingleObject(this->pi.hProcess, INFINITE), WAIT_FAILED);
		}

		if (this->si.hStdOutput) SAFE_CALL(CloseHandle(this->si.hStdOutput));
		if (this->si.hStdInput)  SAFE_CALL(CloseHandle(this->si.hStdInput));
		if (this->si.hStdError)  SAFE_CALL(CloseHandle(this->si.hStdError));
		if (this->pi.hProcess)   SAFE_CALL(CloseHandle(this->pi.hProcess));
		if (this->pi.hThread)    SAFE_CALL(CloseHandle(this->pi.hThread));

		this->si.hStdOutput = this->si.hStdInput = this->si.hStdError = NULL;
		this->pi.hProcess = this->pi.hThread = NULL;

		this->processCreated = false;
	}

public:

	HANDLE & hStdError;
	HANDLE & hStdOutput;
	HANDLE & hStdInput;
	DWORD  & dwFlags;
	const DWORD & dwProcessId;

	bool free = false;

	WinProcess() : pi(pi_zero), si(si_zero), processCreated(false), hStdError(si.hStdError), 
				   hStdOutput(si.hStdOutput), hStdInput(si.hStdInput), dwFlags(si.dwFlags), dwProcessId(pi.dwProcessId) {
		si.cb = sizeof(si);
	}

	WinProcess(const WinProcess &) = delete;
	WinProcess(WinProcess && that) : 
		hStdError(si.hStdError), hStdOutput(si.hStdOutput), 
		hStdInput(si.hStdInput), dwFlags(si.dwFlags), dwProcessId(pi.dwProcessId)
	{
		this->processCreated = that.processCreated;
		this->free = that.free;
		this->pi = that.pi;
		this->si = that.si;

		that.pi = that.pi_zero;
		that.si = that.si_zero;
		that.processCreated = false;
	}
	
	WinProcess & operator=(const WinProcess &) = delete;

	WinProcess & operator=(WinProcess && that) {
		this->~WinProcess();
		new (this) WinProcess(std::move(that));
	}

	BOOL IsRunning() {
		if (!this->processCreated) return false;
		else return WaitForSingleObject(this->pi.hProcess, 0) == WAIT_TIMEOUT;
	}

	WinProcess & WaitFor(DWORD milliseconds = INFINITE) {
		if (this->processCreated) {
			auto r = WaitForSingleObject(this->pi.hProcess, milliseconds);
			if (r == WAIT_FAILED) {
				SAFE_CALL(0);
				int a = 0;
			}
		}
		return *this;
	}

	HANDLE GetHandle() {
		return this->pi.hProcess;
	}

	BOOL Create(
		LPCWSTR lpApplicationName,
		LPWSTR lpCommandLine,
		LPSECURITY_ATTRIBUTES lpProcessAttributes,
		LPSECURITY_ATTRIBUTES lpThreadAttributes,
		BOOL bInheritHandles,
		DWORD dwCreationFlags,
		LPVOID lpEnvironment,
		LPCWSTR lpCurrentDirectory)
	{

		if (this->processCreated) MY_ERROR("process already created");

		this->processCreated = CreateProcessW(
			lpApplicationName, lpCommandLine,
			lpProcessAttributes, lpThreadAttributes,
			bInheritHandles, dwCreationFlags,
			lpEnvironment, lpCurrentDirectory,
			&this->si, &this->pi);

		return processCreated;
	}

	BOOL CreateFree(LPCWSTR lpApplicationName,
		LPWSTR lpCommandLine,
		LPSECURITY_ATTRIBUTES lpProcessAttributes,
		LPSECURITY_ATTRIBUTES lpThreadAttributes,
		BOOL bInheritHandles,
		DWORD dwCreationFlags,
		LPVOID lpEnvironment,
		LPCWSTR lpCurrentDirectory)
	{
		auto retVal = this->Create(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
			bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory);
		this->SetFree();
		return retVal;
	}

	WinProcess & Terminate() {
		this->Destroy();
	}

	WinProcess & SetFree(bool enable = true) {
		this->free = enable;
		return *this;
	}

	~WinProcess() {
		this->Destroy();
	}
};

SIZE GetNonClientAdditionalSize(HWND hWnd);

class WinNamedPipe {
	
	HANDLE hPipe;
	aVect<wchar_t> pipeName;
	DWORD dwOpenMode;
	DWORD dwPipeMode;
	DWORD nMaxInstances;
	WORD nDefaultTimeOut;

public:

	WinNamedPipe(
		wchar_t * pName,
		DWORD dwOpenMode = PIPE_ACCESS_OUTBOUND,
		DWORD dwPipeMode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
		DWORD nMaxInstances = PIPE_UNLIMITED_INSTANCES,
		WORD nDefaultTimeOut = 50)
		: 
		hPipe(NULL), 
		pipeName(L"\\\\.\\pipe\\%s", pName),
		dwOpenMode(dwOpenMode),
		dwPipeMode(dwPipeMode),
		nMaxInstances(nMaxInstances),
		nDefaultTimeOut(nDefaultTimeOut)
	{}

	BOOL Create(
		DWORD nOutBufferSize = 10000,
		DWORD nInBufferSize = 10000,
		LPSECURITY_ATTRIBUTES lpSecurityAttributes = NULL) {

		if (this->hPipe) SAFE_CALL(this->Close());

		this->hPipe = CreateNamedPipeW(this->pipeName, this->dwOpenMode, this->dwPipeMode, this->nMaxInstances,
			nOutBufferSize, nInBufferSize, this->nDefaultTimeOut, lpSecurityAttributes);

		return this->hPipe != INVALID_HANDLE_VALUE;
	}

	BOOL Write(
		LPCVOID lpBuffer,
		DWORD nNumberOfBytesToWrite,
		LPDWORD lpNumberOfBytesWritten = NULL,
		LPOVERLAPPED lpOverlapped = NULL) {

		DWORD unused;

		if (!lpNumberOfBytesWritten) lpNumberOfBytesWritten = &unused;

		return WriteFile(this->hPipe, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped);
	}

	BOOL Write(
		aVect<char> & str,
		LPDWORD lpNumberOfBytesWritten = NULL,
		LPOVERLAPPED lpOverlapped = NULL) {

		return this->Write(str, (DWORD)str.Count(), lpNumberOfBytesWritten, lpOverlapped);
	}

	wchar_t * Name() {
		return this->pipeName;
	}

	BOOL Close() {
		BOOL retVal = CloseHandle(this->hPipe);
		if (retVal) this->hPipe = NULL;
		return retVal;
	}

	~WinNamedPipe() {
		SAFE_CALL(this->Close());
	}
};

class WinConsole {

	WinCriticalSection criticalSection;
	WinNamedPipe namedPipe;
	WinProcess clientProcess;

public:

	WinConsole() : namedPipe(L"pipounet") {

		Critical_Section(this->criticalSection) {

			SAFE_CALL(this->namedPipe.Create());

			//char * exeName = "D:\\paul\\documents\\visual studio 2013\\Projects\\WinConsole\\Debug\\WinConsole.exe";
			auto exeName = L"C:\\Users\\Tristan\\Documents\\Visual Studio 2013\\Projects\\WinConsole\\Release\\WinConsole.exe";
			//char * exeName = "C:\\users-data\\saf924000\\Documents\\Visual Studio 2013\\Projects\\WinConsole\\Release\\WinConsole.exe";
			aVect<wchar_t> command(L"\"%s\" \"%s\"", exeName, namedPipe.Name());

			SAFE_CALL(this->clientProcess.Create(NULL, command, NULL, NULL, TRUE,
				CREATE_NEW_CONSOLE, NULL, NULL));
		}
	}

	template <class... Args>
	size_t Printf(const char* formatString, Args&&... args) {

		//static aVect<char> buffer;
		aVect<char> buffer;

		Critical_Section(this->criticalSection) {
			va_list args;
			va_start(args, formatString);
			buffer.sprintf(formatString, std::forward<Args>(args)...);
			va_end(args);

			size_t n = buffer.Count();
			if (n < 2) return 0;

			//Remove terminal null
			buffer.Redim(n - 1);

			while (true) {
				BOOL ok = this->namedPipe.Write(buffer);
				if (!ok) {
					auto lastErr = GetLastError();
					if (lastErr == ERROR_PIPE_LISTENING) {
						Sleep(50);
					} else if (lastErr == ERROR_NO_DATA) {
						/* do nothing */
					} else {
						SAFE_CALL(ok);
					}
				}
				else break;
			}
		}

		return buffer.Count();
	}
};

void AutoFitWindowForChildren(HWND hWnd, bool vertical = true, bool horizontal = true);
void RedrawWindow(HWND hWnd);

struct FileEvent {
	aVect<wchar_t> fileName;
	DWORD Action;
	time_t time;
	unsigned long long eventNumber;
	DWORD fileAttributes;

	FileEvent() {}

	FileEvent(aVect<wchar_t> & fileName, DWORD Action, time_t time,
		unsigned long long eventNumber, DWORD fileAttributes) :
		fileName(fileName),
		Action(Action),
		time(time),
		eventNumber(eventNumber),
		fileAttributes(fileAttributes) {}

	FileEvent(aVect<wchar_t> && fileName, DWORD Action, time_t time,
		unsigned long long eventNumber, DWORD fileAttributes) :
		fileName(std::move(fileName)),
		Action(Action),
		time(time),
		eventNumber(eventNumber),
		fileAttributes(fileAttributes)  {}

	FileEvent(FileEvent && that) :
		fileName(std::move(that.fileName)),
		Action(that.Action),
		time(that.time),
		eventNumber(that.eventNumber),
		fileAttributes(that.fileAttributes) {}

	FileEvent & operator=(FileEvent && that) {
		this->fileName = std::move(that.fileName);
		this->Action = that.Action;
		this->time = that.time;
		this->eventNumber = that.eventNumber;
		this->fileAttributes = that.fileAttributes;
		return *this;
	}
};

class WinEvent {

	HANDLE handle;

public:

	WinEvent(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCTSTR lpName) {
		this->handle = SAFE_CALL(CreateEvent(lpEventAttributes, bManualReset, bInitialState, lpName));
	}

	WinEvent(HANDLE h) {
		this->handle = h;
	}

	WinEvent(WinEvent && that) : handle (that.handle) {
		that.handle = NULL;
	}

	WinEvent & Set() {
		SAFE_CALL(SetEvent(this->handle));
		return *this;
	}

	WinEvent & Reset() {
		SAFE_CALL(ResetEvent(this->handle));
		return *this;
	}

	operator HANDLE() const {
		return this->handle;
	}

	unsigned IsSignaled() {
		return WaitForSingleObject(this->handle, 0) == WAIT_OBJECT_0;
	}

	DWORD WaitFor(DWORD dwMilliseconds = INFINITE) {
		return WaitForSingleObject(this->handle, dwMilliseconds);
	}

	bool AutoWaitFor(DWORD dwMilliseconds = INFINITE) {
		auto r = WaitForSingleObject(this->handle, INFINITE);
		if (r == WAIT_OBJECT_0)  return true;
		if (r == WAIT_TIMEOUT)   return false;
		if (r == WAIT_ABANDONED) throw std::runtime_error("WAIT_ABANDONED");
		if (r == WAIT_FAILED)    throw std::runtime_error("WAIT_FAILED");
		throw std::runtime_error("WaitForSingleObject unkown return value");
		return false;
	}
	 
	void Wait() {
		auto r = WaitForSingleObject(this->handle, INFINITE);
		if (r == WAIT_OBJECT_0) return;
		if (r == WAIT_ABANDONED) throw std::runtime_error("WAIT_ABANDONED");
		if (r == WAIT_FAILED)    throw std::runtime_error("WAIT_FAILED");
		if (r == WAIT_TIMEOUT)   throw std::runtime_error("WAIT_TIMEOUT???");
		throw std::runtime_error("WaitForSingleObject unkown return value");
		return;
	}

	void Leak() {
		this->handle = NULL;
	}

	~WinEvent() {
		if (this->handle) SAFE_CALL(CloseHandle(this->handle));
	}
};

struct WinEvents {

	aVect<HANDLE> events;

	WinEvents& Push(const WinEvent & event) {
		this->events.Push(event);
		return *this;
	}

	WinEvents& Push(const HANDLE & h) {
		this->events.Push(h);
		return *this;
	}

	bool WaitForAll(DWORD milliseconds = INFINITE) {

		auto r = WaitForMultipleObjects((DWORD)this->events.Count(), this->events, true, milliseconds);

		if (r >= WAIT_OBJECT_0 && r <= WAIT_OBJECT_0 + this->events.Count() - 1) {
			return true;
		}
		if (r == WAIT_TIMEOUT) return false;
		if (r == WAIT_FAILED) SAFE_CALL(0);/*MY_ERROR("WaitForMultipleObjects Failed");*/
		if (r >= WAIT_ABANDONED_0 && r <= WAIT_ABANDONED_0 + this->events.Count() - 1) {
			MY_ERROR("WaitForMultipleObjects Failed");
		}
		MY_ERROR("??");
		return false;
	}

	HANDLE WaitForAny(DWORD milliseconds = INFINITE) {

		auto r = WaitForMultipleObjects((DWORD)this->events.Count(), this->events, false, milliseconds);

		if (r >= WAIT_OBJECT_0 && r <= WAIT_OBJECT_0 + this->events.Count() - 1) {
			return this->events[r - WAIT_OBJECT_0];
		}
		if (r == WAIT_TIMEOUT) return NULL;
		if (r == WAIT_FAILED) SAFE_CALL(0);//MY_ERROR("WaitForMultipleObjects Failed");
		if (r >= WAIT_ABANDONED_0 && r <= WAIT_ABANDONED_0 + this->events.Count() - 1) {
			MY_ERROR("WaitForMultipleObjects Failed");
		}
		MY_ERROR("??");
		return NULL;
	}
};


DWORD WINAPI PathWatcher(LPVOID lpParameter);

struct FolderWatcher {

	aVect<wchar_t> path;
	mVect<FileEvent> fileEvents;
	HANDLE newChanges;
	HANDLE closeThread;
	HANDLE hThread;
	CRITICAL_SECTION * pCriticalSection;
	unsigned long long eventNumberStart;
	unsigned long long eventNumberDec;
	unsigned long long lastEventNumber;
	aVect<wchar_t> sourceFilePath;

	aVect<FileEvent> waitingVect, todoVect;

	bool missedEvents = false;
	typedef void(*MissedEventsUserFunction)(FolderWatcher * pFolderWatcher);

	MissedEventsUserFunction missedEventsUserFunc = nullptr;

	//void Fix_mVect(Fix_mVect_Data data) {
	//	fileEvents.FixRef(data);
	//}

	FolderWatcher(wchar_t * path, unsigned long long ens = 0) : path(L"%s", path) {
		this->newChanges = SAFE_CALL(CreateEvent(NULL, TRUE, FALSE, NULL));
		this->closeThread = SAFE_CALL(CreateEvent(NULL, TRUE, FALSE, NULL));
		pCriticalSection = (CRITICAL_SECTION*)malloc(sizeof *pCriticalSection);
		InitializeCriticalSectionAndSpinCount(pCriticalSection, 10000);
		this->lastEventNumber = this->eventNumberStart = ens;
		this->eventNumberDec = 0;
		this->hThread = SAFE_CALL(CreateThread(NULL, 0, PathWatcher, this, 0, NULL));
	}

	void AddEventNumberDec(unsigned long long dec) {
		this->eventNumberDec += dec;
		this->lastEventNumber += dec;
		aVect_static_for(this->waitingVect, i) {
			this->waitingVect[i].eventNumber += dec;
		}
		aVect_static_for(this->todoVect, i) {
			this->todoVect[i].eventNumber += dec;
		}
	}

	bool PopRaw(FileEvent & event, DWORD dwMilliseconds = INFINITE) {

		EnterCriticalSection(this->pCriticalSection);
		{
			for (;;) {
				SAFE_CALL(ResetEvent(this->newChanges));

				if (this->fileEvents) {
					this->fileEvents.Pop(0, event);
					event.eventNumber += this->eventNumberDec;
					this->lastEventNumber = event.eventNumber;
					break;
				}
				else {
					LeaveCriticalSection(this->pCriticalSection);
					DWORD result = WaitForSingleObject(this->newChanges, dwMilliseconds);
					if (result == WAIT_TIMEOUT)  return false;
					else if (result != WAIT_OBJECT_0) MY_ERROR("ici");
					EnterCriticalSection(this->pCriticalSection);
				}
			}
		}
		LeaveCriticalSection(this->pCriticalSection);

		this->sourceFilePath.sprintf(L"%s\\%s", (wchar_t*)this->path, (wchar_t*)event.fileName);
		event.fileAttributes = GetFileAttributesW(this->sourceFilePath);

		if (this->missedEvents) {
			if (this->missedEventsUserFunc) this->missedEventsUserFunc(this);
			this->missedEvents = false;
		}

		return true;
	}

	FolderWatcher & SetMissedEventsCallBack(MissedEventsUserFunction userFunc) {
		this->missedEventsUserFunc = userFunc;
		return *this;
	}

	//bool PopRaw(FileEvent & event, DWORD dwMilliseconds = INFINITE) {

	//	//EnterCriticalSection(this->pCriticalSection);
	//	//{
	//		for (;;) {
	//			SAFE_CALL(ResetEvent(this->newChanges));

	//			if (this->fileEvents) {
	//				EnterCriticalSection(this->pCriticalSection);
	//				this->fileEvents.Pop(0, event);
	//				LeaveCriticalSection(this->pCriticalSection);
	//				event.eventNumber += this->eventNumberDec;
	//				this->lastEventNumber = event.eventNumber;
	//				break;
	//			}
	//			else {
	//				//LeaveCriticalSection(this->pCriticalSection);
	//				DWORD result = WaitForSingleObject(this->newChanges, dwMilliseconds);
	//				if (result == WAIT_TIMEOUT)  return false;
	//				else if (result != WAIT_OBJECT_0) MY_ERROR("ici");
	//				//EnterCriticalSection(this->pCriticalSection);
	//			}
	//		}
	//	//}
	//	//LeaveCriticalSection(this->pCriticalSection);

	//	this->sourceFilePath.sprintf(L"%s\\%s", (wchar_t*)this->path, (wchar_t*)event.fileName);
	//	event.fileAttributes = GetFileAttributesW(this->sourceFilePath);

	//	return true;
	//}

	template <bool block = true> bool Pop(FileEvent & fileEvent, DWORD dwMilliseconds) {

		while (!this->todoVect) {

			FileEvent fe, emptyFe; 

			DWORD maxSecondsToWait = dwMilliseconds / 1000;

			time_t currentTime = time(nullptr);

			double waitDelay = maxSecondsToWait;

			if (waitingVect) {
				waitDelay = maxSecondsToWait - difftime(currentTime, waitingVect.Last().time);
				if (waitDelay < 0) waitDelay = 0;
			}

			bool result = this->PopRaw(fe, (DWORD)(waitDelay * 1000));

			currentTime = time(nullptr);

			if (result) {

				aVect_static_for(waitingVect, i) {
					if (waitingVect[i].Action == fe.Action && fe.Action == FILE_ACTION_MODIFIED &&
						_wcsicmp(waitingVect[i].fileName, fe.fileName) == 0)
					{
						if (difftime(fe.time, waitingVect[i].time) < 0) MY_ERROR("ici");
						waitingVect.Remove(i);
						break;
					}
				}
				waitingVect.Insert(0, std::move(fe));
			}

			aVect_for_inv(waitingVect, i) {
				if (difftime(currentTime, waitingVect[i].time) < 0) MY_ERROR("ici");
				if (difftime(currentTime, waitingVect[i].time) > dwMilliseconds / 1000) {
					todoVect.Insert(0, waitingVect.Pop(i));
				}
				else break;
			}

			if (!block) break;
		}

		if (this->todoVect) {
			this->todoVect.Pop(fileEvent);
			return true;
		}
		else {
			fileEvent.fileName.Redim(0);
			return false;
		}
	}

	~FolderWatcher() {
		SAFE_CALL(SetEvent(this->closeThread));
	}
};

struct FolderWatcherCore {

	aVect<wchar_t> path;
	mVect<FileEvent> fileEvents;
	WinEvent newChanges;
	WinEvent closeThread;
	CRITICAL_SECTION * pCriticalSection;
	HANDLE hThread;
	HANDLE folder;

	//void Fix_mVect(Fix_mVect_Data data) {
	//	fileEvents.FixRef(data);
	//}

	FolderWatcherCore(FolderWatcher * pFolderWatcher) :
		newChanges(pFolderWatcher->newChanges),
		closeThread(pFolderWatcher->closeThread),
		pCriticalSection(pFolderWatcher->pCriticalSection)
	{
		this->path.Copy(pFolderWatcher->path);
		this->fileEvents.Reference(pFolderWatcher->fileEvents);
		this->hThread = pFolderWatcher->hThread;

		this->folder = CreateFileW(this->path, FILE_LIST_DIRECTORY,
			FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

		if (this->folder == INVALID_HANDLE_VALUE) MY_ERROR(GetWin32ErrorDescription(GetLastError()));
	}

	~FolderWatcherCore() {
		SAFE_CALL(CloseHandle(this->folder));
		DeleteCriticalSection(this->pCriticalSection);
		free(this->pCriticalSection);
	}
};




class PerformanceChronometer {

	LARGE_INTEGER frequency;
	LARGE_INTEGER start;
	LARGE_INTEGER stop;

public:
	enum States { unstarted, started, stopped };

private:
	States state;

public:

	PerformanceChronometer(bool start = true) {
		QueryPerformanceFrequency(&this->frequency);
		if (start) this->Start();
		else this->state = unstarted;
	}

	void Start() {
		QueryPerformanceCounter(&this->start);
		this->state = started;
	}

	void Stop() {
		if (this->state == unstarted) return;
		QueryPerformanceCounter(&this->stop);
		this->state = stopped;
	}

	void Resume() {
		if (this->state == stopped) {
			LONGLONG startBackup = this->start.QuadPart;
			this->start = this->stop;
			this->state = started;
			this->start.QuadPart = startBackup + this->GetTime();
		}
		else if (this->state == unstarted) this->Start();
	}

	void Reset() {
		this->state = unstarted;
	}

	void Restart() {
		this->Start();
	}

	template <unsigned long long factor = 1> LONGLONG GetTime() {

		LONGLONG elapsedTime;

		if (this->state == started) {
			LARGE_INTEGER currentTime;
			QueryPerformanceCounter(&currentTime);
			elapsedTime = currentTime.QuadPart - this->start.QuadPart;
		}
		else if (this->state == unstarted)  {
			elapsedTime = 0;
		}
		else {
			elapsedTime = this->stop.QuadPart - this->start.QuadPart;
		}

		elapsedTime *= factor;

		return elapsedTime;
	}

	double GetMicroSeconds() {
		return (double)this->GetTime<1000000>() / this->frequency.QuadPart;
	}

	double GetMilliSeconds() {
		return (double)this->GetTime<1000>() / this->frequency.QuadPart;
	}

	double GetSeconds() {
		return (double)this->GetTime<1>() / this->frequency.QuadPart;
	}

	LONGLONG GetStartTick() {
		return this->start.QuadPart;
	}

	LONGLONG GetStopTick() {
		return this->stop.QuadPart;
	}

	States State() {
		return this->state;
	}
};

class Chronometer {

	double start;
	double stop;

	enum States { started, stopped };
	States state;

public:

	Chronometer() {
		this->Start();
	}

	void Start() {
		this->start = tic();
		this->state = started;
	}

	void Stop() {
		this->stop = tic();
		this->state = stopped;
	}

	void Resume() {
		if (this->state == stopped) {
			double startBackup = this->start;
			this->start = this->stop;
			this->state = started;
			this->start = startBackup + this->GetTime();
		}
	}

	template <unsigned long long factor = 1> double GetTime() {

		double elapsedTime;

		if (this->state == started) {
			elapsedTime = tic() - this->start;
		}
		else {
			elapsedTime = this->stop - this->start;
		}

		elapsedTime *= factor;

		return elapsedTime;
	}

	double GetMicroSeconds() {
		return this->GetTime<1000000>();
	}

	double GetSeconds() {
		return this->GetTime<1>();
	}
};





class PathEnumerator {

	aVect<wchar_t> path;
	HANDLE hFind;
	WIN32_FIND_DATAW findData;
	bool noMoreFiles;

public:

	PathEnumerator() : hFind(NULL), noMoreFiles(false) {}
	PathEnumerator(const wchar_t * p) : path(p), hFind(NULL), noMoreFiles(false) {}

	PathEnumerator(PathEnumerator & that) = delete;

	PathEnumerator(PathEnumerator && that) :
		path(std::move(that.path)),
		hFind(that.hFind),
		findData(that.findData),
		noMoreFiles(that.noMoreFiles)
	{
		that.hFind = NULL;
	}

	PathEnumerator & Reset() {
		if (this->hFind) {
			SAFE_CALL(FindClose(this->hFind));
			this->hFind = NULL;
		}
		this->noMoreFiles = false;
		return *this;
	}

	PathEnumerator & SetPath(wchar_t * p) {
		this->path.Copy(p);
		this->Reset();
		return *this;
	}

	bool Next() {
		if (this->noMoreFiles) return false;
		if (!this->hFind) {
			if (!this->path) MY_ERROR("path not set");
			this->hFind = FindFirstFileW(aVect<wchar_t>(L"%s\\*", (wchar_t*)this->path), &this->findData);
			if (this->hFind == INVALID_HANDLE_VALUE) {
				this->hFind = NULL;
				DWORD lastError = GetLastError();
				if (lastError == ERROR_NO_MORE_FILES || lastError == ERROR_ACCESS_DENIED) {
					this->noMoreFiles = true;
					return false;
				}
				else MY_ERROR(GetWin32ErrorDescription(GetLastError()));
			}
			if (this->findData.cFileName[0] == L'.') return this->Next();
			else return true;
		}
		else {
			if (!FindNextFileW(this->hFind, &this->findData)) {
				if (GetLastError() == ERROR_NO_MORE_FILES) {
					this->noMoreFiles = true;
					return false;
				}
				else MY_ERROR(GetWin32ErrorDescription(GetLastError()));
			}
			if (this->findData.cFileName[0] == L'.') return this->Next();
			else return true;
		}
	}

	WIN32_FIND_DATAW * GetCurrentData() {
		return (!this->hFind && !this->Next()) ? nullptr : &this->findData;
	}

	bool NoMoreFiles() {
		return this->noMoreFiles;
	}

	template <bool fullPath = false> PathEnumerator & GetCurrentFileName(aVect<wchar_t> & str) {

		if (!this->hFind && !this->Next()) {
			str.Free();
			return *this;
		}

		if (fullPath) {
			str.sprintf(L"%s\\%s", (wchar_t*)this->path, (wchar_t*)this->findData.cFileName);
		}
		else {
			str.sprintf(L"%s", (wchar_t*)this->findData.cFileName);
		}
		return *this;
	}

	template <bool fullPath = false> aVect<wchar_t> GetCurrentFileName() {
		aVect<wchar_t> str;
		this->GetCurrentFileName(str);
		return std::move(str);
	}

	aVect<wchar_t> GetCurrentFilePath() {
		return this->GetCurrentFileName<true>();
	}

	PathEnumerator & GetCurrentFilePath(aVect<wchar_t> & str) {
		return this->GetCurrentFileName<true>(str);
	}

	~PathEnumerator() {
		this->Reset();
	}

	explicit operator bool() {
		if (this->noMoreFiles) return false;
		if (!this->path) return false;
		if (!this->hFind) return this->Next();
		return true;
	}

	wchar_t * GetPath() {
		return this->path;
	}
};

class RecursiveFileEnumerator {

	aVect<wchar_t> path;
	aVect<PathEnumerator> pathEnumVect;
	bool noMoreFiles;

public:

	RecursiveFileEnumerator() {}
	RecursiveFileEnumerator(const wchar_t * p) : path(p), noMoreFiles(false) {}

	RecursiveFileEnumerator & Reset() {
		this->pathEnumVect.Free();
		noMoreFiles = false;
		return *this;
	}

	RecursiveFileEnumerator & SetPath(wchar_t * p) {
		this->path.Copy(p);
		this->Reset();
		return *this;
	}

	RecursiveFileEnumerator & MakeFullPath(aVect<wchar_t> & str, wchar_t * file, size_t levelDec = 0) {
		if (this->pathEnumVect.Count() > levelDec) {
			str.sprintf(L"%s\\%s", (wchar_t*)this->pathEnumVect.Last(-(int)levelDec * 0).GetPath(), file);
		}
		else str.sprintf(L"%s\\%s", (wchar_t*)this->path, file);

		return *this;
	}

	aVect<wchar_t> MakeFullPath(wchar_t * file, size_t levelDec = 0) {
		aVect<wchar_t> str;
		this->MakeFullPath(str, file, levelDec);
		return std::move(str);
	}

	bool Next() {
		if (this->noMoreFiles) return false;
		if (!this->pathEnumVect) {
			this->pathEnumVect.Grow(1);
			this->pathEnumVect.Last().SetPath(path);
			if (!this->pathEnumVect.Last().Next()) {
				this->noMoreFiles = true;
				return false;
			}
			return true;
		}
		else {
			auto & lastPath = this->pathEnumVect.Last();
			if (lastPath.NoMoreFiles()) {
				if (this->pathEnumVect.Count() == 1) {
					this->noMoreFiles = true;
					return false;
				}
				else {
					this->pathEnumVect.Grow(-1);
					if (this->pathEnumVect.Last().Next()) return true;
					else return this->Next();
				}
			}
			if ((lastPath.GetCurrentData()->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
				lastPath.GetCurrentData()->cFileName[0] != L'.')
			{
				//static aVect<wchar_t> str;
				aVect<wchar_t> str;
				this->MakeFullPath(str, this->pathEnumVect.Last().GetCurrentData()->cFileName, 1);
				if (this->pathEnumVect.Grow(1).Last().SetPath(str).Next()) {
					return true;
				}
				else return this->Next();
			}
			else if (lastPath.Next()) {
				return true;
			}
			else {
				if (this->pathEnumVect.Count() == 1) {
					this->noMoreFiles = true;
					return false;
				}
				else {
					this->pathEnumVect.Grow(-1);
					if (this->pathEnumVect.Last().Next()) return true;
					else return this->Next();
				}
			}
		}
		MY_ERROR("not reachable");
		return false;
	}

	WIN32_FIND_DATAW * GetCurrentData() {
		return (!this->pathEnumVect && !this->Next()) ?
			nullptr : this->pathEnumVect.Last().GetCurrentData();
	}

	template <bool FullPath = false> RecursiveFileEnumerator & GetCurrentFileName(aVect<wchar_t> & str) {
		if (!this->pathEnumVect && !this->Next()) {
			str.Free();
		}
		else this->pathEnumVect.Last().GetCurrentFileName<FullPath>(str);
		return *this;
	}

	RecursiveFileEnumerator & GetCurrentFilePath(aVect<wchar_t> & str) {
		this->GetCurrentFileName<true>(str);
		return *this;
	}

	template <bool FullPath = false> aVect<wchar_t> GetCurrentFileName() {
		aVect<wchar_t> str;
		this->GetCurrentFileName<FullPath>(str);
		return std::move(str);
	}

	aVect<wchar_t> GetCurrentFilePath() {
		return this->GetCurrentFileName<true>();
	}

	RecursiveFileEnumerator & GetCurrentFileSubPath(aVect<wchar_t> & str) {
		this->GetCurrentFileName<true>(str);
		str.sprintf(L"%s", (wchar_t*)str + wcslen(this->path) + 1);
		return *this;
	}

	aVect<wchar_t> GetCurrentFileSubPath() {
		aVect<wchar_t> str;
		this->GetCurrentFileName<true>(str);
		str.sprintf(L"%s", (wchar_t*)str + wcslen(this->path) + 1);
		return std::move(str);
	}

	bool NoMoreFiles() {
		return this->noMoreFiles;
	}

	~RecursiveFileEnumerator() {
		this->Reset();
	}

	explicit operator bool() {
		if (this->noMoreFiles) return false;
		if (!this->path) return false;
		if (!this->pathEnumVect) return this->Next();
		return true;
	}
};

DWORD IsDirectory(const wchar_t * dir);
DWORD IsDirectory(const char * dir);
DWORD IsDirectory(DWORD attrib);

class AutoHandleCloser {

	HANDLE handle;

public:

	AutoHandleCloser(HANDLE h) : handle(h) {}

	~AutoHandleCloser() {
		SAFE_CALL(CloseHandle(handle));
	}
};

void DeleteDirectory(wchar_t * directory);


struct OutputInfo;

struct VerboseLevelGuard {

	OutputInfo & outputInfo;
	long previousVerboseLevel;

	VerboseLevelGuard(OutputInfo & outputInfo, long scopeVerboseLevel);
	~VerboseLevelGuard();

	explicit operator bool() {
		return true;
	}
};

struct OutputInfo {

	enum PrintMode { Console, Buffer };

	PrintMode printMode;
	aVect<char> buffer;
	long outputVerboseLevel;
	long inputVerboseLevel;

	OutputInfo(
		PrintMode printMode = Console, 
		long outputVerboseLevel = 0,
		long inputVerboseLevel = 0)
	: 
		inputVerboseLevel(inputVerboseLevel), 
		outputVerboseLevel(outputVerboseLevel)
	{
		this->SetPrintMode(printMode);
	}

	OutputInfo & SetVerboseLevel_Input(long verboseLevel) {
		this->inputVerboseLevel = verboseLevel;
		return *this;
	}

	long GetVerboseLevel_Input() {
		return this->inputVerboseLevel;
	}

	OutputInfo & SetVerboseLevel_Output(long verboseLevel) {
		this->outputVerboseLevel = verboseLevel;
		return *this;
	}

	long GetVerboseLevel_Output() {
		return this->outputVerboseLevel;
	}

	OutputInfo & SetPrintMode(PrintMode printMode) {
		this->printMode = printMode;
		return *this;
	}
	 
	OutputInfo & Reset() {
		if (this->buffer) this->buffer.Redim(0);
		return *this;
	}

	template <class... Args>
	int printf(long verboseLevel, const char * fStr, Args&&... args) {

		if (this->outputVerboseLevel >= verboseLevel) {
			PrintfStaticArgChecker(std::forward<Args>(args)...);

			if (this->printMode == Console) {
				return ::printf(fStr, std::forward<Args>(args)...);
			}

			this->buffer.Append(fStr, std::forward<Args>(args)...);
		}
		return 0;
	}

	template <class... Args>
	int printf(const char * fStr, Args&&... args) {
		return this->printf(this->inputVerboseLevel, fStr, std::forward<Args>(args)...);
	}

	template <long verboseLevel, class... Args>
	int printf(const char * fStr, Args&&... args) {
		return this->printf(verboseLevel, fStr, std::forward<Args>(args)...);
	}

	template <class...Args>
	OutputInfo & Format(long verboseLevel, const char * fStr, Args&&... args) {

		if (this->outputVerboseLevel >= verboseLevel) {
			if (this->printMode == Console) {
				::xPrintf(fStr, std::forward<Args>(args)...);
			}

			this->buffer.AppendFormat(fStr, std::forward<Args>(args)...);
		}
		return *this;
	}

	template <class... Args>
	OutputInfo & Format(const char * fStr, Args&&... args) {
		return this->Format(this->inputVerboseLevel, fStr, std::forward<Args>(args)...);
	}

	template <long verboseLevel, class... Args>
	OutputInfo & Format(const char * fStr, Args&&... args) {
		return this->Format(verboseLevel, fStr, std::forward<Args>(args)...);
	}

	VerboseLevelGuard SetScopeVerbose(long scopeVerboseLevel) {
		return VerboseLevelGuard(*this, scopeVerboseLevel);
	}
};


inline size_t GraphemCount(wchar_t * str) {

	size_t len = 0;
	for (;;) {
		wchar_t * prev = str;
		str = CharNextW(str);
		if (prev == str) break;
		++len;
	}

	return len;
}


//from http://randomascii.wordpress.com/2012/04/21/exceptional-floating-point/
class FPExceptionEnabler {

	unsigned int mOldValues;

	// Make the copy constructor and assignment operator private
	// and unimplemented to prohibit copying.
	FPExceptionEnabler(const FPExceptionEnabler&);
	FPExceptionEnabler& operator=(const FPExceptionEnabler&);

public:
	// Overflow, divide-by-zero, and invalid-operation are the FP
	// exceptions most frequently associated with bugs.
	FPExceptionEnabler(unsigned int enableBits = _EM_OVERFLOW |
		_EM_ZERODIVIDE | _EM_INVALID)
	{
		// Retrieve the current state of the exception flags. This
		// must be done before changing them. _MCW_EM is a bit
		// mask representing all available exception masks.
		_controlfp_s(&mOldValues, 0, 0);

		// Make sure no non-exception flags have been specified,
		// to avoid accidental changing of rounding modes, etc.
		enableBits &= _MCW_EM;

		// Clear any pending FP exceptions. This must be done
		// prior to enabling FP exceptions since otherwise there
		// may be a \91deferred crash\92 as soon the exceptions are
		// enabled.
		_clearfp();

		// Zero out the specified bits, leaving other bits alone.
		_controlfp_s(0, ~enableBits, enableBits);
	}
	~FPExceptionEnabler()
	{
		// Reset the exception state.
		_controlfp_s(0, mOldValues, _MCW_EM);
	}

};

HICON CreateIconFromFile(const wchar_t * file);
HICON CreateIconFromFile(const char * file);
HBRUSH CreateCheckerBoardBrush(bool coarse = false);

void MySetConsoleSize(int scBufSizeX, int scBufSizeY, int winVisBufX, int winPix_dx, int winPix_dy);
char * strstr_caseInsensitive(const char * str, const char * subStr);

size_t ReplaceStr(aVect<char> & str, char * subStr, char * replaceStr, bool onlyFirstOccurence = false, bool caseSensitive = true, bool reverse = false, size_t startFrom = 0);

size_t ReplaceStr_CaseInsensitive(aVect<char> & str, char * subStr, char * replaceStr, bool onlyFirstOccurence = false, size_t startFrom = 0);
size_t ReplaceStr_FirstOccurence(aVect<char> & str, char * subStr, char * replaceStr, bool caseSensitive = true, size_t startFrom = 0);
size_t ReplaceStr_LastOccurence(aVect<char> & str, char * subStr, char * replaceStr, bool caseSensitive = true, size_t startFrom = 0);
size_t ReplaceStr_FirstOccurence_CaseInsensitive(aVect<char> & str, char * subStr, char * replaceStr, size_t startFrom = 0);
size_t ReplaceStr_LastOccurence_CaseInsensitive(aVect<char> & str, char * subStr, char * replaceStr, size_t startFrom = 0);

aVect<char> ReplaceStr(char * str, char * subStr, char * replaceStr, bool onlyFirstOccurence = false, bool caseSensitive = true, bool reverse = false, size_t startFrom = 0);

aVect<char> ReplaceStr_CaseInsensitive(char * str, char * subStr, char * replaceStr, bool onlyFirstOccurence = false, bool reverse = false, size_t startFrom = 0);
aVect<char> ReplaceStr_FirstOccurence(char * str, char * subStr, char * replaceStr, bool caseSensitive = true, size_t startFrom = 0);
aVect<char> ReplaceStr_LastOccurence(char * str, char * subStr, char * replaceStr, bool caseSensitive = true, size_t startFrom = 0);
aVect<char> ReplaceStr_FirstOccurence_CaseInsensitive(char * str, char * subStr, char * replaceStr, size_t startFrom = 0);
aVect<char> ReplaceStr_LastOccurence_CaseInsensitive(char * str, char * subStr, char * replaceStr, size_t startFrom = 0);

int strcmp_caseInsensitive(const char * str1, const char * str2);

char * reverse_strchr(char * str, char chr);

DWORD FindProcessId(char* processName);

aVect<wchar_t> BasicFileDialog(
	const wchar_t * defaultPath,
	bool save, bool pickFolder,
	const aVect<COMDLG_FILTERSPEC> & c_rgSaveTypes,
	const wchar_t * title);

aVect<wchar_t> BasicFileDialogWrapper(
	const wchar_t * typeExtension,
	const wchar_t * typeName,
	const wchar_t * defaultPath,
	bool save, bool pickFolder,
	const wchar_t * title);

aVect<wchar_t> FileSaveDialog(
	const wchar_t * typeExtension = nullptr,
	const wchar_t * typeName = nullptr,
	const wchar_t * defaultPath = nullptr,
	const wchar_t * title = nullptr);

aVect<wchar_t> FileOpenDialog(
	const wchar_t * typeExtension = nullptr,
	const wchar_t * typeName = nullptr,
	const wchar_t * defaultPath = nullptr,
	const wchar_t * title = nullptr);

aVect<wchar_t> BrowseForFolder(const wchar_t * defaultPath = nullptr, const wchar_t * title = nullptr);

aVect<char> FileSaveDialogA(
	const char * typeExtension = nullptr,
	const char * typeName = nullptr,
	const char * defaultPath = nullptr,
	const char * title = nullptr);

aVect<char> FileSaveDialog(
	const char * typeExtension,
	const char * typeName = nullptr,
	const char * defaultPath = nullptr,
	const char * title = nullptr);

aVect<char> FileOpenDialog(
	const char * typeExtension = nullptr,
	const char * typeName = nullptr,
	const char * defaultPath = nullptr,
	const char * title = nullptr);

aVect<char> FileOpenDialogA(
	const char * typeExtension = nullptr,
	const char * typeName = nullptr,
	const char * defaultPath = nullptr,
	const char * title = nullptr);

aVect<char> BrowseForFolder(const char * defaultPath, const char * title = nullptr);

aVect<char> BrowseForFolderA(const char * defaultPath = nullptr, const char * title = nullptr);

void CreateBMPFile(const wchar_t * pszFile, PBITMAPINFO pbi, HBITMAP hBMP, HDC hDC);
PBITMAPINFO CreateBitmapInfoStruct(HBITMAP hBmp);

LSTATUS GetRegistry(aVect<wchar_t> & value, HKEY key, const wchar_t * path, const wchar_t * valueName);

aVect<wchar_t> GetRegistry(HKEY key, const wchar_t * path, const wchar_t * valueName);
bool WriteRegistry(HKEY key, const wchar_t * path, const wchar_t * valueName, const wchar_t * value, LSTATUS * pStatus = nullptr);

aVect<char> GetRegistry(HKEY key, const char * path, const char * valueName);
bool WriteRegistry(HKEY key, const char * path, const char * valueName, const char * value, LSTATUS * pStatus = nullptr);

template <class To, class From> To BoundedCast(From val) {
#undef max
#undef min

	if (val > std::numeric_limits<To>::max()) {
		return std::numeric_limits<To>::max();
	}
	if (val < std::numeric_limits<To>::min()) {
		return std::numeric_limits<To>::min();
	}

	return static_cast<To>(val);
}

struct TopWindowFinder {
	aVect<wchar_t> buf;
	aVect<wchar_t> strToFind;
	HWND hWnd;

	static BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam) {

		auto params = (TopWindowFinder*)lParam;

		auto&& buf = params->buf;

		for (;;) {
			//auto buf_count = BoundedCast<int>(buf.Count());
			auto buf_count = (int)buf.Count();
			auto ans = (size_t)GetWindowTextW(hWnd, buf, buf_count);
			if (ans + 1 < buf.Count()) break;
			buf.Redim(2 * buf.Count());
		}

		if (strcmp_caseInsensitive(params->strToFind, buf) == 0) {
			params->hWnd = hWnd;
			return false;
		}

		return true;
	}

public:
	TopWindowFinder() {}
	TopWindowFinder(const char * strToFind) : strToFind(strToFind) {}
	TopWindowFinder(const wchar_t * strToFind) : strToFind(strToFind) {}

	HWND GetHwnd(const char * strToFind) {
		return this->GetHwnd(aVect<wchar_t>(strToFind));
	}

	HWND GetHwnd(const wchar_t * strToFind = nullptr) {

		if (strToFind) this->strToFind = strToFind;
		this->hWnd = NULL;

		if (!this->buf.Count()) this->buf.Redim(200);

		EnumWindows(EnumWindowsProc, (LPARAM)this);

		return this->hWnd;
	}
};


HTREEITEM MyTreeView_InsertItem(HWND hTree, HTREEITEM hParent, HTREEITEM hInsertAfter, const wchar_t * str, void * param = nullptr);
void MyTreeView_RemoveCheckBox(HWND hTreeView, HTREEITEM hNode);

void * MyTreeView_GetItemParam(HWND hWnd, HTREEITEM hItem);

const wchar_t * MyTreeView_GetItemText(aVect<wchar_t>& buf, HWND hWnd, HTREEITEM hItem);

aVect<wchar_t> MyTreeView_GetItemText(HWND hWnd, HTREEITEM hItem);

SIZE_T GetLargestFreeContinuousBlock(LPVOID *lpBaseAddr = NULL);

struct SeTranslateGuard {

	//_se_translator_function function;
	_se_translator_function previous;

	SeTranslateGuard(_se_translator_function function) /*: function(function) */ {
		this->previous = _set_se_translator(function);
	}

	~SeTranslateGuard() {
		_set_se_translator(this->previous);
	}
};

aVect<wchar_t> GetCurrentProcessImagePath();
size_t GetCurrentProcessPrivateBytes();


inline aVect<wchar_t> Wide(const char * str) {
	return str;
}

inline aVect<char> ASCII(const wchar_t * str) {
	return str;
}

bool IsNetworkPath(const wchar_t * path);
bool IsNetworkPath(const char * path);

BOOL MessagePump();

template <class Str>
bool StrEqualCore(const Str * s1, const Str * s2) {
	
	if (!s1 && !s2) return true;
	if (!s1 || !s2) return false;

	static_assert(std::is_same<Str, wchar_t>::value || std::is_same<Str, char>::value, "Wrong string type");

	auto func = Choose<std::is_same<Str, wchar_t>::value>(_wcsicmp, _strcmpi);
		
	return func(s1, s2) == 0;
}

inline bool StrEqual(const char * s1, const char * s2) {
	return StrEqualCore(s1, s2);
}

inline bool StrEqual(const wchar_t * s1, const wchar_t * s2) {
	return StrEqualCore(s1, s2);
}

aVect<wchar_t> MyGetWindowText(HWND hWnd);
void MyGetWindowText(aVect<wchar_t> & str, HWND hWnd);
aVect<wchar_t> ListBox_MyGetText(HWND hListBox);
void ListBox_MyGetText(aVect<wchar_t> str, HWND hListBox);
void CopyToClipBoard(const aVect<wchar_t> & str);
void AdjustWindowSizeForText(HWND hWnd, bool adjust_width, bool adjust_height);

#endif

