#define _CRT_SECURE_NO_WARNINGS

#include "math.h"
#include "float.h"
#include "stdio.h"
#include "conio.h"
#include "crtdbg.h"
#include "process.h"
#include "windows.h"
#include "Winternl.h"

#include <limits>
#include "emmintrin.h"
#include <atomic>

#include "xVect.h"
#include "WinUtils.h"
#include "MyUtils.h"
#include "AxisCore.h"

//#define DEBUG_AXIS
//#define DEBUG_THREADS 
#define AXIS_MAX_WORK 2
#define AXIS_CS_SPINCOUNT 4000

//#pragma optimize("", off)

//Global variables 
//volatile bool g_Axis_init;
//volatile bool g_EndOfTimes;
namespace {
	std::atomic<bool> g_Axis_init;
	std::atomic<bool> g_EndOfTimes;

	volatile HWND g_hWndAxisMgrWnd;
	volatile HANDLE g_hAxisMgrThread;
	aVect<HANDLE> g_hRefreshThread;
	aVect<HANDLE> g_hEventKillRefreshThread;
	aVect<HANDLE> g_hEventRefreshThreadKilled;
	HANDLE g_AxisHeap;
	aVectEx<Axis*, g_AxisHeap> g_axisArray;
	//volatile bool g_terminating;
	std::atomic<bool> g_terminating;
	unsigned int g_nThreads;
	int g_refreshThreadCount;
	HANDLE g_hEventEndOfTimes;

	//struct zero initializers
	Axis		Axis_Zero_Initializer;
	AxisTicks	AxisTicks_Zero_Initializer;
	AxisBorder	AxisBorder_Zero_Initializer;
	AxisLegend	AxisLegend_Zero_Initializer;
	AxisSerie	AxisSerie_Zero_Initializer;
	AxisWindowParam  AxisWindowParam_Zero_Initializer;
	AxisFont AxisFont_ZeroInitializer;
	AxisMatrixView AxisMatrixView_ZeroInitializer;
}

COLORREF g_AxisDefaultColors[] = {
	RGB(0, 0, 255),
	RGB(0, 255, 0),
	RGB(255, 0, 0),
	RGB(0, 255, 255),
	RGB(255, 0, 255),
	RGB(175, 175, 0),
	RGB(0, 150, 255),
	RGB(0, 255, 150),
	RGB(255, 0, 150),
	RGB(150, 0, 255),
	RGB(255, 150, 0),
	RGB(150, 255, 0),
	RGB(0, 0, 0)
};

COLORREF AxisDefaultColor(size_t i) {

	i = i % NUMEL(g_AxisDefaultColors);

	return g_AxisDefaultColors[i];
}

void *  AxisMalloc(size_t size) {
	if (!g_AxisHeap) MY_ERROR("AxisMalloc : g_AxisHeap = 0!");
	return HeapAlloc(g_AxisHeap, 0, size);
}

void AxisFree(void * ptr) {
	if (!g_AxisHeap) MY_ERROR("AxisFree : g_AxisHeap = 0!");
	HeapFree(g_AxisHeap, 0, ptr);
}

void *  AxisMalloc(size_t size, void * unused) {
	if (!g_AxisHeap) MY_ERROR("AxisMalloc : g_AxisHeap = 0!");
	return HeapAlloc(g_AxisHeap, 0, size);
}

void AxisFree(void * ptr, void * unused) {
	if (!g_AxisHeap) MY_ERROR("AxisFree : g_AxisHeap = 0!");
	HeapFree(g_AxisHeap, 0, ptr);
}

void * AxisRealloc(void * ptr, size_t size, void * unused) {
	if (!g_AxisHeap) MY_ERROR("AxisFree : g_AxisHeap = 0!");
	return HeapReAlloc(g_AxisHeap, 0, ptr, size);
}

void Axis_CloseRefreshThread(size_t i) {

	WIN32_SAFE_CALL(SetEvent(g_hEventKillRefreshThread[i]));

	if (WaitForSingleObject(g_hEventRefreshThreadKilled[i], INFINITE) != WAIT_OBJECT_0) MY_ERROR("there");

	CloseHandle(g_hRefreshThread[i]);
	g_hRefreshThread.Remove(i);

	CloseHandle(g_hEventRefreshThreadKilled[i]);
	g_hEventRefreshThreadKilled.Remove(i);

	CloseHandle(g_hEventKillRefreshThread[i]);
	g_hEventKillRefreshThread.Remove(i);
	
	g_refreshThreadCount--;
}

unsigned int _stdcall Axis_TerminatorThread(void * terminating) {
	DbgStr(L"+[0x%p] New thread : Axis_CreateTerminatorThread\n", GetCurrentThreadId());
	Axis_EndOfTimes(terminating != nullptr);
	return 0;
}

void Axis_CreateTerminatorThread() {
	SAFE_CALL(CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)Axis_TerminatorThread, nullptr, 0, nullptr));
}

void Axis_EndOfTimes(bool terminating) {
	
	g_EndOfTimes = true;
	g_terminating = terminating;

	if (!g_hAxisMgrThread) return;

	DbgStr(L" [0x%p] Axis_EndOfTimes\n", GetCurrentThreadId());

	if (!g_hEventEndOfTimes) g_hEventEndOfTimes = CreateEvent(NULL, 1, 0, NULL);
	if (!g_hEventEndOfTimes) MY_ERROR("CreateEvent failed");

	if (GetCurrentThreadId() == SAFE_CALL(GetThreadId(g_hAxisMgrThread), 0)) {
		DbgStr(L" [0x%p] Axis_EndOfTimes : Calling Axis_CreateTerminatorThread...\n", GetCurrentThreadId());
		Axis_CreateTerminatorThread();
		return;
	}

	Axis_CloseAllWindows(false);

	int count = 0;

	DbgStr(L" [0x%p] Axis_EndOfTimes : Unregistering AxisWinClass Class...\n", GetCurrentThreadId());

	WNDCLASSA wc;

	if (GetClassInfoA(GetModuleHandle(NULL), "AxisWinClass", &wc)) {

		//MSG msg;
		while (!UnregisterClassA("AxisWinClass", GetModuleHandle(NULL))) {
			DbgStr(L" [0x%p] Unable to unregister class... error %s\n",
				GetCurrentThreadId(), GetWin32ErrorDescription(GetLastError()));
			count++;
			//GetMessageW(&msg, NULL, 0, 0);
			//TranslateMessage(&msg);
			//DispatchMessageA(&msg);
			Sleep(10);
		}
	}

	if (count) {
		DbgStr(" [0x%p] Class unregistered after %d attempts\n", GetCurrentThreadId(), count);
	}

	aVect_for_inv(g_hRefreshThread, i) {
		if (g_hEventKillRefreshThread[i]) {
			DbgStr(L" [0x%p] Axis_EndOfTimes : Calling Axis_CloseRefreshThread n°%d\n", GetCurrentThreadId(), i);
			Axis_CloseRefreshThread(i);
		}
	}

	MyAssert(!g_refreshThreadCount);

	DbgStr(L" [0x%p] Axis_EndOfTimes : Calling Axis_RefreshAsync(NULL)\n", GetCurrentThreadId());
	Axis_RefreshAsync(NULL);//Reset signal

	ResetEvent(g_hEventEndOfTimes);

	DbgStr(L" [0x%p] Axis_EndOfTimes : Posting WM_CLOSE to AxisMgrWinProc (NULL)\n", GetCurrentThreadId());
	if (g_hAxisMgrThread) {
		WIN32_SAFE_CALL(PostMessageW(g_hWndAxisMgrWnd, WM_CLOSE, (WPARAM)g_hEventEndOfTimes, (LPARAM)terminating));
		if (WaitForSingleObject(g_hEventEndOfTimes, INFINITE) != WAIT_OBJECT_0) MyMessageBox("there %d", GetLastError());
		
		//No, because we want this function be callable from DllMain !
		//if (WaitForSingleObject(g_hAxisMgrThread, INFINITE) != WAIT_OBJECT_0) DbgStr("Ooopss.. ??");

		//Same problem
		//for (;;) {
		//	DWORD exitCode;
		//	SAFE_CALL(GetExitCodeThread(g_hAxisMgrThread, &exitCode));
		//	if (exitCode != STILL_ACTIVE) break;
		//}

		CloseHandle(g_hAxisMgrThread);
	}

	//return;

	DbgStr(L" [0x%p] Axis_EndOfTimes : Unregistering AxisMgr Class...\n", GetCurrentThreadId());

	if (GetClassInfoA(GetModuleHandle(NULL), "AxisMgr", &wc)) {
		WIN32_SAFE_CALL(UnregisterClassA("AxisMgr", GetModuleHandle(NULL)));
	}
		
	//moved to before closing AxisMgrThread
	//int count = 0;

	//DbgStr(L" [0x%p] Axis_EndOfTimes : Unregistering AxisWinClass Class...\n", GetCurrentThreadId());

	//if (GetClassInfoA(GetModuleHandle(NULL), "AxisWinClass", &wc)) {

	//	MSG msg;
	//	while (!UnregisterClassA("AxisWinClass", GetModuleHandle(NULL))) {
	//		DbgStr(L" [0x%p] Unable to unregister class... error %s\n", 
	//			GetCurrentThreadId(), GetWin32ErrorDescription(GetLastError()));
	//		count++;
	//		GetMessageW(&msg, NULL, 0, 0);
	//		TranslateMessage(&msg);
	//		DispatchMessageA(&msg);
	//	}
	//}

	//if (count) {
	//	DbgStr(" [0x%p] Class unregistered after %d attempts\n", GetCurrentThreadId(), count);
	//}

	CloseHandle(g_hEventEndOfTimes);
	g_hEventEndOfTimes = NULL;

	DbgStr(" [0x%p] Axis_EndOfTimes : reseting global variables & \"leaking\" g_axisArray\n", GetCurrentThreadId());

	g_hAxisMgrThread = NULL;
	g_hWndAxisMgrWnd = NULL;

	g_axisArray.Leak();

	g_EndOfTimes = false;
}

void Axis_SetCoordinateTransform(
		Axis * axis, 
		double CoordTransf_xFact, 
		double CoordTransf_yFact, 
		double CoordTransf_dx, 
		double CoordTransf_dy) {
	if (!(_finite(CoordTransf_xFact)) 
	||  !(_finite(CoordTransf_yFact))
	||  !(_finite(CoordTransf_yFact))
	||  !(_finite(CoordTransf_yFact))) 
	{
		CoordTransf_xFact = CoordTransf_yFact = CoordTransf_dx = CoordTransf_dy = 1;
	}

	axis->CoordTransf_xFact = CoordTransf_xFact;
	axis->CoordTransf_yFact = CoordTransf_yFact;
	axis->CoordTransf_dx = CoordTransf_dx;
	axis->CoordTransf_dy = CoordTransf_dy;
}

void AxisFont_Destroy(AxisFont * pFont) {

	if (pFont->hFont) {
		DeleteFont(pFont->hFont);
		pFont->hFont = nullptr;
	}

	if (pFont->pLogFont) {
		AxisFree(pFont->pLogFont);
		pFont->pLogFont = nullptr;
	}
}

void Axis_DestroyLegend(Axis *pAxis) {

	if (pAxis->legend) {
		xVect_Free<AxisFree>(pAxis->legend->pText);
		AxisFont_Destroy(&pAxis->legend->font);
		AxisFree(pAxis->legend);
		pAxis->legend = nullptr;
	}
}


AxisFont AxisFont_Create(
		wchar_t * fontName = L"arial",
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
		BYTE pitchAndFamily = DEFAULT_PITCH | FF_DONTCARE)
{
	
	AxisFont retVal;

	if (!
		(retVal.pLogFont = (LOGFONTW *)AxisMalloc(sizeof(LOGFONTW)))
	) MY_ERROR("malloc = 0");
	
	(*retVal.pLogFont) = CreateLogFont(fontName, height, avWidth, escapement, 
			orientation, weight, italic, underline, strikeOut, charSet,
			outPrecision, clipPrecision, quality, pitchAndFamily);

	retVal.hFont = CreateFontIndirectW(retVal.pLogFont);

	return retVal;
}

AxisFont AxisFont_CreateIndirect(LOGFONTW * logFont) {
	
	AxisFont retVal;

	if (!
		(retVal.pLogFont = (LOGFONTW *)AxisMalloc(sizeof(LOGFONTW)))
	) MY_ERROR("malloc = 0");
	
	*(retVal.pLogFont) = *logFont;
	retVal.hFont = CreateFontIndirectW(retVal.pLogFont);

	return retVal;
}

void AxisTicks_SetOption(AxisTicks * pTicks, DWORD option, bool val) {
	SET_DWORD_BYTE_MASK(pTicks->options, option, val);
}
bool AxisTicks_GetOption(AxisTicks * pTicks, DWORD option) {
	return (pTicks->options & option) != 0;
}
void Axis_SetOption(Axis * pAxis, DWORD option, bool val) {
	SET_DWORD_BYTE_MASK(pAxis->options, option, val);
}
bool AxisTicks_GetOption(Axis * pAxis, DWORD option) {
	return (pAxis->options & option) != 0;
}

bool Axis_IsMatrixViewLegend(Axis * pAxis) {
	return (pAxis->options & AXIS_OPT_ISMATVIEWLEGEND) != 0;
}

bool Axis_HasMatrixViewLegend(Axis * pAxis) {
	return (
		!(pAxis->options & AXIS_OPT_ISMATVIEWLEGEND)
		&& pAxis->pMatrixView 
		&& pAxis->pMatrixView->cMap 
		&& pAxis->pMatrixView->cMap->legendCmap
	);
}

void Axis_AssertIsNotMatrixViewLegend(Axis * pAxis, char * msg) {
	if (Axis_IsMatrixViewLegend(pAxis)) MY_ERROR(msg);
}

void Axis_CreateLegend(Axis * pAxis, wchar_t * title) {

	if (pAxis->legend) Axis_DestroyLegend(pAxis);
	if (!(pAxis->legend = (AxisLegend *)AxisMalloc(sizeof(AxisLegend)))) MY_ERROR("malloc = 0");
	*(pAxis->legend) = AxisLegend_Zero_Initializer;
	if (title) pAxis->legend->pText = xVect_Asprintf<AxisMalloc, AxisRealloc>(L"%s", title);

	pAxis->legend->font = AxisFont_Create(L"Arial", 14);

	pAxis->legend->marge = 5;
	pAxis->legend->adim_left = 0.7;
	pAxis->legend->adim_top = 0.2;
	pAxis->legend->lineLength = 40;
	pAxis->legend->pAxis = pAxis;
}

void Axis_DrawLegend(Axis * pAxis) {

	if ((pAxis->options & AXIS_OPT_LEGEND)
	&& xVect_Count(pAxis->pSeries)) 
	{
		if (!(pAxis->legend)) {
			Axis_CreateLegend(pAxis, NULL);
		}
		Axis_DrawLegendCore(pAxis, true);
		Axis_DrawLegendCore(pAxis, false);
	}
}

bool Axis_IsLegendArea(Axis * pAxis, POINT * pt) {

	if (!pAxis->legend || (pAxis->options & AXIS_OPT_GHOST)) return false;

	if (pt->x > pAxis->legend->rect.left   + pAxis->DrawArea_nPixels_dec_x	
	&&  pt->x < pAxis->legend->rect.right  + pAxis->DrawArea_nPixels_dec_x
	&&  pt->y > pAxis->legend->rect.top    + pAxis->DrawArea_nPixels_dec_y
	&&  pt->y < pAxis->legend->rect.bottom + pAxis->DrawArea_nPixels_dec_y)
	{
		return true;
	}
	else {
		return false;
	}
}

bool Axis_IsMatrixViewLegendArea(Axis * pAxis, POINT * pt) {

	if (!(pAxis->options & AXIS_OPT_LEGEND) || !Axis_HasMatrixViewLegend(pAxis)) return false;

	Axis * pLegendAxis = pAxis->pMatrixView->cMap->legendCmap;

	if (pt->x > pLegendAxis->DrawArea_nPixels_dec_x + pAxis->DrawArea_nPixels_dec_x
	&&  pt->x < pLegendAxis->DrawArea_nPixels_dec_x + pLegendAxis->DrawArea_nPixels_x  + pAxis->DrawArea_nPixels_dec_x
	&&  pt->y > pLegendAxis->DrawArea_nPixels_dec_y + pAxis->DrawArea_nPixels_dec_y
	&&  pt->y < pLegendAxis->DrawArea_nPixels_dec_y + pLegendAxis->DrawArea_nPixels_y + pAxis->DrawArea_nPixels_dec_y) {
		return true;
	}
	else {
		return false;
	}
}


void Axis_DrawLegendCore(Axis * pAxis, bool computeExtent) {

	RECT rect;
	AxisLegend * pLegend;
	static HPEN hPen;
	HPEN oldPen;
	static HBRUSH hBrush;
	static HBRUSH oldBrush;
	
	SIZE sz;

	if (!hPen) hPen = GetStockPen(BLACK_PEN);
	if (!hBrush) hBrush = GetStockBrush(WHITE_BRUSH);

	Critical_Section(pAxis->pCs) {

		pLegend = pAxis->legend;
	
		rect.left = MyRound(pLegend->adim_left * pAxis->DrawArea_nPixels_x);
		rect.top  = MyRound(pLegend->adim_top *  pAxis->DrawArea_nPixels_y);
		rect.right  = rect.left;
		rect.bottom = rect.top + pLegend->marge;

		GDI_SAFE_CALL(SelectFont(pAxis->hdcDraw, pLegend->font.hFont));
		SetTextAlign(pAxis->hdcDraw, TA_TOP | TA_LEFT);

		if (!computeExtent) {

			GDI_SAFE_CALL(oldPen   = SelectPen(pAxis->hdcDraw, hPen));
			GDI_SAFE_CALL(oldBrush = SelectBrush(pAxis->hdcDraw, hBrush));
		
			GDI_SAFE_CALL(Rectangle(pAxis->hdcDraw, 
				pLegend->rect.left,  pLegend->rect.top, 
				pLegend->rect.right, pLegend->rect.bottom));
	
			GDI_SAFE_CALL(SelectPen(pAxis->hdcDraw, oldPen));
			GDI_SAFE_CALL(SelectPen(pAxis->hdcDraw, oldBrush));
		}

		if (pLegend->pText) {

			GetTextExtentPoint32W(
				pAxis->hdcDraw, pLegend->pText, 
				(int)xVect_Count(pLegend->pText),&sz);
		
			if (!computeExtent) {

				auto oldColor = SetTextColor(pAxis->hdcDraw, RGB(0, 0, 0));

				GDI_SAFE_CALL(TextOutW(
					pAxis->hdcDraw, rect.left + pLegend->marge, 
					rect.bottom, pLegend->pText, (int)xVect_Count(pLegend->pText)-1));

				SetTextColor(pAxis->hdcDraw, oldColor);
			}

			rect.bottom += sz.cy + 2;
			int test = MyRound(rect.left + sz.cx + 2*pLegend->marge);
			if (rect.right < test) {
				rect.right = test;
			}
		}

		AxisSerie * pSeries = pAxis->pSeries;
		if (pSeries) {
			xVect_static_for(pSeries, i) {

				if (pSeries[i].pName) {

					GetTextExtentPoint32W(
						pAxis->hdcDraw, pSeries[i].pName, 
						(int)xVect_Count(pSeries[i].pName), &sz);

					if (!computeExtent) {

						Axis_SelectPen(pAxis, CreatePenIndirect(&pSeries[i].pen));
						GDI_SAFE_CALL(MoveToEx(
							pAxis->hdcDraw, rect.left + pLegend->marge, 
							rect.bottom + MyRound(sz.cy/2), NULL));
						GDI_SAFE_CALL(LineTo(
							pAxis->hdcDraw, rect.left + pLegend->marge + pLegend->lineLength, 
							rect.bottom + MyRound(sz.cy / 2)));

						auto oldColor = SetTextColor(pAxis->hdcDraw, RGB(0, 0, 0));

						GDI_SAFE_CALL(TextOutW(
							pAxis->hdcDraw, rect.left + 2*pLegend->marge + pLegend->lineLength, 
							rect.bottom, pSeries[i].pName, (int)xVect_Count(pSeries[i].pName)-1));

						SetTextColor(pAxis->hdcDraw, oldColor);
					}

					rect.bottom += sz.cy + 2;

					int test = MyRound(
						rect.left + pLegend->lineLength
						+ sz.cx + 2*pLegend->marge);

					if (rect.right < test) {
						rect.right = test;
					}
				}
			}

			rect.bottom += pLegend->marge;

			if (computeExtent) pLegend->rect = rect;
		}
	}
}

bool Axis_Exist(Axis * pAxis) {

	return pAxis 
	   &&  pAxis->hWnd 
	   && !(pAxis->options & AXIS_OPT_ISORPHAN)
	   && (pAxis->hdc || (pAxis->options & AXIS_OPT_USERGHOST));
}

void Axis_GetWindowedAdimPos(HWND hWnd, int plotArea_nx, int plotArea_ny, 
							 double * dx, double * dy, double * nx, double * ny) {

	RECT rect;
	RECT rect2;
	HWND hParent;

	hParent = GetParent(hWnd);
	if (hParent) {
		GetClientRect(hParent, &rect) ;
		GetWindowRect(hParent, &rect2);
	}
	else {
		GetClientRect(GetDesktopWindow(), &rect) ;
		GetWindowRect(GetDesktopWindow(), &rect2);
	}

	double client_width = rect.right;
	double client_height = rect.bottom;

	GetClientRect(hWnd, &rect);
	GetWindowRect(hWnd, &rect2);

	int decRect_x = (rect2.right - rect2.left) - rect.right;
	int decRect_y = (rect2.bottom - rect2.top) - rect.bottom;

	GetWindowRect(hWnd, &rect2);
	if (hParent) {
		ScreenToClient(hParent, (LPPOINT)&rect2);
	}
	else {
		ScreenToClient(GetDesktopWindow(), (LPPOINT)&rect2);
	}
	
	double dec_x = rect2.left;
	double dec_y = rect2.top ;

	double x = plotArea_nx + decRect_x;
	double y = plotArea_ny + decRect_y;

	(*dx) = dec_x / (double)client_width;
	(*dy) = dec_y / (double)client_height;
	(*nx) = x / (double)client_width;
	(*ny) = y / (double)client_height;
}

void Axis_UpdateAdimPos(Axis * pAxis) {

	RECT rect;
	double x, y, dec_x, dec_y;
	double client_width, client_height;

	if (pAxis->options & AXIS_OPT_WINDOWED) {

		Axis_GetWindowedAdimPos(pAxis->hWnd,
			pAxis->DrawArea_nPixels_x,
			pAxis->DrawArea_nPixels_y,
			&pAxis->adim_dx, &pAxis->adim_dy,
			&pAxis->adim_nX, &pAxis->adim_nY);

		return;

	}
	else {
		GetClientRect(pAxis->hWnd, &rect);
		dec_x = pAxis->DrawArea_nPixels_dec_x;
		dec_y = pAxis->DrawArea_nPixels_dec_y;
		x = pAxis->DrawArea_nPixels_x;
		y = pAxis->DrawArea_nPixels_y;
		client_width = rect.right;
		client_height = rect.bottom;
	}

	pAxis->adim_dx = dec_x / client_width;
	pAxis->adim_dy = dec_y / client_height;
	pAxis->adim_nX = x / client_width;
	pAxis->adim_nY = y / client_height;

	if (Axis_HasMatrixViewLegend(pAxis)) {
		Axis * pLegendAxis = pAxis->pMatrixView->cMap->legendCmap;
		Axis_UpdateAdimPos(pLegendAxis);
	}
}

void Axis_SetAdimMode(Axis * pAxis, bool val) {

	Axis_UpdateAdimPos(pAxis);

	SET_DWORD_BYTE_MASK(pAxis->options, AXIS_OPT_ADIM_MODE, val);
}

bool Axis_GetAdimMode(Axis * pAxis) {
	return (pAxis->options & AXIS_OPT_ADIM_MODE)!=0;
}

void Axis_UpdatePlotAreaRect(Axis * pAxis) {

	pAxis->PlotArea.left   = pAxis->DrawArea_nPixels_dec_x 
						   + (pAxis->pY_Axis ? pAxis->pY_Axis->extent : 0);

	pAxis->PlotArea.top    = pAxis->DrawArea_nPixels_dec_y
		                   + (pAxis->pTitle  ? pAxis->pTitle->extent  : 0);

	pAxis->PlotArea.right  = pAxis->DrawArea_nPixels_dec_x
		                   + pAxis->DrawArea_nPixels_x 
		                   - (pAxis->pY_Axis2 ? pAxis->pY_Axis2->extent : 0);

	pAxis->PlotArea.bottom = pAxis->DrawArea_nPixels_dec_y
		                   + pAxis->DrawArea_nPixels_y 
		                   - (pAxis->pX_Axis ?    pAxis->pX_Axis->extent    : 0)
						   - (pAxis->pStatusBar ? pAxis->pStatusBar->extent : 0);

	pAxis->PlotArea.right -=  (!pAxis->pY_Axis2);
	pAxis->PlotArea.bottom -= (!pAxis->pX_Axis && !pAxis->pStatusBar);
}

void Axis_Zoom(
		Axis * pAxis, 
		double xMin, 
		double xMax, 
		double yMin, 
		double yMax) {


	auto noChanges = 
		xMin == pAxis->xMin &&  
		xMax == pAxis->xMax &&  
		yMin == pAxis->yMin &&  
		yMax == pAxis->yMax;

	if (xMax == 0 && xMin == 0) xMax = 1;
	if (yMax == 0 && yMin == 0) yMax = 1;

	if (xMax == xMin) {
		xMax *= 1.01;
		xMin *= 0.99;
	}
	else if (xMin > xMax) {
		Swap(xMin, xMax);
	}

	if (yMax == yMin) {
		yMax *= 1.01;
		yMin *= 0.99;
	}
	else if (yMin > yMax) {
		Swap(yMin, yMax);
	}
	
	auto bkp_CoordTransf_xFact = pAxis->CoordTransf_xFact;
	auto bkp_CoordTransf_yFact = pAxis->CoordTransf_yFact;
	auto bkp_CoordTransf_dx = pAxis->CoordTransf_dx;
	auto bkp_CoordTransf_dy = pAxis->CoordTransf_dy;
	auto bkp_xMin = pAxis->xMin;
	auto bkp_xMax = pAxis->xMax;
	auto bkp_yMin = pAxis->yMin;
	auto bkp_yMax = pAxis->yMax;

	pAxis->xMin = xMin;
	pAxis->xMax = xMax;
	pAxis->yMin = yMin;
	pAxis->yMax = yMax;
	
	if (pAxis->options & AXIS_OPT_CUR_XLOG) xMin = log10(xMin), xMax = log10(xMax);
	if (pAxis->options & AXIS_OPT_CUR_YLOG) yMin = log10(yMin), yMax = log10(yMax);

	if (xMax - xMin > 0 || yMax - yMin > 0) {
		if (pAxis->pMatrixView && (pAxis->pMatrixView->options & AXISMATRIXVIEW_OPT_MATRIXORIENTATION)) {

			double new_xFact =       (pAxis->PlotArea.right - pAxis->PlotArea.left) / (xMax - xMin);
			double new_yFact =       (pAxis->PlotArea.bottom - pAxis->PlotArea.top) / (yMax - yMin);

			double new_dx =   xMin * (pAxis->PlotArea.right - pAxis->PlotArea.left) / (xMin - xMax)
				                    + pAxis->PlotArea.left - pAxis->DrawArea_nPixels_dec_x;

			double new_dy =   yMin * (pAxis->PlotArea.bottom - pAxis->PlotArea.top) / (yMin - yMax)
				                    + pAxis->PlotArea.top - pAxis->DrawArea_nPixels_dec_y;

			Axis_SetCoordinateTransform(pAxis, new_xFact, new_yFact, new_dx, new_dy);
		}
		else{
			double new_xFact =       (pAxis->PlotArea.right - pAxis->PlotArea.left) / (xMax - xMin);
			double new_yFact =     - (pAxis->PlotArea.bottom - pAxis->PlotArea.top) / (yMax - yMin);

			double new_dx =   xMin * (pAxis->PlotArea.right - pAxis->PlotArea.left) / (xMin - xMax)
				                    + pAxis->PlotArea.left - pAxis->DrawArea_nPixels_dec_x;

			double new_dy =  - yMin * (pAxis->PlotArea.bottom - pAxis->PlotArea.top) / (yMin - yMax)
				                    + pAxis->PlotArea.bottom - pAxis->DrawArea_nPixels_dec_y;

			Axis_SetCoordinateTransform(pAxis, new_xFact, new_yFact, new_dx, new_dy);
		}
	}

	if (pAxis->checkZoomCallback) {
		
		if (noChanges) return;

		bool cancel = false;
		pAxis->checkZoomCallback(pAxis, cancel);

		if (cancel) {
			pAxis->CoordTransf_xFact = bkp_CoordTransf_xFact;
			pAxis->CoordTransf_yFact = bkp_CoordTransf_yFact;
			pAxis->CoordTransf_dx = bkp_CoordTransf_dx;
			pAxis->CoordTransf_dy = bkp_CoordTransf_dy;
			pAxis->xMin = bkp_xMin;
			pAxis->xMax = bkp_xMax;
			pAxis->yMin = bkp_yMin;
			pAxis->yMax = bkp_yMax;
		}
	}
}

void Axis_SetZoomed(Axis * pAxis) {

	double xMin = pAxis->xMin == 0 ? DBL_MIN : pAxis->xMin * (1 + 10 * DBL_EPSILON);
	
	Axis_Zoom(pAxis, xMin, pAxis->xMax, pAxis->yMin, pAxis->yMax);
}


AxisDrawCallback Axis_GetDrawCallback(Axis * pAxis) {

	return pAxis->drawCallback;
}

void Axis_InitBitmaps(Axis * pAxis) {

	static HPEN defaultPen;
	static HPEN defaultPen2;

	Critical_Section(pAxis->pCs) {
	
		if (pAxis->hdc 
			&& (!(pAxis->options & AXIS_OPT_GHOST) || (pAxis->options & AXIS_OPT_MAINHDC))
			&& pAxis->DrawArea_nPixels_x 
			&& pAxis->DrawArea_nPixels_y) {
		
			if (!defaultPen)  defaultPen  = CreatePen(PS_SOLID, 1, RGB(0,0,0));
			if (!defaultPen2) defaultPen2 = CreatePen(PS_SOLID, 1, pAxis->BackGroundColor);

			if (pAxis->hdcDraw)      GDI_SAFE_CALL(DeleteObject(pAxis->hdcDraw));
			if (pAxis->hdcErase)     GDI_SAFE_CALL(DeleteObject(pAxis->hdcErase));
			if (pAxis->hBitmapDraw)  GDI_SAFE_CALL(DeleteObject(pAxis->hBitmapDraw));
			if (pAxis->hBitmapErase) GDI_SAFE_CALL(DeleteObject(pAxis->hBitmapErase));
			if (pAxis->hBackGroundBrush) GDI_SAFE_CALL(DeleteObject(pAxis->hBackGroundBrush));

			GDI_SAFE_CALL(pAxis->hdcDraw     = CreateCompatibleDC(pAxis->hdc));

			BITMAPINFO bmi;

			bmi.bmiHeader.biSize   = sizeof(bmi.bmiHeader);
			bmi.bmiHeader.biWidth  = pAxis->DrawArea_nPixels_x;
			bmi.bmiHeader.biHeight = -pAxis->DrawArea_nPixels_y;
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biBitCount = 32;
			bmi.bmiHeader.biCompression = BI_RGB;
			bmi.bmiHeader.biSizeImage = 0;
			bmi.bmiHeader.biXPelsPerMeter = 0;
			bmi.bmiHeader.biYPelsPerMeter = 0;
			bmi.bmiHeader.biClrUsed = 0;
			bmi.bmiHeader.biClrImportant = 0;

			GDI_SAFE_CALL(pAxis->hBitmapDraw = CreateDIBSection(pAxis->hdc, &bmi, DIB_RGB_COLORS, &pAxis->bitmapDrawPtr, nullptr, 0));
			pAxis->bitmapDrawCount = pAxis->DrawArea_nPixels_x * pAxis->DrawArea_nPixels_y;

			SelectObject(pAxis->hdcDraw, pAxis->hBitmapDraw);

			GDI_SAFE_CALL(pAxis->hdcErase = CreateCompatibleDC(pAxis->hdc));
			GDI_SAFE_CALL(pAxis->hBitmapErase  = CreateCompatibleBitmap(pAxis->hdc, 
				pAxis->DrawArea_nPixels_x, pAxis->DrawArea_nPixels_y));
			GDI_SAFE_CALL(SelectObject(pAxis->hdcErase, pAxis->hBitmapErase));
		
			GDI_SAFE_CALL(pAxis->hBackGroundBrush = CreateSolidBrush(pAxis->BackGroundColor));
			SelectObject(pAxis->hdcErase, pAxis->hBackGroundBrush);
			SelectObject(pAxis->hdcErase, defaultPen2);

			GDI_SAFE_CALL(Rectangle(pAxis->hdcErase, 0, 0, 
				pAxis->DrawArea_nPixels_x,
				pAxis->DrawArea_nPixels_y));
		
			if (pAxis->hCurrentBrush) 
				SelectObject(pAxis->hdcDraw, pAxis->hCurrentBrush);
			if (pAxis->hCurrentPen) 
				SelectObject(pAxis->hdcDraw, pAxis->hCurrentPen);
		}
	}
}

void Axis_ReposCore(Axis * pAxis, 
				   unsigned int nPixels_x, 
				   unsigned int nPixels_y, 
				   int nPixels_x_dec, 
				   int nPixels_y_dec, 
				   bool updateAdimPos = 1) {
	
	bool bitmapSizeChange = true;

	Critical_Section(pAxis->pCs) {

		if (pAxis->options & AXIS_OPT_WINDOWED) {
			RECT rect;
			GDI_SAFE_CALL(GetClientRect(pAxis->hWnd, &rect));
			if (pAxis->DrawArea_nPixels_x == rect.right
			&&  pAxis->DrawArea_nPixels_y == rect.bottom) {
				bitmapSizeChange = false;
				if (pAxis->DrawArea_nPixels_dec_x == 0
				&&  pAxis->DrawArea_nPixels_dec_y == 0) {
					return;
				}
			}
			else {
				pAxis->DrawArea_nPixels_x = rect.right;
				pAxis->DrawArea_nPixels_y = rect.bottom;
			}
		}
		else {
			if (pAxis->DrawArea_nPixels_x == nPixels_x
			&&  pAxis->DrawArea_nPixels_y == nPixels_y) {
				bitmapSizeChange = false;
				if (pAxis->DrawArea_nPixels_dec_x == nPixels_x_dec
				&&  pAxis->DrawArea_nPixels_dec_y == nPixels_y_dec) {
					return;
				}
			}
			else {
				pAxis->DrawArea_nPixels_x = nPixels_x;
				pAxis->DrawArea_nPixels_y = nPixels_y;
			}
		}

		if (Axis_HasMatrixViewLegend(pAxis))
			Axis_Resize(pAxis->pMatrixView->cMap->legendCmap);

		pAxis->DrawArea_nPixels_dec_x = nPixels_x_dec;//remodifié après Axis_UpdateAdimPos
		pAxis->DrawArea_nPixels_dec_y = nPixels_y_dec;

		if (updateAdimPos) 
			Axis_UpdateAdimPos(pAxis);

		if (pAxis->options & AXIS_OPT_WINDOWED) pAxis->DrawArea_nPixels_dec_x = 0;
		if (pAxis->options & AXIS_OPT_WINDOWED) pAxis->DrawArea_nPixels_dec_y = 0;

		Axis_UpdateBorderPos(pAxis);
		Axis_UpdatePlotAreaRect(pAxis);

		Axis_Zoom(pAxis, pAxis->xMin, pAxis->xMax, pAxis->yMin, pAxis->yMax);

		if (bitmapSizeChange) Axis_InitBitmaps(pAxis);
	}
}

void Axis_Repos(Axis * pAxis, 
			   unsigned int nPixels_x, 
			   unsigned int nPixels_y, 
			   int nPixels_x_dec, 
			   int nPixels_y_dec, 
			   bool updateAdimPos = 1,
			   bool refresh_draw = true) {

	bool bitmapSizeChanged = true;

	if (pAxis->options & AXIS_OPT_WINDOWED) {
		RECT rect;
		GetClientRect(pAxis->hWnd, &rect);
		if (pAxis->DrawArea_nPixels_x == rect.right
		&&  pAxis->DrawArea_nPixels_y == rect.bottom) {
			bitmapSizeChanged = false;
			if (pAxis->DrawArea_nPixels_dec_x == 0
			&&  pAxis->DrawArea_nPixels_dec_y == 0) {
				return;
			}
		}
	}
	else {
		if (pAxis->DrawArea_nPixels_x == nPixels_x
		&&  pAxis->DrawArea_nPixels_y == nPixels_y) {
			bitmapSizeChanged = false;
			if (pAxis->DrawArea_nPixels_dec_x == nPixels_x_dec
			&&  pAxis->DrawArea_nPixels_dec_y == nPixels_y_dec) {
				return;
			}
		}
	}

	Axis_ReposCore(pAxis, nPixels_x, nPixels_y, nPixels_x_dec, nPixels_y_dec, updateAdimPos);

	if (bitmapSizeChanged && refresh_draw) Axis_Refresh(pAxis, false, false, false);
}

void Axis_GetContainerRect(Axis * pAxis, RECT * rect) {

	HWND hWnd_rect = pAxis->options & AXIS_OPT_WINDOWED ?
		GetParent(pAxis->hWnd) : pAxis->hWnd;
	if (!hWnd_rect) hWnd_rect = GetDesktopWindow();
	GetClientRect(hWnd_rect, rect);
}

void Axis_GetRect(Axis * pAxis, RECT * rect) {

	if (pAxis->options & AXIS_OPT_WINDOWED) {
		GetClientRect(pAxis->hWnd, rect);
	}
	else {
		rect->left   = pAxis->DrawArea_nPixels_dec_x;
		rect->right  = pAxis->DrawArea_nPixels_x + pAxis->DrawArea_nPixels_dec_x;
		rect->top    = pAxis->DrawArea_nPixels_dec_y;
		rect->bottom = pAxis->DrawArea_nPixels_y + pAxis->DrawArea_nPixels_dec_y;
	}
}

HWND Axis_GetParent(Axis * pAxis) {
	
	HWND hWnd = pAxis->options & AXIS_OPT_WINDOWED ?
	GetParent(pAxis->hWnd) : pAxis->hWnd;
	if (!hWnd) hWnd = GetDesktopWindow();

	return hWnd;
}

void Axis_old_AdimResize(Axis * pAxis) {
	
	RECT rect;

	Axis_GetContainerRect(pAxis, &rect);

	if (pAxis->options & AXIS_OPT_ISMATVIEWLEGEND) {
		Axis_Repos(pAxis,
			pAxis->DrawArea_nPixels_x,
			pAxis->DrawArea_nPixels_y,
			(int)(pAxis->adim_dx * rect.right),
			(int)(pAxis->adim_dy * rect.bottom), false);
	}
	else {
		Axis_Repos(pAxis,
			(int)(pAxis->adim_nX * rect.right),
			(int)(pAxis->adim_nY * rect.bottom),
			(int)(pAxis->adim_dx * rect.right),
			(int)(pAxis->adim_dy * rect.bottom), false);
	}
}

void Axis_AdimResizeWindow(Axis * pAxis) {

	RECT rect;
	Axis_GetContainerRect(pAxis, &rect);

	GDI_SAFE_CALL(SetWindowPos(
		pAxis->hWnd, NULL,
		MyRound(pAxis->adim_dx * rect.right),
		MyRound(pAxis->adim_dy * rect.bottom),
		MyRound(pAxis->adim_nX * rect.right),
		MyRound(pAxis->adim_nY * rect.bottom),
		SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE));
}

void Axis_SetAdimPos(Axis * pAxis, double nX, double nY, double dx, double dy, bool updateAdimPos = 1, bool refresh_draw = true) {

	RECT rect;
	Axis_GetContainerRect(pAxis, &rect);
	
	Axis_Repos(pAxis, 
		MyRound(nX * rect.right), 
		MyRound(nY * rect.bottom),
		MyRound(dx * rect.right), 
		MyRound(dy * rect.bottom), updateAdimPos);
}

void Axis_GetAdimPos(Axis * pAxis, double * nX, double * nY, double * dx, double * dy) {

	Axis_UpdateAdimPos(pAxis);
	*(nX) = pAxis->adim_nX;
	*(nY) = pAxis->adim_nY;
	*(dx) = pAxis->adim_dx;
	*(dy) = pAxis->adim_dy;
}

bool IsDifferentAndFinite(double x, double y) {
	return std::isfinite(x) && std::isfinite(y) && x != y;
}

bool Axis_IsZoomed(Axis * pAxis) {
	return 
		IsDifferentAndFinite(pAxis->xMax, pAxis->xMax_orig) ||
		IsDifferentAndFinite(pAxis->yMax, pAxis->yMax_orig) ||
		IsDifferentAndFinite(pAxis->xMin, pAxis->xMin_orig) ||
		IsDifferentAndFinite(pAxis->yMin, pAxis->yMin_orig);
}

bool Axis_IsColorZoomed(Axis * pAxis) {

	if (!pAxis->pMatrixView || !pAxis->pMatrixView->cMap) return false;

	return 
		IsDifferentAndFinite(pAxis->pMatrixView->cMap->vMax, pAxis->pMatrixView->cMap->vMax_orig) ||
		IsDifferentAndFinite(pAxis->pMatrixView->cMap->vMin, pAxis->pMatrixView->cMap->vMin_orig);
}

bool Axis_WasColorZoomed(Axis * pAxis) {

	if (!(pAxis->previousMatViewSettings.options & AXISMATRIXVIEW_OPT_VMIN_INITIALIZED)) return false;
	if (!(pAxis->previousMatViewSettings.options & AXISMATRIXVIEW_OPT_VMAX_INITIALIZED)) return false;
	if (!(pAxis->previousMatViewSettings.options & AXISMATRIXVIEW_OPT_VMINO_INITIALIZED)) return false;
	if (!(pAxis->previousMatViewSettings.options & AXISMATRIXVIEW_OPT_VMAXO_INITIALIZED)) return false;

	return 
		IsDifferentAndFinite(pAxis->previousMatViewSettings.cMap_vMax, pAxis->previousMatViewSettings.cMap_vMax_orig) ||
		IsDifferentAndFinite(pAxis->previousMatViewSettings.cMap_vMin, pAxis->previousMatViewSettings.cMap_vMin_orig);
}

void Axis_SetLim(Axis * pAxis, 
		double xMin, 
		double xMax, 
		double yMin, 
		double yMax,
		bool forceAdjustZoom = false) {
	
	bool wasZoomed = Axis_IsZoomed(pAxis);

	if (xMax == 0 && xMin == 0) xMax = 1;
	if (yMax == 0 && yMin == 0) yMax = 1;

	pAxis->xMin_orig = xMin;
	pAxis->xMax_orig = xMax;
	pAxis->yMin_orig = yMin;
	pAxis->yMax_orig = yMax;

	if (forceAdjustZoom || !wasZoomed) Axis_Zoom(pAxis, xMin, xMax, yMin, yMax);
}

void Axis_ResetZoom(Axis * pAxis) {

	Axis_Zoom(pAxis, 
		pAxis->xMin_orig, 
		pAxis->xMax_orig, 
		pAxis->yMin_orig, 
		pAxis->yMax_orig);

	xVect_static_for(pAxis->subAxis, i) {
		Axis_ResetZoom(pAxis->subAxis[i]);
	}
}

double Axis_GetCoordX(Axis * axis, long x) {
	
	double xd = x;

	double coord_X = (xd - axis->CoordTransf_dx - axis->DrawArea_nPixels_dec_x/* - 0.5*/) / axis->CoordTransf_xFact;

	return axis->options & AXIS_OPT_CUR_XLOG ? pow(10, coord_X) : coord_X;
}

double Axis_GetCoordY(Axis * axis, long y) {

	double yd = y;

	double coord_Y = (yd - axis->CoordTransf_dy - axis->DrawArea_nPixels_dec_y/* + 0.5*/) / axis->CoordTransf_yFact;

	return axis->options & AXIS_OPT_CUR_YLOG ? pow(10, coord_Y) : coord_Y;
}

void Axis_GetCoord(Axis * axis, double * x, double * y) {

	*x = Axis_GetCoordX(axis, (long)*x);
	*y = Axis_GetCoordY(axis, (long)*y);
}

double Axis_CoordTransf_X_double(Axis * axis, double x) {

	if (axis->options & AXIS_OPT_CUR_XLOG) x = log10(x);

	return x * axis->CoordTransf_xFact + axis->CoordTransf_dx;
}

double Axis_CoordTransf_Y_double(Axis * axis, double y) {

	if (axis->options & AXIS_OPT_CUR_YLOG) y = log10(y);

	return y * axis->CoordTransf_yFact + axis->CoordTransf_dy;
}

long Axis_CoordTransf_X(Axis * axis, double x) {

	return MyRound(Axis_CoordTransf_X_double(axis, x));
}

long Axis_CoordTransf_Y(Axis * axis, double y) {

	return MyRound(Axis_CoordTransf_Y_double(axis, y));
}

void Axis_CoordTransfDoubleToInt(Axis * axis, long * x_int, long * y_int, double x, double y) {

	(*x_int) = Axis_CoordTransf_X(axis, x);
	(*y_int) = Axis_CoordTransf_Y(axis, y);
}

void Axis_CoordTransf(Axis * axis, double * x, double * y) {

	(*x) = Axis_CoordTransf_X(axis, *x);
	(*y) = Axis_CoordTransf_Y(axis, *y);
}

void Axis_SetLogScaleX(Axis * axis, bool enable) {

	SET_DWORD_BYTE_MASK(axis->options, AXIS_OPT_XLOG, enable);

	if (xVect_Count(axis->subAxis)) {
		Axis_RefreshAsync(axis);
	}
}

void Axis_SetCurrentLogScaleX(Axis * axis, bool enable) {

	SET_DWORD_BYTE_MASK(axis->options, AXIS_OPT_CUR_XLOG, enable);

	if (axis->pX_Axis && axis->pX_Axis->pTicks) {
		if (enable) {
			axis->pX_Axis->pTicks->maxTicks = 39;
			axis->pX_Axis->pTicks->minDeltaTicks_pix = 5;
		}
		else {
			axis->pX_Axis->pTicks->maxTicks = 15;
			axis->pX_Axis->pTicks->minDeltaTicks_pix = 10;
		}
	}
}

void Axis_SetLogScaleY(Axis * axis, bool enable) {

	SET_DWORD_BYTE_MASK(axis->options, AXIS_OPT_YLOG, enable);

	if (xVect_Count(axis->subAxis)) {
		Axis_RefreshAsync(axis);
	}
}

void Axis_SetLegendColorStatus(Axis * axis, bool enable) {
	SET_DWORD_BYTE_MASK(axis->options, AXIS_OPT_LEGEND_CLR_STATUS, enable);
}


void Axis_SetCurrentLogScaleY(Axis * axis, bool enable) {

	SET_DWORD_BYTE_MASK(axis->options, AXIS_OPT_CUR_YLOG, enable);

	if (axis->pY_Axis && axis->pY_Axis->pTicks) {
		if (enable) {
			axis->pY_Axis->pTicks->maxTicks = 39;
			axis->pY_Axis->pTicks->minDeltaTicks_pix = 5;
		}
		else {
			axis->pY_Axis->pTicks->maxTicks = 15;
			axis->pY_Axis->pTicks->minDeltaTicks_pix = 10;
		}
	}
}

void Axis_TextOut(Axis * pAxis, double left, double top, double right, double bottom, 
	              wchar_t * pText, int nText, DWORD textAlignment, int isVertical, 
				  HFONT hFont, int isPixel) {

	SIZE sz;
	int newLeft;
	int newTop;

	if (hFont) {
		GDI_SAFE_CALL(SelectFont(pAxis->hdcDraw, hFont));
	}
	else {
		hFont = (HFONT)GetCurrentObject(pAxis->hdcDraw, OBJ_FONT);
	}

	if (!isPixel) {
		Axis_GetCoord(pAxis, &left, &top);
		Axis_GetCoord(pAxis, &right, &bottom);
	}
			
	SetTextAlign(pAxis->hdcDraw, TA_LEFT | TA_TOP);

	GetTextExtentPoint32W(pAxis->hdcDraw, pText, nText, &sz);

	if (isVertical) {
		Swap(sz.cx, sz.cy);
		sz.cy = -sz.cy;
	}

	LOGFONT logFont;
	GetObject(hFont, sizeof(LOGFONT), &logFont);

	if (textAlignment & AXIS_ALIGN_XCENTER) {
		newLeft = MyRound(0.5*(left + right + 1) - sz.cx/2);
	}
	else if (textAlignment & AXIS_ALIGN_XRIGHT) {
		newLeft = (int)(right - logFont.lfHeight - 2 - 1);
	}
	else if (textAlignment & AXIS_ALIGN_XLEFT) {
		newLeft = (int)(left + 1);
	}
	else MY_ERROR("Bad Alignement");

	if (textAlignment & AXIS_ALIGN_YCENTER) {
		newTop  = MyRound(0.5*(top  + bottom + 1) - sz.cy/2);
	}
	else if (textAlignment & AXIS_ALIGN_YBOTTOM) {
		newTop = (int)(bottom - sz.cy - 1);
	}
	else if (textAlignment & AXIS_ALIGN_YTOP){
		newTop = (int)(top + 1);
	}
	else MY_ERROR("Bad Alignement");

	TextOutW(pAxis->hdcDraw, newLeft, newTop, pText, nText);
}

COLORREF Axis_GetPenColor(HPEN hPen) {
	LOGPEN logPen;
	GDI_SAFE_CALL(GetObject(hPen, sizeof(LOGPEN), &logPen));
	return logPen.lopnColor;
}


void AxisBorder_Draw(AxisBorder * pBorder,
					 bool drawText = 1, 
					 bool drawBackGround = 1) {

	if (!pBorder) return;

	bool vertical = pBorder->isVertical;

	Critical_Section(pBorder->pAxis->pCs) {

		Axis * pAxis = pBorder->pAxis;

		if (pAxis->hdcDraw) {
			if (drawBackGround) {

				HBRUSH oldBrush = SelectBrush(pAxis->hdcDraw, GetStockBrush(WHITE_BRUSH));
				HPEN   oldPen   = SelectPen(pAxis->hdcDraw, GetStockPen(WHITE_PEN));

				GDI_SAFE_CALL(Rectangle(pAxis->hdcDraw, 
					vertical ? pBorder->rect.left : 0,
					vertical ? 0 : pBorder->rect.top,
					vertical ? pBorder->rect.right : pAxis->DrawArea_nPixels_x,
					vertical ? pAxis->DrawArea_nPixels_y : pBorder->rect.bottom));
		
				SelectBrush(pAxis->hdcDraw, oldBrush);
				SelectPen(pAxis->hdcDraw, oldPen);
			}

			if (drawText && pBorder->pText) {

				COLORREF color = pBorder->hCurrentPen ? Axis_GetPenColor(pBorder->hCurrentPen) : color = RGB(0, 0, 0);
				
				auto oldColor = SetTextColor(pAxis->hdcDraw, color);

				Axis_TextOut(pAxis, 
					pBorder->rect.left + pBorder->marge2,  pBorder->rect.top,
					pBorder->rect.right, pBorder->rect.bottom, 
					pBorder->pText, (int)xVect_Count(pBorder->pText) - 1,
					pBorder->textAlignment,	pBorder->isVertical,
					pBorder->font.hFont, true);

				SetTextColor(pAxis->hdcDraw, oldColor);
			}
		}
	}
}

void Axis_DrawBorders(Axis * pAxis, bool drawText = 1, bool drawBackGround = 1) {

	AxisBorder_Draw(pAxis->pTitle,     drawText, drawBackGround);
	AxisBorder_Draw(pAxis->pX_Axis,    drawText, drawBackGround);
	AxisBorder_Draw(pAxis->pY_Axis,    drawText, drawBackGround);
	AxisBorder_Draw(pAxis->pY_Axis2,   drawText, drawBackGround);
	AxisBorder_Draw(pAxis->pStatusBar, drawText, drawBackGround);
}

bool Axis_CallbacksDataWaiting(Axis * pAxis) {
	return (xVect_Count(pAxis->callbacksData)) > 1;
}

void Axis_CallPlotFunc(Axis * pAxis) {

	Critical_Section(pAxis->pCs) {

		if (pAxis->drawCallback)  {

			void * callbacksData = pAxis->callbacksData ? pAxis->callbacksData[0] : NULL;

			if (pAxis->hdc) pAxis->drawCallback(pAxis, callbacksData);
		}
		if (pAxis->callbacksCleanUp) {

			void * callbacksData = pAxis->callbacksData ? pAxis->callbacksData[0] : NULL;

			size_t I = xVect_Count(pAxis->callbacksData);

			if (I > 1) {
				pAxis->callbacksCleanUp(pAxis, callbacksData);
				xVect_Remove<AxisMalloc, AxisRealloc>(pAxis->callbacksData, 0);
			}
		}
	}
}

void Axis_Redraw(Axis * pAxis, HDC hdcPaint, bool redrawLegend) {

	Critical_Section(pAxis->pCs) {

		if (pAxis->hdc) {

			if (pAxis->hdcDraw == 0) Axis_InitBitmaps(pAxis);

			if ((pAxis->options & AXIS_OPT_LEGEND) && Axis_HasMatrixViewLegend(pAxis) && redrawLegend) {
				Axis_Redraw(pAxis->pMatrixView->cMap->legendCmap, pAxis->hdcDraw);
			}
			
			BitBlt(hdcPaint ? hdcPaint : pAxis->hdc, 
					pAxis->DrawArea_nPixels_dec_x, pAxis->DrawArea_nPixels_dec_y, 
					pAxis->DrawArea_nPixels_x,     pAxis->DrawArea_nPixels_y, 
					pAxis->hdcDraw, 0, 0, SRCCOPY);
		}
		else if ((pAxis->options & AXIS_OPT_USERGHOST) && pAxis->hBackGroundBrush) {

			if (!hdcPaint) MY_ERROR("ici");

			HBRUSH oldBrush;
			GDI_SAFE_CALL(oldBrush = SelectBrush(hdcPaint, pAxis->hBackGroundBrush));

			GDI_SAFE_CALL(Rectangle(hdcPaint, 
				pAxis->DrawArea_nPixels_dec_x, 
				pAxis->DrawArea_nPixels_dec_y, 
				pAxis->DrawArea_nPixels_dec_x + pAxis->DrawArea_nPixels_x, 
				pAxis->DrawArea_nPixels_dec_y + pAxis->DrawArea_nPixels_y));

			GDI_SAFE_CALL(SelectBrush(hdcPaint, oldBrush));
		}
	} 
}

void Axis_SelectPen(Axis * axis, HPEN hPen) {

	Critical_Section(axis->pCs) {

		if (!axis->hdc) {
			GDI_SAFE_CALL(DeleteObject(hPen));
			return;
		}
		if (hPen != axis->hCurrentPen) {
			if (axis->hCurrentPen) GDI_SAFE_CALL(DeleteObject(axis->hCurrentPen));
			SelectObject(axis->hdcDraw, hPen);
			axis->hCurrentPen = hPen;
		}
	}
}

void Axis_SelectBrush(Axis * axis, HBRUSH  hBrush) {
	
	Critical_Section(axis->pCs) {
		if (!axis->hdc) {
			GDI_SAFE_CALL(DeleteObject(hBrush));
			return;
		}

		if (axis->hCurrentBrush != hBrush) {
			if (axis->hCurrentBrush)  GDI_SAFE_CALL(DeleteObject(axis->hCurrentBrush));
			SelectObject(axis->hdcDraw, hBrush);
			axis->hCurrentBrush = hBrush;
		}
	}
}

void Axis_PolyLine(Axis * pAxis, double * x, double * y, size_t n) {

	aVectEx<POINT, g_AxisHeap> pt;

	Critical_Section(pAxis->pCs) {

		pt.Redim(n);

		//checking *points* is not correct, must check if any segment intersect the plot area...

		//size_t j = 0;
		//if (n != 0) {
		//	

		//	auto x_dbl = Axis_CoordTransf_X_double(pAxis, x[0]);
		//	auto y_dbl = Axis_CoordTransf_Y_double(pAxis, y[0]);

		//	bool prevPointInBounds = x_dbl >= pAxis->PlotArea.left && x_dbl <= pAxis->PlotArea.right && y_dbl >= pAxis->PlotArea.top && y_dbl <= pAxis->PlotArea.bottom;

		//	POINT prevPoint;
		//	prevPoint.x = MyRound(x_dbl);
		//	prevPoint.y = MyRound(y_dbl);

		//	if (prevPointInBounds) {
		//		pt[j] = prevPoint;
		//		j++;
		//	}

		//	for (size_t i = 1; i < n; i++) {
		//		//Axis_CoordTransfDoubleToInt(pAxis, &pt[i].x, &pt[i].y, x[i], y[i]);
		//		auto x_dbl = Axis_CoordTransf_X_double(pAxis, x[i]);
		//		auto y_dbl = Axis_CoordTransf_Y_double(pAxis, y[i]);

		//		bool currentPointInBounds = x_dbl >= pAxis->PlotArea.left && x_dbl <= pAxis->PlotArea.right && y_dbl >= pAxis->PlotArea.top && y_dbl <= pAxis->PlotArea.bottom;

		//		POINT currentPoint;
		//		currentPoint.x = MyRound(x_dbl);
		//		currentPoint.y = MyRound(y_dbl);

		//		if (j > i) MY_ERROR("gotcha");

		//		if (currentPointInBounds) {
		//			if (!prevPointInBounds) {
		//				pt[j] = prevPoint;
		//				j++;
		//			}
		//			pt[j] = currentPoint;
		//			j++;
		//		}

		//		if (j > i + 1) MY_ERROR("gotcha");

		//		if (prevPointInBounds) {
		//			if (!currentPointInBounds) {
		//				pt[j] = currentPoint;
		//				j++;
		//			}
		//		}

		//		if (j > i + 1) MY_ERROR("gotcha");

		//		prevPoint = currentPoint;
		//		prevPointInBounds = currentPointInBounds;
		//	}
		//}

		size_t iStart = 0;

		for (size_t i = 0; i < n; i++) {
			auto x_dbl = Axis_CoordTransf_X_double(pAxis, x[i]);
			auto y_dbl = Axis_CoordTransf_Y_double(pAxis, y[i]);

			if (x_dbl > 0.01 * LONG_MIN && x_dbl < 0.01 * LONG_MAX && y_dbl > 0.01 * LONG_MIN && y_dbl < 0.01 * LONG_MAX) {
				pt[i] = { MyRound(x_dbl),  MyRound(y_dbl) };
			} else {
				if (i > iStart) {
					Polyline(pAxis->hdcDraw, &pt[iStart], (int)(i - iStart));
				} else {
					/* do  nothing, previous point was incorrect */
				}
				iStart = i + 1;
			}
		}

		auto nRemaining = (int)(n - iStart);

		if (nRemaining > 1) {
			Polyline(pAxis->hdcDraw, &pt[iStart], (int)(n-iStart));
		} else if (nRemaining > 0) {
			//Axis_MoveToEx(pAxis, pt[0].x, pt[0].y);
			//Axis_LineTo(pAxis, pt[0].x, pt[0].y);
			//?? from and to are the same point ??
			Axis_MoveToEx(pAxis, x[0], y[0]);
			Axis_LineTo  (pAxis, x[0], y[0]);
		}
	}
}

void Axis_PolyLine(Axis * pAxis, float * x, float * y, size_t n) {

	aVectEx<POINT, g_AxisHeap> pt;

	Critical_Section(pAxis->pCs) {

		pt.Redim(n);

		for (size_t i = 0; i<n; i++) {
			Axis_CoordTransfDoubleToInt(pAxis, &pt[i].x, &pt[i].y, x[i], y[i]);
		}

		if (n>1) {
			Polyline(pAxis->hdcDraw, &pt[0], (int)n);
		}
		else if (n>0) {
			Axis_MoveToEx(pAxis, pt[0].x, pt[0].y);
			Axis_LineTo(pAxis, pt[0].x, pt[0].y);
		}
	}
}

void Axis_LineTo(Axis * pAxis, double x, double y) {
	Axis_CoordTransf(pAxis, &x,  &y);
	LineTo(pAxis->hdcDraw, (int)x, (int)y);
}

void Axis_MoveToEx(Axis * pAxis, double x, double y) {
	Axis_CoordTransf(pAxis, &x,  &y);
	MoveToEx(pAxis->hdcDraw, (int)x, (int)y, NULL);
}


template <class T> 
void Axis_AutoFitCore(
	Axis * pAxis, 
	size_t & count1, size_t & count2,
	double & xMin, double & xMax,
	double & yMin, double & yMax) {

	T *xPtr, *yPtr;
	size_t I = xVect_Count(pAxis->pSeries);

	while (true) {

		if (std::is_same<T, double>::value) {
			xPtr = (T*)pAxis->pSeries[count1].xDouble;
			yPtr = (T*)pAxis->pSeries[count1].yDouble;
		}
		else {
			xPtr = (T*)pAxis->pSeries[count1].xSingle;
			yPtr = (T*)pAxis->pSeries[count1].ySingle;
		}

		if (!(
				pAxis->pSeries[count1].n > 1 && count1 < I && (
					!_finite(xPtr[count2]) || 
					!_finite(yPtr[count2]) ||
					((pAxis->options & AXIS_OPT_XLOG) && xPtr[count2] <= 0) ||
					((pAxis->options & AXIS_OPT_YLOG) && yPtr[count2] <= 0)
				)
			)
		) 
			break;

		while (true) {
			if (
					(
						!_finite(xPtr[count2]) || 
						!_finite(yPtr[count2]) || 
						((pAxis->options & AXIS_OPT_XLOG) && xPtr[count2] <= 0) ||
						((pAxis->options & AXIS_OPT_YLOG) && yPtr[count2] <= 0)
					) &&
					count2 < pAxis->pSeries[count1].n
				)
			{
				count2++;
			}
			else {
				goto break_loop;
			}
		}
		count2 = 0;
		count1++;
	}
break_loop:

	xMax = xMin = xPtr[count2];
	yMax = yMin = yPtr[count2];
}

template <class T> 
void Axis_AutoFitCore2(
	AxisSerie * pSerie, 
	size_t count2,
	double & xMin, double & xMax,
	double & yMin, double & yMax) {

	T * xPtr, * yPtr;

	if (std::is_same<T, double>::value) {
		xPtr = (T*)pSerie->xDouble;
		yPtr = (T*)pSerie->yDouble;
	}
	else {
		xPtr = (T*)pSerie->xSingle;
		yPtr = (T*)pSerie->ySingle;
	}

	size_t N = pSerie->n;

	auto xLog = pSerie->pAxis->options & AXIS_OPT_XLOG;
	auto yLog = pSerie->pAxis->options & AXIS_OPT_YLOG;

	for (size_t j = count2; j<N; j++) {
		if (_finite(xPtr[j]) && _finite(yPtr[j])) {
			if (xPtr[j] > xMax) xMax = xPtr[j];
			if (xPtr[j] < xMin && (!xLog || xPtr[j] > 0)) xMin = xPtr[j];
			if (yPtr[j] > yMax) yMax = yPtr[j];
			if (yPtr[j] < yMin && (!yLog || yPtr[j] > 0)) yMin = yPtr[j];
		}
	}
}

template <class T> 
void Axis_ColorMapAutoFitCore(Axis * pAxis, bool force = false) {

	if (!pAxis->pMatrixView) return;

	T * ptr = std::is_same<T, double>::value ? (T*)pAxis->pMatrixView->mDouble : (T*)pAxis->pMatrixView->mSingle;

	if (ptr) {
		
		const size_t I = pAxis->pMatrixView->nRow;
		const size_t J = pAxis->pMatrixView->nCol;

		int isFinite = _finite(ptr[0]);

		double vMin = (double)ptr[0];
		double vMax = (double)ptr[0];
		for (size_t i = 0; i < I*J; ++i) {
			double val = (double)ptr[i];
			if (_finite(val)) {
				if (isFinite){
					if (val > vMax) vMax = val;
					if (val < vMin) vMin = val;
				}
				else {
					vMin = vMax = val;
					isFinite = true;
				}
			}
		}

		Axis_cLim(pAxis, vMin, vMax, force);
	}
}

void Axis_ColorMapAutoFit(Axis * pAxis, bool force = false) {
	if (pAxis->pMatrixView->options & AXISMATRIXVIEW_OPT_DOUBLEPREC)
		Axis_ColorMapAutoFitCore<double>(pAxis, force);
	else
		Axis_ColorMapAutoFitCore<float>(pAxis, force);
}

void Axis_AutoFit(Axis *pAxis, bool colorAutoFit, bool forceAdjustZoom) {

	double xMin = 0, xMax = 1, yMin = 0, yMax = 1;
	double tolRelDelta = 0.1;
	size_t count1 = 0;
	size_t count2 = 0;
	AxisSerie *pSerie = nullptr;
	
	if (xVect_Count(pAxis->subAxis)) {

		auto hack = pAxis->subAxis;
		pAxis->subAxis = nullptr;
		Axis_AutoFit(pAxis, colorAutoFit, forceAdjustZoom);
		pAxis->subAxis = hack;

		auto xMin_orig = pAxis->xMin_orig;
		auto xMax_orig = pAxis->xMax_orig;

		xVect_static_for(pAxis->subAxis, i) {
			auto pSubAxis = pAxis->subAxis[i];

			if (xVect_Count(pSubAxis->pSeries) == 0) continue;

			Axis_AutoFit(pSubAxis, colorAutoFit, forceAdjustZoom);
			if (pSubAxis->xMin_orig < xMin_orig) xMin_orig = pSubAxis->xMin_orig;
			if (pSubAxis->xMax_orig > xMax_orig) xMax_orig = pSubAxis->xMax_orig;
		}

		Axis_xLim(pAxis, xMin_orig, xMax_orig);

		xVect_static_for(pAxis->subAxis, i) {
			auto pSubAxis = pAxis->subAxis[i];

			Axis_xLim(pSubAxis, xMin_orig, xMax_orig);
		}

		return;
	}

	Critical_Section(pAxis->pCs) {

		xVect_static_for(pAxis->pSeries, i) {
			if (pAxis->pSeries[i].n && pAxis->pSeries[i].xDouble && pAxis->pSeries[i].yDouble) {
				pSerie = &pAxis->pSeries[i];
				count1 = i;
				break;
			}
		}

		if (pSerie) {
			if (pAxis->pSeries[count1].doublePrec) 
				Axis_AutoFitCore<double>(pAxis, count1, count2, xMin, xMax, yMin, yMax);
			else 
				Axis_AutoFitCore<float> (pAxis, count1, count2, xMin, xMax, yMin, yMax);

			bool doSetLim = false;

			for (size_t i = count1, I = xVect_Count(pAxis->pSeries); i < I; i++) {
				if (pAxis->pSeries[i].n > 1) {

					doSetLim = true;
					AxisSerie * pSerie = &pAxis->pSeries[i];

					if (pSerie->doublePrec) 
						Axis_AutoFitCore2<double>(pSerie, count2, xMin, xMax, yMin, yMax);
					else 
						Axis_AutoFitCore2<float>(pSerie, count2, xMin, xMax, yMin, yMax);

					count2 = 0;
				}
			}

			xMin = xMin*(1 - tolRelDelta*sign(xMin));
			xMax = xMax*(1 + tolRelDelta*sign(xMax));
			yMin = yMin*(1 - tolRelDelta*sign(yMin));
			yMax = yMax*(1 + tolRelDelta*sign(yMax));

			if (xMin == xMax) xMax = 1 + xMin;
			if (yMin == yMax) yMax = 1 + yMin;
		
			if (doSetLim) Axis_SetLim(pAxis, xMin, xMax, yMin, yMax, forceAdjustZoom);
		}

		if (pAxis->pMatrixView) {

			auto matrixOrientation = pAxis->pMatrixView->options & AXISMATRIXVIEW_OPT_MATRIXORIENTATION;
				
			auto matView_xMin = pAxis->pMatrixView->xCoord ? pAxis->pMatrixView->xCoord[0] : 0;

			auto matView_xMax = pAxis->pMatrixView->xCoord ? xVect_Last(pAxis->pMatrixView->xCoord) :
								matrixOrientation ? pAxis->pMatrixView->nCol : pAxis->pMatrixView->nRow;

			auto matView_yMin = pAxis->pMatrixView->yCoord ? pAxis->pMatrixView->yCoord[0] : 0;

			auto matView_yMax = pAxis->pMatrixView->yCoord ? xVect_Last(pAxis->pMatrixView->yCoord) :
								matrixOrientation ? pAxis->pMatrixView->nRow :  pAxis->pMatrixView->nCol;

			auto series = false;

			if (xVect_Count(pAxis->pSeries)) {

				xVect_static_for(pAxis->pSeries, i) {
					if (pAxis->pSeries[i].n > 0) {
						series = true;
						break;
					}
				}
			}

			if (series) {
				if (matView_xMax > xMax || !std::isfinite(xMax)) xMax = (double)matView_xMax;
				if (matView_yMax > yMax || !std::isfinite(yMax)) yMax = (double)matView_yMax;
				if (matView_xMin < xMin || !std::isfinite(xMin)) xMin = (double)matView_xMin;
				if (matView_yMin < yMin || !std::isfinite(yMin)) yMin = (double)matView_yMin;
			} else {
				xMax = (double)matView_xMax;
				yMax = (double)matView_yMax;
				xMin = (double)matView_xMin;
				yMin = (double)matView_yMin;
			}

			Axis_SetLim(pAxis, xMin, xMax, yMin, yMax, forceAdjustZoom);

			if (colorAutoFit) Axis_ColorMapAutoFit(pAxis, forceAdjustZoom);
		}
	}
}

void PlotDiagCrossMarker(HDC hdc, int x, int y, int nPixel) {

	auto x_m = x - nPixel;
	auto y_m = y - nPixel;
	auto x_p = x_m + 2 * nPixel;
	auto y_p = y_m + 2 * nPixel;

	MoveToEx	(hdc,	x_p,		y_m,		NULL);
	LineTo		(hdc,	x_m - 1,	y_p + 1);
	MoveToEx	(hdc,	x_m,		y_m,		NULL);
	LineTo		(hdc,	x_p + 1,	y_p + 1);
}

void PlotCircleMarker(HDC hdc, int x, int y, int nPixel) {

	auto x_m = x - nPixel;
	auto y_m = y - nPixel;
	auto x_p = x_m + 2 * nPixel;
	auto y_p = y_m + 2 * nPixel;

	Ellipse(hdc, x_m, y_m, x_p, y_p);
}

void AxisSerie_PlotMarkers(AxisSerie * pSerie) {

	AxisSerieMarker * pMarker = pSerie->pMarker;

	if (pMarker) {
	
		Axis * pAxis = pSerie->pAxis;
		size_t I = pSerie->n;
		HDC hdc = Axis_GetHdc(pAxis);
		Axis_SelectPen(pAxis, CreatePen(PS_SOLID, 1, pMarker->color));

		decltype(&PlotDiagCrossMarker) func = nullptr;

		switch(pMarker->markerType) {

			case AXIS_MARKER_DIAGCROSS: {
				func = PlotDiagCrossMarker;
				break;
			}
			case AXIS_MARKER_CIRCLE: {
				func = PlotCircleMarker;
				break;
			}
			default : {
				MY_ERROR("TODO");
			}
		}

		for (unsigned int i = 0; i<I; i++) {

			double x = pSerie->doublePrec ? pSerie->xDouble[i] : pSerie->xSingle[i];
			double y = pSerie->doublePrec ? pSerie->yDouble[i] : pSerie->ySingle[i];

			auto pix_x = Axis_CoordTransf_X(pAxis, x);
			auto pix_y = Axis_CoordTransf_Y(pAxis, y);

			for (long j = 0; j < (long)pMarker->lineWidth; ++j) {
				func(hdc, pix_x + j, pix_y, pMarker->nPixel);
			}
		}

	}

}


void AxisSerie_PlotLines(AxisSerie * pSerie, bool toMetaFile) {

	size_t J = pSerie->n;
	size_t ind;
	double x,y;
	size_t flagReady;
	bool cond;

	Axis * pAxis = pSerie->pAxis;

	if (J>0 && pSerie->drawLines) {
		Axis_SelectPen(pAxis, CreatePenIndirect(&pSerie->pen));
		if (toMetaFile) {
			if (pSerie->n > 0) {
				flagReady = 0;
				cond = 0;

				if (pSerie->doublePrec) {
					for (unsigned int j = 0; j<pSerie->n; j++) {
						ind = (j == pSerie->n - 1) ? j : j + 1;

						if (!_finite(pSerie->xDouble[j]) || !_finite(pSerie->yDouble[j])) {
							flagReady = 0;
							cond = false;
						}
						else {
							x = pSerie->xDouble[ind];
							y = pSerie->yDouble[ind];
							cond = x > pAxis->xMin && x < pAxis->xMax && y > pAxis->yMin && y < pAxis->yMax;
							if (!cond && flagReady)
								flagReady--;
						}

						if (flagReady) {
							Axis_LineTo(pAxis, pSerie->xDouble[j], pSerie->yDouble[j]);
							if (cond) flagReady = 3;
						}
						else if (cond)  {
							Axis_MoveToEx(pAxis, pSerie->xDouble[j], pSerie->yDouble[j]);
							flagReady = 3;
						}
					}
				}
				else {
					for (unsigned int j = 0; j<pSerie->n; j++) {
						ind = (j == pSerie->n - 1) ? j : j + 1;

						if (!_finite(pSerie->xSingle[j]) || !_finite(pSerie->ySingle[j])) {
							flagReady = 0;
							cond = false;
						}
						else {
							x = pSerie->xSingle[ind];
							y = pSerie->ySingle[ind];
							cond = x > pAxis->xMin && x < pAxis->xMax && y > pAxis->yMin && y < pAxis->yMax;
							if (!cond && flagReady)
								flagReady--;
						}

						if (flagReady) {
							Axis_LineTo(pAxis, pSerie->xSingle[j], pSerie->ySingle[j]);
							if (cond) flagReady = 3;
						}
						else if (cond)  {
							Axis_MoveToEx(pAxis, pSerie->xSingle[j], pSerie->ySingle[j]);
							flagReady = 3;
						}
					}
				}
			}
		}
		else {
			if (pSerie->doublePrec) {
				Axis_PolyLine(pAxis, pSerie->xDouble, pSerie->yDouble, pSerie->n);
			}
			else {
				Axis_PolyLine(pAxis, pSerie->xSingle, pSerie->ySingle, pSerie->n);
			}
		}
	}
}

double AxisColorMap_GrayScale_vr[] = {0, 1/6.0f, 2/6.0f, 3/6.0f, 4/6.0f, 5/6.0f, 1};
COLORREF AxisColorMap_GrayScale_c[]  = {RGB(0,0,0), RGB(50,50,50), RGB(100,100,100), RGB(150,150,150), RGB(200,200,200), RGB(250,250,250)};

double AxisColorMap_HSV_vr[] = { 0, 1 / 7.0f, 2 / 7.0f, 3 / 7.0f, 4 / 7.0f, 5 / 7.0f, 6 / 7.0f, 1 };
COLORREF AxisColorMap_HSV_c[]  = {RGB(255,0,0), RGB(255,255,50), RGB(0,255,0), RGB(0,255,255), RGB(0,0,255), RGB(255,20,147), RGB(250,0,0)};

double AxisColorMap_Jet_vr[] = { 0, 1 / 4.0f, 2 / 4.0f, 3 / 4.0f, 1 };
COLORREF AxisColorMap_Jet_c[]  = {RGB(0,0,255), RGB(0,255,255), RGB(255,255,0), RGB(255,0,0)};

double AxisColorMap_Jet2_vr[] = { 0, 0.75 / 6.0f, 1.5 / 6.0f, 3.9 / 6.0f, 4.9 / 6.0f, 5.35 / 6.0f, 1 };
COLORREF AxisColorMap_Jet2_c[] = { RGB(0, 0, 110), RGB(0, 0, 255), RGB(0, 255, 255), RGB(255, 255, 0), RGB(255, 0, 0), RGB(112, 0, 0) };

double AxisColorMap_Hot_vr[] = { 0, 1 / 4.0f, 2 / 4.0f, 3 / 4.0f, 1 };
COLORREF AxisColorMap_Hot_c[]  = {RGB(0,0,0), RGB(255,0,0), RGB(255,255,0), RGB(255,255,255)};

double AxisColorMap_COSMOS_vr[] = {
	0,         1 / 40.,  2 / 40.,  3 / 40.,  4 / 40.,  5 / 40.,  6 / 40.,  7 / 40.,  8 / 40.,  9 / 40., 10 / 40., 11 / 40., 12 / 40., 13 / 40., 14 / 40.,
	15 / 40., 16 / 40., 17 / 40., 18 / 40., 19 / 40., 20 / 40., 21 / 40., 22 / 40., 23 / 40., 24 / 40., 25 / 40., 26 / 40., 27 / 40., 28 / 40., 29 / 40., 
	30 / 40., 31 / 40., 32 / 40., 33 / 40., 34 / 40., 35 / 40., 36 / 40., 37 / 40., 38 / 40., 39 / 40., 1
};

COLORREF AxisColorMap_COSMOS_c[] = {
	RGB(0, 0, 112),   RGB(0, 0, 152),   RGB(0, 0, 192),   RGB(0, 0, 232),   RGB(16, 16, 255), RGB(64, 64, 255), RGB(0, 120, 255), RGB(0, 136, 255), 
	RGB(0, 152, 255), RGB(0, 168, 255), RGB(0, 192, 192), RGB(0, 208, 208), RGB(0, 232, 232), RGB(0, 248, 248), RGB(0, 255, 168), RGB(0, 255, 152), 
	RGB(0, 255, 136), RGB(0, 255, 120), RGB(0, 208, 0),   RGB(0, 232, 0),   RGB(0, 248, 0),   RGB(16, 255, 16), RGB(120, 255, 0), RGB(136, 255, 0), 
	RGB(152, 255, 0), RGB(168, 255, 0), RGB(248, 248, 8), RGB(240, 240, 0), RGB(224, 224, 0), RGB(200, 200, 0), RGB(255, 120, 0), RGB(240, 104, 0), 
	RGB(216, 88, 0),  RGB(200, 72, 0),  RGB(255, 64, 64), RGB(255, 16, 16), RGB(232, 0, 0),   RGB(192, 0, 0),   RGB(152, 0, 0),   RGB(112, 0, 0)
};

void AxisMatrixView_DestroyColorMap(AxisMatrixView * pMatView) {

	if (!pMatView || pMatView->pAxis->options & AXIS_OPT_ISMATVIEWLEGEND) return;

	AxisColorMap * pCMap = pMatView->cMap;
	if (!pCMap) return;

	xVect_Free<AxisFree>(pCMap->c);
	xVect_Free<AxisFree>(pCMap->v);
	xVect_Free<AxisFree>(pCMap->vr);

	if (Axis * legendCmap = pCMap->legendCmap) {
		legendCmap->options |= AXIS_OPT_GHOST;
		AdimPos * pAdimPos = &pMatView->pAxis->previousMatViewSettings.legendAdimPos;

		pAdimPos->nX = legendCmap->adim_nX;
		pAdimPos->nY = legendCmap->adim_nY;
		pAdimPos->dx = legendCmap->adim_dx;
		pAdimPos->dy = legendCmap->adim_dy;
		pAdimPos->initialized = true;

		if (!g_EndOfTimes) {
			if (legendCmap->hWnd && legendCmap->hdc) {
				GDI_SAFE_CALL(ReleaseDC(legendCmap->hWnd, legendCmap->hdc));
				legendCmap->hdc = 0;
			}
			Axis_DestroyAsync(legendCmap);
		}
		else {
			Axis_Destroy(legendCmap);
		}
		legendCmap = nullptr;
	}

	AxisFree(pCMap);
	pMatView->cMap = nullptr;
}

void AxisMatrixView_CreateLegendCMap(AxisColorMap * pCmap) {

	Axis * pMainAxis = pCmap->pMatView->pAxis;

	if (pMainAxis->DrawArea_nPixels_x > 0 && pMainAxis->DrawArea_nPixels_y > 0) {
	
		double nX, nY, dx, dy;

		if (pMainAxis->options & AXIS_OPT_WINDOWED) {
			nX = 1;
			nY = 1;
		}
		else {
			nX = pMainAxis->adim_nX;
			nY = pMainAxis->adim_nY;
		}

		if (pMainAxis->previousMatViewSettings.legendAdimPos.initialized) {
			dx = pMainAxis->previousMatViewSettings.legendAdimPos.dx;
			dy = pMainAxis->previousMatViewSettings.legendAdimPos.dy;
		}
		else {
			dx = nX * 0.9;
			dy = nY * 0.05;
		}

		nX *= 40  / (double)pMainAxis->DrawArea_nPixels_x;
		nY *= 100 / (double)pMainAxis->DrawArea_nPixels_y;

		if (pCmap->legendCmap) MY_ERROR("pCmap->legendCmap supposed to be nullptr");

		pCmap->legendCmap = Axis_CreateAt(
			nX, nY, dx, dy, pMainAxis,
			AXIS_OPT_ADIM_MODE | 
			AXIS_OPT_BORDERS   | 
			AXIS_OPT_DISABLED  | 
			AXIS_OPT_ISMATVIEWLEGEND,
			L"ColorMap");
	}

	if (pCmap->legendCmap) {
	
		AxisBorder_DestroyTicks(pCmap->legendCmap->pX_Axis);
	
		pCmap->legendCmap->pTitle->marge   = 6;
		pCmap->legendCmap->pX_Axis->marge  = 6;
		pCmap->legendCmap->pY_Axis->marge  = 8;
		pCmap->legendCmap->pY_Axis2->marge = 4;

		Axis_SetTickLabelsFontSize(pCmap->legendCmap, 10);

		AxisBorder_ComputeExtent(pCmap->legendCmap->pTitle);
		AxisBorder_ComputeExtent(pCmap->legendCmap->pX_Axis);
		AxisBorder_ComputeExtent(pCmap->legendCmap->pY_Axis);
		AxisBorder_ComputeExtent(pCmap->legendCmap->pY_Axis2);

		Axis_UpdatePlotAreaRect(pCmap->legendCmap);

		const short N = 200;
		double colorVector[N];

		for (short i = 0; i < N; ++i) {
			colorVector[i] = pCmap->vMin + (i / (double)N)* (pCmap->vMax - pCmap->vMin);
		}

		Axis_CreateMatrixView(pCmap->legendCmap, colorVector, 1, N, pCmap->pMatView->options & ~AXISMATRIXVIEW_OPT_MATRIXORIENTATION);
		if (pCmap->legendCmap->pMatrixView->cMap) MY_ERROR("Legend colormap should alias axis colormap");
		pCmap->legendCmap->pMatrixView->cMap = pCmap;
	}
}

void AxisMatrixView_CreateColorMap(
	AxisMatrixView * pMatView,
	double vMin, double vMax,
	double vMin_orig, double vMax_orig,
	double * vr, COLORREF * c, size_t n)
{

	if (!pMatView) MY_ERROR("AxisMatrixView_CreateColorMap : pMatView = nullptr");

	if (pMatView->pAxis->hdc == 0) return;

	if (pMatView->cMap) {
		if (pMatView->cMap->c == c)  pMatView->cMap->c = nullptr;
		if (pMatView->cMap->vr == vr) pMatView->cMap->vr = nullptr;
		AxisMatrixView_DestroyColorMap(pMatView);
	}

	if (pMatView->options & AXIS_CMAP_COLORREF) return;

	if (pMatView->pAxis->options & AXIS_OPT_ISMATVIEWLEGEND) {

	}
	else {
		pMatView->cMap = (AxisColorMap*)AxisMalloc(sizeof AxisColorMap);

		AxisColorMap * pCmap = pMatView->cMap;

		pCmap->pMatView = pMatView;
		pCmap->vMin = vMin;
		pCmap->vMax = vMax;

		pCmap->vMin_orig = vMin_orig;
		pCmap->vMax_orig = vMax_orig;

		pCmap->c = xVect_Create<COLORREF, false, AxisMalloc>(n);
		pCmap->v = xVect_Create<double, false, AxisMalloc>(n + 1);
		pCmap->vr = xVect_Create<double, false, AxisMalloc>(n + 1);
		pCmap->legendCmap = nullptr;

		bool interpol = (pMatView->options & AXISMATRIXVIEW_OPT_INTERPOL) != 0;

		for (size_t i = 0; i < n; ++i) {
			pCmap->c[i] = c[i];
		}

		if (interpol) {
			double fact = vr[n] / vr[n - 1];
			for (size_t i = 0; i < n; ++i) {
				pCmap->vr[i] = fact * vr[i];
			}
			pCmap->vr[n] = vr[n];
		}
		else {
			for (size_t i = 0; i < n + 1; ++i) {
				pCmap->vr[i] = vr[i];
			}
		}

		for (size_t i = 0; i < n + 1; ++i) {
			pCmap->v[i] = vMin + pCmap->vr[i] * (vMax - vMin);
		}

		if (pMatView->pAxis->options & AXIS_OPT_LEGEND) {
			AxisMatrixView_CreateLegendCMap(pCmap);
			Axis_Refresh(pMatView->pAxis, false, false, false);
		}
	}
}

void AxisMatrixView_CreateColorMap(AxisMatrixView * pMatView, double vMin, double vMax,
								   double * vr, COLORREF * c, size_t n) {

	AxisMatrixView_CreateColorMap(pMatView, vMin, vMax, vMin, vMax, vr, c, n);
}

void AxisMatrixView_CreateColorMap(
	AxisMatrixView * pMatView, 
	double vMin, double vMax, 
	double vMin_orig, double vMax_orig,
	DWORD options) 
{
	
	DWORD mask1 =
		AXISMATRIXVIEW_OPT_DOUBLEPREC |
		AXISMATRIXVIEW_OPT_INITIALIZED |
		AXISMATRIXVIEW_OPT_MATRIXORIENTATION |
		AXISMATRIXVIEW_OPT_VMAX_INITIALIZED |
		AXISMATRIXVIEW_OPT_VMIN_INITIALIZED |
		AXISMATRIXVIEW_OPT_VMAXO_INITIALIZED |
		AXISMATRIXVIEW_OPT_VMINO_INITIALIZED;

	SET_DWORD_BYTE_MASK(pMatView->options, ~mask1, false);
	SET_DWORD_BYTE_MASK(options, mask1, false);

	pMatView->options |= options;

	DWORD mask2 = mask1 | AXISMATRIXVIEW_OPT_INTERPOL;

	SET_DWORD_BYTE_MASK(options, mask2, false);

	switch (options) {
		case AXIS_CMAP_GRAYSCALE : {
			AxisMatrixView_CreateColorMap(pMatView, vMin, vMax, vMin_orig, vMax_orig, 
				AxisColorMap_GrayScale_vr,AxisColorMap_GrayScale_c, NUMEL(AxisColorMap_GrayScale_c));
			break;
		}
		case AXIS_CMAP_HSV : {
			AxisMatrixView_CreateColorMap(pMatView, vMin, vMax, vMin_orig, vMax_orig, 
				AxisColorMap_HSV_vr, AxisColorMap_HSV_c, NUMEL(AxisColorMap_HSV_c));
			break;
		}
		case AXIS_CMAP_JET2: {
			AxisMatrixView_CreateColorMap(pMatView, vMin, vMax, vMin_orig, vMax_orig,
				AxisColorMap_Jet2_vr, AxisColorMap_Jet2_c, NUMEL(AxisColorMap_Jet2_c));
			break;
		}
		case AXIS_CMAP_JET : {
			AxisMatrixView_CreateColorMap(pMatView, vMin, vMax,	vMin_orig, vMax_orig, 
				AxisColorMap_Jet_vr, AxisColorMap_Jet_c, NUMEL(AxisColorMap_Jet_c));
			break;
		}
		case AXIS_CMAP_HOT : {
			AxisMatrixView_CreateColorMap(pMatView, vMin, vMax, vMin_orig, vMax_orig,
				AxisColorMap_Hot_vr, AxisColorMap_Hot_c, NUMEL(AxisColorMap_Hot_c));
			break;
		}
		case AXIS_CMAP_COSMOS : {
			AxisMatrixView_CreateColorMap(pMatView, vMin, vMax, vMin_orig, vMax_orig,
				AxisColorMap_COSMOS_vr, AxisColorMap_COSMOS_c, NUMEL(AxisColorMap_COSMOS_c));
			break;
		}
		case AXIS_CMAP_COLORREF: {
			AxisMatrixView_CreateColorMap(pMatView, 0, 0, 0, 0, nullptr, nullptr, 0);
			break;
		}
		default: {
			MY_ERROR("Unkown mapID");
		}
	}
}

void AxisMatrixView_CreateColorMap(AxisMatrixView * pMatView, double vMin, double vMax, DWORD options) {

	AxisMatrixView_CreateColorMap(pMatView, vMin, vMax, vMin, vMax, options);
}

void Axis_SetMatrixViewColorMap(Axis * pAxis, DWORD options) {

	Critical_Section(pAxis->pCs) {
		if (pAxis->pMatrixView && (pAxis->pMatrixView->options & AXISMATRIXVIEW_OPT_INTERPOL))
			options |= AXISMATRIXVIEW_OPT_INTERPOL;

		if (!pAxis->pMatrixView) Axis_CreateMatrixView(pAxis, (double*)nullptr, 0, 0);
		AxisMatrixView_CreateColorMap(pAxis->pMatrixView, options);
	}
}

void Axis_SetColorMap(Axis * pAxis, DWORD options) {
	Axis_SetMatrixViewColorMap(pAxis, options);
}

void Axis_SetMatrixViewColorMap(Axis * pAxis, double vMin, double vMax, DWORD options) {

	if (pAxis->pMatrixView && (pAxis->pMatrixView->options & AXISMATRIXVIEW_OPT_INTERPOL))
		options |= AXISMATRIXVIEW_OPT_INTERPOL;

	if (!pAxis->pMatrixView) Axis_CreateMatrixView(pAxis, (double*)nullptr, 0, 0);
	AxisMatrixView_CreateColorMap(pAxis->pMatrixView, vMin, vMax, options);
}

void Axis_SetMatrixViewColorMap(Axis * pAxis, double vMin, double vMax) {

	if (!pAxis->pMatrixView) Axis_CreateMatrixView(pAxis, (double*)nullptr, 0, 0);
	AxisMatrixView_CreateColorMap(pAxis->pMatrixView, vMin, vMax, AXIS_CMAP_DEFAULT);
}

void Axis_SetMatrixViewColorMap(Axis * pAxis, double vMin, double vMax,
	double * vr, COLORREF * c, size_t n) {

	if (!pAxis->pMatrixView) Axis_CreateMatrixView(pAxis, (double*)nullptr, 0, 0);
	AxisMatrixView_CreateColorMap(pAxis->pMatrixView, vMin, vMax, vr, c, n);
}

void Axis_SetMatrixViewColorMap(
	Axis * pAxis, 
	double vMin, double vMax,
	double vMin_orig, double vMax_orig,
	double * vr, COLORREF * c, size_t n) 
{

	if (!pAxis->pMatrixView) Axis_CreateMatrixView(pAxis, (double*)nullptr, 0, 0);
	AxisMatrixView_CreateColorMap(pAxis->pMatrixView, vMin, vMax, vMin_orig, vMax_orig, vr, c, n);
}

void AxisMatrixView_CreateColorMap_GetcLims(
	AxisMatrixView * pMatrixView,
	double * vMin, double * vMax,
	double * vMin_orig, double * vMax_orig) {

	if (pMatrixView) {
		if (pMatrixView->cMap) {
			*vMin = pMatrixView->cMap->vMin;
			*vMax = pMatrixView->cMap->vMax;
			*vMin_orig = pMatrixView->cMap->vMin_orig;
			*vMax_orig = pMatrixView->cMap->vMax_orig;
		}
		else {
			*vMin = pMatrixView->pAxis->previousMatViewSettings.options & AXISMATRIXVIEW_OPT_VMIN_INITIALIZED ?
				pMatrixView->pAxis->previousMatViewSettings.cMap_vMin : 0;
			*vMax = pMatrixView->pAxis->previousMatViewSettings.options & AXISMATRIXVIEW_OPT_VMAX_INITIALIZED ?
				pMatrixView->pAxis->previousMatViewSettings.cMap_vMax : 1;
			*vMin_orig = pMatrixView->pAxis->previousMatViewSettings.options & AXISMATRIXVIEW_OPT_VMINO_INITIALIZED ?
				pMatrixView->pAxis->previousMatViewSettings.cMap_vMin_orig : 0;
			*vMax_orig = pMatrixView->pAxis->previousMatViewSettings.options & AXISMATRIXVIEW_OPT_VMINO_INITIALIZED ?
				pMatrixView->pAxis->previousMatViewSettings.cMap_vMax_orig : 1;
		}
	}
	else {
		*vMin = 0;
		*vMax = 1;
		*vMin_orig = 0;
		*vMax_orig = 1;
	}
}

void AxisMatrixView_CreateColorMap(AxisMatrixView * pMatrixView, DWORD options) {

	double vMin, vMax, vMin_orig, vMax_orig;
	AxisMatrixView_CreateColorMap_GetcLims(pMatrixView, &vMin, &vMax, &vMin_orig, &vMax_orig);

	AxisMatrixView_CreateColorMap(pMatrixView, vMin, vMax, vMin_orig, vMax_orig, options);
}

void AxisMatrixView_CreateColorMap(AxisMatrixView * pMatrixView, double * vr, COLORREF * c, size_t n) {

	double vMin, vMax, vMin_orig, vMax_orig;
	AxisMatrixView_CreateColorMap_GetcLims(pMatrixView, &vMin, &vMax, &vMin_orig, &vMax_orig);

	AxisMatrixView_CreateColorMap(pMatrixView, vMin, vMax, vMin_orig, vMax_orig, vr, c, n);
}

void AxisMatrixView_CreateColorMap(AxisMatrixView * pMatrixView, double * vr, COLORREF * c, size_t n, DWORD options) {

	pMatrixView->options = options;
	AxisMatrixView_CreateColorMap(pMatrixView, vr, c, n);
}

void Axis_SetMatrixViewColorMap(Axis * pAxis, double * vr, COLORREF * c, size_t n) {

	if (!pAxis->pMatrixView) Axis_CreateMatrixView(pAxis, (double*)nullptr, 0, 0);
	AxisMatrixView_CreateColorMap(pAxis->pMatrixView, vr, c, n);
}

void Axis_DestroyMatrixView(Axis * pAxis) {
	
	Critical_Section(pAxis->pCs) {

		AxisMatrixView * pMatrixView = pAxis->pMatrixView;
		if (!pMatrixView) return;

		if (pMatrixView->cMap)		AxisMatrixView_DestroyColorMap(pMatrixView);
		if (pMatrixView->mSingle)   {
			if (pMatrixView->options & AXISMATRIXVIEW_OPT_DOUBLEPREC) 
				xVect_Free<AxisFree>(pMatrixView->mDouble);
			else
				xVect_Free<AxisFree>(pMatrixView->mSingle);
		}
		if (pMatrixView->xCoord)	xVect_Free<AxisFree>(pMatrixView->xCoord);
		if (pMatrixView->yCoord)	xVect_Free<AxisFree>(pMatrixView->yCoord);

		AxisFree(pMatrixView);
		pAxis->pMatrixView = nullptr;
	}
}

void AxisMatrixView_SetMatrixOrientation(AxisMatrixView * pMatView, bool matrixOrientation) {

	bool matOrient = (pMatView->options & AXISMATRIXVIEW_OPT_MATRIXORIENTATION) != 0;

	SET_BYTE_MASK(pMatView->options, AXISMATRIXVIEW_OPT_MATRIXORIENTATION, matrixOrientation);

	if (matOrient != matrixOrientation) Swap(pMatView->xCoord, pMatView->yCoord);
}

void Axis_SetMatrixViewOrientation(Axis * pAxis, bool matrixOrientation) {

	if (!pAxis->pMatrixView) Axis_CreateMatrixView(pAxis, (double*)nullptr, 0, 0);
	if (!pAxis->pMatrixView) MY_ERROR("Axis_CreateMatrixView failed");

	AxisMatrixView_SetMatrixOrientation(pAxis->pMatrixView, matrixOrientation);
}

template <class T> 
void Axis_CreateMatrixViewCore(Axis * pAxis, T * m, size_t nRow, size_t nCol, DWORD options) {

	Critical_Section(pAxis->pCs) {

		bool matrixExisted = false;

		if (pAxis->pMatrixView) {
			if (pAxis->pMatrixView->cMap) {
				pAxis->previousMatViewSettings.options = pAxis->pMatrixView->options | AXISMATRIXVIEW_OPT_INITIALIZED;
				pAxis->previousMatViewSettings.cMap_vMin = pAxis->pMatrixView->cMap->vMin;
				pAxis->previousMatViewSettings.cMap_vMax = pAxis->pMatrixView->cMap->vMax;
				pAxis->previousMatViewSettings.options |= AXISMATRIXVIEW_OPT_VMIN_INITIALIZED | AXISMATRIXVIEW_OPT_VMAX_INITIALIZED;
				pAxis->previousMatViewSettings.cMap_vMax_orig = pAxis->pMatrixView->cMap->vMax_orig;
				pAxis->previousMatViewSettings.cMap_vMin_orig = pAxis->pMatrixView->cMap->vMin_orig;
				pAxis->previousMatViewSettings.options |= AXISMATRIXVIEW_OPT_VMINO_INITIALIZED | AXISMATRIXVIEW_OPT_VMAXO_INITIALIZED;
			}
			Axis_DestroyMatrixView(pAxis);
			matrixExisted = true;
		}

		pAxis->pMatrixView = (AxisMatrixView*)AxisMalloc(sizeof AxisMatrixView);
		*pAxis->pMatrixView = AxisMatrixView_ZeroInitializer;
		pAxis->pMatrixView->pAxis = pAxis;
		pAxis->pMatrixView->nRow = nRow;
		pAxis->pMatrixView->nCol = nCol;
		pAxis->pMatrixView->cMap = nullptr;
		pAxis->pMatrixView->xCoord = nullptr;
		pAxis->pMatrixView->yCoord = nullptr;
		pAxis->pMatrixView->options = options;

		if (std::is_same<T, double>::value) {
			pAxis->pMatrixView->options |= AXISMATRIXVIEW_OPT_DOUBLEPREC;
			pAxis->pMatrixView->mDouble = (double*)xVect_Create<T, false, AxisMalloc>(nRow*nCol);
		}
		else {
			pAxis->pMatrixView->options &= ~AXISMATRIXVIEW_OPT_DOUBLEPREC;
			pAxis->pMatrixView->mSingle = (float*)xVect_Create<T, false, AxisMalloc>(nRow*nCol);
		}

		memcpy(pAxis->pMatrixView->mDouble, m, nRow*nCol*sizeof(T));

		if (nRow > 0 && nCol > 0) Axis_AutoFit(pAxis, false, !matrixExisted);
	}
}

void Axis_CreateMatrixView(Axis * pAxis, double * m, size_t nRow, size_t nCol, DWORD options) {
	Axis_CreateMatrixViewCore<double>(pAxis, m, nRow, nCol, options);
}

void Axis_CreateMatrixView(Axis * pAxis, float * m, size_t nRow, size_t nCol, DWORD options) {
	Axis_CreateMatrixViewCore<float>(pAxis, m, nRow, nCol, options);
}

void Axis_CreateMatrixView(Axis * pAxis, double * m, size_t nRow, size_t nCol) {

	Critical_Section(pAxis->pCs) {

		if (pAxis->pMatrixView) {
			Axis_CreateMatrixViewCore<double>(pAxis, m, nRow, nCol, pAxis->pMatrixView->options);
		}
		else if (pAxis->previousMatViewSettings.options & AXISMATRIXVIEW_OPT_INITIALIZED) {
			Axis_CreateMatrixViewCore<double>(pAxis, m, nRow, nCol, pAxis->previousMatViewSettings.options);
		}
		else {
			Axis_CreateMatrixViewCore<double>(pAxis, m, nRow, nCol, AXIS_CMAP_DEFAULT);
		}
	}
}

void Axis_CreateMatrixView(Axis * pAxis, float * m, size_t nRow, size_t nCol) {

	Critical_Section(pAxis->pCs) {

		if (pAxis->pMatrixView) {
			Axis_CreateMatrixViewCore<float>(pAxis, m, nRow, nCol, pAxis->pMatrixView->options);
		}
		else if (pAxis->previousMatViewSettings.options & AXISMATRIXVIEW_OPT_INITIALIZED) {
			Axis_CreateMatrixViewCore<float>(pAxis, m, nRow, nCol, pAxis->previousMatViewSettings.options);
		}
		else {
			Axis_CreateMatrixViewCore<float>(pAxis, m, nRow, nCol, AXIS_CMAP_DEFAULT);
		}
	}
}


void AxisMatrixView_Draw(AxisMatrixView * pMatView) {

	if (!pMatView->cMap) AxisMatrixView_CreateColorMap(pMatView, 0, 1, pMatView->options);

	size_t I = pMatView->nRow;
	size_t J = pMatView->nCol;
	Axis * pAxis = pMatView->pAxis;
	bool doublePrec = (pMatView->options & AXISMATRIXVIEW_OPT_DOUBLEPREC) != 0;
	double * mDouble = pMatView->mDouble;
	float  * mSingle = pMatView->mSingle;
	COLORREF * c   = pMatView->cMap->c;
	double * v = pMatView->cMap->v;
	bool interpol = (pMatView->options & AXISMATRIXVIEW_OPT_INTERPOL) != 0;

	HGDIOBJ oldBrush = SelectObject(pAxis->hdcDraw, GetStockObject(NULL_BRUSH));
	HGDIOBJ oldPen = SelectObject(pAxis->hdcDraw, GetStockObject(NULL_PEN));

	size_t K = xVect_Count(v);
	size_t old_k = 1;

	if (K < 2) MY_ERROR("Axis_ColorMaps must have at least 2 colors");

	unsigned char red, blue, green;

	if (doublePrec) {
		for (size_t i = 0; i < I; ++i) {
			for (size_t j = 0; j<J; ++j) {
				double mv = mDouble[i*J + j];
				size_t k = old_k;
				bool do_interpolation = false;

				if (mv <= v[0]) k = 1;
				else if (mv >= v[K - 1]) k = K - 1;
				else {
					do_interpolation = interpol;
					for (;;)
					if (mv > v[k - 1])
					if (mv <= v[k]) break;
					else k++;
					else k--;
				}

				if (do_interpolation) {
					double alpha = (mv - v[k - 1]) / (v[k] - v[k - 1]);
					red = (unsigned char)(alpha*GetRValue(c[k]) + (1 - alpha)*GetRValue(c[k - 1]));
					green = (unsigned char)(alpha*GetGValue(c[k]) + (1 - alpha)*GetGValue(c[k - 1]));
					blue = (unsigned char)(alpha*GetBValue(c[k]) + (1 - alpha)*GetBValue(c[k - 1]));
				}
				else {
					red = GetRValue(c[k - 1]);
					green = GetGValue(c[k - 1]);
					blue = GetBValue(c[k - 1]);
				}

				old_k = k;
				Axis_DrawRectangle2(pAxis, (double)j, (double)i, (double)j + 1, (double)i + 1, RGB(blue, green, red));
			}
		}
	}
	else {
		for (size_t i = 0; i < I; ++i) {
			for (size_t j = 0; j<J; ++j) {
				double mv = (double)mSingle[i*J + j];
				size_t k = old_k;
				bool do_interpolation = false;

				if (mv <= v[0]) k = 1;
				else if (mv >= v[K - 1]) k = K - 1;
				else {
					do_interpolation = interpol;
					for (;;)
					if (mv > v[k - 1])
					if (mv <= v[k]) break;
					else k++;
					else k--;
				}

				if (do_interpolation) {
					double alpha = (mv - v[k - 1]) / (v[k] - v[k - 1]);
					red = (unsigned char)(alpha*GetRValue(c[k]) + (1 - alpha)*GetRValue(c[k - 1]));
					green = (unsigned char)(alpha*GetGValue(c[k]) + (1 - alpha)*GetGValue(c[k - 1]));
					blue = (unsigned char)(alpha*GetBValue(c[k]) + (1 - alpha)*GetBValue(c[k - 1]));
				}
				else {
					red = GetRValue(c[k - 1]);
					green = GetGValue(c[k - 1]);
					blue = GetBValue(c[k - 1]);
				}

				old_k = k;
				Axis_DrawRectangle2(pAxis, (double)j, (double)i, (double)j + 1, (double)i + 1, RGB(blue, green, red));
			}
		}
	}

	SelectObject(pAxis->hdcDraw, oldBrush);
	SelectObject(pAxis->hdcDraw, oldPen);
}

void AxisMatrixView_Draw2(AxisMatrixView * pMatView) {

	if (!pMatView->cMap) AxisMatrixView_CreateColorMap(pMatView, 0, 1, pMatView->options);
	if (!pMatView->cMap) MY_ERROR("AxisMatrixView_CreateColorMap failes");

	size_t J = pMatView->nCol;

	Axis * pAxis = pMatView->pAxis;
	bool doublePrec = (pMatView->options & AXISMATRIXVIEW_OPT_DOUBLEPREC) != 0;
	double * mDouble = pMatView->mDouble;
	float  * mSingle = pMatView->mSingle;
	COLORREF * c   = pMatView->cMap->c;
	double * v = pMatView->cMap->v;
	bool interpol = (pMatView->options & AXISMATRIXVIEW_OPT_INTERPOL) != 0;

	HGDIOBJ oldBrush = SelectObject(pAxis->hdcDraw, GetStockObject(NULL_BRUSH));
	HGDIOBJ oldPen   = SelectObject(pAxis->hdcDraw, GetStockObject(NULL_PEN));

	size_t K = xVect_Count(v);
	size_t old_k = 1;

	if (K < 2) MY_ERROR("Axis_ColorMaps must have at least 2 colors");

	unsigned char red, blue, green;

	double xMin = Max(pAxis->xMin, 0);
	double yMin = Max(pAxis->yMin, 0);
	double xMax = pAxis->xMax;
	double yMax = pAxis->yMax;

	size_t I1 = (size_t)Min((int)yMax+1, (int)pMatView->nRow);
	size_t J1 = (size_t)Min((int)xMax+1, (int)pMatView->nCol);
	size_t I0 = (size_t)Max(0, (int)yMin);
	size_t J0 = (size_t)Max(0, (int)xMin);

	if (doublePrec) {
		for (size_t i = I0; i < I1; ++i) {
			for (size_t j = J0; j<J1; ++j) {
				double mv = mDouble[i*J + j];
				size_t k = old_k;
				bool do_interpolation = false;

				if (mv <= v[0]) k = 1;
				else if (mv >= v[K - 1]) k = K - 1;
				else {
					do_interpolation = interpol;
					for (;;)
					if (mv > v[k - 1])
					if (mv <= v[k]) break;
					else k++;
					else k--;
				}

				if (do_interpolation) {
					double alpha = (mv - v[k - 1]) / (v[k] - v[k - 1]);
					red = (unsigned char)(alpha*GetRValue(c[k]) + (1 - alpha)*GetRValue(c[k - 1]));
					green = (unsigned char)(alpha*GetGValue(c[k]) + (1 - alpha)*GetGValue(c[k - 1]));
					blue = (unsigned char)(alpha*GetBValue(c[k]) + (1 - alpha)*GetBValue(c[k - 1]));
				}
				else {
					red = GetRValue(c[k - 1]);
					green = GetGValue(c[k - 1]);
					blue = GetBValue(c[k - 1]);
				}

				old_k = k;
				Axis_DrawRectangle3(pAxis, (double)j, (double)(i + 1), (double)(j + 1), (double)i, RGB(blue, green, red));
			}
		}
	}
	else {
		for (size_t i = I0; i<I1; ++i) {
			for (size_t j = J0; j<J1; ++j) {
				double mv = (double)mSingle[i*J + j];
				size_t k = old_k;
				bool do_interpolation = false;

				if (mv <= v[0]) k = 1;
				else if (mv >= v[K - 1]) k = K - 1;
				else {
					do_interpolation = interpol;
					for (;;)
					if (mv > v[k - 1])
					if (mv <= v[k]) break;
					else k++;
					else k--;
				}

				if (do_interpolation) {
					double alpha = (mv - v[k - 1]) / (v[k] - v[k - 1]);
					red = (unsigned char)(alpha*GetRValue(c[k]) + (1 - alpha)*GetRValue(c[k - 1]));
					green = (unsigned char)(alpha*GetGValue(c[k]) + (1 - alpha)*GetGValue(c[k - 1]));
					blue = (unsigned char)(alpha*GetBValue(c[k]) + (1 - alpha)*GetBValue(c[k - 1]));
				}
				else {
					red = GetRValue(c[k - 1]);
					green = GetGValue(c[k - 1]);
					blue = GetBValue(c[k - 1]);
				}

				old_k = k;
				Axis_DrawRectangle3(pAxis, (double)j, (double)(i + 1), (double)(j + 1), (double)i, RGB(blue, green, red));
			}
		}
	}

	SelectObject(pAxis->hdcDraw, oldBrush);
	SelectObject(pAxis->hdcDraw, oldPen);
}

template <bool trueColor = true, class T>
COLORREF AxisColorMap_GetColor(AxisColorMap * cMap, T mv) {

	return AxisColorMap_GetColor(mv, cMap->v, xVect_Count(cMap->v), 
		cMap->c, cMap->pMatView->options & AXISMATRIXVIEW_OPT_INTERPOL);
}

template <bool trueColor = true, class T>
COLORREF Axis_GetColorFromColorMap(Axis * pAxis, T mv) {

	return AxisColorMap_GetColor(pAxis->pMatrixView->cMap, mv);
}

template COLORREF Axis_GetColorFromColorMap<true, double>(Axis * pAxis, double mv);
template COLORREF Axis_GetColorFromColorMap<true, float> (Axis * pAxis, float  mv);

template <bool trueColor = true, class T>
COLORREF AxisColorMap_GetColor(
	T mv, 
	double * v, 
	size_t K, 
	COLORREF * c, 
	bool interpol) 
{

	static size_t old_k = 1;
	unsigned char red, blue, green;

	size_t k = old_k;
	bool do_interpolation = false;

	if (mv <= v[0]) k = 1;
	else {
		if (mv >= v[K - 1]) k = K - 1;
		else {
			do_interpolation = interpol;
			while (true) {
				if (mv > v[k - 1]) {
					if (mv <= v[k]) break;
					else k++;
				}
				else {
					if (!_finite(mv)) {
						k = -1;
						break;
					}
					else k--;
				}
			}
		}
	}

	if (k == -1) red = blue = green = 0;
	else {
		if (do_interpolation) {
			double alpha = (mv - v[k - 1]) / (v[k] - v[k - 1]);
			red = (unsigned char)(alpha*GetRValue(c[k]) + (1 - alpha)*GetRValue(c[k - 1]));
			green = (unsigned char)(alpha*GetGValue(c[k]) + (1 - alpha)*GetGValue(c[k - 1]));
			blue = (unsigned char)(alpha*GetBValue(c[k]) + (1 - alpha)*GetBValue(c[k - 1]));
		}
		else {
			red = GetRValue(c[k - 1]);
			green = GetGValue(c[k - 1]);
			blue = GetBValue(c[k - 1]);
		}
	}

	if (trueColor) return RGB(red, green, blue);
	else return RGB(blue, green, red);
}

template <double * AxisMatrixView::*mPtr, long(*CoordTransf)(Axis*, double)>
void AxisMatrixView_DrawElementsBoundary(AxisMatrixView * pMatView) {

	auto&& coord = pMatView->*mPtr;

	auto pAxis = pMatView->pAxis;

	auto&& pAxis_Min = mPtr == &AxisMatrixView::xCoord ? pAxis->xMin : pAxis->yMin;
	auto&& pAxis_Max = mPtr == &AxisMatrixView::xCoord ? pAxis->xMax : pAxis->yMax;

	if (coord) {
		double * prec = nullptr;
	
		xVect_static_for(coord, i) {
			auto&& c = coord[i];

			if (c >= pAxis_Min && c <= pAxis_Max) {
				if (prec) {
					auto&& pix1 = CoordTransf(pAxis, c);
					auto&& pix2 = CoordTransf(pAxis, *prec);
					if (abs(pix1 - pix2) <= 10)  return;
				}

				prec = &c;
			}
		}

		Axis_SetPenColor(pAxis, RGB(100, 100, 100));

		xVect_static_for(coord, i) {
			auto&& c = coord[i];

			double x0, x1, y0, y1;

			if (mPtr == &AxisMatrixView::xCoord) {
				x0 = x1 = c;
				y0 = pAxis->yMin;
				y1 = pAxis->yMax;
			}
			else {
				y0 = y1 = c;
				x0 = pAxis->xMin;
				x1 = pAxis->xMax;
			}

			Axis_MoveToEx(pAxis, x0, y0);
			Axis_LineTo(pAxis, x1, y1);
		}
	}
	else {
		if (abs(CoordTransf(pAxis, pAxis_Max) - CoordTransf(pAxis, pAxis_Min)) / (pAxis_Max - pAxis_Min) > 10) {
			Axis_SetPenColor(pAxis, RGB(100, 100, 100));
			for (long i = (long)pAxis_Min; i <= (long)pAxis_Max; ++i) {
				double x0, x1, y0, y1;
				if (mPtr == &AxisMatrixView::xCoord) {
					x0 = x1 = i;
					y0 = pAxis->yMin;
					y1 = pAxis->yMax;
				}
				else {
					y0 = y1 = i;
					x0 = pAxis->xMin;
					x1 = pAxis->xMax;
				}
				Axis_MoveToEx(pAxis, x0, y0);
				Axis_LineTo(pAxis, x1, y1);
			}
		}
	}
}

template <class T, bool matrixOrientation> 
void AxisMatrixView_Draw3Core(AxisMatrixView * pMatView) {

	if (!pMatView->cMap) {
		double vMin, vMax, vMin_orig, vMax_orig;
		AxisMatrixView_CreateColorMap_GetcLims(pMatView, &vMin, &vMax, &vMin_orig, &vMax_orig);
		AxisMatrixView_CreateColorMap(pMatView, vMin, vMax, vMin_orig, vMax_orig, pMatView->options);
	}

	Axis * pAxis = pMatView->pAxis;

	size_t bitmapWidth = pAxis->DrawArea_nPixels_x;
	size_t bitmapHeight = pAxis->DrawArea_nPixels_y;
	DWORD * pData = (DWORD*)pAxis->bitmapDrawPtr;

	T * m = (T*)pMatView->mSingle;
	double * v = pMatView->cMap ? pMatView->cMap->v : nullptr;

	size_t old_k = 1;
	size_t I = pMatView->nRow, J = pMatView->nCol;
	size_t K = xVect_Count(v);
	bool interpol = (pMatView->options & AXISMATRIXVIEW_OPT_INTERPOL) != 0;
	COLORREF * c = pMatView->cMap ? pMatView->cMap->c : nullptr;
	
	auto pAxis_iMin = matrixOrientation ? pAxis->yMin : pAxis->xMin;
	auto pAxis_jMin = matrixOrientation ? pAxis->xMin : pAxis->yMin;
	auto pAxis_iMax = matrixOrientation ? pAxis->yMax : pAxis->xMax;
	auto pAxis_jMax = matrixOrientation ? pAxis->xMax : pAxis->yMax;

	auto&& Axis_CoordTransf_I = matrixOrientation ? Axis_CoordTransf_Y : Axis_CoordTransf_X;
	auto&& Axis_CoordTransf_J = matrixOrientation ? Axis_CoordTransf_X : Axis_CoordTransf_Y;

	long i_min = Axis_CoordTransf_I(pAxis, pAxis_iMin);
	long i_max = Axis_CoordTransf_I(pAxis, pAxis_iMax);
	long j_min = Axis_CoordTransf_J(pAxis, pAxis_jMin);
	long j_max = Axis_CoordTransf_J(pAxis, pAxis_jMax);

	if (i_min > i_max) Swap(i_min, i_max);
	if (j_min > j_max) Swap(j_min, j_max);
	
	auto&& coord_I = matrixOrientation ? pMatView->yCoord : pMatView->xCoord;
	auto&& coord_J = matrixOrientation ? pMatView->xCoord : pMatView->yCoord;

	auto DrawArea_nPixels_dec_i = matrixOrientation ? pAxis->DrawArea_nPixels_dec_y : pAxis->DrawArea_nPixels_dec_x;
	auto DrawArea_nPixels_dec_j = matrixOrientation ? pAxis->DrawArea_nPixels_dec_x : pAxis->DrawArea_nPixels_dec_y;

	auto coord_I_N = xVect_Count(coord_I);
	auto coord_J_N = xVect_Count(coord_J);

	double v_i_min = coord_I ? coord_I[0] : 0;
	double v_i_max = coord_I ? coord_I[coord_I_N - 1] : I;
	double v_j_min = coord_J ? coord_J[0] : 0;
	double v_j_max = coord_J ? coord_J[coord_J_N - 1] : J;

	auto&& Axis_GetCoord_I = matrixOrientation ? Axis_GetCoordY : Axis_GetCoordX;
	auto&& Axis_GetCoord_J = matrixOrientation ? Axis_GetCoordX : Axis_GetCoordY;

	size_t i_I = 0;
	
	if ((coord_I && coord_I_N < 2) || (coord_I && coord_J_N < 2)) return;

	for (unsigned i = (unsigned)i_min; i < (unsigned)i_max; ++i) {

		double v_I = Axis_GetCoord_I(pAxis, i + DrawArea_nPixels_dec_i);

		if (v_I <= v_i_min || v_I >= v_i_max) continue;

		if (coord_I) {
			for (;; ++i_I) {
				if (i_I + 1 >= coord_I_N) MY_ERROR("gotcha");
				if (coord_I[i_I + 1] >= v_I) break;
			}
		} else {
			i_I = _mm_cvttsd_si32(_mm_load_sd(&v_I));
		}

		size_t i_J = matrixOrientation ? 0 : J - 1;

		for (unsigned j = (unsigned)j_min; j < (unsigned)j_max; ++j) {

			double v_J = Axis_GetCoord_J(pAxis, j + DrawArea_nPixels_dec_j);

			if (v_J <= v_j_min || v_J >= v_j_max) continue;

			if (coord_J) {
				for (;; matrixOrientation ? ++i_J : --i_J) {
					if (matrixOrientation) {
						if (i_J + 1 >= coord_J_N) MY_ERROR("gotcha");
						if (coord_J[i_J + 1] >= v_J) break;
					} else {
						if (i_J >= coord_J_N) MY_ERROR("gotcha");
						if (coord_J[i_J] <= v_J) break;
					}
				}
			} else {
				i_J = _mm_cvttsd_si32(_mm_load_sd(&v_J));
			}

			T mv = m[i_I*J + i_J];

			COLORREF color;

			if (pAxis->pMatrixView->options & AXIS_CMAP_COLORREF) {
				color = (COLORREF)mv;
				auto red   = GetRValue(color);
				auto green = GetGValue(color);
				auto blue  = GetBValue(color);
				color = RGB(blue, green, red);
			}
			else {
				color = AxisColorMap_GetColor<false>(mv, v, K, c, interpol);
			}

			(matrixOrientation ? pData[i*bitmapWidth + j] : pData[j*bitmapWidth + i]) = color;
		}
	}

	AxisMatrixView_DrawElementsBoundary<&AxisMatrixView::xCoord, Axis_CoordTransf_X>(pMatView);
	AxisMatrixView_DrawElementsBoundary<&AxisMatrixView::yCoord, Axis_CoordTransf_Y>(pMatView);
}

void AxisMatrixView_Draw3(AxisMatrixView * pMatView) {

	auto count = pMatView->options & AXISMATRIXVIEW_OPT_DOUBLEPREC ? xVect_Count(pMatView->mDouble) : xVect_Count(pMatView->mSingle);
	if (count == 0)  return;

	if (pMatView->options & AXISMATRIXVIEW_OPT_DOUBLEPREC) 
		if (pMatView->options & AXISMATRIXVIEW_OPT_MATRIXORIENTATION) 
			AxisMatrixView_Draw3Core<double, true>(pMatView);
		else 
			AxisMatrixView_Draw3Core<double, false>(pMatView);
	else 
		if (pMatView->options & AXISMATRIXVIEW_OPT_MATRIXORIENTATION) 
			AxisMatrixView_Draw3Core<float, true>(pMatView);
		else 
			AxisMatrixView_Draw3Core<float, false>(pMatView);
}

void Axis_DrawMatrix(Axis * pAxis) {
	if (pAxis->hdc) AxisMatrixView_Draw(pAxis->pMatrixView);
}

void Axis_DrawMatrix2(Axis * pAxis) {
	if (pAxis->hdc) AxisMatrixView_Draw2(pAxis->pMatrixView);
}

void Axis_DrawMatrix3(Axis * pAxis) {
	if (pAxis->hdc) AxisMatrixView_Draw3(pAxis->pMatrixView);
}

double Axis_cMin(Axis * axis, bool orig) {
	if (axis->pMatrixView && axis->pMatrixView->cMap)
		return orig ? axis->pMatrixView->cMap->vMin_orig : axis->pMatrixView->cMap->vMin;
	else if (axis->previousMatViewSettings.options & AXISMATRIXVIEW_OPT_VMAX_INITIALIZED) {
		return orig ? axis->previousMatViewSettings.cMap_vMin_orig : axis->previousMatViewSettings.cMap_vMin;
	}
	else return std::numeric_limits<double>::quiet_NaN();
}

double Axis_cMax(Axis * axis, bool orig) {
	if (axis->pMatrixView && axis->pMatrixView->cMap)
		return orig ? axis->pMatrixView->cMap->vMax_orig : axis->pMatrixView->cMap->vMax;
	else if (axis->previousMatViewSettings.options & AXISMATRIXVIEW_OPT_VMAX_INITIALIZED) {
		return orig ? axis->previousMatViewSettings.cMap_vMax_orig : axis->previousMatViewSettings.cMap_vMax;
	}
	else return std::numeric_limits<double>::quiet_NaN();
}

void Axis_cLim(Axis * pAxis, double vMin, double vMax, bool forceAdjustZoom, bool zoom) {

	if (!std::isfinite(vMin)) vMin = Axis_cMin(pAxis, false);
	if (!std::isfinite(vMax)) vMax = Axis_cMax(pAxis, false);

	Critical_Section(pAxis->pCs) {

		if (!pAxis->pMatrixView) Axis_CreateMatrixView(pAxis, (double*)nullptr, 0, 0);
		if (!pAxis->pMatrixView) MY_ERROR("Axis_CreateMatrixView failed");

		if (!pAxis->pMatrixView->cMap) {

			double vMin2use = pAxis->previousMatViewSettings.options & AXISMATRIXVIEW_OPT_VMIN_INITIALIZED ?
				pAxis->previousMatViewSettings.cMap_vMin : vMin;
			double vMax2use = pAxis->previousMatViewSettings.options & AXISMATRIXVIEW_OPT_VMAX_INITIALIZED ?
				pAxis->previousMatViewSettings.cMap_vMax : vMax;
			double vMin_orig2use = pAxis->previousMatViewSettings.options & AXISMATRIXVIEW_OPT_VMINO_INITIALIZED ?
				pAxis->previousMatViewSettings.cMap_vMin_orig : vMin;
			double vMax_orig2use = pAxis->previousMatViewSettings.options & AXISMATRIXVIEW_OPT_VMAXO_INITIALIZED ?
				pAxis->previousMatViewSettings.cMap_vMax_orig : vMax;

			bool wasZoomed = Axis_WasColorZoomed(pAxis);

			if (zoom) {
				AxisMatrixView_CreateColorMap(
					pAxis->pMatrixView, vMin, vMax,
					vMin_orig2use, vMax_orig2use,
					pAxis->pMatrixView->options);
			}
			else if (forceAdjustZoom || !wasZoomed) {
				AxisMatrixView_CreateColorMap(
					pAxis->pMatrixView, vMin, vMax,
					pAxis->pMatrixView->options);
			}
			else {
				AxisMatrixView_CreateColorMap(
					pAxis->pMatrixView, vMin2use, vMax2use,
					vMin, vMax,
					pAxis->pMatrixView->options);
			}
		}
		else {

			bool wasZoomed = Axis_IsColorZoomed(pAxis);

			if (zoom) {
				Axis_SetMatrixViewColorMap(
					pAxis, vMin, vMax,
					pAxis->pMatrixView->cMap->vMin_orig,
					pAxis->pMatrixView->cMap->vMax_orig,
					pAxis->pMatrixView->cMap->vr,
					pAxis->pMatrixView->cMap->c,
					xVect_Count(pAxis->pMatrixView->cMap->c));
			}
			else if (forceAdjustZoom || !wasZoomed) {
				Axis_SetMatrixViewColorMap(
					pAxis, vMin, vMax,
					pAxis->pMatrixView->cMap->vr,
					pAxis->pMatrixView->cMap->c,
					xVect_Count(pAxis->pMatrixView->cMap->c));
			}
			else {
				pAxis->pMatrixView->cMap->vMin_orig = vMin;
				pAxis->pMatrixView->cMap->vMax_orig = vMax;
			}
		}
	}
}

void Axis_cMin(Axis * pAxis, double vMin) {
	
	Critical_Section(pAxis->pCs) {
	
		double vMax = pAxis->pMatrixView ? pAxis->pMatrixView->cMap ? pAxis->pMatrixView->cMap->vMax : 1 : 1;
		Axis_cLim(pAxis, vMin, vMax);
	
	}
}

void Axis_cMax(Axis * pAxis, double vMax) {

	Critical_Section(pAxis->pCs) {

		double vMin = pAxis->pMatrixView ? pAxis->pMatrixView->cMap ? pAxis->pMatrixView->cMap->vMin : 0 : 0;
		Axis_cLim(pAxis, vMin, vMax);

	}
}

void AxisMatrixView_CopyCMap(AxisMatrixView * pMatrixView, AxisColorMap * cMap2Copy) {

	if (pMatrixView->cMap) AxisMatrixView_DestroyColorMap(pMatrixView);
	if (cMap2Copy)	{
		AxisMatrixView_CreateColorMap(pMatrixView, 
			cMap2Copy->vMin, cMap2Copy->vMax,
			cMap2Copy->vMin_orig, cMap2Copy->vMax_orig,
			cMap2Copy->vr, cMap2Copy->c, xVect_Count(cMap2Copy->c));
		
		if (!pMatrixView->cMap) MY_ERROR("AxisMatrixView_CreateColorMap failed");

		if (cMap2Copy->legendCmap && pMatrixView->cMap->legendCmap) {

			Axis * legendAxis = pMatrixView->cMap->legendCmap;
			Axis * legendAxisToCopy = cMap2Copy->legendCmap;

			if (cMap2Copy->pMatView->pAxis->options & AXIS_OPT_WINDOWED) {
				legendAxis->adim_dx = legendAxisToCopy->adim_dx;
				legendAxis->adim_dy = legendAxisToCopy->adim_dy;
			}
			else {
				legendAxis->adim_dx = legendAxisToCopy->adim_dx / cMap2Copy->pMatView->pAxis->adim_nX;
				legendAxis->adim_dy = legendAxisToCopy->adim_dy / cMap2Copy->pMatView->pAxis->adim_nY;
			}
			Axis_Resize(legendAxis);
		}
	}
}

void Axis_CopyMatrixView(Axis * pAxis, AxisMatrixView * pMatrixView2Copy) {

	if (pAxis->pMatrixView) Axis_DestroyMatrixView(pAxis);

	if (pMatrixView2Copy) {

		if (pMatrixView2Copy->options & AXISMATRIXVIEW_OPT_DOUBLEPREC) {
			Axis_CreateMatrixView(pAxis, pMatrixView2Copy->mDouble, pMatrixView2Copy->nRow,
				pMatrixView2Copy->nCol, pMatrixView2Copy->options);
		}
		else {
			Axis_CreateMatrixView(pAxis, pMatrixView2Copy->mSingle, pMatrixView2Copy->nRow,
				pMatrixView2Copy->nCol, pMatrixView2Copy->options);
		}

		if (!pAxis->pMatrixView) MY_ERROR("Axis_CreateMatrixView failed");

		xVect_Copy<AxisMalloc, AxisRealloc>(pAxis->pMatrixView->xCoord, pMatrixView2Copy->xCoord);
		xVect_Copy<AxisMalloc, AxisRealloc>(pAxis->pMatrixView->yCoord, pMatrixView2Copy->yCoord);
	
		AxisMatrixView_CopyCMap(pAxis->pMatrixView, pMatrixView2Copy->cMap);
	}
}

void AxisSerie_Plot(AxisSerie * pSerie, bool toMetaFile = false) {
	AxisSerie_PlotLines(pSerie, toMetaFile);
	AxisSerie_PlotMarkers(pSerie);
}

void Axis_PlotSeries(Axis * pAxis, bool toMetaFile = false) {

	if (pAxis->hdc) {
		xVect_static_for(pAxis->pSeries, i) {
			AxisSerie_Plot(&pAxis->pSeries[i], toMetaFile);
		}
	}

	if (xVect_Count(pAxis->subAxis)) {
		xVect_static_for(pAxis->subAxis, i) {
			auto pSubAxis = pAxis->subAxis[i];
			pSubAxis->PlotArea = pAxis->PlotArea;
			pSubAxis->options = pAxis->options;
			Axis_Zoom(pSubAxis, pSubAxis->xMin, pSubAxis->xMax, pSubAxis->yMin, pSubAxis->yMax);
			Axis_PlotSeries(pSubAxis);
		}
	}
}

void Axis_Lock(Axis * pAxis) {
	pAxis->lockCount++;
}

void Axis_Unlock(Axis * pAxis) {
	pAxis->lockCount--;
}

void Axis_BeginDraw(Axis * pAxis) {
	EnterCriticalSection(pAxis->pCs);
}

void Axis_EndDraw(Axis * pAxis) {
	LeaveCriticalSection(pAxis->pCs);
	Axis_Redraw(pAxis);
}

void Axis_Clear(Axis * pAxis) {
	
	Critical_Section(pAxis->pCs) {
	
		if (pAxis->hdc) {

			if (!pAxis->hdcDraw && pAxis->hdc) Axis_InitBitmaps(pAxis);

			if (!pAxis->hdcDraw) return;// happens when minimized.  MY_ERROR("Axis_InitBitmaps failed"); 

			GDI_SAFE_CALL(BitBlt(pAxis->hdcDraw, 0, 0, 
								 pAxis->DrawArea_nPixels_x, 
								 pAxis->DrawArea_nPixels_y, 
								 pAxis->hdcErase, 0, 0, SRCCOPY));
		}
	}
}

void Axis_DrawBoundingRects(Axis * pAxis, bool interior = 1, bool exterior = 1) {

	HPEN oldPen;
	HBRUSH oldBrush;

	Critical_Section(pAxis->pCs) {

		if (pAxis->hdcDraw) {

			GDI_SAFE_CALL(oldPen = SelectPen(pAxis->hdcDraw, GetStockPen(BLACK_PEN)));
			oldBrush = SelectBrush(pAxis->hdcDraw, GetStockBrush(NULL_BRUSH));

			if (interior) {

				GDI_SAFE_CALL(Rectangle(pAxis->hdcDraw, 
					pAxis->PlotArea.left - pAxis->DrawArea_nPixels_dec_x,
					pAxis->PlotArea.top - pAxis->DrawArea_nPixels_dec_y,
					pAxis->PlotArea.right - pAxis->DrawArea_nPixels_dec_x + 1, 
					pAxis->PlotArea.bottom - pAxis->DrawArea_nPixels_dec_y + 1));
			}

			if (exterior) {
				GDI_SAFE_CALL(Rectangle(pAxis->hdcDraw, 0, 0,
					pAxis->DrawArea_nPixels_x, pAxis->DrawArea_nPixels_y));
			}

			SelectObject(pAxis->hdcDraw, oldBrush);
			SelectObject(pAxis->hdcDraw, oldPen);
		}
	}

}
void Axis_DrawZerosLines(Axis * pAxis) {
		
	static HPEN hPenGrid;
	
	if (!hPenGrid) hPenGrid = GetStockPen(BLACK_PEN);

	Critical_Section(pAxis->pCs) {

		HPEN oldPen ;
		GDI_SAFE_CALL(oldPen = SelectPen(pAxis->hdcDraw, hPenGrid));

		if (pAxis->xMax > 0 && pAxis->xMin < 0) { 
			Axis_MoveToEx(pAxis, 0, pAxis->yMin);
			Axis_LineTo(pAxis, 0, pAxis->yMax);
		}

		if (pAxis->yMax > 0 && pAxis->yMin < 0) { 
			Axis_MoveToEx(pAxis, pAxis->xMin, 0);
			Axis_LineTo(pAxis, pAxis->xMax, 0);
		}

		GDI_SAFE_CALL(SelectPen(pAxis->hdcDraw, oldPen));
	}

}

void AxisBorder_ComputeExtent(AxisBorder * pBorder) {
	
	int totExtent = 0;
	SIZE sz;

	if (pBorder) {
		if (pBorder->pText) {
			GDI_SAFE_CALL(SelectFont(pBorder->pAxis->hdcDraw, pBorder->font.hFont));
			GetTextExtentPoint32W(pBorder->pAxis->hdcDraw, 
				pBorder->pText, (int)xVect_Count(pBorder->pText), &sz);
			totExtent += sz.cy;
		}
		if (pBorder->pTicks && pBorder->pTicks->options & AXISTICKS_OPT_LABELS) {
			totExtent += pBorder->isVertical ?
							pBorder->pTicks->maxLabelExtent.cx :
							pBorder->pTicks->maxLabelExtent.cy;
		}

		pBorder->extent = totExtent + pBorder->marge + pBorder->marge2;
	}
}

bool Axis_IsValidPlotArea(Axis * pAxis) {
	return pAxis->PlotArea.left < pAxis->PlotArea.right 
	&&  pAxis->PlotArea.top  < pAxis->PlotArea.bottom;
}

void Axis_DestroyCallbacksData(Axis * pAxis) {

	if (pAxis->callbacksData) {
		if (pAxis->callbacksCleanUp) {
			xVect_static_for(pAxis->callbacksData, i) {
				pAxis->callbacksCleanUp(pAxis, pAxis->callbacksData[i]);
			}
		}
		xVect_Free<AxisFree>(pAxis->callbacksData);
	}
}

void * Axis_PopStartCallbacksData(Axis * pAxis) {
	
	void * retVal = NULL;

	Critical_Section(pAxis->pCs) {
	
		retVal = pAxis->callbacksData ? pAxis->callbacksData[0] : NULL;

		xVect_Remove<AxisMalloc, AxisRealloc>(pAxis->callbacksData, 0);

	}

	return retVal;
}

void * Axis_GetDrawCallbackData(Axis * pAxis) {
	
	void * retVal = NULL;

	if (pAxis->callbacksData) {
		retVal = pAxis->callbacksData[0];
	}

	return  retVal;
}

void Axis_PushCallbacksData(Axis * pAxis, void * callbacksData) {
	
	Critical_Section(pAxis->pCs) {

		if (xVect_Count(pAxis->callbacksData) > 1000) {
			DbgStr(" [0x%p] %d plotData waiting !\n", GetCurrentThreadId(), xVect_Count(pAxis->callbacksData));
		}

		xVect_Push_ByVal<AxisMalloc, AxisRealloc>(pAxis->callbacksData, callbacksData);
	}
}

void Axis_SetCallbacksData(Axis * pAxis, void * callbacksData) {
	
	Critical_Section(pAxis->pCs) {

		Axis_DestroyCallbacksData(pAxis);

		if (callbacksData) {
			Axis_PushCallbacksData(pAxis, callbacksData);
		}

	}
}

void Axis_SetTextColor(Axis * pAxis, COLORREF color) {
	SetTextColor(pAxis->hdcDraw, color);
}

void Axis_SetBkColor(Axis * pAxis, COLORREF color) {
	SetBkColor(pAxis->hdcDraw, color);
}

void Axis_SetBkMode(Axis * pAxis, int mode) {
	SAFE_CALL(SetBkMode(pAxis->hdcDraw, mode));
}

COLORREF Axis_SetPenColor(Axis * pAxis, COLORREF color) {
	Axis_SelectPen(pAxis, SAFE_CALL(GetStockPen(DC_PEN)));
	return SetDCPenColor(pAxis->hdcDraw, color);
}

COLORREF Axis_SetBrushColor(Axis * pAxis, COLORREF color) {
	Axis_SelectBrush(pAxis, SAFE_CALL(GetStockBrush(DC_BRUSH)));
	return SetDCBrushColor(pAxis->hdcDraw, color);
}

void Axis_SetStatusTextCallback(Axis * pAxis,
						    AxisStatusTextCallback statusTextCallback) {

	pAxis->statusTextCallback = statusTextCallback;
}

AxisStatusTextCallback Axis_GetStatusTextCallback(Axis * pAxis) {

	return pAxis->statusTextCallback;
}

void Axis_EnforceSquareZoom(Axis * pAxis, bool enable) {
	SET_BYTE_MASK(pAxis->options, AXIS_OPT_SQUAREZOOM, enable);
}

void Axis_SetDrawCallback(Axis * pAxis, 
					  AxisDrawCallback callback, 
					  AxisDrawCallback CleanUpCallback) {
	
	
	Axis_SetCallbacksData(pAxis, NULL);

	pAxis->drawCallback = callback;
	pAxis->callbacksCleanUp =  CleanUpCallback;
}

void Axis_SetCheckZoomCallback(Axis * pAxis, AxisCheckZoomCallback callback) {

	pAxis->checkZoomCallback = callback;
}

void Axis_SetCallbacksCleanUpCallback(Axis * pAxis, AxisDrawCallback callback) {
	pAxis->callbacksCleanUp = callback;
}

void Axis_SquareZoom(Axis * pAxis);

void Axis_SetDrawCallbackOrder(Axis * pAxis, int order) {
	pAxis->drawCallbackOrder = order;
}

int Axis_GetDrawCallbackOrder(Axis * pAxis) {
	return pAxis->drawCallbackOrder;
}

void Axis_SetTitle(Axis * pAxis, const wchar_t * title);

void Axis_DrawY_Axis(Axis * pAxis) {

	HPEN   oldPen = SelectPen(pAxis->hdcDraw, pAxis->pY_Axis->hCurrentPen ? pAxis->pY_Axis->hCurrentPen : GetStockPen(BLACK_PEN));

	LONG xMin, yMin, yMax;

	Axis_CoordTransfDoubleToInt(pAxis, &xMin, &yMin, pAxis->xMin, pAxis->yMin);
	Axis_CoordTransfDoubleToInt(pAxis, &xMin, &yMax, pAxis->xMin, pAxis->yMax);

	MoveToEx(pAxis->hdcDraw, xMin, yMax, NULL);
	LineTo(pAxis->hdcDraw, xMin, yMin);

	SelectPen(pAxis->hdcDraw, oldPen);
}

void Axis_RefreshWithSubAxis(Axis * pAxis) {

	if (xVect_Count(pAxis->pSeries)) {
		if (pAxis->pY_Axis->hCurrentPen) SAFE_CALL(DeletePen(pAxis->pY_Axis->hCurrentPen));
		pAxis->pY_Axis->hCurrentPen = SAFE_CALL(CreatePen(PS_SOLID, 1, pAxis->pSeries[0].pen.lopnColor));
	}

	int accumulatedExtent = 0;

	auto N = xVect_Count(pAxis->subAxis);

	aVectEx<double, g_AxisHeap> CoordTransf_xFact(N), CoordTransf_dx(N);
	aVectEx<LONG, g_AxisHeap> PlotAreaLeft(N);

	Axis_DrawGridAndTicks(pAxis, false, false, true);
	AxisBorder_ComputeExtent(pAxis->pTitle);
	AxisBorder_ComputeExtent(pAxis->pStatusBar);
	AxisBorder_ComputeExtent(pAxis->pX_Axis);
	AxisBorder_ComputeExtent(pAxis->pY_Axis);
	Axis_UpdateBorderPos(pAxis);
	Axis_UpdatePlotAreaRect(pAxis);

	xVect_for_inv(pAxis->subAxis, i) {

		auto pSubAxis = pAxis->subAxis[i];

		pSubAxis->hdc = pAxis->hdc;
		pSubAxis->hdcDraw = pAxis->hdcDraw;

		pSubAxis->pCs = pAxis->pCs;
		pSubAxis->PlotArea = pAxis->PlotArea;

		pSubAxis->xMin = pAxis->xMin;
		pSubAxis->xMax = pAxis->xMax;
		pSubAxis->xMin_orig = pAxis->xMin_orig;
		pSubAxis->xMax_orig = pAxis->xMax_orig;

		pSubAxis->options = pAxis->options;

		Axis_Zoom(pSubAxis, pSubAxis->xMin, pSubAxis->xMax, pSubAxis->yMin, pSubAxis->yMax);
		if (!xVect_Count(pSubAxis->pSeries)) continue;

		COLORREF YaxisColor = pSubAxis->pSeries[0].pen.lopnColor;

		if (pSubAxis->pY_Axis->hCurrentPen) SAFE_CALL(DeletePen(pSubAxis->pY_Axis->hCurrentPen));
		pSubAxis->pY_Axis->hCurrentPen = SAFE_CALL(CreatePen(PS_SOLID, 1, YaxisColor));

		pSubAxis->tickLabelFont = pAxis->tickLabelFont;

		pSubAxis->pY_Axis->marge = 10;
		pSubAxis->pY_Axis->marge2 = accumulatedExtent + (accumulatedExtent ? 10 : 3);

		Axis_DrawGridAndTicks(pSubAxis, false, false, true);
		AxisBorder_ComputeExtent(pSubAxis->pY_Axis);

		pSubAxis->pX_Axis = pAxis->pX_Axis;
		pSubAxis->pY_Axis2 = pAxis->pY_Axis2;
		pSubAxis->pTitle = pAxis->pTitle;
		pSubAxis->pStatusBar = pAxis->pStatusBar;

		pSubAxis->DrawArea_nPixels_x = pAxis->DrawArea_nPixels_x;
		pSubAxis->DrawArea_nPixels_y = pAxis->DrawArea_nPixels_y;
		pSubAxis->DrawArea_nPixels_dec_x = pAxis->DrawArea_nPixels_dec_x;
		pSubAxis->DrawArea_nPixels_dec_y = pAxis->DrawArea_nPixels_dec_y;

		Axis_UpdateBorderPos(pSubAxis);
		Axis_UpdatePlotAreaRect(pSubAxis);
		Axis_Zoom(pSubAxis, pSubAxis->xMin, pSubAxis->xMax, pSubAxis->yMin, pSubAxis->yMax);

		CoordTransf_xFact[i] = pSubAxis->CoordTransf_xFact;
		CoordTransf_dx[i] = pSubAxis->CoordTransf_dx;
		PlotAreaLeft[i] = pSubAxis->PlotArea.left;

		pSubAxis->pX_Axis = nullptr;
		pSubAxis->pY_Axis2 = nullptr;
		pSubAxis->pTitle = nullptr;
		pSubAxis->pStatusBar = nullptr;

		accumulatedExtent = pSubAxis->pY_Axis->extent;
	}

	pAxis->pY_Axis->marge = 10;
	pAxis->pY_Axis->marge2 = accumulatedExtent + 10;

	auto legend = pAxis->options & AXIS_OPT_LEGEND;
	if (legend) pAxis->options &= ~AXIS_OPT_LEGEND;

	Axis_Refresh(pAxis, false, false, false, true);

	Axis_DrawY_Axis(pAxis);

	xVect_for_inv(pAxis->subAxis, i) {

		auto pSubAxis = pAxis->subAxis[i];

		if (!xVect_Count(pSubAxis->pSeries)) continue;

		Swap(pSubAxis->CoordTransf_xFact, CoordTransf_xFact[i]);
		Swap(pSubAxis->CoordTransf_dx, CoordTransf_dx[i]);
		Swap(pSubAxis->PlotArea.left, PlotAreaLeft[i]);

		AxisBorder_Draw(pSubAxis->pY_Axis, true, false);
		Axis_DrawGridAndTicks(pSubAxis, false, true, false);
		Axis_DrawY_Axis(pSubAxis);
		
		Swap(pSubAxis->CoordTransf_xFact, CoordTransf_xFact[i]);
		Swap(pSubAxis->CoordTransf_dx, CoordTransf_dx[i]);
		Swap(pSubAxis->PlotArea.left, PlotAreaLeft[i]);
	}

	if (legend) {
		pAxis->options |= AXIS_OPT_LEGEND;
		Axis_DrawLegend(pAxis);
		Axis_DrawBoundingRects(pAxis, false, true);
	}

	Axis_Redraw(pAxis);
}

void Axis_Refresh(Axis * pAxis, bool autoFit, bool toMetaFile, bool redraw, bool subAxisRecursion) {

	if (pAxis->options & AXIS_OPT_ISORPHAN) {
		DbgStr(" [x0%p] Axis_Refresh, axis is AXIS_OPT_ISORPHAN (id = %d)! Exiting function...", GetCurrentThreadId(), pAxis->id);
		return;
	}

	Critical_Section(pAxis->pCs) {

		if (xVect_Count(pAxis->subAxis) && !subAxisRecursion) {
			Axis_RefreshWithSubAxis(pAxis);
			return;
		}

		if (pAxis->hdc) {

			if (!Axis_IsValidPlotArea(pAxis)) {
				Axis_Clear(pAxis);
				Axis_DrawLegend(pAxis);
			}
			else {
				if ((pAxis->drawCallback || pAxis->pSeries || pAxis->pMatrixView) && !toMetaFile) Axis_Clear(pAxis);
			
				if (autoFit) Axis_AutoFit(pAxis, true);

				if (((pAxis->options & AXIS_OPT_XLOG) && (pAxis->xMin <= 0 || pAxis->xMax <= 0))
					|| ((pAxis->options & AXIS_OPT_YLOG) && (pAxis->yMin <= 0 || pAxis->yMax <= 0)))
				{
					Axis_AutoFit(pAxis, false);
					if (pAxis->xMin <= 0) pAxis->xMin = 1;
					if (pAxis->xMax <= 0) pAxis->xMax = 1;
					if (pAxis->yMin <= 0) pAxis->yMin = 1;
					if (pAxis->yMax <= 0) pAxis->yMax = 1;
				}

				int oldXaxisExtent, oldYaxisExtent;
				int count = 0;

				bool bitmapSizeChanged = false;

				for(;;) {

					if (pAxis->options & AXIS_OPT_SQUAREZOOM) Axis_SquareZoom(pAxis);

					oldXaxisExtent = pAxis->pX_Axis ? pAxis->pX_Axis->extent : 0;
					oldYaxisExtent = pAxis->pY_Axis ? pAxis->pY_Axis->extent : 0;

					Axis_DrawGridAndTicks(pAxis, false, false, true);
					AxisBorder_ComputeExtent(pAxis->pTitle);
					AxisBorder_ComputeExtent(pAxis->pStatusBar);
					AxisBorder_ComputeExtent(pAxis->pX_Axis);
					AxisBorder_ComputeExtent(pAxis->pY_Axis);

					if (pAxis->pY_Axis && pAxis->pY_Axis2 && pAxis->pY_Axis->pTicks 
					&& !pAxis->pY_Axis2->pTicks && !pAxis->pY_Axis2->pText
					&& !(pAxis->options & AXIS_OPT_ISMATVIEWLEGEND)
					&& !xVect_Count(pAxis->subAxis))
					{
						pAxis->pY_Axis2->extent = (int)(0.7 * pAxis->pY_Axis->extent);
					}
					else {
						AxisBorder_ComputeExtent(pAxis->pY_Axis2);
					}

					Axis_UpdateBorderPos(pAxis);
					Axis_UpdatePlotAreaRect(pAxis);

					if (pAxis->options & AXIS_OPT_ISMATVIEWLEGEND) {
						auto adjustX = 10 - (pAxis->PlotArea.right - pAxis->PlotArea.left);

						if (adjustX) {
							pAxis->DrawArea_nPixels_x += adjustX;

							Axis_UpdateBorderPos(pAxis);
							Axis_UpdatePlotAreaRect(pAxis);

							bitmapSizeChanged = true;
						}
					}

					Axis_Zoom(pAxis, pAxis->xMin, pAxis->xMax, pAxis->yMin, pAxis->yMax);

					if (count++ > 20) break;
					if (oldXaxisExtent == (pAxis->pX_Axis ? pAxis->pX_Axis->extent : 0)
					&&  oldYaxisExtent == (pAxis->pY_Axis ? pAxis->pY_Axis->extent : 0)) break;
				}

				if (bitmapSizeChanged) Axis_InitBitmaps(pAxis);

				if (pAxis->drawCallback && pAxis->drawCallbackOrder <= -3) Axis_CallPlotFunc(pAxis);

				Axis_DrawGridAndTicks(pAxis, 1, 0, 0);
				
				if (pAxis->drawCallback && pAxis->drawCallbackOrder == -2) Axis_CallPlotFunc(pAxis);

				if (pAxis->options & AXIS_OPT_ZEROLINES) Axis_DrawZerosLines(pAxis);

				if (pAxis->drawCallback && pAxis->drawCallbackOrder == -1) Axis_CallPlotFunc(pAxis);

				if (pAxis->pMatrixView && !toMetaFile) Axis_DrawMatrix3(pAxis);

				if (pAxis->drawCallback && pAxis->drawCallbackOrder == 0) Axis_CallPlotFunc(pAxis);

				Axis_PlotSeries(pAxis, toMetaFile);

				if (pAxis->drawCallback && pAxis->drawCallbackOrder == 1) Axis_CallPlotFunc(pAxis);

				Axis_DrawBorders(pAxis);

				if (pAxis->drawCallback && pAxis->drawCallbackOrder == 2) Axis_CallPlotFunc(pAxis);

				Axis_DrawGridAndTicks(pAxis, 0, 1, 0);

				if (pAxis->drawCallback && pAxis->drawCallbackOrder == 3) Axis_CallPlotFunc(pAxis);

				Axis_DrawBoundingRects(pAxis, 1, 0);

				if (pAxis->drawCallback && pAxis->drawCallbackOrder == 4) Axis_CallPlotFunc(pAxis);

				Axis_DrawLegend(pAxis);

				if (pAxis->drawCallback && pAxis->drawCallbackOrder == 5) Axis_CallPlotFunc(pAxis);

				Axis_DrawBoundingRects(pAxis, 0, 1);

				if (pAxis->drawCallback && pAxis->drawCallbackOrder >= 6) {
					Axis_CallPlotFunc(pAxis);
				}

				if ((pAxis->options & AXIS_OPT_LEGEND) &&
					pAxis->pMatrixView && 
					!(pAxis->pMatrixView->options & AXIS_CMAP_COLORREF)) 
				{
					if (!pAxis->pMatrixView->cMap) 
						AxisMatrixView_CreateColorMap(pAxis->pMatrixView, pAxis->pMatrixView->options);
					if (!pAxis->pMatrixView->cMap->legendCmap)
						AxisMatrixView_CreateLegendCMap(pAxis->pMatrixView->cMap);

					Axis_Refresh(pAxis->pMatrixView->cMap->legendCmap, false, toMetaFile, false);
					if (!toMetaFile && redraw) Axis_Redraw(pAxis->pMatrixView->cMap->legendCmap, pAxis->hdcDraw);
				}
				
				if (!toMetaFile && redraw) {
					Axis_Redraw(pAxis);
				}
			}
		}
		else if (pAxis->callbacksCleanUp) {
			Axis_CallPlotFunc(pAxis);
		}
	}
}

unsigned long long g_AxisRefreshThreadHomeWork_id;

struct AxisRefreshThreadHomeWork {
	CRITICAL_SECTION cs;
	Axis ** pAxis;
	bool * autoFit;
	bool * redraw;
	bool * destroy;
	bool * toMetaFile;

	unsigned long long * id;
};

struct AxisRefreshThreadParam {
	HANDLE hEventThreadCreated;
	HANDLE hEventRefreshThreadReady;
	HANDLE hEventGo;
	AxisRefreshThreadHomeWork homeWork;
	unsigned int ind;
};

void Axis_SetRefreshThreadNumber(int nThreads) {
	g_nThreads = nThreads;
}

AxisRefreshThreadParam AxisRefreshThreadParam_ZeroInitialiszer;

void Axis_RefreshAsyncCore(
	Axis * pAxis, bool autoFit, 
	bool toMetaFile, bool redraw, 
	bool destroy, bool cancelPreviousJobs)
{

	size_t iThread2Use;

	static aVect<AxisRefreshThreadParam> params;
	static aVect<HANDLE> hEventThreadCreated;
	static aVect<HANDLE> hEventRefreshThreadReady;
	static aVect<HANDLE> hEventGo;

	if (g_refreshThreadCount == 0 && !pAxis) { //Reset signal
		
		aVect_static_for(hEventThreadCreated, i) {
			CloseHandle(hEventThreadCreated[i]);
			CloseHandle(hEventRefreshThreadReady[i]);
			CloseHandle(hEventGo[i]);
			DeleteCriticalSection(&params[i].homeWork.cs);
		}

		params.Free();
		hEventThreadCreated.Free();
		hEventRefreshThreadReady.Free();
		hEventGo.Free();

		return;
	}

	if (g_EndOfTimes) return;

	if (pAxis->options & AXIS_OPT_ISORPHAN) {
		DbgStr(" [x0%p] Axis_RefreshAsyncCore, axis is AXIS_OPT_ISORPHAN (id = %d)! Exiting function...", GetCurrentThreadId(), pAxis->id);
		return;
	}

	if (params.Count() == 0) {
		params.Redim(g_nThreads);
		params.Set(AxisRefreshThreadParam_ZeroInitialiszer);
	
		for (unsigned int i=0 ; i<g_nThreads ; i++) {
			hEventThreadCreated.Push(CreateEvent(NULL, true, false, NULL));
			hEventRefreshThreadReady.Push(CreateEvent(NULL, true, false, NULL));
			hEventGo.Push(CreateEvent(NULL, false, false, NULL));
		}
	}

	if (params.Count() > g_nThreads) {
		for (size_t i=params.Count()-1 ; i >= g_nThreads ; i--) {
			Axis_CloseRefreshThread(i);
			params.Remove(i);
		}
	}
	
	if (cancelPreviousJobs) {
		aVect_static_for(g_hRefreshThread, i) {
			auto&& param_i = params[i];
			Critical_Section(&param_i.homeWork.cs) {
				xVect_for_inv(param_i.homeWork.pAxis, j) {
					if (param_i.homeWork.pAxis[j] == pAxis &&
						param_i.homeWork.autoFit[j] == autoFit &&
						param_i.homeWork.destroy[j] == destroy &&
						param_i.homeWork.redraw[j] == redraw &&
						param_i.homeWork.toMetaFile[j] == toMetaFile)
					{
						xVect_Remove(param_i.homeWork.pAxis, j);
						xVect_Remove(param_i.homeWork.autoFit, j);
						xVect_Remove(param_i.homeWork.destroy, j);
						xVect_Remove(param_i.homeWork.redraw, j);
						xVect_Remove(param_i.homeWork.toMetaFile, j);
						xVect_Remove(param_i.homeWork.id, j);
					}
				}
			}
		}
	}

	iThread2Use = (size_t)-1;
	size_t minWork = (size_t)-1;

	aVect_static_for(g_hRefreshThread, i) {	
		
		Critical_Section(&params[i].homeWork.cs) {
		
			size_t nWork = xVect_Count(params[i].homeWork.pAxis);
			if (nWork >= AXIS_MAX_WORK) ResetEvent(params[i].hEventRefreshThreadReady);
			else if (nWork < minWork) iThread2Use = i, minWork = nWork;
		}
	}

	if ((iThread2Use == -1 || minWork > 0) && g_hRefreshThread.Count() < g_nThreads) {

		static CRITICAL_SECTION localCs;
		static bool localCsInitialized = 0;

		if (!localCsInitialized) {
			InitializeCriticalSection(&localCs);
			localCsInitialized = true;
		}

		Critical_Section(&localCs) {

			params[g_refreshThreadCount].hEventThreadCreated = hEventThreadCreated[g_refreshThreadCount];
			params[g_refreshThreadCount].hEventRefreshThreadReady = hEventRefreshThreadReady[g_refreshThreadCount];
			params[g_refreshThreadCount].hEventGo = hEventGo[g_refreshThreadCount];
			params[g_refreshThreadCount].ind = g_refreshThreadCount;

			InitializeCriticalSectionAndSpinCount(&params[g_refreshThreadCount].homeWork.cs, AXIS_CS_SPINCOUNT);

			HANDLE hNewTread;

			SAFE_CALL(hNewTread =
					(HANDLE)CreateThread(NULL, 0, RefreshWorkerThread, (void*)&params[g_refreshThreadCount], 0, NULL)); 

			if (WaitForSingleObject(hEventThreadCreated[g_refreshThreadCount], INFINITE) != WAIT_OBJECT_0) {
				MY_ERROR("ici");
			}

			g_hRefreshThread.Redim(g_refreshThreadCount + 1);

			g_hRefreshThread[g_refreshThreadCount] = hNewTread;

			EVENT_SAFE_CALL(SetEvent(hEventRefreshThreadReady[g_refreshThreadCount]));

			iThread2Use = g_refreshThreadCount;

			g_refreshThreadCount++;
		};
	}

	if (iThread2Use == -1) {
		auto waitResult = WaitForMultipleObjects(
			(DWORD)g_hRefreshThread.Count(),
			&hEventRefreshThreadReady[0], false, 500);

		if (waitResult == WAIT_TIMEOUT) {
			iThread2Use = 0;
			DbgStr("No ready thread after 500 ms wait !!\n");
		} else {
			iThread2Use = waitResult - WAIT_OBJECT_0;
		}
	}

	Critical_Section(&params[iThread2Use].homeWork.cs) {

		xVect_Push_ByVal<AxisMalloc, AxisRealloc>(params[iThread2Use].homeWork.pAxis, pAxis);
		xVect_Push_ByVal<AxisMalloc, AxisRealloc>(params[iThread2Use].homeWork.autoFit, autoFit);
		xVect_Push_ByVal<AxisMalloc, AxisRealloc>(params[iThread2Use].homeWork.redraw, redraw);
		xVect_Push_ByVal<AxisMalloc, AxisRealloc>(params[iThread2Use].homeWork.destroy, destroy);
		xVect_Push_ByVal<AxisMalloc, AxisRealloc>(params[iThread2Use].homeWork.toMetaFile, toMetaFile);
		xVect_Push_ByVal<AxisMalloc, AxisRealloc>(params[iThread2Use].homeWork.id, g_AxisRefreshThreadHomeWork_id++);
	}

	EVENT_SAFE_CALL(SetEvent(hEventGo[iThread2Use]));
}

void Axis_RefreshAsync(Axis * pAxis, bool autoFit, bool toMetaFile, bool redraw, bool cancelPreviousJobs) {
	Axis_RefreshAsyncCore(pAxis, autoFit, toMetaFile, redraw, false, cancelPreviousJobs);
}

void Axis_DestroyAsync(Axis * pAxis) {
	Axis_RefreshAsyncCore(pAxis, false, false, false, true, false);
}

void AxisBorder_SetText(AxisBorder * pAxisBorder, const wchar_t * str) {
	
	size_t count;
	
	if (!pAxisBorder) return;

	if (!str) {
		if (pAxisBorder->pText) pAxisBorder->pText[0] = 0;
		return;
	}

	count = wcslen(str) + 1;
	
	xVect_Redim<false, false, AxisMalloc, AxisRealloc>(pAxisBorder->pText, count);

	if (pAxisBorder->pText) memcpy(pAxisBorder->pText, str, count*sizeof(wchar_t));
}

void Axis_SetStatusBarText(Axis * pAxis, wchar_t * str) {

	if (pAxis->pStatusBar) {
		if (!str) xVect_Free<AxisFree>(pAxis->pStatusBar->pText);
		else AxisBorder_SetText(pAxis->pStatusBar, str);
	}
}

//returns 0 if serie is created in the axis, i+1 for subAxis[i]...
template <class T>
int Axis_AddSubAxisSerieCore(
	Axis * pAxis, const wchar_t * pSubAxisName, const wchar_t * pSerieName, const T * x, const T * y,
	size_t n, COLORREF color, DWORD penStyle, unsigned short lineWidth,
	DWORD markerType, COLORREF markerColor, unsigned short marker_nPixel,
	unsigned short markerLineWidth) 
{

	Axis * foundAxis = nullptr;
	bool createFakeLegend = false;
	int retVal = 0;

	if (xVect_Count(pAxis->pSeries) == 0) {
		foundAxis = pAxis;
		AxisBorder_SetText(pAxis->pY_Axis, pSubAxisName);
	}
	
	if (pAxis->pY_Axis && pAxis->pY_Axis->pText) {
		if (strcmp_caseInsensitive(pAxis->pY_Axis->pText, pSubAxisName) == 0) {
			foundAxis = pAxis;
		}
	}

	if (!foundAxis) {
		xVect_static_for(pAxis->subAxis, i) {
			auto pSubAxis = pAxis->subAxis[i];
			if (pSubAxis->pY_Axis && pSubAxis->pY_Axis->pText) {
				if (strcmp_caseInsensitive(pSubAxis->pY_Axis->pText, pSubAxisName) == 0) {
					foundAxis = pSubAxis;
					retVal = (int)i + 1;
					createFakeLegend = true;
				}
			}
		}
	}

	if (!foundAxis) {
		if (xVect_Count(pAxis->pSeries)) {
			foundAxis = Axis_CreateSubAxis(pAxis, pSubAxisName);
			retVal = (int)xVect_Count(pAxis->subAxis);//i+1
			createFakeLegend = true;
		}
		else {
			foundAxis = pAxis;
			AxisBorder_SetText(pAxis->pY_Axis, pSubAxisName);
		}
	}

	Axis_AddSerie_Raw(foundAxis, pSerieName, x, y, n, color, penStyle,
		lineWidth, markerType, markerColor, marker_nPixel, markerLineWidth);

	auto id = xVect_Last(foundAxis->pSeries).id;

	//when subAxis exist, id is the same for the real and the fake serie (fake serie = empty serie only for legend)
	if (createFakeLegend) {
		Axis_AddSerie_Raw(pAxis, pSerieName, (double*)nullptr, (double*)nullptr, 0, color, penStyle,
			lineWidth, markerType, markerColor, marker_nPixel, markerLineWidth);

		xVect_Last(pAxis->pSeries).id = id;
	}

	return retVal;
}

int Axis_AddSubAxisSerie_Raw(
	Axis * pAxis, const wchar_t * pSubAxisName, const wchar_t * pSerieName, const double * x, const double * y,
	size_t n, COLORREF color, DWORD penStyle, unsigned short lineWidth,
	DWORD markerType, COLORREF markerColor, unsigned short marker_nPixel,
	unsigned short markerLineWidth)
{
	return Axis_AddSubAxisSerieCore(pAxis, pSubAxisName, pSerieName, x, y, n, color, 
		penStyle, lineWidth, markerType, markerColor, marker_nPixel, markerLineWidth);
}

int Axis_AddSubAxisSerie_Raw(
	Axis * pAxis, const wchar_t * pSubAxisName, const wchar_t * pSerieName, const float * x, const float * y,
	size_t n, COLORREF color, DWORD penStyle, unsigned short lineWidth,
	DWORD markerType, COLORREF markerColor, unsigned short marker_nPixel,
	unsigned short markerLineWidth)
{
	return Axis_AddSubAxisSerieCore(pAxis, pSubAxisName, pSerieName, x, y, n, color,
		penStyle, lineWidth, markerType, markerColor, marker_nPixel, markerLineWidth);
}

int Axis_AddSubAxisSerie(
	Axis * pAxis, const wchar_t * pSubAxisName, const wchar_t * pSerieName,
	const aVect<double> & x, const aVect<double> & y,
	COLORREF color, DWORD penStyle, unsigned short lineWidth,
	DWORD markerType, COLORREF markerColor, unsigned short marker_nPixel,
	unsigned short markerLineWidth)
{
	return Axis_AddSubAxisSerieCore(pAxis, pSubAxisName, pSerieName, x.GetDataPtr(), y.GetDataPtr(), Min(x.Count(), y.Count()), color,
		penStyle, lineWidth, markerType, markerColor, marker_nPixel, markerLineWidth);
}

int Axis_AddSubAxisSerie(
	Axis * pAxis, const wchar_t * pSubAxisName, const wchar_t * pSerieName,
	const aVect<float> & x, const aVect<float> & y,
	COLORREF color, DWORD penStyle, unsigned short lineWidth,
	DWORD markerType, COLORREF markerColor, unsigned short marker_nPixel,
	unsigned short markerLineWidth)
{
	return Axis_AddSubAxisSerieCore(pAxis, pSubAxisName, pSerieName, x.GetDataPtr(), y.GetDataPtr(), Min(x.Count(), y.Count()), color,
		penStyle, lineWidth, markerType, markerColor, marker_nPixel, markerLineWidth);
}

void Axis_SubAxis_yLim(Axis * pAxis, int i, double yMin, double yMax, bool forceAdjustZoom = false) {

	if (i == 0) {
		Axis_yLim(pAxis, yMin, yMax, forceAdjustZoom);
	} else {
		auto ind = (size_t)(i - 1);
		if (ind < xVect_Count(pAxis->subAxis)) {
			Axis_yLim(pAxis->subAxis[ind], yMin, yMax, forceAdjustZoom);
		}
		else {
			DbgStr("Axis_SubAxis_yLim: wrong i passed (%d), xVect_Count(pAxis->subAxis) = %d\n", i, xVect_Count(pAxis->subAxis));
		}
	}
}

double Axis_SubAxis_yMin(Axis * pAxis, int i, bool orig) {

	if (i == 0) {
		return Axis_yMin(pAxis, orig);
	}
	else {
		auto ind = (size_t)(i - 1);
		if (ind < xVect_Count(pAxis->subAxis)) {
			return Axis_yMin(pAxis->subAxis[ind], orig);
		}
		else {
			DbgStr("Axis_SubAxis_yLim: wrong i passed (%d), xVect_Count(pAxis->subAxis) = %d\n", i, xVect_Count(pAxis->subAxis));
		}
	}
	return std::numeric_limits<double>::quiet_NaN();
}

double Axis_SubAxis_yMax(Axis * pAxis, int i, bool orig) {

	if (i == 0) {
		return Axis_yMax(pAxis, orig);
	}
	else {
		auto ind = (size_t)(i - 1);
		if (ind < xVect_Count(pAxis->subAxis)) {
			return Axis_yMax(pAxis->subAxis[ind], orig);
		}
		else {
			DbgStr("Axis_SubAxis_yLim: wrong i passed (%d), xVect_Count(pAxis->subAxis) = %d\n", i, xVect_Count(pAxis->subAxis));
		}
	}
	return std::numeric_limits<double>::quiet_NaN();
}

void Axis_SubAxis_yMin(Axis * pAxis, int i, double yMin, bool forceAdjustZoom) {

	if (i == 0) {
		Axis_yMin(pAxis, yMin, forceAdjustZoom);
	}
	else {
		auto ind = (size_t)(i - 1);
		if (ind < xVect_Count(pAxis->subAxis)) {
			Axis_yMin(pAxis->subAxis[ind], yMin, forceAdjustZoom);
		}
		else {
			DbgStr("Axis_SubAxis_yLim: wrong i passed (%d), xVect_Count(pAxis->subAxis) = %d\n", i, xVect_Count(pAxis->subAxis));
		}
	}
}

void Axis_SubAxis_yMax(Axis * pAxis, int i, double yMax, bool forceAdjustZoom) {

	if (i == 0) {
		Axis_yMax(pAxis, yMax, forceAdjustZoom);
	}
	else {
		auto ind = (size_t)(i - 1);
		if (ind < xVect_Count(pAxis->subAxis)) {
			Axis_yMax(pAxis->subAxis[ind], yMax, forceAdjustZoom);
		}
		else {
			DbgStr("Axis_SubAxis_yLim: wrong i passed (%d), xVect_Count(pAxis->subAxis) = %d\n", i, xVect_Count(pAxis->subAxis));
		}
	}
}

template <class T> 
AxisSerie Axis_AddSerieCore(
	Axis * pAxis, const wchar_t * pName, const T * x, const T * y,
	size_t n, COLORREF color, DWORD penStyle, unsigned short lineWidth,
	DWORD markerType, COLORREF markerColor, unsigned short marker_nPixel,
	unsigned short markerLineWidth) {

	//TODO: properly use RAII; (this implies significat rewrite of the whole Axis lib)
	//for now, we only check & destroy for the copy of user vector, 
	//as it is the main risk of memory exhaustion.

	AxisSerie serie = AxisSerie_Zero_Initializer;

	static unsigned __int64 id;
	AutoCriticalSection acs;

	if (pAxis) acs.Enter(pAxis->pCs);

	size_t size = n * sizeof(T);

	if (std::is_same<T, double>::value) {
		if (!(serie.xDouble = (double*)AxisMalloc(size))) AxisSeries_Destroy(&serie), MY_ERROR("malloc = 0");
		if (!(serie.yDouble = (double*)AxisMalloc(size))) AxisSeries_Destroy(&serie), MY_ERROR("malloc = 0");
		serie.doublePrec = true;
	}
	else {
		if (!(serie.xSingle = (float*)AxisMalloc(size))) AxisSeries_Destroy(&serie), MY_ERROR("malloc = 0");
		if (!(serie.ySingle = (float*)AxisMalloc(size))) AxisSeries_Destroy(&serie), MY_ERROR("malloc = 0");
		serie.doublePrec = false;
	}

#ifdef DEBUG_AXIS
	char tmp[500];
	sprintf_s(tmp, sizeof(tmp), "Axis : malloc serie.x : 0x%p (size = %d)\n", serie.x, size);
	OutputDebugString(tmp);
	sprintf_s(tmp, sizeof(tmp), "Axis : malloc serie.y : 0x%p (size = %d)\n", serie.y, size);
	OutputDebugString(tmp);
#endif

	size_t nSeries = pAxis ? xVect_Count(pAxis->pSeries) : 0;

	memcpy(serie.xDouble, x, size);
	memcpy(serie.yDouble, y, size);
	serie.pen.lopnColor = color;
	serie.pen.lopnStyle = penStyle;
	serie.pen.lopnWidth.x = lineWidth;
	
	serie.drawLines = (lineWidth != 0);

	serie.n = n;
	serie.iSerie = nSeries;
	serie.pAxis = pAxis;
	serie.id = id++;

	if (pName && pName[0] != 0) serie.pName = xVect_Asprintf<AxisMalloc, AxisRealloc>(L"%s", pName);
	else serie.pName = NULL;

	AxisSerie * pSerie;

	if (pAxis) {
		xVect_Push_ByRef<AxisMalloc, AxisRealloc>(pAxis->pSeries, serie);

		pSerie = &pAxis->pSeries[nSeries];
	}
	else {
		pSerie = &serie;
	}

	if (markerType != AXIS_MARKER_NONE) {

		pSerie->pMarker =
			AxisSerieMarker_Create(pAxis, nSeries, markerType, markerColor, marker_nPixel, markerLineWidth);
	}
	else pSerie->pMarker = NULL;

	return serie;
}

AxisSerie Axis_AddSerie_(Axis * pAxis, const wchar_t * pName, const double * x, const double * y,
	size_t n, COLORREF color, DWORD penStyle, unsigned short lineWidth,
	DWORD markerType, COLORREF markerColor, unsigned short marker_nPixel,
	unsigned short markerLineWidth) {

	return Axis_AddSerieCore<double>(
		pAxis, pName, x, y,
		n, color, penStyle, lineWidth,
		markerType, markerColor, marker_nPixel,
		markerLineWidth);
}

void Axis_AddSerie(Axis * pAxis, const wchar_t * pName, const aVect<double> & x,
	const aVect<double> & y, COLORREF color, DWORD penStyle, unsigned short lineWidth,
	DWORD markerType, COLORREF markerColor, unsigned short marker_nPixel,
	unsigned short markerLineWidth) 
{
	Axis_AddSerie_(
		pAxis, pName, x, y, Min(x.Count(), y.Count()),
		color, penStyle, lineWidth,
		markerType, markerColor, marker_nPixel,
		markerLineWidth);
}

void Axis_AddSerie_Raw(Axis * pAxis, const wchar_t * pName, const double * x, const double * y,
	size_t n, COLORREF color, DWORD penStyle, unsigned short lineWidth,
	DWORD markerType, COLORREF markerColor, unsigned short marker_nPixel,
	unsigned short markerLineWidth) {

	Axis_AddSerie_(
		pAxis, pName, x, y,
		n, color, penStyle, lineWidth,
		markerType, markerColor, marker_nPixel,
		markerLineWidth);
}

AxisSerie Axis_AddSerie_(Axis * pAxis, const wchar_t * pName, const float * x, const float * y,
	size_t n, COLORREF color, DWORD penStyle, unsigned short lineWidth,
	DWORD markerType, COLORREF markerColor, unsigned short marker_nPixel,
	unsigned short markerLineWidth) {

	return Axis_AddSerieCore<float>(
		pAxis, pName, x, y,
		n, color, penStyle, lineWidth,
		markerType, markerColor, marker_nPixel,
		markerLineWidth);
}

void Axis_AddSerie(Axis * pAxis, const wchar_t * pName, const aVect<float> & x,
	const aVect<float> & y, COLORREF color, DWORD penStyle, unsigned short lineWidth,
	DWORD markerType, COLORREF markerColor, unsigned short marker_nPixel,
	unsigned short markerLineWidth)
{
	Axis_AddSerie_(
		pAxis, pName, x, y, Min(x.Count(), y.Count()),
		color, penStyle, lineWidth,
		markerType, markerColor, marker_nPixel,
		markerLineWidth);
}

void Axis_AddSerie_Raw(Axis * pAxis, const wchar_t * pName, const float * x, const float * y,
	size_t n, COLORREF color, DWORD penStyle, unsigned short lineWidth,
	DWORD markerType, COLORREF markerColor, unsigned short marker_nPixel,
	unsigned short markerLineWidth) {

	Axis_AddSerie_(
		pAxis, pName, x, y,
		n, color, penStyle, lineWidth,
		markerType, markerColor, marker_nPixel,
		markerLineWidth);
}

HDC Axis_GetHdc(Axis * pAxis) {
	return pAxis->hdcDraw;
}

//Don't call when axis critical section is held !
void Axis_SetTitle(Axis * pAxis, const wchar_t * title) {
	
	Critical_Section(pAxis->pCs) {

		if (pAxis->pTitle) {
			if (!title) xVect_Free<AxisFree>(pAxis->pTitle->pText);
			else AxisBorder_SetText(pAxis->pTitle, title);
		}
	}

	if (pAxis->options & AXIS_OPT_WINDOWED) {
		SetWindowTextW(pAxis->hWnd, title);
		//SendMessageW(pAxis->hWnd, WM_SETTEXT, NULL, (LPARAM)title);
	}
}

void Axis_xLabel(Axis * pAxis, const wchar_t * label) {
	
	Critical_Section(pAxis->pCs) {

		if (pAxis->pX_Axis) {
			if (!label) xVect_Free<AxisFree>(pAxis->pX_Axis->pText);
			else AxisBorder_SetText(pAxis->pX_Axis, label);
		}
	}
}

void Axis_yLabel(Axis * pAxis, const wchar_t * label) {
	
	Critical_Section(pAxis->pCs) {

		if (pAxis->pY_Axis) {
			if (!label) xVect_Free<AxisFree>(pAxis->pY_Axis->pText);
			else AxisBorder_SetText(pAxis->pY_Axis, label);
		}
	}
}

void Axis_xLim(Axis * pAxis, double xMin, double xMax, bool forceAdjustZoom) {
	
	Critical_Section(pAxis->pCs) {

		Axis_SetLim(pAxis, xMin, xMax, pAxis->yMin_orig, pAxis->yMax_orig, forceAdjustZoom);
	}
}

void Axis_yLim(Axis * pAxis, double yMin, double yMax, bool forceAdjustZoom) {
	
	Critical_Section(pAxis->pCs) {

		Axis_SetLim(pAxis, pAxis->xMin_orig, pAxis->xMax_orig, yMin, yMax, forceAdjustZoom);
	}
}

void Axis_yMin(Axis * pAxis, double yMin, bool forceAdjustZoom) {

	Critical_Section(pAxis->pCs) {

		Axis_SetLim(pAxis, pAxis->xMin_orig, pAxis->xMax_orig, yMin, pAxis->yMax_orig, forceAdjustZoom);
	}
}

void Axis_yMax(Axis * pAxis, double yMax, bool forceAdjustZoom) {

	Critical_Section(pAxis->pCs) {

		Axis_SetLim(pAxis, pAxis->xMin_orig, pAxis->xMax_orig, pAxis->yMin_orig, yMax, forceAdjustZoom);
	}
}

void Axis_xMin(Axis * pAxis, double xMin, bool forceAdjustZoom) {

	Critical_Section(pAxis->pCs) {

		Axis_SetLim(pAxis, xMin, pAxis->xMax_orig, pAxis->yMin_orig, pAxis->yMax_orig, forceAdjustZoom);
	}
}

void Axis_xMax(Axis * pAxis, double xMax, bool forceAdjustZoom) {

	Critical_Section(pAxis->pCs) {

		Axis_SetLim(pAxis, pAxis->xMin_orig, xMax, pAxis->yMin_orig, pAxis->yMax_orig, forceAdjustZoom);
	}
}

void AxisSeries_Destroy(AxisSerie * pSerie) {

#ifdef DEBUG_AXIS
	char tmp[500];
	sprintf_s(tmp, sizeof(tmp), "Axis : free serie.x : 0x%p\n", pAxis->pSeries[i].x);
	OutputDebugString(tmp);
	sprintf_s(tmp, sizeof(tmp), "Axis : free serie.y : 0x%p\n", pAxis->pSeries[i].y);
	OutputDebugString(tmp);
#endif
	if (pSerie->xDouble)  AxisFree(pSerie->xDouble);
	if (pSerie->yDouble)  AxisFree(pSerie->yDouble);
	if (pSerie->pMarker)  AxisSerieMarker_Destroy(pSerie->pMarker);
	if (pSerie->pName)    xVect_Free<AxisFree>(pSerie->pName);
}

void Axis_RemoveSerie(Axis * pAxis, AxisSerie * pSerie) {

	Critical_Section(pAxis->pCs) {

		//when subAxis exist, id is the same for the real and the fake serie (fake serie = empty serie only for legend)
		xVect_static_for(pAxis->pSeries, i) {
			if (pAxis->pSeries[i].id == pSerie->id) {
				AxisSeries_Destroy(&pAxis->pSeries[i]);
				xVect_Remove<AxisMalloc, AxisRealloc>(pAxis->pSeries, i);
				break;
			}
		}

		xVect_static_for(pAxis->subAxis, k) {
			auto pSubAxis = pAxis->subAxis[k];
			xVect_static_for(pSubAxis->pSeries, i) {
				if (pSubAxis->pSeries[i].id == pSerie->id) {
					AxisSeries_Destroy(&pSubAxis->pSeries[i]);
					xVect_Remove<AxisMalloc, AxisRealloc>(pSubAxis->pSeries, i);
					return;
				}
			}
		}
	}
}

void Axis_DeleteTickLabelFont(Axis * pAxis) {
	AxisFont_Destroy(&pAxis->tickLabelFont);
}

void Axis_FreeGdiRessources(Axis * pAxis) {
	
	Critical_Section(pAxis->pCs) {

		Axis_DeleteTickLabelFont(pAxis);

		if (pAxis->hdcDraw)			GDI_SAFE_CALL(DeleteObject(pAxis->hdcDraw));
		pAxis->hdcDraw = 0;
		if (pAxis->hdcErase)		GDI_SAFE_CALL(DeleteObject(pAxis->hdcErase));
		pAxis->hdcErase = 0;
		if (pAxis->hBitmapDraw)		GDI_SAFE_CALL(DeleteObject(pAxis->hBitmapDraw));
		pAxis->hBitmapDraw = 0;
		if (pAxis->hBitmapErase)	GDI_SAFE_CALL(DeleteObject(pAxis->hBitmapErase));
		pAxis->hBitmapErase = 0;
		if (pAxis->hBackGroundBrush) GDI_SAFE_CALL(DeleteObject(pAxis->hBackGroundBrush));
		pAxis->hBackGroundBrush = 0;
		if (pAxis->hCurrentBrush)	GDI_SAFE_CALL(DeleteObject(pAxis->hCurrentBrush));
		pAxis->hCurrentBrush = 0;
		if (pAxis->hCurrentPen)		GDI_SAFE_CALL(DeleteObject(pAxis->hCurrentPen));
		pAxis->hCurrentPen = 0;
		if (pAxis->hdcDraw)			GDI_SAFE_CALL(DeleteDC(pAxis->hdcDraw));
		pAxis->hdcDraw = 0;
		if (pAxis->hdcErase)		GDI_SAFE_CALL(DeleteDC(pAxis->hdcErase));
		pAxis->hdcErase = 0;
		if (pAxis->hdc)				GDI_SAFE_CALL(ReleaseDC(pAxis->hWnd, pAxis->hdc));
		pAxis->hdc = 0;
	}
}

void AxisBorder_DeleteFont(AxisBorder * pBorder) {
	AxisFont_Destroy(&pBorder->font);
}

void AxisBorder_DestroyTicks(AxisBorder * pBorder) {

	if (!pBorder->pTicks) return;

	AxisFree(pBorder->pTicks);
	pBorder->pTicks = NULL;
}

void AxisBorder_Destroy(AxisBorder * & pBorder) {
	if (!pBorder) return;
	xVect_Free<AxisFree>(pBorder->pText);
	if (pBorder->hCurrentPen) GDI_SAFE_CALL(DeletePen(pBorder->hCurrentPen));
#ifdef DEBUG_AXIS
	if (pBorder->pCurrentFont) {
		char tmp[500];
		sprintf_s(tmp, sizeof(tmp), "AxisBorder : free pCurrentFont 0x%p\n", pBorder->pCurrentFont);
	}
	if (pBorder) {
		char tmp[500];
		sprintf_s(tmp, sizeof(tmp), "AxisBorder : free pBorder 0x%p\n", pBorder);
	}
#endif
	AxisBorder_DestroyTicks(pBorder);
	AxisBorder_DeleteFont(pBorder);
	if (pBorder) AxisFree(pBorder);
	pBorder = NULL;
}

void Axis_ClearSeries(Axis * pAxis);

void SubAxis_Destroy(Axis *& subAxis) {

	Critical_Section(subAxis->pCs) {
		AxisBorder_Destroy(subAxis->pY_Axis);
		Axis_ClearSeries(subAxis);

		subAxis = nullptr;
	}
}

void Axis_DestroySubAxis(Axis * pAxis) {

	Critical_Section(pAxis->pCs) {
		xVect_static_for(pAxis->subAxis, i) {
			SubAxis_Destroy(pAxis->subAxis[i]);
		}

		xVect_Free<AxisFree>(pAxis->subAxis);
	}
}

void Axis_ClearSeries(Axis * pAxis) {

	Critical_Section(pAxis->pCs) {

		//breaks things
		//if (pAxis->pSeries && xVect_Count(pAxis->pSeries)) {
		//	Axis_Clear(pAxis);
		//}

		xVect_static_for(pAxis->pSeries, i) {
			AxisSeries_Destroy(&pAxis->pSeries[i]);
		}

		xVect_Free<AxisFree>(pAxis->pSeries);

		xVect_static_for(pAxis->subAxis, i) {
			Axis_ClearSeries(pAxis->subAxis[i]);
		}
	}
}

void Axis_DestroyCore(Axis * pAxis) {

//#ifdef _DEBUG
	DbgStr(" [0x%p] Axis_DestroyCore : Destroying Axis (id = %d, Nb Axis = %d)\n",
		GetCurrentThreadId(), pAxis->id, g_axisArray.Count());
//#endif

	if (pAxis->callbacksData) Axis_DestroyCallbacksData(pAxis);
	if (pAxis->pSeries)		  Axis_ClearSeries(pAxis);
	if (pAxis->pMatrixView)   Axis_DestroyMatrixView(pAxis);

	Axis_FreeGdiRessources(pAxis);

#ifdef DEBUG_AXIS
	char tmp[500];
	sprintf_s(tmp, sizeof(tmp), "Axis : free critical section : 0x%p\n", pAxis->pCs);
	OutputDebugString(tmp);
#endif

	AxisBorder_Destroy(pAxis->pStatusBar);
	AxisBorder_Destroy(pAxis->pX_Axis);
	AxisBorder_Destroy(pAxis->pY_Axis);
	AxisBorder_Destroy(pAxis->pY_Axis2);
	AxisBorder_Destroy(pAxis->pTitle);

	Axis_DestroyLegend(pAxis);
	Axis_DestroySubAxis(pAxis);

	DeleteCriticalSection(pAxis->pCs);
	AxisFree(pAxis->pCs);
	pAxis->pCs = NULL;
}

bool Axis_IsValidhWnd(HWND hWnd) {

	RECT rect;
	
	bool result = GetClientRect(hWnd, &rect)!=0;

	if (!result) {
		if (GetLastError() == ERROR_INVALID_WINDOW_HANDLE) {
			return false;
		}
		else WIN32_SAFE_CALL(result);
	}

	return true;
}

void AxisFont_SetName(AxisFont * pFont, wchar_t * fontName) {
	
	wcscpy_s(pFont->pLogFont->lfFaceName, fontName);
	GDI_SAFE_CALL(DeleteFont(pFont->hFont));
	GDI_SAFE_CALL(pFont->hFont = CreateFontIndirectW(pFont->pLogFont));
}

void AxisFont_SetSize(AxisFont * pFont, unsigned short size, unsigned short width = 0) {
	
	pFont->pLogFont->lfHeight = size;
	pFont->pLogFont->lfWidth = width;
	GDI_SAFE_CALL(DeleteFont(pFont->hFont));
	GDI_SAFE_CALL(pFont->hFont = CreateFontIndirectW(pFont->pLogFont));
}

void Axis_SetBackGround(Axis * pAxis, COLORREF color) {
	if (pAxis->hBackGroundBrush) {
		DeleteBrush(pAxis->hBackGroundBrush);
		pAxis->hBackGroundBrush = 0;
	}
	pAxis->BackGroundColor = color;
}

void Axis_SetTickLabelFont(Axis * pAxis, LOGFONTW * logFont) {

	AxisFont_Destroy(&pAxis->tickLabelFont);
	pAxis->tickLabelFont = AxisFont_CreateIndirect(logFont);
}

void Axis_SetTickLabelsFontSize(Axis * pAxis, int size, int width) {
	AxisFont_SetSize(&pAxis->tickLabelFont, size);
}

void Axis_SetTickLabelsFontName(Axis * pAxis, wchar_t * fName) {
	AxisFont_SetName(&pAxis->tickLabelFont, fName);
}

void Axis_SetTickLabelsFont(Axis * pAxis, wchar_t* fontName, int size, int width = 0) {
	if (fontName) AxisFont_SetName(&pAxis->tickLabelFont, fontName);
	if (size>0) AxisFont_SetSize(&pAxis->tickLabelFont, size, width);
}
void Axis_SetxLabelFont(Axis * pAxis, wchar_t* fontName, int size) {
	if (fontName) AxisFont_SetName(&pAxis->pX_Axis->font, fontName);
	if (size>0) AxisFont_SetSize(&pAxis->pX_Axis->font, size);
}
void Axis_SetyLabelFont(Axis * pAxis, wchar_t* fontName, int size) {
	if (fontName) AxisFont_SetName(&pAxis->pY_Axis->font, fontName);
	if (size>0) AxisFont_SetSize(&pAxis->pY_Axis->font, size);
}
void Axis_SetTitleFont(Axis * pAxis, wchar_t* fontName, int size) {
	if (fontName) AxisFont_SetName(&pAxis->pTitle->font, fontName);
	if (size>0) AxisFont_SetSize(&pAxis->pTitle->font, size);
}
void Axis_SetStatusBarFont(Axis * pAxis, wchar_t* fontName, int size) {
	if (fontName) AxisFont_SetName(&pAxis->pStatusBar->font, fontName);
	if (size>0) AxisFont_SetSize(&pAxis->pStatusBar->font, size);
}
void Axis_SetLegendFont(Axis * pAxis, wchar_t* fontName, int size) {
	if (pAxis->legend) {
		if (fontName) AxisFont_SetName(&pAxis->legend->font, fontName);
		if (size>0) AxisFont_SetSize(&pAxis->legend->font, size);
	}
}

void AxisFont_Set(AxisFont * pFont, LOGFONTW * logFont) {
	
	AxisFont_Destroy(pFont);
	*pFont = AxisFont_CreateIndirect(logFont);
}

void AxisFont_Update(AxisFont * pFont) {
	
	DeleteFont(pFont->hFont);
	pFont->hFont = CreateFontIndirectW(pFont->pLogFont); 
}

void AxisBorder_SetFont(AxisBorder * pBorder, LOGFONTW * logFont) {

	AxisBorder_DeleteFont(pBorder);

	logFont->lfEscapement = pBorder->isVertical ? 900 : 0;

	AxisFont_Set(&pBorder->font, logFont);
}

void AxisBorder_SetFontSize(AxisBorder * pBorder, unsigned short size) {
	AxisFont_SetSize(&pBorder->font, size);
}

AxisBorder * AxisBorder_Create(Axis * pAxis, unsigned int marge, bool isVertical) {

	AxisBorder * pBorder = (AxisBorder*)AxisMalloc(sizeof(AxisBorder));
#ifdef DEBUG_AXIS
	char tmp[500];
	sprintf_s(tmp, sizeof(tmp), "AxisBorder : malloc 0x%p, (size = %d)\n", pBorder, sizeof(AxisBorder));
	OutputDebugString(tmp);
#endif
	*pBorder = AxisBorder_Zero_Initializer;
	pBorder->pAxis = pAxis;
	pBorder->marge = marge;
	pBorder->isVertical = isVertical;

	return pBorder;
}

void Axis_UpdateBorderPos(Axis * pAxis) {

	AxisBorder * pBorder;

	pBorder = pAxis->pStatusBar;

	if (pBorder) {
		pBorder->rect.left   = 0;
		pBorder->rect.top    = pAxis->DrawArea_nPixels_y - pBorder->extent;
		pBorder->rect.right  = pAxis->DrawArea_nPixels_x + 1;
		pBorder->rect.bottom = pAxis->DrawArea_nPixels_y + 1;
	}

	pBorder = pAxis->pTitle;

	if (pBorder) {
		pBorder->rect.left   = pAxis->pY_Axis ? pAxis->pY_Axis->extent : 0;
		pBorder->rect.top    = 0;
		pBorder->rect.right  = pAxis->DrawArea_nPixels_x 
			                 - (pAxis->pY_Axis2 ? pAxis->pY_Axis2->extent : 0) + 1;
		pBorder->rect.bottom = pBorder->extent + 1;
	}
	
	pBorder = pAxis->pX_Axis;

	if (pBorder) {
		pBorder->rect.left   = pAxis->pY_Axis ? pAxis->pY_Axis->extent : 0;
		pBorder->rect.top    = pAxis->DrawArea_nPixels_y - pBorder->extent 
			                 - (pAxis->pStatusBar ? pAxis->pStatusBar->extent : 0);
		pBorder->rect.right  = pAxis->DrawArea_nPixels_x 
			                 - (pAxis->pY_Axis2 ? pAxis->pY_Axis2->extent : 0) + 1;
		pBorder->rect.bottom = pBorder->rect.top + pBorder->extent + 1;
	}
	
	pBorder = pAxis->pY_Axis;

	if (pBorder) {
		pBorder->rect.left   = 0;
		pBorder->rect.top    = pAxis->pTitle ? pAxis->pTitle->extent : 0;
		pBorder->rect.right  = pBorder->extent + 1;
		pBorder->rect.bottom = pAxis->DrawArea_nPixels_y 
			                 - (pAxis->pStatusBar ? pAxis->pStatusBar->extent : 0)
			                 - (pAxis->pX_Axis ?    pAxis->pX_Axis->extent : 0) + 1;
	}

	pBorder = pAxis->pY_Axis2;

	if (pBorder) {
		pBorder->rect.left   = pAxis->DrawArea_nPixels_x - pBorder->extent;
		pBorder->rect.top    = pAxis->pTitle ? pAxis->pTitle->extent : 0;
		pBorder->rect.right  = pAxis->DrawArea_nPixels_x + 1;
		pBorder->rect.bottom = pAxis->DrawArea_nPixels_y 
			                 - (pAxis->pStatusBar ? pAxis->pStatusBar->extent : 0)
			                 - (pAxis->pX_Axis ?    pAxis->pX_Axis->extent : 0) + 1;
	}
}

void Axis_GetAxisPtrVect(Axis ** & axisVect, HWND hWnd) {
	
	xVect_Redim<false, false, AxisMalloc, AxisRealloc>(axisVect, 0);
	
	Critical_Section(g_axisArray.CriticalSection()) {
		aVect_static_for(g_axisArray, i) {
			Axis_AssertIsNotMatrixViewLegend(g_axisArray[i], "Legend Axis not supposed to be referenced in g_axisArray");
			if (g_axisArray[i]->hWnd == hWnd) {
				xVect_Push_ByVal<AxisMalloc, AxisRealloc>(axisVect, g_axisArray[i]);
			}
		}
	}
}

Axis * Axis_FindFirst(HWND hWnd) {

	Axis * retVal = NULL;
	
	Critical_Section(g_axisArray.CriticalSection()) {
		aVect_static_for(g_axisArray, i) {
			Axis_AssertIsNotMatrixViewLegend(g_axisArray[i], "Legend Axis not supposed to be referenced in g_axisArray");
			if (g_axisArray[i]->hWnd == hWnd) {
				retVal = g_axisArray[i];
				break;
			}
		}
	}

	return retVal;
}

void AxisBorder_SetTextAlignment(AxisBorder * pBorder, DWORD textAlignment, bool val) {

	MyAssert(pBorder);

	SET_DWORD_BYTE_MASK(pBorder->textAlignment, textAlignment, 1);
}

bool Axis_CopyToClipBoard(Axis * pAxis, HENHMETAFILE hMetaFile) {

    if (pAxis) { 
		
		if (!OpenClipboard(pAxis->hWnd)) 
			return false; 

		EmptyClipboard(); 

		GDI_SAFE_CALL(SetClipboardData(CF_ENHMETAFILE, hMetaFile)); 

		CloseClipboard(); 
        
    } 
    
     return TRUE; 
}

void Axis_Resize(Axis * pAxis) {

	RECT rect;

	if (pAxis->options & AXIS_OPT_ADIM_MODE) {

		Axis_GetContainerRect(pAxis, &rect);

		if (pAxis->options & AXIS_OPT_ISMATVIEWLEGEND) {
			Axis_Repos(pAxis,
				pAxis->DrawArea_nPixels_x,
				pAxis->DrawArea_nPixels_y,
				(int)(pAxis->adim_dx * rect.right),
				(int)(pAxis->adim_dy * rect.bottom), false);
		}
		else {
			Axis_Repos(pAxis,
				(int)(pAxis->adim_nX * rect.right),
				(int)(pAxis->adim_nY * rect.bottom),
				(int)(pAxis->adim_dx * rect.right),
				(int)(pAxis->adim_dy * rect.bottom), false);
		}
	}
	else if (pAxis->options & AXIS_OPT_WINDOWED) {
		if (pAxis->options & AXIS_OPT_ISMATVIEWLEGEND) MY_ERROR("MatrixView legend not supposed to be windowed");
		Axis_GetRect(pAxis, &rect);
		Axis_Repos(pAxis, 
			rect.right - rect.left, 
			rect.bottom - rect.top, 
			rect.left, 
			rect.top, 
			false);
	}
}

double Axis_GetCoordPixelSizeX(Axis * pAxis, double x) {
	
	long i = pAxis->options & AXIS_OPT_CUR_XLOG ? Axis_CoordTransf_Y(pAxis, x) : 0;

	double x1 = Axis_GetCoordX(pAxis, i);
	double x2 = Axis_GetCoordX(pAxis, i + 1);
	
	return abs(x2 - x1);
}

double Axis_GetCoordPixelSizeY(Axis * pAxis, double y) {

	long i = pAxis->options & AXIS_OPT_CUR_YLOG ? Axis_CoordTransf_Y(pAxis, y) : 0;

	double y1 = Axis_GetCoordY(pAxis, i);
	double y2 = Axis_GetCoordY(pAxis, i + 1);
	
	return abs(y2 - y1);
}

template <class T> 
AxisSerie * Axis_GetSerieFromPointCore(
	AxisSerie * pSerie, double pixSizeX, 
	double pixSizeY, double x, double y) {

	T * xi = std::is_same<T, double>::value ? (T*)pSerie->xDouble : (T*)pSerie->xSingle;
	T * yi = std::is_same<T, double>::value ? (T*)pSerie->yDouble : (T*)pSerie->ySingle;;

	size_t N = pSerie->n;

	for (unsigned int j = 0; j < N-1; j++) {

		T x1 = xi[j];
		T x2 = xi[j + 1];
		T y1 = yi[j];
		T y2 = yi[j + 1];

		if ((x > Min(x1, x2) - pixSizeX && x < Max(x1, x2) + pixSizeX)
			&& (y > Min(y1, y2) - pixSizeY && y < Max(y1, y2) + pixSizeY)) {

			if (x1 == x2) {
				if (abs(x - x1) < pixSizeX) {
					return pSerie;
				}
			}

			double a = (y2 - y1) / (x2 - x1);
			double b = y1 - a*x1;

			if (_finite(a) && _finite(b)) {

				if (y + pixSizeY > a*(x - sign(a)*pixSizeX) + b && y - pixSizeY < a*(x + sign(a)*pixSizeX) + b) {
					return pSerie;
				}
			}
		}
	}

	return nullptr;
}

AxisSerie * Axis_GetSerieFromPoint(Axis * pAxis, double x, double y) {
	
	double pixSizeX = Axis_GetCoordPixelSizeX(pAxis, x);
	double pixSizeY = Axis_GetCoordPixelSizeY(pAxis, y);

	xVect_static_for(pAxis->pSeries, i) {
		AxisSerie * pSerie = &pAxis->pSeries[i];
		if (!pSerie->n) continue;

		if (pSerie->doublePrec)
			pSerie = Axis_GetSerieFromPointCore<double>(pSerie, pixSizeX, pixSizeY, x, y);
		else 
			pSerie = Axis_GetSerieFromPointCore<float>(pSerie, pixSizeX, pixSizeY, x, y);

		if (pSerie) return pSerie;
	}

	return nullptr;
}

AxisSerie * Axis_GetSerieFromPixel(Axis * pAxis, int x, int y) {

	double xPosDbl = x;
	double yPosDbl = y;

	Axis_GetCoord(pAxis, &xPosDbl, &yPosDbl);

	auto s = Axis_GetSerieFromPoint(pAxis, xPosDbl, yPosDbl);
	if (s) return s;

	xVect_static_for(pAxis->subAxis, i) {
		auto pSubAxis = pAxis->subAxis[i];
		auto s = Axis_GetSerieFromPixel(pSubAxis, x, y);
		if (s) return s;
	}

	return nullptr;
}


HENHMETAFILE Axis_CreateEnhMetaFile(Axis * pAxis) {

	HENHMETAFILE hMetaFile = nullptr;

	Critical_Section(pAxis->pCs) {

		DWORD fact = 10;
		unsigned short uFact = (unsigned short)fact;
		aVectEx<double, g_AxisHeap> stk;

		HDC OrigHdcDraw = pAxis->hdcDraw;
		HDC hdcMetaFile;

		RECT rect = {0};
		double   PixelsX, PixelsY, MMX, MMY;

		PixelsX = (double)GetDeviceCaps(pAxis->hdc, HORZRES);
		PixelsY = (double)GetDeviceCaps(pAxis->hdc, VERTRES);
		MMX = (double)GetDeviceCaps(pAxis->hdc, HORZSIZE);
		MMY = (double)GetDeviceCaps(pAxis->hdc, VERTSIZE);

	#define PUSH_FACTOR_MACRO(a, b) {stk.Push((double)a);a *= b;}

		PUSH_FACTOR_MACRO(pAxis->DrawArea_nPixels_x, fact);			//1
		PUSH_FACTOR_MACRO(pAxis->DrawArea_nPixels_y, fact);			//2
		PUSH_FACTOR_MACRO(pAxis->DrawArea_nPixels_dec_x, fact);		//3
		PUSH_FACTOR_MACRO(pAxis->DrawArea_nPixels_dec_y, fact);		//4

		AxisBorder * pBorder = pAxis->pTitle;


		if (pBorder) {
			PUSH_FACTOR_MACRO(pBorder->font.pLogFont->lfHeight, fact);	//5
			PUSH_FACTOR_MACRO(pBorder->marge, uFact);					//6
			if (pBorder->pTicks) {
				PUSH_FACTOR_MACRO(pBorder->pTicks->deltaLabel, fact);	//7
				PUSH_FACTOR_MACRO(pBorder->pTicks->tickHeight, uFact);	//8
				PUSH_FACTOR_MACRO(pBorder->pTicks->minorTickHeight, uFact);//9
			}
			AxisFont_Update(&pBorder->font);
		}

		pBorder = pAxis->pX_Axis;
		if (pBorder) {
			PUSH_FACTOR_MACRO(pBorder->font.pLogFont->lfHeight, fact);//10
			PUSH_FACTOR_MACRO(pBorder->marge, uFact);//11
			if (pBorder->pTicks) {
				PUSH_FACTOR_MACRO(pBorder->pTicks->deltaLabel, fact);//12
				PUSH_FACTOR_MACRO(pBorder->pTicks->tickHeight, uFact);//13
				PUSH_FACTOR_MACRO(pBorder->pTicks->minorTickHeight, uFact);//14
			}
			AxisFont_Update(&pBorder->font);
		}

		pBorder = pAxis->pY_Axis;
		if (pBorder) {
			PUSH_FACTOR_MACRO(pBorder->font.pLogFont->lfHeight, fact);//15
			PUSH_FACTOR_MACRO(pBorder->marge, uFact);//16
			if (pBorder->pTicks) {
				PUSH_FACTOR_MACRO(pBorder->pTicks->deltaLabel, fact);//17
				PUSH_FACTOR_MACRO(pBorder->pTicks->tickHeight, uFact);//18
				PUSH_FACTOR_MACRO(pBorder->pTicks->minorTickHeight, uFact);//19
			}
			AxisFont_Update(&pBorder->font);
		}

		pBorder = pAxis->pStatusBar;
		if (pBorder) {
			PUSH_FACTOR_MACRO(pBorder->font.pLogFont->lfHeight, fact);//20
			PUSH_FACTOR_MACRO(pBorder->marge, uFact);//21
			if (pBorder->pTicks) {
				PUSH_FACTOR_MACRO(pBorder->pTicks->deltaLabel, fact);//22
				PUSH_FACTOR_MACRO(pBorder->pTicks->tickHeight, uFact);//23
				PUSH_FACTOR_MACRO(pBorder->pTicks->minorTickHeight, uFact);//24
			}
			AxisFont_Update(&pBorder->font);
		}
	
		AxisLegend * pLegend = pAxis->legend;

		if (pLegend) {
			PUSH_FACTOR_MACRO(pLegend->font.pLogFont->lfHeight, fact);//25
			PUSH_FACTOR_MACRO(pLegend->lineLength, uFact);//26
			PUSH_FACTOR_MACRO(pLegend->marge, uFact);//27
			AxisFont_Update(&pLegend->font);
		}

		if (pAxis->tickLabelFont.pLogFont) {
			PUSH_FACTOR_MACRO(pAxis->tickLabelFont.pLogFont->lfHeight, fact);//28
		}
		AxisFont_Update(&pAxis->tickLabelFont);
	
	#undef PUSH_FACTOR_MACRO

		rect.left = 0;
		rect.top = 0;
		rect.right  = (long)(pAxis->DrawArea_nPixels_x * (double)MMX/PixelsX*100.0);
		rect.bottom = (long)(pAxis->DrawArea_nPixels_y * (double)MMY/PixelsY*100.0);
		GDI_SAFE_CALL(hdcMetaFile = CreateEnhMetaFile(OrigHdcDraw, NULL, &rect, NULL));

		pAxis->hdcDraw = hdcMetaFile;

		GDI_SAFE_CALL(SelectBitmap(hdcMetaFile, pAxis->hCurrentPen));
		GDI_SAFE_CALL(SelectBitmap(hdcMetaFile, pAxis->hCurrentBrush));
	
		Axis_Refresh(pAxis, false, true);

		hMetaFile = CloseEnhMetaFile(hdcMetaFile);

		size_t count = 0;

		pAxis->DrawArea_nPixels_x = (int)stk[count++];
		pAxis->DrawArea_nPixels_y = (int)stk[count++];
		pAxis->DrawArea_nPixels_dec_x = (int)stk[count++];
		pAxis->DrawArea_nPixels_dec_y = (int)stk[count++];
	
		pBorder = pAxis->pTitle;

		if (pBorder) {
			pBorder->font.pLogFont->lfHeight = (long)stk[count++];
			pBorder->marge = (unsigned short)stk[count++];
			if (pBorder->pTicks) {
				pBorder->pTicks->deltaLabel = (int)stk[count++];
				pBorder->pTicks->tickHeight = (int)stk[count++];
				pBorder->pTicks->minorTickHeight = (unsigned short)stk[count++];
			}
			AxisFont_Update(&pBorder->font);
		}

		pBorder = pAxis->pX_Axis;
		if (pBorder) {
			pBorder->font.pLogFont->lfHeight = (long)stk[count++];
			pBorder->marge = (unsigned short)stk[count++];
			if (pBorder->pTicks) {
				pBorder->pTicks->deltaLabel = (int)stk[count++];
				pBorder->pTicks->tickHeight = (unsigned short)stk[count++];
				pBorder->pTicks->minorTickHeight = (unsigned short)stk[count++];
			}
			AxisFont_Update(&pBorder->font);
		}

		pBorder = pAxis->pY_Axis;
		if (pBorder) {
			pBorder->font.pLogFont->lfHeight = (long)stk[count++];
			pBorder->marge = (unsigned short)stk[count++];
			if (pBorder->pTicks) {
				pBorder->pTicks->deltaLabel = (int)stk[count++];
				pBorder->pTicks->tickHeight = (unsigned short)stk[count++];
				pBorder->pTicks->minorTickHeight = (unsigned short)stk[count++];
			}
			AxisFont_Update(&pBorder->font);
		}

		pBorder = pAxis->pStatusBar;
		if (pBorder) {
			pBorder->font.pLogFont->lfHeight = (long)stk[count++];
			pBorder->marge = (unsigned short)stk[count++];
			if (pBorder->pTicks) {
				pBorder->pTicks->deltaLabel = (int)stk[count++];
				pBorder->pTicks->tickHeight = (unsigned short)stk[count++];
				pBorder->pTicks->minorTickHeight = (unsigned short)stk[count++];
			}
			AxisFont_Update(&pBorder->font);
		}
	
		pLegend = pAxis->legend;

		if (pLegend) {
			pLegend->font.pLogFont->lfHeight = (long)stk[count++];
			pLegend->lineLength = (unsigned short)stk[count++];
			pLegend->marge = (unsigned short)stk[count++];
			AxisFont_Update(&pLegend->font);
		}

		if (pAxis->tickLabelFont.pLogFont) {
			pAxis->tickLabelFont.pLogFont->lfHeight = (long)stk[count++];
		}
		AxisFont_Update(&pAxis->tickLabelFont);

		pAxis->hdcDraw = OrigHdcDraw;

		GDI_SAFE_CALL(SelectBitmap(OrigHdcDraw, pAxis->hCurrentPen));
		GDI_SAFE_CALL(SelectBitmap(OrigHdcDraw, pAxis->hCurrentBrush));

		Axis_Refresh(pAxis, false);

	}

	return hMetaFile;
}

void Axis_CopyMetaFileToClipBoard(Axis * pAxis) {
	
	HENHMETAFILE hMetaFile = Axis_CreateEnhMetaFile(pAxis);
	
	Axis_CopyToClipBoard(pAxis, hMetaFile);
}

AxisFont AxisFont_Copy(AxisFont * pFont) {

	
	AxisFont retVal = AxisFont_ZeroInitializer;
	
	if (pFont) {
		if (pFont->pLogFont) {
			retVal.pLogFont = (LOGFONTW *)AxisMalloc(sizeof(LOGFONTW));
			*(retVal.pLogFont) = *(pFont->pLogFont);
		}
		retVal.hFont = CreateFontIndirectW(retVal.pLogFont);
	}

	return retVal;
}

AxisTicks * AxisTicks_Copy(AxisBorder * pBorder, AxisTicks  * pTicks) {

	AxisTicks * pRet = (AxisTicks*)AxisMalloc(sizeof(AxisTicks)) ;
	*pRet = *pTicks;
	pRet->pBorder = pBorder;

	return pRet;
}

AxisBorder * AxisBorder_Copy(Axis * pAxis, AxisBorder * pBorder) {

	if (pBorder) {
		AxisBorder * pRet = (AxisBorder*)AxisMalloc(sizeof(AxisBorder));
		*pRet = *pBorder;
		pRet->font = AxisFont_Copy(&pBorder->font);
		pRet->pAxis = pAxis;
		if (pBorder->hCurrentPen) {
			LOGPEN logPen;
			GDI_SAFE_CALL(GetObject(pBorder->hCurrentPen, sizeof(LOGPEN), &logPen));
			pRet->hCurrentPen = CreatePenIndirect(&logPen);
		}
		
		if (pBorder->pText) pRet->pText = xVect_Copy<AxisMalloc, AxisRealloc>(pBorder->pText);
		if (pBorder->pTicks) pRet->pTicks = AxisTicks_Copy(pRet, pBorder->pTicks);
		return pRet;
	}
	else return NULL;
}

template <class T> 
AxisSerie AxisSerie_CopyCore(AxisSerie * pSerie, bool copyData = true) {

	T * x = copyData ? AlloCopy(std::is_same<T, double>::value ? (T*)pSerie->xDouble : (T*)pSerie->xSingle, pSerie->n, g_AxisHeap) : nullptr;
	T * y = copyData ? AlloCopy(std::is_same<T, double>::value ? (T*)pSerie->yDouble : (T*)pSerie->ySingle, pSerie->n, g_AxisHeap) : nullptr;

	wchar_t * pName = pSerie->pName ? xVect_Copy<AxisMalloc, AxisRealloc>(pSerie->pName) : NULL;
	
	return Axis_AddSerie_(NULL, pName, x, y, copyData ? pSerie->n : 0,
		pSerie->pen.lopnColor, pSerie->pen.lopnStyle, (unsigned short)pSerie->pen.lopnWidth.x,
		pSerie->pMarker ? pSerie->pMarker->markerType : 0,
		pSerie->pMarker ? pSerie->pMarker->color : AXIS_COLOR_DEFAULT,
		pSerie->pMarker ? pSerie->pMarker->nPixel : 0,
		pSerie->pMarker ? pSerie->pMarker->lineWidth : 1);
}

AxisSerie AxisSerie_Copy(Axis * pAxis, AxisSerie * pSeries, size_t i) {
	
	AxisSerie retVal;

	if (pSeries->doublePrec)
		retVal = AxisSerie_CopyCore<double>(&pSeries[i]);
	else
		retVal = AxisSerie_CopyCore<float>(&pSeries[i]);

	retVal.pAxis = pAxis;
	retVal.iSerie = i;

	return retVal;
}

AxisSerie * AxisSerie_Copy(AxisSerie * pSeries) {

	AxisSerie retVal;

	if (pSeries->doublePrec)
		retVal = AxisSerie_CopyCore<double>(pSeries);
	else
		retVal = AxisSerie_CopyCore<float>(pSeries);

	return AlloCopy(&retVal, g_AxisHeap);
}

AxisSerie * AxisSerie_CopySeries(Axis * pAxis, AxisSerie * pSeries) {

	if (pSeries) {
		AxisSerie * pRet = xVect_Copy<AxisMalloc, AxisRealloc>(pSeries);
		for (size_t i=0, I=xVect_Count(pSeries) ; i<I ; i++) {

			pRet[i] = AxisSerie_Copy(pAxis, pSeries, i);
		}
		return pRet;
	}
	else {
		return nullptr;
	}
}

Axis * Axis_Create(
	HWND hWnd, DWORD options, wchar_t * title,
	int style, bool managed, Axis * copyFrom) 
{
	
	RECT rect;

	double nx, ny, dx, dy;

	if (!hWnd) options |= AXIS_OPT_WINDOWED;
	
	options |= AXIS_OPT_ADIM_MODE;

	if (hWnd) GetClientRect(hWnd, &rect);
	else GetClientRect(GetDesktopWindow(), &rect);

	if (options & AXIS_OPT_ADIM_MODE) {
		nx = 0.5;
		ny = 0.5;
		dx = 0.25;
		dy = 0.25;
	}
	else {
		nx = rect.right;
		ny = rect.bottom;
		dx = 0;
		dy = 0;
	}

	return Axis_CreateAt(
		nx, ny, dx, dy, hWnd,
		options, title, style, managed, copyFrom);
}

Axis * Axis_Create(Axis * pAxis, DWORD options, wchar_t * title, int style, bool managed, Axis * copyFrom) {

	return Axis_Create(pAxis ? pAxis->hWnd : (HWND)NULL, options, title, style, managed, copyFrom);
}

Axis * Axis_CreateAt(
	double nx, double ny, double dx, double dy, 
	Axis * pAxis, DWORD options, wchar_t * title, int style,
	bool managed, Axis * copyFrom)
{

	return Axis_CreateAt(
		nx, ny, dx, dy, 
		pAxis ? pAxis->hWnd : (HWND)NULL, 
		options, title, style, managed, copyFrom);
}

//void Axis_AtExit(void) {
//	g_terminating = true;
//}

bool Axis_IsTerminating(void) {
	return g_terminating;
}

void Axis_AtExitFunc() {
	Axis_EndOfTimes(true);
}

void Axis_Init(void) {

	if (!g_Axis_init) {
		SYSTEM_INFO si;

		//atexit(Axis_AtExit);
		atexit(Axis_AtExitFunc);
		g_Axis_init = true;

		g_AxisHeap = HeapCreate(0, 0, 0);
		g_axisArray.CreateCriticalSection(AXIS_CS_SPINCOUNT);

		GetSystemInfo(&si);

		if (!g_nThreads) g_nThreads = si.dwNumberOfProcessors;
	}
}

Axis * Axis_CreateSubAxis(Axis * pAxis, const wchar_t * label) {

	static Axis	Axis_Zero_Initializer;

	Axis * pSubAxis = (Axis*)AxisMalloc(sizeof(*pSubAxis));

	if (!pSubAxis) MY_ERROR("malloc failure");

	*pSubAxis = Axis_Zero_Initializer;

	xVect_Push_ByVal<AxisMalloc, AxisRealloc>(pAxis->subAxis, pSubAxis);

	pSubAxis->pCs = pAxis->pCs;

	pSubAxis->pY_Axis = AxisBorder_Create(pSubAxis, 20, true);
	AxisBorder_SetTextAlignment(pSubAxis->pY_Axis, AXIS_ALIGN_XLEFT | AXIS_ALIGN_YCENTER, 1);
	AxisBorder_SetFont(pSubAxis->pY_Axis, pAxis->pY_Axis->font.pLogFont);
	pSubAxis->pY_Axis->pTicks = AxisBorder_CreateTicks(pSubAxis->pY_Axis);
	AxisBorder_SetText(pSubAxis->pY_Axis, label);

	return pSubAxis;
}

void Axis_CopyAxisLimits(Axis * from, Axis * to) {
	to->xMin_orig = from->xMin_orig;
	to->xMax_orig = from->xMax_orig;
	to->yMin_orig = from->yMin_orig;
	to->yMax_orig = from->yMax_orig;
	to->xMin = from->xMin;
	to->xMax = from->xMax;
	to->yMin = from->yMin;
	to->yMax = from->yMax;
}

Axis * Axis_CreateAt(double nx, double ny, double dx, 
			double dy, HWND hWnd, DWORD options, 
			wchar_t * title, int style, bool managed, 
			Axis * copyFrom, bool createWindowForAxisIfNeeded)
{
		
	static int count = 0;
	Axis * pAxis = NULL;

	if ((options & AXIS_OPT_WINDOWED) && (options & AXIS_OPT_ISMATVIEWLEGEND)) MY_ERROR("AXIS_OPT_WINDOWED incompatible with AXIS_OPT_ISMATVIEWLEGEND");

	style |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
	
	WNDCLASSW ci;
	
	if (!GetClassInfoW(GetModuleHandle(NULL), L"AxisWinClass", &ci)) {
		MyRegisterClass(AxisWindowProc, L"AxisWinClass");
	}

	if (!g_hAxisMgrThread) 
		CreateManagedAxisWindow(
			L"", 0, 0, 0, 0, 0, 0, 
			NULL, NULL, NULL, NULL, true);
	
	if (hWnd) style |= WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
	if (title && (*title != 0)) style |= WS_CAPTION;

	if (hWnd && !Axis_IsValidhWnd(hWnd)) {
		OutputDebugStringA("Axis_CreateAt : invalid window\n");
		return NULL;
	}

	if ((options & AXIS_OPT_WINDOWED) && createWindowForAxisIfNeeded) {

		HWND axisHwnd;
		if (managed) {
			axisHwnd = CreateManagedAxisWindow(title, dx, dy, nx, ny, options, style, hWnd, NULL, NULL, copyFrom);
		
			bool found  = false;

			Critical_Section(g_axisArray.CriticalSection()) {
		
				aVect_static_for(g_axisArray, i) {
					pAxis = g_axisArray[i];
					Axis_AssertIsNotMatrixViewLegend(pAxis, "Legend Axis not supposed to be referenced in g_axisArray");
					if (axisHwnd == pAxis->hWnd) {
						found = true;
						break;
					}
				}
			}

			if (!found) MY_ERROR("ici");
		}
		else {
			RECT r1, r2;
			double decx = 0;
			double decy = 0;
			if (options & AXIS_OPT_ADIM_MODE)	{
				if (hWnd) {
					GetClientRect(hWnd, &r1);
				}
				else GetClientRect(GetDesktopWindow(), &r1);
			}
		
			auto cond = options & AXIS_OPT_ADIM_MODE;
			double ndx = cond ? MyRound(dx * r1.right)  : dx;
			double ndy = cond ? MyRound(dy * r1.bottom) : dy;
			double nnx = cond ? MyRound(nx * r1.right)  : nx;
			double nny = cond ? MyRound(ny * r1.bottom) : ny;
			
			DbgStr(L" [0x%p] Axis_CreateAt : \"%s\"\n", GetCurrentThreadId(), title);

			auto createInvisible = options & AXIS_OPT_CREATEINVISIBLE;

			axisHwnd = MyCreateWindow(L"AxisWinClass", title, (int)ndx, (int)ndy, (int)nnx,
				(int)nny, style, hWnd, NULL, nullptr, createInvisible ? SW_HIDE : SW_SHOW);
			
			GetClientRect(axisHwnd, &r1);
			GetWindowRect(axisHwnd, &r2);
			decx = (r2.right - r2.left) - (r1.right - r1.left);
			decy = (r2.bottom - r2.top) - (r1.bottom - r1.top);

			double ndecx = (cond) ? MyRound(decx / r1.right)  : decx;
			double ndecy = (cond) ? MyRound(decy / r1.bottom) : decy;

			//pAxis = Axis_CreateAt(nx - ndecx, ny - ndecy, 0, 0, axisHwnd, options & ~AXIS_OPT_WINDOWED, title, style, false, copyFrom);
			pAxis = Axis_CreateAt(nx - ndecx, ny - ndecy, 0, 0, axisHwnd, options, title, style, false, copyFrom, false);
		}
		
		if (pAxis->options != options) MY_ERROR("ICI");
		
		if (options & AXIS_OPT_ADIM_MODE) {
			Axis_SetAdimPos(pAxis, nx, ny, dx, dy);
			Axis_AdimResizeWindow(pAxis);
		}

		Axis_Clear(pAxis);

		return pAxis;
	}
	else {
		
		Axis_Init();

		MyAssert(!pAxis);

		pAxis = (Axis *)AxisMalloc(sizeof(*pAxis));
#ifdef DEBUG_AXIS
		char tmp[500];
		sprintf_s(tmp, sizeof(tmp), "Axis : malloc pAxis 0x%p (size = %d)\n", pAxis, sizeof(*pAxis));
		OutputDebugString(tmp);
#endif

		*pAxis = Axis_Zero_Initializer;

		pAxis->pCs = (CRITICAL_SECTION *)AxisMalloc(sizeof(CRITICAL_SECTION));

#ifdef DEBUG_AXIS
		sprintf_s(tmp, sizeof(tmp), "Axis : malloc critical section : 0x%p (size = %d)\n", pAxis->pCs, sizeof(CRITICAL_SECTION));
		OutputDebugString(tmp);
#endif

		InitializeCriticalSectionAndSpinCount(pAxis->pCs, AXIS_CS_SPINCOUNT);

		Critical_Section(pAxis->pCs) {

			count++;

			pAxis->id = count;
			pAxis->hWnd = hWnd;
			pAxis->lockCount = 1;
			pAxis->hWndAxisMgrWinProc = g_hWndAxisMgrWnd;

#ifdef _DEBUG
		DbgStr(L" [0x%p] Creating Axis (id = %d, Nb Axis = %d)\n", GetCurrentThreadId(), pAxis->id, g_axisArray.Count());
#endif

			pAxis->options = options;

			bool mustCopyMatrixView = false;

			if (copyFrom) {
				Axis_CopyAxisLimits(copyFrom, pAxis);
			} else {
				pAxis->xMin_orig = pAxis->xMin = 0;
				pAxis->xMax_orig = pAxis->xMax = 1;
				pAxis->yMin_orig = pAxis->yMin = 0;
				pAxis->yMax_orig = pAxis->yMax = 1;
			}

			pAxis->BackGroundColor = copyFrom ? copyFrom->BackGroundColor : RGB(255, 255, 255);
			if ((pAxis->options & AXIS_OPT_GHOST)
				&& !(pAxis->options & AXIS_OPT_MAINHDC)) {
				pAxis->hdc = 0;
			}
			else {
				GDI_SAFE_CALL(pAxis->hdc = GetDC(hWnd));
			}

			Axis_SetBackGround(pAxis, RGB(255, 255, 255));

			if (!(pAxis->options & AXIS_OPT_GHOST) || (pAxis->options & AXIS_OPT_MAINHDC)) {

				if (copyFrom) {

					pAxis->tickLabelFont = AxisFont_Copy(&copyFrom->tickLabelFont);
					pAxis->pTitle        = AxisBorder_Copy(pAxis, copyFrom->pTitle);
					pAxis->pX_Axis       = AxisBorder_Copy(pAxis, copyFrom->pX_Axis);
					pAxis->pY_Axis       = AxisBorder_Copy(pAxis, copyFrom->pY_Axis);
					pAxis->pY_Axis2      = AxisBorder_Copy(pAxis, copyFrom->pY_Axis2);
					pAxis->pStatusBar    = AxisBorder_Copy(pAxis, copyFrom->pStatusBar);
					pAxis->pSeries       = AxisSerie_CopySeries(pAxis, copyFrom->pSeries);

					if (xVect_Count(copyFrom->subAxis)) {

						xVect_static_for(copyFrom->subAxis, i) {
							auto pSubAxis = Axis_CreateSubAxis(pAxis, copyFrom->subAxis[i]->pY_Axis->pText);
							auto&& pSubAxisFrom = copyFrom->subAxis[i];

							Axis_CopyAxisLimits(pSubAxisFrom, pSubAxis);
							pSubAxis->pSeries = AxisSerie_CopySeries(pSubAxis, pSubAxisFrom->pSeries);
						}
					}

					//when subAxis exist, id is the same for the real and the fake serie (fake serie = empty serie only for legend)
					xVect_static_for(copyFrom->subAxis, i) {
						auto pSubAxis = copyFrom->subAxis[i];
						xVect_static_for(pSubAxis->pSeries, j) {
							xVect_static_for(copyFrom->pSeries, k) {
								if (pSubAxis->pSeries[j].id == copyFrom->pSeries[k].id) {
									pAxis->subAxis[i]->pSeries[j].id = pAxis->pSeries[k].id;
								}
							}
						}
					}

					pAxis->adim_dx = dx;
					pAxis->adim_dy = dy;
					pAxis->adim_nX = nx;
					pAxis->adim_nY = ny;
					
					mustCopyMatrixView = true;
				}
				else {

					LOGFONTW logFont = CreateLogFont(L"arial", 16);
					Axis_SetTickLabelFont(pAxis, &logFont);

					if (pAxis->options & AXIS_OPT_TOPBORDER) {
						logFont.lfHeight = 20;
						wcscpy_s(logFont.lfFaceName, L"arial");
						pAxis->pTitle = AxisBorder_Create(pAxis, 15, false);
						AxisBorder_SetTextAlignment(pAxis->pTitle, AXIS_ALIGN_XCENTER | AXIS_ALIGN_YCENTER, 1);
						AxisBorder_SetFont(pAxis->pTitle, &logFont);
					}
					
					wcscpy_s(logFont.lfFaceName, L"MS Shell Dlg");
					logFont.lfHeight = 18;

					if (pAxis->options & AXIS_OPT_BOTTOMBORDER) {
						pAxis->pX_Axis = AxisBorder_Create(pAxis, 5, false);
						AxisBorder_SetTextAlignment(pAxis->pX_Axis, AXIS_ALIGN_XCENTER | AXIS_ALIGN_YBOTTOM, 1);
						AxisBorder_SetFont(pAxis->pX_Axis, &logFont);
						pAxis->pX_Axis->pTicks = AxisBorder_CreateTicks(pAxis->pX_Axis);
					}
			
					if (pAxis->options & AXIS_OPT_LEFTBORDER) {
						pAxis->pY_Axis = AxisBorder_Create(pAxis, 20, true);
						AxisBorder_SetTextAlignment(pAxis->pY_Axis, AXIS_ALIGN_XLEFT | AXIS_ALIGN_YCENTER, 1);
						AxisBorder_SetFont(pAxis->pY_Axis, &logFont);
						pAxis->pY_Axis->pTicks = AxisBorder_CreateTicks(pAxis->pY_Axis);
					}

					if (pAxis->options & AXIS_OPT_RIGHTBORDER) {
						pAxis->pY_Axis2 = AxisBorder_Create(pAxis, 30, true);
						AxisBorder_SetTextAlignment(pAxis->pY_Axis2, AXIS_ALIGN_XRIGHT | AXIS_ALIGN_YCENTER, 1);
						AxisBorder_SetFont(pAxis->pY_Axis2, &logFont);
					}

					if (pAxis->options & AXIS_OPT_STATUSPOS) {
						//strcpy(logFont.lfFaceName, "Arial");
						auto desiredFont = L"consolas";
						wcscpy(logFont.lfFaceName, desiredFont);
						logFont.lfHeight = 15;
						pAxis->pStatusBar = AxisBorder_Create(pAxis, 5, 0);
						AxisBorder_SetTextAlignment(pAxis->pStatusBar, AXIS_ALIGN_XLEFT | AXIS_ALIGN_YTOP, 1);
						AxisBorder_SetFont(pAxis->pStatusBar, &logFont);

						wchar_t tmp[50];
						SAFE_CALL(SelectFont(pAxis->hdc, pAxis->pStatusBar->font.hFont));
						GetTextFaceW(pAxis->hdc, sizeof tmp, tmp);

						if (strcmp_caseInsensitive(tmp, desiredFont) != 0) {
							AxisBorder_DeleteFont(pAxis->pStatusBar);
							wcscpy(logFont.lfFaceName, L"Arial");
							AxisBorder_SetFont(pAxis->pStatusBar, &logFont);
						}

						AxisBorder_SetText(pAxis->pStatusBar, L"");
					}
				}
			}

			if (options & AXIS_OPT_ADIM_MODE) 
				Axis_SetAdimPos(pAxis, nx, ny, dx, dy, true, false);
			else 
				Axis_ReposCore(pAxis, (unsigned int)nx, (unsigned int)ny, (unsigned int)dx, (unsigned int)dy);
		
			if (mustCopyMatrixView) Axis_CopyMatrixView(pAxis, copyFrom->pMatrixView);

			if (pAxis->hdcDraw) { 
				Axis_SelectPen(pAxis, CreatePen(PS_SOLID, 1, RGB(50, 70, 150)));
				Axis_SelectBrush(pAxis, GetStockBrush(NULL_BRUSH));
			}

			if (!Axis_IsMatrixViewLegend(pAxis))
				AXIS_SAFE_ACCESS(g_axisArray.Push(pAxis));
		}
	}

	return pAxis;
}

DWORD WINAPI RefreshWorkerThread(LPVOID pParams) {
	AxisRefreshThreadParam * params = (AxisRefreshThreadParam *)pParams;

	DbgStr(L"+[0x%p] New thread : RefreshWorkerThread\n", GetCurrentThreadId());

	HANDLE hEvents[2];
	int ind = params->ind;
	
	g_hEventKillRefreshThread.Push(CreateEvent(NULL, 1, 0, NULL));
	MyAssert(ind == g_hEventKillRefreshThread.Count()-1);

	g_hEventRefreshThreadKilled.Push(CreateEvent(NULL, 1, 0, NULL));
	MyAssert(ind == g_hEventRefreshThreadKilled.Count() - 1);

	hEvents[0] = params->hEventGo;
	hEvents[1] = g_hEventKillRefreshThread[ind];

	bool refreshThreadReady = false;

	EVENT_SAFE_CALL(SetEvent(params->hEventThreadCreated));
	
	DWORD ObjectInd;

	while(true) {

		ObjectInd = WaitForMultipleObjects(2, hEvents, false, INFINITE) - WAIT_OBJECT_0;

		if (ObjectInd  == 0) {

			bool lockRefresh = false;

			while (true) {

				size_t n = 0;
				Axis * pAxis = NULL;
				bool autoFit = false, redraw = false, destroy = false, toMetaFile = false;
				unsigned long long id;

				Critical_Section(&params->homeWork.cs) {

					n = xVect_Count(params->homeWork.pAxis);

					lockRefresh =  n>100;

					if (n>0) {
						pAxis		= params->homeWork.pAxis[0];
						autoFit		= params->homeWork.autoFit[0];
						redraw		= params->homeWork.redraw[0];
						destroy		= params->homeWork.destroy[0];
						toMetaFile  = params->homeWork.toMetaFile[0];
						id			= params->homeWork.id[0];
					}
				} 

				if (n>0) {

					if (destroy) 
						Axis_Destroy(pAxis);
					else 
						Axis_Refresh(pAxis, autoFit, toMetaFile, redraw);

					if (!params->homeWork.cs.DebugInfo) MY_ERROR("ICI");

					Critical_Section(&params->homeWork.cs) {

						size_t new_n = xVect_Count(params->homeWork.pAxis);
						
						if (new_n > 0 && params->homeWork.id[0] == id) {
							xVect_Remove<AxisMalloc, AxisRealloc>(params->homeWork.pAxis, 0);
							xVect_Remove<AxisMalloc, AxisRealloc>(params->homeWork.autoFit, 0);
							xVect_Remove<AxisMalloc, AxisRealloc>(params->homeWork.redraw, 0);
							xVect_Remove<AxisMalloc, AxisRealloc>(params->homeWork.destroy, 0);
							xVect_Remove<AxisMalloc, AxisRealloc>(params->homeWork.toMetaFile, 0);
							xVect_Remove<AxisMalloc, AxisRealloc>(params->homeWork.id, 0);
						}
						else {
							int a = 0;
						}

						if (new_n < AXIS_MAX_WORK) {
							SetEvent(params->hEventRefreshThreadReady);
						}
					}
				}
				else {
					break;
				}
			}
		}
		else if (ObjectInd == 1) {
			EVENT_SAFE_CALL(SetEvent(g_hEventRefreshThreadKilled[ind]));
			break;
		}
		else {
			MY_ERROR("ici");
		}
	}

	DbgStr(L"-[0x%p] RefreshWorkerThread exiting...\n", GetCurrentThreadId());

	return true;
}

AxisSerieMarker * AxisSerieMarker_Create(Axis * pAxis, size_t iSerie,
	DWORD markerType, COLORREF color = AXIS_COLOR_DEFAULT, 
	unsigned short nPixel = 2, unsigned short lineWidth = 1) {
	
	AxisSerieMarker * retVal = (AxisSerieMarker *)AxisMalloc(sizeof(AxisSerieMarker));
	retVal->pAxis = pAxis;
	retVal->iSerie = iSerie;
	retVal->markerType = markerType;
	retVal->color = color;
	retVal->lineWidth = lineWidth;
	retVal->nPixel = nPixel;

	if (color == AXIS_COLOR_DEFAULT) {
		retVal->color = pAxis->pSeries[iSerie].pen.lopnColor;
	}

	return retVal;
}

void AxisSerieMarker_Destroy(AxisSerieMarker * & pMarker) {
	AxisFree((void*)pMarker);
	pMarker = NULL;
}

void Axis_SetIcon(HICON hIcon, HICON hIconSmall) {

	WNDCLASSW wc;

	if (!GetClassInfoW(GetModuleHandle(NULL), L"AxisWinClass", &wc)) {
		MyRegisterClass(AxisWindowProc, L"AxisWinClass");
	}

	HWND tmpWnd = CreateWindowA(
		"AxisWinClass", NULL, 0, 0, 0, 0, 0, 
		HWND_MESSAGE, 0, GetModuleHandle(NULL), NULL);

	
	if (!tmpWnd) {
		
		static bool once;
		if (!once) {
			once = true;
			auto errorDesc = GetWin32ErrorDescription(GetLastError());
			CoreDump(aVect<char>(errorDesc), NULL, GetCurrentThreadId(), __FILE__, __LINE__, __FUNCTION__);
			MessageBoxA(NULL, "Could not set axis window icon.", "Error", MB_ICONINFORMATION);
		}
		
		return;
	}

	if (hIcon) SetClassLongPtr(tmpWnd, GCLP_HICON, (LONG_PTR)hIcon);
	if (hIconSmall) SetClassLongPtr(tmpWnd, GCLP_HICONSM, (LONG_PTR)hIconSmall);

	SAFE_CALL(DestroyWindow(tmpWnd));
}

//VOID NTAPI MyFlsCallBack (PVOID lpFlsData) {
//	EVENT_SAFE_CALL(SetEvent(g_hEventEndOfTimes));
//}

//unsigned int _stdcall MessagePump(void * params) {
DWORD WINAPI AxisMessagePump(LPVOID params) {

	static MSG msgInit;
	MSG msg;

	DbgStr(L"+[0x%p] New thread : AxisMessagePump\n", GetCurrentThreadId());

	AxisWindowParam * CW_params = (AxisWindowParam *)params;

	while (!g_hWndAxisMgrWnd || GetMessageW(&msg, NULL, 0, 0)) {

		if (!g_hWndAxisMgrWnd) {

			msg = msgInit;
			MyRegisterClass(AxisMgrWinProc, L"AxisMgr");

			g_hWndAxisMgrWnd = MyCreateWindow(
					L"AxisMgr", L"", 0, 0, 0, 0,
					0, 0, (HMENU)0, 0, SW_HIDE);

			if (CW_params->className[0] != 0) {

				msg.hwnd = g_hWndAxisMgrWnd;
				msg.message = AXIS_MSG_CREATE_WINDOW;
				msg.wParam = (WPARAM)CW_params;
			}

			EVENT_SAFE_CALL(SetEvent(CW_params->hEventThreadCreated));

		}

		TranslateMessage(&msg) ;
		DispatchMessageW(&msg) ;
	}

	DbgStr(L" [0x%p] AxisMgrThread : Message pump exited, destroying Axis Heap\n", GetCurrentThreadId());

	g_axisArray.Leak();
	if (g_AxisHeap) WIN32_SAFE_CALL(HeapDestroy(g_AxisHeap));
	g_Axis_init = false;
	g_AxisHeap = NULL;

	DbgStr(L"-[0x%p] AxisMgrThread: Thread exiting\n", GetCurrentThreadId());

	//WinEvent e(NULL, false, false, NULL);
	//e.WaitFor(1000 * 5);

	//potential risk of dll being unmapped just before after SetEvent and before returning, when thread exit is caused by Axis_EndOfTimes
	//no better idea though...
	//probably safe because Axis_EndOfTimes does significant work after waiting on g_hEventEndOfTimes and before returning...
	if (g_hEventEndOfTimes) EVENT_SAFE_CALL(SetEvent(g_hEventEndOfTimes));

	//Useless, the risk of dll being unmapped just before returning is also there in the callback
	//auto hf = FlsAlloc(MyFlsCallBack);
	//FlsGetValue(hf);
	//FlsSetValue(hf, (LPVOID)1);
	////FlsFree(hf);

	return ((DWORD)msg.wParam);
}

void Axis_CloseAllWindows(bool terminating) {

	aVectEx<HWND, g_AxisHeap> hwndArray;
	//aVectEx<char, g_AxisHeap> tmp;
	
	AutoCriticalSection acs;

	if (!g_axisArray) return;

	if (!terminating) acs.Enter(g_axisArray.CriticalSection());

	if (g_axisArray.Count() > 0) {
	
		Axis ** ptr = &g_axisArray[0];
		if (terminating) g_axisArray.Leak();

		aVect_static_for(g_axisArray, i) {

			Axis_AssertIsNotMatrixViewLegend(ptr[i], "Legend Axis not supposed to be referenced in g_axisArray");

			bool found = false;
			HWND axisHwnd = ptr[i]->hWnd;

			aVect_static_for(hwndArray, j) {
				if (hwndArray[j] == axisHwnd) {
					found = true;
					break;
				}
			}
			if (!found) {
				hwndArray.Push(axisHwnd);
			}
		}

		aVect_static_for(hwndArray, i) {
			Axis_CloseWindow(hwndArray[i]);
		}
	}	
}

HWND CreateManagedAxisWindow(
	wchar_t * strTitle, 
	double x, double y, 
	double nX, double nY,
	DWORD options,
	DWORD style,
	HWND parent,
	HMENU menu,
	void * pData,
	Axis * copyFrom, 
	bool empty)
{

	static CRITICAL_SECTION cs;
	static bool cs_init;

	if (!cs_init) {
		cs_init = true;
		InitializeCriticalSectionAndSpinCount(&cs, AXIS_CS_SPINCOUNT);
	}

	//static unsigned int windowsManagerThreadID = NULL;
	static DWORD windowsManagerThreadID = NULL;
	static HANDLE hEventWindowCreated   = NULL;
	static HANDLE hEventThreadCreated   = NULL;
	static struct AxisWindowParam params;
	static bool init = 0;
	static wchar_t * className = L"AxisWinClass";

	bool isAxisMgrThread;

	Critical_Section(&cs) {
		if (!g_hAxisMgrThread) {
			hEventWindowCreated = CreateEvent(NULL, 1, 0, NULL);
			hEventThreadCreated = CreateEvent(NULL, 1, 0, NULL);
			if (!hEventThreadCreated) MY_ERROR("CreateEvent failed");
			isAxisMgrThread = 0;
		}
		else {
			isAxisMgrThread = (GetCurrentThreadId() == windowsManagerThreadID);
		}

		params.className = empty ? L"" : className;
		params.strTitle = strTitle;
		params.x = x;
		params.y = y;
		params.nX = nX;
		params.nY = nY;
		params.options = options;
		params.style = style;
		params.parent = parent;
		params.menu = menu;
		params.pData = pData;
		params.hEventWindowCreated = hEventWindowCreated;
		params.hEventThreadCreated = hEventThreadCreated;
		params.copyFrom = copyFrom;

		EVENT_SAFE_CALL(ResetEvent(hEventThreadCreated));

		if (!g_hAxisMgrThread) {
			g_hAxisMgrThread = (HANDLE)1;

			DbgStr(L" [0x%p] CreateManagedAxis : _beginthreadex AxisMessagePump : params.className = \"%s\"\n", GetCurrentThreadId(), params.className);

			//EVENT_SAFE_CALL(g_hAxisMgrThread = (HANDLE)_beginthreadex(
			//	NULL, NULL, MessagePump, (void*)&params, 
			//	NULL, &windowsManagerThreadID)); 

			EVENT_SAFE_CALL(g_hAxisMgrThread = (HANDLE)CreateThread(
				NULL, 0, AxisMessagePump, (void*)&params, 
				NULL, &windowsManagerThreadID)); 
	
			if (!WaitForSingleObject(hEventThreadCreated, INFINITE) == WAIT_OBJECT_0) {
				MY_ERROR("ici");
			}
		}
		else if (!empty){

			DbgStr(L" [0x%p] CreateManagedAxis : params.className = \"%s\"\n", GetCurrentThreadId(), params.className);

			if (isAxisMgrThread) {
				SendMessageW(g_hWndAxisMgrWnd, AXIS_MSG_CREATE_WINDOW, (WPARAM)&params, NULL);
			}
			else {
				WIN32_SAFE_CALL(PostMessageW(g_hWndAxisMgrWnd, AXIS_MSG_CREATE_WINDOW, (WPARAM)&params, NULL));
			}

			if (WaitForSingleObject(hEventWindowCreated, INFINITE) == WAIT_OBJECT_0) {
				EVENT_SAFE_CALL(ResetEvent(hEventWindowCreated));
			}
			else {
				MY_ERROR("ici");
			}
		}
	}

	return params.result_hWnd;
}

#define AXIS_GDI_MAX_PIXEL (1073741951)

void Axis_DrawRectangle(Axis * axis, double left, double top, double right, double bottom) {

	Axis_CoordTransf(axis, &left, &top);
	Axis_CoordTransf(axis, &right, &bottom);

	if (left > right)  Swap(left, right);
	if (top >  bottom) Swap(top, bottom);

	Rectangle(axis->hdcDraw, (int)left, (int)top, (int)right + 1, (int)bottom + 1);
}

void Axis_DrawRectangle2(Axis * pAxis, double left, double top, double right, double bottom, COLORREF rgbColor) {

	long left_int, top_int, right_int, bottom_int;

	Axis_CoordTransfDoubleToInt(pAxis, &left_int, &top_int, left, top);
	Axis_CoordTransfDoubleToInt(pAxis, &right_int, &bottom_int, right, bottom);

	if (left_int > right_int)  Swap(left_int, right_int);
	if (top_int >  bottom_int) Swap(top_int, bottom_int);

	DWORD * pData = (DWORD*)pAxis->bitmapDrawPtr;
	size_t bitmapWidth = pAxis->DrawArea_nPixels_x;
	size_t bitmapHeight = pAxis->DrawArea_nPixels_y;

	if ((size_t)left_int   > bitmapWidth - 1)  left_int   = (long)bitmapWidth - 1;
	if ((size_t)right_int  > bitmapWidth - 1)  right_int  = (long)bitmapWidth - 1;
	if ((size_t)top_int    > bitmapHeight - 1) top_int    = (long)bitmapHeight - 1;
	if ((size_t)bottom_int > bitmapHeight - 1) bottom_int = (long)bitmapHeight - 1;

	if (left_int   < 0) left_int = 0;
	if (right_int  < 0) right_int = 0;
	if (top_int    < 0) top_int = 0;
	if (bottom_int < 0) bottom_int = 0;

	for (int i = top_int; i < bottom_int + 1; ++i) {
		for (int j = left_int; j < right_int + 1; ++j) {
			pData[i*bitmapWidth + j] = rgbColor;
		}
	}
}


void Axis_DrawRectangle3(Axis * pAxis, double left, double top, double right, double bottom, COLORREF rgbColor) {

	LONG left_int, top_int, right_int, bottom_int;

	Axis_CoordTransfDoubleToInt(pAxis, &left_int, &top_int, left, top);
	Axis_CoordTransfDoubleToInt(pAxis, &right_int, &bottom_int, right, bottom);

	DWORD * pData = (DWORD*)pAxis->bitmapDrawPtr;
	size_t bitmapWidth = pAxis->DrawArea_nPixels_x;

	size_t I = pAxis->bitmapDrawCount;

	for (int i = top_int; i < bottom_int + 1; ++i) {
		for (int j = left_int; j < right_int + 1; ++j) {
			size_t ind = i*bitmapWidth + j;
			if (ind < I) pData[ind] = rgbColor;
		}
	}
}

void Axis_DrawEllipse(Axis * axis, double left, double top, double right, double bottom) {
	
	Axis_CoordTransf(axis, &left,  &top);
	Axis_CoordTransf(axis, &right, &bottom);

	if (left > right)  Swap(left, right);
	if (top >  bottom) Swap(top,  bottom);
	
	Ellipse(axis->hdcDraw, (int)left, (int)top, (int)right + 1, (int)bottom + 1);
}

void Axis_Release(Axis * pAxis) {

	if (Axis_IsTerminating()) return;

	if (pAxis->options & AXIS_OPT_GHOST && !(pAxis->options & AXIS_OPT_USERGHOST)) return;

	DbgStr(L" [0x%p] Axis_Release (pAxis = 0x%p, id = %d)\n", GetCurrentThreadId(), pAxis, pAxis->id);

	auto retVal = WaitForSingleObject(g_hAxisMgrThread, 0);

	if (retVal == WAIT_OBJECT_0 || retVal == WAIT_FAILED) return;

	EVENT_SAFE_CALL(PostMessageW(pAxis->hWndAxisMgrWinProc, AXIS_MSG_RELEASE, (WPARAM) pAxis, NULL));
}

double Axis_xMin(Axis * pAxis, bool orig) {
	return orig ? pAxis->xMin_orig : pAxis->xMin;
}
double Axis_xMax(Axis * pAxis, bool orig) {
	return orig ? pAxis->xMax_orig : pAxis->xMax;
}
double Axis_yMin(Axis * pAxis, bool orig) {
	return orig ? pAxis->yMin_orig : pAxis->yMin;
}
double Axis_yMax(Axis * pAxis, bool orig) {
	return orig ? pAxis->yMax_orig : pAxis->yMax;
}

void Axis_GetUserRectSel(Axis * pAxis, POINT * pt, RECT * rect, int mouseEvent) {
	MSG msg;
	HWND hWnd = pAxis->hWnd;

	int xPos = -1;
	int yPos = -1;

	RECT rect2;

	GetWindowRect(hWnd, &rect2);
	SetCapture(hWnd);

	while (Axis_IsValidhWnd(pAxis->hWnd)) {
		int count = 0;

		GetMessageW(&msg, NULL, 0, 0);

		bool messageHandled = false;
		if (msg.message == mouseEvent) {
			
			int xPos = GET_X_LPARAM(msg.lParam); 
			int yPos = GET_Y_LPARAM(msg.lParam);

			messageHandled = true;

			if (abs(pt->x - xPos) < 2 && abs(pt->y - yPos) < 2 ) {
				memset(rect,0,sizeof(*rect));
				break;
			}

			Axis_Redraw(pAxis);

			rect->left    = pt->x - pAxis->DrawArea_nPixels_dec_x;
			rect->right   = xPos  - pAxis->DrawArea_nPixels_dec_x;
			rect->top     = pt->y - pAxis->DrawArea_nPixels_dec_y;
			rect->bottom  = yPos  - pAxis->DrawArea_nPixels_dec_y;

			if (rect->left > rect->right) 
				Swap(rect->left, rect->right);
			if (rect->top > rect->bottom) 
				Swap(rect->top, rect->bottom);

			break;
		}
		else if (msg.message == WM_MOUSEMOVE) { 
				
			HPEN oldPen;
			HBRUSH oldBrush;
			static HPEN hPen;
			static HBRUSH hBrush;

			messageHandled = true;

			xPos = GET_X_LPARAM(msg.lParam); 
			yPos = GET_Y_LPARAM(msg.lParam);
			Axis_Redraw(pAxis);
				
			if (!hPen) hPen = CreatePen(PS_SOLID,2,RGB(100,100,100));
			GDI_SAFE_CALL(oldPen = SelectPen(pAxis->hdc, hPen));
			if (oldPen!=0 && oldPen!=hPen) GDI_SAFE_CALL(DeletePen(oldPen));
			if (!hBrush) hBrush = GetStockBrush(NULL_BRUSH);
			oldBrush = SelectBrush(pAxis->hdc, GetStockBrush(NULL_BRUSH));
			if (oldBrush!=0 && oldBrush!=hBrush)  GDI_SAFE_CALL(DeleteBrush(oldBrush));
			Rectangle(pAxis->hdc, pt->x, pt->y, xPos, yPos);
		}
		else if (msg.message == WM_LBUTTONDOWN ||
			     msg.message == WM_LBUTTONUP || 
				 msg.message == WM_RBUTTONDOWN ||
				 msg.message == WM_RBUTTONUP) {
			messageHandled = true;
		}

		if (!messageHandled) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}
	ReleaseCapture();
}

CRITICAL_SECTION * OnlyIfNotLegend(Axis * pAxis, CRITICAL_SECTION * pCs) {
	return Axis_IsMatrixViewLegend(pAxis) ? nullptr : pCs;
}

bool Axis_CloseLastOpenedAxis() {

	Axis * pAxis = nullptr;

	Critical_Section(g_axisArray.CriticalSection()) {
		unsigned long long maxID = 0;

		for (auto&& axis : g_axisArray) {
			EnterCriticalSection(axis->pCs);
			if (axis->id >= maxID && !(axis->options & AXIS_OPT_2BECLOSED)) {
				maxID = axis->id;
				pAxis = axis;
			}
		}

		if (pAxis) {
			Axis_CloseWindow(pAxis);
			pAxis->options |= AXIS_OPT_2BECLOSED;
		}

		for (auto&& axis : g_axisArray) {
			LeaveCriticalSection(axis->pCs);
		}
	}

	return pAxis != nullptr;
}

bool Axis_Destroy(Axis * pAxis) {

	Critical_Section(OnlyIfNotLegend(pAxis, g_axisArray.CriticalSection())) {

		AutoCriticalSection acs(pAxis->pCs); 

		bool found = false;

		if (!Axis_IsMatrixViewLegend(pAxis)) {
			aVect_static_for(g_axisArray, i) {
				Axis_AssertIsNotMatrixViewLegend(g_axisArray[i], "Legend Axis not supposed to be referenced in g_axisArray");
				if (g_axisArray[i] == pAxis) {
					g_axisArray.Remove(i);
					found = true;
					break;
				}
			}
			if (!found) {
				OutputDebugStringA("Axis : Axis_Destroy : axis not found !\n");
			}
		}

		if (!((pAxis->options & AXIS_OPT_GHOST) && !(pAxis->options & AXIS_OPT_USERGHOST)) 
		&& pAxis->lockCount != 0) {
			if (Axis_HasMatrixViewLegend(pAxis)) {
				pAxis->pMatrixView->cMap->legendCmap->options |= AXIS_OPT_GHOST;
				Axis_Destroy(pAxis->pMatrixView->cMap->legendCmap);
				pAxis->pMatrixView->cMap->legendCmap = nullptr;
			}
			pAxis->options |= AXIS_OPT_ISORPHAN;
			if (pAxis->hWnd && pAxis->hdc) GDI_SAFE_CALL(ReleaseDC(pAxis->hWnd, pAxis->hdc));
			pAxis->hdc = 0;
		} else {
			acs.Leave();

			if (!acs.CheckReleased()) MY_ERROR("critical section not released before free()");

			Axis_DestroyCore(pAxis);
			AxisFree(pAxis);

			return true;
		}

		return (pAxis->options & AXIS_OPT_ISORPHAN) != 0;
	}

	MY_ERROR("This code is never reached, but the compiler complains that the function should return something");
	return *((bool*)nullptr);
}

bool Axis_IsBorderArea(AxisBorder * pBorder, POINT * pt) {

	return
		pt->x > pBorder->rect.left &&
		pt->x < pBorder->rect.right &&
		pt->y > pBorder->rect.top &&
		pt->y < pBorder->rect.bottom;
}

bool Axis_IsPlotArea(Axis * pAxis, POINT * pt, bool includeLegend = false) {

	if ((pAxis->options & AXIS_OPT_GHOST) || 
		(!includeLegend && (
			Axis_IsLegendArea(pAxis, pt) ||
			Axis_IsMatrixViewLegendArea(pAxis, pt)))) 
	{
		return false;
	}

	if (includeLegend &&
		(Axis_IsLegendArea(pAxis, pt) || Axis_IsMatrixViewLegendArea(pAxis, pt))) 
	{
		return true;
	}

	if (pt->x > pAxis->PlotArea.left &&  
		pt->x < pAxis->PlotArea.right &&  
		pt->y > pAxis->PlotArea.top &&  
		pt->y < pAxis->PlotArea.bottom) 
	{
		return true;
	}
	else {
		return false;
	}
}

bool Axis_IsDrawArea(Axis * pAxis, POINT * pt, bool includeLegend = 0) {

	if ((pAxis->options & AXIS_OPT_GHOST) || 
		(!includeLegend && Axis_IsLegendArea(pAxis, pt)))
	{
		return 0;
	}

	if (pt->x > pAxis->DrawArea_nPixels_dec_x	
	&& pt->x < pAxis->DrawArea_nPixels_dec_x + pAxis->DrawArea_nPixels_x
	&& pt->y > pAxis->DrawArea_nPixels_dec_y
	&& pt->y < pAxis->DrawArea_nPixels_dec_y + pAxis->DrawArea_nPixels_y) {
		return 1;
	}
	else {
		return 0;
	}
}


LRESULT CALLBACK GoodOlWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {

	aVectEx<wchar_t, g_AxisHeap> tmp;

	switch (message) {
		case AXIS_MSG_RCLICK : {
			tmp.sprintf(L"AXIS_MSG_RCLICK : %f, %f", *(double*)wParam, *(double*)lParam);
			MessageBoxW(hWnd, &tmp[0], Wide(__FUNCTION__), MB_ICONINFORMATION);
			break;
		}
		case AXIS_MSG_LCLICK : {
			tmp.sprintf(L"AXIS_MSG_LCLICK : %f, %f", *(double*)wParam, *(double*)lParam);
			MessageBoxW(hWnd, &tmp[0], Wide(__FUNCTION__), MB_ICONINFORMATION);
			break;
		}
		default: return DefWindowProcW(hWnd, message, wParam, lParam) ;
	}

	return 0;
}

void Axis_CloseWindow(HWND hWnd) {

	bool result;

	if (GetWindowThreadProcessId(hWnd, NULL) == GetCurrentThreadId()) {
		SendMessageW(hWnd, WM_CLOSE, NULL, NULL);
		result = true;
	} else {
		result = PostMessageW(hWnd, WM_CLOSE, NULL, NULL) != 0;
		do Sleep(1); while(GetWindowWord(hWnd, 0));
	}

	if (!result) {
		if (GetLastError() == ERROR_INVALID_WINDOW_HANDLE) {
			OutputDebugStringA("Axis : Axis_CloseWindow on unexistant window !\n");
			return;
		}
		else WIN32_SAFE_CALL(result);
	}
}

void Axis_CloseWindow(Axis * pAxis) {
	if (pAxis->options & AXIS_OPT_WINDOWED) Axis_CloseWindow(pAxis->hWnd);
	else Axis_Delete(pAxis);
}

void Axis_Delete(Axis* pAxis) {
	
	HWND hwndNew = pAxis->hWnd;

	if (!g_EndOfTimes) {
		Axis_DestroyAsync(pAxis);
	} else {
		Axis_Destroy(pAxis);
	}

	if (hwndNew) InvalidateRect(hwndNew, NULL, 1);
}

void Axis_MoveWindow(Axis * pAxis, double nx, double ny, double dx, double dy) {

	RECT rect;

	if (pAxis->options & AXIS_OPT_ADIM_MODE) {
		GetClientRect(Axis_GetParent(pAxis), &rect);
	} else {
		rect.right  = 1;
		rect.bottom = 1;
	}

	SetWindowPos(pAxis->hWnd, 
		NULL, 
		MyRound(dx * rect.right),
		MyRound(dy * rect.bottom), 
		MyRound(nx * rect.right),
		MyRound(ny * rect.bottom), 
		SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
}

void Axis_Move(Axis * pAxis, double nx, double ny, double dx, double dy) {

	RECT rect;

	if (pAxis->options & AXIS_OPT_ADIM_MODE) {
		GetClientRect(Axis_GetParent(pAxis), &rect);
	} else {
		rect.right  = 1;
		rect.bottom = 1;
	}

	if (pAxis->options & AXIS_OPT_WINDOWED) {
		Axis_MoveWindow(pAxis, nx, ny, dx, dy);
	} else {
		Axis_Repos(pAxis, (unsigned int)(dx * rect.right), (unsigned int)(dy * rect.bottom),
			(int)(nx * rect.right), (int)(ny * rect.bottom));
	}
}

template <
	double * AxisMatrixView::*coord_mPtr,
	size_t AxisMatrixView::*col_or_row_mPtr>
void Axis_SetMatrixViewCoord(
	Axis * pAxis, 
	const aVect<double> & coord) 
{

	if (!pAxis->pMatrixView) return;

	if (coord.Count() != pAxis->pMatrixView->*col_or_row_mPtr) {
		if (coord.Count() != pAxis->pMatrixView->*col_or_row_mPtr + 1) {
			MY_ERROR("wrong coordinate vector element count");
		}
	}

	xVect_Redim<false, false, AxisMalloc, AxisRealloc>(pAxis->pMatrixView->*coord_mPtr, coord.Count());
	memcpy(pAxis->pMatrixView->*coord_mPtr, coord, sizeof(double) * coord.Count());
}

void Axis_SetMatrixViewCoord_X(Axis * pAxis, const aVect<double> & coord_X) {
	
	auto matrixOrientation = pAxis->pMatrixView->options & AXISMATRIXVIEW_OPT_MATRIXORIENTATION;

	if (matrixOrientation) {
		Axis_SetMatrixViewCoord<&AxisMatrixView::xCoord, &AxisMatrixView::nCol>(pAxis, coord_X);
	} else {
		Axis_SetMatrixViewCoord<&AxisMatrixView::xCoord, &AxisMatrixView::nRow>(pAxis, coord_X);
	}
}

void Axis_SetMatrixViewCoord_Y(Axis * pAxis, const aVect<double> & coord_Y) {

	auto matrixOrientation = pAxis->pMatrixView->options & AXISMATRIXVIEW_OPT_MATRIXORIENTATION;

	if (matrixOrientation) {
		Axis_SetMatrixViewCoord<&AxisMatrixView::yCoord, &AxisMatrixView::nRow>(pAxis, coord_Y);
	}
	else {
		Axis_SetMatrixViewCoord<&AxisMatrixView::yCoord, &AxisMatrixView::nCol>(pAxis, coord_Y);
	}
}

double KeepSignificantDigits(double num, int nDigits = 8) {

	int numSign = sign(num);
	int mag = (int)pow(10, nDigits - (int)log10(abs(num)));
	double retVal = mag > 1 ? round(num * mag) / mag : num;
	return retVal;
}

void Axis_DrawGridAndTicksCore(
	AxisTicks * pTicks, 
	bool drawGrid  = true,
	bool drawTicks = true,
	bool computeTextExtent = false,
	double  firstTick  = 0, 
	double  deltaTicks = 0, 
	double  nPixels    = 0,
	int tick_nPixels   = 0,
	double  min = 0, 
	double  max = 0,
	bool minorTickRecursion = false)
{

	double x, y, tmp;
	wchar_t c[50];
	wchar_t * ptr1, * ptr2;
	SIZE sz, label_maxExtent;
	double tolRelMin, tolRelMax;
	double minorDeltaTicks, start, end;
	int count, orig_tick_nPixels;
	HPEN oldPen = NULL;
	bool isXaxis, isLog = false;

	static HPEN hPenGrid;

	Critical_Section(pTicks->pBorder->pAxis->pCs) {

		AxisBorder * pBorder = pTicks->pBorder;
		Axis * pAxis = pTicks->pBorder->pAxis;

		if (!hPenGrid) hPenGrid = CreatePen(PS_SOLID, 1, RGB(225, 225, 225));

		if (!minorTickRecursion) {

			firstTick  = pTicks->firstTick;
			deltaTicks = pTicks->deltaTicks;
			tick_nPixels = pTicks->tickHeight;

			if (pBorder->isVertical) {
				isXaxis = false;
				nPixels = pAxis->PlotArea.bottom - pAxis->PlotArea.top;
				min = pAxis->yMin;
				max = pAxis->yMax;
			}
			else {
				isXaxis = true;
				nPixels = pAxis->PlotArea.right - pAxis->PlotArea.left;
				min = pAxis->xMin;
				max = pAxis->xMax;
			}

			if ((isXaxis && (pAxis->options & AXIS_OPT_CUR_XLOG)) ||
				(!isXaxis && (pAxis->options & AXIS_OPT_CUR_YLOG)))
			{
				isLog = true;
				max = log10(max);
				min = log10(min);
			}
		}

		tolRelMin = 1 - 1e-15;
		tolRelMax = 1 + 1e-15;

		label_maxExtent.cx = 0;
		label_maxExtent.cy = 0;

		start = firstTick - deltaTicks;
		end   = max*tolRelMax;

		if (isLog) end = ceil(end);

		count = 0;
		orig_tick_nPixels = tick_nPixels;

		double previousTickText = std::numeric_limits<double>::quiet_NaN();
		double previousTick = previousTickText;

		if (start + 0.1*deltaTicks != start) {
			
			COLORREF oldColor;

			if (drawTicks && (pTicks->options & AXISTICKS_OPT_LABELS)) {
				COLORREF color = pBorder->hCurrentPen ? Axis_GetPenColor(pBorder->hCurrentPen) : color = RGB(0, 0, 0);
				oldColor = SetTextColor(pAxis->hdcDraw, color);
			}

			for (double currentTick_ = start ; 
				currentTick_ <= end ; 
				currentTick_ += deltaTicks) {

				if (count++ > 100000) break;

				double currentTick, logPreviousTick, logNextTick ;

				if (isLog) {
					currentTick_ = KeepSignificantDigits(currentTick_);
					int intPart = (int)floor(currentTick_);
					double decPart = currentTick_ - intPart;
					if (abs(decPart) < 1e-6) decPart = 0;
					currentTick = pow(10, intPart);
					if (decPart) currentTick = 10 * currentTick * decPart;
					currentTick = KeepSignificantDigits(currentTick);

					logPreviousTick = pow(10, floor(log10(currentTick)));
					logNextTick = 10 * logPreviousTick;

					if (!(log10(currentTick) >= tolRelMin * min || log10(currentTick) <= tolRelMax * max)) continue;

					if (std::isfinite(previousTick) && currentTick <= previousTick) continue;
					previousTick = currentTick;

				} else {
					currentTick = currentTick_;
				}

				if (drawTicks || computeTextExtent) {
					if (!minorTickRecursion) {
						if (drawTicks) {
							if (pTicks->options & AXISTICKS_OPT_MINOR_TICKS) {
								if (isLog) {
									if (deltaTicks == 1) {
										double nextTick = pow(10, currentTick_ + deltaTicks);
										auto tick_nPixels = MyRound(0.75*pTicks->minorTickHeight + 0.25*pTicks->tickHeight);

										for (int iPrec = 1, i = 2, iGrow = 1; i <= 10 && i * currentTick < nextTick;) {
											long y0, y1, x0, x1, delta, delta_max;

											if (pBorder->isVertical) {

												if (i * currentTick < pAxis->yMin || i * currentTick > pAxis->yMax) {
													delta = 0;
												}
												else {
													y0 = Axis_CoordTransf_Y(pAxis, iPrec * currentTick);
													y1 = Axis_CoordTransf_Y(pAxis, i * currentTick);
													x0 = Axis_CoordTransf_X(pAxis, pAxis->xMin);
													x1 = x0 + tick_nPixels;
													delta = y0 - y1;
													y0 = y1;
													delta_max = pTicks->maxLabelExtent.cy;
												}
											}
											else {
												if (i * currentTick < pAxis->xMin || i * currentTick > pAxis->xMax) {
													delta = 0;
												}
												else {
													x0 = Axis_CoordTransf_X(pAxis, iPrec * currentTick);
													x1 = Axis_CoordTransf_X(pAxis, i * currentTick);
													y0 = Axis_CoordTransf_Y(pAxis, pAxis->yMin);
													y1 = y0 - tick_nPixels;
													delta = x1 - x0;
													x0 = x1;
													delta_max = pTicks->maxLabelExtent.cx;
												}
											}

											if (delta) {

												if (pBorder->hCurrentPen) SAFE_CALL(oldPen = SelectPen(pAxis->hdcDraw, pBorder->hCurrentPen));

												MoveToEx(pAxis->hdcDraw, x0, y0, NULL);
												LineTo(pAxis->hdcDraw, x1, y1);

												if (pBorder->hCurrentPen) SAFE_CALL(SelectPen(pAxis->hdcDraw, oldPen));

												if (delta > delta_max) {
													iPrec = i;
												}
											}
											i += iGrow;
										}
									}
								}
								else {
									minorDeltaTicks = log10(deltaTicks) - log10(1.5);
									if (floor(minorDeltaTicks) == minorDeltaTicks) {
										minorDeltaTicks = .5 * pow(10, minorDeltaTicks);
									}
									else {
										minorDeltaTicks = log10(deltaTicks) - log10(2.5);
										if (floor(minorDeltaTicks) == minorDeltaTicks) {
											minorDeltaTicks = .5 * pow(10, minorDeltaTicks);
										}
										else {
											minorDeltaTicks = floor(log10(deltaTicks));
											minorDeltaTicks = pow(10, minorDeltaTicks);
										}
									}

									if (minorDeltaTicks == deltaTicks) minorDeltaTicks *= 0.1;

									int minor_nPix = MyRound((pBorder->isVertical ?
										pAxis->CoordTransf_dy : pAxis->CoordTransf_dx)
										* minorDeltaTicks);

									Axis_DrawGridAndTicksCore(pTicks, false, drawTicks, false, currentTick, minorDeltaTicks,
										minor_nPix, pTicks->minorTickHeight, currentTick, currentTick + deltaTicks, true);
								}
							}
						}
			
						if (!isLog) {
							//évite 1.4237e-17 à la place de 0
							tmp = 0.1 * (max - min) / nPixels;
							if (currentTick > -tmp && currentTick < tmp &&
								!(deltaTicks  > -tmp && deltaTicks  < tmp))
								currentTick = 0;
						}

						if (pTicks->options & AXISTICKS_OPT_LABELS && count > 1) {
							
							if (currentTick == 0) {
								swprintf(c, sizeof(c), L"0");
							}
							else if (abs(currentTick) < 1e6 && abs(currentTick) > 1e-3) {
								swprintf(c, sizeof(c), L"%.8f", currentTick);
							}
							else {
								swprintf(c, sizeof(c), L"%.10e", currentTick);
							}

							ptr1 = c;
							bool expFormat = false;
							bool hasComa = false;

							//enlève les '+' et les '0' après 'e'
							for (;;) {
								if (*ptr1 == L'.') hasComa = true;
								if (*(ptr1++) == L'e') {
									expFormat = true;
									ptr2 = ptr1;
									bool flag = 0;
									for (;;) {
										*(ptr1) = *(ptr2);
										if (*(ptr2) == 0) break;
										if (*(ptr2) != L'0' && *(ptr2) != L'+' || flag) {
											ptr1++;
											if (*(ptr2) != L'-') flag = 1;
										}
										ptr2++;
									}
									break;
								}
								else if (*(ptr1) == 0) break;
							}

							//ptr1--;
							if (hasComa) {
								for (;;) {
									if (ptr1 < c) break;
									if (!expFormat || *(--ptr1) == L'e') {
										ptr2 = ptr1;
										for (;;) {
											if (*(--ptr2) != L'0') {
												if (*ptr2 != L'.') ptr2++;
												if (ptr1 == ptr2) goto ouf;
												for (;;) {
													*ptr2 = *ptr1;
													if (*ptr1 == 0) {
														if (ptr2[-1] == '.') ptr2[-1] = 0;
														goto ouf;
													}
													ptr2++;
													ptr1++;
												}
											}
										}
									}
								}
							}
ouf:

							GetTextExtentPoint32W(pAxis->hdcDraw, c, (int)wcslen(c), &sz);
							if (sz.cx > label_maxExtent.cx) label_maxExtent.cx = sz.cx;
							if (sz.cy > label_maxExtent.cy) label_maxExtent.cy = sz.cy;
						}
					}

					if (minorTickRecursion && currentTick - max && IsEqualWithPrec((firstTick - max)/(currentTick - max), 2, 0.01))
						tick_nPixels = MyRound(0.75*pTicks->minorTickHeight + 0.25*pTicks->tickHeight);
					else tick_nPixels = orig_tick_nPixels;
				}
				
				if (pBorder->isVertical
				&& currentTick <= pAxis->yMax * tolRelMax 
				&& currentTick >= pAxis->yMin * tolRelMin) {

					if (drawTicks) {
						int x, y;
						Axis_CoordTransfDoubleToInt(pAxis, (LONG*)&x, (LONG*)&y, pAxis->xMin, currentTick);

						if (pBorder->hCurrentPen) SAFE_CALL(oldPen = SelectPen(pAxis->hdcDraw, pBorder->hCurrentPen));

						MoveToEx(pAxis->hdcDraw, x, y, NULL);
						LineTo(pAxis->hdcDraw, x + tick_nPixels, y);

						if (pBorder->hCurrentPen) SAFE_CALL(SelectPen(pAxis->hdcDraw, oldPen));
					}

					if (!minorTickRecursion) {

						if (drawGrid) {
							GDI_SAFE_CALL(oldPen = SelectPen(pAxis->hdcDraw, hPenGrid));
					
							Axis_MoveToEx(pAxis, pAxis->xMin, currentTick);
							Axis_LineTo(pAxis, pAxis->xMax, currentTick);

							GDI_SAFE_CALL(SelectPen(pAxis->hdcDraw, oldPen));
						}
						if (drawTicks && (pTicks->options & AXISTICKS_OPT_LABELS) && count > 1) {

							x = 0;
							y = currentTick;
		
							Axis_CoordTransf(pAxis, &x, &y);
		
							if (isLog && currentTick != logPreviousTick && currentTick != logNextTick) {
								GetTextExtentPoint32W(pAxis->hdcDraw, c, (int)wcslen(c), &sz);

								auto pixLogPreviousTick = Axis_CoordTransf_Y(pAxis, logPreviousTick);
								auto pixLogNextTick     = Axis_CoordTransf_Y(pAxis, logNextTick);
								auto pixCurrentTick     = Axis_CoordTransf_Y(pAxis, currentTick);

								if (std::isfinite(previousTickText)) {
									auto pixPreviousTick = Axis_CoordTransf_Y(pAxis, previousTickText);
									if (abs(pixPreviousTick - pixCurrentTick) < 0.85 * sz.cy) continue;
								}

								if (0.85 * sz.cy > abs(pixLogPreviousTick - pixCurrentTick)) continue;
								if (0.85 * sz.cy > abs(pixLogNextTick - pixCurrentTick)) continue;
							}

							GDI_SAFE_CALL(TextOutW(pAxis->hdcDraw, 
								(int)(pAxis->PlotArea.left - pAxis->DrawArea_nPixels_dec_x 
								- tick_nPixels - sz.cx), (int)(y - sz.cy/2), c, (int)wcslen(c))); 

							previousTickText = currentTick;
						}
					}
				}
				else if (!pBorder->isVertical
				&& currentTick <= pAxis->xMax * tolRelMax 
				&& currentTick >= pAxis->xMin * tolRelMin) {

					if (drawTicks) {
						int x, y;
						double yStart = pAxis->pMatrixView && (pAxis->pMatrixView->options & AXISMATRIXVIEW_OPT_MATRIXORIENTATION) ? pAxis->yMax : pAxis->yMin;
						Axis_CoordTransfDoubleToInt(pAxis, (LONG*)&x, (LONG*)&y, currentTick, yStart);

						if (pBorder->hCurrentPen) SAFE_CALL(oldPen = SelectPen(pAxis->hdcDraw, pBorder->hCurrentPen));

						MoveToEx(pAxis->hdcDraw, x, y, NULL);
						LineTo(pAxis->hdcDraw, x, y - tick_nPixels);

						if (pBorder->hCurrentPen) SAFE_CALL(SelectPen(pAxis->hdcDraw, oldPen));
					}

					if (!minorTickRecursion) {
						if (drawGrid) {
							GDI_SAFE_CALL(oldPen = SelectPen(pAxis->hdcDraw, hPenGrid));
						
							Axis_MoveToEx(pAxis, currentTick, pAxis->yMin);
							Axis_LineTo(pAxis, currentTick, pAxis->yMax);

							GDI_SAFE_CALL(SelectPen(pAxis->hdcDraw, oldPen));
						}

						if (drawTicks && (pTicks->options & AXISTICKS_OPT_LABELS) && count > 1) {
							x = currentTick;
							y = 0;
			
							Axis_CoordTransf(pAxis, &x, &y);
			
							if (isLog && currentTick != logPreviousTick && currentTick != logNextTick) {
								GetTextExtentPoint32W(pAxis->hdcDraw, c, (int)wcslen(c), &sz);

								auto pixLogPreviousTick = Axis_CoordTransf_X(pAxis, logPreviousTick);
								auto pixLogNextTick     = Axis_CoordTransf_X(pAxis, logNextTick);
								auto pixCurrentTick     = Axis_CoordTransf_X(pAxis, currentTick);

								if (std::isfinite(previousTickText)) {
									auto pixPreviousTick = Axis_CoordTransf_X(pAxis, previousTickText);
									if (pixPreviousTick + 1.5 * sz.cx > pixCurrentTick) continue;
								}

								if (1.75 * sz.cx > abs(pixLogPreviousTick - pixCurrentTick)) continue;
								if (1.75 * sz.cx > abs(pixLogNextTick - pixCurrentTick)) continue;
							}

							GDI_SAFE_CALL(TextOutW(pAxis->hdcDraw, 
								(int)(x - sz.cx/2), (int)(pAxis->PlotArea.bottom 
								- pAxis->DrawArea_nPixels_dec_y + tick_nPixels), 
								c, (int)wcslen(c))); 

							previousTickText = currentTick;
						}
					}
				}
			}
			if (computeTextExtent) {
				pTicks->maxLabelExtent = label_maxExtent;
			}

			if (drawTicks && (pTicks->options & AXISTICKS_OPT_LABELS)) {
				SetTextColor(pAxis->hdcDraw, oldColor);
			}
		}
	}
}

void AxisTicks_ComputeDeltaTicks(AxisTicks * pTicks) {
		
	double delta, nTicks, deltaTicks, log_deltaTicks; 
	double floor_log_deltaTicks, trunc_log_deltaTicks;
	double final_deltaTicks, min, max;
	double coordTransf;
	int nPixels, * maxLabelExtent;
	bool isXaxis, isLog = false;

	if (!pTicks) return;

	Critical_Section(pTicks->pBorder->pAxis->pCs) {

		AxisBorder * pBorder = pTicks->pBorder;
		Axis * pAxis         = pTicks->pBorder->pAxis;

		pTicks->deltaTicks_pix = pTicks->minDeltaTicks_pix;

		unsigned int tryCount = 0;

		for (;;) {

			if (tryCount++ > 500) break;

			if (pBorder->isVertical) {
				isXaxis = false;
				min = pAxis->yMin;
				max = pAxis->yMax;
				nPixels = pAxis->PlotArea.bottom - pAxis->PlotArea.top;
				maxLabelExtent = (int*)&pTicks->maxLabelExtent.cy;
				coordTransf = pAxis->CoordTransf_yFact;
			}
			else {
				isXaxis = true;
				min = pAxis->xMin;
				max = pAxis->xMax;
				nPixels = pAxis->PlotArea.right - pAxis->PlotArea.left;
				maxLabelExtent = (int*)&pTicks->maxLabelExtent.cx;
				coordTransf = pAxis->CoordTransf_xFact;
			}

			if (nPixels <= 1) break;

			if ((isXaxis && (pAxis->options & AXIS_OPT_CUR_XLOG)) ||
				(!isXaxis && (pAxis->options & AXIS_OPT_CUR_YLOG)))
			{
				isLog = true;
			}

			if (isLog) {
				if ((isXaxis && !(pAxis->options & AXIS_OPT_XLOG)) ||
					(!isXaxis && !(pAxis->options & AXIS_OPT_YLOG)))
				{
					if (isXaxis) Axis_SetCurrentLogScaleX(pAxis, false);
					else Axis_SetCurrentLogScaleY(pAxis, false);
					isLog = false;
				}
			}

			if (isLog) {
				max = log10(max);
				min = log10(min);
			}

			delta   = max - min;
			nTicks  = ((double)nPixels) / pTicks->deltaTicks_pix;

			if (isLog) {
				if (delta < 0.05)  {
					if (isXaxis) Axis_SetCurrentLogScaleX(pAxis, false);
					else Axis_SetCurrentLogScaleY(pAxis, false);
					AxisTicks_ComputeDeltaTicks(pTicks);
					return;
				} else if (delta < 1) {
					delta *= 0.5;
				}
			} else {
				if ((isXaxis && (pAxis->options & AXIS_OPT_XLOG)) ||
					(!isXaxis && (pAxis->options & AXIS_OPT_YLOG)))
				{
					if (log10(max) - log10(min) > 0.06) {
						if (isXaxis) Axis_SetCurrentLogScaleX(pAxis, true);
						else Axis_SetCurrentLogScaleY(pAxis, true);
						AxisTicks_ComputeDeltaTicks(pTicks);
						return;
					}
				}
			}

			if (nTicks > pTicks->maxTicks) 
				nTicks = pTicks->maxTicks;

			deltaTicks           = delta / nTicks;
			log_deltaTicks       = log10(deltaTicks);
			floor_log_deltaTicks = floor(log_deltaTicks);
			trunc_log_deltaTicks = log_deltaTicks - floor_log_deltaTicks;

			if (isLog) {
				final_deltaTicks = 10 * pow(10, floor_log_deltaTicks);

				if (final_deltaTicks >= 1) {
					final_deltaTicks = pow(10, log_deltaTicks + 0.875);
					final_deltaTicks = round(final_deltaTicks);
				}

				pTicks->firstTick = floor(min);

				pTicks->deltaTicks = final_deltaTicks;
				Axis_DrawGridAndTicksCore(pTicks, false, false, true);

				return;
			} else {
				if (trunc_log_deltaTicks < log10(1.5) && nTicks < 8) {
					final_deltaTicks = 1.5 * pow(10, floor_log_deltaTicks);
				} else if (trunc_log_deltaTicks < log10(2.0)) {
					final_deltaTicks = 2 * pow(10, floor_log_deltaTicks);
				} else if (trunc_log_deltaTicks < log10(2.5) && nTicks < 8) {
					final_deltaTicks = 2.5 * pow(10, floor_log_deltaTicks);
				} else if (trunc_log_deltaTicks < log10(3.0) && nTicks < 8) {
					final_deltaTicks = 3 * pow(10, floor_log_deltaTicks);
				} else if (trunc_log_deltaTicks < log10(4.0) && nTicks < 8) {
					final_deltaTicks = 4 * pow(10, floor_log_deltaTicks);
				} else if (trunc_log_deltaTicks < log10(5.0)) {
					final_deltaTicks = 5 * pow(10, floor_log_deltaTicks);
				} else {
					final_deltaTicks = 10 * pow(10, floor_log_deltaTicks);
				}

				pTicks->firstTick = ceil(min / final_deltaTicks) * final_deltaTicks;
				pTicks->deltaTicks = final_deltaTicks;

				Axis_DrawGridAndTicksCore(pTicks, false, false, true);
				if (*maxLabelExtent + pTicks->deltaLabel > MyRound(abs(pTicks->deltaTicks * coordTransf))) {
					pTicks->deltaTicks_pix += 1;
				} else {
					break;
				}
			}
		}
	}
}

AxisTicks * AxisBorder_CreateTicks(AxisBorder * pBorder) {

	AxisTicks * retVal;

	if (!pBorder) return NULL;

	if (!(retVal = (AxisTicks *)AxisMalloc(sizeof(AxisTicks)))) MY_ERROR("malloc = 0");

	*retVal = AxisTicks_Zero_Initializer;

	retVal->maxTicks = 15;
	retVal->minorTickHeight = 2;
	retVal->tickHeight = 5;
	retVal->minDeltaTicks_pix = 10;
	retVal->deltaLabel = 10;
	retVal->options = AXISTICKS_OPT_GRID | AXISTICKS_OPT_LABELS 
		| AXISTICKS_OPT_MINOR_TICKS ;
	retVal->pBorder = pBorder;

	return retVal;
}


void Axis_DrawGridAndTicks(Axis * pAxis, bool drawGrid, bool drawTicks, bool computeDeltaTicks) {

	HPEN oldPen;
	AxisBorder * pBorder;

	Critical_Section(pAxis->pCs) {

		double orig_yMin, orig_yMax;

		if (pAxis->options & AXIS_OPT_ISMATVIEWLEGEND && pAxis->pMatrixView) {
			orig_yMin = pAxis->yMin;
			orig_yMax = pAxis->yMax;
			Axis_Zoom(pAxis, pAxis->xMin, pAxis->xMax, pAxis->pMatrixView->cMap->vMin, pAxis->pMatrixView->cMap->vMax);
		}

		pBorder = pAxis->pX_Axis;

		if (Axis_IsValidPlotArea(pAxis)) {

			if (pAxis->tickLabelFont.hFont) {
				GDI_SAFE_CALL(SelectFont(pAxis->hdcDraw, pAxis->tickLabelFont.hFont));
				SetTextAlign(pAxis->hdcDraw, TA_LEFT | TA_TOP);
			}

			GDI_SAFE_CALL(oldPen = SelectPen(pAxis->hdcDraw, GetStockPen(BLACK_PEN)));

			if (computeDeltaTicks) {
				if (pAxis->pX_Axis) AxisTicks_ComputeDeltaTicks(pAxis->pX_Axis->pTicks);
				if (pAxis->pY_Axis) AxisTicks_ComputeDeltaTicks(pAxis->pY_Axis->pTicks);
			}

			if (drawGrid || drawTicks) {
				if (pAxis->pX_Axis && pAxis->pX_Axis->pTicks){
					Axis_DrawGridAndTicksCore(pAxis->pX_Axis->pTicks, 
						drawGrid && (pAxis->pX_Axis->pTicks->options & AXISTICKS_OPT_GRID),
						drawTicks);
				}
				if (pAxis->pY_Axis && pAxis->pY_Axis->pTicks) {
					Axis_DrawGridAndTicksCore(pAxis->pY_Axis->pTicks, 
						drawGrid && (pAxis->pY_Axis->pTicks->options & AXISTICKS_OPT_GRID), 
						drawTicks);
				}
			}
			GDI_SAFE_CALL(SelectPen(pAxis->hdcDraw, oldPen));
		}

		if (pAxis->options & AXIS_OPT_ISMATVIEWLEGEND && pAxis->pMatrixView) {
			Axis_Zoom(pAxis, pAxis->xMin, pAxis->xMax, orig_yMin, orig_yMax);
		}
	}
}

//Don't call when axis critical section is held !
void Axis_BringOnTop(Axis * pAxis) {

	if (!pAxis || !pAxis->hWnd) return;
	
	//may fail if user close the window at the same time
	//SAFE_CALL(SetWindowPos(pAxis->hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE));

	//enclose this in the axis critical section causes a deadlock
	SetWindowPos(pAxis->hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
}

Axis * Axis_Copy(Axis * pAxis) {

	Axis * newAxis = 
		Axis_Create((HWND)NULL, 
		pAxis->options & ~AXIS_OPT_WINDOWED, 
		L"Copy", WS_OVERLAPPEDWINDOW, false, pAxis);

	return newAxis;
}

HWND Axis_GetHwnd(Axis * pAxis) {
	return pAxis->hWnd;
}

size_t Axis_GetID(Axis * pAxis) {
	return pAxis->id;
}

void Axis_Export2Excel(Axis * pAxis, bool toClipboard, unsigned __int64 * pOnlyThisOne, bool coalesceAbscissa) {

	size_t maxN = 0;
	bool replacePointsByCommas = false;

	aVectEx<wchar_t, g_AxisHeap> buffer;
	aVectEx<Axis*, g_AxisHeap> allAxis;
	aVectEx<AxisSerie*, g_AxisHeap> allSeries;

	Critical_Section(pAxis->pCs) {
	
		allAxis.Push(pAxis);

		xVect_static_for(pAxis->subAxis, i) {
			allAxis.Push(pAxis->subAxis[i]);
		}

		for (auto&& a : allAxis) {
			xVect_static_for(a->pSeries, i) {

				AxisSerie * pSeries = &a->pSeries[i];

				if (pOnlyThisOne && *pOnlyThisOne != pSeries->id) continue;

				if (!pSeries->n) continue;

				allSeries.Push(pSeries);
			}
		}

		if (allSeries) {
			bool firstPass = true;

			aVect<bool> useLastAbscissa;
			auto I = allSeries.Count();
			useLastAbscissa.Redim(I, false);

			if (coalesceAbscissa) {
				size_t lastAbscissa = 0;
				for (size_t i = 1; i < I; ++i) {
					auto J = allSeries[lastAbscissa]->n;
					if (J != allSeries[i]->n) {
						lastAbscissa = i;
						continue;
					}
					auto dblPrec = allSeries[lastAbscissa]->doublePrec;
					if (dblPrec != allSeries[i]->doublePrec) {
						lastAbscissa = i;
						continue;
					}

					auto xDbl1 = allSeries[lastAbscissa]->xDouble;
					auto xSgl1 = allSeries[lastAbscissa]->xSingle;
					auto xDbl2 = allSeries[i]->xDouble;
					auto xSgl2 = allSeries[i]->xSingle;
					
					auto same = [J](auto&& x1, auto&& x2) {
						for (size_t j = 0; j < J; ++j) if (x1[j] != x2[j]) return false;
						return true;
					};

					if (dblPrec) {
						if (same(xDbl1, xDbl2)) useLastAbscissa[i] = true;
					} else {
						if (same(xSgl1, xSgl2)) useLastAbscissa[i] = true;
					}
				}
			}

			aVect_static_for(allSeries, i) {
				auto&& pSeries = allSeries[i];
				if (pSeries->n > maxN) maxN = pSeries->n;
				if (!firstPass) buffer.Append(L"\t");
				else firstPass = false;
				if (!useLastAbscissa[i]) buffer.Append(L"x\t");
				buffer.Append(L"%s", pSeries->pName);
			}
			buffer.Append(L"\n");

			wchar_t tmpBuf[100];

			bool firstPass_j = true;

			for (size_t j = 0; j < maxN; j++) {

				if (!firstPass_j) buffer.Append(L"\n");
				else firstPass_j = false;

				bool firstPass_i = true;

				aVect_static_for(allSeries, i) {

					auto&& pSeries = allSeries[i];
					if (!firstPass_i) buffer.Append(L"\t");

					if (!useLastAbscissa[i]) {
						if (j < pSeries->n) {
							swprintf_s(tmpBuf, L"%.18g", pSeries->doublePrec ? pSeries->xDouble[j] : pSeries->xSingle[j]);
							if (replacePointsByCommas) for (size_t n = wcslen(tmpBuf), k = 0; k < n; k++) if (tmpBuf[k] == L'.') tmpBuf[k] = L',';
							buffer.Append(L"%s", tmpBuf);
						}

						buffer.Append(L"\t");
					}

					if (firstPass_i) firstPass_i = false;

					if (j < pSeries->n) {
						swprintf_s(tmpBuf, L"%.18g", pSeries->doublePrec ? pSeries->yDouble[j] : pSeries->ySingle[j]);
						if (replacePointsByCommas) for (size_t n = wcslen(tmpBuf), k = 0; k < n; k++) if (tmpBuf[k] == L'.') tmpBuf[k] = L',';
						buffer.Append(L"%s", tmpBuf);
					}
				}
			}
		} else {
			if (auto pMatView = pAxis->pMatrixView) {

				auto matOrient = pMatView->options & AXISMATRIXVIEW_OPT_MATRIXORIENTATION;
				auto dblPrec   = pMatView->options & AXISMATRIXVIEW_OPT_DOUBLEPREC;
				auto I = pMatView->nRow;
				auto J = pMatView->nCol;
				auto mDouble = pMatView->mDouble;
				auto mSingle = pMatView->mSingle;
				auto coord_I = matOrient ? pMatView->yCoord : pMatView->xCoord;
				auto coord_J = matOrient ? pMatView->xCoord : pMatView->yCoord;

				if (I && J) {
					buffer.Append(L"X\\Y\t");

					for (size_t j = 0; ; ) {
						if (coord_J) buffer.Append(L"%.18g", coord_J[j]);
						else buffer.Append(L"%llu", (unsigned long long)j);
						if (++j >= J) break;
						buffer.Append(L"\t");
					}

					buffer.Append(L"\n");

					for (size_t i = 0; i<I; ++i) {
						if (coord_I) buffer.Append(L"%.18g", coord_I[i]);
						else buffer.Append(L"%llu", (unsigned long long)i);

						for (size_t j = 0; j<J; ++j) {
							auto ind = i*J + j;
							double value = dblPrec ? mDouble[ind] : mSingle[ind];
							buffer.Append(L"\t%.18g", value);
						}

						buffer.Append(L"\n");
					}
				}
			}
		}
	}

	if (!buffer) {
		buffer.Redim(1);
		buffer[0] = 0;
	}

	if (toClipboard) {

		OpenClipboard(NULL);
		
		HGLOBAL clipbuffer;
		wchar_t * buf;
		EmptyClipboard();
		clipbuffer = GlobalAlloc(GMEM_DDESHARE, buffer.Count() * sizeof(wchar_t));
		buf = (wchar_t*)GlobalLock(clipbuffer);
		wcscpy(buf, LPWSTR(&buffer[0]));
		GlobalUnlock(clipbuffer);
		SetClipboardData(CF_UNICODETEXT,clipbuffer);
		CloseClipboard();
	}
	else {

		wchar_t * tempDir = _wgetenv(L"TEMP");
	
		aVectEx<wchar_t, g_AxisHeap> fileName;

		fileName.sprintf(L"%s\\tempExcelFile.xls", tempDir);

		FILE * hFile = _wfopen(&fileName[0], L"w");

		if (hFile == NULL) {
			MessageBoxA(NULL, "Impossible d'écrire dans le fichier temporaire", "Erreur", MB_ICONEXCLAMATION);
			return;
		}

		fwprintf(hFile, L"%s", &buffer[0]);

		fclose(hFile);

		aVectEx<wchar_t, g_AxisHeap> buf;
	
		buf.sprintf(L"explorer %s", &fileName[0]);

		_wsystem(&buf[0]);
	}
}

void Axis_Export2Excel_old(Axis * pAxis, bool toClipboard) {

	size_t maxN = 0;
	aVectEx<wchar_t, g_AxisHeap> buffer;
	buffer.sprintf(L"");

	Critical_Section(pAxis->pCs) {

		bool firstPass = true;

		xVect_static_for(pAxis->pSeries, i) {
			AxisSerie * pSeries = &pAxis->pSeries[i];
			if (pSeries->n > maxN) maxN = pSeries->n;
			if (!firstPass) buffer.sprintf(L"%s\t", (wchar_t*)buffer);
			else firstPass = false;
			buffer.sprintf(L"%sx\t", (wchar_t*)buffer);
			buffer.sprintf(L"%s%s", (wchar_t*)buffer, pSeries->pName);
		}
		buffer.sprintf(L"%s\n", (wchar_t*)buffer);

		wchar_t tmpBuf[100];

		bool firstPass_j = true;

		for (unsigned int j=0 ; j<maxN ; j++) {
	
			if (!firstPass_j) buffer.sprintf(L"%s\n", (wchar_t*)buffer);
			else firstPass_j = false;

			bool firstPass_i = true;

			xVect_static_for(pAxis->pSeries, i) {
		
				AxisSerie * pSeries = &pAxis->pSeries[i];

				if (!firstPass_i) buffer.sprintf(L"%s\t", (wchar_t*)buffer);

				if (j < pSeries->n) {
					swprintf_s(tmpBuf, L"%.18g", pSeries->doublePrec ? pSeries->xDouble[j] : pSeries->xSingle[j]);
					for (size_t n=wcslen(tmpBuf), k=0; k<n ; k++) if (tmpBuf[k] == L'.') tmpBuf[k] = L',';
					buffer.sprintf(L"%s%s", (wchar_t*)buffer, tmpBuf);
				}
			
				buffer.sprintf(L"%s\t", (wchar_t*)buffer);

				if (firstPass_i) firstPass_i = false;

				if (j < pSeries->n) {
					swprintf_s(tmpBuf, L"%.18g", pSeries->doublePrec ? pSeries->yDouble[j] : pSeries->ySingle[j]);
					for (size_t n=wcslen(tmpBuf), k=0; k<n ; k++) if (tmpBuf[k] == L'.') tmpBuf[k] = L',';
					buffer.sprintf(L"%s%s", (wchar_t*)buffer, tmpBuf);
				}
			}
		}
	}

	if (toClipboard) {

		OpenClipboard(NULL);
		
		HGLOBAL clipbuffer;
		wchar_t * buf;
		EmptyClipboard();
		clipbuffer = GlobalAlloc(GMEM_DDESHARE, buffer.Count() * sizeof(wchar_t));
		buf = (wchar_t*)GlobalLock(clipbuffer);
		wcscpy(buf, LPWSTR(&buffer[0]));
		GlobalUnlock(clipbuffer);
		SetClipboardData(CF_UNICODETEXT,clipbuffer);
		CloseClipboard();
		
	}
	else {

		wchar_t * tempDir = _wgetenv(L"TEMP");
		aVectEx<wchar_t, g_AxisHeap> fileName;
		fileName.sprintf(L"%s\\tempExcelFile.xls", tempDir);
		FILE * hFile = _wfopen(&fileName[0], L"w");

		if (hFile == NULL) {
			MessageBoxA(NULL, "Impossible d'écrire dans le fichier temporaire", "Erreur", MB_ICONEXCLAMATION);
			return;
		}

		fwprintf(hFile, L"%s", &buffer[0]);
		fclose(hFile);
		aVectEx<wchar_t, g_AxisHeap> buf;
		buf.sprintf(L"explorer %s", (wchar_t*)fileName);
		_wsystem(&buf[0]);
	}
}

//vect must be increasing
template <bool nearest = true>
size_t FindIndex(double x, double * vect) {

	size_t I = xVect_Count(vect);

	if (I && x <= vect[0]) return 0;

	for (size_t i = 0, I = xVect_Count(vect); i < I - 1;++i) {
		if (x > vect[i] && x <= vect[i + 1]) {
			if (nearest) {
				if (abs(x - vect[i]) < abs(x - vect[i + 1])) {
					return i;
				} else {
					return i + 1;
				}
			} else { 
				return i;
			}
		}
	}

	return I - 1;
}

template <class T, bool matrixOrientation> 
void AxisMatrixView_ZoomFitCore(AxisMatrixView * pMatrixView) {

	Axis * pAxis = pMatrixView->pAxis;

	size_t I = pMatrixView->nRow;
	size_t J = pMatrixView->nCol;

	INT_PTR i1, i2, j1, j2;

	if (matrixOrientation) {
		if (pMatrixView->xCoord) {
			j1 = FindIndex<false>(pAxis->xMin, pMatrixView->xCoord);
			j2 = FindIndex<false>(pAxis->xMax, pMatrixView->xCoord);
		} else {
			j1 = (int)pAxis->xMin;
			j2 = (int)pAxis->xMax;
		}
		if (pMatrixView->yCoord) {
			i1 = FindIndex<false>(pAxis->yMin, pMatrixView->yCoord);
			i2 = FindIndex<false>(pAxis->yMax, pMatrixView->yCoord);
		} else {
			i1 = (int)pAxis->yMin;
			i2 = (int)pAxis->yMax;
		}
	}
	else {
		if (pMatrixView->xCoord) {
			i1 = FindIndex<false>(pAxis->xMin, pMatrixView->xCoord);
			i2 = FindIndex<false>(pAxis->xMax, pMatrixView->xCoord);
		} else {
			i1 = (int)pAxis->xMin;
			i2 = (int)pAxis->xMax;
		}
		if (pMatrixView->yCoord) {
			j1 = FindIndex<false>(pAxis->yMin, pMatrixView->yCoord);
			j2 = FindIndex<false>(pAxis->yMax, pMatrixView->yCoord);
		} else {
			j1 = (int)pAxis->yMin;
			j2 = (int)pAxis->yMax;
		}
	}

	if (i1 < 0)				i1 = 0;
	if ((size_t)i1 > I - 1) i1 = I - 1;
	if (j1 < 0)				j1 = 0;
	if ((size_t)j1 > J - 1) j1 = J - 1;

	T * ptr = (pMatrixView->options & AXISMATRIXVIEW_OPT_DOUBLEPREC) ? (T*)pMatrixView->mDouble : (T*)pMatrixView->mSingle;

	double vMax = -std::numeric_limits<double>::infinity();
	double vMin = -vMax;

	for (INT_PTR i = i1; i<i2; ++i) {
		for (INT_PTR j = j1; j<j2; ++j) {
			T val = ptr[i*J + j];
			if (std::isfinite(val)) {
				if (val > vMax) vMax = (double)val;
				if (val < vMin) vMin = (double)val;
			}
		}
	}

	if (std::isfinite(vMin) && std::isfinite(vMax)) {
		Axis_cLim(pAxis, vMin, vMax, true, true);
	}
}

void Axis_SquareZoom(Axis * pAxis) {

	double pixX = Axis_GetCoordPixelSizeX(pAxis, pAxis->xMin);
	double pixY = Axis_GetCoordPixelSizeY(pAxis, pAxis->yMin);

	if (IsEqualWithPrec(pixX, pixY, 1e-5)) return;

	if (pixX > pixY) {

		double factor = pixX / pixY;

		double yMin = pAxis->yMin;
		double yMax = pAxis->yMax;

		double L = (yMax - yMin) * (factor - 1);

		Axis_Zoom(pAxis, pAxis->xMin, pAxis->xMax, yMin - L / 2, yMax + L / 2);
	}
	else if (pixX < pixY) {

		double factor = pixY / pixX;

		double xMin = pAxis->xMin;
		double xMax = pAxis->xMax;

		double L = (xMax - xMin) * (factor - 1);

		Axis_Zoom(pAxis, xMin - L / 2, xMax + L / 2, pAxis->yMin, pAxis->yMax);
	}
}

void AxisMatrixView_ZoomFit(AxisMatrixView * pMatrixView) {

	auto&& dblPrec = pMatrixView->options & AXISMATRIXVIEW_OPT_DOUBLEPREC;
	auto&& matOrient = pMatrixView->options & AXISMATRIXVIEW_OPT_MATRIXORIENTATION;

	auto&& ZoomFitCore =
		dblPrec ?
			(matOrient ? AxisMatrixView_ZoomFitCore<double, true> : AxisMatrixView_ZoomFitCore<double, false>)
		:
			(matOrient ? AxisMatrixView_ZoomFitCore<float, true> : AxisMatrixView_ZoomFitCore<float, false>);

	ZoomFitCore(pMatrixView);
}

template <bool matrixOrientation> bool AxisMatrixView_IsPointInsideMatrix(AxisMatrixView * pMatView, double xPosDbl, double yPosDbl) {
	
	bool i_ok = false, j_ok = false;

	auto&& iCoord = matrixOrientation ? pMatView->yCoord : pMatView->xCoord;
	auto&& jCoord = matrixOrientation ? pMatView->xCoord : pMatView->yCoord;

	auto&& i = matrixOrientation ? yPosDbl : xPosDbl;
	auto&& j = matrixOrientation ? xPosDbl : yPosDbl;

	if (iCoord) {
		i_ok = i >= iCoord[0] && i <= xVect_Last(iCoord);
	} else {
		i_ok = (size_t)i < pMatView->nRow;
	}
	
	if (!i_ok) return false;

	if (jCoord) {
		j_ok = j >= jCoord[0] && j <= xVect_Last(jCoord);
	} else {
		j_ok = (size_t)j < pMatView->nCol;
	}

	if (j_ok) return true;
	else return false;
}

template <class T, bool matrixOrientation, bool nearest> 
T AxisMatrixView_GetValueFromPointCore(AxisMatrixView * pMatView, double xPosDbl, double yPosDbl) {

	auto&& iCoord = matrixOrientation ? pMatView->yCoord : pMatView->xCoord;
	auto&& jCoord = matrixOrientation ? pMatView->xCoord : pMatView->yCoord;

	auto&& v_i = matrixOrientation ? yPosDbl : xPosDbl;
	auto&& v_j = matrixOrientation ? xPosDbl : yPosDbl;

	size_t i, j;

	if (iCoord) {
		i = FindIndex<nearest>(v_i, iCoord);

		//AxisMatrixView_Draw3Corei += !matrixOrientation;

		if (i >= xVect_Count(iCoord)) MY_ERROR("gotcha");
	} else {
		i = (size_t)v_i;
	}

	if (jCoord) {
		j = FindIndex<nearest>(v_j, jCoord);

		if (j >= xVect_Count(jCoord)) MY_ERROR("gotcha");
	} else {
		j = (size_t)v_j;
	}

	

	size_t ind = i * pMatView->nCol + j;
	return std::is_same<T, double>::value ? (T)pMatView->mDouble[ind] : (T)pMatView->mSingle[ind];
}

template <bool nearest>
double AxisMatrixView_GetValueFromPoint(AxisMatrixView * pMatView, double xPosDbl, double yPosDbl) {

	if (pMatView->options & AXISMATRIXVIEW_OPT_DOUBLEPREC)
		if (pMatView->options & AXISMATRIXVIEW_OPT_MATRIXORIENTATION)
			return AxisMatrixView_GetValueFromPointCore<double, true, nearest>(pMatView, xPosDbl, yPosDbl);
		else
			return AxisMatrixView_GetValueFromPointCore<double, false, nearest>(pMatView, xPosDbl, yPosDbl);
	else
		if (pMatView->options & AXISMATRIXVIEW_OPT_MATRIXORIENTATION)
			return AxisMatrixView_GetValueFromPointCore<float, true, nearest>(pMatView, xPosDbl, yPosDbl);
		else
			return AxisMatrixView_GetValueFromPointCore<float, false, nearest>(pMatView, xPosDbl, yPosDbl);
}

bool AxisMatrixView_IsMatrixVisible(AxisMatrixView * pMatView) {
	
	Axis * pAxis = pMatView->pAxis;

	auto&& IsPointInsideMatrix = pMatView->options & AXISMATRIXVIEW_OPT_MATRIXORIENTATION ?
		AxisMatrixView_IsPointInsideMatrix<true> : AxisMatrixView_IsPointInsideMatrix<false>;

	return 
		IsPointInsideMatrix(pMatView, pAxis->xMin, pAxis->yMin) ||
		IsPointInsideMatrix(pMatView, pAxis->xMin, pAxis->yMax) ||
		IsPointInsideMatrix(pMatView, pAxis->xMax, pAxis->yMin) ||
		IsPointInsideMatrix(pMatView, pAxis->xMax, pAxis->yMax);
}

struct AxisZoomCopy {
	bool initialized;
	double xMin;
	double yMin;
	double xMax;
	double yMax;
} g_axisZoomBackup;

aVect<AxisSerie*> g_seriesCopy;

void Axis_FreeSeriesCopy() {
	for (auto&& serie : g_seriesCopy) {
		AxisSeries_Destroy(serie);
	}
	g_seriesCopy.Redim<false>(0);
}

void AxisPopupMenu_SetLim(
	Axis * pAxis, 
	double Axis::*mPtr, 
	void (*func)(Axis*, double, bool), 
	const wchar_t * question, 
	const wchar_t * title,
	bool isSubAxis) 
{

	auto xMin = pAxis->*mPtr;
	auto buffer = xFormat(L"%g", xMin);
	buffer = InputBox(title, question, buffer);

	if (buffer) {
		if (IsNumber(buffer)) {
			func(pAxis, _wtof(buffer), true);
			if (!isSubAxis) Axis_RefreshAsync(pAxis);
		}
		else {
			MessageBoxA(NULL, xFormat("\"%s\" is not recognized as a number", buffer), "Invalid input", MB_ICONERROR);
		}
	}

}

void Axis_FitToLegendOnly(Axis * axis) {

	if (!axis->legend) return;

	auto&& r = axis->legend->rect;
	auto nx = r.right - r.left;
	auto ny = r.bottom - r.top;

	RECT rect;

	GetWindowRect(axis->hWnd, &rect);
	auto as = GetNonClientAdditionalSize(axis->hWnd);

	auto mode = Axis_GetAdimMode(axis);
	Axis_SetAdimMode(axis, false);
	Axis_Move(axis, nx + as.cx, ny + as.cy, rect.left, rect.top);
	Axis_SetAdimMode(axis, mode);

	axis->legend->adim_left = 0;
	axis->legend->adim_top = 0;
}

void Axis_SaveImageToFile(Axis * axis, const wchar_t * filePath, bool onlyLegend) {

	if (!axis->hBitmapDraw) return;

	if (onlyLegend && !axis->legend) return;

	auto oldText = axis->pStatusBar ? axis->pStatusBar->pText : nullptr;

	if (oldText) {
		AxisBorder_Draw(axis->pStatusBar, false);
		Axis_DrawBoundingRects(axis, false, true);
	}

	RECT orig_rect; 
	double orig_adim_left, orig_adim_top;

	if (onlyLegend) {
		orig_adim_left = axis->legend->adim_left;
		orig_adim_top = axis->legend->adim_top;
		GetWindowRect(axis->hWnd, &orig_rect);
		Axis_FitToLegendOnly(axis);
		Axis_Refresh(axis);
	}
	
	auto pbi = CreateBitmapInfoStruct(axis->hBitmapDraw);
	CreateBMPFile(filePath, pbi, axis->hBitmapDraw, axis->hdcDraw);
	LocalFree(pbi);

	if (onlyLegend) {
		axis->legend->adim_left = orig_adim_left;
		axis->legend->adim_top  = orig_adim_top;
		auto mode = Axis_GetAdimMode(axis);
		Axis_SetAdimMode(axis, false);
		Axis_Move(axis, orig_rect.right - orig_rect.left, orig_rect.bottom - orig_rect.top, orig_rect.left, orig_rect.top);
		Axis_SetAdimMode(axis, mode);
		Axis_Refresh(axis);
	}

	if (oldText) {
		AxisBorder_Draw(axis->pStatusBar);
		Axis_DrawBoundingRects(axis, false, true);
	}
}

void Axis_UpdateSubAxisLegend(Axis * pAxis) {

	//when subAxis exist, id is the same for the real and the fake serie (fake serie = empty serie only for legend)
	xVect_static_for(pAxis->subAxis, i) {
		auto pSubAxis = pAxis->subAxis[i];
		xVect_static_for(pSubAxis->pSeries, j) {
			xVect_static_for(pAxis->pSeries, k) {
				if (pSubAxis->pSeries[j].id == pAxis->pSeries[k].id) {
					AxisSeries_Destroy(&pAxis->pSeries[k]);
					pAxis->pSeries[k] = AxisSerie_CopyCore<double>(&pSubAxis->pSeries[j], false);
					pAxis->pSeries[k].pAxis = pAxis;
					pAxis->pSeries[k].iSerie = k;
					pAxis->pSeries[k].id = pSubAxis->pSeries[j].id;
				}
			}
		}
	}
}

void Axis_PopupMenu(Axis * pAxis, POINT * pt) {

	enum  {	ZOOM_OUT = 1, DUPLICATE_AXIS, COPY_TO_METAFILE, AUTOFIT, LEGEND, EXPORT2EXCEL, CLOSE, 
			COPY_TO_CLIPBOARD, CMAP_GRAYSCALE, CMAP_JET, CMAP_HSV, CMAP_JET2, CMAP_HOT, CMAP_COSMOS, 
			OPT_INTERPOL, CMAP_SETMIN, CMAP_SETMAX, CMAP_ZOOMFIT, CMAP_RESETZOOM, SERIE_REMOVE, 
			ZOOM_COPY, ZOOM_PASTE, SERIE_COPY, SERIES_COPY, SERIE_ADD_COPY, SERIES_PASTE, SERIE_COPY_DATA,
		    SERIE_SET_COLOR, SERIE_SET_LINEWIDTH, SERIE_SET_LINESTYLE_SOLID, SERIE_SET_LINESTYLE_DOT,
		    SERIE_SET_LINESTYLE_DASH, SERIE_SET_LINESTYLE_DASHDOT, SERIE_SET_LINESTYLE_DASHDOTDOT, 
			MATRIX_ORIENT, PLAN_ORIENT, XLOG, YLOG, XLIN, YLIN, XMIN, XMAX, YMIN, YMAX, SAVEBMP,
		    SUBAXIS_ID // Must be last
	};

	Critical_Section(pAxis->pCs) {

		bool wasOrphan = (pAxis->options & AXIS_OPT_ISORPHAN) != 0;
		if (wasOrphan) {
			DbgStr(L" [0x%p] AXIS_OPT_ISORPHAN entering Axis_PopupMenu (id = %d)?!\n", GetCurrentThreadId(), pAxis->id);
			if (IsDebuggerPresent()) {
				__debugbreak();
			}
		}

		Axis_Lock(pAxis);

		DWORD hasData = 0;
		DWORD hasCMap = 0;
		DWORD hasMatView = 0;

		HMENU hMenu = CreatePopupMenu();
		HMENU hCMapMenu = NULL;
		HMENU hMatOrientMenu = NULL; 
		HMENU hXaxisMenu = NULL;
		HMENU hYaxisMenu = NULL;
		HMENU hSerieMenu = NULL;
		HMENU hLineStyle = NULL;

		if (!(pAxis->pSeries) && !(pAxis->pMatrixView)) hasData = MF_GRAYED;
		if (!(pAxis->pMatrixView) || (pAxis->pMatrixView->options & AXIS_CMAP_COLORREF)) hasCMap = MF_GRAYED;
		if (!pAxis->pMatrixView) hasMatView = MF_GRAYED;

		double xPosDbl = pt->x;
		double yPosDbl = pt->y;
		Axis_GetCoord(pAxis, &xPosDbl, &yPosDbl);

		bool isXaxis = Axis_IsBorderArea(pAxis->pX_Axis, pt);
		bool isYaxis = Axis_IsBorderArea(pAxis->pY_Axis, pt);

		aVectEx<wchar_t, g_AxisHeap> serieTxt;
		AxisSerie * pHooveredSerie = Axis_GetSerieFromPixel(pAxis, pt->x, pt->y);
		
		DWORD isPointInsideMatrix = 0;
		DWORD isMatrixVisible = 0;
		DWORD isCMap_Hot = 0;
		DWORD isCMap_HSV = 0;
		DWORD isCMap_Jet2 = 0;
		DWORD isCMap_COSMOS = 0;
		DWORD isCMap_Jet = 0;
		DWORD isCMap_GrayScale = 0;
		DWORD isXlog = 0;
		DWORD isYlog = 0;
		DWORD isMatrixOrient = 0;

		if (pAxis->pMatrixView) {
			if (!AxisMatrixView_IsMatrixVisible(pAxis->pMatrixView)) isMatrixVisible = MF_GRAYED;
			if (pAxis->pMatrixView->options & AXISMATRIXVIEW_OPT_MATRIXORIENTATION) {
				if (!AxisMatrixView_IsPointInsideMatrix<true>(pAxis->pMatrixView, xPosDbl, yPosDbl))
					isPointInsideMatrix = MF_GRAYED;
			} else {
				if (!AxisMatrixView_IsPointInsideMatrix<false>(pAxis->pMatrixView, xPosDbl, yPosDbl)) 
					isPointInsideMatrix = MF_GRAYED;
			}
			if (pAxis->pMatrixView->options & AXIS_CMAP_HOT)		isCMap_Hot		 = MF_CHECKED;
			if (pAxis->pMatrixView->options & AXIS_CMAP_GRAYSCALE)	isCMap_GrayScale = MF_CHECKED;
			if (pAxis->pMatrixView->options & AXIS_CMAP_HSV)		isCMap_HSV		 = MF_CHECKED;
			if (pAxis->pMatrixView->options & AXIS_CMAP_JET2)		isCMap_Jet2		 = MF_CHECKED;
			if (pAxis->pMatrixView->options & AXIS_CMAP_COSMOS)		isCMap_COSMOS	 = MF_CHECKED;
			if (pAxis->pMatrixView->options & AXIS_CMAP_JET)		isCMap_Jet		 = MF_CHECKED;
			if (pAxis->pMatrixView->options & AXISMATRIXVIEW_OPT_MATRIXORIENTATION)		isMatrixOrient = MF_CHECKED;
		}

		if (pAxis->options & AXIS_OPT_XLOG)		isXlog = MF_CHECKED;
		if (pAxis->options & AXIS_OPT_YLOG)		isYlog = MF_CHECKED;

		AppendMenuA(hMenu, MF_STRING, ZOOM_OUT,   "Reset zoom");
		AppendMenuA(hMenu, MF_STRING, ZOOM_COPY,  "Copy  zoom");

		if (g_axisZoomBackup.initialized) {
			AppendMenuA(hMenu, MF_STRING, ZOOM_PASTE, "Paste zoom");
		}

		AppendMenuA(hMenu, MF_STRING | hasData
			| (pAxis->options & AXIS_OPT_LEGEND) ? MF_CHECKED : MF_UNCHECKED, LEGEND,
			"Show legend");

		AppendMenuA(hMenu, MF_STRING | hasData, AUTOFIT, "Auto fit");

		AppendMenuA(hMenu, MF_SEPARATOR, NULL, NULL);

		if (hasCMap != MF_GRAYED) {
			hCMapMenu = CreatePopupMenu();
			AppendMenuA(hMenu, MF_POPUP | hasCMap, (UINT_PTR)hCMapMenu, "ColorMap");
			AppendMenuA(hCMapMenu, MF_STRING | isCMap_Jet, CMAP_JET, "Jet");
			AppendMenuA(hCMapMenu, MF_STRING | isCMap_Jet2, CMAP_JET2, "Jet2");
			AppendMenuA(hCMapMenu, MF_STRING | isCMap_COSMOS, CMAP_COSMOS, "COSMOS");
			AppendMenuA(hCMapMenu, MF_STRING | isCMap_HSV, CMAP_HSV, "HSV");
			AppendMenuA(hCMapMenu, MF_STRING | isCMap_Hot, CMAP_HOT, "Hot");
			AppendMenuA(hCMapMenu, MF_STRING | isCMap_GrayScale, CMAP_GRAYSCALE, "GrayScale");
			AppendMenuA(hCMapMenu, MF_SEPARATOR, NULL, NULL);
			AppendMenuA(hCMapMenu, MF_STRING | (pAxis->pMatrixView->options & AXISMATRIXVIEW_OPT_INTERPOL) ? MF_CHECKED : MF_UNCHECKED, OPT_INTERPOL, "Interpolate");
			AppendMenuA(hCMapMenu, MF_SEPARATOR, NULL, NULL);
			AppendMenuA(hCMapMenu, MF_STRING /*| isPointInsideMatrix*/, CMAP_SETMIN, "Set min");
			AppendMenuA(hCMapMenu, MF_STRING /*| isPointInsideMatrix*/, CMAP_SETMAX, "Set max");
			AppendMenuA(hCMapMenu, MF_STRING | isMatrixVisible, CMAP_ZOOMFIT, "Visible fit");
			AppendMenuA(hCMapMenu, MF_STRING, CMAP_RESETZOOM, "Reset");
		}

		if (hasMatView != MF_GRAYED) {
			hMatOrientMenu = CreatePopupMenu();
			AppendMenuA(hMenu, MF_POPUP | hasMatView, (UINT_PTR)hMatOrientMenu, "Orientation");
			AppendMenuA(hMatOrientMenu, MF_STRING | isMatrixOrient, MATRIX_ORIENT, "Matrix");
			AppendMenuA(hMatOrientMenu, MF_STRING | (isMatrixOrient ? 0 : MF_CHECKED), PLAN_ORIENT, "Plan");
		}

		if (hasCMap != MF_GRAYED || hasMatView != MF_GRAYED) {
			AppendMenuA(hMenu, MF_SEPARATOR, NULL, NULL);
		}

		aVectEx<HMENU, g_AxisHeap> hYaxisMenus;
		
		hXaxisMenu = CreatePopupMenu();
		hYaxisMenu = CreatePopupMenu();
			
		AppendMenuA(hMenu, MF_POPUP, (UINT_PTR)hXaxisMenu, "x axis");
		AppendMenuA(hMenu, MF_POPUP, (UINT_PTR)hYaxisMenu, "y axis");
		AppendMenuA(hXaxisMenu, MF_STRING, XMIN, "Set min");
		AppendMenuA(hXaxisMenu, MF_STRING, XMAX, "Set max");
		AppendMenuA(hXaxisMenu, MF_STRING | (isXlog ? 0 : MF_CHECKED), XLIN, "Linear");
		AppendMenuA(hXaxisMenu, MF_STRING | isXlog, XLOG, "Log");

		if (xVect_Count(pAxis->subAxis)) {

			hYaxisMenus.Push(CreatePopupMenu());

			AppendMenuW(hYaxisMenu, MF_POPUP, (UINT_PTR)hYaxisMenus.Last(), xFormat(L"Axis 1%s", pAxis->pY_Axis->pText ? xFormat(L" \"%s\"", pAxis->pY_Axis->pText) : L""));
			AppendMenuA(hYaxisMenus.Last(), MF_STRING, SUBAXIS_ID + 0, "Set min");
			AppendMenuA(hYaxisMenus.Last(), MF_STRING, SUBAXIS_ID + 1, "Set max");

			xVect_static_for(pAxis->subAxis, i) {
				auto pSubAxis = pAxis->subAxis[i];
				hYaxisMenus.Push(CreatePopupMenu());
				AppendMenuW(hYaxisMenu, MF_POPUP, (UINT_PTR)hYaxisMenus.Last(), xFormat(L"Axis %d%s", i+2, pSubAxis->pY_Axis->pText ? xFormat(L" \"%s\"", pSubAxis->pY_Axis->pText) : L""));
				AppendMenuA(hYaxisMenus.Last(), MF_STRING, SUBAXIS_ID + 2*(i+1) + 0, "Set min");
				AppendMenuA(hYaxisMenus.Last(), MF_STRING, SUBAXIS_ID + 2*(i+1) + 1, "Set max");
			}
		} else {
			AppendMenuA(hYaxisMenu, MF_STRING, YMIN, "Set min");
			AppendMenuA(hYaxisMenu, MF_STRING, YMAX, "Set max");
			AppendMenuA(hYaxisMenu, MF_STRING | (isYlog ? 0 : MF_CHECKED), YLIN, "Linear");
			AppendMenuA(hYaxisMenu, MF_STRING | isYlog, YLOG, "Log");
		}
			

		AppendMenuA(hMenu, MF_SEPARATOR, NULL, NULL);


		AppendMenuA(hMenu, MF_STRING, DUPLICATE_AXIS, "Duplicate");
		AppendMenuA(hMenu, MF_STRING, COPY_TO_METAFILE, "Copy image");
		AppendMenuA(hMenu, MF_STRING | hasData, COPY_TO_CLIPBOARD, "Copy data");
		AppendMenuA(hMenu, MF_STRING | hasData, EXPORT2EXCEL, "Export to Excel");
		AppendMenuA(hMenu, MF_STRING, SAVEBMP, "Save as bmp file");

		AppendMenuA(hMenu, MF_SEPARATOR, NULL, NULL);

		AppendMenuA(hMenu, MF_STRING, CLOSE, "Close");

		AppendMenuA(hMenu, MF_SEPARATOR, NULL, NULL);

		if (pHooveredSerie) {

			serieTxt.sprintf(L"Serie n°%llu, \"%s\"", pHooveredSerie->id, pHooveredSerie->pName);
			if (serieTxt.Count() > 50) {
				serieTxt.Redim(50).Last() = 0;
				serieTxt.Last(-1) = L'"';
				serieTxt.Last(-2) = L'.';
				serieTxt.Last(-3) = L'.';
				serieTxt.Last(-4) = L'.';
			}

			hSerieMenu = CreatePopupMenu();

			AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSerieMenu, serieTxt);

			AppendMenuA(hSerieMenu, MF_STRING, SERIE_REMOVE, "Remove");
			AppendMenuA(hSerieMenu, MF_STRING, SERIE_COPY_DATA, "Copy data");
			AppendMenuA(hSerieMenu, MF_STRING, SERIE_COPY, "Copy");
			AppendMenuA(hSerieMenu, MF_STRING, SERIE_ADD_COPY, "Add to copies");
			AppendMenuA(hSerieMenu, MF_SEPARATOR, NULL, NULL);
			AppendMenuA(hSerieMenu, MF_STRING, SERIE_SET_COLOR, "Color");
			AppendMenuA(hSerieMenu, MF_STRING, SERIE_SET_LINEWIDTH, "Line width");

			hLineStyle = CreatePopupMenu();

			auto style = pHooveredSerie->pen.lopnStyle;

			AppendMenuA(hSerieMenu, MF_POPUP,  (UINT_PTR)hLineStyle, "Line style");
			AppendMenuA(hLineStyle, MF_STRING | (style == PS_SOLID      ? MF_CHECKED : 0), SERIE_SET_LINESTYLE_SOLID,   "Solid");
			AppendMenuA(hLineStyle, MF_STRING | (style == PS_DASH       ? MF_CHECKED : 0), SERIE_SET_LINESTYLE_DASH,    "Dash");
			AppendMenuA(hLineStyle, MF_STRING | (style == PS_DOT        ? MF_CHECKED : 0), SERIE_SET_LINESTYLE_DOT,     "Dot");
			AppendMenuA(hLineStyle, MF_STRING | (style == PS_DASHDOT    ? MF_CHECKED : 0), SERIE_SET_LINESTYLE_DASHDOT, "Dash-dot");
			AppendMenuA(hLineStyle, MF_STRING | (style == PS_DASHDOTDOT ? MF_CHECKED : 0), SERIE_SET_LINESTYLE_DASHDOTDOT,    "Dash-dot-dot");
		}

		AppendMenuA(hMenu, MF_STRING, SERIES_COPY, "Copy all series");

		if (g_seriesCopy) {
			if (g_seriesCopy.Count() == 1) AppendMenuA(hMenu, MF_STRING, SERIES_PASTE, "Paste serie");
			else AppendMenuA(hMenu, MF_STRING, SERIES_PASTE, "Paste series");
		}

		ClientToScreen(pAxis->hWnd, pt);

		BOOL val = TrackPopupMenu(hMenu,
			TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD,
			pt->x, pt->y, 0, pAxis->hWnd, NULL);

		switch (val) {
			case XLOG: {
				Axis_SetLogScaleX(pAxis, true);
				Axis_RefreshAsync(pAxis);
				break;
			}
			case YLOG: {
				Axis_SetLogScaleY(pAxis, true);
				Axis_RefreshAsync(pAxis);
				break;
			}
			case XLIN: {
				Axis_SetLogScaleX(pAxis, false);
				Axis_RefreshAsync(pAxis);
				break;
			}
			case YLIN: {
				Axis_SetLogScaleY(pAxis, false);
				Axis_RefreshAsync(pAxis);
				break;
			}
			case XMIN: {
				AxisPopupMenu_SetLim(pAxis, &Axis::xMin, Axis_xMin, L"Minimum value ?", L"x axis", false);
				break;
			}
			case XMAX: {
				AxisPopupMenu_SetLim(pAxis, &Axis::xMax, Axis_xMax, L"Maximum value ?", L"x axis", false);
				break;
			}
			case YMIN: {
				AxisPopupMenu_SetLim(pAxis, &Axis::yMin, Axis_yMin, L"Minimum value ?", L"y axis", false);
				break;
			}
			case YMAX: {
				AxisPopupMenu_SetLim(pAxis, &Axis::yMax, Axis_yMax, L"Maximum value ?", L"y axis", false);
				break;
			}
			case MATRIX_ORIENT: {
				Axis_SetMatrixViewOrientation(pAxis, true);
				Axis_RefreshAsync(pAxis);
				break;
			}
			case PLAN_ORIENT: {
				Axis_SetMatrixViewOrientation(pAxis, false);
				Axis_RefreshAsync(pAxis);
				break;
			}
			case ZOOM_OUT: {
				Axis_ResetZoom(pAxis);
				Axis_RefreshAsync(pAxis);
				break;
			}
			case ZOOM_COPY: {
				g_axisZoomBackup.initialized = true;
				g_axisZoomBackup.xMin = pAxis->xMin;
				g_axisZoomBackup.xMax = pAxis->xMax;
				g_axisZoomBackup.yMin = pAxis->yMin;
				g_axisZoomBackup.yMax = pAxis->yMax;
				break;
			}			
			case ZOOM_PASTE: {
				pAxis->xMin = g_axisZoomBackup.xMin;
				pAxis->xMax = g_axisZoomBackup.xMax;
				pAxis->yMin = g_axisZoomBackup.yMin;
				pAxis->yMax = g_axisZoomBackup.yMax;
				Axis_RefreshAsync(pAxis);
				break;
			}
			case COPY_TO_METAFILE: {
				Axis_CopyMetaFileToClipBoard(pAxis);
				break;
			}
			case DUPLICATE_AXIS: {
				Axis * newAxis = Axis_Copy(pAxis);
				Axis_RefreshAsync(newAxis);
				Axis_Release(newAxis);
				break;
			}
			case LEGEND: {
				if (pAxis->options & AXIS_OPT_LEGEND) pAxis->options &= ~AXIS_OPT_LEGEND;
				else pAxis->options |= AXIS_OPT_LEGEND;
				Axis_RefreshAsync(pAxis);
				break;
			}
			case AUTOFIT: {
				Axis_AutoFit(pAxis, true, true);
				Axis_RefreshAsync(pAxis);
				break;
			}
			case EXPORT2EXCEL: {
				Axis_Export2Excel(pAxis, false);
				break;
			}
			case SAVEBMP: {

				if (auto filePath = FileSaveDialog(L"*.bmp", L"bitmap files")) {

					wchar_t * ext;

					SplitPathW(filePath, nullptr, nullptr, nullptr, &ext);

					if (!ext) filePath.Append(L".bmp");

					if (filePath && IsFileWritable(filePath)) {
						Axis_SaveImageToFile(pAxis, filePath, false);
					}
				}

				break;
			}
			case COPY_TO_CLIPBOARD: {
				Axis_Export2Excel(pAxis, true);
				break;
			}
			case CLOSE: {
				Axis_CloseWindow(pAxis);
				break;
			}
			case CMAP_GRAYSCALE: {
				Axis_SetMatrixViewColorMap(pAxis, AXIS_CMAP_GRAYSCALE);
				Axis_RefreshAsync(pAxis);
				break;
			}
			case CMAP_JET: {
				Axis_SetMatrixViewColorMap(pAxis, AXIS_CMAP_JET);
				Axis_RefreshAsync(pAxis);
				break;
			}
			case CMAP_JET2: {
				Axis_SetMatrixViewColorMap(pAxis, AXIS_CMAP_JET2);
				Axis_RefreshAsync(pAxis);
				break;
			}
			case CMAP_HSV: {
				Axis_SetMatrixViewColorMap(pAxis, AXIS_CMAP_HSV);
				Axis_RefreshAsync(pAxis);
				break;
			}
			case CMAP_HOT: {
				Axis_SetMatrixViewColorMap(pAxis, AXIS_CMAP_HOT);
				Axis_RefreshAsync(pAxis);
				break;
			}
			case CMAP_COSMOS: {
				Axis_SetMatrixViewColorMap(pAxis, AXIS_CMAP_COSMOS);
				Axis_RefreshAsync(pAxis);
				break;
			}
			case OPT_INTERPOL: {
				DWORD opt = pAxis->pMatrixView->options;
				if (opt & AXISMATRIXVIEW_OPT_INTERPOL) {
					SET_DWORD_BYTE_MASK(opt, AXISMATRIXVIEW_OPT_INTERPOL, false);
				}
				else SET_DWORD_BYTE_MASK(opt, AXISMATRIXVIEW_OPT_INTERPOL, true);

				AxisMatrixView_CreateColorMap(pAxis->pMatrixView, opt);
				Axis_RefreshAsync(pAxis);
				break;
			}
			case CMAP_RESETZOOM: {
				Axis_SetMatrixViewColorMap(pAxis, pAxis->pMatrixView->cMap->vMin_orig, pAxis->pMatrixView->cMap->vMax_orig);
				Axis_RefreshAsync(pAxis);
				break;
			}
			case CMAP_ZOOMFIT: {
				AxisMatrixView_ZoomFit(pAxis->pMatrixView);
				Axis_RefreshAsync(pAxis);
				break;
			}
			case CMAP_SETMIN: {
				
				if (!pAxis->pMatrixView->cMap)  break;

				aVect<wchar_t> buffer;

				if (isPointInsideMatrix != MF_GRAYED) {
					double value = AxisMatrixView_GetValueFromPoint<false>(pAxis->pMatrixView, xPosDbl, yPosDbl);
					buffer.Format(L"%g", value);
				}

				buffer = InputBox(L"Color Map", L"Minimum value ?", buffer);

				if (buffer) {
					double value = _wtof(buffer);

					Axis_cLim(pAxis, value, pAxis->pMatrixView->cMap->vMax, false, true);
					Axis_RefreshAsync(pAxis);
				}

				break;
			}
			case CMAP_SETMAX: {

				if (!pAxis->pMatrixView->cMap)  break;

				aVect<wchar_t> buffer;

				if (isPointInsideMatrix != MF_GRAYED) {
					double value = AxisMatrixView_GetValueFromPoint<false>(pAxis->pMatrixView, xPosDbl, yPosDbl);
					buffer.Format(L"%g", value);
				}

				buffer = InputBox(L"Color Map", L"Maximum value ?", buffer);
				
				if (buffer) {
					double value = _wtof(buffer);

					Axis_cLim(pAxis, pAxis->pMatrixView->cMap->vMin, value, false, true);
					Axis_RefreshAsync(pAxis);
				}

				break;
			}
			case SERIE_REMOVE: {
				
				if (pHooveredSerie) {
					Axis_RemoveSerie(pAxis, pHooveredSerie);
				}
				
				Axis_CreateLegend(pAxis, NULL);
				Axis_RefreshAsync(pAxis);
				break;
			}
			case SERIE_COPY: {
				if (pHooveredSerie) {
					Axis_FreeSeriesCopy();
					g_seriesCopy.Push(AxisSerie_Copy(pHooveredSerie));
				}
				break;
			}
			case SERIE_ADD_COPY: {
				if (pHooveredSerie) {
					g_seriesCopy.Push(AxisSerie_Copy(pHooveredSerie));
				}
				break;
			}
			case SERIE_SET_COLOR : {

				if (pHooveredSerie) {

					CHOOSECOLORA clr = { 0 };
						
					static COLORREF customColors[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

					clr.lStructSize = sizeof(clr);
					clr.Flags = CC_ANYCOLOR | CC_FULLOPEN | CC_RGBINIT;
					clr.rgbResult = pHooveredSerie->pen.lopnColor;
					clr.lpCustColors = customColors;

					if (ChooseColorA(&clr)) {
						pHooveredSerie->pen.lopnColor = clr.rgbResult;

						Axis_UpdateSubAxisLegend(pAxis);
						Axis_CreateLegend(pAxis, NULL);
						Axis_RefreshAsync(pAxis);
					}
				}

				break;
			}
			case SERIE_SET_LINEWIDTH: {

				if (pHooveredSerie) {
					if (auto ans = InputBox(L"Set line width", L"Line width?", xFormat(L"%d", pHooveredSerie->pen.lopnWidth.x))) {
						if (IsNumber(ans)) {
							pHooveredSerie->pen.lopnWidth.x = _wtoi(ans);

							Axis_UpdateSubAxisLegend(pAxis);
							Axis_CreateLegend(pAxis, NULL);
							Axis_RefreshAsync(pAxis);
						}
					}
				}

				break;
			}
			case SERIE_SET_LINESTYLE_SOLID: {
				if (pHooveredSerie) {
					pHooveredSerie->pen.lopnStyle = PS_SOLID;
					Axis_UpdateSubAxisLegend(pAxis);
					Axis_CreateLegend(pAxis, NULL);
					Axis_RefreshAsync(pAxis);
				}
				break;
			}
			case SERIE_SET_LINESTYLE_DOT: {
				if (pHooveredSerie) {
					pHooveredSerie->pen.lopnStyle = PS_DOT;
					Axis_UpdateSubAxisLegend(pAxis);
					Axis_CreateLegend(pAxis, NULL);
					Axis_RefreshAsync(pAxis);
				}
				break;
			}
			case SERIE_SET_LINESTYLE_DASH: {
				if (pHooveredSerie) {
					pHooveredSerie->pen.lopnStyle = PS_DASH;
					Axis_UpdateSubAxisLegend(pAxis);
					Axis_CreateLegend(pAxis, NULL);
					Axis_RefreshAsync(pAxis);
				}
				break;
			}
			case SERIE_SET_LINESTYLE_DASHDOT: {
				if (pHooveredSerie) {
					pHooveredSerie->pen.lopnStyle = PS_DASHDOT;
					Axis_UpdateSubAxisLegend(pAxis);
					Axis_CreateLegend(pAxis, NULL);
					Axis_RefreshAsync(pAxis);
				}
				break;
			}
			case SERIE_SET_LINESTYLE_DASHDOTDOT: {
				if (pHooveredSerie) {
					pHooveredSerie->pen.lopnStyle = PS_DASHDOTDOT;
					Axis_UpdateSubAxisLegend(pAxis);
					Axis_CreateLegend(pAxis, NULL);
					Axis_RefreshAsync(pAxis);
				}
				break;
			}
			case SERIES_COPY: {
				Axis_FreeSeriesCopy();
				xVect_static_for(pAxis->pSeries, i) {
					g_seriesCopy.Push(AxisSerie_Copy(&pAxis->pSeries[i]));
				}
				break;
			}
			case SERIES_PASTE: {
				for (auto pSerie : g_seriesCopy) {

					if (pSerie->pMarker) {
						Axis_AddSerie_Raw(pAxis, pSerie->pName, pSerie->xDouble, pSerie->yDouble,
							pSerie->n, pSerie->pen.lopnColor, pSerie->pen.lopnStyle, 
							(unsigned short)pSerie->pen.lopnWidth.x,
							pSerie->pMarker->markerType, pSerie->pMarker->color, pSerie->pMarker->nPixel,
							pSerie->pMarker->lineWidth); 
					}
					else {
						Axis_AddSerie_Raw(pAxis, pSerie->pName, pSerie->xDouble, pSerie->yDouble,
							pSerie->n, pSerie->pen.lopnColor, pSerie->pen.lopnStyle, 
							(unsigned short)pSerie->pen.lopnWidth.x);
					}
				}

				Axis_UpdateSubAxisLegend(pAxis);
				Axis_CreateLegend(pAxis, NULL);
				Axis_RefreshAsync(pAxis);
				break;
			}
			case SERIE_COPY_DATA: {

				AxisSerie * pSerie = nullptr;

				xVect_static_for(pAxis->pSeries, i) {
					if (pAxis->pSeries[i].id == pHooveredSerie->id) {
						pSerie = &pAxis->pSeries[i];
						break;
					}
				}

				if (!pSerie) {
					xVect_static_for(pAxis->subAxis, k) {
						auto pSubAxis = pAxis->subAxis[k];
						xVect_static_for(pSubAxis->pSeries, i) {
							if (pSubAxis->pSeries[i].id == pHooveredSerie->id) {
								pSerie = &pSubAxis->pSeries[i];
								break;
							}
						}
					}
				}

				Axis_Export2Excel(pAxis, true, &pSerie->id);

				break;
			}
			default: {
				//subaxis min/max 
				if (val >= SUBAXIS_ID) {
					val -= SUBAXIS_ID;
					auto iAxis = val / 2;
					auto min = val % 2 == 0;

					Axis * selAxis = nullptr;

					if (iAxis == 0) {
						selAxis = pAxis;
					}
					else {
						selAxis = pAxis->subAxis[iAxis - 1];
					}

					if (min) {
						AxisPopupMenu_SetLim(selAxis, &Axis::yMin, Axis_yMin, L"Minimum value ?", L"y axis", true);
					}
					else {
						AxisPopupMenu_SetLim(selAxis, &Axis::yMax, Axis_yMax, L"Maximum value ?", L"y axis", true);
					}

					Axis_Refresh(pAxis);
				}
			}
		}

		DestroyMenu(hMenu);
		if (hCMapMenu)      DestroyMenu(hCMapMenu);
		if (hMatOrientMenu) DestroyMenu(hMatOrientMenu);
		if (hXaxisMenu)		DestroyMenu(hXaxisMenu);
		if (hYaxisMenu)		DestroyMenu(hYaxisMenu);
		if (hSerieMenu)		DestroyMenu(hSerieMenu);
		if (hLineStyle)		DestroyMenu(hLineStyle);

		Axis_Unlock(pAxis);

		if (pAxis->options & AXIS_OPT_ISORPHAN && !wasOrphan) {
			DbgStr(L" [0x%p] axis (id = %d) became AXIS_OPT_ISORPHAN during Axis_popupmenu ! Destroying axis...\n", GetCurrentThreadId(), pAxis->id);
			Axis_Destroy(pAxis);
		}
	}
}

BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam) {
	aVect<HWND> * hWndArray = (aVect<HWND>*)lParam;
	hWndArray->Push(hwnd);
	return true;
}

LRESULT CALLBACK AxisMgrWinProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	
	aVectEx<Axis*, g_AxisHeap> pAxisToFree;

	switch (message) {

		case WM_DESTROY: {

			PostQuitMessage(true);
			return 0;
		}
		case WM_CLOSE: {

			HANDLE g_hEventEndOfTimes = (HANDLE)wParam;

			Axis_CloseAllWindows(lParam != 0);

			DbgStr(L" [0x%p] AxisMgrWinProc : WM_CLOSE\n", GetCurrentThreadId());

			//DbgStr(" [0x%p] AxisMgrWinProc : WM_CLOSE : Destroy Axis Heap\n", GetCurrentThreadId());

			//if (g_AxisHeap) WIN32_SAFE_CALL(HeapDestroy(g_AxisHeap));
			//g_Axis_init = false;
			//g_AxisHeap = NULL;
			//g_axisArray.Leak();
			
			WIN32_SAFE_CALL(DestroyWindow(hWnd));

			//Not there actually.
			//Now it is set just before Axis Mgr Thread return;
			//if (g_hEventEndOfTimes) EVENT_SAFE_CALL(SetEvent(g_hEventEndOfTimes));

			break;
		}
		case AXIS_MSG_CREATE_WINDOW: {
			
#ifdef DEBUG_THREADS
			OutputDebugString("** Worker Thread : Creating Window...\n");
#endif
			AxisWindowParam * CW_params;

			CW_params = (AxisWindowParam *) wParam;

			Axis * pAxis = Axis_CreateAt(CW_params->nX, CW_params->nY, CW_params->x, CW_params->y, 
				CW_params->parent, CW_params->options, CW_params->strTitle, CW_params->style, false, CW_params->copyFrom);
			
			CW_params->result_hWnd = pAxis->hWnd;
				
			EVENT_SAFE_CALL(SetEvent(CW_params->hEventWindowCreated));
			return 0;
		}
		case AXIS_MSG_REFRESH: {
			return 0;
		}
		case AXIS_MSG_RELEASE: {
			Axis * wParamAxis = (Axis *)wParam;

			Axis_Unlock(wParamAxis);

			if (wParamAxis->options & AXIS_OPT_ISORPHAN) { 
				if (wParamAxis->lockCount == 0) {
				
					Axis_DestroyCore(wParamAxis);
					AxisFree(wParamAxis);
				}
				else {
					pAxisToFree.Push(wParamAxis);
					SetTimer(hWnd, 1, 1000, NULL);
				}
			}
			return 0;
		}
		case WM_TIMER: {
			if (wParam == 1) {
				aVect_for_inv(pAxisToFree, i) {
					if (pAxisToFree[i]->lockCount == 0) {

						Axis_DestroyCore(pAxisToFree[i]);
						AxisFree(pAxisToFree[i]);
						pAxisToFree.Remove(i);
					}
				}
				if (pAxisToFree.Count() == 0) {
					KillTimer(hWnd, 1);
				}
				return 0;
			}
			break;
		}
	}
	
	return DefWindowProcW(hWnd, message, wParam, lParam);
}; 

CRITICAL_SECTION * Axis_GetCriticalSection(Axis * pAxis) {
	return pAxis->pCs;
}

bool Axis_GetUserStatusText(Axis * pAxis, wchar_t * buf, size_t bufSize, AxisSerie * pSerie, double x, double y) {

	if (!pAxis->statusTextCallback) return false;

	void * callbacksData = pAxis->callbacksData ? pAxis->callbacksData[0] : nullptr;

	return pAxis->statusTextCallback(
		buf,
		bufSize,
		pSerie ? pSerie->iSerie : -1,
		pSerie ? pSerie->pName : nullptr,
		x, y,
		callbacksData);
}

struct AxisLockGuard {

	Axis * pAxis;

	AxisLockGuard(Axis * pAxis) {
		this->pAxis = pAxis;
		pAxis->lockCount++;
	}

	~AxisLockGuard() {
		this->pAxis->lockCount--;
	}
};

LRESULT CALLBACK AxisWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {

    PAINTSTRUCT ps;

	Axis * pAxis = nullptr;
	aVectEx<wchar_t, g_AxisHeap> tmp;
	bool wm_move = false;
	
	if (!g_Axis_init) {
		Axis_Init();
	}
	
	bool found = false;

	switch (message) {
		//case AXIS_MSG_RCLICK : {
		//	tmp = new_xAsprintf("AXIS_MSG_RCLICK : %f, %f", *(double*)wParam, *(double*)lParam);
		//	MessageBox(hWnd, tmp, __FUNCTION__, MB_ICONINFORMATION);
		//	break;
		//}
		//case AXIS_MSG_LCLICK : {
		//	tmp = new_xAsprintf("AXIS_MSG_LCLICK : %f, %f", *(double*)wParam, *(double*)lParam);
		//	MessageBox(hWnd, tmp, __FUNCTION__, MB_ICONINFORMATION);
		//	break;
		//}
		case WM_DESTROY: {
			
			DbgStr(L" [0x%p] AxisWinProc : WM_DESTROY\n", GetCurrentThreadId());

			if (g_terminating) break;

			static WinCriticalSection cs;

			Critical_Section(g_axisArray.CriticalSection()) {// moved here because of rare conditions when crit sect are aquired in the other order
				Critical_Section(cs) {
					static aVect<Axis*> axisArray;
					axisArray.Redim(0);

					//Critical_Section(g_axisArray.CriticalSection()) {

					aVect_for_inv(g_axisArray, i) {
						pAxis = g_axisArray[i];
						Axis_AssertIsNotMatrixViewLegend(pAxis, "Legend Axis not supposed to be referenced in g_axisArray");
						if (pAxis->hWnd == hWnd) {
							axisArray.Push(pAxis);
						}
					}
					//}

					aVect_for_inv(axisArray, i) {
						DbgStr(L" [0x%p] Axis_Destroy(pAxis = 0x%p)\n", GetCurrentThreadId(), axisArray[i]);
						Axis_Destroy(axisArray[i]);
					}
				}
			}

			break;
		}
		case WM_ERASEBKGND: {

			return true;

		}
		case WM_CREATE: {
			
			DbgStr(L" [0x%p] AxisWindowProc : WM_CREATE\n", GetCurrentThreadId());

			return DefWindowProcW(hWnd, message, wParam, lParam) ;
		}
		case WM_CLOSE: {
			bool hideWindow = 0;
			Axis * pAxis;

			aVectEx<HWND, g_AxisHeap> hWndArray;

			hWndArray.Push(hWnd);

			EnumChildWindows(hWnd, EnumChildProc, (LPARAM)&hWndArray);

			Critical_Section(g_axisArray.CriticalSection()) {
				aVect_for_inv(hWndArray, i) {
					aVect_for_inv(g_axisArray, j) {
						pAxis = g_axisArray[j];
						Axis_AssertIsNotMatrixViewLegend(pAxis, "Legend Axis not supposed to be referenced in g_axisArray");
						if (pAxis->hWnd == hWndArray[i]) {
							Axis_Destroy(pAxis);
							//pAxis->hWnd = NULL; !! NoNoNo, here pAxis may be invalid, setting destroyed hWnd to NULL would be nice but this would require proper treatment in Axis_Destroy
						}
					}
				}
			}

			if (hideWindow) {
				SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_HIDEWINDOW);
				return 0;
			}
			else {
				wchar_t tmpBuf[500];
				GetWindowTextW(hWnd, tmpBuf, NUMEL(tmpBuf));
				DbgStr(L" [0x%p] AxisWinProc : WM_CLOSE \"%s\" => DefWindowProc\n", 
					GetCurrentThreadId(), tmpBuf);
				 
				//pAxis->hWnd = NULL; !! NoNoNo, here pAxis may be invalid, setting destroyed hWnd to NULL would be nice but this would require proper treatment in Axis_Destroy
				return DefWindowProcW(hWnd, message, wParam, lParam);
			}
		}
		case WM_PAINT: {

			BeginPaint(hWnd, &ps);
			HDC mainHdc = ps.hdc;
			Axis * pMainHdcAxis = nullptr;

			Critical_Section(g_axisArray.CriticalSection()) {

				int nMainHdc = 0;

				aVect_static_for(g_axisArray, i) {
					pAxis = g_axisArray[i];
					Axis_AssertIsNotMatrixViewLegend(pAxis, "Legend Axis not supposed to be referenced in g_axisArray");
					
					if (pAxis->hWnd == hWnd && pAxis->options & AXIS_OPT_MAINHDC) {
						if (!pAxis->hdc) MY_ERROR("Axis with option AXIS_OPT_MAINHDC must have hdc != 0");
						if (nMainHdc++) MY_ERROR("Only 1 Axis with option AXIS_OPT_MAINHDC is allowed per window");
						mainHdc = pAxis->hdcDraw;
						pMainHdcAxis = pAxis;
						RECT r;
						GetClientRect(hWnd, &r);

						HBRUSH oldBrush = SelectBrush(mainHdc, GetStockBrush(GRAY_BRUSH));
						Rectangle(mainHdc, r.left, r.top, r.right, r.bottom);
						SelectBrush(mainHdc, oldBrush);
					}
				}

				aVect_static_for(g_axisArray, i) {
					pAxis = g_axisArray[i];
					Axis_AssertIsNotMatrixViewLegend(pAxis, "Legend Axis not supposed to be referenced in g_axisArray");
					if (pAxis->hWnd == hWnd) {
						if (pAxis->options & AXIS_OPT_MAINHDC) {

						}
						else {
							Axis_Redraw(pAxis, mainHdc);
						}
					}
				}

				if (pMainHdcAxis) Axis_Redraw(pMainHdcAxis, ps.hdc);

				EndPaint(hWnd, &ps);
			}

			break;
		}

		case WM_MOUSEACTIVATE:
		case WM_ACTIVATE:
		case WM_CHILDACTIVATE:
			SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			break;

		case WM_SIZE:

			/* Fall through */

		case WM_MOVE :

			wm_move = true;

			/* Fall through */

		case WM_WINDOWPOSCHANGED : {
			
			static int recursionLevel = 0;

			Critical_Section(g_axisArray.CriticalSection()) {
			
		
				size_t count;
				AXIS_SAFE_ACCESS(count = g_axisArray.Count());

				//static WinCriticalSection cs;
				//Critical_Section(cs) { 
					
					//static aVect<HWND> hWndArray; !!NONONONO!! must be reetrant
					aVectEx<HWND, g_AxisHeap> hWndArray;

					hWndArray.Redim(0);
					hWndArray.Push(hWnd);
					EnumChildWindows(hWnd, EnumChildProc, (LPARAM)&hWndArray);

					recursionLevel++;

					aVect_for(g_axisArray, i) {
						Axis * pAxis = g_axisArray[i];
						Critical_Section(pAxis->pCs) {
							Axis_AssertIsNotMatrixViewLegend(pAxis, "Legend Axis not supposed to be referenced in g_axisArray");
							aVect_for(hWndArray, j){
								if (pAxis->hWnd == hWndArray[j]) {
									if (j == 0) {
										if (!wm_move) Axis_Resize(pAxis);
										if ((pAxis->options & AXIS_OPT_WINDOWED) &&
											(pAxis->options & AXIS_OPT_ADIM_MODE) &&
											recursionLevel == 1) {
											Axis_UpdateAdimPos(pAxis);
										}
									}
									else if (!wm_move) {
										Axis_Resize(pAxis);
										if (pAxis->options & AXIS_OPT_WINDOWED &&
											pAxis->options & AXIS_OPT_ADIM_MODE) {
											Axis_AdimResizeWindow(pAxis);
										}
									}
								}
							}
						}
					}
				//}
			}
			
			recursionLevel--;

			return 0;
			//break;
		}
		case WM_RBUTTONDOWN : {
			POINT pt;
			RECT rect;
			bool found = false;

			AutoCriticalSection acs;

			pt.x = GET_X_LPARAM(lParam); 
			pt.y = GET_Y_LPARAM(lParam);

			bool opt_adim_mode_before;

			Critical_Section(g_axisArray.CriticalSection()) {
				aVect_static_for(g_axisArray, i) {
					pAxis = g_axisArray[i];
					acs.Enter(pAxis->pCs);
					Axis_AssertIsNotMatrixViewLegend(pAxis, "Legend Axis not supposed to be referenced in g_axisArray");
					if (pAxis->hWnd == hWnd && Axis_IsDrawArea(pAxis, &pt)) {
						found = true;
						break;
					}
					acs.Leave();
				}

				//}   must not release g_axisArray CS because we hold axis CS, and may try to reacquire g_axisArray CS in Axis_GetUserRectSel message pump => potential deadlock

				if (!found) break;

				if (pAxis->pMatrixView && pAxis->pMatrixView->cMap && pAxis->pMatrixView->cMap->legendCmap) {
					Axis_Refresh(pAxis->pMatrixView->cMap->legendCmap, false, false, false);
				}

				opt_adim_mode_before = (pAxis->options & AXIS_OPT_ADIM_MODE) != 0;

				Axis_GetUserRectSel(pAxis, &pt, &rect, WM_RBUTTONUP);
				//} Apparently, SendMessage can end up processing other messages => must not release g_axisArray CS here neither

				if (rect.left == 0 && rect.right == 0 && rect.top == 0 && rect.bottom == 0) {
					double xPosDbl = pt.x;
					double yPosDbl = pt.y;
					Axis_GetCoord(pAxis, &xPosDbl, &yPosDbl);

					AxisMsg_Point ptDbl = { xPosDbl , yPosDbl };

					if (!SendMessageW(GetParent(hWnd), AXIS_MSG_RCLICK, (WPARAM)pAxis, (LPARAM)&ptDbl)) {
						if (!(pAxis->options & AXIS_OPT_DISABLED)) Axis_PopupMenu(pAxis, &pt);
					}
					break;
				}
			}

			if (pAxis->options & AXIS_OPT_DISABLED) break;

			if (pAxis->drawCallback || pAxis->pSeries || pAxis->pMatrixView) {
				
				if (rect.right  == rect.left) rect.right++;
				if (rect.bottom == rect.top)  rect.bottom++;

				double x1 = Axis_GetCoordX(pAxis, rect.left);
				double x2 = Axis_GetCoordX(pAxis, rect.right);
				double y1 = Axis_GetCoordY(pAxis, rect.bottom);
				double y2 = Axis_GetCoordY(pAxis, rect.top);

				Axis_Zoom(pAxis, x1, x2, y1, y2); 

				xVect_static_for(pAxis->subAxis, i) {
					auto pSubAxis = pAxis->subAxis[i];
					x1 = Axis_GetCoordX(pSubAxis, rect.left);
					x2 = Axis_GetCoordX(pSubAxis, rect.right);
					y1 = Axis_GetCoordY(pSubAxis, rect.bottom);
					y2 = Axis_GetCoordY(pSubAxis, rect.top);

					Axis_Zoom(pSubAxis, x1, x2, y1, y2);
				}
			}
			
			Axis_Refresh(pAxis);

			bool opt_adim_mode_after = (pAxis->options & AXIS_OPT_ADIM_MODE) != 0;
			if (opt_adim_mode_before != opt_adim_mode_after) {
				DbgStr(L" [0x%p] Axis n°%d adim mode option changed from %d to %d !!\n", 
					GetCurrentThreadId(),
					pAxis->id, opt_adim_mode_before, opt_adim_mode_after);
			}

			break;
		}
		case WM_MOUSEMOVE: {

			bool found = false;
			POINT pt;

			double xPosDbl; 
			double yPosDbl;
			AutoCriticalSection acs;
			
			xPosDbl = pt.x = GET_X_LPARAM(lParam); 
			yPosDbl = pt.y = GET_Y_LPARAM(lParam); 

			Critical_Section(g_axisArray.CriticalSection()) {

				aVect_static_for(g_axisArray, i) {
					pAxis = g_axisArray[i];
					//Critical_Section(pAxis->pCs) {
					acs.Enter(pAxis->pCs);
						Axis_AssertIsNotMatrixViewLegend(pAxis, "Legend Axis not supposed to be referenced in g_axisArray");
						if (pAxis->hWnd == hWnd && Axis_IsPlotArea(pAxis, &pt)) {
							found = true;
							break;
						}
					//}
					acs.Leave();
				}
			}

			if (!found || (pAxis->options & AXIS_OPT_DISABLED)) break;

			AxisLockGuard guard(pAxis);

			wchar_t tmp[500];

			Axis_GetCoord(pAxis, &xPosDbl, &yPosDbl);

			AxisSerie * pHooveredSerie = Axis_GetSerieFromPixel(pAxis, pt.x, pt.y);

			AxisSerieMarker oldMarker;
			AxisSerieMarker * pOldMarker = NULL;

			if (pHooveredSerie) {

				xPosDbl = pt.x;
				yPosDbl = pt.y;

				auto pSerieAxis = pHooveredSerie->pAxis;

				Axis_GetCoord(pSerieAxis, &xPosDbl, &yPosDbl);

				if (!Axis_GetUserStatusText(pAxis, tmp, sizeof tmp, pHooveredSerie, xPosDbl, yPosDbl)) {

					auto y = pHooveredSerie->doublePrec ?
						Interpolate<true, true, false>(xPosDbl, pHooveredSerie->xDouble, pHooveredSerie->yDouble, pHooveredSerie->n) :
						Interpolate<true, true, false>((float)xPosDbl, pHooveredSerie->xSingle, pHooveredSerie->ySingle, pHooveredSerie->n);

					swprintf_s(tmp,
						L"Serie %llu \"%s\" x = %12g,   y = %12g",
						(long long unsigned)pHooveredSerie->iSerie,
						pHooveredSerie->pName,
						xPosDbl, y);
				}
				AxisBorder_SetText(pAxis->pStatusBar, tmp);
				AxisBorder_Draw(pAxis->pStatusBar);
				Axis_DrawBoundingRects(pAxis, 0, 1);
				Axis_Redraw(pAxis);

				if (!pHooveredSerie->pMarker) {
					
					pHooveredSerie->pMarker = 
						AxisSerieMarker_Create(pSerieAxis, pHooveredSerie->iSerie, AXIS_MARKER_DIAGCROSS);
				}
				else {
					oldMarker = *pHooveredSerie->pMarker;
					pOldMarker = &oldMarker;
					pHooveredSerie->pMarker->markerType = AXIS_MARKER_DIAGCROSS;
					pHooveredSerie->pMarker->nPixel = 2;
					pHooveredSerie->pMarker->lineWidth = 1;
				}
				
				unsigned __int64 serieId = pHooveredSerie->id;

				acs.Leave();
				if (!acs.CheckReleased()) MY_ERROR("ICI");

				Axis_RefreshAsync(pAxis);
				
				MSG msg;
				Axis * pAxis2 = NULL;
				bool removeMarker = false;

				SetCapture(hWnd);
				while (Axis_IsValidhWnd(pAxis->hWnd)) {

					int count = 0;

					GetMessageW(&msg, NULL, 0, 0);
					bool messageHandled = false;

					if (msg.message == WM_MOUSEMOVE && msg.hwnd == hWnd && (pAxis->drawCallback || pAxis->pSeries)) {

						AutoCriticalSection acs2;

						Critical_Section(g_axisArray.CriticalSection()) { 
							aVect_static_for(g_axisArray, i) {
								pAxis2 = g_axisArray[i];
								Axis_AssertIsNotMatrixViewLegend(pAxis2, "Legend Axis not supposed to be referenced in g_axisArray");
								if (pAxis2->hWnd == hWnd && Axis_IsPlotArea(pAxis2, &pt)) {
									found = true;
									break;
								}
							}
						}

						if (found) {

							acs2.Enter(pAxis2->pCs);

							bool bBreak = false;
							if (pAxis2 == pAxis && msg.message == WM_MOUSEMOVE && msg.hwnd == hWnd) {
								messageHandled = true;
								xPosDbl = GET_X_LPARAM(msg.lParam); 
								yPosDbl = GET_Y_LPARAM(msg.lParam); 

								Axis_GetCoord(pSerieAxis, &xPosDbl, &yPosDbl);
								AxisSerie * pHooveredSerie2 = Axis_GetSerieFromPoint(pSerieAxis, xPosDbl, yPosDbl);

								if (pHooveredSerie2) {
									if (!Axis_GetUserStatusText(pAxis, tmp, sizeof tmp, pHooveredSerie2, xPosDbl, yPosDbl)) {

										auto y = pHooveredSerie2->doublePrec ?
											Interpolate<true, true, false>(xPosDbl, pHooveredSerie2->xDouble, pHooveredSerie2->yDouble, pHooveredSerie2->n) :
											Interpolate<true, true, false>((float)xPosDbl, pHooveredSerie2->xSingle, pHooveredSerie2->ySingle, pHooveredSerie2->n);

										swprintf_s(tmp,
											L"Serie %llu \"%s\" x = %12g,   y = %12g",
											(long long unsigned)pHooveredSerie2->iSerie,
											pHooveredSerie2->pName,
											xPosDbl, y);
									}

									AxisBorder_SetText(pAxis->pStatusBar, tmp);
									AxisBorder_Draw(pAxis->pStatusBar);
									Axis_DrawBoundingRects(pAxis, 0, 1);
									Axis_Redraw(pAxis);
								}

								if (!pHooveredSerie2 || pHooveredSerie2->id != serieId) {
									xVect_static_for(pSerieAxis->pSeries, i) {
										AxisSerie * pSerie = &pSerieAxis->pSeries[i];
										if (pSerie->id == serieId) {
											if (pOldMarker) {
												*pSerie->pMarker = *pOldMarker;
											}
											else {
												AxisSerieMarker_Destroy(pSerie->pMarker);
											}
											break;
										}
									}
									bBreak = true;
								}
							}
							if (bBreak) break;
						}
					}

					if (!messageHandled) {
						TranslateMessage(&msg);
						DispatchMessageW(&msg);
					}
				}

				ReleaseCapture();

				if (!acs.CheckReleased()) MY_ERROR("ICI");

				Axis_RefreshAsync(pAxis);

				acs.Enter();
			}

			if (pAxis->options & AXIS_OPT_STATUSPOS) {
				tmp[0] = 0;
				if (pAxis->pMatrixView && pAxis->pMatrixView->mDouble) {
					bool cond = (pAxis->pMatrixView->options & AXISMATRIXVIEW_OPT_MATRIXORIENTATION) ? 
						AxisMatrixView_IsPointInsideMatrix<true> (pAxis->pMatrixView, xPosDbl, yPosDbl) :
						AxisMatrixView_IsPointInsideMatrix<false>(pAxis->pMatrixView, xPosDbl, yPosDbl);

					if (cond) {
						double value = AxisMatrixView_GetValueFromPoint<false>(pAxis->pMatrixView, xPosDbl, yPosDbl);
						
						if (!Axis_GetUserStatusText(pAxis, tmp, sizeof tmp, nullptr, xPosDbl, yPosDbl)) {
							
							if (!pAxis->pMatrixView->xCoord) xPosDbl = (double)(size_t)xPosDbl;
							if (!pAxis->pMatrixView->yCoord) yPosDbl = (double)(size_t)yPosDbl;

							auto&& matOrient = pAxis->pMatrixView->options & AXISMATRIXVIEW_OPT_MATRIXORIENTATION;

							auto&& p1 = matOrient ? yPosDbl : xPosDbl;
							auto&& p2 = matOrient ? xPosDbl : yPosDbl;
							auto&& n1 = matOrient ? L"Row" : L"x";
							auto&& n2 = matOrient ? L"Col" : L"y";

							if (pAxis->pMatrixView->options & AXIS_CMAP_COLORREF) {

								COLORREF clr = (COLORREF)value;
								if (pAxis->options & AXIS_OPT_LEGEND_CLR_STATUS) {
									xVect_static_for(pAxis->pSeries, i) {
										if (clr == pAxis->pSeries[i].pen.lopnColor) {
											swprintf_s(tmp, L"%s = %12g,   %s = %12g, \"%s\"",
												n1, p1, n2, p2, pAxis->pSeries[i].pName);
											break;
										}
									}
								};

								if (tmp[0] == 0) {
									swprintf_s(tmp, L"%s = %12g,   %s = %12g, Red = %3d, Green = %3d, Blue = %3d",
										n1, p1, n2, p2, GetRValue(clr), GetGValue(clr), GetBValue(clr));
								}
							} else {
								swprintf_s(tmp, L"%s = %12g,   %s = %12g, value = %12g",
									n1, p1, n2, p2, value);
							}
						}
					}
				}
				if (tmp[0] == 0) {
					if (!Axis_GetUserStatusText(pAxis, tmp, sizeof tmp, nullptr, xPosDbl, yPosDbl)) {
						swprintf_s(tmp, L"x = %12g,   y = %12g", xPosDbl, yPosDbl);
					}
				}
				AxisBorder_SetText(pAxis->pStatusBar, tmp);
				AxisBorder_Draw(pAxis->pStatusBar);
				Axis_DrawBoundingRects(pAxis, 0, 1);

				Axis_Redraw(pAxis);
			}

			SetTimer(hWnd, pAxis->id, 100, NULL);

			break;
		}
		case WM_TIMER: {
			POINT pt;
			HWND hWndFromPoint;
			AutoCriticalSection acs;

			Critical_Section(g_axisArray.CriticalSection()) {

				aVect_static_for(g_axisArray, i) {
					pAxis = g_axisArray[i];
					Axis_AssertIsNotMatrixViewLegend(pAxis, "Legend Axis not supposed to be referenced in g_axisArray");
					if (pAxis->hWnd == hWnd && pAxis->id == wParam) {
						found = true;
						break;
					}
				}
			}

			if (!found) {
				OutputDebugStringA("Axis : timer : axis not found !\n");
				KillTimer(hWnd, wParam);
				break;
			}

			acs.Enter(pAxis->pCs);

			GetCursorPos(&pt);
			hWndFromPoint = WindowFromPoint(pt);
			ScreenToClient(pAxis->hWnd, &pt);
			if(!Axis_IsPlotArea(pAxis, &pt) || hWndFromPoint != pAxis->hWnd) {
				AxisBorder_SetText(pAxis->pStatusBar, L"");
				AxisBorder_Draw(pAxis->pStatusBar, 0);
				Axis_DrawBoundingRects(pAxis, 1, 0);
				Axis_DrawLegend(pAxis);
				Axis_DrawBoundingRects(pAxis, 0, 1);
				if (xVect_Count(pAxis->subAxis)) {
					Axis_DrawY_Axis(pAxis);
				}
				Axis_Redraw(pAxis);
				KillTimer(pAxis->hWnd, pAxis->id);
			}

			break;
		}
		case WM_LBUTTONDOWN : {
			MSG msg;
			POINT pt;
			POINT ptDepart;
			pt.x = GET_X_LPARAM(lParam); 
			pt.y = GET_Y_LPARAM(lParam);

			AutoCriticalSection acs;
			//
			Critical_Section(g_axisArray.CriticalSection()) {
				aVect_static_for(g_axisArray, i) {
					pAxis = g_axisArray[i];
					Critical_Section(pAxis->pCs) {
						Axis_AssertIsNotMatrixViewLegend(pAxis, "Legend Axis not supposed to be referenced in g_axisArray");
						if (pAxis->hWnd == hWnd) {
							if (Axis_IsPlotArea(pAxis, &pt, true)) {
								found = true;
								acs.Set(pAxis->pCs);
								break;
							}
						}
					}
				}
			}

			if (!found || (pAxis->options & AXIS_OPT_DISABLED)) break;

			struct DragInfo {
				Axis * pAxis;
				double xMin_depart;
				double xMax_depart;
				double yMin_depart;
				double yMax_depart;
			};

			static WinCriticalSection cs;
			
			Critical_Section(cs) {

				static aVect<DragInfo> drag;

				ptDepart = pt;

				drag.Redim(0);
				drag.Push(DragInfo{ pAxis, pAxis->xMin, pAxis->xMax, pAxis->yMin, pAxis->yMax });

				xVect_static_for(pAxis->subAxis, i) {
					auto pSubAxis = pAxis->subAxis[i];
					drag.Push(DragInfo{ pSubAxis, pSubAxis->xMin, pSubAxis->xMax, pSubAxis->yMin, pSubAxis->yMax });
				}

				bool wasLegendArea = Axis_IsLegendArea(pAxis, &pt);

				bool wasMatrixViewLegendArea = Axis_IsMatrixViewLegendArea(pAxis, &pt);

				SetCapture(hWnd);

				while (Axis_IsValidhWnd(pAxis->hWnd)) {

					int count = 0;

					GetMessageW(&msg, NULL, 0, 0);
					bool messageHandled = false;

					if (msg.message == WM_LBUTTONUP) {
						messageHandled = true;
						int xPos = GET_X_LPARAM(msg.lParam);
						int yPos = GET_Y_LPARAM(msg.lParam);
						if (xPos == ptDepart.x && yPos == ptDepart.y) {
							double xPosDbl = xPos;
							double yPosDbl = yPos;
							Axis_GetCoord(pAxis, &xPosDbl, &yPosDbl);
							SendMessageW(GetParent(hWnd), AXIS_MSG_LCLICK, (WPARAM)&xPosDbl, (LPARAM)&yPosDbl);
						}
						break;
					}
					else if (msg.message == WM_MOUSEMOVE && msg.hwnd == hWnd
						&& (pAxis->drawCallback || pAxis->pSeries || pAxis->pMatrixView)) {
						int xPos = GET_X_LPARAM(msg.lParam);
						int yPos = GET_Y_LPARAM(msg.lParam);

						messageHandled = true;

						long dec_x = ptDepart.x - xPos;
						long dec_y = ptDepart.y - yPos;

						acs.Enter();

						for (auto&& d : drag) {
							long pixMin_x = Axis_CoordTransf_X(d.pAxis, d.xMin_depart);
							long pixMax_x = Axis_CoordTransf_X(d.pAxis, d.xMax_depart);
							long pixMin_y = Axis_CoordTransf_Y(d.pAxis, d.yMin_depart);
							long pixMax_y = Axis_CoordTransf_Y(d.pAxis, d.yMax_depart);

							double xMin = Axis_GetCoordX(d.pAxis, pixMin_x + dec_x);
							double xMax = Axis_GetCoordX(d.pAxis, pixMax_x + dec_x);
							double yMin = Axis_GetCoordY(d.pAxis, pixMin_y + dec_y);
							double yMax = Axis_GetCoordY(d.pAxis, pixMax_y + dec_y);

							if (wasLegendArea) {
								pAxis->legend->adim_left -= (double)(pt.x - xPos) / pAxis->DrawArea_nPixels_x;
								pAxis->legend->adim_top -= (double)(pt.y - yPos) / pAxis->DrawArea_nPixels_y;
								break;//no legend for subAxis;
							}
							else if (wasMatrixViewLegendArea) {
								if (!pAxis->pMatrixView || !pAxis->pMatrixView->cMap || !pAxis->pMatrixView->cMap->legendCmap) {
									Axis_Refresh(pAxis, false, false, false);
								}
								Axis * pLegendAxis = pAxis->pMatrixView->cMap->legendCmap;
								if (pAxis->options & AXIS_OPT_WINDOWED) {
									pLegendAxis->adim_dx -= (double)(pt.x - xPos) / pAxis->DrawArea_nPixels_x;
									pLegendAxis->adim_dy -= (double)(pt.y - yPos) / pAxis->DrawArea_nPixels_y;
								}
								else {
									pLegendAxis->adim_dx -= (double)(pt.x - xPos) / pAxis->DrawArea_nPixels_x * pAxis->adim_nX;
									pLegendAxis->adim_dy -= (double)(pt.y - yPos) / pAxis->DrawArea_nPixels_y * pAxis->adim_nY;
								}
								Axis_Resize(pLegendAxis);
								break;//no legend for subAxis;
							}
							else {
								Axis_Zoom(d.pAxis, xMin, xMax, yMin, yMax);
							}
						}

						acs.Leave();
						if (!acs.CheckReleased()) MY_ERROR("ICI");

						Axis_RefreshAsync(pAxis);

						pt.x = xPos;
						pt.y = yPos;
					}
					else if (msg.message == WM_LBUTTONDOWN ||
						msg.message == WM_LBUTTONUP ||
						msg.message == WM_RBUTTONDOWN ||
						msg.message == WM_RBUTTONUP) {
						messageHandled = true;
					}

					if (!messageHandled) {
						TranslateMessage(&msg);
						DispatchMessageW(&msg);
					}
				}
			}

			ReleaseCapture();

			break; 
		}
	}

	return DefWindowProcW(hWnd, message, wParam, lParam);
}; 

#include "xMat.h"
#include "AxisClass.h"

__declspec(noinline)
bool AxisClass::Check_hAxis(char * func) {
	if (hAxis) return true;
	DbgStr(L"\n%S : Axis not created or already released!\n\n", func);
	return false;
}


AxisClass::operator bool() const {
	if (!hAxis) return false;
	if (Axis_GetHwnd(hAxis) == NULL) return false;
	return true;
}

AxisFontClass AxisClass::SetFont(void) {
	AxisFontClass c(this);
	return c;
}

AxisClass& AxisClass::SetFontSize(int fontSize) {
	SetFont().Legend().FontSize(fontSize);
	SetFont().StatusBar().FontSize(fontSize);
	SetFont().Title().FontSize(fontSize);
	SetFont().xLabel().FontSize(fontSize);
	SetFont().yLabel().FontSize(fontSize);
	SetFont().TickLabels().FontSize(fontSize);

	return *this;
}

AxisClass& AxisClass::SetFontName(wchar_t * fontName) {
	SetFont().Legend().FontName(fontName);
	SetFont().StatusBar().FontName(fontName);
	SetFont().Title().FontName(fontName);
	SetFont().xLabel().FontName(fontName);
	SetFont().yLabel().FontName(fontName);
	SetFont().TickLabels().FontName(fontName);

	return *this;
}

AxisHandle AxisClass::GetHandle(bool giveUpOwnership) {
	if (giveUpOwnership) this->owner = false;
	return hAxis;
}

//Don't call when axis critical section is held !
AxisClass& AxisClass::BringOnTop(void) {
	Axis_BringOnTop(hAxis);
	return *this;
}

AxisClass& AxisClass::EnforceSquareZoom(bool enable) {
	Axis_EnforceSquareZoom(hAxis, enable);
	return *this;
}

AxisClass& AxisClass::ShowLegend(void) {
	Axis_SetOption(hAxis, AXIS_OPT_LEGEND, true);
	return *this;
}

AxisClass& AxisClass::HideLegend(void) {
	Axis_SetOption(hAxis, AXIS_OPT_LEGEND, false);
	return *this;
}

void AxisClass::Detach(void) {
	hAxis = NULL;
	owner = false;
}

AxisClass::AxisClass(void) {
	hAxis = NULL;
}

AxisClass::AxisClass(AxisHandle axisHdl, bool takeOwnership) {
	hAxis = axisHdl;
	owner = takeOwnership;
}

AxisClass::~AxisClass(void) {
	if (hAxis && owner && g_hAxisMgrThread)
		Axis_Release(hAxis);
}

AxisClass& AxisClass::Wrap(AxisHandle axisHdl, bool takeOwnership) {

	if (hAxis && owner) Axis_Release(hAxis);

	hAxis = axisHdl;
	owner = takeOwnership;

	return *this;
}

AxisClass& AxisClass::Create(AxisClass * pAxisClassParent,
	DWORD options, wchar_t * title, int style, bool managed) {

	return Create(pAxisClassParent ? pAxisClassParent->hWnd() : (HWND)NULL, options, title, style, managed);
}

AxisClass& AxisClass::Create(nullptr_t,
	DWORD options, wchar_t * title, int style, bool managed) {

	return Create((HWND)NULL, options, title, style, managed);
}

AxisClass& AxisClass::SetColorMap(int options) {
	Axis_SetColorMap(hAxis, options);
	return *this;
}

AxisClass& AxisClass::CreateMatrixView() {
	Axis_CreateMatrixView(hAxis, (double*)nullptr, 0, 0);
	return *this;
}

AxisClass& AxisClass::CreateMatrixView(float * m, size_t nRow, size_t nCol) {
	Axis_CreateMatrixView(hAxis, m, nRow, nCol);
	return *this;
}

AxisClass& AxisClass::CreateMatrixView(aMat<float> & m, bool setMatrixOrientation) {
	Axis_CreateMatrixView(hAxis, m, m.nRow(), m.nCol());
	if (setMatrixOrientation) Axis_SetMatrixViewOrientation(hAxis, true);
	return *this;
}

AxisClass& AxisClass::SetMatrixViewOrientation(bool matrixOriented) {
	Axis_SetMatrixViewOrientation(hAxis, matrixOriented);
	return *this;
}

AxisClass& AxisClass::CreateMatrixView(double * m, size_t nRow, size_t nCol) {
	Axis_CreateMatrixView(hAxis, m, nRow, nCol);
	return *this;
}

AxisClass& AxisClass::CreateMatrixView(aMat<double> & m, bool setMatrixOrientation) {
	Axis_CreateMatrixView(hAxis, m, m.nRow(), m.nCol());
	if (setMatrixOrientation) Axis_SetMatrixViewOrientation(hAxis, true);
	return *this;
}

AxisClass& AxisClass::CreateMatrixView(aPlan<double> & m) {
	Axis_CreateMatrixView(hAxis, m, m.xCount(), m.yCount());
	return *this;
}

AxisClass& AxisClass::CreateMatrixView(aPlan<float> & m) {
	Axis_CreateMatrixView(hAxis, m, m.xCount(), m.yCount());
	return *this;
}

AxisClass& AxisClass::CreateMatrixView(bTens<double> & m) {
	Axis_CreateMatrixView(hAxis, m, m.Dim(0), m.Dim(1));
	return *this;
}

AxisClass& AxisClass::CreateMatrixView(bTens<float> & m) {
	Axis_CreateMatrixView(hAxis, m, m.Dim(0), m.Dim(1));
	return *this;
}

AxisClass& AxisClass::Create(AxisClass & axisClassParent,
	DWORD options, wchar_t * title, int style, bool managed) {

	return Create(axisClassParent.hWnd(), options, title, style, managed);
}

AxisClass& AxisClass::Create(HWND hWndParent,
	DWORD options, wchar_t * title, int style, bool managed) {

	if (hAxis && owner) Axis_Release(hAxis);

	hAxis = Axis_Create(hWndParent, options, title, style, managed);

	owner = true;
	return *this;
}

AxisClass& AxisClass::CreateAt(double nx, double ny, double dx, double dy,
	HWND hWnd, DWORD options, wchar_t * title, int style, bool managed) {

	if (hAxis && owner) Axis_Release(hAxis);

	hAxis = Axis_CreateAt(nx, ny, dx, dy,
		hWnd, options, title, style, managed);

	owner = true;
	return *this;
}

AxisClass& AxisClass::CreateAt(double nx, double ny, double dx, double dy,
	AxisClass * pAxisClassParent, DWORD options, wchar_t * title, int style, bool managed) {


	CreateAt(nx, ny, dx, dy,
		pAxisClassParent ? pAxisClassParent->hWnd() : (HWND)NULL,
		options, title, style, managed);

	return *this;
}

AxisClass& AxisClass::CreateAt(double nx, double ny, double dx, double dy,
	nullptr_t, DWORD options, wchar_t * title, int style, bool managed) {


	CreateAt(nx, ny, dx, dy,
		(HWND)NULL,
		options, title, style, managed);

	return *this;
}

AxisClass& AxisClass::CreateAt(double nx, double ny, double dx, double dy,
	AxisClass & axisClassParent, DWORD options, wchar_t * title, int style, bool managed) {


	CreateAt(nx, ny, dx, dy,
		axisClassParent.hWnd(),
		options, title, style, managed);

	return *this;
}

bool AxisClass::Exist(void) {
	return Axis_Exist(hAxis);
}

AxisClass& AxisClass::DrawRectangle(double left, double top, double right, double bottom) {
	if (Check_hAxis(__FUNCTION__)) Axis_DrawRectangle(hAxis, left, top, right, bottom);
	return *this;
}

AxisClass& AxisClass::DrawEllipse(double left, double top, double right, double bottom) {
	if (Check_hAxis(__FUNCTION__)) Axis_DrawEllipse(hAxis, left, top, right, bottom);
	return *this;
}

AxisClass& AxisClass::MoveTo(double x, double y) {
	if (Check_hAxis(__FUNCTION__)) Axis_MoveToEx(hAxis, x, y);
	return *this;
}

AxisClass& AxisClass::LineTo(double x, double y) {
	if (Check_hAxis(__FUNCTION__)) Axis_LineTo(hAxis, x, y);
	return *this;
}

void AxisClass::Delete(void) {
	if (Check_hAxis(__FUNCTION__)) Axis_Delete(hAxis);
	hAxis = NULL;
}

void AxisClass::Release(void) {
	if (Check_hAxis(__FUNCTION__)) Axis_Release(hAxis);
	hAxis = NULL;
}

AxisClass& AxisClass::SetDrawCallback(
	AxisDrawCallback drawCallback,
	AxisDrawCallback callbacksCleanUpCallback) {

	if (Check_hAxis(__FUNCTION__)) {
		Axis_SetDrawCallback(hAxis, drawCallback, callbacksCleanUpCallback);
	}

	return *this;
}

AxisClass& AxisClass::SetCheckZoomCallback(AxisCheckZoomCallback checkZoomCallback) {

	if (Check_hAxis(__FUNCTION__)) {
		Axis_SetCheckZoomCallback(hAxis, checkZoomCallback);
	}

	return *this;
}

AxisClass& AxisClass::SetCallbacksCleanUpCallback(
	AxisDrawCallback callbacksCleanUpCallback) {

	if (Check_hAxis(__FUNCTION__)) {
		Axis_SetCallbacksCleanUpCallback(hAxis, callbacksCleanUpCallback);
	}

	return *this;
}

AxisClass& AxisClass::SetCallbacksData(void * callbacksData) {
	if (Check_hAxis(__FUNCTION__)) Axis_SetCallbacksData(hAxis, callbacksData);
	return *this;
}

AxisClass& AxisClass::PushCallbacksData(void * callbacksData) {
	if (Check_hAxis(__FUNCTION__)) Axis_PushCallbacksData(hAxis, callbacksData);
	return *this;
}

AxisClass& AxisClass::Move(double nx, double ny, double dx, double dy) {
	if (Check_hAxis(__FUNCTION__)) Axis_Move(hAxis, nx, ny, dx, dy);
	return *this;
}

//Don't call when axis critical section is held !
AxisClass& AxisClass::SetTitle(const wchar_t * title) {
	if (Check_hAxis(__FUNCTION__)) Axis_SetTitle(hAxis, title);
	return *this;
}

AxisClass& AxisClass::xLabel(const wchar_t * label) {
	if (Check_hAxis(__FUNCTION__)) Axis_xLabel(hAxis, label);
	return *this;
}

AxisClass& AxisClass::yLabel(const wchar_t * label) {
	if (Check_hAxis(__FUNCTION__)) Axis_yLabel(hAxis, label);
	return *this;
}

AxisClass& AxisClass::PolyLine(double * x, double * y, size_t n) {
	Axis_PolyLine(hAxis, x, y, n);
	return *this;
}

bool AxisClass::PlotDataWaiting(void) {
	return Axis_CallbacksDataWaiting(hAxis);
}

AxisClass& AxisClass::Refresh(bool autoFit, bool redraw) {
	if (Check_hAxis(__FUNCTION__)) Axis_Refresh(hAxis, autoFit, false, redraw);
	return *this;
}

AxisClass& AxisClass::RefreshAsync(bool autoFit, bool redraw, bool cancelPreviousJobs) {
	if (Check_hAxis(__FUNCTION__) && hAxis) Axis_RefreshAsync(hAxis, autoFit, false, redraw, cancelPreviousJobs);
	return *this;
}

AxisClass& AxisClass::SetZoomed(void) {
	if (Check_hAxis(__FUNCTION__)) Axis_SetZoomed(hAxis);
	return *this;
}

//Don't call when axis critical section is held !
void AxisClass::AlwaysOnTop(void) {
	SetWindowPos(this->hWnd(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
}

void AxisClass::CloseWindow(void) {
	if (Check_hAxis(__FUNCTION__)) {
		Axis_CloseWindow(hAxis);
		//hAxis = nullptr; // ???????
	}
}

AxisClass& AxisClass::AddSerie_Raw(
	const wchar_t * pName, const double * x, const double * y, size_t n,
	COLORREF color, DWORD penStyle, int lineWidth, DWORD markerType,
	COLORREF markerColor, unsigned short marker_nPixel, unsigned short markerLineWidth) {

	if (Check_hAxis(__FUNCTION__)) {
		Axis_AddSerie_Raw(hAxis, pName, x, y, n, color, penStyle,
			(unsigned short)lineWidth, markerType, markerColor, marker_nPixel, markerLineWidth);
	}
	return *this;
}

AxisClass& AxisClass::AddSerie_Raw(
	const wchar_t * pName, const float * x, const float * y, size_t n,
	COLORREF color, DWORD penStyle, int lineWidth, DWORD markerType,
	COLORREF markerColor, unsigned short marker_nPixel, unsigned short markerLineWidth) {

	if (Check_hAxis(__FUNCTION__)) {
		Axis_AddSerie_Raw(hAxis, pName, x, y, n, color, penStyle,
			(unsigned short)lineWidth, markerType, markerColor, marker_nPixel, markerLineWidth);
	}
	return *this;
}

int AxisClass::AddSubAxisSerie_Raw(
	const wchar_t * pSubAxisName, const wchar_t * pSerieName,
	const double * x, const double * y, size_t n,
	COLORREF color, DWORD penStyle, int lineWidth,
	DWORD markerType, COLORREF markerColor, unsigned short marker_nPixel,
	unsigned short markerLineWidth)
{
	if (Check_hAxis(__FUNCTION__)) {
		return Axis_AddSubAxisSerieCore(hAxis, pSubAxisName, pSerieName, x, y, n,
			color, penStyle, lineWidth, markerType, markerColor, marker_nPixel, markerLineWidth);
	}
	return -1;
}

int AxisClass::AddSubAxisSerie_Raw(
	const wchar_t * pSubAxisName, const wchar_t * pSerieName,
	const float * x, const float * y, size_t n,
	COLORREF color, DWORD penStyle, int lineWidth,
	DWORD markerType, COLORREF markerColor, unsigned short marker_nPixel,
	unsigned short markerLineWidth)
{
	if (Check_hAxis(__FUNCTION__)) {
		return Axis_AddSubAxisSerieCore(hAxis, pSubAxisName, pSerieName, x, y, n, color,
			penStyle, lineWidth, markerType, markerColor, marker_nPixel, markerLineWidth);
	}
	return -1;
}


AxisClass& AxisClass::AutoFit(bool colorAutoFit, bool forceAdjustZoom) {
	if (Check_hAxis(__FUNCTION__)) Axis_AutoFit(hAxis, colorAutoFit, forceAdjustZoom);
	return *this;
}

AxisClass& AxisClass::Clear(void) {
	if (Check_hAxis(__FUNCTION__)) Axis_Clear(hAxis);
	return *this;
}

AxisClass&  AxisClass::xLim(double xMin, double xMax, bool forceAdjustZoom) {
	Axis_xLim(hAxis, xMin, xMax, forceAdjustZoom);
	return *this;
}

AxisClass&  AxisClass::yLim(double yMin, double yMax, bool forceAdjustZoom) {
	Axis_yLim(hAxis, yMin, yMax, forceAdjustZoom);
	return *this;
}

AxisClass&  AxisClass::cLim(double cMin, double cMax) {
	Axis_cLim(hAxis, cMin, cMax);
	return *this;
}

AxisClass&  AxisClass::xMin(double xMin, bool forceAdjustZoom) {
	Axis_xMin(hAxis, xMin, forceAdjustZoom);
	return *this;
}

AxisClass&  AxisClass::xMax(double xMax, bool forceAdjustZoom) {
	Axis_xMax(hAxis, xMax, forceAdjustZoom);
	return *this;
}

AxisClass&  AxisClass::yMin(double yMin, bool forceAdjustZoom) {
	Axis_yMin(hAxis, yMin, forceAdjustZoom);
	return *this;
}

AxisClass&  AxisClass::yMax(double yMax, bool forceAdjustZoom) {
	Axis_yMax(hAxis, yMax, forceAdjustZoom);
	return *this;
}

AxisClass&  AxisClass::cMin(double cMin) {
	Axis_cMin(hAxis, cMin);
	return *this;
}

AxisClass&  AxisClass::cMax(double cMax) {
	Axis_cMax(hAxis, cMax);
	return *this;
}

double AxisClass::xMin(bool orig) {
	return Axis_xMin(hAxis, orig);
}

double AxisClass::xMax(bool orig) {
	return Axis_xMax(hAxis, orig);
}

double AxisClass::yMin(bool orig) {
	return Axis_yMin(hAxis, orig);
}

double AxisClass::yMax(bool orig) {
	return Axis_yMax(hAxis, orig);
}

double AxisClass::cMin(bool orig) {
	return Axis_cMin(hAxis, orig);
}

double AxisClass::cMax(bool orig) {
	return Axis_cMax(hAxis, orig);
}

AxisClass& AxisClass::BeginDraw(void) {
	Axis_BeginDraw(hAxis);
	return *this;
}

AxisClass& AxisClass::EndDraw(void) {
	Axis_EndDraw(hAxis);
	return *this;
}

AxisClass& AxisClass::Redraw(void) {
	Axis_Redraw(hAxis);
	return *this;
}

AxisClass& AxisClass::SetBrush(HBRUSH hBrush) {
	Axis_SelectBrush(hAxis, hBrush);
	return *this;
}

AxisClass& AxisClass::SetPen(HPEN hPen) {
	Axis_SelectPen(hAxis, hPen);
	return *this;
}

AxisClass& AxisClass::SetPenColor(COLORREF color) {
	Axis_SetPenColor(hAxis, color);
	return *this;
}

AxisClass& AxisClass::SetBrushColor(COLORREF color) {
	Axis_SetBrushColor(hAxis, color);
	return *this;
}

AxisClass& AxisClass::SetPen(COLORREF color, int linewidth) {
	HPEN hPen = CreatePen(PS_SOLID, linewidth, color);
	Axis_SelectPen(hAxis, hPen);
	return *this;
}

CRITICAL_SECTION * AxisClass::GetCriticalSection() {
	if (Check_hAxis(__FUNCTION__)) return Axis_GetCriticalSection(this->hAxis);
	return nullptr;
}

AxisClass& AxisClass::Init(void) {
	Axis_InitBitmaps(hAxis);
	return *this;
}

AxisClass& AxisClass::ToClipboard(void) {
	Axis_CopyMetaFileToClipBoard(hAxis);
	return *this;
}

HWND AxisClass::hWnd(void) {
	if (Check_hAxis(__FUNCTION__)) return Axis_GetHwnd(hAxis);
	return NULL;
}

AxisDrawCallback AxisClass::GetDrawCallback(void) {
	if (Check_hAxis(__FUNCTION__)) return Axis_GetDrawCallback(hAxis);
	else return nullptr;
}

AxisClass& AxisClass::ClearSeries(void) {
	if (Check_hAxis(__FUNCTION__)) Axis_ClearSeries(hAxis);
	return *this;
}

AxisClass& AxisClass::SetStatusTextCallback(AxisStatusTextCallback statusTextCallback) {
	if (Check_hAxis(__FUNCTION__)) Axis_SetStatusTextCallback(hAxis, statusTextCallback);
	return *this;
}

AxisStatusTextCallback AxisClass::GetStatusTextCallback() {
	if (Check_hAxis(__FUNCTION__)) return Axis_GetStatusTextCallback(hAxis);
	else return nullptr;
}

AxisClass& AxisClass::SetDrawCallbackOrder(int order) {
	if (Check_hAxis(__FUNCTION__)) Axis_SetDrawCallbackOrder(hAxis, order);
	return *this;
}

int AxisClass::GetDrawCallbackOrder() {
	if (Check_hAxis(__FUNCTION__)) return Axis_GetDrawCallbackOrder(hAxis);
	return 0;
}

AxisClass& AxisClass::SetMatrixViewCoord_X(const aVect<double> & coord_X) {
	if (Check_hAxis(__FUNCTION__)) Axis_SetMatrixViewCoord_X(hAxis, coord_X);
	return *this;
}

AxisClass& AxisClass::SetMatrixViewCoord_Y(const aVect<double> & coord_Y) {
	if (Check_hAxis(__FUNCTION__)) Axis_SetMatrixViewCoord_Y(hAxis, coord_Y);
	return *this;
}

AxisClass& AxisClass::SetLogScaleX(bool enable) {
	if (Check_hAxis(__FUNCTION__)) Axis_SetLogScaleX(hAxis, enable);
	return *this;
}

AxisClass& AxisClass::SetLogScaleY(bool enable) {
	if (Check_hAxis(__FUNCTION__)) Axis_SetLogScaleY(hAxis, enable);
	return *this;
}

AxisClass& AxisClass::SetLegendColorStatus(bool enable) {
	if (Check_hAxis(__FUNCTION__)) Axis_SetLegendColorStatus(hAxis, enable);
	return *this;
}

AxisClass& AxisClass::Enable(bool enable) {
	if (Check_hAxis(__FUNCTION__)) Axis_SetOption(hAxis, AXIS_OPT_DISABLED, !enable);
	return *this;
}

AxisClass& AxisClass::SetTextColor(COLORREF color) {
	if (Check_hAxis(__FUNCTION__)) Axis_SetTextColor(hAxis, color);
	return *this;
}

AxisClass& AxisClass::SetBkColor(COLORREF color) {
	if (Check_hAxis(__FUNCTION__)) Axis_SetBkColor(hAxis, color);
	return *this;
}

AxisClass& AxisClass::SetBkMode(int mode) {
	if (Check_hAxis(__FUNCTION__)) Axis_SetBkMode(hAxis, mode);
	return *this;
}

AxisClass& AxisClass::SaveImageToFile(const wchar_t * filePath, bool onlyLegend) {
	if (Check_hAxis(__FUNCTION__)) Axis_SaveImageToFile(hAxis, filePath, onlyLegend);
	return *this;
}

double AxisClass::GetPixelSizeX(double x) {
	if (Check_hAxis(__FUNCTION__)) {
		return Axis_GetCoordPixelSizeX(this->hAxis, x);
	}
	return 0;
}

double AxisClass::GetPixelSizeY(double y) {
	if (Check_hAxis(__FUNCTION__)) {
		return Axis_GetCoordPixelSizeY(this->hAxis, y);
	}
	return 0;
}

AxisClass& AxisClass::SubAxis_yLim(int i, double yMin, double yMax, bool forceAdjustZoom) {
	if (Check_hAxis(__FUNCTION__)) {
		Axis_SubAxis_yLim(this->hAxis, i, yMin, yMax, forceAdjustZoom);
	}
	return *this;
}

double AxisClass::SubAxis_yMin(int i, bool orig) {
	if (Check_hAxis(__FUNCTION__)) {
		return Axis_SubAxis_yMin(this->hAxis, i, orig);
	}
	return std::numeric_limits<double>::quiet_NaN();
}

double AxisClass::SubAxis_yMax(int i, bool orig) {
	if (Check_hAxis(__FUNCTION__)) {
		return Axis_SubAxis_yMax(this->hAxis, i, orig);
	}
	return std::numeric_limits<double>::quiet_NaN();
}

void AxisClass::SubAxis_yMin(int i, double yMin, bool forceAdjustZoom) {
	if (Check_hAxis(__FUNCTION__)) {
		Axis_SubAxis_yMin(this->hAxis, i, yMin, forceAdjustZoom);
	}
}

void AxisClass::SubAxis_yMax(int i, double yMax, bool forceAdjustZoom) {
	if (Check_hAxis(__FUNCTION__)) {
		Axis_SubAxis_yMax(this->hAxis, i, yMax, forceAdjustZoom);
	}
}
