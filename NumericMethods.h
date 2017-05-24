#ifndef _NumericMethods_
#define _NumericMethods_

#include <limits>
#include <stdexcept>
#include "Axis.h"
#include "xVect.h"
#include "Complex.h"

template <class Derived>
struct Base_Solver {

	double result;
	double y_target;
	double tol;
	size_t maxIter;
	double xInit;
	double xMin;
	double xMax;
	double residue;

	size_t nIter;

	bool isConverged = false;

	//AxisClass plot;
	AxisClass * pPlot;
	aVect<double> pt_x, pt_y, pt_y_target;

	Base_Solver(
		double y_target = 0,
		double tol = 1e-10,
		size_t maxIter = 1000,
		double xMin = -std::numeric_limits<double>::infinity(),
		double xMax = std::numeric_limits<double>::infinity(),
		double xInit = std::numeric_limits<double>::quiet_NaN())
	:
		y_target(y_target),
		tol(tol),
		maxIter(maxIter),
		xMin(xMin),
		xMax(xMax),
		xInit(xInit)
	{
		//this->pPlot = &this->plot;
		this->pPlot = nullptr;
	}

	Derived * This() {
		return (Derived*)this;
	}

	bool SetResults(double result, size_t nIter, double residue, bool isConverged) {
		this->result = result;
		this->nIter = nIter;
		this->residue = residue;
		return this->isConverged = isConverged;
	}

	operator double() {
		if (this->isConverged) return this->result;
		else return std::numeric_limits<double>::signaling_NaN();
	}
};


struct SecantMethod_Solver : Base_Solver<SecantMethod_Solver> {

	//double x_init;
	double relaxFactor1;
	size_t iterRelaxFactor2;
	double relaxFactor2;

	SecantMethod_Solver(
		double y_target = 0,
		double x_init = 0,
		double tol = 1e-10,
		size_t maxIter = 1000,
		double relaxFactor1 = 1,
		size_t iterRelaxFactor2 = 100,
		double relaxFactor2 = 0.1,
		double xMin = -std::numeric_limits<double>::infinity(),
		double xMax = std::numeric_limits<double>::infinity())
	:
		Base_Solver(y_target, tol, maxIter, xMin, xMax, x_init),
		relaxFactor1(relaxFactor1),
		iterRelaxFactor2(iterRelaxFactor2),
		relaxFactor2(relaxFactor2)
	{}

	template <bool plotConvergence = false, class T, class... Args >
	bool Solve(T & function, Args&&... args) {

		auto y_target = this->y_target;
		auto tol = this->tol;
		auto maxIter = this->maxIter;
		auto xMin = this->xMin;
		auto xMax = this->xMax;
		auto relaxFactor1 = this->relaxFactor1;
		auto relaxFactor2 = this->relaxFactor2;
		auto iterRelaxFactor2 = this->iterRelaxFactor2;

		auto x = this->xInit;
		auto y = function(x, std::forward<Args>(args)...);
		auto res = y - y_target;

		if (!_finite(y)) return this->SetResults(x, 0, res, false);

		if (plotConvergence) {
			pt_x.Push(x);
			pt_y.Push(y);
		}

		if (abs(res / (y_target ? y_target : 1)) < tol) {
			return this->SetResults(x, 0, res, true);
		}

		double dx = 0.01 * x;

		double y_of_x_plus_dx = function(x + dx, std::forward<Args>(args)...);

		if (!_finite(y_of_x_plus_dx)) return this->SetResults(x + dx, 0, res, false);

		if (plotConvergence) {
			this->pt_x.Push(x + dx);
			this->pt_y.Push(y_of_x_plus_dx);
			this->pt_y_target.Push(y_target);
			this->pPlot->Create().AddSerie(pt_x, pt_y, RGB(0, 0, 255), PS_SOLID, 1, AXIS_MARKER_DIAGCROSS).AddSerie(pt_x, pt_y_target)
				      .AutoFit().Refresh();
		}

		for (size_t nIter = 0; nIter < maxIter; nIter++) {

			double dy = y_of_x_plus_dx - y;
			double dres = dy;

			double delta_x = -dx * res / dres;

			delta_x *= nIter > iterRelaxFactor2 ? relaxFactor2 : relaxFactor1;

			double new_x = x + delta_x;

			if (new_x < xMin) {
				new_x = xMin;
				delta_x = (new_x - x);
			}
			else if (new_x > xMax) {
				new_x = xMax;
				delta_x = (new_x - x);
			}

			//now use the new x and old x as the 2 points to estimate the derivative

			dx = -delta_x;
			x = new_x;
			y_of_x_plus_dx = y;

			y = function(x, std::forward<Args>(args)...);
			if (!_finite(y)) return this->SetResults(x, nIter, res, false);

			if (plotConvergence) {
				this->pt_x.Push(x);
				this->pt_y.Push(y);
				this->pt_y_target.Push(y_target);
				this->pPlot->ClearSeries().AddSerie(pt_x, pt_y, RGB(0, 0, 255), PS_SOLID, 1, AXIS_MARKER_DIAGCROSS).AddSerie(pt_x, pt_y_target)
						  .AutoFit().Refresh();
				getchar();
			}

			res = y - y_target;

			if (abs(res / (y_target ? y_target : 1)) < tol) {
				return this->SetResults(x, nIter, res, true);
			}
		}

		return this->SetResults(x, maxIter, res, false);
	}
};


struct Dicotomy1D_Solver : Base_Solver<Dicotomy1D_Solver> {

private:
	int currentRecursion = 0;
public:

	int maxRecursion = 5;

	double y_xMin = std::numeric_limits<double>::quiet_NaN();
	double y_xMax = std::numeric_limits<double>::quiet_NaN();

	aVect<double> * p_pt_x;
	aVect<double> * p_pt_y;
	aVect<double> * p_pt_y_target;

	bool tolCheckX_Relative = true;
	bool tolCheckX = false;
	double tol_x = 1e-10;

	Dicotomy1D_Solver(
		double y_target = 0,
		double tol = 1e-10,
		size_t maxIter = 1000,
		double xMin = -std::numeric_limits<double>::infinity(),
		double xMax = std::numeric_limits<double>::infinity())
	:
		Base_Solver(y_target, tol, maxIter, xMin, xMax)
	{
		this->p_pt_x		= &this->pt_x;
		this->p_pt_y		= &this->pt_y;
		this->p_pt_y_target = &this->pt_y_target;
	}

	bool SetResults(double result, size_t nIter, double residue, bool isConverged) {
		//if (this->currentRecursion) this->currentRecursion = -1; nonono
		return Base_Solver::SetResults(result, nIter, residue, isConverged);
	}

	//template <bool plotConvergence = false, class T, class... Args >
	//bool Solve(T & function, Args&&... args) {
	template <class T, T & function, bool plotConvergence = false, bool strict = false, bool failSafe = false, class... Args >
	bool Solve(Args&&... args) {

		auto y_target = this->y_target;
		auto tol = this->tol;
		auto maxIter = this->maxIter;
		double x1 = this->xMin;
		double x2 = this->xMax;

		if (plotConvergence && !(*this->pPlot)) this->pPlot->Create();

		double y1;
		double y2;

		if (failSafe) {
			try {
				y1 = _finite(this->y_xMin) ? this->y_xMin : function(x1, std::forward<Args>(args)...);
			}
			catch (const std::runtime_error & e) {
				e;//happy compiler
				y1 = std::numeric_limits<double>::quiet_NaN();
			}
		}
		else {
			y1 = _finite(this->y_xMin) ? this->y_xMin : function(x1, std::forward<Args>(args)...);
		}

		if (failSafe) {
			try {
				y2 = _finite(this->y_xMax) ? this->y_xMax : function(x2, std::forward<Args>(args)...);
			}
			catch (const std::runtime_error & e) {
				e;//happy compiler
				y2 = std::numeric_limits<double>::quiet_NaN();
			}
		}
		else {
			y2 = _finite(this->y_xMax) ? this->y_xMax : function(x2, std::forward<Args>(args)...);
		}

		double x = std::numeric_limits<double>::quiet_NaN();
		double res = std::numeric_limits<double>::infinity();

		if (plotConvergence) {
			this->p_pt_x->Push(x1).Push(x2);
			this->p_pt_y->Push(y1).Push(y2);
			this->p_pt_y_target->Push(y_target).Push(y_target);
			//this->pPlot->AddSerie(*p_pt_x, *p_pt_y, RGB(0, 0, 255), PS_NULL, 1, AXIS_MARKER_DIAGCROSS)
			//	.AddSerie(*p_pt_x, *p_pt_y_target)
			//	.AutoFit().Refresh();
			//getchar();
		}

		for (size_t nIter = 0; nIter < maxIter; nIter++) {

			auto x_mid = 0.5 * (x1 + x2);
			auto old_x = x;
			x = x_mid;

			double y_mid;
			
			if (failSafe) {
				try {
					y_mid = function(x_mid, std::forward<Args>(args)...);
				}
				catch (const std::runtime_error & e) {
					e;//happy compiler
					y_mid = std::numeric_limits<double>::quiet_NaN();
				}
			} else {
				y_mid = function(x_mid, std::forward<Args>(args)...);
			}

			if (failSafe && (!_finite(y1) || !_finite(y2) || !_finite(y_mid))) return this->SetResults(x1, 0, std::numeric_limits<double>::quiet_NaN(), false);

			res = y_target - y_mid;

			if (plotConvergence) {
				this->p_pt_x->Push(x_mid);
				this->p_pt_y->Push(y_mid);
				this->p_pt_y_target->Push(y_target);
				this->pPlot->ClearSeries()
					.AddSerie(*p_pt_x, *p_pt_y, RGB(0, 0, 255), PS_NULL, 1, AXIS_MARKER_DIAGCROSS)
					.AddSerie(*p_pt_x, *p_pt_y_target)
					.AutoFit().Refresh();
				getchar();
			}

			if (abs(res / (y_target ? y_target : 1)) < tol) {
				bool ok = true;
				if (this->tolCheckX) {
					auto v = abs(old_x - x);
					if (this->tolCheckX_Relative) {
						v /= x;
					}
					if (!(v < tol)) ok = false;
				}
				if (ok) return this->SetResults(x_mid, nIter, res, true);
			}

			bool inFirst  = std::signbit(y_target - y1) != std::signbit(res);
			bool inSecond = std::signbit(y_target - y2) != std::signbit(res);
			bool nowhere = !inFirst && !inSecond;

			if (nowhere) {
				auto maxRecursion = this->maxRecursion == -1 ? 5 : this->maxRecursion;
				if (this->currentRecursion >= maxRecursion) {
					return this->SetResults(x, nIter, res, false);
				} else {
					//if (this->currentRecursion == -1) {
					//	//for (int i = 1; i < 20; ++i) {
					//		//this->recurse = i;
					//	this->currentRecursion = 5;
					//	if (this->Solve<T, function, plotConvergence, strict, failSafe>(std::forward<Args>(args)...)) return true;
					//	//}
					//} else {
					//if (this->currentRecursion) {

					Dicotomy1D_Solver first, second;
					first.maxIter  = second.maxIter = maxIter;
					first.tol	   = second.tol     = tol;
					first.tol_x	   = second.tol_x   = this->tol_x;
					first.tolCheckX = second.tolCheckX = this->tolCheckX;
					first.tolCheckX_Relative = second.tolCheckX_Relative = this->tolCheckX_Relative;
					first.y_target = second.y_target = y_target;
					first.p_pt_x   = second.p_pt_x = this->p_pt_x;
					first.p_pt_y   = second.p_pt_y = this->p_pt_y;
					first.pPlot    = second.pPlot = this->pPlot;
					first.xMin     = x1;
					first.y_xMin   = y1;
					first.xMax     = second.xMin   = x_mid;
					first.y_xMax   = second.y_xMin = y_mid;
					second.xMax    = x2;
					second.y_xMax  = y2;
					first.p_pt_y_target = second.p_pt_y_target = this->p_pt_y_target;
					first.maxRecursion = second.maxRecursion = this->maxRecursion;

					first.currentRecursion = second.currentRecursion = this->currentRecursion + 1;
					if (first.Solve<T, function, plotConvergence, strict, failSafe>(std::forward<Args>(args)...)) {
						return this->SetResults(first.result, first.nIter, first.residue, first.isConverged);
					}
					if (second.Solve<T, function, plotConvergence, strict, failSafe>(std::forward<Args>(args)...)) {
						return this->SetResults(second.result, second.nIter, second.residue, second.isConverged);
					}
					//	}
					//}

					return this->SetResults(x_mid, nIter, res, false);
				}
			}

			if (inFirst) {
				x2 = x_mid;
				y2 = y_mid;
			}
			else {
				x1 = x_mid;
				y1 = y_mid;
			}

			if (!strict && IsEqualWithPrec(x1, x2, tol)) {
				return this->SetResults(x, nIter, res, true);
			}
		}

		return this->SetResults(x, maxIter, res, false);
	}
};


struct Naive1D_Solver : Base_Solver < Naive1D_Solver > {

	Naive1D_Solver(
		double y_target = 0,
		double tol = 1e-10,
		size_t maxIter = 1000,
		double xMin = -std::numeric_limits<double>::infinity(),
		double xMax = std::numeric_limits<double>::infinity())
	:
		Base_Solver(y_target, tol, maxIter, xMin, xMax)
	{}


	template <bool plotConvergence = false, class T, class... Args >
	bool Solve(T & function, Args&&... args) {

		auto y_target = this->y_target;
		auto tol = this->tol;
		auto maxIter = this->maxIter;
		auto xMin = this->xMin;
		auto xMax = this->xMax;
		auto xInit = this->xInit;

		double x = xInit;
		double dx = 2 * (xMax - xMin) / maxIter;
		double minRes = std::numeric_limits<double>::infinity();
		double xMinRes = std::numeric_limits<double>::signaling_NaN();
		
		if (plotConvergence) {
			this->plot.Create();
		}
		
		bool signChange = false;

		for (size_t nIter = 0; nIter < maxIter; nIter++) {
			
			if (x > xMax || signChange) {
				x = xMinRes - dx;
				xMax = xMinRes + dx;
				dx *= 0.1;
			}

			double y = function(x, std::forward<Args>(args)...);
			double res = y - y_target;

			if (abs(res / (y_target ? y_target : 1)) < tol) {
				return this->SetResults(x, 0, res, true);
			}

			if (abs(res) < minRes) {
				minRes = abs(res);
				xMinRes = x;
			}

			if (plotConvergence) {
				this->pt_x.Push(x);
				this->pt_y.Push(y);
				this->pt_y_target.Push(y_target);
				this->plot.AddSerie(pt_x, pt_y, RGB(0, 0, 255), PS_NULL, 1, AXIS_MARKER_DIAGCROSS)
					.AddSerie(pt_x, pt_y_target)
					.AutoFit().Refresh();
				getchar();
			}

			x += dx;
		}

		return this->SetResults(xMinRes, maxIter, minRes, false);
	}
};



struct Scan1D_Solver : Dicotomy1D_Solver {

	double dx;
	bool adapt_dx;

	Scan1D_Solver(
		double y_target = 0,
		double tol = 1e-10,
		double dx = std::numeric_limits<double>::quiet_NaN(),
		double xMin = -std::numeric_limits<double>::infinity(),
		double xMax = std::numeric_limits<double>::infinity(),
		bool adapt_dx = true)
		:
		Dicotomy1D_Solver(y_target, tol, 1000, xMin, xMax), dx(dx), adapt_dx(adapt_dx)
	{}


	template <class T, T & function, bool plotConvergence = false, class... Args >
	bool Solve(Args&&... args) {

		auto y_target = this->y_target;
		auto tol  = this->tol;
		auto dx   = this->dx;
		auto xMin = this->xMin;
		auto xMax = this->xMax;
		auto adapt_dx = this->adapt_dx;
		
		double x = xMin;
		double x_1 = x;
		double y_1 = function(x_1, std::forward<Args>(args)...);
		double res_1 = y_1 - y_target;

		if (plotConvergence) {
			this->pPlot->Create();
			this->pt_x.Push(x_1);
			this->pt_y.Push(y_1);
			this->pt_y_target.Push(y_target);
		}

		if (abs(res_1 / (y_target ? y_target : 1)) < tol) {
			return this->SetResults(x, 0, res_1, true);
		}

		double res_2;

		if (!_finite(dx)) {
			dx = this->dx = (xMax - xMin) / 1e4;
		}

		for (size_t nIter = 0; x <= xMax; nIter++) {

			x += dx;

			if (x > xMax) {
				res_2 = NaN;
				break;
			}

			double x_2 = x;
			double y_2 = function(x_2, std::forward<Args>(args)...);
			res_2 = y_2 - y_target;

			bool signChange = sign(y_1) != sign(y_2);

			if (adapt_dx) {
				auto c = (res_2 - res_1) / res_2;
				if ((c < -1 || signChange || x > xMax) && dx > this->dx) {
					x -= dx;
					dx *= 0.5;
					if (dx < this->dx) dx = this->dx;
					continue;
				} else if (c > -0.5) {
					dx *= 1.25;
				}
			}

			if (abs(res_2 / (y_target ? y_target : 1)) < tol) {
				return this->SetResults(x_2, 0, res_2, true);
			}

			if (plotConvergence) {
				this->pt_x.Push(x_2);
				this->pt_y.Push(y_2);
				this->pt_y_target.Push(y_target);
				this->pPlot->AddSerie(pt_x, pt_y, RGB(0, 0, 255), PS_NULL, 1, AXIS_MARKER_DIAGCROSS)
					.AddSerie(pt_x, pt_y_target)
					.AutoFit().Refresh();
				getchar();
			}

			if (signChange) {
				this->xMin = x_1;
				this->xMax = x_2;
				this->y_xMin = y_1;
				this->y_xMax = y_2;
				return Dicotomy1D_Solver::Solve<T, function, plotConvergence>(std::forward<Args>(args)...);
			}

			x_1 = x_2;
			y_1 = y_2;
			res_1 = res_2;
		}

		return this->SetResults(xMin, nIter, res_2, false);
	}
};


struct Min_Naive_1D_Solver : Base_Solver < Min_Naive_1D_Solver > {

	double dx_init;

	Min_Naive_1D_Solver(
		double y_target = 0,
		double tol = 1e-10,
		size_t maxIter = 1000,
		double xMin = -std::numeric_limits<double>::infinity(),
		double xMax = std::numeric_limits<double>::infinity(),
		double xInit = std::numeric_limits<double>::quiet_NaN(),
		double dx_init = std::numeric_limits<double>::quiet_NaN())
		:
		Base_Solver(y_target, tol, maxIter, xMin, xMax, xInit), dx_init(dx_init)
	{}


	template <bool plotConvergence = false, class T, class... Args >
	bool Solve(T & function, Args&&... args) {

		//auto y_target = this->y_target;
		auto tol = this->tol;
		auto maxIter = this->maxIter;
		auto xMin = this->xMin;
		auto xMax = this->xMax;
		auto xInit = this->xInit;
		auto dx_init = this->dx_init;

		double x = xInit;
		double dx = _finite(dx_init) ? dx_init : 2 * (xMax - xMin) / maxIter;
		double min_y = std::numeric_limits<double>::infinity();
		double xMin_y = std::numeric_limits<double>::quiet_NaN();
		double old_y = std::numeric_limits<double>::quiet_NaN();

		if (plotConvergence) {
			this->plot.Create();
		}

		bool firstReverse = true;
		
		//double y = function(x, std::forward<Args>(args)...);

		for (size_t nIter = 0; nIter < maxIter; nIter++) {

			//if (x > xMax) {
			//	x = xMinRes - dx;
			//	xMax = xMinRes + dx;
			//	dx *= 0.1;
			//}

			double y = function(x, std::forward<Args>(args)...);
			//double res = y;

			if (!_finite(xMin_y) || y < min_y) {
				min_y = y;
				xMin_y = x;
			}
			 
			if (_finite(old_y) && y > old_y) {
				if (firstReverse) {
					firstReverse = false;
				} 
				else { 
					dx *= 0.1; 
				}
				if (abs(y - min_y) / abs(min_y) < tol) {
					if (abs(y - old_y) / abs(old_y) < tol) {
						return this->SetResults(xMin_y, nIter, min_y, true);
					}
				}
				dx *= -1;
				if (_finite(xMin_y)) x = xMin_y;
			}
			else {
				dx *= 1.05;
			}


			//if (abs(res / (y_target ? y_target : 1)) < tol) {
			//	return this->SetResults(x, 0, res, true);
			//}

			if (plotConvergence) {
				this->pt_x.Push(x);
				this->pt_y.Push(y);
				this->plot.AddSerie(pt_x, pt_y, RGB(0, 0, 255), PS_NULL, 1, AXIS_MARKER_DIAGCROSS)
					.AutoFit().Refresh();
				getchar();
			}

			x += dx;

			old_y = y;
		}

		return this->SetResults(xMin_y, maxIter, min_y, false);
	}
};


//from wikipedia
const double TwoPi = 2 * 3.1415926535897932384626433832795;

template<bool localAlloc = false>
void FFTAnalysis(const double *AVal, double *FTvl, int Nvl, int Nft) {
	int i, j, n, m, Mmax, Istp;
	double Tmpr, Tmpi, Wtmp, Theta;
	double Wpr, Wpi, Wr, Wi;
	double *Tmvl;

	aVect<double> localBuffer;

	n = Nvl * 2; 
	
	static WinCriticalSection cs;

	if (localAlloc) {
		//Tmvl = new double[n + 1];
		localBuffer.Redim(n + 1);
		Tmvl = localBuffer;
	}
	else {
		cs.Enter();
		static aVect<double> staticBuffer;
		staticBuffer.Redim(n + 1);
		Tmvl = staticBuffer;
	}

	for (i = 0; i <Nvl; i++) {
		j = i * 2; Tmvl[j] = 0; Tmvl[j + 1] = AVal[i];
	}

	i = 1; j = 1;
	while (i <n) {
		if (j> i) {
			Tmpr = Tmvl[i]; Tmvl[i] = Tmvl[j]; Tmvl[j] = Tmpr;
			Tmpr = Tmvl[i + 1]; Tmvl[i + 1] = Tmvl[j + 1]; Tmvl[j + 1] = Tmpr;
		}
		i = i + 2; m = Nvl;
		while ((m >= 2) && (j> m)) {
			j = j - m; m = m >> 2;
		}
		j = j + m;
	}

	Mmax = 2;
	while (n> Mmax) {
		Theta = -TwoPi / Mmax; Wpi = sin(Theta);
		Wtmp = sin(Theta / 2); Wpr = Wtmp * Wtmp * 2;
		Istp = Mmax * 2; Wr = 1; Wi = 0; m = 1;

		while (m <Mmax) {
			i = m; m = m + 2; Tmpr = Wr; Tmpi = Wi;
			Wr = Wr - Tmpr * Wpr - Tmpi * Wpi;
			Wi = Wi + Tmpr * Wpi - Tmpi * Wpr;

			while (i <n) {
				j = i + Mmax;
				Tmpr = Wr * Tmvl[j] - Wi * Tmvl[j + 1];
				Tmpi = Wi * Tmvl[j] + Wr * Tmvl[j + 1];

				Tmvl[j] = Tmvl[i] - Tmpr; Tmvl[j + 1] = Tmvl[i + 1] - Tmpi;
				Tmvl[i] = Tmvl[i] + Tmpr; Tmvl[i + 1] = Tmvl[i + 1] + Tmpi;
				i = i + Istp;
			}
		}

		Mmax = Istp;
	}

	for (i = 1; i <Nft; i++) {
		j = i * 2; FTvl[i] = sqrt(Sqr(Tmvl[j]) + Sqr(Tmvl[j + 1]));
	}

	//if (localAlloc) {
	//	delete[]Tmvl;
	//}
}

template <bool localAlloc = false>
void FFTAnalysis(const aVect<double> & AVal, aVect<double> & FTvl) {

	auto N = AVal.Count();

	FTvl.Redim(N);

	FFTAnalysis<localAlloc>(AVal, FTvl, N, N);
}


template <bool localAlloc = false>
aVect<double> FFTAnalysis(const aVect<double> & AVal) {

	auto N = AVal.Count();

	aVect<double> FTvl(N);

	FFTAnalysis<localAlloc>(AVal, FTvl, N, N);

	return FTvl;
}

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif

inline aVect<double> DHT_slow(const aVect<double> & x) {

	auto N = x.Count();

	aVect<double> h(N);

	for (size_t i = 0; i < N; ++i) {

		double sum = 0;

		for (size_t j = 0; j < N; ++j) {
			sum += x[j] * cos(2 * PI / N * i * j - PI / 4);
		}

		h[i] = sqrt(2.0 / N) * sum;
	}

	return h;
}

inline aVect<double> DHT_mod_slow(const aVect<double> & x) {

	auto N = x.Count();

	aVect<double> h(N);

	for (size_t i = 0; i < N; ++i) {

		double sum = 0;

		for (size_t j = 0; j < N; ++j) {
			sum += x[j] * cos(2 * PI / N * i * j);
		}

		h[i] = sqrt(1.0 / N) * sum;
	}

	return h;
}


inline aVect<double> DHT_mod_inv_slow(const aVect<double> & x) {

	auto N = x.Count();

	aVect<double> h(N);

	for (size_t i = 0; i < N; ++i) {

		double sum = 0;

		for (size_t j = 0; j < N; ++j) {
			sum += x[j] * sin(2 * PI / N * i * j);
		}

		h[i] = sqrt(1.0 / N) * sum;
	}

	return h;
}

inline aVect<double> DFT_DHT_slow(const aVect<double> & x) {

	auto h = DHT_slow(x);

	auto N = x.Count();
	aVect<double> ft(N);

	for (size_t i = 0; i < N; ++i) {
		ft[i] = sqrt(Sqr(h[i]) + Sqr(h[N - i - 1]));
	}

	return ft;
}

template <class T>
aVect<Complex<double>> DFT_slow(const aVect<T> & x) {

	auto N = x.Count();

	aVect<Complex<double>> ft(N);

	for (size_t i = 0; i < N; ++i) {

		Complex<double> sum(0, 0);

		for (size_t j = 0; j < N; ++j) {
			sum += x[j] * Complex<double>(1, -2*PI*i*j/N, Complex<double>::State::Polar);
		}

		ft[i] = sum;
	}

	return ft;
}

template <class T>
aVect<Complex<double>> DFT_inv_slow(const aVect<T> & x) {

	auto N = x.Count();
	aVect<Complex<double>> ft_inv(N);

	for (size_t i = 0; i < N; ++i) {

		Complex<double> sum(0, 0);

		for (size_t j = 0; j < N; ++j) {
			sum += x[j] * Complex<double>(1, 2 * PI*i*j / N, Complex<double>::State::Polar);
		}

		ft_inv[i] = sum * (1.0 / N);
	}

	return ft_inv;
}

inline aVect<double> DTT(const aVect<double> & x) {

	auto N = x.Count();
	aVect<double> retVal(N);

	const auto fact = sqrt(0.5);

	for (size_t i = 0; i < (N + 1) / 2; ++i) {
		retVal[i] = fact  * (x[i] + x[N - 1 - i]);
	}

	for (size_t i = (N + 1) / 2; i < N; ++i) {
		retVal[i] = fact  * (x[i] - x[N - 1 - i]);
	}

	return retVal;
}


inline aVect<double> DTT_inv(const aVect<double> & x) {

	auto N = x.Count();
	aVect<double> retVal(N);

	const auto fact = sqrt(0.5);

	for (size_t i = 0; i < (N + 1) / 2; ++i) {
		retVal[i] = fact * (x[i] - x[N - 1 - i]);
	}

	for (size_t i = (N + 1) / 2; i < N; ++i) {
		retVal[i] = fact * (x[i] + x[N - 1 - i]);
	}

	if (N % 2 == 1) retVal[(N - 1) / 2] = fact * x[(N - 1) / 2];

	return retVal;
}


template <class T>
struct CubicRoots {
	T z1;
	T z2;
	T z3;
	bool onlyOneRoot;

	CubicRoots(T z) : z1(z), onlyOneRoot(true) {}
	CubicRoots(T z1, T z2, T z3) : z1(z1), z2(z2), z3(z3), onlyOneRoot(false) {}
};

//z^3 + a2*z^2 + a1*z + a0 = 0
template <class T>
CubicRoots<T> SolveCubicEquation(T a0, T a1, T a2) {

	auto p = (3 * a1 - a2*a2) / 3;
	auto q = (2 * a2*a2*a2 - 9 * a2*a1 + 27 * a0) / 27;
	auto r = q*q / 4 + p*p*p / 27;

	if (r >= 0) {
		auto P = std::cbrt(-q / 2 + sqrt(r));
		auto Q = std::cbrt(-q / 2 - sqrt(r));
		auto z1 = P + Q;
		return CubicRoots<T>(z1 - a2 / 3);
	}
	else {
		auto m = 2 * sqrt(-p / 3);
		auto teta = acos(3 * q / (p*m)) / 3;
		auto z1 = m * cos(teta + 2 * PI / 3);
		auto z2 = m * cos(teta + 4 * PI / 3);
		auto z3 = m * cos(teta);
		return CubicRoots<T>(z1 - a2 / 3, z2 - a2 / 3, z3 - a2 / 3);
	}
}

struct SecantMethod_Solver2 : Base_Solver<SecantMethod_Solver2> {

	//double x_init;
	//double relaxFactor1;
	//size_t iterRelaxFactor2;
	//double relaxFactor2;

	SecantMethod_Solver2(
		double y_target = 0,
		double x_init = 0,
		double tol = 1e-10,
		size_t maxIter = 1000,
		//double relaxFactor1 = 1,
		//size_t iterRelaxFactor2 = 100,
		//double relaxFactor2 = 0.1,
		double xMin = -std::numeric_limits<double>::infinity(),
		double xMax = std::numeric_limits<double>::infinity())
		:
		Base_Solver(y_target, tol, maxIter, xMin, xMax, x_init)//,
		//relaxFactor1(relaxFactor1),
		//iterRelaxFactor2(iterRelaxFactor2),
		//relaxFactor2(relaxFactor2)
	{}

	template <bool plotConvergence = false, bool fastFail = false, class T, class... Args >
	bool Solve(T & function, Args&&... args) {

		auto y_target = this->y_target;
		auto tol = this->tol;
		auto maxIter = this->maxIter;
		auto xMin = this->xMin;
		auto xMax = this->xMax;

		//used if fastFail == true
		auto xMin_ok = std::numeric_limits<double>::quiet_NaN();
		auto xMax_ok = std::numeric_limits<double>::quiet_NaN();
		double old_x;

		auto x = this->xInit;
		auto y = function(x, std::forward<Args>(args)...);
		auto res = y - y_target;

		if (!_finite(y)) return this->SetResults(x, 0, res, false);

		if (plotConvergence) {
			this->pt_x.Erase();
			this->pt_y.Erase();
			this->pt_y_target.Erase();
			this->pt_x.Push(x);
			this->pt_y.Push(y);
			this->pt_y_target.Push(y_target);
		}

		if (abs(res / (y_target ? y_target : 1)) < tol) {
			return this->SetResults(x, 0, res, true);
		}

		double dx = 0.01 * x;

		double y_of_x_plus_dx = function(x + dx, std::forward<Args>(args)...);

		if (!_finite(y_of_x_plus_dx)) return this->SetResults(x + dx, 0, res, false);

		if (plotConvergence) {
			this->pt_x.Push(x + dx);
			this->pt_y.Push(y_of_x_plus_dx);
			this->pt_y_target.Push(y_target);
			this->pPlot->Create().AddSerie(pt_x, pt_y, RGB(0, 0, 255), PS_SOLID, 1, AXIS_MARKER_DIAGCROSS).AddSerie(pt_x, pt_y_target)
				.AutoFit().Refresh();
		}

		int nCorrect;

		if (fastFail) nCorrect = 0;

		for (size_t nIter = 0; nIter < maxIter; nIter++) {

			double dy = y_of_x_plus_dx - y;
			double dres = dy;

			double delta_x = -dx * res / dres;

			//delta_x *= nIter > iterRelaxFactor2 ? relaxFactor2 : relaxFactor1;

			double new_x = x + delta_x;

			if (new_x < xMin) {
				if (fastFail && ++nCorrect > 5) return this->SetResults(x, nIter, res, false);
				new_x = 0.9 * xMin + 0.1 * x;
				delta_x = new_x - x;
			} else if (new_x > xMax) {
				if (fastFail && ++nCorrect > 5) return this->SetResults(x, nIter, res, false);
				new_x = 0.9 * xMax + 0.1 * x;
				delta_x = new_x - x;
			}

			if (fastFail) {
				if (new_x < xMin_ok || new_x > xMax_ok) {
					this->xMin = xMin_ok;
					this->xMax = xMax_ok;
					return this->SetResults(x, nIter, res, false);
				}
				old_x = x;
			}

			//now use the new x and old x as the 2 points to estimate the derivative

			dx = -delta_x;
			x = new_x;
			y_of_x_plus_dx = y;

			y = function(x, std::forward<Args>(args)...);
			if (!_finite(y)) return this->SetResults(x, nIter, res, false);

			if (plotConvergence) {
				this->pt_x.Push(x);
				this->pt_y.Push(y);
				this->pt_y_target.Push(y_target);
				this->pPlot->ClearSeries().AddSerie(pt_x, pt_y, RGB(0, 0, 255), PS_SOLID, 1, AXIS_MARKER_DIAGCROSS).AddSerie(pt_x, pt_y_target)
					.AutoFit().Refresh();
				getchar();
			}

			if (fastFail) {
				if (std::signbit(y_of_x_plus_dx - y_target) != std::signbit(y - y_target)) {
					if (delta_x > 0) xMin_ok = old_x, xMax_ok = x; 
					else xMin_ok = x, xMax_ok = old_x;
				}
			}

			res = y - y_target;

			if (abs(res / (y_target ? y_target : 1)) < tol) {
				return this->SetResults(x, nIter, res, true);
			}
		}

		return this->SetResults(x, maxIter, res, false);
	}
};

struct MonotoneHybrid_Solver : Dicotomy1D_Solver {

	MonotoneHybrid_Solver(
		double y_target = 0,
		double x_init = 0,
		double tol = 1e-10,
		size_t maxIter = 1000,
		double xMin = -std::numeric_limits<double>::infinity(),
		double xMax = std::numeric_limits<double>::infinity())
		:
		Dicotomy1D_Solver(y_target, tol, maxIter, xMin, xMax)
	{
		this->xInit = x_init;
		this->maxRecursion = 0;
	}

	template <class T, T& function, bool plotConvergence = false, bool failSafe = false, class... Args >
	bool Solve(Args&&... args) {
		 
		auto secantSolver = (SecantMethod_Solver2 *)this;

		bool ok = false;
		if (failSafe) {
			try{
				ok = secantSolver->Solve<plotConvergence, true>(function, std::forward<Args>(args)...);
			}
			catch (const std::runtime_error &) {
				/*do nothing*/
			}
		} else {
			ok = secantSolver->Solve<plotConvergence, true>(function, std::forward<Args>(args)...);
		}

		if (ok) {
			return true;
		}
		else {
			auto dicotomySolver = (Dicotomy1D_Solver*)this;
			auto nIter   = dicotomySolver->nIter;
			auto maxIter = dicotomySolver->maxIter;
			dicotomySolver->maxIter = maxIter - nIter;
			auto oldMaxRecursion = dicotomySolver->maxRecursion;
			dicotomySolver->maxRecursion = -1;
			auto retVal = dicotomySolver->Solve<T, function, plotConvergence, true, failSafe>(std::forward<Args>(args)...);
			dicotomySolver->maxRecursion = oldMaxRecursion;
			dicotomySolver->nIter += nIter;

			if (!retVal) {
				auto d = dicotomySolver->xMax - dicotomySolver->xMin;
				auto old_xMin = dicotomySolver->xMin;
				auto old_xMax = dicotomySolver->xMax;
				dicotomySolver->xMin -= d / 2;
				dicotomySolver->xMax += d / 2;
				oldMaxRecursion = dicotomySolver->maxRecursion;
				dicotomySolver->maxRecursion = -1;
				retVal = dicotomySolver->Solve<T, function, plotConvergence, true, failSafe>(std::forward<Args>(args)...);
				dicotomySolver->maxRecursion = oldMaxRecursion;
				dicotomySolver->nIter += nIter;
				dicotomySolver->xMin = old_xMin;
				dicotomySolver->xMax = old_xMax;
			}

			dicotomySolver->maxIter = maxIter;
			return retVal;
		}
	}
};


#endif

