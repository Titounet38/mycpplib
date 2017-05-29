
#define _CRT_SECURE_NO_WARNINGS

#include "WinUtils.h"
#include "ShlObj.h"
#include "xVect.h"
#include "string.h"
#include "TlHelp32.h"
#include "psapi.h"

#pragma warning( push )
#pragma warning( disable : 4091 )
#include "DbgHelp.h"
#pragma warning( pop ) 

#include "Shobjidl.h"
#include "shlwapi.h"

#define DEBUG_FILENAME "LastError.log"
#pragma comment(lib, "dbghelp.lib") 
#pragma comment(lib, "shlwapi.lib") 

//extern HANDLE g_dirtyHeapHandle;
 
BOOL CALLBACK EnumFunc(HWND hWnd, LPARAM lParam) {

	//static aVect<char> text(1000000);
	aVect<char> text(1000000);
	//static aVect<char> cName(1024);
	aVect<char> cName(1024);
	 
	size_t				 & curArgGroup = ((EnumFuncArg*)lParam)->currentArgGroup;
	mVect<ArgGroup>		 & argGroup = ((EnumFuncArg*)lParam)->argGroup;
	mVect<char>          & WC_Name     = ((EnumFuncArg*)lParam)->winClassName;
	int                  & enumOption  = ((EnumFuncArg*)lParam)->enumOption;

	SendMessageTimeoutA(hWnd, WM_GETTEXT, (WPARAM)(text.Count()), (LPARAM)(char*)text, SMTO_ABORTIFHUNG, 5000, NULL);

	GetClassNameA(hWnd, (char*)cName, (int)cName.Count());

	if (enumOption & ENUM_ALL) {
		if (strcmp(cName, WC_Name) == 0) {
			mVect_static_for(argGroup, i) {
				if (strstr(text, argGroup[i].stringToFindInWinTitle)) {
					enumOption = ENUM_LB;
					curArgGroup = i;
					EnumChildWindows(hWnd, EnumFunc, lParam);
					enumOption = ENUM_ALL;
					break;
				}
			}
		}
		else {
			enumOption = ENUM_CHILD;
			EnumChildWindows(hWnd, EnumFunc, lParam);
			enumOption = ENUM_ALL;
		}
	}
	else if (enumOption & ENUM_LB) {
		if (strcmp(cName, "ListBox") == 0) {
			mVect<char> & r = argGroup[curArgGroup].listBoxContents.Grow(1).Last();
			size_t I = SendMessageA(hWnd, LB_GETCOUNT, NULL, NULL);
			for (size_t i=0; i<I; ++i) {
				size_t len = SendMessageA(hWnd, LB_GETTEXTLEN, (WPARAM)(i), NULL);
				if (text.Count() > len+1) {
					SendMessageA(hWnd, LB_GETTEXT, (WPARAM)(i), (LPARAM)(char*)text);
					size_t jStart = r.Count();
					r.Redim(jStart + len + 1);
					size_t J = r.Count();
					for (size_t j=jStart; j<jStart+len; ++j) {
						r[j] = text[j-jStart];
					}
					r.Last() = '\n';
				}
				else MY_ERROR("text too long");
			}
			r.Push((char)0);
		}
	}

	return true;
}

void MyTerminateProcess(PROCESS_INFORMATION & pi, STARTUPINFO & si) {

	if (SAFE_CALL(WaitForSingleObject(pi.hProcess, 0), WAIT_FAILED) == WAIT_TIMEOUT) {
		//printf("Terminating process %d.\n", pi.dwProcessId);
		SAFE_CALL(TerminateProcess(pi.hProcess, 0), 0);
		SAFE_CALL(WaitForSingleObject(pi.hProcess, INFINITE), WAIT_FAILED);
	}
	SAFE_CALL(CloseHandle(pi.hProcess), 0);
	SAFE_CALL(CloseHandle(pi.hThread), 0);
}

DWORD WinKbHit() {

	DWORD nInputEvents;
	auto hStdInput = SAFE_CALL(GetStdHandle(STD_INPUT_HANDLE), INVALID_HANDLE_VALUE);

	if (hStdInput == NULL) return 0;

	SAFE_CALL(GetNumberOfConsoleInputEvents(hStdInput, &nInputEvents), 0);

	const DWORD nCharEventToRead = 1000;
	DWORD nKeysWaiting = 0;

	static WinCriticalSection cs;

	Critical_Section(cs) {

		static aVect<INPUT_RECORD> ir; 
		ir.Redim(nCharEventToRead);

		if (nInputEvents) {
			SAFE_CALL(PeekConsoleInput(hStdInput, ir, nCharEventToRead, &nInputEvents), 0);

			for (DWORD i = 0; i < nInputEvents; ++i) {

				if (ir[i].EventType == KEY_EVENT) {
					if (ir[i].Event.KeyEvent.bKeyDown) {
						char c = *(char*)(&ir[i].Event.KeyEvent.uChar);
						if (c != 0) {
							nKeysWaiting++;
						}
					}
				}
			}
		}
	}

	return nKeysWaiting;
}

void * WinGetCharsCore(DWORD nCharMaxToRead, bool only1char) {

	//static aVect<INPUT_RECORD> ir;
	aVect<INPUT_RECORD> ir;

	aVect<char> buffer;
	char c;

	if (only1char && nCharMaxToRead != 1) MY_ERROR("only1char && nCharMaxToRead != 1");

	if (nCharMaxToRead) {
	
		ir.Redim(nCharMaxToRead);
		buffer.Redim(nCharMaxToRead+1).Set(0);

		bool keyEvents = false;

		while (!keyEvents) {
			DWORD nInputEvents;
			SAFE_CALL(ReadConsoleInput(SAFE_CALL(GetStdHandle(STD_INPUT_HANDLE), INVALID_HANDLE_VALUE), ir, nCharMaxToRead, &nInputEvents), 0);

			if (nInputEvents > nCharMaxToRead) nInputEvents = nCharMaxToRead;

			for (DWORD i=0; i<nInputEvents; ++i) {
				if (ir[i].EventType == KEY_EVENT) {
					if (ir[i].Event.KeyEvent.bKeyDown) {
						c = *(char*)(&ir[i].Event.KeyEvent.uChar);
						if (c!=0) {
							buffer[i] = c;
							keyEvents = true;
						}
					}
				}
			}
		}
	}

	if (only1char) {
		return (void*)c;
	}
	else {
		char * pData = buffer.GetDataPtr();
		buffer.Leak();
		return pData;
	}
}

aVect<char> WinGetChars(DWORD nCharMaxToRead) {

	void * pData = WinGetCharsCore(nCharMaxToRead, false);
	
	return aVect<char>((char*)pData);
}

char WinGetChar() {

	void * pData = WinGetCharsCore(1, true);

	return *(char*)(&pData);
}

bool IsFileReadable(const wchar_t * fPath, DWORD * errCode) {

	HANDLE f = CreateFileW(fPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (f == INVALID_HANDLE_VALUE) {
		if (errCode) *errCode = GetLastError();
		return false;
	}
	CloseHandle(f);
	return true;
}

bool IsFileWritable(const wchar_t * fPath, DWORD * errCode) {

	HANDLE f = CreateFileW(fPath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (f == INVALID_HANDLE_VALUE) {
		f = CreateFileW(fPath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_NEW, 0, NULL);
		if (f == INVALID_HANDLE_VALUE) {
			if (errCode) *errCode = GetLastError();
			return false;
		}
		CloseHandle(f);
		SAFE_CALL(DeleteFileW(fPath));
		return true;
	}
	CloseHandle(f);
	return true;
}

bool IsFileReadable(const char * fPath, DWORD * errCode) {

	return IsFileReadable(aVect<wchar_t>(fPath), errCode);
}

bool IsFileWritable(const char * fPath, DWORD * errCode) {

	return IsFileWritable(aVect<wchar_t>(fPath), errCode);
}

void RemoveFiles(aVect< aVect<wchar_t> > & files2remove, char * introMsg, LogFile & logFile) {

	while (true) {

		bool removeError = false;
		bool anyReadableFile = false;

		aVect_static_for(files2remove, i) {
			if (IsFileReadable(files2remove[i])) {
				anyReadableFile = true;
				break;
			}
		}

		if (anyReadableFile) {
			if (introMsg) if (logFile) logFile.Printf(1, "%s\n", introMsg);
		}

		aVect_static_for(files2remove, i) {
			if (IsFileReadable(files2remove[i])) {
				if (logFile) logFile.Printf(1, L"- \"%s\"\n", (wchar_t*)files2remove[i]);
			}
		}

		aVect_static_for(files2remove, i) {
			if (DeleteFileW(files2remove[i]) != 0 && IsFileReadable(files2remove[i])) {
				if (logFile) logFile.Printf(L"Couldn't delete \"%s\"\n", (wchar_t*)files2remove[i]);
				removeError = true;
			}
		}

		if (removeError) {
			//if (logFile) logFile.Printf("Retrying in 5 seconds...\n");
			/*double start = tic();
			Sleep(5000);*/
			logFile.Printf("Press enter...\n");
			getchar();
		}
		else break;
	}
}

//=================================
//============ WinFile ============
//=================================

WinFile::WinFile() : File(), hFile(nullptr), basePtr(0), eof(false) {}
//WinFile::WinFile(const char * fn) : File(fn), hFile(nullptr), basePtr(0), eof(false) {}
//WinFile::WinFile(const char * fn, const char * om) : File(fn, om), hFile(nullptr), basePtr(0), eof(false) {}

WinFile::WinFile(const char * fn) : File(fn), filePath(fn), hFile(nullptr), basePtr(0), eof(false) {}
WinFile::WinFile(const char * fn, const char * om) : File(fn, om), filePath(fn), hFile(nullptr), basePtr(0), eof(false) {}

WinFile::WinFile(const wchar_t * fn) : File((aVect<char>)fn), filePath(fn), hFile(nullptr), basePtr(0), eof(false) {}
WinFile::WinFile(const wchar_t * fn, const char * om) : File((aVect<char>)fn, om), filePath(fn), hFile(nullptr), basePtr(0), eof(false) {}

WinFile & WinFile::SetFile(const wchar_t * fn)
{
	this->Close();
	this->filePath = fn;
	File::filePath = fn;
	return *this;
}

WinFile & WinFile::SetFile(const char * fn)
{
	this->Close();
	this->filePath = fn;
	File::filePath = fn;
	return *this;
}

const aVect<wchar_t> & WinFile::GetFilePath() const {
	return this->filePath;
}

WinFile::WinFile(WinFile && that) : 
	File(std::move(that)), 
	filePath(std::move(that.filePath)),
	hFile(that.hFile), 
	basePtr(that.basePtr), 
	eof(that.eof)
{
	that.hFile = NULL;
	that.basePtr = 0;
	that.eof = false;
	that.console = false;
}

WinFile& WinFile::operator=(WinFile && that) {

	((File*)this)->operator=(std::move(that));
	this->filePath = std::move(that.filePath);
	this->hFile = that.hFile;
	this->basePtr = that.basePtr;
	this->eof = that.eof;

	that.hFile = NULL;
	that.basePtr = 0;
	that.eof = false;
	that.console = false;

	return *this;
}

WinFile::~WinFile() {
	Close();
}

long long WinFile::GetCurrentPosition() {
	if (!*this) return 0;

	LARGE_INTEGER size, ptr;
	size.QuadPart = 0;

	if (this->console) MY_ERROR("GetCurrentPosition: not implemented for console");

	SetFilePointerEx(hFile, size, &ptr, FILE_CURRENT);

	return ptr.QuadPart - basePtr;
}

WinFile::operator bool() {
	if (this->console) {
		//return WaitForSingleObject(this->FileReadHandle(), 0) == WAIT_OBJECT_0;
		return true;
	} else {
		return hFile != 0;
	}
}

bool WinFile::EofCore() {
	return eof;
}

void WinFile::CloseCore() {
	if (hFile) CloseHandle(hFile);
	hFile = nullptr;
	eof = false;
}

void WinFile::OpenCore(const wchar_t * fp, const char * om) {

	if (this->console) MY_ERROR("WinFile::Open not implemented for console");

	DWORD access, creation;
	if (IsWriteMode(om)) {
		access = GENERIC_WRITE;
		if (IsReadMode(om)) access |= GENERIC_READ;
		if (IsAppendMode(om) || IsReadMode(om)) creation = OPEN_ALWAYS;
		else creation = CREATE_ALWAYS;
	}
	else {
		access = GENERIC_READ;
		creation = OPEN_EXISTING;
	}

	hFile = CreateFileW(
					fp, 
					access, 
					FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, 
					NULL, 
					creation, 
					FILE_ATTRIBUTE_NORMAL, 
					NULL);

	if (hFile == INVALID_HANDLE_VALUE) {
		DbgStr(L"Error opening file \"%s\" :\n%s\n", fp, GetWin32ErrorDescriptionLocal(GetLastError()));
		hFile = nullptr;
	}

	eof = false;

	LARGE_INTEGER size, ptr;

	if (hFile) {
		GetFileSizeEx(hFile, &size);
		File::fileSize = size.QuadPart;

		size.QuadPart = 0;
		SetFilePointerEx(hFile, size, &ptr, FILE_BEGIN);
		basePtr = ptr.QuadPart;
	}
}

void WinFile::OpenCore(const char * fp, const char * om) {
	// dirty hack: ignore fp
	this->OpenCore(this->filePath, om);
}

WinFile& WinFile::Open(const wchar_t * fp, const char * om) {

	if (fp) {
		this->filePath = fp;
		File::filePath = fp;
	}
	File::Open(nullptr, om);
	return *this;
}

WinFile& WinFile::Open(const char * fp, const char * om) {

	if (fp) {
		this->filePath = fp;
		File::filePath = fp;
	}
	File::Open(nullptr, om);

	return *this;
}

WinFile& WinFile::OpenConsole() {

	this->~WinFile();

	new (this) WinFile;

	this->console = true;

	return *this;
}

WinFile& WinFile::Open(const wchar_t * fp) {

	this->filePath = fp;
	File::filePath = fp;
	File::Open(aVect<char>(fp));

	return *this;
}

WinFile& WinFile::Open(const char * fp) {

	this->filePath = fp;
	File::filePath = fp;
	File::Open(fp);

	return *this;
}

WinFile& WinFile::Open() {

	File::Open();
	return *this;
}


void WinFile::FlushCore() {
	FlushFileBuffers(hFile);
}

HANDLE WinFile::FileReadHandle() {
	if (this->console) {
		return GetStdHandle(STD_INPUT_HANDLE);
	} else {
		return this->hFile;
	}
}

HANDLE WinFile::FileWriteHandle() {
	if (this->console) {
		return GetStdHandle(STD_OUTPUT_HANDLE);
	} else {
		return this->hFile;
	}
}

size_t WinFile::ReadCore(void* ptr, size_t s, size_t n) {
	DWORD nBytesRead;
	BOOL retVal = ReadFile(this->FileReadHandle(), ptr, (DWORD)(n*s), &nBytesRead, NULL);
	if (retVal) {
		if (nBytesRead == 0 && n*s > 0) 
			eof = true;
		else eof = false;
	}
	else {
		if (GetLastError() == ERROR_HANDLE_EOF) {
			eof = true;
		}
	}
	
	return nBytesRead;
}

size_t WinFile::WriteCore(void* ptr, size_t s, size_t n) {
	DWORD nBytesWritten;
	BOOL retVal = WriteFile(this->FileWriteHandle(), ptr, (DWORD)(n*s), &nBytesWritten, NULL);
	
	return nBytesWritten;
}



//=================================
//============= MMFile ============
//=================================

MMFile::MMFile() : WinFile(), hFileMapping(false), pFileStart(nullptr), pFilePos(nullptr), pFileEnd(nullptr) {}
MMFile::MMFile(const char * fn) : WinFile(fn), hFileMapping(false), pFileStart(nullptr), pFilePos(nullptr), pFileEnd(nullptr) {}
MMFile::MMFile(const char * fn, const char * om) : WinFile(fn, om), hFileMapping(false), pFileStart(nullptr), pFilePos(nullptr), pFileEnd(nullptr) {}
MMFile::MMFile(const wchar_t * fn) : WinFile(fn), hFileMapping(false), pFileStart(nullptr), pFilePos(nullptr), pFileEnd(nullptr) {}
MMFile::MMFile(const wchar_t * fn, const char * om) : WinFile(fn, om), hFileMapping(false), pFileStart(nullptr), pFilePos(nullptr), pFileEnd(nullptr) {}

MMFile::~MMFile() {
	Close();
}

long long MMFile::GetCurrentPosition() {
	
	if (!*this) return 0;
	return pFilePos - pFileStart;
}

void MMFile::CloseCore() {
	if (pFileStart) UnmapViewOfFile(pFileStart);
	if (hFileMapping) CloseHandle(hFileMapping);
	pFileStart = pFilePos = pFileEnd = nullptr;
	hFileMapping = nullptr;
	WinFile::CloseCore();
}

void MMFile::OpenCore(char * fp, char * om) {
	WinFile::OpenCore(fp, om);

	DWORD fileAccess, mapViewAccess;
	if (IsWriteMode(openMode)) fileAccess = PAGE_READWRITE, mapViewAccess = FILE_MAP_WRITE;
	else if (IsReadMode(openMode)) fileAccess = PAGE_READONLY, mapViewAccess = FILE_MAP_READ;
	else MY_ERROR("invalid open mode");

	hFileMapping = CreateFileMapping(
		WinFile::hFile, 
		NULL,
		fileAccess,
		0,
		0,
		nullptr);

	if (hFileMapping == nullptr) MY_ERROR("CreateFileMapping failed");

	pFileStart = (char*)MapViewOfFile(
		hFileMapping,
		mapViewAccess,
		0,
		0,
		0);

	if (pFileStart == nullptr) MY_ERROR("MapViewOfFile failed");
	pFilePos = pFileStart;

	pFileEnd = pFileStart + File::fileSize;
}

void MMFile::FlushCore() {

	FlushViewOfFile(pFileStart, 0);
}

size_t MMFile::ReadCore(void* ptr, size_t s, size_t n) {

	SIZE_T_OVERFLOW_CHECK_MULT_(s, n);
	
	size_t nTot = s*n;

	if ((size_t)pFileStart > (size_t)pFileEnd) MY_ERROR("pFileStart > pFileEnd");

	if (nTot > (size_t)pFileEnd - (size_t)pFilePos) {
		nTot = ((size_t)pFileEnd - (size_t)pFilePos);
		WinFile::eof = true;
	}

	__try {
		memcpy(ptr, pFilePos, nTot);
	}
	__except(GetExceptionCode()==EXCEPTION_IN_PAGE_ERROR ?
	EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
		MY_ERROR("EXCEPTION_IN_PAGE_ERROR");
	}

	pFilePos += nTot;

	return nTot;
}

size_t MMFile::WriteCore(void* ptr, size_t s, size_t n) {

	SIZE_T_OVERFLOW_CHECK_MULT_(s, n);

	size_t nTot = s*n;

	if ((size_t)pFileStart > (size_t)pFileEnd) MY_ERROR("pFileStart > pFileEnd");

	if (nTot >= (size_t)pFileEnd - (size_t)pFilePos) {
		MY_ERROR("Attempt to write past the end of the file");
	}

	__try {
		memcpy(pFilePos, ptr, nTot);
	}
	__except(GetExceptionCode()==EXCEPTION_IN_PAGE_ERROR ?
	EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
		MY_ERROR("EXCEPTION_IN_PAGE_ERROR");
	}

	pFilePos += nTot;

	return nTot;
}

//=================================
//=================================
//=================================


int MyMessageBox(char* formatString, ...) {

	va_list args;
	va_start(args, formatString);
	int num = _vsnprintf(NULL, 0, formatString, args);
	aVect<char> tmp(num + 1);
	_vsnprintf(&tmp[0], tmp.Count() + 1, formatString, args);
	va_end(args);
	return MessageBoxA((HWND)NULL, &tmp[0], "", MB_SERVICE_NOTIFICATION);
}


int MyMessageBox(wchar_t* formatString, ...) {

	va_list args;
	va_start(args, formatString);
	int num = _vsnwprintf(NULL, 0, formatString, args);
	aVect<wchar_t> tmp(num + 1);
	_vsnwprintf(&tmp[0], tmp.Count() + 1, formatString, args);
	va_end(args);
	return MessageBoxW((HWND)NULL, &tmp[0], L"", MB_SERVICE_NOTIFICATION);
}

int MyMessageBox(DWORD style, char* formatString, ...) {

	va_list args;
	va_start(args, formatString);
	int num = _vsnprintf(NULL, 0, formatString, args);
	aVect<char> tmp(num + 1);
	_vsnprintf(&tmp[0], tmp.Count() + 1, formatString, args);
	va_end(args);
	return MessageBoxA((HWND)NULL, &tmp[0], "", style | MB_SERVICE_NOTIFICATION);
}

int MyMessageBox(HWND hWnd, DWORD style, char* formatString, ...) {

	va_list args;
	va_start(args, formatString);
	int num = _vsnprintf(NULL, 0, formatString, args);
	aVect<char> tmp(num + 1);
	_vsnprintf(&tmp[0], tmp.Count() + 1, formatString, args);
	va_end(args);
	return MessageBoxA((HWND)hWnd, &tmp[0], "", hWnd ? style : style | MB_SERVICE_NOTIFICATION);
}

void DbgStr(char* formatString, ...) {

	va_list args;
	va_start(args, formatString);
	int num = _vsnprintf(NULL, 0, formatString, args);
	aVect<char> tmp(num + 1);
	_vsnprintf(&tmp[0], tmp.Count() + 1, formatString, args);
	va_end(args);
	OutputDebugStringA(&tmp[0]);
}


void DbgStr(wchar_t* formatString, ...) {

	va_list args;
	va_start(args, formatString);
	int num = _vsnwprintf(NULL, 0, formatString, args);
	aVect<wchar_t> tmp(num + 1);
	_vsnwprintf(&tmp[0], tmp.Count() + 1, formatString, args);
	va_end(args);
	OutputDebugStringW(&tmp[0]);
}

WinCriticalSection g_SplitPathW_cs;

//not thread safe
template <class Char>
void SplitPath_Core(const Char * src, Char ** pDrive, Char ** pPath, Char ** pFName, Char ** pExt) {

	static WinCriticalSection cs;
	AutoCriticalSection acs(cs, false);

	MY_ASSERT(acs.TryEnter());	//assert no other thread is running this function

	if (!src) {
		if (pDrive) *pDrive = nullptr;
		if (pPath) *pPath = nullptr;
		if (pFName) *pFName = nullptr;
		if (pExt) *pExt = nullptr;
		return;
	}

	static aVect<Char> buf;

	buf = src;

	auto ext = (Char*)buf + Strlen(buf.GetDataPtr()) - 1;

	while (true) {
		if (ext <= buf || *ext == '\\' || *ext == '/') {
			ext = NULL;
			break;
		}
		--ext;
		if (*ext == '.') {
			if (pExt) *ext = 0;
			ext++;
			break;
		}
	}
	auto fName = (Char*)buf + Strlen(buf.GetDataPtr()) - 1;
	while (true) {
		if (fName <= buf) {
			fName = NULL;
			break;
		}
		--fName;
		if (*fName == '\\' || *fName == '/') {
			*fName = 0;
			fName++;
			break;
		}
	}

	auto path = (Char*)buf + Strlen(buf.GetDataPtr()) - 1;
	while (true) {
		if (path <= buf) {
			path = NULL;
			break;
		}
		--path;

		bool semicolon = path[0] == ':';
		bool UNC = path[0] == '\\' && path[1] == '\\';

		if (semicolon) {
			path++;
			if (semicolon && *path == '\\') {
				if (pDrive) *path = 0;
				path++;
				break;
			}
			else {
				path = NULL;
			}
		}
		else if (UNC) {
			path += 2;
			for (;;) {
				if (path[0] == 0) {
					path = NULL;
					break;
				}
				if (path[0] == '\\') {
					if (pDrive) path[0] = 0;
					path++;
					goto dbl_break;
				}
				path++;
			}
		}
	}

dbl_break:

	auto drive = (Char*)buf;
	bool flag_semicolon = false;
	bool flag_UNC_1 = false;
	bool flag_UNC_2 = false;
	bool flag_UNC_3 = false;

	while (true) {
		if (drive > (Char*)buf + Strlen(buf.GetDataPtr())) {
			drive = NULL;
			break;
		}
		if (flag_semicolon || flag_UNC_3) {
			if (pDrive) {

				if (flag_semicolon) {
					*drive = 0;
					drive = buf;
				}
				else {
					drive[-1] = 0;
					drive = buf + (ptrdiff_t)2;
				}
			}
			else path = buf;
			break;
		}
		else if (*drive == ':') flag_semicolon = true;
		else if (flag_UNC_2 && *drive == '\\') flag_UNC_3 = true;
		else if (flag_UNC_1) flag_UNC_2 = true;
		else if (drive[0] == '\\' && drive[1] == '\\') flag_UNC_1 = true;

		drive++;
	}

	if (pDrive) *pDrive = drive;
	if (pPath)  *pPath = path;
	if (pFName) *pFName = fName;
	if (pExt)   *pExt = ext;
}

void SplitPath(const char * src, char ** pDrive, char ** pPath, char ** pFName, char ** pExt) {

	return SplitPath_Core(src, pDrive, pPath, pFName, pExt);
}

void SplitPathW(const wchar_t * src, wchar_t ** pDrive, wchar_t ** pPath, wchar_t ** pFName, wchar_t ** pExt) {

	return SplitPath_Core(src, pDrive, pPath, pFName, pExt);
}
 
mVect<WIN32_FIND_DATAA> GetFolderContents(char * path, char * searchStr = "*") {

	WIN32_FIND_DATAA f;
	mVect<WIN32_FIND_DATAA> contents;
	aVect<char> buffer;
	HANDLE h;

	if ((h = FindFirstFileA(buffer.sprintf("%s\\%s", path, searchStr), &f)) == INVALID_HANDLE_VALUE) {
		MY_ERROR(buffer.sprintf("\"%s\" introuvable", path));
	}

	do {
		if (f.cFileName && strlen(f.cFileName) && f.cFileName[0] != '.') contents.Push(f);
	} while(FindNextFileA(h, &f));

	FindClose(h);
	
	return contents;
}

mVect< mVect<wchar_t> > WalkBackPathForFile(
							const aVect<wchar_t>& path, 
							const aVect<wchar_t> & fileName1,
							const aVect<wchar_t> & fileName2,
							aVect<WinFile>* pFiles,
							bool stopAtFirst) {

	aVect< aVect<wchar_t> > fileNames;
	fileNames.Push((aVect<wchar_t>)fileName1).Push((aVect<wchar_t>)fileName2);

	return WalkBackPathForFile(path, fileNames, pFiles, stopAtFirst);
}

mVect< mVect<wchar_t> > WalkBackPathForFile(
							const aVect<wchar_t>& path, 
							const aVect< aVect<wchar_t> > & fileNames,
							aVect<WinFile>* pFiles,
							bool stopAtFirst) {

	mVect< mVect<wchar_t> > fullPathArray;
	aVect<wchar_t> fullPath;
	aVect<wchar_t> tmp(path);

	int pathLvl = 1;
    aVect_static_for(path, i) if (path[i]=='\\') pathLvl++;

	if (pFiles && *pFiles) pFiles->Redim(0);

	while(true) {
		aVect_static_for(fileNames, i) {
			fullPath.sprintf(L"%s\\%s", (wchar_t*)tmp, (wchar_t*)fileNames[i]);
			WinFile f(fullPath, "rb");
			if (f.Open()) {
				fullPathArray.Push(fullPath);
				if (pFiles) pFiles->Push(std::move(f));
				if (stopAtFirst) break;
			}
		}
        if (--pathLvl<0) break;

		aVect_for_inv(tmp, i) {
			if (tmp[i] == '\\') {
				tmp[i] = 0;
				break;
			}
		}
    };

	return std::move(fullPathArray);
}

mVect< mVect<wchar_t> > WalkBackPathForFile(
							const aVect<wchar_t>& path, 
							const aVect<wchar_t> & fileName,
							aVect<WinFile>* pFiles,
							bool stopAtFirst) {

	aVect< aVect<wchar_t> > fileNames;
	fileNames.Push((aVect<wchar_t>)fileName);

	return WalkBackPathForFile(path, fileNames, pFiles, stopAtFirst);
}

mVect< mVect<wchar_t> > WalkForwardPathForFile(
							const aVect<wchar_t>& path, 
							const aVect<wchar_t>& fileName, 
							mVect< mVect<wchar_t> > * folderEnds,
							bool stopAtFirst,
							bool includeAll) {
	
	aVect< aVect<wchar_t> > fileNames;
	fileNames.Push(fileName);

	return WalkForwardPathForFile(path, fileNames, folderEnds, stopAtFirst, includeAll);
}

mVect< mVect<wchar_t> > WalkForwardPathForFile(
							const aVect<wchar_t>& path, 
							const aVect<wchar_t>& fileName1, 
							const aVect<wchar_t>& fileName2, 
							mVect< mVect<wchar_t> > * folderEnds,
							bool stopAtFirst,
							bool includeAll) {
	
	aVect< aVect<wchar_t> > fileNames;
	fileNames.Push(fileName1).Push(fileName2);

	return WalkForwardPathForFile(path, fileNames, folderEnds, stopAtFirst, includeAll);
}

mVect< mVect<wchar_t> > WalkForwardPathForFile(
							const aVect<wchar_t>& path, 
							const aVect< aVect<wchar_t> > & fileNames, 
							mVect< mVect<wchar_t> > * folderEnds,
							bool stopAtFirst,
							bool includeAll) {
	
	//TODO : complete rewrite

	WIN32_FIND_DATAW f;
	aVect<wchar_t> buffer;
	aVect<HANDLE> hStack;
	aVect<wchar_t> curFolder(path);
	aVect<wchar_t> curLastParamFile;
	mVect< mVect<wchar_t> > paramFiles;
	aVect<bool> foundSubDir;
	aVect<bool> alreadySearched;
	aVect<size_t> paramFileIndex;
	aVect< aVect< aVect<wchar_t> > > folderNameStack;

	while(true) {
		if (hStack && hStack.Last() == 0) {
			hStack.Pop();
			foundSubDir.Pop();
			paramFileIndex.Pop();
			folderNameStack.Pop();
			alreadySearched.Pop();
			if (!hStack) break;
			SplitPathW(curFolder, nullptr, &curFolder, nullptr, nullptr);
		} else {
			HANDLE result = FindFirstFileW(buffer.sprintf(L"%s\\*", (wchar_t*)curFolder), &f);
			if (result == INVALID_HANDLE_VALUE) {
				if (GetLastError() == ERROR_NO_MORE_FILES) result = 0;
				else SAFE_CALL(0);
			}
			hStack.Push(result);
			foundSubDir.Push(false);
			paramFileIndex.Push(fileNames.Count());
			folderNameStack.Redim(folderNameStack.Count() + 1);
			alreadySearched.Push(false);
		}
		if (INVALID_HANDLE_VALUE == hStack.Last()) {
			MY_ERROR(aVect<char>("\"%S\" introuvable", (wchar_t*)curFolder));
		}
		bool isDir = false;
		BOOL result;

		if (!alreadySearched.Last()) {
			do {
				if (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && f.cFileName && wcslen(f.cFileName) && f.cFileName[0] != '.') {
					folderNameStack.Last().Push(f.cFileName);
					foundSubDir.Last() = true;
					isDir = true;
				}
				aVect_static_for(fileNames, i) {
					if (strcmp_caseInsensitive(fileNames[i], f.cFileName) == 0) {
						if (paramFileIndex.Last() > i) {
							curLastParamFile.sprintf(L"%s\\%s", (wchar_t*)curFolder, f.cFileName);
							paramFileIndex.Last() = i;
						}
						break;
					}
				}
				result = FindNextFileW(hStack.Last(), &f);
				if (!result && GetLastError() != ERROR_NO_MORE_FILES) SAFE_CALL(0);
			} while (result);
			alreadySearched.Last() = true;
		}

		if (folderNameStack.Last()) {
			swprintf(f.cFileName, sizeof f.cFileName, L"%s", (wchar_t*)folderNameStack.Last().Last());
			folderNameStack.Last().Pop();
			isDir = true;
		}

		if (!foundSubDir.Last() || includeAll) {
			
			if (curLastParamFile) {
				if (folderEnds) folderEnds->Push(curFolder);
				paramFiles.Push(curLastParamFile);
			}

			curLastParamFile.Redim(0);

			if (!foundSubDir.Last()) {
				SAFE_CALL(FindClose(hStack.Last()));
				hStack.Last() = 0;
				if (hStack.Count() == 1) break;
			}
			
			if (stopAtFirst) break;
		}
		else if (isDir) {
			curFolder.Copy(buffer.sprintf(L"%s\\%s", (wchar_t*)curFolder, f.cFileName));
		}
		else {
			SAFE_CALL(FindClose(hStack.Last()));
			hStack.Last() = 0;
		}
	}
	
	aVect_static_for(hStack, i) {
		if (hStack[i]) SAFE_CALL(FindClose(hStack[i]));
	}

	return paramFiles;
}



HINSTANCE g_hInst;


WNDPROC g_wpOrigStaticProc;


// Subclass procedure 
LRESULT APIENTRY EditSubclassProc(
	HWND hWnd,
	UINT message,
	WPARAM wParam,
	LPARAM lParam) {

	switch (message) {

		case WM_DESTROY: {
			if (!g_wpOrigStaticProc) MY_ERROR("ici");
			SetWindowLongPtrA(hWnd, GWLP_WNDPROC, (LONG_PTR)g_wpOrigStaticProc);
			break;
		}
		case WM_NOTIFY:
		case WM_COMMAND: {

			SendMessage(GetParent(hWnd), message, wParam, lParam);

			//int code = HIWORD(wParam);

			//switch (code) {

			//	case EN_KILLFOCUS:
			//	case BN_CLICKED:
			//		/*char parentWindowClassName[500];
			//		char parentWindowTitle[500];
			//		char windowClassName[500];
			//		char windowTitle[500];
			//		GetClassName(hWnd, windowClassName, sizeof(windowClassName));
			//		GetWindowText(hWnd, windowTitle, sizeof(windowTitle));
			//		GetClassName(GetParent(hWnd), parentWindowClassName, sizeof(parentWindowClassName));
			//		GetWindowText(GetParent(hWnd), parentWindowTitle, sizeof(parentWindowTitle));
			//		DbgStr("\n\nWindow 0x%p, class \"%s\", title \"%s\",\n"
			//		"Sending to \nWindow 0x%p, class \"%s\", title \"%s\",\n"
			//		"message %d, wParam = 0x%p, lParam = 0x%p\n",
			//		hWnd, windowClassName, windowTitle,
			//		GetParent(hWnd), parentWindowClassName,
			//		parentWindowTitle,
			//		message, wParam, lParam);*/

			//		SendMessage(GetParent(hWnd), message, wParam, lParam);

			//		break;
			//}

			//break;
		}
	}

	return CallWindowProc(
		g_wpOrigStaticProc,
		hWnd, message,
		wParam, lParam);
}

BOOL CALLBACK MyEnumChildWindow_EnumChildProc(HWND hwnd, LPARAM lParam) {
	aVect<HWND> * hWndArray = (aVect<HWND>*)lParam;
	hWndArray->Push(hwnd);
	return true;
}

aVect<HWND> MyEnumChildWindow(HWND hWnd) {

	aVect<HWND> hWndArray;

	hWndArray.Push(hWnd);

	EnumChildWindows(hWnd, MyEnumChildWindow_EnumChildProc, (LPARAM)&hWndArray);

	return hWndArray;
}

HWND MyGetDlgItem(HWND hWnd, int id) {

	HWND retVal = NULL;

	if (!(retVal = GetDlgItem(hWnd, id))) {

		aVect<HWND> hWndArray;

		hWndArray = MyEnumChildWindow(hWnd);

		aVect_static_for(hWndArray, i) {
			if ((retVal = GetDlgItem(hWndArray[i], id)))
				break;
		}
	}

	return retVal;
}


HWND PutThere(HWND hParent, const wchar_t * className, const wchar_t * str, int width, int height,
	int decX, int decY, INT_PTR index, int myFlag, DWORD additionalStyle, DWORD additionalStyleEx, void * ptr) {

	//static HFONT hFont = NULL;
	HFONT hFont = NULL;

	if (!hFont) {
		hFont = CreateFontW(-11, 0, 0, 0, 0, 0, 0, 0,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			DEFAULT_QUALITY, DEFAULT_PITCH, L"Tahoma");
	}

	DWORD exStyle, style;
	const wchar_t * realClasseName = nullptr;
	const char * realClasseNameA = nullptr;

	int x, y;
	HWND hWnd = hParent;

	if (myFlag == PUTTHERE_INTO) x = decX, y = decY;
	else {
		RECT lastRect = GetRect(hParent);
		if (myFlag == PUTTHERE_RIGHT) x = lastRect.right + decX, y = lastRect.top + decY;
		else if (myFlag == PUTTHERE_BOTTOM) x = lastRect.left + decX, y = lastRect.bottom + decY;
		else MY_ERROR("Bad flag");
		hWnd = GetParent(hParent);
	}

	if (_wcsicmp(className, L"ListBox") == 0)
		exStyle = NULL/*WS_EX_CONTROLPARENT*/ /*| WS_EX_CLIENTEDGE*/,
		style = WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | WS_TABSTOP | LBS_SORT,
		realClasseName = className;
	else if (_wcsicmp(className, L"Button") == 0)
		exStyle = NULL/*WS_EX_CONTROLPARENT*/ /* | WS_EX_CLIENTEDGE*/,
		style = WS_CHILD | WS_VISIBLE | WS_TABSTOP,
		realClasseName = className;
	else if (_wcsicmp(className, L"SysTreeView32") == 0)
		exStyle = NULL/*WS_EX_CONTROLPARENT*/ /* | WS_EX_CLIENTEDGE*/,
		style = WS_CHILD | WS_VISIBLE | WS_TABSTOP,
		realClasseName = className;
	else if (_wcsicmp(className, TRACKBAR_CLASSW) == 0)
		exStyle = NULL/*WS_EX_CONTROLPARENT*/ /* | WS_EX_CLIENTEDGE*/,
		style = WS_CHILD | WS_VISIBLE | WS_TABSTOP,
		realClasseName = className;
	else if (_wcsicmp(className, L"ToggleButton") == 0)
		exStyle = NULL/*WS_EX_CONTROLPARENT*/,
		style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHLIKE | BS_AUTOCHECKBOX,// BS_DEFPUSHBUTTON,
		realClasseName = L"Button";
	else if (_wcsicmp(className, L"CheckBox") == 0)
		exStyle = NULL/*WS_EX_CONTROLPARENT*/,
		style = WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
		realClasseName = L"Button";
	else if (_wcsicmp(className, L"MLEdit") == 0)
		exStyle = NULL /*WS_EX_CONTROLPARENT*/ /*| WS_EX_CLIENTEDGE*/,
		style = WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_TABSTOP,
		realClasseName = L"EDIT";
	else if (_wcsicmp(className, L"Edit") == 0)
		exStyle = NULL /*WS_EX_CONTROLPARENT*/ /*| WS_EX_CLIENTEDGE*/,
		style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
		realClasseName = className;
	else if (_wcsicmp(className, L"Label") == 0)
		exStyle = NULL/*WS_EX_CONTROLPARENT*/,
		style = WS_CHILD | SS_LEFT | WS_VISIBLE | SS_NOTIFY,// | SS_EDITCONTROL,
		realClasseName = L"STATIC";
	else if (_wcsicmp(className, L"ComboBox") == 0)
		exStyle = NULL/*WS_EX_CONTROLPARENT*/,
		style = WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,// | SS_SUNKEN,// | SS_EDITCONTROL,
		realClasseName = L"ComboBox";
	else if (_wcsicmp(className, L"InnerFrame") == 0)
		//hWnd = hParent,
		exStyle = NULL/*WS_EX_CONTROLPARENT*/,
		style = WS_CHILD | /*SS_SUNKEN*//*SS_BLACKFRAME*/SS_ETCHEDFRAME | WS_VISIBLE,// | SS_EDITCONTROL,
		realClasseName = L"STATIC";
	else if (_wcsicmp(className, L"OuterFrame") == 0)
		//hWnd = hParent,
		exStyle = NULL/*WS_EX_CONTROLPARENT*/,
		style = WS_CHILD | WS_VISIBLE,// | SS_SUNKEN,// | SS_EDITCONTROL,
		realClasseName = L"STATIC";
	else if (_wcsicmp(className, L"Frame") == 0) {

		HWND hOuterFrame = PutThere(hParent, L"OuterFrame", NULL, width, height, decX, decY, 0, myFlag);
		HWND hLabel = PutThere(hOuterFrame, L"Label", str, 20, 20, 12, 0, 0, PUTTHERE_INTO);
		HFONT hFont = GetWindowFont(hLabel);
		HDC hdc = SAFE_CALL(GetDC(hLabel));
		HFONT oldFont;
		GDI_SAFE_CALL(oldFont = SelectFont(hdc, hFont));

		SIZE sz;

		GetTextExtentPoint32W(hdc, str, (int)wcslen(str), &sz);
		GDI_SAFE_CALL(SelectFont(hdc, oldFont));
		ReleaseDC(hLabel, hdc);
		DestroyWindow(hLabel);

		HWND hInnerFrame = PutThere(
			hOuterFrame,
			L"InnerFrame", NULL,
			width - 6,
			height - sz.cy / 2,
			0, sz.cy / 2,
			0, PUTTHERE_INTO);

		hLabel = PutThere(hOuterFrame, L"Label", str, sz.cx, sz.cy, 12, 0, 0, PUTTHERE_INTO);

		return hInnerFrame;
	}
	else if (_wcsicmp(className, L"ListView") == 0) 
		//hWnd = hParent,
		exStyle = NULL/*WS_EX_CONTROLPARENT*/,
		style = WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_EDITLABELS,
		realClasseName = WC_LISTVIEWW,
		realClasseNameA = WC_LISTVIEWA;
	else MY_ERROR("Window classe inconnue");//return NULL;

	style |= additionalStyle;
	exStyle |= additionalStyleEx;


	HWND hRet = /*wide ? */CreateWindowExW(
		/*dwExStyle*/     exStyle,
		/*lpClassName*/   realClasseName,
		/*lpWindowsName*/ str,
		/*dwStyle*/       style,
		/*Pos*/           x, y, width, height,
		/*hWndParent*/	  hWnd,
		/*hMenu*/         (HMENU)index,
		/*hInstance*/     g_hInst,
		/*lpParam*/       ptr);// :
	//CreateWindowExA(
	//	/*dwExStyle*/     exStyle,
	//	/*lpClassName*/   realClasseNameA,
	//	/*lpWindowsName*/ xFormat("%S", str),
	//	/*dwStyle*/       style,
	//	/*Pos*/           x, y, width, height,
	//	/*hWndParent*/	  hWnd,
	//	/*hMenu*/         (HMENU)index,
	//	/*hInstance*/     g_hInst,
	//	/*lpParam*/       ptr);

	SetWindowFont(hRet, hFont, 1);

	if (_wcsicmp(className, L"InnerFrame") == 0
		|| _wcsicmp(className, L"OuterFrame") == 0) {
		if (!g_wpOrigStaticProc) {
			g_wpOrigStaticProc = (WNDPROC)SetWindowLongPtrW(hRet, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
		}
		else {
			if (g_wpOrigStaticProc != (WNDPROC)SetWindowLongPtrW(hRet, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc)) MY_ASSERT(false);
		}
	}
	else if (_wcsicmp(className, L"ComboBox") == 0)  {
		SetWindowTextW(hRet, str);
	}

	return hRet;
}

HWND PutBottom(HWND hParent, const wchar_t * className, const wchar_t * str, int width, int height, int marge, INT_PTR index, DWORD additionalStyle, DWORD additionalStyleEx, void * ptr) {
	return PutThere(hParent, className, str, width, height, 0, marge, index, PUTTHERE_BOTTOM, additionalStyle, additionalStyleEx, ptr);
}

HWND PutBottomEx(HWND hParent, const wchar_t * className, const wchar_t * str, int width, int height, int margeX, int margeY, INT_PTR index, DWORD additionalStyle, DWORD additionalStyleEx, void * ptr) {
	return PutThere(hParent, className, str, width, height, margeX, margeY, index, PUTTHERE_BOTTOM, additionalStyle, additionalStyleEx, ptr);
}


HWND PutRight(HWND hParent, const wchar_t * className, const wchar_t * str, int width, int height, int marge, INT_PTR index, DWORD additionalStyle, DWORD additionalStyleEx, void * ptr) {
	return PutThere(hParent, className, str, width, height, marge, 0, index, PUTTHERE_RIGHT, additionalStyle, additionalStyleEx, ptr);
}

HWND PutRightEx(HWND hParent, const wchar_t * className, const wchar_t * str, int width, int height, int margeX, int margeY, INT_PTR index, DWORD additionalStyle, DWORD additionalStyleEx, void * ptr) {
	return PutThere(hParent, className, str, width, height, margeX, margeY, index, PUTTHERE_RIGHT, additionalStyle, additionalStyleEx, ptr);
}

HWND PutTopLeft(HWND hParent, const wchar_t * className, const wchar_t * str, int width, int height, int margeX, int margeY, INT_PTR index, DWORD additionalStyle, DWORD additionalStyleEx, void * ptr) {
	return PutThere(hParent, className, str, width, height, margeX, margeY, index, PUTTHERE_INTO, additionalStyle, additionalStyleEx, ptr);
}

RECT GetRect(HWND hListePlats){
	RECT thisRect;
	if (hListePlats) {
		WINDOWPLACEMENT wndPos;
		wndPos.length = sizeof(wndPos);
		GetWindowPlacement(hListePlats, &wndPos);
		thisRect = wndPos.rcNormalPosition;
	}
	else memset(&thisRect, 0, sizeof(thisRect));

	return thisRect;
}

HWND CreateToolTipWindow(int additionalStyles) {

	//INITCOMMONCONTROLSEX ic;
	//ic.dwSize = sizeof(INITCOMMONCONTROLSEX);
	//ic.dwICC = ICC_TAB_CLASSES;
	//if (!InitCommonControlsEx(&ic)) MyMessageBox("InitCommonControlsEx Failure");

	HWND toolTipHwnd = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS,
		NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP | additionalStyles,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, GetCurrentModuleHandle(), NULL);

	WIN32_SAFE_CALL(SetWindowPos(toolTipHwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE));

	SendMessage(toolTipHwnd, TTM_SETMAXTIPWIDTH, 0, 500);

	return toolTipHwnd;
}

void AddToolToToolTip(HWND toolTipHwnd, HWND ctrlHwnd, char * toolTipText) {

	if (!toolTipText || toolTipText[0] == 0) return;

	TOOLINFOA ti;
	ti.cbSize = sizeof ti;
	ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	ti.uId = (UINT_PTR)ctrlHwnd;
	ti.lpszText = toolTipText;
	ti.hwnd = GetParent(ctrlHwnd);

	SendMessage(toolTipHwnd, TTM_ADDTOOL, 0, (LPARAM)&ti);

}

void MyPrintWindow(HWND h) {
	PRINTDLG pd;

	// Initialize PRINTDLG
	ZeroMemory(&pd, sizeof(pd));
	pd.lStructSize = sizeof(pd);
	pd.hwndOwner = h;
	pd.hDevMode = NULL;     // Don't forget to free or store hDevMode.
	pd.hDevNames = NULL;     // Don't forget to free or store hDevNames.
	pd.Flags = PD_USEDEVMODECOPIESANDCOLLATE | PD_RETURNDC;
	pd.nCopies = 1;
	pd.nFromPage = 0xFFFF;
	pd.nToPage = 0xFFFF;
	pd.nMinPage = 1;
	pd.nMaxPage = 0xFFFF;

	if (PrintDlg(&pd) == TRUE) {

		DOCINFOA docInfo;
		ZeroMemory(&docInfo, sizeof(docInfo));
		docInfo.cbSize = sizeof(docInfo);
		docInfo.lpszDocName = "DocName";

		if (true) {

			if (StartDocA(pd.hDC, &docInfo) <= 0)
				MessageBoxA(h, "Echec de l'impression.", "", NULL);

			if (StartPage(pd.hDC) <= 0)
				MessageBoxA(h, "Echec de l'impression.", "", NULL);

			char * str = "Titre :";
			TextOutA(pd.hDC, 10, 10, str, lstrlenA(str));

			RECT rc;
			GetClientRect(h, &rc);

			int x = rc.right - rc.left;
			int y = rc.bottom - rc.top;

			int X, Y;

			X = GetDeviceCaps(pd.hDC, HORZRES);
			Y = GetDeviceCaps(pd.hDC, VERTRES);

			float marge_x = 0.1f*X;
			float marge_y = 0.1f*Y;

			float ratio = Min((float)(X - 2 * marge_x) / x, (float)(Y - 2 * marge_y) / y);

			XFORM xForm;
			xForm.eM11 = ratio;
			xForm.eM12 = 0.0;
			xForm.eM21 = 0.0;
			xForm.eM22 = ratio;
			xForm.eDx = marge_x;
			xForm.eDy = marge_y;

			int swt_res = SetWorldTransform(pd.hDC, &xForm);

			SendMessage(h, WM_PAINT, (WPARAM)pd.hDC, 0);

			EndPage(pd.hDC);
			EndDoc(pd.hDC);

			//if (debugMode) {
			//	FILE * debugFile = MyOpenFile("debugInfos.txt","w");
			//	fprintf(debugFile,"Debug infos : print\n");
			//	fprintf(debugFile,"Printer : %s\n",*((char**)(pd.hDevMode)));
			//	fprintf(debugFile,"Client Rect : left %d top %d bottom %d right %d\n",rc.left,rc.top,rc.bottom,rc.right);
			//	fprintf(debugFile,"GetDeviceCaps( pd.hDC, HORZRES ) = %d\n",X);
			//	fprintf(debugFile,"GetDeviceCaps( pd.hDC, VERTRES ) = %d\n",Y);
			//	fprintf(debugFile,"marge_x = %d\n",marge_x);
			//	fprintf(debugFile,"marge_y = %d\n",marge_y);
			//	fprintf(debugFile,"ratio = min(%d, %d)\n",(X-2*marge_x)/x,(Y-2*marge_y)/y);
			//	fprintf(debugFile,"SetWorldTransform = %d\n",swt_res);

			//	fclose(debugFile);
			//}

			//Delete DC when done.
			DeleteDC(pd.hDC);

		}
		else {



			RECT rc;
			GetClientRect(h, &rc);


			// Get the text width
			HDC hDC = SAFE_CALL(GetDC(h));
			if (NULL == hDC)
				MY_ERROR("NULL == hDC");

			int cx = rc.right - rc.left;
			int cy = rc.bottom - rc.top;
			int cxStretch, cyStretch;


			cxStretch = GetDeviceCaps(pd.hDC, HORZRES);
			cyStretch = GetDeviceCaps(pd.hDC, VERTRES);

			HDC hMemDC = CreateCompatibleDC(hDC);
			HBITMAP hbitmap = CreateCompatibleBitmap(hDC, cx, cy);
			ReleaseDC(h, hDC);


			if (hbitmap) {
				SelectObject(hMemDC, hbitmap);
				//ShowScrollBar( h, SB_BOTH, FALSE );
				//SendMessage( h, WM_PAINT, (WPARAM)hMemDC, 0 );
				if (PrintWindow(h, hMemDC, NULL) == NULL)
					MessageBoxA(h, "Echec de l'impression.", "", NULL);


				// Init the DOCINFO and start the document.
				//InitDocStruct( &di, _T("TreePrinting"));
				StartDocA(pd.hDC, &docInfo);
				//int y = 0;

				double ratio = 3.0 / 4.0;
				int x = (int)(ratio * cxStretch);
				int y = (int)(ratio * cyStretch);
				int delta_x = (int)(cxStretch*(1 - ratio) / 2);
				int delta_y = (int)(cyStretch*(1 - ratio) / 2);

				StartPage(pd.hDC);
				SetStretchBltMode(pd.hDC, COLORONCOLOR);
				StretchBlt(pd.hDC, delta_x, delta_y, (int)(ratio*cxStretch),
					(int)(ratio*cyStretch), hMemDC, 0, 0, cx, cy,
					SRCCOPY);

				EndPage(pd.hDC);
				//y += ( nRowHeight * 42 );

				EndDoc(pd.hDC); //Indicating the print job is finished.
				DeleteObject(hbitmap); //No need to hold bitmap in memory anymore.
			}

			//Memory DC is no more needed.
			DeleteDC(hMemDC);
			//Delete Printer DC when done.
			DeleteDC(pd.hDC);
		}

	}
	else {
		int error = CommDlgExtendedError();
	}

	//if (pd.hDevMode) free(pd.hDevMode);
	//if (pd.hDevNames) free(pd.hDevNames);
}

HINSTANCE GetCurrentModuleHandle() {

	static const wchar_t s_empty[] = L"";

	auto flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;

	HINSTANCE hInstance;

	if (!GetModuleHandleExW(flags, s_empty, &hInstance)) {
		MY_ERROR("GetCurrentModule failed");
	}

	return hInstance;
}

WNDCLASSW MyRegisterClass(WNDPROC WinProc, const wchar_t * className) {

	HINSTANCE hInstance;

	//static bool once;

	WNDCLASSW wndclass = { 0 };
	hInstance = GetCurrentModuleHandle();

	//if (!once) {
	//	once = true;
	//	if (0 == GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, NULL, &hInstance)) {
	//		MessageBoxA(NULL, "GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_PIN) Failed!", "", MB_SERVICE_NOTIFICATION);
	//	}
	//}

	if (GetClassInfoW(GetCurrentModuleHandle(), className, &wndclass)) {
		MessageBoxA(NULL, "Class already exist", "", MB_SERVICE_NOTIFICATION);
	} else {

		HBRUSH hBrush = (HBRUSH)NULL_BRUSH;

		wndclass.cbClsExtra = 0;
		wndclass.cbWndExtra = 0;
		wndclass.hbrBackground = hBrush;//(HBRUSH) NULL_BRUSH;
		wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
		wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		wndclass.hInstance = hInstance;
		wndclass.lpfnWndProc = WinProc;
		wndclass.lpszClassName = className;
		wndclass.lpszMenuName = NULL;
		wndclass.style = CS_HREDRAW | CS_VREDRAW;

		SAFE_CALL(RegisterClassW(&wndclass));
	}

	return wndclass;
}


aVect<wchar_t> GetWin32ErrorDescriptionCore(DWORD systemErrorCode, WORD langID, bool appendErrorCode) {

	static unsigned recursionCount;
	//static char * outStr = NULL;
	wchar_t * outStr = NULL;
	//static char tmp[5000];
	aVect<wchar_t> tmp;// [5000];

	RecursionCounter rc(recursionCount);

	DWORD retVal = FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		systemErrorCode,
		langID,
		(LPWSTR)&outStr,
		0, NULL);

	if (retVal == 0) {
		DWORD lastError = GetLastError();
		if ((lastError == 15100 || lastError == 15105) && recursionCount < 2) {
			if (langID == MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)) {
				return GetWin32ErrorDescription(systemErrorCode, appendErrorCode);
			}
			else if (langID == MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)) {
				return GetWin32ErrorDescriptionLocal(systemErrorCode, appendErrorCode);
			}
		}
		else MY_ERROR(xFormat("FormatMessage failed (error code = %d) trying to get error %d description", lastError, systemErrorCode));
	}

	size_t count = Strlen(outStr) + 1;
	for (size_t i=0; i<count; ++i) {
		if (outStr[i] == '\n') {
			outStr[i] = 0;
			if (i > 0 && outStr[i-1] == '\r') {
				outStr[i-1] = 0;
			}
		}
	}

	//sprintf_s(tmp, appendErrorCode ? "%s (%d)" : "%s", outStr, systemErrorCode);
	tmp.sprintf(appendErrorCode ? L"%s (%d)" : L"%s", outStr, systemErrorCode);

	return tmp;
}

aVect<wchar_t> GetWin32ErrorDescription(DWORD systemErrorCode, bool appendErrorCode) {
	return GetWin32ErrorDescriptionCore(
		systemErrorCode, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
		appendErrorCode);
}

aVect<wchar_t> GetWin32ErrorDescriptionLocal(DWORD systemErrorCode, bool appendErrorCode) {
	return GetWin32ErrorDescriptionCore(
		systemErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		appendErrorCode);
}


HWND MyCreateWindow(
	const wchar_t * className,
	const wchar_t * strTitle,
	int x, int y,
	int nX, int nY,
	DWORD style,
	HWND parent,
	HMENU menu,
	void * data,
	int SW_FLAG) {

	static bool initialized = 0;
	HWND hWnd;

	HINSTANCE hInstance = GetCurrentModuleHandle();

	if (parent) style |= WS_CHILD;

	hWnd = CreateWindowW(
		className,
		strTitle,
		style,
		x, y,
		nX, nY,
		parent,
		menu,
		hInstance,
		(LPVOID)data);

	if (!hWnd) {
		MyMessageBox(L"CreateWindow Fail : %s", GetWin32ErrorDescriptionLocal(GetLastError()));
	}
	else ShowWindow(hWnd, SW_FLAG);

	return hWnd;
}

HWND MyCreateWindow(
	const char * className,
	const char * strTitle,
	int x, int y,
	int nX, int nY,
	DWORD style,
	HWND parent,
	HMENU menu,
	void * data,
	int SW_FLAG) {

	return MyCreateWindow(Wide(className), Wide(strTitle), x, y, nX, nY, style, parent, menu, data, SW_FLAG);
}

HWND MyCreateWindowEx(
	DWORD exStyle,
	const wchar_t * className, 
	const wchar_t * strTitle,
	int x, int y,
	int nX, int nY,
	DWORD style,
	HWND parent,
	HMENU menu,
	void * data,
	int SW_FLAG) {

	static bool initialized = 0;
	HWND hWnd;

	HINSTANCE hInstance = GetCurrentModuleHandle();

	if (parent) style |= WS_CHILD;

	hWnd = CreateWindowExW(
		exStyle, 
		className,
		strTitle,
		style,
		x, y,
		nX, nY,
		parent,
		menu,
		hInstance,
		(LPVOID)data);

	if (!hWnd) {
		MyMessageBox(L"CreateWindow Fail : %s", GetWin32ErrorDescriptionLocal(GetLastError()));
	}
	else {
		ShowWindow(hWnd, SW_FLAG);
		SAFE_CALL_NO_ERROR_OK(SetFocus(hWnd));
	    SAFE_CALL(SetActiveWindow(hWnd));
		SetForegroundWindow(hWnd);
	}

	return hWnd;
}


HWND MyCreateWindowEx(
	DWORD exStyle,
	const char * className,
	const char * strTitle,
	int x, int y,
	int nX, int nY,
	DWORD style,
	HWND parent,
	HMENU menu,
	void * data,
	int SW_FLAG) {

	return MyCreateWindowEx(exStyle, Wide(className), Wide(strTitle), x, y, nX, nY, style, parent, menu, data, SW_FLAG);
}


LOGFONTW CreateLogFont(
	wchar_t * fontName, LONG height,
	LONG avWidth, LONG escapement,
	LONG orientation, LONG weight,
	BYTE italic, BYTE underline,
	BYTE strikeOut, BYTE charSet,
	BYTE outPrecision, BYTE clipPrecision,
	BYTE quality, BYTE pitchAndFamily) {

	LOGFONTW logFont;

	logFont.lfHeight = height;
	logFont.lfWidth = avWidth;
	logFont.lfEscapement = escapement;
	logFont.lfOrientation = orientation;
	logFont.lfWeight = weight;
	logFont.lfItalic = italic;
	logFont.lfUnderline = underline;
	logFont.lfStrikeOut = strikeOut;
	logFont.lfCharSet = charSet;
	logFont.lfOutPrecision = outPrecision;
	logFont.lfClipPrecision = clipPrecision;
	logFont.lfQuality = quality;
	logFont.lfPitchAndFamily = pitchAndFamily;
	wcscpy_s(logFont.lfFaceName, fontName);

	return logFont;
}

void DoEvents(int MsgMax, HWND hWnd) {
	MSG msg;
	BOOL result;

	while (PeekMessage(&msg, hWnd, 0, MsgMax, PM_NOREMOVE)) {
		result = GetMessage(&msg, hWnd, 0, MsgMax);
		if (result == 0) { // WM_QUIT
			PostQuitMessage((int)msg.wParam);
			break;
		}
		else if (result == -1) {
			// Handle errors/exit application, etc.
		}
		else  {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}




// Examples how to use the folder and file common dialogs

///////////////////////////////////////////////////////////////////////////////
// Browse for folder using SHBrowseForFolder
// Shows how to set the initial folder using a callback

int CALLBACK BrowseCallbackProc(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM lpData) {
	if (uMsg == BFFM_INITIALIZED)
		SendMessage(hWnd, BFFM_SETSELECTION, TRUE, lpData);
	return 0;
}

bool BrowseForFolder1(HWND parent, char *path, const char *title)
{
	char name[_MAX_PATH];
	BROWSEINFOA info = { parent, NULL, name, title, BIF_USENEWUI, BrowseCallbackProc, (LPARAM)path };
	LPITEMIDLIST items = SHBrowseForFolderA(&info);
	if (!items) return false;
	SHGetPathFromIDListA(items, path);
	LPMALLOC pMalloc;
	SHGetMalloc(&pMalloc);
	pMalloc->Free(items);
	pMalloc->Release();
	return true;
}


///////////////////////////////////////////////////////////////////////////////
// Browse for folder using GetOpenFileName
// Shows how to customize the file browser dialog to select folders instead of files
// Also stores and restores the dialog placement

static WNDPROC g_OldOFNFolderProc;
static char *g_Path;
static RECT *g_Placement;

template <bool folder = true>
static LRESULT CALLBACK OFNFolderProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

	// if the OK button is pressed
	if (uMsg == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDOK) {

		if (!folder) {
			CallWindowProc(g_OldOFNFolderProc, hWnd, uMsg, wParam, lParam);
			return TRUE;
		}

		bool valid = false;

		HWND list = GetDlgItem(GetDlgItem(hWnd, lst2), 1);
		int idx = ListView_GetNextItem(list, -1, LVNI_SELECTED);
		if (GetWindowTextLength(GetDlgItem(hWnd, cmb13))>0) {
			// the file name box is not empty
			// use the default processing, which will open the folder with that name
			CallWindowProc(g_OldOFNFolderProc, hWnd, uMsg, wParam, lParam);
			// then clear the text
			SetDlgItemTextA(hWnd, cmb13, "");
			return TRUE;
		}
		else if (idx >= 0) {
			// if a folder is selected in the list view, its user data is a PIDL
			// get the full folder name as described here: http://msdn.microsoft.com/msdnmag/issues/03/09/CQA/
			LVITEM item = { LVIF_PARAM, idx, 0 };
			ListView_GetItem(list, &item);

			size_t len = SendMessage(hWnd, CDM_GETFOLDERIDLIST, 0, NULL);
			if (len>0) {
				LPMALLOC pMalloc;
				SHGetMalloc(&pMalloc);
				LPCITEMIDLIST pidlFolder = (LPCITEMIDLIST)pMalloc->Alloc(len);
				SendMessage(hWnd, CDM_GETFOLDERIDLIST, len, (LPARAM)pidlFolder);

				STRRET str = { STRRET_WSTR };

				IShellFolder *pDesktop, *pFolder;
				SHGetDesktopFolder(&pDesktop);
				if (SUCCEEDED(pDesktop->BindToObject(pidlFolder, NULL, IID_IShellFolder, (void**)&pFolder))) {
					if (FAILED(pFolder->GetDisplayNameOf((LPITEMIDLIST)item.lParam, SHGDN_FORPARSING, &str)))
						str.pOleStr = NULL;
					pFolder->Release();
					pDesktop->Release();
				}
				else {
					if (FAILED(pDesktop->GetDisplayNameOf((LPITEMIDLIST)item.lParam, SHGDN_FORPARSING, &str)))
						str.pOleStr = NULL;
					pDesktop->Release();
				}

				if (str.pOleStr) {
					DWORD attrib = GetFileAttributesW(str.pOleStr);
					if (attrib != INVALID_FILE_ATTRIBUTES && (attrib&FILE_ATTRIBUTE_DIRECTORY)) {
//#ifdef _UNICODE
//						wcsncpy(g_Path, str.pOleStr, _MAX_PATH);
//#else
						WideCharToMultiByte(CP_ACP, 0, str.pOleStr, -1, g_Path, _MAX_PATH, NULL, NULL);
//#endif
						g_Path[_MAX_PATH - 1] = 0;
						valid = true;
					}
					pMalloc->Free(str.pOleStr);
				}

				pMalloc->Free((void*)pidlFolder);
				pMalloc->Release();
			}
		}
		else {
			// no item is selected, use the current folder
			char path[_MAX_PATH];
			SendMessage(hWnd, CDM_GETFOLDERPATH, _MAX_PATH, (LPARAM)path);
			DWORD attrib = GetFileAttributesA(path);
			if (attrib != INVALID_FILE_ATTRIBUTES && (attrib&FILE_ATTRIBUTE_DIRECTORY)) {
				strcpy_s(g_Path, MAX_PATH, path);
				valid = true;
			}
		}
		if (valid) {
			EndDialog(hWnd, IDOK);
			return TRUE;
		}
	}

	if (uMsg == WM_SHOWWINDOW && wParam && g_Placement)
		SetWindowPos(hWnd, NULL, g_Placement->left, g_Placement->top, g_Placement->right - g_Placement->left, g_Placement->bottom - g_Placement->top, SWP_NOZORDER);

	if (uMsg == WM_DESTROY && g_Placement)
		GetWindowRect(hWnd, g_Placement);

	return CallWindowProc(g_OldOFNFolderProc, hWnd, uMsg, wParam, lParam);
}

template <bool folder = true>
static UINT_PTR APIENTRY OFNFolderHook(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	if (!folder) return 0;

	if (uMsg == WM_INITDIALOG) {
		HWND hWnd = GetParent(hDlg);
		SendMessage(hWnd, CDM_HIDECONTROL, stc2, 0); // hide "Save as type"
		SendMessage(hWnd, CDM_HIDECONTROL, cmb1, 0); // hide the filter combo box
		SendMessage(hWnd, CDM_SETCONTROLTEXT, IDOK, (LPARAM)"Select");
		SendMessage(hWnd, CDM_SETCONTROLTEXT, stc3, (LPARAM)(folder ? "Folder Name:" : "File Name:"));
		auto funcPtr = OFNFolderProc<folder>;
		g_OldOFNFolderProc = (WNDPROC)SetWindowLongPtrA(hWnd, GWLP_WNDPROC, (LONG_PTR)(funcPtr));

		if (g_Placement && (g_Placement->top >= g_Placement->bottom)) {
			// the first time center the dialog relative to its parent
			RECT rc1, rc2;
			HWND parent = GetParent(hWnd);
			if (parent) {
				GetClientRect(parent, &rc1);
				MapWindowPoints(parent, NULL, (POINT*)&rc1, 2);
			}
			else
				GetWindowRect(GetDesktopWindow(), &rc1);
			GetWindowRect(hWnd, &rc2);
			int x = rc1.left + ((rc1.right - rc1.left) - (rc2.right - rc2.left)) / 2;
			int y = rc1.top + ((rc1.bottom - rc1.top) - (rc2.bottom - rc2.top)) / 2;
			if (x<rc1.left) x = rc1.left;
			if (y<rc1.top) y = rc1.top;
			g_Placement->left = x;
			g_Placement->top = y;
			g_Placement->right = x + (rc2.right - rc2.left);
			g_Placement->bottom = y + (rc2.bottom - rc2.top);
		}

		return 1;
	}

	
	if (uMsg == WM_NOTIFY && ((OFNOTIFY*)lParam)->hdr.code == CDN_FILEOK) {
		if (folder) {
			// reject all files when the OK button is pressed
			// this will stop the dialog from closing
			SetWindowLongPtrA(hDlg, DWLP_MSGRESULT, TRUE);
			return TRUE;
		}
		else {
			return 0;
		}
	}

	return 0;
}

BOOL BrowseForFolderCore(char *path, const char *title, RECT *placement) {

	OPENFILENAMEA of;
	char path2[MAX_PATH];
	strcpy_s(path2, MAX_PATH, path);
	char fname[MAX_PATH];
	fname[0] = 0;

	HWND parent = NULL;

	memset(&of, 0, sizeof(of));
	of.lStructSize = sizeof(of);
	of.hwndOwner = parent;
	// weird filter to exclude all files and just keep the folders
	of.lpstrFilter = "Folders\0qqqqqqqqqqqqqqq.qqqqqqqqq\0";
	of.nFilterIndex = 1;
	of.lpstrInitialDir = path2; // use the original path as the initial directory
	of.lpstrFile = fname;
	of.lpstrTitle = title;
	of.nMaxFile = MAX_PATH;
	of.Flags = OFN_ENABLEHOOK | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_ENABLESIZING | OFN_DONTADDTORECENT;
	of.lpstrDefExt = "";
	of.lpfnHook = OFNFolderHook;

	g_Path = path;
	g_Placement = placement;

	return (BOOL)GetOpenFileNameA(&of);
}

aVect<char> BrowseForFile(
	const char * filters, 
	const char *path, 
	const char *title, 
	RECT *placement) 
{

	OPENFILENAMEA of;
	char path2[MAX_PATH];
	strcpy_s(path2, MAX_PATH, path);
	char fname[MAX_PATH];
	fname[0] = 0;

	HWND parent = NULL;

	memset(&of, 0, sizeof(of));
	of.lStructSize = sizeof(of);
	of.hwndOwner = parent;
	of.lpstrInitialDir = path2; // use the original path as the initial directory
	of.lpstrFile = fname;
	of.lpstrTitle = title;
	of.nMaxFile = MAX_PATH;
	of.Flags = OFN_ENABLEHOOK | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_ENABLESIZING | OFN_DONTADDTORECENT;
	of.lpstrDefExt = "";
	//of.lpfnHook = OFNFolderHook<false>;
	of.lpstrFilter = filters;

	g_Placement = placement;

	aVect<char> retVal;

	if (GetOpenFileNameA(&of)) {
		retVal = of.lpstrFile;
	}

	return std::move(retVal);
}

aVect<char> BrowseForFolder_old(const char * path, const char *title, RECT *placement) {

	aVect<char> retVal;
	char path2[MAX_PATH];

	if (path) {
		strcpy_s(path2, MAX_PATH, path);
	} else {
		path2[0] = 0;
	}
	
	if (BrowseForFolderCore(path2, title, placement)) {
		retVal.sprintf("%s", path2);
	}

	return retVal;
}

//Mesure du "temps processeur" \E9coul\E9
double tic(void) {
	SYSTEMTIME systemTimeNow;
	__int64 fileTimeNow;

	GetSystemTime(&systemTimeNow);
	SystemTimeToFileTime(&systemTimeNow, (LPFILETIME)&fileTimeNow);

	return (double)(fileTimeNow) / 1e7;
}

double toc(double t) { double s = tic(); return (s - t); }


//Retourne le temps CPU KernelMode utilis\E9 (en secondes)
double CPU_KernelTic(void) {
	__int64 lpCreationTime, lpExitTime, lpKernelTime, lpUserTime;
	GetProcessTimes(GetCurrentProcess(), (LPFILETIME)&lpCreationTime, (LPFILETIME)&lpExitTime, (LPFILETIME)&lpKernelTime, (LPFILETIME)&lpUserTime);

	return (double)((__int64)lpKernelTime) / 1e7;
}

//Retourne le temps CPU KernelMode utilis\E9 depuis CPU_KernelTic (en secondes)
double CPU_KernelToc(double kernelTic) {
	__int64 lpCreationTime, lpExitTime, lpKernelTime, lpUserTime;
	GetProcessTimes(GetCurrentProcess(), (LPFILETIME)&lpCreationTime, (LPFILETIME)&lpExitTime, (LPFILETIME)&lpKernelTime, (LPFILETIME)&lpUserTime);

	return (double)((__int64)lpKernelTime) / 1e7 - kernelTic;
}

//Retourne le temps CPU UserMode utilis\E9 (en secondes)
double CPU_UserTic(void) {
	__int64 lpCreationTime, lpExitTime, lpKernelTime, lpUserTime;
	GetProcessTimes(GetCurrentProcess(), (LPFILETIME)&lpCreationTime, (LPFILETIME)&lpExitTime, (LPFILETIME)&lpKernelTime, (LPFILETIME)&lpUserTime);

	return (double)((__int64)lpUserTime) / 1e7;
}

//Retourne le temps CPU UserMode utilis\E9 depuis CPU_UserTic (en secondes)
double CPU_UserToc(double userTic) {
	__int64 lpCreationTime, lpExitTime, lpKernelTime, lpUserTime;
	GetProcessTimes(GetCurrentProcess(), (LPFILETIME)&lpCreationTime, (LPFILETIME)&lpExitTime, (LPFILETIME)&lpKernelTime, (LPFILETIME)&lpUserTime);

	return (double)((__int64)lpUserTime) / 1e7 - userTic;
}


double ComputationStartTime(void) {
	__int64 lpCreationTime, lpExitTime, lpKernelTime, lpUserTime;

	GetProcessTimes(GetCurrentProcess(), (LPFILETIME)&lpCreationTime, (LPFILETIME)&lpExitTime, (LPFILETIME)&lpKernelTime, (LPFILETIME)&lpUserTime);

	return (double)(__int64)(lpCreationTime) / 1e7;
}

void * aVectEx_malloc(size_t s, void * heap) {
	return HeapAlloc(*(HANDLE*)heap, 0, s);
}

void * aVectEx_realloc(void * ptr, size_t s, void * heap) {
	return HeapReAlloc(*(HANDLE*)heap, 0, ptr, s);
}

void aVectEx_free(void * ptr, void * heap) {
	HeapFree(*(HANDLE*)heap, 0, ptr);
}


//
//template <class T, class U> class ExecAtScopeEnd {
//	T param;
//
//};

WNDPROC g_OldWndProc;

namespace controlID {
	enum {labelID = 2, editID, buttonOkID, buttonCancelID, buttonCloseID, additionalButtonID};
}

LRESULT CALLBACK EditSubWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {

	//printf("EditSubWndProc : Message %d :\n", message);
	/*static InputBoxParams * params;

	static bool ctrlKeyDown;

	if (message == WM_USER + 1) {
		params = (InputBoxParams*)(lParam);
	}
	else */if (message == WM_CHAR || message == WM_KEYDOWN || message == WM_KEYUP) {
		
		if (message == WM_CHAR) {
			if (wParam == VK_RETURN) {
				SAFE_CALL(PostMessage(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(controlID::buttonOkID, BN_CLICKED), NULL));
			}
			else if (wParam == VK_ESCAPE) {
				SAFE_CALL(PostMessage(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(controlID::buttonCancelID, BN_CLICKED), NULL));
			}
		}

		/*if (!params) MY_ERROR("params uninitialized");

		if (wParam == VK_CONTROL || wParam == VK_LCONTROL || wParam == VK_RCONTROL) {
			if (message == WM_KEYDOWN) {
				ctrlKeyDown = true;
			}
			else if (message == WM_KEYUP) {
				ctrlKeyDown = false;
			}
		}

		if (!params->editable &&
			wParam != VK_LEFT &&
			wParam != VK_RIGHT &&
			wParam != VK_SHIFT &&
			wParam != VK_CONTROL &&
			wParam != VK_LSHIFT &&
			wParam != VK_LCONTROL &&
			wParam != VK_RSHIFT &&
			wParam != VK_RCONTROL &&
			wParam != VK_END &&
			wParam != VK_HOME &&
			!(ctrlKeyDown && (wParam == 0x43 || wParam == 3))) {
			return 0;
		}*/
	}

	return CallWindowProc(g_OldWndProc, hWnd, message, wParam, lParam);
}

SIZE TextSize(HWND hWnd, const wchar_t * str) {
	
	SIZE sz;
	sz.cx = sz.cy = 0;

	if (str) {
		bool mustDeleteFont = false;
		HFONT hFont = (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0);
		HDC hdc = SAFE_CALL(GetDC(hWnd));
		if (hFont == NULL) {
			NONCLIENTMETRICS ncm;
			static NONCLIENTMETRICS zero_ncm;
			ncm = zero_ncm;
			ncm.cbSize = sizeof(ncm);
			SAFE_CALL(SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0));
			hFont = SAFE_CALL(CreateFontIndirect(&ncm.lfCaptionFont));
			mustDeleteFont = true;
		}
		HFONT hOldFont = SAFE_CALL(SelectFont(hdc, hFont));
		SAFE_CALL(GetTextExtentPoint32W(hdc, str, (int)wcslen(str), &sz));
		if (hOldFont != NULL) SAFE_CALL(SelectFont(hdc, hOldFont));
		if (mustDeleteFont) SAFE_CALL(DeleteFont(hFont));
	}

	return sz;
}

int TextWidth(HWND hWnd, const wchar_t * str, bool accountForLineFeeds) {
	
	aVect<wchar_t> buffer;

	if (accountForLineFeeds) {
		buffer.Copy(str);
		wchar_t * remain = buffer;
		int maxWidth = 0;
		while (wchar_t * line = RetrieveStr<L'\n'>(remain)) {
			int width = TextSize(hWnd, line).cx;
			if (width > maxWidth) maxWidth = width;
		}
		return maxWidth;
	}

	return TextSize(hWnd, str).cx;
}

int TextHeight(HWND hWnd, const wchar_t * str, bool accountForLineFeeds) {
	if (!str) return 0;
	const wchar_t * found, *beginSearch = str;
	
	int nLines = 1;

	while (true) {
		found = wcsstr(beginSearch, L"\n");
		if (!found) break;
		nLines++;
		beginSearch = found + 1; //ok because strings must be null-terminated
	}
	
	
	return TextSize(hWnd, str).cy * (accountForLineFeeds ? nLines : 1);
}

SIZE GetNonClientAdditionalSize(HWND hWnd) {

	SIZE sz;
	RECT rInner, rOuter;

	SAFE_CALL(GetWindowRect(hWnd, &rOuter));
	SAFE_CALL(GetClientRect(hWnd, &rInner));

	sz.cx = (rOuter.right - rOuter.left) - (rInner.right - rInner.left);
	sz.cy = (rOuter.bottom - rOuter.top) - (rInner.bottom - rInner.top);

	return sz;
}

bool g_InputBox_invalid_params;

LRESULT CALLBACK InputBoxWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {

	static aVect<wchar_t> * pBuffer;
	static HFONT hFont;
	static HFONT hFontTitle;
	static InputBoxParams * params;
	static int quitValue;
	//printf("InputBoxWndProc : Message %d :\n", message);

	switch (message) {
		case WM_MOUSEWHEEL: {
			SendMessageW(GetConsoleWindow(), message, wParam, lParam);
			return 0;
		}
		case WM_SETFOCUS: {
			if (g_InputBox_invalid_params) {
				return 0;
			}
			SAFE_CALL(SetFocus(SAFE_CALL(GetDlgItem(hWnd, controlID::editID), (HWND)NULL)), (HWND)NULL);
			return 0;
		}
		case WM_CREATE: {

			SIZE sz = GetNonClientAdditionalSize(hWnd);

			CREATESTRUCT * createStruct = (CREATESTRUCT*)lParam;
			params = (InputBoxParams*)(createStruct->lpCreateParams);

			pBuffer = params->pBuffer;
			int height, requestedWidth = createStruct->cx, margeWidth = 3, margeHeight = 3, totalHeight = 0;

			bool autoWidth = (requestedWidth == -1);
			if (autoWidth) requestedWidth = 5;

			int neededWidth = requestedWidth + 2 * margeWidth;

			int maxWidth = neededWidth;
			
			int controlWidth = TextWidth(hWnd, params->title) + 6;
			if (controlWidth > maxWidth) maxWidth = controlWidth;

			quitValue = 0;
			g_InputBox_invalid_params = false;

			HWND hLabel = (HWND)INVALID_HANDLE_VALUE;

			if (params->parentWindow) {

				totalHeight += margeHeight + (height = 15);

				hLabel = PutTopLeft(hWnd, L"Label", params->title, neededWidth,
					height, margeWidth, margeHeight, controlID::labelID);

				if (params->pLogFont) {
					if (hFontTitle) SAFE_CALL(DeleteObject(hFontTitle));
					LONG prevWeight = params->pLogFont->lfWeight;
					params->pLogFont->lfWeight = prevWeight > FW_BOLD ? FW_HEAVY : FW_BOLD;
					hFontTitle = SAFE_CALL(CreateFontIndirectW(params->pLogFont));
					params->pLogFont->lfWeight = prevWeight;
					SendMessageW(hLabel, WM_SETFONT, (WPARAM)hFontTitle, TRUE);
				}

				if (params->pLogFont) SendMessageW(hLabel, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

				controlWidth = TextWidth(hLabel, params->title) + 6;
				if (controlWidth > maxWidth) maxWidth = controlWidth;

				int realHeight = TextHeight(hLabel, params->title);
				if (realHeight > height) {
					totalHeight += (realHeight - height);
					height = realHeight;
				}

				if (controlWidth > neededWidth) {
					SAFE_CALL(SetWindowPos(hLabel, 0, 0, 0, controlWidth, height, SWP_NOZORDER | SWP_NOMOVE));
				}
			}

			if (params->question) {

				totalHeight += margeHeight + (height = 15);

				hLabel = (hLabel != (HWND)INVALID_HANDLE_VALUE) ? 
					PutBottom(hLabel, L"Label", params->question, neededWidth, height, margeHeight, controlID::labelID) :
					PutTopLeft(hWnd,  L"Label", params->question, neededWidth, height, margeWidth, margeHeight, controlID::labelID);;
				
				if (params->pLogFont) {
					if (hFont) SAFE_CALL(DeleteObject(hFont));
					hFont = SAFE_CALL(CreateFontIndirectW(params->pLogFont));
					SendMessage(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
				}

				if (params->pLogFont) SendMessageW(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

				controlWidth = TextWidth(hLabel, params->question) + 4*margeWidth;
				if (controlWidth > maxWidth) maxWidth = controlWidth;

				int realHeight = TextHeight(hLabel, params->question);
				if (realHeight > height) {
					totalHeight += (realHeight - height);
					height = realHeight;
				}

				if (controlWidth > neededWidth) {
					SAFE_CALL(SetWindowPos(hLabel, 0, 0, 0, controlWidth, height, SWP_NOZORDER | SWP_NOMOVE));
				}
			}

			if (params->suggestion) pBuffer->Copy(params->suggestion);

			int editHeight, editWidth = Max(autoWidth ? requestedWidth : params->length, 20);

			totalHeight +=  margeHeight + (editHeight = 18);
			HWND hEdit = (hLabel != (HWND)INVALID_HANDLE_VALUE) ? 
				PutBottom(hLabel, L"Edit", *pBuffer, editWidth, editHeight, margeHeight, controlID::editID, WS_BORDER) :
				PutTopLeft(hWnd,  L"Edit", *pBuffer, editWidth, editHeight, margeWidth, margeHeight, controlID::editID, WS_BORDER);

			int maxMonitorWidth = GetDeviceCaps(SAFE_CALL(GetDC(hWnd)), HORZRES);

			int editControlWidth = TextWidth(hEdit, *pBuffer) + 18;
			if (editControlWidth > maxWidth) maxWidth = editControlWidth;
			if (maxWidth > (int)(0.75 * maxMonitorWidth)) maxWidth = (int)(0.75 * maxMonitorWidth);

			HWND hLastCtrl, hButtonOK, hButtonCancel, hButtonClose;

			hLastCtrl = hButtonOK = hButtonCancel = hButtonClose = (HWND)INVALID_HANDLE_VALUE;

			int totalWidth = 0;
			int width;

			if (params->OkButton) {
				totalHeight += margeHeight + (height = 20);
				totalWidth += margeWidth + (width = 40);
				hLastCtrl = hButtonOK = PutBottom(hEdit, L"Button", L"OK", width, height, margeHeight, controlID::buttonOkID);
			}
			
			if (params->CancelButton) {
				totalWidth += margeWidth + (width = 40);
				if (hLastCtrl != INVALID_HANDLE_VALUE) {
					hLastCtrl = hButtonCancel = PutRight(hLastCtrl, L"Button", L"Cancel", width, height, margeWidth, controlID::buttonCancelID);
				}
				else {
					totalHeight += margeHeight + (height = 20);
					hLastCtrl = hButtonCancel = PutBottom(hEdit, L"Button", L"Cancel", width, height, margeWidth, controlID::buttonCancelID);
				}
			}

			if (params->CloseButton) {
				totalWidth += margeWidth + (width = 40);
				if (hLastCtrl != INVALID_HANDLE_VALUE) {
					hLastCtrl = hButtonClose = PutRight(hLastCtrl, L"Button", L"Close", width, height, margeWidth, controlID::buttonCloseID);
				}
				else {
					totalHeight += margeHeight + (height = 20);
					hLastCtrl = hButtonClose = PutBottom(hEdit, L"Button", L"Close", width, height, margeHeight, controlID::buttonCloseID);
				}
			}

			if (params->additionalButton) {
				width = 20;
				if (hLastCtrl != INVALID_HANDLE_VALUE) {
					hLastCtrl = hButtonClose = PutRight(hLastCtrl, L"Button", params->additionalButton, width, height, margeWidth, controlID::additionalButtonID);
				}
				else {
					totalHeight += margeHeight + (height = 20);
					hLastCtrl = hButtonClose = PutBottom(hEdit, L"Button", params->additionalButton, width, height, margeHeight, controlID::additionalButtonID);
				}

				controlWidth = TextWidth(hLastCtrl, params->additionalButton) + 4 * margeWidth;
				SAFE_CALL(SetWindowPos(hLastCtrl, 0, 0, 0, controlWidth, height, SWP_NOZORDER | SWP_NOMOVE));
				totalWidth += margeWidth +  controlWidth;
			}

			if (totalWidth > maxWidth) maxWidth = totalWidth + margeWidth;
			if (maxWidth > (int)(0.75 * maxMonitorWidth)) maxWidth = (int)(0.75 * maxMonitorWidth);

			//SAFE_CALL(SetWindowPos(hWnd, 0, 0, 0, width + borders, totalHeight + marge + borders, SWP_NOZORDER | SWP_NOMOVE));
			SAFE_CALL(SetWindowPos(hWnd, 0, 0, 0, maxWidth + sz.cx, totalHeight + sz.cy + margeHeight, SWP_NOZORDER | SWP_NOMOVE));

			if (autoWidth || maxWidth - 2*margeWidth > editWidth) {
				SAFE_CALL(SetWindowPos(hEdit, 0, 0, 0, maxWidth - 2*margeWidth, editHeight, SWP_NOZORDER | SWP_NOMOVE));
			}

			/*EditSubWndProc(nullptr, WM_USER + 1, (WPARAM)nullptr, (LPARAM)params);*/
			g_OldWndProc = (WNDPROC)SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)EditSubWndProc);

			if (!params->editable) SAFE_CALL(SendMessageW(hEdit, EM_SETREADONLY, (WPARAM)true, 0L));

			if (params->parentWindow) {
				AutoFitWindowForChildren(params->parentWindow, true, false);
			}

			SAFE_CALL1_NO_ERROR_OK(SetFocus(hEdit), (HWND)NULL);

			size_t len = (*pBuffer).Count()-1;
			if (len > 0) SendMessageW(hEdit, EM_SETSEL, 0, len);

			return 0;
		}
		case WM_COMMAND: {

			if (g_InputBox_invalid_params) {
				DefWindowProcW(hWnd, WM_CLOSE, 0, 0);
				return 0;
			}

			int controlID = LOWORD(wParam);
			int notificationCode = HIWORD(wParam);

			//printf("InputBoxWndProc : \tWM_COMMAND : controlID = %d, notificationCode = %d\n", controlID, notificationCode);

			switch (controlID) {
				case controlID::additionalButtonID: {
					if (notificationCode == BN_CLICKED) {
						if (params->additionalButtonCallback(params)) return 1;
					}
				} /* fallthrough */
				case controlID::buttonOkID: { 
					if (notificationCode == BN_CLICKED) {
						HWND editHWND = SAFE_CALL(GetDlgItem(hWnd, controlID::editID), (HWND)NULL);
						int len = GetWindowTextLengthW(editHWND);
						if (len) {
							pBuffer->Redim(len+1);
							SAFE_CALL(GetWindowTextW(editHWND, *pBuffer, len+1));
						}
						else {
							pBuffer->Redim(1);
							(*pBuffer)[0] = 0;
						}
						//PostQuitMessage(1);
						quitValue = 1;

						//printf("BN_CLICKED : PostQuitMessage(1)\n");
						break;
					}
					return 1;
				}
				case controlID::buttonCloseID: /* fallthrough */
				case controlID::buttonCancelID: {
					if (notificationCode == BN_CLICKED) {
						//printf("BN_CLICKED : PostQuitMessage(0)\n");
						//PostQuitMessage(0);
						quitValue = 0;
						break;
					}
					return 1;
				}
				case controlID::editID:
				default:{
					return 1;
				}
			}

			DefWindowProcW(hWnd, WM_CLOSE, 0, 0);

			return 0;

		}
		case WM_CLOSE: {
			//PostQuitMessage(0);
			quitValue = 0;
			//printf("WM_CLOSE : PostQuitMessage(0)\n");
			/* fallthrough */
		}
		case WM_DESTROY: {
			PostQuitMessage(quitValue);
		}
		default:{
			return DefWindowProcW(hWnd, message, wParam, lParam);
		}
	}
}

void InputBox_UnregisterWndClass() {
	UnregisterClassW(L"InputBox", GetCurrentModuleHandle());
}

void InputBoxCore(
	aVect<wchar_t> & retVal,
	const wchar_t * title,
	const wchar_t * question,
	const wchar_t * suggestion,
	int boxLength, 
	bool editable,
	bool OkButton,
	bool CancelButton, 
	bool CloseButton,
	HWND parentWindow,
	int left,
	int top,
	LOGFONTW * pLogFont,
	const wchar_t * additionalButton,
	bool (*additionalButtonCallback)(InputBoxParams*),
	void * additionalButtonCallbackParams)
{

	static bool wndClassRegistered;

	if (!wndClassRegistered) {
		MyRegisterClass(InputBoxWndProc, L"InputBox");
		wndClassRegistered = true;
		atexit(InputBox_UnregisterWndClass);
	}

	//static aVect<char> buffer;
	aVect<wchar_t> buffer;
	//static InputBoxParams params;
	InputBoxParams params;

	params.pBuffer  = &buffer;
	params.title = title;
	params.question = question;
	params.suggestion = suggestion;
	params.length = boxLength;
	params.editable = editable;
	params.OkButton = OkButton;
	params.CancelButton = CancelButton;
	params.CloseButton = CloseButton;
	params.parentWindow = parentWindow;
	params.top = top;
	params.left = left;
	params.pLogFont = pLogFont;
	params.additionalButton = additionalButton;
	params.additionalButtonCallback = additionalButtonCallback;
	params.additionalButtonCallbackParams = additionalButtonCallbackParams;

	//DWORD additionalStyle = parentWindow ? WS_THICKFRAME : 0;
	DWORD wndStyle = NULL;// parentWindow ? WS_OVERLAPPEDWINDOW : NULL;
	DWORD wndStyleEx = /*parentWindow ? NULL :*/ WS_EX_TOOLWINDOW | WS_EX_APPWINDOW;

	SAFE_CALL(MyCreateWindowEx(wndStyleEx, L"InputBox", (wchar_t*)title,
		parentWindow ? left : 400, 
		parentWindow ? top : 400, 
		boxLength > 0 ? boxLength : 50, 93,
		wndStyle, parentWindow, NULL, &params));

	MSG msg;

	while (GetMessageW(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg) ;
		DispatchMessageW(&msg) ;
	}

	g_InputBox_invalid_params = true;
	msg.wParam ? retVal.Copy(buffer) : retVal.Free();
}

aVect<wchar_t> InputBox(const wchar_t * title, const wchar_t * question, const wchar_t * suggestion, int boxLength) {

	aVect<wchar_t> retVal;

	InputBoxCore(retVal, title, question, suggestion, boxLength, true, true, true, false, NULL, 0, 0, nullptr);

	return retVal;
}

aVect<wchar_t> InputBox(LOGFONTW & logFont, const wchar_t * title, const wchar_t * question, const wchar_t * suggestion, int boxLength) {

	aVect<wchar_t> retVal;

	InputBoxCore(retVal, title, question, suggestion, boxLength, true, true, true, false, NULL, 0, 0, &logFont);

	return retVal;
}

aVect<wchar_t> InputBox(HWND parentWindow, int left, int top, const wchar_t * title, const wchar_t * question, const wchar_t * suggestion, int boxLength) {

	aVect<wchar_t> retVal;

	InputBoxCore(retVal, title, question, suggestion, boxLength, true, true, true, false, parentWindow, left, top, nullptr);

	return retVal;
}

aVect<wchar_t> InputBox(LOGFONTW & logFont, HWND parentWindow, int left, int top, const wchar_t * title, const wchar_t * question, const wchar_t * suggestion, int boxLength) {

	aVect<wchar_t> retVal;

	InputBoxCore(retVal, title, question, suggestion, boxLength, true, true, true, false, parentWindow, left, top, &logFont);

	return retVal;
}

aVect<wchar_t> TextBox(const wchar_t * title, const wchar_t * subTitle, const wchar_t * text, int boxLength) {

	//static aVect<char> retVal;
	aVect<wchar_t> retVal;

	InputBoxCore(retVal, title, subTitle, text, boxLength, false, false, false, true, NULL, 0, 0, nullptr);

	return retVal;
}

aVect<wchar_t> TextBox(LOGFONTW & logFont, const wchar_t * title, const wchar_t * subTitle, const wchar_t * text, int boxLength) {

	aVect<wchar_t> retVal;

	InputBoxCore(retVal, title, subTitle, text, boxLength, false, false, false, true, NULL, 0, 0, &logFont);

	return retVal;
}

aVect<wchar_t> TextBox(HWND parentWindow, int left, int top, const wchar_t * title, const wchar_t * subTitle, const wchar_t * text, int boxLength) {

	aVect<wchar_t> retVal;

	InputBoxCore(retVal, title, subTitle, text, boxLength, false, false, false, true, parentWindow, left, top, nullptr);

	return retVal;
}

aVect<wchar_t> TextBox(LOGFONTW & logFont, HWND parentWindow, int left, int top, const wchar_t * title, const wchar_t * subTitle, const wchar_t * text, int boxLength) {

	aVect<wchar_t> retVal;

	InputBoxCore(retVal, title, subTitle, text, boxLength, false, false, false, true, parentWindow, left, top, &logFont);

	return retVal;
}

BOOL CALLBACK DisableRetryButton(HWND hWnd, LPARAM lParam) {

	if (/*SAFE_CALL(*/GetDlgCtrlID(hWnd)/*)*/ == IDTRYAGAIN) { //sometimes return 0 ...
		EnableWindow(hWnd, false);
		return false;
	}

	return true; 
}


HHOOK hMsgBoxHook;

LRESULT CALLBACK CBTProc(int nCode, WPARAM wParam, LPARAM lParam) {

	HWND hWnd;
 
    if(nCode < 0)
        return CallNextHookEx(hMsgBoxHook, nCode, wParam, lParam);
 
    switch(nCode) {
		case HCBT_ACTIVATE: {
			// Get handle to the message box
			hWnd = (HWND)wParam;

			// Do customization
			EnumChildWindows(hWnd, DisableRetryButton, 0);
 			
			return 0;
		}
    }
    
    // Call the next hook, if there is one
    return CallNextHookEx(hMsgBoxHook, nCode, wParam, lParam);
}

int MsgBoxEx(HWND hWnd, TCHAR *szText, TCHAR *szCaption, UINT uType) {
    
	int retval;
	
    // Install a window hook, so we can intercept the message-box
    // creation, and customize it
    hMsgBoxHook = SAFE_CALL(SetWindowsHookEx(
        WH_CBT, 
        CBTProc, 
        NULL, 
        GetCurrentThreadId()            // Only install for THIS thread!!!
        ));
 
    // Display a standard message box
    retval = MessageBox(hWnd, szText, szCaption, uType);
 
    // remove the window hook
    SAFE_CALL(UnhookWindowsHookEx(hMsgBoxHook));

    return retval;
}

void StopAllThreadsBut(DWORD threadId, bool resume) {
	
	static WinCriticalSection cs;

	Critical_Section(cs) {

		static aVect<DWORD> stoppedThreadsID;

		if (resume) {
			if (!stoppedThreadsID) __debugbreak();

			aVect_static_for(stoppedThreadsID, i) {
				HANDLE hThread = SAFE_CALL(OpenThread(THREAD_SUSPEND_RESUME, false, stoppedThreadsID[i]));
				SAFE_CALL(ResumeThread(hThread));
				SAFE_CALL(CloseHandle(hThread));
			}

			stoppedThreadsID.Redim(0);
			return;
		}
			 
		int currentProcessID = GetCurrentProcessId();

		HANDLE hProcessSnap = CreateToolhelp32Snapshot(
			/*__in  DWORD dwFlags*/			TH32CS_SNAPTHREAD,
			/*__in  DWORD th32ProcessID*/	currentProcessID
			);

		THREADENTRY32 te32;

		if (hProcessSnap != INVALID_HANDLE_VALUE && hProcessSnap != NULL) {
			te32.dwSize = sizeof(te32);
			if (Thread32First(hProcessSnap, &te32))  {
				te32.dwSize = sizeof(te32);
				do {
					if (te32.th32OwnerProcessID == currentProcessID) {
						if (te32.th32ThreadID != threadId) {
							HANDLE hThread = SAFE_CALL(OpenThread(THREAD_SUSPEND_RESUME, false, te32.th32ThreadID));
							SuspendThread(hThread);
							CloseHandle(hThread);
							stoppedThreadsID.Push(te32.th32ThreadID);
						}
					}
				} while (Thread32Next(hProcessSnap, &te32));
			}
			else {
				int err = GetLastError();
				printf("Thread32Next( hProcessSnap, &te32 ) == NULL\n");
				printf("Thread32Next error : \"%s\"\n", strerror(err));
			}
			CloseHandle(hProcessSnap);
		}
		else printf("hProcessSnap == INVALID_HANDLE_VALUE\n");

		
	}
}

void StopAllOtherThreads(bool resume) {
	StopAllThreadsBut(GetCurrentThreadId(), resume);
}

DWORD WINAPI Thread_ReportErrorAndExit(LPVOID errorString) {

	StopAllThreadsBut(GetCurrentThreadId());

	MessageBoxA(NULL, (char*)errorString, "", MB_ICONSTOP | MB_SERVICE_NOTIFICATION);
	ExitProcess(666);
}

DWORD WINAPI ReportErrorAndExit(char * errorString) {

	HANDLE h = CreateThread(NULL, 0, Thread_ReportErrorAndExit, (void*)errorString, 0, NULL);
	
	if (h == NULL) {
		printf("\nReportErrorAndExit : CreateThread failed, errorString = \"%s\"\n", errorString);
		return false;
	}
	else {
		WaitForSingleObject(h, INFINITE);
		CloseHandle(h);
	}

	return true;
}

struct MiniDumpWriteDumpInfo {
	HANDLE hProcess;
	DWORD ProcessId;
	HANDLE hFile;
	MINIDUMP_TYPE DumpType;
	PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam;
	PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam;
	PMINIDUMP_CALLBACK_INFORMATION CallbackParam;
};

DWORD WINAPI GenerateDump(LPVOID lpParameter) {

	if (!MiniDumpWriteDump(
		/*HANDLE hProcess*/  ((MiniDumpWriteDumpInfo*)lpParameter)->hProcess,
		/*DWORD ProcessId*/ ((MiniDumpWriteDumpInfo*)lpParameter)->ProcessId,
		/*HANDLE hFile*/    ((MiniDumpWriteDumpInfo*)lpParameter)->hFile, 
		/*MINIDUMP_TYPE DumpType*/ ((MiniDumpWriteDumpInfo*)lpParameter)->DumpType,
		/*PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam*/ ((MiniDumpWriteDumpInfo*)lpParameter)->ExceptionParam,
		/*PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam*/ ((MiniDumpWriteDumpInfo*)lpParameter)->UserStreamParam,
		/*PMINIDUMP_CALLBACK_INFORMATION CallbackParam*/ ((MiniDumpWriteDumpInfo*)lpParameter)->CallbackParam
	))	{
		DWORD error = GetLastError();
		char tmp[5000];
		//sprintf_s(tmp, sizeof(tmp), "echo \"MiniDumpWriteDump fail! err = %p\" > dbg.txt", error);
		sprintf_s(tmp, sizeof(tmp), 
			       "MiniDumpWriteDump fail! err = %x\n"
				   "Description : %S", error, (wchar_t*)GetWin32ErrorDescription(error));
		//system(tmp);
		printf("Error : \"%s\"\n", tmp);
		
		if (FILE * file = fopen(DEBUG_FILENAME, "a")) {
			fprintf(file, "\n%s %s\n%s\n", (char*)GetDateStr(), (char*)GetTimeStr(), tmp);
			fclose(file);
		}
	}
	return 0;
}

void CoreDump(
	const char * str, 
	void * pvEx, 
	int threadID, 
	char * file, 
	size_t line, 
	char * func,
	bool reportAndExit) {
	
	_EXCEPTION_POINTERS * pEx = (_EXCEPTION_POINTERS *)pvEx;

	char buffer[5000];
	char exeptionStr[1000];
	char currentDirectory[MAX_PATH];

	GetCurrentDirectoryA(
		/*__in   DWORD nBufferLength,*/ MAX_PATH,
		/*__out  LPTSTR lpBuffer*/		currentDirectory
	);

	static int preventReentrance;
	
	if (preventReentrance) {
		ReportErrorAndExit("Error while dumping...\nExiting.");
		ExitThread(false);
	}

	preventReentrance++;

	buffer[0]=0;
	exeptionStr[0]=0;

	//_fcloseall();
	//fcloseall();

	if (pEx) {
		sprintf_s(exeptionStr, sizeof(exeptionStr), 
			"Unhandled Exception 0x%x @ 0x%p!\n",
			pEx->ExceptionRecord->ExceptionCode,
#ifdef _M_X64
			(void*)pEx->ContextRecord->Rip);
#else
			(void*)pEx->ContextRecord->Eip);
#endif
		for (unsigned int i=0 ; i < pEx->ExceptionRecord->NumberParameters ; i++) {
			char tmp[1000];
			sprintf_s(tmp, sizeof(tmp), "%s", exeptionStr);
			sprintf_s(exeptionStr, sizeof(exeptionStr),
				"%sParam %d : %p\n", tmp, i, (void*)pEx->ExceptionRecord->ExceptionInformation[i]);
		}
	}

//	if (IsDebuggerPresent()) {
//
//
//		char tmp_errorString[5000];
//		
//		if (pEx) sprintf_s(tmp_errorString, sizeof(tmp_errorString), "Unhandled Exception %p @ %p!\n",
//						   pEx->ExceptionRecord->ExceptionCode, 
//						   pEx->ContextRecord->Eip);
//		else sprintf_s(tmp_errorString, sizeof(tmp_errorString), "Erreur : %s", str);
//
//
//		__debugbreak();
//
////		MessageBox((HWND)NULL, str, "Erreur", MB_ICONSTOP|MB_SERVICE_NOTIFICATION);
//	}

	char * errorFileName = DEBUG_FILENAME;
	FILE * filePtr = fopen(errorFileName, "a");

	if (filePtr) {
		fprintf(filePtr, "%s %s\n%s\n -Function : %s\n -File : %s\n -Line : %llu\n\n"
			"%s\n", (char*)GetDateStr(), (char*)GetTimeStr(), str, func, file, (unsigned long long)line, exeptionStr);
		
		fclose(filePtr);
	}

	int currentProcessID = GetCurrentProcessId();
	 
	HANDLE hProcessSnap = CreateToolhelp32Snapshot(
		/*__in  DWORD dwFlags*/			TH32CS_SNAPALL,
		/*__in  DWORD th32ProcessID*/	currentProcessID
	);

#ifdef PROCESSENTRY32
#define OLD_PROCESSENTRY32 PROCESSENTRY32
#define OLD_Process32First Process32First
#define OLD_Process32Next  Process32Next
#undef PROCESSENTRY32 
#undef Process32First
#undef Process32Next
#endif

#define PROCESSENTRY32A PROCESSENTRY32 
#define Process32FirstA Process32First 
#define Process32NextA  Process32Next 

#ifdef OLD_PROCESSENTRY32
#define PROCESSENTRY32 OLD_PROCESSENTRY32 
#define Process32First OLD_Process32First 
#define Process32Next  OLD_Process32Next 
#endif

	PROCESSENTRY32 pe32;

	pe32.dwSize = sizeof(pe32);

	aVect<char> dumpFileName;

	if (hProcessSnap != INVALID_HANDLE_VALUE && hProcessSnap != NULL) {

		if( Process32First( hProcessSnap, &pe32 ) )  {
			do {
				if (pe32.th32ProcessID == currentProcessID) {
					dumpFileName.Copy(pe32.szExeFile);
					break;
				}
			} while( Process32Next( hProcessSnap, &pe32 ) );
		}
		else {
			int err = GetLastError();
			printf("Process32First( hProcessSnap, &pe32 ) == NULL\n");
			printf("Process32First error : \"%s\"\n", strerror(err));
		}
	}
	else printf("hProcessSnap == INVALID_HANDLE_VALUE\n");
	aVect<char> suffix;

	suffix.sprintf("%s_%s", (char*)GetDateStr("%.2d%.2d%.2d"), (char*)GetTimeStr("%.2d%.2d%.2d"));

	if (!dumpFileName) 
		dumpFileName.sprintf("%s\\MiniDump%s.dmp", currentDirectory, (char*)suffix);
	else {
		dumpFileName.sprintf("%s\\%s%s.dmp", currentDirectory, (char*)dumpFileName, (char*)suffix);
	}

	HANDLE hFile = CreateFileA(dumpFileName, GENERIC_READ | GENERIC_WRITE, 
		0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL); 

	if (INVALID_HANDLE_VALUE == hFile) printf("Unable to create file \"%s\"\n", (char*)dumpFileName);

	MINIDUMP_TYPE mdt = (MINIDUMP_TYPE)(MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory); 
	
	struct _MINIDUMP_EXCEPTION_INFORMATION mei;

	mei.ClientPointers = 1;
	mei.ExceptionPointers = pEx;
	mei.ThreadId = threadID;

	MiniDumpWriteDumpInfo miniDumpWriteDumpInfo;

	miniDumpWriteDumpInfo.hProcess = GetCurrentProcess();
	miniDumpWriteDumpInfo.ProcessId = GetCurrentProcessId();
	miniDumpWriteDumpInfo.hFile = hFile;
	miniDumpWriteDumpInfo.DumpType = mdt;
	miniDumpWriteDumpInfo.ExceptionParam = (pEx) ? &mei : NULL;
	miniDumpWriteDumpInfo.UserStreamParam = NULL;
	miniDumpWriteDumpInfo.CallbackParam = NULL;

	if (hFile != INVALID_HANDLE_VALUE) {
		HANDLE hThread = SAFE_CALL(CreateThread(NULL,0,GenerateDump,&miniDumpWriteDumpInfo,NULL,NULL));
		WaitForSingleObject(hThread,INFINITE);
		CloseHandle(hFile);
		CloseHandle(hThread);
		printf("Dump file writed to:\n\"%s\"\n", (char*)dumpFileName);
		printf("You may send it to tristan.desrues@wanadoo.fr for debugging.\n");
	}

	sprintf/*sprintf_s*/(buffer,/*sizeof(buffer),*/"%s\n -Function : %s\n -File : %s\n -Line : %llu\n\n"
		"Please send following files to tristan.desrues@wanadoo.fr : \n\n"
		"%s\\%s\n\n"
		"%s\\%s",
		str, func, file, (unsigned long long)line, currentDirectory, (char*)dumpFileName, currentDirectory, errorFileName);

	//SetTimer(NULL, TimerId, 0, NULL);

	if (reportAndExit) {
		ReportErrorAndExit(buffer);
		ExitThread(false);
	}
	//int msgboxID = MessageBoxA(
 //       NULL,
 //       (LPCSTR) buffer,
 //       (LPCSTR) "Error",
 //       MB_ICONERROR | MB_SERVICE_NOTIFICATION
 //   );

	//if (msgboxID==0) {
	//	int err = GetLastError();
	//}

	//ExitProcess(666);

	preventReentrance--;
}

LONG WINAPI lpTopLevelExceptionFilter(struct _EXCEPTION_POINTERS * ExceptionInfo) {

	//MessageBox(NULL, "Attach!", "Attach!", 0);

	if (GetAncestor(GetConsoleWindow(), GA_PARENT)) {
		SetParent(GetConsoleWindow(), NULL);
		SetWindowPos(GetConsoleWindow(), NULL, 50, 50, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	}
	
	UNRECOVERABLE_ERROREX("Unhandled exception :(\nTerminating application...\n\n", ExceptionInfo);

	return EXCEPTION_EXECUTE_HANDLER;
}

bool TestDumpOnUnhandledException() {
	
	auto prev = SetUnhandledExceptionFilter(NULL);
	auto retVal = prev == lpTopLevelExceptionFilter;
	SetUnhandledExceptionFilter(prev);

	return retVal;
}

void SetDumpOnUnhandledException(bool enable) {
	SetUnhandledExceptionFilter(enable ? lpTopLevelExceptionFilter : NULL);
	if (SetUnhandledExceptionFilter(enable ? lpTopLevelExceptionFilter : NULL) != (enable ? lpTopLevelExceptionFilter : NULL)) {
		OutputDebugStringA("SetUnhandledExceptionFilter fail");
	}
}

const PROCESS_INFORMATION WinProcess::pi_zero;
const STARTUPINFOW WinProcess::si_zero;

//size_t WinConsole::refCount;
//WinProcess WinConsole::origConsoleHolder;
//WinCriticalSection WinConsole::criticalSection;
//DWORD WinConsole::currentConsoleProcessId;
//bool WinConsole::switchedMod;

//BOOL CALLBACK AutoFitWindowForChildrenEnumFunc(HWND hWnd, LPARAM lParam) {
//
//	RECT * pMaxRect = (RECT*)lParam;
//
//	RECT r;
//
//	SAFE_CALL(GetWindowRect(hWnd, &r));
//	
//	SetLastError(ERROR_SUCCESS);
//
//	SAFE_CALL_NO_ERROR_OK(MapWindowPoints(NULL, GetAncestor(hWnd, GA_PARENT), (LPPOINT)&r, 2));
//
//	if (r.right > pMaxRect->right) pMaxRect->right = r.right;
//	if (r.bottom > pMaxRect->bottom) pMaxRect->bottom = r.bottom;
//
//	return true;
//}
//
//
// DOES NOT WORK BECAUSE EnumChildWindows enumerates all descendants :(
//void AutoFitWindowForChildren(HWND hWnd) {
//
//	RECT maxRect;
//
//	maxRect.bottom = maxRect.left = maxRect.right = maxRect.top = 0;
//
//	EnumChildWindows(hWnd, AutoFitWindowForChildrenEnumFunc, (LPARAM)&maxRect);
//
//	SAFE_CALL(SetWindowPos(hWnd, NULL, 0, 0, maxRect.right, maxRect.bottom, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED));
//}

void RedrawWindow(HWND hWnd) {
	SAFE_CALL(RedrawWindow(hWnd, NULL, NULL, RDW_INTERNALPAINT | RDW_INVALIDATE | RDW_UPDATENOW));
}

void AutoFitWindowForChildren(HWND hWnd, bool vertical, bool horizontal) {

	MY_ASSERT(vertical || horizontal);

	RECT maxRect, origRect;

	maxRect.bottom = maxRect.left = maxRect.right = maxRect.top = 0;

	HWND hWndChild = NULL;
	SAFE_CALL(GetWindowRect(hWnd, &origRect));

	while (true)  {
		SetLastError(ERROR_SUCCESS);
		hWndChild =	SAFE_CALL_NO_ERROR_OK(FindWindowEx(hWnd, hWndChild, NULL, NULL));

		if (!hWndChild) break;

		RECT r;

		SAFE_CALL(GetWindowRect(hWndChild, &r));
			
		SetLastError(ERROR_SUCCESS);
		
		SAFE_CALL_NO_ERROR_OK(MapWindowPoints(NULL, hWnd, (LPPOINT)&r, 2));
		
		if (r.right > maxRect.right) maxRect.right = r.right;
		if (r.bottom > maxRect.bottom) maxRect.bottom = r.bottom;
	}

	SIZE sz = GetNonClientAdditionalSize(hWnd);

	SAFE_CALL(SetWindowPos(hWnd, NULL, 0, 0, 
		horizontal ? maxRect.right + sz.cx : origRect.right - origRect.left, 
		vertical ?  maxRect.bottom + sz.cy : origRect.bottom - origRect.top,
		SWP_NOMOVE | SWP_NOZORDER));

	RedrawWindow(hWnd);
}


DWORD WINAPI PathWatcher(LPVOID lpParameter) {

	FolderWatcher * fw = (FolderWatcher*)lpParameter;
	FolderWatcherCore folderWatcherCore(fw);
	mVect<FileEvent> * pFileEvent = &folderWatcherCore.fileEvents;

	aVect<char> bufferVect(63999); //64 kB limit on network
	char * buffer = bufferVect;
	DWORD bytesReturned;
	//aVect<char> unicodeFileName;
	aVect<wchar_t> fileName;
	FileEvent fe;

	unsigned long long eventNumber = fw->eventNumberStart + 1;

	PerformanceChronometer chronoRetry;

	while (true) {

		chronoRetry.Stop();

		SAFE_CALL(ReadDirectoryChangesW(folderWatcherCore.folder, buffer, (DWORD)bufferVect.Count(), TRUE,
			FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
			FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_LAST_ACCESS | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_SECURITY,
			&bytesReturned, NULL, NULL));

		if (!bytesReturned) {
			//if (GetLastError() == ERROR_SUCCESS) {
				//auto str = xFormat(L"Elapsed \B5s: %g", chronoRetry.GetMicroSeconds());
				//wchar_t locBuf[1024];
				//GetModuleFileNameW(NULL, locBuf, sizeof locBuf);
				//SplitPathW(locBuf, nullptr, nullptr, &fileName, nullptr);
				//MessageBoxW(NULL, L"Missed File Events!", fileName, MB_SERVICE_NOTIFICATION);
			fw->missedEvents = true;
			//}
			//else {
				//MY_ERROR(GetErrorDescription(GetLastError()));
			//}
		}

		chronoRetry.Start();

		FILE_NOTIFY_INFORMATION * pFni = (FILE_NOTIFY_INFORMATION*)buffer;

		while (true) {

			if (folderWatcherCore.closeThread.IsSignaled()) return 0;

			fileName.Redim(pFni->FileNameLength/2 + 1);
			fileName.Last() = 0;

			memcpy(fileName, pFni->FileName, pFni->FileNameLength);

			//unicodeFileName.Redim(pFni->FileNameLength + 2);
			//unicodeFileName.Last() = unicodeFileName.Last(-1) = 0;

			//memcpy(unicodeFileName, pFni->FileName, pFni->FileNameLength);

			//int requiredSize = SAFE_CALL(WideCharToMultiByte(CP_ACP, 0,
			//	(LPCWCH)(char*)unicodeFileName, -1, fileName, 0, "?", NULL));

			//fileName.Redim(requiredSize + 1);

			//SAFE_CALL(WideCharToMultiByte(CP_ACP, 0, (LPCWCH)(char*)unicodeFileName,
			//	-1, fileName, requiredSize, "?", NULL));

			//fileName.Last() = 0;

			EnterCriticalSection(folderWatcherCore.pCriticalSection);
			{
				folderWatcherCore.newChanges.Set();
				//if (pFileEvent->Count() == 1 && pFileEvent->AllocatedCount() == 1) __debugbreak();
				pFileEvent->Push(std::move(fileName), pFni->Action, time(nullptr), eventNumber++, 0);
			}
			LeaveCriticalSection(folderWatcherCore.pCriticalSection);

			if (pFni->NextEntryOffset) pFni = (FILE_NOTIFY_INFORMATION*)((char*)pFni + pFni->NextEntryOffset);
			else break;
		}
	}

	return 0;
}

DWORD IsDirectory(const wchar_t * dir) {

	DWORD attrib = GetFileAttributesW(dir);
	if (attrib == INVALID_FILE_ATTRIBUTES) return 0;
	return (attrib & FILE_ATTRIBUTE_DIRECTORY);
}

DWORD IsDirectory(const char * dir) {

	return IsDirectory(aVect<wchar_t>(dir));
}

DWORD IsDirectory(DWORD attrib) {

	if (attrib == INVALID_FILE_ATTRIBUTES) return 0;
	return (attrib & FILE_ATTRIBUTE_DIRECTORY);
}

void DeleteDirectory(wchar_t * directory) {
	
	DWORD attrib = GetFileAttributesW(directory);
	if (attrib == INVALID_FILE_ATTRIBUTES) {
		MY_ERROR(aVect<char>("Cannot delete directory \"%S\" : doesn't exist", directory));
		return;
	}
	if (!(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
		MY_ERROR(aVect<char>("Cannot delete directory \"%S\" : is not a directory", directory));
		return;
	}
	if (attrib & FILE_ATTRIBUTE_READONLY) {
		attrib &= ~FILE_ATTRIBUTE_READONLY;
		SAFE_CALL(SetFileAttributesW(directory, attrib));
	}

	aVect<wchar_t> deleteFromStr(L"%s\\*", directory);
	deleteFromStr.Grow(1)[wcslen(deleteFromStr) + 1] = 0; // double null terminate
	SHFILEOPSTRUCTW sDelete = { 0 };
	sDelete.hwnd = NULL;
	sDelete.wFunc = FO_DELETE;
	sDelete.fFlags = FOF_NOCONFIRMATION/* | FOF_SILENT*/;
	sDelete.pTo = NULL;
	sDelete.pFrom = deleteFromStr;
	int result = SHFileOperationW(&sDelete);
	if (result/* && result!=2*/) MY_ERROR(aVect<char>("SHFileOperation delete operation failed with error code : %d", result));

	SAFE_CALL(RemoveDirectoryW(directory));
}

HICON CreateIconFromFile(const wchar_t * file) {

	HBITMAP hBitmap = (HBITMAP)LoadImageW(GetCurrentModuleHandle(),
		file, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

	if (!hBitmap) return NULL;

	BITMAP bm = { 0 };
	GetObject(hBitmap, sizeof(bm), &bm);

	HDC memdc = CreateCompatibleDC(NULL);
	HBITMAP mask = CreateCompatibleBitmap(memdc, bm.bmWidth, bm.bmHeight);

	ICONINFO ii;
	ii.fIcon = TRUE;
	ii.hbmMask = mask;
	ii.hbmColor = hBitmap;
	HICON hIcon = CreateIconIndirect(&ii);

	DeleteDC(memdc);
	DeleteObject(mask);
	DeleteObject(hBitmap);

	return hIcon;
}

HICON CreateIconFromFile(const char * file) {
	return CreateIconFromFile(aVect<wchar_t>(file));
}

HBRUSH CreateCheckerBoardBrush(bool coarse) {

	char whatever = 42;

	char pat_coarse[] = { (char)0x30, whatever, (char)0x30, whatever, (char)0xC0, whatever, (char)0xC0, whatever };
	char pat_fine[] = { (char)0xAA, (char)0x0, (char)0x44, (char)0x0 };

	auto&&  pat = coarse ? pat_coarse : pat_fine;
	int size = coarse ? 4 : 2;

	auto hBitmap = SAFE_CALL(CreateBitmap(size, size, 1, 1, pat));

	auto hBrush = SAFE_CALL(CreatePatternBrush(hBitmap));

	DeleteObject(hBitmap);

	return hBrush;
}

void MySetConsoleSize(int scBufSizeX, int scBufSizeY, int winVisBufX, int winPix_dx, int winPix_dy) {

	COORD dwSize;
	dwSize.X = scBufSizeX;
	dwSize.Y = scBufSizeY;

	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	HWND hConsoldeWindow = GetConsoleWindow();
	SetWindowPos(hConsoldeWindow, NULL, winPix_dx, winPix_dy, 0, 0, SWP_NOMOVE | SWP_NOZORDER);
	SetConsoleScreenBufferSize(hConsole, dwSize);

	SMALL_RECT sr;
	sr.Top = 0;
	sr.Left = 0;
	sr.Bottom = winVisBufX;
	sr.Right = sr.Left + dwSize.X - 1;

	SetConsoleWindowInfo(hConsole, true, &sr);
	ShowWindow(hConsoldeWindow, SW_MAXIMIZE);
}

VerboseLevelGuard::VerboseLevelGuard(
	OutputInfo & outputInfo,
	long scopeVerboseLevel)
	:
	outputInfo(outputInfo)
{
	this->previousVerboseLevel = outputInfo.GetVerboseLevel_Input();
	outputInfo.SetVerboseLevel_Input(scopeVerboseLevel);
}

VerboseLevelGuard::~VerboseLevelGuard() {
	outputInfo.SetVerboseLevel_Input(this->previousVerboseLevel);
}



char * strstr_caseInsensitive(const char * str, const char * subStr) {

	if (!str || !subStr) return nullptr;

	static WinCriticalSection cs;

	Critical_Section(cs) {
		static aVect<char> str_lowercase;
		static aVect<char> subStr_lowercase;

		str_lowercase.Redim(strlen(str) + 1);
		subStr_lowercase.Redim(strlen(subStr) + 1);

		aVect_static_for(str_lowercase, i) str_lowercase[i] = tolower(str[i]);
		aVect_static_for(subStr_lowercase, i) subStr_lowercase[i] = tolower(subStr[i]);

		char * ptr = strstr(str_lowercase, subStr_lowercase);

		if (!ptr) return nullptr;

		ptrdiff_t offset = ptr - (char*)str_lowercase;

		return (char*)str + offset;
	}

	MY_ERROR("Unreachable code");
	return nullptr;
}


size_t ReplaceStr(
	aVect<char> & str,
	char * subStr,
	char * replaceStr,
	bool onlyFirstOccurence,
	bool caseSensitive,
	bool reverse,
	size_t startFrom)
{

	size_t strLen = strlen(str);
	size_t subStrLen = strlen(subStr);
	size_t replaceStrLen = strlen(replaceStr);

	ptrdiff_t grow = replaceStrLen;
	grow -= subStrLen; // because if (replaceStrLen - subStrLen) is negative the result is (size_t)-1 = 4294967295 :(

	char * strEnd = (char*)str + strLen;
	char * subStrEnd = subStr + subStrLen;
	char * replaceStrEnd = replaceStr + replaceStrLen;

	if (startFrom > strLen) return 0;

	size_t handledChars = startFrom;

	while (true) {
		char * foundSubStr = caseSensitive ?
			(reverse ?
			reverse_strstr((char*)str - handledChars, subStr)
			:
			strstr((char*)str + handledChars, subStr))
			:
			(reverse ?
			reverse_strstr_caseInsensitive((char*)str - handledChars, subStr)
			:
			strstr_caseInsensitive((char*)str + handledChars, subStr));

		if (!foundSubStr) return handledChars == startFrom ? 0 : handledChars;

		//First we must move not-to-be-erased data
		if (grow > 0) {// if grow > 0, grow before moving data
			char * oldPtr = str;
			str.Grow(grow);
			if (str != oldPtr) {//str might have been reallocted at a new address
				ptrdiff_t diff = str - oldPtr;
				strEnd += diff;
				foundSubStr += diff;
			}//move data
			for (char * ptr = strEnd; ptr >= foundSubStr + subStrLen; --ptr) {
				*(ptr + grow) = *ptr;
			}

		}
		else if (grow < 0) {// if grow < 0, move data from begin to end
			for (char * ptr = foundSubStr + subStrLen; ptr <= strEnd; ++ptr) {
				*(ptr + grow) = *ptr;
			}
			char * oldPtr = str;
			str.Grow(grow); // then shrink after moving data
			if (str != oldPtr) {//str might have been reallocted at a new address
				ptrdiff_t diff = str - oldPtr;
				strEnd += diff;
			}
		}
		strEnd += grow;

		handledChars = reverse ? strEnd - foundSubStr : foundSubStr - str + replaceStrLen;

		//Then we put replaceStr
		for (size_t i = 0; i<replaceStrLen; ++i) {
			foundSubStr[i] = replaceStr[i];
		}

		if (onlyFirstOccurence) return handledChars;
	}
}

size_t ReplaceStr_CaseInsensitive(aVect<char> & str, char * subStr, char * replaceStr, bool onlyFirstOccurence, size_t startFrom) {
	return ReplaceStr(str, subStr, replaceStr, onlyFirstOccurence, false, false, startFrom);
}

size_t ReplaceStr_FirstOccurence(aVect<char> & str, char * subStr, char * replaceStr, bool caseSensitive, size_t startFrom) {
	return ReplaceStr(str, subStr, replaceStr, true, caseSensitive, false, startFrom);
}

size_t ReplaceStr_LastOccurence(aVect<char> & str, char * subStr, char * replaceStr, bool caseSensitive, size_t startFrom) {
	return ReplaceStr(str, subStr, replaceStr, true, caseSensitive, true, startFrom);
}

size_t ReplaceStr_FirstOccurence_CaseInsensitive(aVect<char> & str, char * subStr, char * replaceStr, size_t startFrom) {
	return ReplaceStr(str, subStr, replaceStr, true, false, false, startFrom);
}

size_t ReplaceStr_LastOccurence_CaseInsensitive(aVect<char> & str, char * subStr, char * replaceStr, size_t startFrom) {
	return ReplaceStr(str, subStr, replaceStr, true, false, true, startFrom);
}

aVect<char> ReplaceStr(
	char * str,
	char * subStr,
	char * replaceStr,
	bool onlyFirstOccurence,
	bool caseSensitive,
	bool reverse,
	size_t startFrom) {

	aVect<char> retVal(str);
	ReplaceStr(retVal, subStr, replaceStr, onlyFirstOccurence, caseSensitive, reverse, startFrom);
	return std::move(retVal);
}

aVect<char> ReplaceStr_CaseInsensitive(char * str, char * subStr, char * replaceStr, bool onlyFirstOccurence, bool reverse, size_t startFrom) {
	return ReplaceStr(str, subStr, replaceStr, onlyFirstOccurence, false, reverse, startFrom);
}

aVect<char> ReplaceStr_FirstOccurence(char * str, char * subStr, char * replaceStr, bool caseSensitive, size_t startFrom) {
	return ReplaceStr(str, subStr, replaceStr, true, caseSensitive, false, startFrom);
}

aVect<char> ReplaceStr_LastOccurence(char * str, char * subStr, char * replaceStr, bool caseSensitive, size_t startFrom) {
	return ReplaceStr(str, subStr, replaceStr, true, caseSensitive, true, startFrom);
}

aVect<char> ReplaceStr_FirstOccurence_CaseInsensitive(char * str, char * subStr, char * replaceStr, size_t startFrom) {
	return ReplaceStr(str, subStr, replaceStr, true, false, false, startFrom);
}

aVect<char> ReplaceStr_LastOccurence_CaseInsensitive(char * str, char * subStr, char * replaceStr, size_t startFrom) {
	return ReplaceStr(str, subStr, replaceStr, true, false, true, startFrom);
}

char * reverse_strstr(char * str, char * subStr, bool caseSensitive) {

	char * lastPtr = caseSensitive ? strstr(str, subStr) : strstr_caseInsensitive(str, subStr);

	if (!lastPtr) return lastPtr;

	while (true) {
		char* newPtr = caseSensitive ? strstr(lastPtr + 1, subStr) : strstr_caseInsensitive(lastPtr + 1, subStr);
		if (newPtr) lastPtr = newPtr;
		else break;
	}

	return lastPtr;
}

char * reverse_strstr_caseInsensitive(char * str, char * subStr) {
	return reverse_strstr(str, subStr, false);
}


int strcmp_caseInsensitive(const char * str1, const char * str2) {

	static WinCriticalSection cs;

	Critical_Section(cs) {
		static aVect<char> a;
		static aVect<char> b;

		//aVect<char> a;
		//aVect<char> b;

		if (str1) {
			size_t count_str1 = strlen(str1) + 1;
			if (count_str1 > a.Count()) a.Redim(count_str1);
			for (size_t i = 0; i<count_str1; ++i) a[i] = tolower(str1[i]);
		}
		else {
			if (a.Count() < 1) a.Redim(1);
			a[0] = 0;
		}


		if (str2) {
			size_t count_str2 = strlen(str2) + 1;
			if (count_str2 > b.Count()) b.Redim(count_str2);
			for (size_t i = 0; i<count_str2; ++i) b[i] = tolower(str2[i]);
		}
		else {
			if (b.Count() < 1) a.Redim(1); b.Redim(1);
			b[0] = 0;
		}

		return strcmp(a, b);
	}

	MY_ERROR("unreachable code");
	return 0;
}

char * reverse_strchr(char * str, char chr) {

	char * lastPtr = strchr(str, chr);

	if (!lastPtr) return lastPtr;

	for (;;) {
		char* newPtr = strchr(lastPtr + 1, chr);
		if (newPtr) lastPtr = newPtr;
		else break;
	}

	return lastPtr;
}

//from stackoverflow
DWORD FindProcessId(char* processName) {

	// strip path
	char* p = strrchr(processName, '\\');
	
	if (p) processName = p + 1;

	PROCESSENTRY32 processInfo;
	processInfo.dwSize = sizeof(processInfo);

	HANDLE processesSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

	if (processesSnapshot == INVALID_HANDLE_VALUE) return 0;

	Process32First(processesSnapshot, &processInfo);

	if (strcmp_caseInsensitive(processName, processInfo.szExeFile) == 0) {
		CloseHandle(processesSnapshot);
		return processInfo.th32ProcessID;
	}

	while (Process32Next(processesSnapshot, &processInfo)) {
		if (strcmp_caseInsensitive(processName, processInfo.szExeFile) == 0) {
			CloseHandle(processesSnapshot);
			return processInfo.th32ProcessID;
		}
	}

	CloseHandle(processesSnapshot);
	return 0;
}

aVect<wchar_t> BasicFileDialog(
	const wchar_t * defaultPath,
	bool save, bool pickFolder,
	const aVect<COMDLG_FILTERSPEC> & c_rgSaveTypes,
	const wchar_t * title)
{

	aVect<wchar_t> retVal;

	CoInitialize(NULL);

	IFileDialog *pfd = NULL;

	HRESULT hr = CoCreateInstance(save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog,
		NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));

	if (FAILED(hr)) return retVal;
		
	DWORD dwFlags;

	hr = pfd->GetOptions(&dwFlags);

	if (SUCCEEDED(hr)) {

		auto newFlags = dwFlags | FOS_FORCEFILESYSTEM;
		if (pickFolder) newFlags |= FOS_PICKFOLDERS;

		hr = pfd->SetOptions(newFlags);

		if (SUCCEEDED(hr)) {

			if (!pickFolder) hr = pfd->SetFileTypes((UINT)c_rgSaveTypes.Count(), c_rgSaveTypes);

			if (SUCCEEDED(hr)) {

				if (defaultPath) {

					wchar_t * path = nullptr;
					wchar_t * fileName = nullptr;

					if (!IsDirectory(defaultPath)) {
						SplitPathW(defaultPath, nullptr, &path, &fileName, nullptr);
					} else {
						path = (wchar_t*)defaultPath;
					}

					if (fileName) pfd->SetFileName(fileName);

					if (path) {
						IShellItem * pShellItem = NULL;

						hr = SHCreateItemFromParsingName(path, NULL, IID_PPV_ARGS(&pShellItem));

						if (SUCCEEDED(hr)) {
							hr = pfd->SetFolder(pShellItem);
							pShellItem->Release();
						}
					}
				}

				if (title) pfd->SetTitle(title);

				hr = pfd->Show(NULL);

				if (SUCCEEDED(hr)) {

					IShellItem *psiResult;
					hr = pfd->GetResult(&psiResult);
					if (SUCCEEDED(hr)) {
						PWSTR pszFilePath = NULL;
						hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

						retVal = pszFilePath;
						CoTaskMemFree(pszFilePath);
						psiResult->Release();
					}
				}
			}
		}
	}

	pfd->Release();
	
	return retVal;
}

aVect<wchar_t> BasicFileDialogWrapper(
	const wchar_t * typeExtension,
	const wchar_t * typeName,
	const wchar_t * defaultPath,
	bool save, bool pickFolder,
	const wchar_t * title)
{

	if (!typeExtension && !typeName) {
		typeExtension = L"*.*";
		typeName = L"All files";
	}
	else if (typeExtension && !typeName) {
		aVect<wchar_t> buf = typeExtension;
		WCharPointer ptr = buf;
		RetrieveStr<'.'>(ptr);
		typeName = xFormat(L"%s files (%s)", ptr, buf);
	}

	aVect<COMDLG_FILTERSPEC> filters(1);

	filters[0].pszName = typeName;
	filters[0].pszSpec = typeExtension;

	return BasicFileDialog(defaultPath, save, pickFolder, filters, title);
}

aVect<wchar_t> FileSaveDialog(
	const wchar_t * typeExtension,
	const wchar_t * typeName,
	const wchar_t * defaultPath,
	const wchar_t * title)
{
	return BasicFileDialogWrapper(typeExtension, typeName, defaultPath, true, false, title);
}

aVect<wchar_t> FileOpenDialog(
	const wchar_t * typeExtension,
	const wchar_t * typeName,
	const wchar_t * defaultPath,
	const wchar_t * title)
{
	return BasicFileDialogWrapper(typeExtension, typeName, defaultPath, false, false, title);
}

aVect<wchar_t> BrowseForFolder(const wchar_t * defaultPath, const wchar_t * title) {
	return BasicFileDialogWrapper(nullptr, nullptr, defaultPath, false, true, title);
}

aVect<char> FileSaveDialogA(
	const char * typeExtension,
	const char * typeName,
	const char * defaultPath,
	const wchar_t * title)
{
	return aVect<char>(FileSaveDialog(
		aVect<wchar_t>(typeExtension),
		aVect<wchar_t>(typeName), 
		aVect<wchar_t>(defaultPath),
		aVect<wchar_t>(title)));
}

aVect<char> FileSaveDialog(
	const char * typeExtension,
	const char * typeName,
	const char * defaultPath,
	const char * title)
{
	return aVect<char>(FileSaveDialog(
		aVect<wchar_t>(typeExtension), 
		aVect<wchar_t>(typeName), 
		aVect<wchar_t>(defaultPath),
		aVect<wchar_t>(title)));
}

aVect<char> FileOpenDialog(
	const char * typeExtension,
	const char * typeName,
	const char * defaultPath,
	const char * title)
{
	return aVect<char>(FileOpenDialog(
		aVect<wchar_t>(typeExtension), 
		aVect<wchar_t>(typeName),
		aVect<wchar_t>(defaultPath),
		aVect<wchar_t>(title)));
}

aVect<char> FileOpenDialogA(
	const char * typeExtension,
	const char * typeName,
	const char * defaultPath,
	const char * title)
{
	return aVect<char>(FileOpenDialog(
		aVect<wchar_t>(typeExtension),
		aVect<wchar_t>(typeName), 
		aVect<wchar_t>(defaultPath),
		aVect<wchar_t>(title)));
}

aVect<char> BrowseForFolder(const char * defaultPath, const char * title) {
	return aVect<char>(BasicFileDialogWrapper(
		nullptr, nullptr, aVect<wchar_t>(defaultPath), false, true, aVect<wchar_t>(title)));
}

aVect<char> BrowseForFolderA(const char * defaultPath, const char * title) {
	return aVect<char>(BasicFileDialogWrapper(
		nullptr, nullptr, aVect<wchar_t>(defaultPath), false, true, aVect<wchar_t>(title)));
}


//From MSDN
PBITMAPINFO CreateBitmapInfoStruct(HBITMAP hBmp) {

	BITMAP bmp;
	PBITMAPINFO pbmi;
	WORD    cClrBits;

	// Retrieve the bitmap color format, width, and height.  
	if (!GetObject(hBmp, sizeof(BITMAP), (LPSTR)&bmp)) MY_ERROR("GetObject");

	// Convert the color format to a count of bits.  
	cClrBits = (WORD)(bmp.bmPlanes * bmp.bmBitsPixel);

	if (cClrBits == 1)       cClrBits = 1;
	else if (cClrBits <= 4)  cClrBits = 4;
	else if (cClrBits <= 8)  cClrBits = 8;
	else if (cClrBits <= 16) cClrBits = 16;
	else if (cClrBits <= 24) cClrBits = 24;
	else					 cClrBits = 32;

	// Allocate memory for the BITMAPINFO structure. (This structure  
	// contains a BITMAPINFOHEADER structure and an array of RGBQUAD  
	// data structures.)  

	if (cClrBits < 24) {
		// There is no RGBQUAD array for these formats: 24-bit-per-pixel or 32-bit-per-pixel 
		pbmi = (PBITMAPINFO)LocalAlloc(LPTR, (int)sizeof(BITMAPINFOHEADER) + (int)sizeof(RGBQUAD) * (1 << cClrBits));
	}
	else {
		pbmi = (PBITMAPINFO)LocalAlloc(LPTR, sizeof(BITMAPINFOHEADER));
	}

	// Initialize the fields in the BITMAPINFO structure.  
	pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pbmi->bmiHeader.biWidth = bmp.bmWidth;
	pbmi->bmiHeader.biHeight = bmp.bmHeight;
	pbmi->bmiHeader.biPlanes = bmp.bmPlanes;
	pbmi->bmiHeader.biBitCount = bmp.bmBitsPixel;

	if (cClrBits < 24) pbmi->bmiHeader.biClrUsed = (1 << cClrBits);

	// If the bitmap is not compressed, set the BI_RGB flag.  
	pbmi->bmiHeader.biCompression = BI_RGB;

	// Compute the number of bytes in the array of color  
	// indices and store the result in biSizeImage.  
	// The width must be DWORD aligned unless the bitmap is RLE 
	// compressed. 
	pbmi->bmiHeader.biSizeImage = ((pbmi->bmiHeader.biWidth * cClrBits + 31) & ~31) / 8 * pbmi->bmiHeader.biHeight;

	// Set biClrImportant to 0, indicating that all of the  
	// device colors are important.  
	pbmi->bmiHeader.biClrImportant = 0;

	return pbmi;
}

//From MSDN
void CreateBMPFile(const wchar_t * pszFile, PBITMAPINFO pbi, HBITMAP hBMP, HDC hDC) {

	HANDLE hf;                  // file handle  
	BITMAPFILEHEADER hdr;       // bitmap file-header  
	PBITMAPINFOHEADER pbih;     // bitmap info-header  
	LPBYTE lpBits;              // memory pointer  
	DWORD dwTotal;              // total count of bytes  
	DWORD cb;                   // incremental count of bytes  
	BYTE *hp;                   // byte pointer  
	DWORD dwTmp;

	pbih = (PBITMAPINFOHEADER)pbi;
	lpBits = (LPBYTE)GlobalAlloc(GMEM_FIXED, pbih->biSizeImage);

	if (!lpBits) MY_ERROR("GlobalAlloc");

	// Retrieve the color table (RGBQUAD array) and the bits  
	// (array of palette indices) from the DIB.  
	if (!GetDIBits(hDC, hBMP, 0, (WORD)pbih->biHeight, lpBits, pbi, DIB_RGB_COLORS)) {
		MY_ERROR("GetDIBits");
	}

	// Create the .BMP file.  
	hf = CreateFileW(pszFile, GENERIC_READ | GENERIC_WRITE, (DWORD)0,
		NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, (HANDLE)NULL);

	if (hf == INVALID_HANDLE_VALUE) MY_ERROR("CreateFile");

	hdr.bfType = 0x4d42;        // 0x42 = "B" 0x4d = "M"  

	// Compute the size of the entire file.  
	hdr.bfSize = (DWORD)(sizeof(BITMAPFILEHEADER) + pbih->biSize + pbih->biClrUsed * sizeof(RGBQUAD) + pbih->biSizeImage);

	hdr.bfReserved1 = 0;
	hdr.bfReserved2 = 0;

	// Compute the offset to the array of color indices.  
	hdr.bfOffBits = (DWORD) sizeof(BITMAPFILEHEADER) + pbih->biSize + pbih->biClrUsed * sizeof(RGBQUAD);

	// Copy the BITMAPFILEHEADER into the .BMP file.  
	if (!WriteFile(hf, (LPVOID)&hdr, sizeof(BITMAPFILEHEADER), (LPDWORD)&dwTmp, NULL)) {
		MY_ERROR("WriteFile");
	}

	// Copy the BITMAPINFOHEADER and RGBQUAD array into the file.  
	if (!WriteFile(hf, (LPVOID)pbih, sizeof(BITMAPINFOHEADER) + pbih->biClrUsed * sizeof(RGBQUAD), (LPDWORD)&dwTmp, (NULL))) {
		MY_ERROR("WriteFile");
	}

	// Copy the array of color indices into the .BMP file.  
	dwTotal = cb = pbih->biSizeImage;
	hp = lpBits;

	if (!WriteFile(hf, (LPSTR)hp, (int)cb, (LPDWORD)&dwTmp, NULL)) MY_ERROR("WriteFile");

	// Close the .BMP file.  
	if (!CloseHandle(hf)) MY_ERROR("CloseHandle");

	// Free memory.  
	GlobalFree((HGLOBAL)lpBits);
}

 LSTATUS GetRegistry(aVect<wchar_t> & value, HKEY key, const wchar_t * path, const wchar_t * valueName) {

	 HKEY hKey;
	 value.Erase();
	 
	 auto r = RegOpenKeyW(key, path, &hKey);
	 if (r == ERROR_SUCCESS) {

		 for (;;) {
			 value.Grow(5000);
			 DWORD sizeof_sResult = (DWORD)(sizeof(wchar_t)*(value.Count() - 1));
			 DWORD valueType;

			 r = RegQueryValueExW(hKey, valueName, 0, &valueType, (BYTE*)value.GetDataPtr(), &sizeof_sResult);

			 if (r == ERROR_SUCCESS) {
				 auto ind = sizeof_sResult / sizeof(wchar_t);
				 value[ind] = 0;
				 value.RedimToStrlen();
				 break;
			 } else if (r == ERROR_MORE_DATA) {
				 continue;
			 } else {
				 value.Redim(0);
				 break;
			 }
		 }

		 RegCloseKey(hKey);
	 }

	 return r;
}

aVect<wchar_t> GetRegistry(HKEY key, const wchar_t * path, const wchar_t * valueName) {

	aVect<wchar_t> retVal;

	GetRegistry(retVal, key, path, valueName);

	return retVal;
}

bool WriteRegistry(HKEY key, const wchar_t * path, const wchar_t * valueName, const wchar_t * value, LSTATUS * pStatus) {

	HKEY hKey;
	bool retVal = false;
	auto ok = RegCreateKeyW(key, path, &hKey);

	if (ok == ERROR_SUCCESS) {
		ok = RegSetValueExW(hKey, valueName, 0, 1, (BYTE*)value, (DWORD)(sizeof(wchar_t)*(wcslen(value) + 1)));
		if (ok == ERROR_SUCCESS) {
			retVal = true;
		}

		RegCloseKey(hKey);
	}

	if (pStatus) *pStatus = ok;
	return retVal;
}

aVect<char> GetRegistry(HKEY key, const char * path, const char * valueName) {
	return aVect<char>(GetRegistry(key, aVect<wchar_t>(path), aVect<wchar_t>(valueName)));
}

bool WriteRegistry(HKEY key, const char * path, const char * valueName, const char * value, LSTATUS * pStatus) {
	return WriteRegistry(key, aVect<wchar_t>(path), aVect<wchar_t>(valueName), aVect<wchar_t>(value), pStatus);
}

HTREEITEM MyTreeView_InsertItem(
	HWND hTree,
	HTREEITEM hParent,
	HTREEITEM hInsertAfter,
	const wchar_t * str,
	void * param)
{

	auto ins = TV_INSERTSTRUCTW();

	ins.hParent = hParent;
	ins.hInsertAfter = hInsertAfter;

	ins.item.mask = TVIF_TEXT | TVIF_PARAM;

	if (!str) str = L"";

	ins.item.pszText = (wchar_t*)str;
	ins.item.cchTextMax = (int)wcslen(str) + 1;
	ins.item.lParam = (LPARAM)param;

	return (HTREEITEM)SendMessageW(hTree, TVM_INSERTITEMW, 0, (LPARAM)&ins);
}

void MyTreeView_RemoveCheckBox(HWND hTreeView, HTREEITEM hNode) {

	TVITEMW tvi;
	tvi.hItem = hNode;
	tvi.mask = TVIF_STATE;
	tvi.stateMask = TVIS_STATEIMAGEMASK;
	tvi.state = 0;

	SAFE_CALL(SendMessageW(hTreeView, TVM_SETITEM, NULL, (LPARAM)&tvi));
}


void * MyTreeView_GetItemParam(HWND hWnd, HTREEITEM hItem) {

	TVITEMW tvItem = {};
	tvItem.hItem = hItem;
	tvItem.mask |= TVIF_PARAM;
	SAFE_CALL(SendMessageW(hWnd, TVM_GETITEMW, 0, (LPARAM)&tvItem));

	return (void*)tvItem.lParam;
}

const wchar_t * MyTreeView_GetItemText(aVect<wchar_t>& buf, HWND hWnd, HTREEITEM hItem) {

	TVITEMW tvItem = {};
	tvItem.hItem = hItem;
	tvItem.mask |= TVIF_TEXT;

	if (buf.Count() < 1) buf.Redim(256);

	for (;;) {
		tvItem.cchTextMax = (int)buf.Count();
		tvItem.pszText = buf;
		SAFE_CALL(SendMessageW(hWnd, TVM_GETITEMW, 0, (LPARAM)&tvItem));

		//if (tvItem.pszText != buf) {
		//	
		//}

		if ((int)wcslen(tvItem.pszText) < tvItem.cchTextMax - 1) return tvItem.pszText;
		buf.Grow(tvItem.cchTextMax);
	}
}

aVect<wchar_t> MyTreeView_GetItemText(HWND hWnd, HTREEITEM hItem) {
	aVect<wchar_t> retVal(256);
	auto test = MyTreeView_GetItemText(retVal, hWnd, hItem);
	if (test != retVal) retVal = test;
	return std::move(retVal);
}


//From internet
SIZE_T GetLargestFreeContinuousBlock(LPVOID *lpBaseAddr) {
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	VOID *p = 0;
	MEMORY_BASIC_INFORMATION mbi;
	SIZE_T largestSize = 0;
	while (p < systemInfo.lpMaximumApplicationAddress) {
		SIZE_T dwRet = VirtualQuery(p, &mbi, sizeof(mbi));
		if (dwRet > 0) {
			if (mbi.State == MEM_FREE) {
				if (largestSize < mbi.RegionSize) {
					largestSize = mbi.RegionSize;
					if (lpBaseAddr) *lpBaseAddr = mbi.BaseAddress;
				}
			}
			p = (void*)(((char*)p) + mbi.RegionSize);
		} else {
			p = (void*)(((char*)p) + systemInfo.dwPageSize);
		}
	}
	return largestSize;
}

aVect<wchar_t> GetCurrentProcessImagePath() {

	aVect<wchar_t> exePath(500);

	for (;;) {
		auto ans = GetModuleFileNameW(GetModuleHandleW(NULL), exePath, (DWORD)exePath.Count());
		if (ans < exePath.Count() - 1) break;
		exePath.Redim(2 * exePath.Count());
	}

	return exePath;
}


size_t GetCurrentProcessPrivateBytes() {

	PROCESS_MEMORY_COUNTERS_EX pmc;

	auto ok = GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&pmc, sizeof pmc);

	if (ok) return pmc.PrivateUsage;
	else return 0;
}

bool IsNetworkPath(const wchar_t * path) {

	if (!path) return false;

	static WinCriticalSection cs;
	ScopeCriticalSection<WinCriticalSection> guard(cs);

	wchar_t * drive;
	SplitPathW(path, &drive, nullptr, nullptr, nullptr);
	return GetDriveTypeW(drive) == DRIVE_REMOTE;

}

bool IsNetworkPath(const char * path) {
	return IsNetworkPath(Wide(path));
}

BOOL MessagePump() {

	MSG msg;
	BOOL bRet;

	while (bRet = GetMessageW(&msg, NULL, 0, 0)) {
		if (bRet == -1) {
			break;
		} else {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	return bRet;
}

bool FileExists(const wchar_t * szPath) {
	DWORD dwAttrib = GetFileAttributesW(szPath);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool FileExists(const char * szPath) {
	DWORD dwAttrib = GetFileAttributesA(szPath);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

void MyGetWindowText(aVect<wchar_t> & str, HWND hWnd) {
	auto l = GetWindowTextLengthW(hWnd) + 5;
	if (l) {
		str.Redim(l);
		GetWindowTextW(hWnd, str.GetDataPtr(), l);
	} else {
		str.Erase();
	}
}

aVect<wchar_t> MyGetWindowText(HWND hWnd) {
	aVect<wchar_t> retVal;
	MyGetWindowText(retVal, hWnd);
	return retVal;
}

void ListBox_MyGetText(aVect<wchar_t> str, HWND hListBox) {
	str.Erase();
	auto curSel = ListBox_GetCurSel(hListBox);
	if (curSel == LB_ERR) return;
	auto count = ListBox_GetTextLen(hListBox, curSel) + 5;
	str.Redim(count);
	ListBox_GetText(hListBox, curSel, str.GetDataPtr());
}

aVect<wchar_t> ListBox_MyGetText(HWND hListBox) {
	auto curSel = ListBox_GetCurSel(hListBox);
	if (curSel == LB_ERR) return aVect<wchar_t>();
	auto count = ListBox_GetTextLen(hListBox, curSel) + 5;
	aVect<wchar_t> text(count);
	ListBox_GetText(hListBox, curSel, text.GetDataPtr());
	return text;
}

void CopyToClipBoard(const aVect<wchar_t> & str) {

	OpenClipboard(NULL);

	HGLOBAL clipbuffer;
	wchar_t * buf;
	EmptyClipboard();
	clipbuffer = GlobalAlloc(GMEM_DDESHARE, str.Count() * sizeof(wchar_t));
	buf = (wchar_t*)GlobalLock(clipbuffer);
	wcscpy(buf, LPWSTR(&str[0]));
	GlobalUnlock(clipbuffer);
	SetClipboardData(CF_UNICODETEXT, clipbuffer);
	CloseClipboard();

}

void AdjustWindowSizeForText(HWND hWnd, bool adjust_width, bool adjust_height) {

	auto txt    = MyGetWindowText(hWnd);

	auto r = GetRect(hWnd);

	auto width  = adjust_width  ? TextWidth(hWnd, txt)  + 4 : r.right - r.left;
	auto height = adjust_height ? TextHeight(hWnd, txt) + 2 : r.bottom - r.top;
	
	SetWindowPos(hWnd, NULL, 0, 0, width, height, SWP_NOMOVE);

}
