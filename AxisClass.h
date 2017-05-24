class AxisFontClass;
//class AxisFontClass_xlabel;

typedef Axis * AxisHandle;

class AxisClass {

private:

	AxisHandle hAxis;
	bool owner;
	bool Check_hAxis(char * func);

public:

	//AxisClass() {}
	AxisClass(const AxisClass &) = delete;

	AxisClass(AxisClass && that) {
		this->hAxis = that.hAxis;
		this->owner = that.owner;
		that.hAxis = nullptr;
		that.owner = false;
	}

	AxisClass& operator=(AxisClass && that) {
		this->~AxisClass();
		new (this) AxisClass(std::move(that));
		return *this;
	}

	explicit operator bool() const;
	//explicit operator AxisHandle();
	AxisFontClass SetFont(void);
	AxisClass& SetFontSize(int fontSize);
	AxisClass& SetFontName(wchar_t * fontName);
	AxisHandle GetHandle(bool giveUpOwnership = false);
	AxisClass& ShowLegend(void);
	AxisClass& HideLegend(void);
	void Detach(void);
	AxisClass(void);
	AxisClass(AxisHandle axisHdl, bool takeOwnership = false);
	~AxisClass(void);
	AxisClass& Wrap(AxisHandle axisHdl, bool takeOwnership = false);
	AxisClass& Create(AxisClass * pAxisClassParent = NULL,
		DWORD options = AXIS_OPT_DEFAULT, wchar_t * title = L"",
		int style = WS_OVERLAPPEDWINDOW, bool managed = true);
	AxisClass& Create(nullptr_t,
		DWORD options = AXIS_OPT_DEFAULT, wchar_t * title = L"",
		int style = WS_OVERLAPPEDWINDOW, bool managed = true);
	AxisClass& Create(AxisClass & axisClassParent,
		DWORD options = AXIS_OPT_DEFAULT, wchar_t * title = L"",
		int style = WS_OVERLAPPEDWINDOW, bool managed = true);
	AxisClass& Create(HWND hWndParent,
		DWORD options = AXIS_OPT_DEFAULT, wchar_t * title = L"",
		int style = WS_OVERLAPPEDWINDOW, bool managed = true);
	AxisClass& CreateAt(double nx, double ny, double dx, double dy,
		HWND hWnd, DWORD options = AXIS_OPT_DEFAULT,
		wchar_t * title = L"", int style = WS_OVERLAPPEDWINDOW, bool managed = true);
	AxisClass& CreateAt(double nx, double ny, double dx, double dy,
		AxisClass * pAxisClassParent = NULL, DWORD options = AXIS_OPT_DEFAULT,
		wchar_t * title = L"", int style = WS_OVERLAPPEDWINDOW, bool managed = true);
	AxisClass& CreateAt(double nx, double ny, double dx, double dy,
		nullptr_t, DWORD options = AXIS_OPT_DEFAULT,
		wchar_t * title = L"", int style = WS_OVERLAPPEDWINDOW, bool managed = true);
	AxisClass& CreateAt(double nx, double ny, double dx, double dy,
		AxisClass & axisClassParent, DWORD options = AXIS_OPT_DEFAULT,
		wchar_t * title = L"", int style = WS_OVERLAPPEDWINDOW, bool managed = true);
	template <bool adim, class T>
	AxisClass& CreateAt(double nx, double ny, double dx, double dy, 
		T t, DWORD options = AXIS_OPT_DEFAULT,
		wchar_t * title = L"", int style = WS_OVERLAPPEDWINDOW, bool managed = true)
	{
		if (adim) options |= AXIS_OPT_ADIM_MODE;
		else options &= ~AXIS_OPT_ADIM_MODE;
		return this->CreateAt(nx, ny, dx, dy, t, options, title, style, managed);
	}
	template <bool adim>
	AxisClass& CreateAt(double nx, double ny, double dx, double dy)
	{
		return this->CreateAt<adim>(nx, ny, dx, dy, nullptr);
	}

	AxisClass& SetColorMap(int options);
	AxisClass& CreateMatrixView();
	AxisClass& CreateMatrixView(float * m, size_t nRow, size_t nCol);
	AxisClass& CreateMatrixView(double * m, size_t nRow, size_t nCol);
	AxisClass& CreateMatrixView(aMat<float> & m,  bool setMatrixOrientation = true);
	AxisClass& CreateMatrixView(aMat<double> & m, bool setMatrixOrientation = true);
	AxisClass& CreateMatrixView(aPlan<double> & m);
	AxisClass& CreateMatrixView(aPlan<float> & m);
	AxisClass& CreateMatrixView(bTens<double> & m);
	AxisClass& CreateMatrixView(bTens<float> & m);
	AxisClass& SetMatrixViewOrientation(bool matrixOriented);
	bool Exist(void);
	AxisClass& DrawRectangle(double left, double top, double right, double bottom);
	AxisClass& DrawEllipse(double left, double top, double right, double bottom);
	AxisClass& MoveTo(double x, double y);
	AxisClass& LineTo(double x, double y);
	void Delete(void);
	void Release(void);
	AxisClass& SetDrawCallback(
		AxisDrawCallback drawCallback,
		AxisDrawCallback callbacksCleanupCallback = NULL);
	AxisClass& SetCheckZoomCallback(AxisCheckZoomCallback checkZoomCallback);
	AxisClass& SetCallbacksCleanUpCallback(
		AxisDrawCallback callbacksCleanupCallback);
	AxisClass& SetCallbacksData(void * callbacksData);
	AxisClass& PushCallbacksData(void * callbacksData);
	AxisClass& Move(double nx, double ny, double dx, double dy);
	AxisClass& SetTitle(const wchar_t * title);
	AxisClass& xLabel(const wchar_t * label);
	AxisClass& yLabel(const wchar_t * label);
	AxisClass& PolyLine(double * x, double * y, size_t n);
	bool PlotDataWaiting(void);
	AxisClass& Refresh(bool autoFit = false, bool Redraw = true);
	AxisClass& RefreshAsync(bool autoFit = false, bool Redraw = true, bool cancelPreviousJobs = false);
	AxisClass& SetZoomed();
	AxisClass& BringOnTop();
	AxisClass& Enable(bool enable = true);
	AxisClass& EnforceSquareZoom(bool enable = true);
	void AlwaysOnTop(void);
	void CloseWindow(void);
	
	AxisClass& AddSerie_Raw(
		const wchar_t * pName, const double * x, const double * y, size_t n,
		COLORREF color = RGB(0, 0, 0), DWORD penStyle = PS_SOLID,
		int lineWidth = 1, DWORD markerType = AXIS_MARKER_NONE,
		COLORREF markerColor = AXIS_COLOR_DEFAULT,
		unsigned short marker_nPixel = 4,
		unsigned short markerLineWidth = 1);

	AxisClass& AddSerie_Raw(
		const wchar_t * pName, const float * x, const float * y, size_t n,
		COLORREF color = RGB(0, 0, 0), DWORD penStyle = PS_SOLID,
		int lineWidth = 1, DWORD markerType = AXIS_MARKER_NONE,
		COLORREF markerColor = AXIS_COLOR_DEFAULT,
		unsigned short marker_nPixel = 4,
		unsigned short markerLineWidth = 1);

	int AddSubAxisSerie_Raw(
		const wchar_t * pSubAxisName, const wchar_t * pSerieName,
		const double * x, const double * y, size_t n,
		COLORREF color = RGB(0, 0, 0), DWORD penStyle = PS_SOLID,
		int lineWidth = 1, DWORD markerType = AXIS_MARKER_NONE,
		COLORREF markerColor = AXIS_COLOR_DEFAULT,
		unsigned short marker_nPixel = 4,
		unsigned short markerLineWidth = 1);

	int AddSubAxisSerie_Raw(
		const wchar_t * pSubAxisName, const wchar_t * pSerieName,
		const float * x, const float * y, size_t n,
		COLORREF color = RGB(0, 0, 0), DWORD penStyle = PS_SOLID,
		int lineWidth = 1, DWORD markerType = AXIS_MARKER_NONE,
		COLORREF markerColor = AXIS_COLOR_DEFAULT,
		unsigned short marker_nPixel = 4,
		unsigned short markerLineWidth = 1);


	template <class U> AxisClass& AddSerie(
		const wchar_t * pName, const aVect<U> & x, const aVect<U> & y,
		COLORREF color = RGB(0, 0, 0), DWORD penStyle = PS_SOLID,
		int lineWidth = 1, DWORD markerType = AXIS_MARKER_NONE,
		COLORREF markerColor = AXIS_COLOR_DEFAULT,
		unsigned short marker_nPixel = 4,
		unsigned short markerLineWidth = 1);

	template <class U> AxisClass& AddSerie(
		const aVect<U> & x, const aVect<U> & y,
		COLORREF color = RGB(0, 0, 0), DWORD penStyle = PS_SOLID,
		int lineWidth = 1, DWORD markerType = AXIS_MARKER_NONE,
		COLORREF markerColor = AXIS_COLOR_DEFAULT,
		unsigned short marker_nPixel = 4,
		unsigned short markerLineWidth = 1);

	template <class U> int AddSubAxisSerie(
		const wchar_t * pSubAxisName, const wchar_t * pSerieName,
		const aVect<U> & x, const aVect<U> & y,
		COLORREF color = RGB(0, 0, 0), DWORD penStyle = PS_SOLID,
		int lineWidth = 1, DWORD markerType = AXIS_MARKER_NONE,
		COLORREF markerColor = AXIS_COLOR_DEFAULT,
		unsigned short marker_nPixel = 4,
		unsigned short markerLineWidth = 1);

	template <class U> int AddSubAxisSerie(
		const wchar_t * pSubAxisName,
		const aVect<U> & x, const aVect<U> & y,
		COLORREF color = RGB(0, 0, 0), DWORD penStyle = PS_SOLID,
		int lineWidth = 1, DWORD markerType = AXIS_MARKER_NONE,
		COLORREF markerColor = AXIS_COLOR_DEFAULT,
		unsigned short marker_nPixel = 4,
		unsigned short markerLineWidth = 1);

	AxisClass& AutoFit(bool colorAutoFit = true, bool forceAdjustZoom = false);
	AxisClass& Clear(void);
	AxisClass& xLim(double xMin, double xMax, bool forceAdjustZoom = false);
	AxisClass& yLim(double yMin, double yMax, bool forceAdjustZoom = false, int iAxis = 0);
	AxisClass& cLim(double cMin, double cMax);
	AxisClass& yMin(double yMin, bool forceAdjustZoom = false);
	AxisClass& yMax(double yMax, bool forceAdjustZoom = false);
	AxisClass& xMin(double xMin, bool forceAdjustZoom = false);
	AxisClass& xMax(double xMax, bool forceAdjustZoom = false);
	AxisClass& cMin(double cMin);
	AxisClass& cMax(double cMax);
	double yMin(bool orig = true, int iAxis = 0);
	double yMax(bool orig = true, int iAxis = 0);
	double xMin(bool orig = true);
	double xMax(bool orig = true);
	double cMin(bool orig = true);
	double cMax(bool orig = true);
	AxisClass& SetDrawCallbackOrder(int order = 0);
	int GetDrawCallbackOrder();
	AxisClass& BeginDraw(void);
	AxisClass& EndDraw(void);
	AxisClass& Redraw(void);
	AxisClass& SetBrush(HBRUSH hBrush);
	AxisClass& SetPen(HPEN hPen);
	AxisClass& SetPenColor(COLORREF color);
	AxisClass& SetBrushColor(COLORREF color);
	AxisClass& SetPen(COLORREF color, int linewidth);
	AxisClass& Init(void);
	AxisClass& ToClipboard(void);
	HWND hWnd(void);
	AxisDrawCallback GetDrawCallback(void);
	AxisClass& ClearSeries(void);
	AxisClass& SetStatusTextCallback(AxisStatusTextCallback statusTextCallback);
	AxisStatusTextCallback GetStatusTextCallback();
	CRITICAL_SECTION * GetCriticalSection();

	AxisClass& SetMatrixViewCoord_X(const aVect<double> & coord_X);
	AxisClass& SetMatrixViewCoord_Y(const aVect<double> & coord_Y);
	AxisClass& SetLogScaleX(bool enable = true);
	AxisClass& SetLogScaleY(bool enable = true);
	AxisClass& SetLegendColorStatus(bool enable = true);

	AxisClass& SetTextColor(COLORREF color);
	AxisClass& SetBkColor(COLORREF color);
	AxisClass& SetBkMode(int mode);

	AxisClass& SaveImageToFile(const wchar_t * filePath, bool onlyLegend = false);

	AxisClass& Plot_Raw(
		const wchar_t * pName, const double * x, const double * y, size_t n,
		COLORREF color = RGB(0, 0, 0), DWORD penStyle = PS_SOLID,
		int lineWidth = 1, DWORD markerType = AXIS_MARKER_NONE,
		COLORREF markerColor = AXIS_COLOR_DEFAULT,
		unsigned short marker_nPixel = 4,
		unsigned short markerLineWidth = 1);

	AxisClass& Plot_Raw(
		const wchar_t * pName, const float * x, const float * y, size_t n,
		COLORREF color = RGB(0, 0, 0), DWORD penStyle = PS_SOLID,
		int lineWidth = 1, DWORD markerType = AXIS_MARKER_NONE,
		COLORREF markerColor = AXIS_COLOR_DEFAULT,
		unsigned short marker_nPixel = 4,
		unsigned short markerLineWidth = 1);

	template <class U> AxisClass& Plot(
		const wchar_t * pName, const aVect<U> & x, const aVect<U> & y,
		COLORREF color = RGB(0, 0, 0), DWORD penStyle = PS_SOLID,
		int lineWidth = 1, DWORD markerType = AXIS_MARKER_NONE,
		COLORREF markerColor = AXIS_COLOR_DEFAULT,
		unsigned short marker_nPixel = 4,
		unsigned short markerLineWidth = 1);

	template <class U> AxisClass& Plot(
		const aVect<U> & x, const aVect<U> & y,
		COLORREF color = RGB(0, 0, 0), DWORD penStyle = PS_SOLID,
		int lineWidth = 1, DWORD markerType = AXIS_MARKER_NONE,
		COLORREF markerColor = AXIS_COLOR_DEFAULT,
		unsigned short marker_nPixel = 4,
		unsigned short markerLineWidth = 1);

	double GetPixelSizeX(double x = 0);
	double GetPixelSizeY(double y = 0);

	AxisClass& SubAxis_yLim(int i, double yMin, double yMax, bool forceAdjustZoom = false);
	void SubAxis_yMax(int i, double yMax, bool orig = false);
	void SubAxis_yMin(int i, double yMin, bool orig = false);
	double SubAxis_yMax(int i, bool forceAdjustZoom = false);
	double SubAxis_yMin(int i, bool forceAdjustZoom = false);

};

class AxisFontClass_xlabel {

	//AxisHandle hAxis;
	AxisClass * pAxisClass;

public:

	//AxisFontClass_xlabel(AxisHandle h) {
	//	hAxis = h;
	//}

	AxisFontClass_xlabel(AxisClass * ptr) {
		pAxisClass = ptr;
	}

	AxisFontClass_xlabel& FontName(wchar_t * fontName) {
		Axis_SetxLabelFont(pAxisClass->GetHandle(), fontName, -1);
		return *this;
	}
	AxisFontClass_xlabel& FontSize(int size) {
		Axis_SetxLabelFont(pAxisClass->GetHandle(), NULL, size);
		return *this;
	}
};

class AxisFontClass_ylabel {

	//AxisHandle hAxis;
	AxisClass * pAxisClass;

public:

	//AxisFontClass_ylabel(AxisHandle h) {
	//	hAxis = h;
	//}

	AxisFontClass_ylabel(AxisClass * ptr) {
		pAxisClass = ptr;
	}

	AxisFontClass_ylabel& FontName(wchar_t * fontName) {
		Axis_SetyLabelFont(pAxisClass->GetHandle(), fontName, -1);
		return *this;
	}
	AxisFontClass_ylabel& FontSize(int size) {
		Axis_SetyLabelFont(pAxisClass->GetHandle(), NULL, size);
		return *this;
	}
};

class AxisFontClass_Title {

	//AxisHandle hAxis;
	AxisClass * pAxisClass;

public:

	//AxisFontClass_Title(AxisHandle h) {
	//	hAxis = h;
	//}

	AxisFontClass_Title(AxisClass * ptr) {
		pAxisClass = ptr;
	}

	AxisFontClass_Title& FontName(wchar_t * fontName) {
		Axis_SetTitleFont(pAxisClass->GetHandle(), fontName, -1);
		return *this;
	}
	AxisFontClass_Title& FontSize(int size) {
		Axis_SetTitleFont(pAxisClass->GetHandle(), NULL, size);
		return *this;
	}
};

class AxisFontClass_StatusBar {

	//AxisHandle hAxis;
	AxisClass * pAxisClass;

public:

	//AxisFontClass_StatusBar(AxisHandle h) {
	//	hAxis = h;
	//}

	AxisFontClass_StatusBar(AxisClass * ptr) {
		pAxisClass = ptr;
	}

	AxisFontClass_StatusBar& FontName(wchar_t * fontName) {
		Axis_SetStatusBarFont(pAxisClass->GetHandle(), fontName, -1);
		return *this;
	}

	AxisFontClass_StatusBar& FontSize(int size) {
		Axis_SetStatusBarFont(pAxisClass->GetHandle(), NULL, size);
		return *this;
	}
};


class AxisFontClass_Legend {

	//AxisHandle hAxis;
	AxisClass * pAxisClass;

public:

	//AxisFontClass_Legend(AxisHandle h) {
	//	hAxis = h;
	//}

	AxisFontClass_Legend(AxisClass * ptr) {
		pAxisClass = ptr;
	}

	AxisFontClass_Legend& FontName(wchar_t * fontName) {
		Axis_SetLegendFont(pAxisClass->GetHandle(), fontName, -1);
		return *this;
	}

	AxisFontClass_Legend& FontSize(int size) {
		Axis_SetLegendFont(pAxisClass->GetHandle(), NULL, size);
		return *this;
	}
};


class AxisFontClass_TickLabels {

	//AxisHandle hAxis;
	AxisClass * pAxisClass;

public:

	//AxisFontClass_xAxis(AxisHandle h) {
	//	hAxis = h;
	//}

	AxisFontClass_TickLabels(AxisClass * ptr) {
		pAxisClass = ptr;
	}

	AxisFontClass_TickLabels& FontName(wchar_t * fontName) {
		Axis_SetTickLabelsFont(pAxisClass->GetHandle(), fontName, -1);
		return *this;
	}

	AxisFontClass_TickLabels& FontSize(int size) {
		Axis_SetTickLabelsFont(pAxisClass->GetHandle(), NULL, size);
		return *this;
	}

	AxisClass& Axis() {
		return *pAxisClass;
	}
};

class AxisFontClass {

	//AxisHandle hAxis;
	AxisClass * pAxisClass;

public:

	//AxisFontClass(AxisHandle h) {
	//	hAxis = h;
	//}

	AxisFontClass(AxisClass * ptr) {
		pAxisClass = ptr;
	}

	AxisFontClass_xlabel xLabel(void) {
		AxisFontClass_xlabel c(pAxisClass);
		return c;
	}
	AxisFontClass_ylabel yLabel(void) {
		AxisFontClass_ylabel c(pAxisClass);
		return c;
	}
	AxisFontClass_Title Title(void) {
		AxisFontClass_Title c(pAxisClass);
		return c;
	}
	AxisFontClass_StatusBar StatusBar(void) {
		AxisFontClass_StatusBar c(pAxisClass);
		return c;
	}
	AxisFontClass_Legend Legend(void) {
		AxisFontClass_Legend c(pAxisClass);
		return c;
	}

	AxisFontClass_TickLabels TickLabels(void) {
		AxisFontClass_TickLabels c(pAxisClass);
		return c;
	}
};


