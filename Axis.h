
#ifndef _AXIS_
#define _AXIS_

#include "windows.h"
#include "stdio.h"
#include "xMat.h"
#include "MyUtils.h"
#include "WinUtils.h"

//#ifndef Min
//#define Min(a, b) ((a) < (b) ? (a) : (b))
//#endif

//#define BIT(n) ((long long int)1<<((n)-1)) 

//Axis option s
#define AXIS_OPT_WINDOWED		 BIT(1)
#define AXIS_OPT_ADIM_MODE		 BIT(2)
#define AXIS_OPT_GHOST			 BIT(3)
#define AXIS_OPT_USERGHOST		 BIT(4)
#define AXIS_OPT_STATUSPOS		 BIT(5)
#define AXIS_OPT_ZEROLINES		 BIT(6)
#define AXIS_OPT_LEGEND			 BIT(7)
#define AXIS_OPT_DISABLED		 BIT(8)
#define AXIS_OPT_LEFTBORDER		 BIT(9)
#define AXIS_OPT_RIGHTBORDER	 BIT(10)
#define AXIS_OPT_TOPBORDER		 BIT(11)
#define AXIS_OPT_BOTTOMBORDER	 BIT(12)
#define AXIS_OPT_MAINHDC		 BIT(13)
#define AXIS_OPT_ISMATVIEWLEGEND BIT(14)
#define AXIS_OPT_ISORPHAN        BIT(15)
#define AXIS_OPT_CREATEINVISIBLE BIT(16)

#define AXIS_OPT_BORDERS		 (AXIS_OPT_LEFTBORDER | AXIS_OPT_RIGHTBORDER | \
								  AXIS_OPT_TOPBORDER  | AXIS_OPT_BOTTOMBORDER)
#define AXIS_OPT_EMPTY			 (AXIS_OPT_GHOST | AXIS_OPT_WINDOWED | AXIS_OPT_USERGHOST)
#define AXIS_OPT_CONTAINER		 (AXIS_OPT_EMPTY | AXIS_OPT_MAINHDC)
#define AXIS_OPT_DEFAULT		 (AXIS_OPT_WINDOWED  | AXIS_OPT_ADIM_MODE |		\
								  AXIS_OPT_STATUSPOS | AXIS_OPT_ZEROLINES |		\
								  AXIS_OPT_LEGEND    | AXIS_OPT_BORDERS)


#define AXIS_COLOR_DEFAULT ((COLORREF)0xFF000000)


#define AXIS_MARKER_NONE		   0	//TODO
#define AXIS_MARKER_CROSS		   1	//TODO
#define AXIS_MARKER_DIAGCROSS      2
#define AXIS_MARKER_POINT          3	//TODO
#define AXIS_MARKER_CIRCLE         4	//TODO
#define AXIS_MARKER_QUARE          5	//TODO
#define AXIS_MARKER_FULLSQUARE     6	//TODO
#define AXIS_MARKER_STAR           7	//TODO
#define AXIS_MARKER_FULLSTAR       8	//TODO
#define AXIS_MARKER_LOSANGE        9	//TODO
#define AXIS_MARKER_FULLLOSANGE   10	//TODO

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

#define AXIS_ALIGN_XLEFT     BIT(1)
#define AXIS_ALIGN_XCENTER   BIT(2)
#define AXIS_ALIGN_XRIGHT    BIT(3)
#define AXIS_ALIGN_YTOP      BIT(4)
#define AXIS_ALIGN_YCENTER	 BIT(5)
#define AXIS_ALIGN_YBOTTOM   BIT(6)

#define AXIS_MSG_RCLICK              (WM_APP + 1)
#define AXIS_MSG_LCLICK              (WM_APP + 2)

struct AxisMsg_Point {
	double x, y;
};

#define Axis_Critical_Section(ax) if (ScopeCriticalSection<CRITICAL_SECTION*> __scs = ax.GetCriticalSection())

COLORREF AxisDefaultColor(size_t i);

//Struct declaration
struct Axis;
struct AxisSerie;

//Typdefs
typedef Axis * AxisHandle;
typedef void (*AxisDrawCallback) (AxisHandle axis, void * callbacksData);
typedef bool (*AxisStatusTextCallback)(wchar_t*, size_t, size_t, const wchar_t*, double, double, void*);
typedef void (*AxisCheckZoomCallback)(AxisHandle axis, bool & cancel);

//Functions
AxisHandle Axis_Create(HWND hWnd = NULL, DWORD options = AXIS_OPT_DEFAULT, wchar_t * title = L"",
				   int style = 0, bool managed = true, AxisHandle copyFrom = NULL);

AxisHandle Axis_Create(AxisHandle hAxis, DWORD options = AXIS_OPT_DEFAULT, wchar_t * title = L"",
				   int style = 0, bool managed = true, AxisHandle copyFrom = NULL);

AxisHandle Axis_CreateAt(double nx, double ny, double dx, double dy, 
					 AxisHandle hAxis, DWORD options = AXIS_OPT_DEFAULT,
					 wchar_t * title = L"", int style = WS_OVERLAPPEDWINDOW, 
					 bool managed = true, AxisHandle copyFrom = NULL) ;

AxisHandle Axis_CreateAt(double nx, double ny, double dx, double dy, 
					 HWND hWnd = NULL, DWORD options = AXIS_OPT_DEFAULT, 
					 wchar_t * title = L"", int style = WS_OVERLAPPEDWINDOW,
					 bool managed = 1, AxisHandle copyFrom = NULL, 
					 bool createWindowForAxisIfNeeded = true);

void Axis_SetDrawCallback(AxisHandle hAxis, 
					  AxisDrawCallback callback, 
					  AxisDrawCallback cleanUpCallback = NULL);

void Axis_SetCheckZoomCallback(Axis * pAxis, AxisCheckZoomCallback callback);

void Axis_SetCallbacksCleanUpCallback(AxisHandle pAxis, AxisDrawCallback callback);

void Axis_SetStatusTextCallback(AxisHandle hAxis, 
							AxisStatusTextCallback callback);

AxisStatusTextCallback Axis_GetStatusTextCallback(Axis * pAxis);

void Axis_AddSerie(AxisHandle pAxis, 
	const wchar_t * pName,
	const aVect<float> & x,
	const aVect<float> & y, 
	COLORREF color, 
	DWORD penStyle, 
	unsigned short lineWidth,
	DWORD markerType, 
	COLORREF markerColor, 
	unsigned short marker_nPixel,
	unsigned short markerLineWidth);

void Axis_AddSerie(AxisHandle pAxis,
	const wchar_t * pName,
	const aVect<double> & x,
	const aVect<double> & y,
	COLORREF color,
	DWORD penStyle,
	unsigned short lineWidth,
	DWORD markerType,
	COLORREF markerColor,
	unsigned short marker_nPixel,
	unsigned short markerLineWidth);

void Axis_AddSerie_Raw(AxisHandle hAxis,
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

void Axis_AddSerie_Raw(AxisHandle hAxis,
	const wchar_t * pName,
	const float * x,
	const float * y, size_t n,
	COLORREF color = RGB(0,0,0), 
	DWORD penStyle = PS_SOLID, 
	unsigned short lineWidth = 1,
	DWORD markerType = AXIS_MARKER_NONE,
	COLORREF markerColor = AXIS_COLOR_DEFAULT,
	unsigned short marker_nPixel = 4, 
	unsigned short markerLineWidth = 1);

int Axis_AddSubAxisSerie_Raw(
	Axis * pAxis, 
	const wchar_t * pSubAxisName,
	const wchar_t * pSerieName,
	const double * x, 
	const double * y,
	size_t n, 
	COLORREF color = RGB(0,0,0),
	DWORD penStyle = PS_SOLID, 
	unsigned short lineWidth = 1,
	DWORD markerType = AXIS_MARKER_NONE, 
	COLORREF markerColor = AXIS_COLOR_DEFAULT,
	unsigned short marker_nPixel = 4,
	unsigned short markerLineWidth = 1);

int Axis_AddSubAxisSerie_Raw(
	Axis * pAxis,
	const wchar_t * pSubAxisName,
	const wchar_t * pSerieName,
	const float * x,
	const float * y,
	size_t n,
	COLORREF color = RGB(0, 0, 0),
	DWORD penStyle = PS_SOLID,
	unsigned short lineWidth = 1,
	DWORD markerType = AXIS_MARKER_NONE,
	COLORREF markerColor = AXIS_COLOR_DEFAULT,
	unsigned short marker_nPixel = 4,
	unsigned short markerLineWidth = 1);

int Axis_AddSubAxisSerie(
	Axis * pAxis,
	const wchar_t * pSubAxisName,
	const wchar_t * pSerieName,
	const aVect<double>& x,
	const aVect<double>& y,
	COLORREF color = RGB(0, 0, 0),
	DWORD penStyle = PS_SOLID,
	unsigned short lineWidth = 1,
	DWORD markerType = AXIS_MARKER_NONE,
	COLORREF markerColor = AXIS_COLOR_DEFAULT,
	unsigned short marker_nPixel = 4,
	unsigned short markerLineWidth = 1);

int Axis_AddSubAxisSerie(
	Axis * pAxis,
	const wchar_t * pSubAxisName,
	const wchar_t * pSerieName,
	const aVect<float>& x,
	const aVect<float>& y,
	COLORREF color = RGB(0, 0, 0),
	DWORD penStyle = PS_SOLID,
	unsigned short lineWidth = 1,
	DWORD markerType = AXIS_MARKER_NONE,
	COLORREF markerColor = AXIS_COLOR_DEFAULT,
	unsigned short marker_nPixel = 4,
	unsigned short markerLineWidth = 1);

void Axis_DrawEllipse(AxisHandle hAxis, double left, double top, double right, double bottom);
void Axis_DrawRectangle(AxisHandle hAxis, double left, double top, double right, double bottom);
void Axis_Delete(AxisHandle hAxis);
void Axis_Release(AxisHandle hAxis);
void Axis_Move(AxisHandle hAxis, double nx, double ny, double dx, double dy);
void Axis_SetTitle(AxisHandle hAxis, const wchar_t * title);
void Axis_xLabel(AxisHandle hAxis, const wchar_t * label);
void Axis_yLabel(AxisHandle hAxis, const wchar_t * label);
void Axis_Refresh(AxisHandle hAxis, bool autoFit = false,  bool toMetaFile = false, bool redraw = true, bool subAxisRecursion = false);
void Axis_RefreshAsync(AxisHandle pAxis, bool autoFit = false, bool toMetaFile = false, bool redraw = true, bool cancelPreviousJob = false);
void Axis_DestroyAsync(AxisHandle pAxis);
void Axis_CloseWindow(AxisHandle hAxis);
void Axis_Clear(AxisHandle hAxis);
void Axis_xLim(AxisHandle hAxis, double xMin, double xMax, bool forceAdjustZoom = false);
void Axis_yLim(AxisHandle hAxis, double yMin, double yMax, bool forceAdjustZoom = false);
void Axis_yMin(AxisHandle pAxis, double yMin, bool forceAdjustZoom = false);
void Axis_yMax(AxisHandle pAxis, double yMax, bool forceAdjustZoom = false);
void Axis_xMin(AxisHandle pAxis, double xMin, bool forceAdjustZoom = false);
void Axis_xMax(AxisHandle pAxis, double xMax, bool forceAdjustZoom = false);
void Axis_cMin(AxisHandle pAxis, double cMin);
void Axis_cMax(AxisHandle pAxis, double cMax);
double Axis_xMin(AxisHandle hAxis, bool orig = true);
double Axis_xMax(AxisHandle hAxis, bool orig = true);
double Axis_yMin(AxisHandle hAxis, bool orig = true);
double Axis_yMax(AxisHandle hAxis, bool orig = true);
double Axis_cMin(AxisHandle hAxis, bool orig = true);
double Axis_cMax(AxisHandle hAxis, bool orig = true);
void Axis_SetTickLabelsFont(AxisHandle hAxis, wchar_t* fontName, int size, int width = 0);
void Axis_SetxLabelFont(AxisHandle  hAxis, wchar_t* fontName, int size);
void Axis_SetyLabelFont(AxisHandle  hAxis, wchar_t* fontName, int size);
void Axis_SetTitleFont(AxisHandle  hAxis, wchar_t* fontName, int size);
void Axis_SetStatusBarFont(AxisHandle  hAxis, wchar_t* fontName, int size);
void Axis_SetLegendFont(AxisHandle  hAxis, wchar_t* fontName, int size);
void Axis_InitBitmaps(AxisHandle hAxis);
void Axis_SelectPen(AxisHandle axis, HPEN hPen);
void Axis_AutoFit(AxisHandle hAxis, bool colorAutoFit = true, bool forceAdjustZoom = false);
void Axis_SelectBrush(AxisHandle axis, HBRUSH  hBrush);
AxisHandle* Axis_GetAxisPtrVect(HWND hWnd);
void Axis_SetBackGround(AxisHandle hAxis, COLORREF color);
void Axis_BeginDraw(AxisHandle hAxis);
void Axis_EndDraw(AxisHandle hAxis);
void Axis_Redraw(AxisHandle hAxis, HDC hdcPaint = NULL, bool redrawLegend = true);
void Axis_CopyMetaFileToClipBoard(AxisHandle hAxis);
bool Axis_Exist(AxisHandle hAxis);
void Axis_ClearSeries(AxisHandle hAxis);
void Axis_CloseAllWindows(bool terminating = false);
void Axis_EndOfTimes(bool terminating);
void Axis_Free(AxisHandle hAxis);
void Axis_SetOption(AxisHandle hAxis, DWORD option, bool val);
bool AxisTicks_GetOption(AxisHandle hAxis, DWORD option);
HWND Axis_GetHwnd(AxisHandle hAxis);
CRITICAL_SECTION * Axis_GetCriticalSection(Axis * pAxis);
bool Axis_CloseLastOpenedAxis();
void Axis_SetDrawCallbackOrder(AxisHandle pAxis, int order = 0);
int  Axis_GetDrawCallbackOrder(AxisHandle pAxis);


void Axis_LineTo(AxisHandle hAxis, double x, double y);
void Axis_MoveToEx(AxisHandle hAxis, double x, double y);
void Axis_PolyLine(AxisHandle hAxis, double * x, double * y, size_t n);
void Axis_GetCoord(AxisHandle hAxis, double * x, double * y);
HDC Axis_GetHdc(AxisHandle hAxis);
//long Axis_CoordTransf_X(AxisHandle hAxis, double x);
//long Axis_CoordTransf_Y(AxisHandle hAxis, double y);
//double Axis_CoordTransf_X_double(AxisHandle hAxis, double x);
//double Axis_CoordTransf_Y_double(AxisHandle hAxis, double y);
void Axis_CoordTransf(AxisHandle hAxis, double * x, double * y);
void Axis_CoordTransfDoubleToInt(Axis * axis, long * x_int, long * y_int, double x, double y);
AxisDrawCallback Axis_GetDrawCallback(AxisHandle hAxis);
void Axis_SetRefreshThreadNumber(int nThreads);
COLORREF Axis_SetPenColor(AxisHandle hAxis, COLORREF color);
COLORREF Axis_SetBrushColor(AxisHandle hAxis, COLORREF color);
void Axis_PushCallbacksData(AxisHandle hAxis, void * callbacksData);
bool Axis_CallbacksDataWaiting(AxisHandle hAxis);
void Axis_TextOut(AxisHandle hAxis, double left, double top, double right, double bottom, 
	              wchar_t * pText, int nText, DWORD textAlignment, int isVertical = false,
				  HFONT hFont = NULL, int isPixel = false);
void Axis_GetRect(AxisHandle pAxis, RECT * rect);

void Axis_SetColorMap(AxisHandle hAxis, DWORD options);
void Axis_CreateMatrixView(AxisHandle hAxis, double * m, size_t nRow, size_t nCol, DWORD options);
void Axis_CreateMatrixView(AxisHandle hAxis, float * m,  size_t nRow, size_t nCol, DWORD options);
void Axis_CreateMatrixView(AxisHandle hAxis, double * m, size_t nRow, size_t nCol);
void Axis_CreateMatrixView(AxisHandle hAxis, float * m,  size_t nRow, size_t nCol);
void Axis_SetMatrixViewColorMap(AxisHandle hAxis, double vMin, double vMax, int mapID);
void Axis_cLim(AxisHandle hAxis, double vMin, double vMax, bool forceAdjustZoom = false, bool zoom = false);
void Axis_CreateMatrixView(AxisHandle hAxis, double * m, size_t nRow, size_t nCol, DWORD options);
void Axis_SetMatrixViewOrientation(AxisHandle hAxis, bool matrixOrientation);
void Axis_SetCallbacksData(AxisHandle hAxis, void * callbacksData);
double Axis_GetCoordPixelSizeX(AxisHandle pAxis, double x);
double Axis_GetCoordPixelSizeY(AxisHandle pAxis, double y);
void * Axis_GetDrawCallbackData(AxisHandle  hAxis);
void * Axis_PopStartCallbacksData(AxisHandle  hAxis);
void Axis_SetZoomed(AxisHandle  hAxis);
void Axis_BringOnTop(AxisHandle hAxis);
void Axis_SetIcon(HICON hIcon, HICON hIconSmall = NULL);
 
void Axis_SetMatrixViewCoord_X(AxisHandle hAxis, const aVect<double> & coord_X);
void Axis_SetMatrixViewCoord_Y(AxisHandle hAxis, const aVect<double> & coord_Y);
void Axis_SetLogScaleX(AxisHandle axis, bool enable = true);
void Axis_SetLogScaleY(AxisHandle axis, bool enable = true);
void Axis_SetLegendColorStatus(AxisHandle axis, bool enable = true);

void Axis_SetTextColor(AxisHandle hAxis, COLORREF color);
void Axis_SetBkColor(AxisHandle hAxis, COLORREF color);
void Axis_SetBkMode(AxisHandle hAxis, int mode);

template <bool trueColor = true, class T>
COLORREF Axis_GetColorFromColorMap(AxisHandle hAxis, T mv);
void Axis_ResetZoom(AxisHandle hAxis);
void Axis_SquareZoom(AxisHandle hAxis);
void Axis_EnforceSquareZoom(AxisHandle hAxis, bool enable);
size_t Axis_GetID(AxisHandle hAxis);
void Axis_Zoom(AxisHandle hAxis,double xMin,double xMax,double yMin,double yMax);
void Axis_SaveImageToFile(AxisHandle hAxis, const wchar_t * filePath, bool onlyLegend = false);
void Axis_TestFunc(AxisHandle hAxis);
void Axis_SubAxis_yLim(AxisHandle pAxis, int i, double yMin, double yMax, bool forceAdjustZoom = false);
void Axis_SubAxis_yMax(Axis * pAxis, int i, double yMax, bool orig = false);
void Axis_SubAxis_yMin(Axis * pAxis, int i, double yMin, bool orig = false);
double Axis_SubAxis_yMax(Axis * pAxis, int i, bool forceAdjustZoom = false);
double Axis_SubAxis_yMin(Axis * pAxis, int i, bool forceAdjustZoom = false);
void Axis_Export2Excel(Axis * pAxis, bool toClipboard, unsigned __int64 * pOnlyThisOne = nullptr, bool coalesceAbscissa = false);

#include "AxisClass.h"

template <class U> AxisClass& AxisClass::AddSerie(
	const wchar_t * pName, const aVect<U> & x, const aVect<U> & y,
	COLORREF color, DWORD penStyle, int lineWidth, DWORD markerType,
	COLORREF markerColor, unsigned short marker_nPixel, unsigned short markerLineWidth) 
{

	if (Check_hAxis(__FUNCTION__)) {
		Axis_AddSerie(hAxis, pName, x, y, color, penStyle, lineWidth, 
			markerType, markerColor, marker_nPixel, markerLineWidth);
	}
	return *this;
}

template <class U> AxisClass& AxisClass::AddSerie(
	const aVect<U> & x, const aVect<U> & y,
	COLORREF color, DWORD penStyle, int lineWidth, DWORD markerType,
	COLORREF markerColor, unsigned short marker_nPixel, unsigned short markerLineWidth) 
{

	if (Check_hAxis(__FUNCTION__)) {
		Axis_AddSerie(hAxis, nullptr, x, y, color, penStyle, lineWidth, 
			markerType, markerColor, marker_nPixel, markerLineWidth);
	}
	return *this;
}


template <class U> int AxisClass::AddSubAxisSerie(
	const wchar_t * pSubAxisName, const wchar_t * pSerieName,
	const aVect<U> & x, const aVect<U> & y,
	COLORREF color, DWORD penStyle, int lineWidth, DWORD markerType,
	COLORREF markerColor, unsigned short marker_nPixel, unsigned short markerLineWidth)
{

	if (Check_hAxis(__FUNCTION__)) {
		return Axis_AddSubAxisSerie(hAxis, pSubAxisName, pSerieName, x, y, color, penStyle, lineWidth,
			markerType, markerColor, marker_nPixel, markerLineWidth);
	}
	return -1;
}

template <class U> int AxisClass::AddSubAxisSerie(
	const wchar_t * pSubAxisName,
	const aVect<U> & x, const aVect<U> & y,
	COLORREF color, DWORD penStyle, int lineWidth, DWORD markerType,
	COLORREF markerColor, unsigned short marker_nPixel, unsigned short markerLineWidth)
{

	if (Check_hAxis(__FUNCTION__)) {
		return Axis_AddSubAxisSerie(hAxis, pSubAxisName, nullptr, x, y, color, penStyle, lineWidth,
			markerType, markerColor, marker_nPixel, markerLineWidth);
	}
	return -1;
}

template <class U> AxisClass& AxisClass::Plot(
	const wchar_t * pName,
	const aVect<U> & x,
	const aVect<U> & y,
	COLORREF color, 
	DWORD penStyle,
	int lineWidth , 
	DWORD markerType,
	COLORREF markerColor,
	unsigned short marker_nPixel,
	unsigned short markerLineWidth) 
{
	if (!*this) this->Create();

	this->AddSerie(pName, x, y, color, penStyle, lineWidth, markerType, markerColor, marker_nPixel, markerLineWidth);
	this->AutoFit();
	this->Refresh();
	
	return *this;
}

template <class U> AxisClass& AxisClass::Plot(
	const aVect<U> & x,
	const aVect<U> & y,
	COLORREF color, 
	DWORD penStyle,
	int lineWidth,
	DWORD markerType,
	COLORREF markerColor,
	unsigned short marker_nPixel,
	unsigned short markerLineWidth) 
{
	
	if (!*this) this->Create();

	this->AddSerie(x, y, color, penStyle, lineWidth, markerType, markerColor, marker_nPixel, markerLineWidth);
	this->AutoFit();
	this->Refresh();

	return *this;
}

class ProgressBar {

	AxisClass axis;
	RECT r;
	aVect<wchar_t> buf, title;
	COLORREF color;
	bool interruptible;
	//bool hidden;
	bool managed;
	bool alwaysOnTop;

public:

	ProgressBar(
		const wchar_t * title = nullptr,
		bool interruptible = false,
		COLORREF color = RGB(100, 100, 255),
		bool managed = true,
		bool alwaysOnTop = false)
	:
		title(title), color(color), interruptible(interruptible), managed(managed), alwaysOnTop(alwaysOnTop) /*, hidden(false)*/
	{}

	ProgressBar(
		const char * title,
		bool interruptible = false,
		COLORREF color = RGB(100, 100, 255),
		bool managed = true,
		bool alwaysOnTop = false)
		:
		title(title), color(color), interruptible(interruptible), managed(managed), alwaysOnTop(alwaysOnTop) /*, hidden(false)*/
	{}

	explicit operator bool() {
		return axis.Exist();
	}

	ProgressBar & SetInterruptible(bool value) {
		this->interruptible = value;
		return *this;
	}

	ProgressBar & SetAlwaysOnTop(bool value) {
		this->alwaysOnTop = value;
		return *this;
	}

	ProgressBar & SetTitle(const wchar_t * title) {
		this->title = title;
		if (this->axis) this->axis.SetTitle(title);
		return *this;
	}

	ProgressBar & SetTitle(const char * title) {
		this->title = title;
		if (this->axis) this->axis.SetTitle(Wide(title));
		return *this;
	}

	HWND hWnd() {
		if (this->axis) return this->axis.hWnd();
		return NULL;
	}

	ProgressBar & Close() {
		this->axis.CloseWindow();
		this->axis.Release();
		return *this;
	}

	ProgressBar & Create(/*bool visible = true*/) {
		if (!this->axis) {
			auto style = this->interruptible ? WS_SYSMENU : WS_CAPTION;

			DWORD options = AXIS_OPT_DISABLED | AXIS_OPT_ADIM_MODE | AXIS_OPT_WINDOWED;
			//if (!visible) options |= AXIS_OPT_CREATEINVISIBLE;

			this->axis.CreateAt(0.15, 0.075, 0.4, 0.4, (HWND)NULL, options, this->title, style, this->managed);
			if (this->alwaysOnTop) this->axis.AlwaysOnTop();
			this->axis.SetPenColor(0).DrawRectangle(0.1, 0.9, 0.9, 0.1).Redraw();

			Axis_GetRect(this->axis.GetHandle(), &this->r);

			this->axis.SetPen(CreatePen(PS_SOLID, 1, 0));
			this->axis.BringOnTop(); 
			
			//this->hidden = !visible;
		}
		return *this;
	}

	//ProgressBar & Show() {
	//	if (this->axis && this->hidden) {
	//		ShowWindow(this->axis.hWnd(), SW_SHOW);
	//		this->hidden = false;
	//	}
	//	return *this;
	//}

	//ProgressBar & Hide() {
	//	if (this->axis && !this->hidden) {
	//		ShowWindow(this->axis.hWnd(), SW_HIDE);
	//		this->hidden = true;
	//	}
	//	return *this;
	//}

	ProgressBar & SetProgress(double completion) {

		//if (!this->axis) {
			this->Create();
		//} else {
		//	this->Show();
		//}

		if (completion > 1) completion = 1;

		if (!axis.Exist()) return *this;

		this->axis.SetBrush(CreateSolidBrush(RGB(255, 255, 255)));
		this->axis.DrawRectangle(0.1, 0.9, 0.9, 0.1);

		this->buf.sprintf(L"%.0f%%", 100 * completion);

		this->axis.SetBrush(CreateSolidBrush(this->color));
		this->axis.DrawRectangle(0.1, 0.9, 0.1 + completion*0.8, 0.1);

		SetBkMode(Axis_GetHdc(this->axis.GetHandle()), TRANSPARENT);

		Axis_TextOut(
			this->axis.GetHandle(),
			this->r.left,
			this->r.top,
			this->r.right,
			this->r.bottom,
			this->buf,
			(int)wcslen(this->buf),
			AXIS_ALIGN_XCENTER | AXIS_ALIGN_YCENTER,
			false, NULL, true);

		this->axis.Redraw();

		return *this;
	}

	~ProgressBar() {
		if (this->axis) this->axis.CloseWindow();
	}
};
//void rien(void) {
//	coucou.
//}



#endif
