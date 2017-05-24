
#include "windows.h"
#include "comutil.h"
#include "Oleacc.h"

//MSO.DLL: C:\Program Files (x86)\Common Files\Microsoft Shared\OFFICE14\

#import "MSO.DLL" no_function_mapping \
rename( "RGB", "MSORGB" ) \
rename( "DocumentProperties", "MSODocumentProperties" ) \
rename ("FirstChild", "MSOFirstChild") \
rename ("NextSibling", "MSONextSibling") \

#pragma warning( disable : 4146 )

using namespace Office;

//VBE7.DLL: C:\Program Files (x86)\Common Files\microsoft shared\VBA\VBA7\

#import "VBE7.DLL" no_function_mapping \
rename( "EOF", "VBAEOF" ) \
rename( "RGB", "VBARGB" ) \
rename( "GetObject", "VBAGetObject" ) \
rename( "DialogBox", "VBADialogBox" )

#include "xVect.h"
#include "WinUtils.h"

using namespace VBA;

//VBE6EXT.OLB C:\Program Files (x86)\Common Files\microsoft shared\VBA\VBA6\

#import "VBE6EXT.OLB" no_function_mapping 

#import "C:\Program Files (x86)\Microsoft Office\Office14\EXCEL.EXE" no_function_mapping \
rename( "DialogBox", "ExcelDialogBox" ) \
rename( "RGB", "ExcelRGB" ) \
rename( "CopyFile", "ExcelCopyFile" ) \
rename( "ReplaceText", "ExcelReplaceText" ) \
rename("FirstChild", "VBAFirstChild") \
rename("NextSibling", "VBANextSibling") \
exclude( "IFont", "IPicture" ) 

struct RemoveOrAddAddinInfo {
	aVect<aVect<wchar_t>> workbooks;
	Excel::_ApplicationPtr app;
	bool macroHasRun = false;
	bool foundAddin = false;
	HWND hWnd = NULL;
};

struct ExcelAddInManager {
	LogFile * pLogFile = nullptr;
	Excel::_ApplicationPtr createdApp;
	aVect<RemoveOrAddAddinInfo> openedApps;
	aVect<wchar_t> dialogTitle;
	bool foundAddin = false;

	ExcelAddInManager() = default;
	ExcelAddInManager(const ExcelAddInManager&) = delete;
	ExcelAddInManager& operator=(const ExcelAddInManager&) = delete;
	ExcelAddInManager(ExcelAddInManager&& that) = default;
	ExcelAddInManager& operator=(ExcelAddInManager&& that);
	~ExcelAddInManager();

	void Remove(const wchar_t * name, const wchar_t * VBA_macroToRunOnce = nullptr, HRESULT VBA_macroToRunOnce_okFailure = 0);
	void Add(const wchar_t * path, bool install);
};

