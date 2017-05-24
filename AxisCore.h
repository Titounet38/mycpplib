#pragma once

//#define BIT(n) (1<<((n)-1))

//Constants
#define AXIS_MSG_RELEASE             (WM_APP)
#define AXIS_MSG_RCLICK              (WM_APP + 1)
#define AXIS_MSG_LCLICK              (WM_APP + 2)
#define AXIS_MSG_CREATE_WINDOW       (WM_APP + 3)
#define AXIS_MSG_REFRESH             (WM_APP + 4)

#ifndef _AXIS_
struct AxisMsg_Point {
	double x, y;
};
#endif

#define AXIS_COLOR_DEFAULT ((COLORREF)0xFF000000)

#define AXISTICKS_OPT_GRID			BIT(1)
#define AXISTICKS_OPT_MINOR_TICKS   BIT(2)
#define AXISTICKS_OPT_LABELS        BIT(3)

#define AXISMATRIXVIEW_OPT_INTERPOL			 BIT(1)
#define AXISMATRIXVIEW_OPT_MATRIXORIENTATION BIT(2)
#define AXISMATRIXVIEW_OPT_DOUBLEPREC		 BIT(3)
#define AXISMATRIXVIEW_OPT_INITIALIZED		 BIT(4)
#define AXISMATRIXVIEW_OPT_VMIN_INITIALIZED  BIT(5)
#define AXISMATRIXVIEW_OPT_VMAX_INITIALIZED  BIT(6)
#define AXISMATRIXVIEW_OPT_VMINO_INITIALIZED BIT(7)
#define AXISMATRIXVIEW_OPT_VMAXO_INITIALIZED BIT(8)
#define AXIS_CMAP_GRAYSCALE					 BIT(9)
#define AXIS_CMAP_JET						 BIT(10)
#define AXIS_CMAP_JET2						 BIT(11)
#define AXIS_CMAP_HSV						 BIT(12)
#define AXIS_CMAP_HOT						 BIT(13)
#define AXIS_CMAP_COSMOS					 BIT(14)
#define AXIS_CMAP_COLORREF					 BIT(15)

#define AXIS_CMAP_DEFAULT AXIS_CMAP_JET | AXISMATRIXVIEW_OPT_INTERPOL

#define AXIS_OPT_WINDOWED			BIT(1)
#define AXIS_OPT_ADIM_MODE			BIT(2)
#define AXIS_OPT_GHOST				BIT(3)
#define AXIS_OPT_USERGHOST			BIT(4)
#define AXIS_OPT_STATUSPOS			BIT(5)
#define AXIS_OPT_ZEROLINES			BIT(6)
#define AXIS_OPT_LEGEND				BIT(7)
#define AXIS_OPT_DISABLED			BIT(8)
#define AXIS_OPT_LEFTBORDER			BIT(9)
#define AXIS_OPT_RIGHTBORDER		BIT(10)
#define AXIS_OPT_TOPBORDER			BIT(11)
#define AXIS_OPT_BOTTOMBORDER		BIT(12)
#define AXIS_OPT_MAINHDC			BIT(13)
#define AXIS_OPT_ISMATVIEWLEGEND	BIT(14)
#define AXIS_OPT_ISORPHAN			BIT(15)
#define AXIS_OPT_CREATEINVISIBLE    BIT(16)
#define AXIS_OPT_SQUAREZOOM			BIT(17)
#define AXIS_OPT_CUR_XLOG			BIT(18)
#define AXIS_OPT_CUR_YLOG			BIT(19)
#define AXIS_OPT_XLOG				BIT(20)
#define AXIS_OPT_YLOG				BIT(21)
#define AXIS_OPT_LEGEND_CLR_STATUS	BIT(22)
#define AXIS_OPT_2BECLOSED		    BIT(23)

#define AXIS_OPT_BORDERS		 (AXIS_OPT_LEFTBORDER | AXIS_OPT_RIGHTBORDER | \
								  AXIS_OPT_TOPBORDER  | AXIS_OPT_BOTTOMBORDER)
#define AXIS_OPT_EMPTY			 (AXIS_OPT_GHOST | AXIS_OPT_WINDOWED | AXIS_OPT_USERGHOST)
#define AXIS_OPT_CONTAINER		 (AXIS_OPT_EMPTY | AXIS_OPT_MAINHDC)
#define AXIS_OPT_DEFAULT		 (AXIS_OPT_WINDOWED  | AXIS_OPT_ADIM_MODE |		\
								  AXIS_OPT_STATUSPOS | AXIS_OPT_ZEROLINES |		\
								  AXIS_OPT_LEGEND    | AXIS_OPT_BORDERS)

#define AXIS_ALIGN_XLEFT     BIT(1)
#define AXIS_ALIGN_XCENTER   BIT(2)
#define AXIS_ALIGN_XRIGHT    BIT(3)
#define AXIS_ALIGN_YTOP      BIT(4)
#define AXIS_ALIGN_YCENTER	 BIT(5)
#define AXIS_ALIGN_YBOTTOM   BIT(6)

#define AXIS_MARKER_NONE		   0
#define AXIS_MARKER_CROSS		   1
#define AXIS_MARKER_DIAGCROSS      2
#define AXIS_MARKER_POINT          3
#define AXIS_MARKER_CIRCLE         4
#define AXIS_MARKER_QUARE          5
#define AXIS_MARKER_FULLSQUARE     6
#define AXIS_MARKER_STAR           7
#define AXIS_MARKER_FULLSTAR       8
#define AXIS_MARKER_LOSANGE        9
#define AXIS_MARKER_FULLLOSANGE   10

#define AXIS_TEXT_HEIGHT 15

//Struct declarations
struct Axis;
struct AxisColorMap;
struct AxisMatrixView;
struct AxisBorder;
struct AxisTicks;
struct AxisSerie;
struct AxisSerieMarker;
struct AxisLegend;
struct AxisFont;

#ifndef _AXIS_

//Typdefs
typedef void(*AxisDrawCallback) (Axis * axis, void * callbacksData);
typedef bool (*AxisStatusTextCallback)(wchar_t*, size_t, size_t, const wchar_t*, double, double, void*);
typedef void(*AxisCheckZoomCallback)(Axis * axis, bool & cancel);

//Function declarations
LRESULT CALLBACK AxisWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK AxisMgrWinProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void Axis_AdimResizeWindow(Axis * pAxis);
void Axis_DrawGridAndTicks(Axis * pAxis, bool drawGrid = 1, bool drawTicks = 1, bool computeDeltaTicks = 1);
AxisTicks * AxisBorder_CreateTicks(AxisBorder * pBorder);
void Axis_SelectPen(Axis * axis, HPEN hPen);
void Axis_DrawLegendCore(Axis * pAxis, bool computeExtent);
void Axis_UpdateBorderPos(Axis * pAxis);
void Axis_DrawRectangle(Axis * axis, double left, double top, double right, double bottom);
void Axis_DrawRectangle2(Axis * pAxis, double left, double top, double right, double bottom, COLORREF rgbColor);
void Axis_DrawRectangle3(Axis * pAxis, double left, double top, double right, double bottom, COLORREF rgbColor);
void Axis_cLim(Axis * pAxis, double vMin, double vMax, bool forceAdjustZoom = false, bool zoom = false);
void Axis_CreateMatrixView(Axis * pAxis, double * m, size_t nRow, size_t nCol);
void Axis_CreateMatrixView(Axis * pAxis, float * m,  size_t nRow, size_t nCol);
void Axis_CreateMatrixView(Axis * pAxis, double * m, size_t nRow, size_t nCol, DWORD options);
void Axis_CreateMatrixView(Axis * pAxis, float * m,  size_t nRow, size_t nCol, DWORD options);
void Axis_Resize(Axis * pAxis);
void AxisBorder_ComputeExtent(AxisBorder * pBorder);
void AxisBorder_DestroyTicks(AxisBorder * pBorder);
void AxisBorder_SetFontSize(AxisBorder * pBorder, unsigned short size);
void Axis_SetTickLabelsFontSize(Axis * pAxis, int size, int width = 0);

Axis * Axis_Create(HWND hWnd, DWORD options = AXIS_OPT_DEFAULT, wchar_t * title = L"",
				   int style = WS_OVERLAPPEDWINDOW, bool managed = true, Axis * copyFrom = NULL);

Axis * Axis_Create(Axis * pAxis, DWORD options = AXIS_OPT_DEFAULT, wchar_t * title = L"",
				   int style = WS_OVERLAPPEDWINDOW, bool managed = true, Axis * copyFrom = NULL);

Axis * Axis_CreateAt(double nx, double ny, double dx, double dy, 
					 Axis * pAxis, DWORD options = AXIS_OPT_DEFAULT,
					 wchar_t * title = L"", int style = WS_OVERLAPPEDWINDOW, 
					 bool managed = true, Axis * copyFrom = NULL/*,
					 bool predraw = true*/);

Axis * Axis_CreateAt(double nx, double ny, double dx, double dy, 
					 HWND hWnd = NULL, DWORD options = AXIS_OPT_DEFAULT, 
					 wchar_t * title = L"", int style = WS_OVERLAPPEDWINDOW,
					 bool managed = 1, Axis * copyFrom = NULL, 
					 bool createWindowForAxisIfNeeded = true/*,
					 bool predraw = true*/);

void Axis_AddSerie_Raw(Axis * pAxis, 
				   const wchar_t * pName, 
				   const double * x,
				   const double * y, size_t n,
				   COLORREF color = RGB(0,0,0), 
				   DWORD penStyle = PS_SOLID, 
				   unsigned short lineWidth = 1,
				   DWORD markerType = AXIS_MARKER_NONE,
				   COLORREF markerColor = AXIS_COLOR_DEFAULT,
				   unsigned short marker_nPixel = 4,
				   unsigned short markerLineWidth = 1);

void Axis_Delete(Axis* pAxis);
bool Axis_Destroy(Axis * pAxis);
void Axis_Release(Axis * pAxis);
void Axis_SetDrawCallback(Axis * pAxis, 
					  AxisDrawCallback callback, 
					  AxisDrawCallback CleanUpCallback = NULL);
void Axis_SetStatusTextCallback(Axis * pAxis,
							AxisDrawCallback callback);
void Axis_Refresh     (Axis * pAxis, bool autoFit = false, bool toMetaFile = false, bool redraw = true, bool subAxisRecursion = false);
void Axis_RefreshAsync(Axis * pAxis, bool autoFit = false, bool toMetaFile = false, bool redraw = true, bool cancelPreviousJob = false);
DWORD WINAPI RefreshWorkerThread(LPVOID pParams);
void Axis_MoveToEx(Axis * pAxis, double x, double y);
void Axis_LineTo(Axis * pAxis, double x, double y);
void Axis_CloseAllWindows(bool terminating = false);
void Axis_CloseWindow(Axis * pAxis);
void Axis_CloseWindow(HWND hWnd);
void AxisFont_Destroy(AxisFont * pFont);
void Axis_DestroyLegend(Axis *pAxis);
void AxisBorder_Destroy(AxisBorder * & pBorder);
void Axis_FreeGdiRessources(Axis * pAxis);
Axis ** Axis_GetAxisPtrVect(HWND hWnd);
void Axis_Redraw(Axis * pAxis, HDC hdcPaint = NULL, bool redrawLegend = true);
HDC Axis_GetHdc(Axis * pAxis);
void AxisSerieMarker_Destroy(AxisSerieMarker * & pMarker);
void Axis_AutoFit(Axis * pAxis, bool colorAutoFit = true, bool forceAdjustZoom = false);
void AxisMatrixView_CreateColorMap(AxisMatrixView * pMatrixView, DWORD options);

AxisSerieMarker * AxisSerieMarker_Create(Axis * pAxis, size_t iSerie,
	DWORD markerType, COLORREF color, 
	unsigned short nPixel, unsigned short lineWidth);

double Axis_xMin(Axis * pAxis, bool orig = true);
double Axis_xMax(Axis * pAxis, bool orig = true);
double Axis_yMin(Axis * pAxis, bool orig = true);
double Axis_yMax(Axis * pAxis, bool orig = true);

void Axis_xLim(Axis * hAxis, double xMin, double xMax, bool forceAdjustZoom = false);
void Axis_yLim(Axis * hAxis, double yMin, double yMax, bool forceAdjustZoom = false);
void Axis_yMin(Axis * pAxis, double yMin, bool forceAdjustZoom = false);
void Axis_yMax(Axis * pAxis, double yMax, bool forceAdjustZoom = false);
void Axis_xMin(Axis * pAxis, double xMin, bool forceAdjustZoom = false);
void Axis_xMax(Axis * pAxis, double xMax, bool forceAdjustZoom = false);
void Axis_Export2Excel(Axis * pAxis, bool toClipboard, unsigned __int64 * pOnlyThisOne = nullptr, bool coalesceAbscissa = false);

HWND CreateManagedAxisWindow(
	wchar_t * strTitle, 
	double x = CW_USEDEFAULT, double y = CW_USEDEFAULT, 
	double nX = CW_USEDEFAULT, double nY = CW_USEDEFAULT,
	DWORD options = AXIS_OPT_ADIM_MODE,
	DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE,
	HWND parent = NULL,
	HMENU menu = NULL,
	void * pData = NULL,
	Axis * copyFrom = NULL, 
	bool empty = false);

void Axis_EndOfTimes(bool terminating);
void Axis_DestroyAsync(Axis * pAxis);

//Macros
#define AXIS_SAFE_ACCESS(a) {g_axisArray.EnterCriticalSection(); (a) ; g_axisArray.LeaveCriticalSection();}

#endif

//Struct bodies
struct AxisFont {
	HFONT  hFont;
	LOGFONTW * pLogFont;
} /*AxisFont_ZeroInitializer*/;

struct AxisTicks {
	AxisBorder * pBorder;
	double deltaTicks;
	double deltaTicks_pix;
	double firstTick;
	double minDeltaTicks_pix;
	short unsigned int maxTicks;
	short unsigned int tickHeight;
	short unsigned int minorTickHeight;
	DWORD options;
	int deltaLabel;
	SIZE maxLabelExtent;
};

struct AxisBorder {
	Axis * pAxis;
	AxisTicks * pTicks;
	AxisFont font;
	HPEN hCurrentPen;
	wchar_t * pText;
	DWORD textAlignment;
	bool isVertical;
	int extent;
	unsigned short marge;
	unsigned short marge2;
	RECT rect;
};

struct AxisLegend {
	Axis * pAxis;
	HPEN   hCurrentPen;
	wchar_t * pText;
	AxisFont font;
	DWORD textAlignment;
	bool isVertical;
	unsigned short marge;
	unsigned short lineLength;
	RECT rect;
	double adim_top;
	double adim_left;
};

struct AxisSerie {
	Axis * pAxis;
	size_t iSerie;
	unsigned __int64 id;
	wchar_t * pName;
	size_t n;
	bool doublePrec;
	union {
		double * xDouble;
		float *  xSingle;
	};
	union {
		double * yDouble;
		float  * ySingle;
	};

	LOGPEN pen;
	AxisSerieMarker * pMarker;

	bool drawLines;
};

struct AxisColorMap {
	AxisMatrixView * pMatView;
	double vMin, vMax, vMin_orig, vMax_orig;
	double * vr;
	double * v;
	COLORREF * c;
	Axis * legendCmap;
};

struct AxisMatrixView {
	Axis * pAxis;
	size_t nRow, nCol;
	double * xCoord;
	double * yCoord;
	union {
		double * mDouble;
		float  * mSingle;
	};
	AxisColorMap * cMap;
	DWORD options;
} /*AxisMatrixView_ZeroInitializer*/;

struct AxisSerieMarker {
	Axis * pAxis;
	size_t iSerie;
	DWORD markerType;
	COLORREF color;
	unsigned short lineWidth;
	unsigned short nPixel;
};

struct AdimPos {
	bool initialized;
	double nX, nY, dx, dy;
};

struct persistentMatrixViewSettings {
	AdimPos legendAdimPos;
	DWORD options;
	double cMap_vMin, cMap_vMax;
	double cMap_vMin_orig, cMap_vMax_orig;
};

struct Axis {

	size_t id;
	HWND hWnd;
	int lockCount;
	HWND hWndAxisMgrWinProc;
	DWORD options;

	Axis ** subAxis;

	HDC hdc;
	HDC hdcDraw;
	HDC hdcErase;
	HBITMAP hBitmapErase;
	HBITMAP hBitmapDraw;
	void * bitmapDrawPtr;
	size_t bitmapDrawCount;
	HBRUSH hBackGroundBrush;
	HBRUSH hCurrentBrush;
	HPEN   hCurrentPen;
	COLORREF BackGroundColor;

	AxisFont tickLabelFont;
	
	void ** callbacksData;
	CRITICAL_SECTION * pCs;
	AxisLegend * legend;
	AxisBorder * pX_Axis;
	AxisBorder * pY_Axis;
	AxisBorder * pY_Axis2;
	AxisBorder * pTitle;
	AxisBorder * pStatusBar;
	AxisSerie  * pSeries;
	AxisMatrixView * pMatrixView;
	
	RECT PlotArea;

	int DrawArea_nPixels_x;
	int DrawArea_nPixels_y;
	int DrawArea_nPixels_dec_x;
	int DrawArea_nPixels_dec_y;
	
	double CoordTransf_xFact;
	double CoordTransf_yFact;
	double CoordTransf_dx;
	double CoordTransf_dy;

	double xMin_orig;
	double xMax_orig;
	double yMin_orig;
	double yMax_orig;

	double xMin;
	double yMin;
	double xMax;
	double yMax;

	double adim_nX;
	double adim_nY;
	double adim_dx;
	double adim_dy;
	
	//AdimPos previousMatrixViewLegendAdimPos;
	persistentMatrixViewSettings previousMatViewSettings;

	AxisDrawCallback drawCallback;
	AxisDrawCallback callbacksCleanUp;
	AxisStatusTextCallback statusTextCallback;
	AxisCheckZoomCallback  checkZoomCallback;

	int drawCallbackOrder;
	//LRESULT CALLBACK (*SubWindowProc)(HWND , UINT , WPARAM , LPARAM );
};

struct AxisWindowParam{
	wchar_t * className; 
	wchar_t * strTitle;
	double x;
	double y;
	double nX;
	double nY;
	DWORD style;
	HWND parent;
	DWORD options;
	HMENU menu;
	void * pData;
	HANDLE hEventWindowCreated;
	HANDLE hEventThreadCreated;
	HWND result_hWnd;
	Axis * copyFrom;
};

