#define _CRT_SECURE_NO_WARNINGS

#include "Thermo.h"
#include "NewtonSolver.h"
#include "Bisect2D.h"

const double R = 8314.4598;	// J/K/kmol
//const double R = 8.3144598;// J/K/mol
const double NaN = std::numeric_limits<double>::quiet_NaN();

double Equation6(double(&coef)[5], double x) {
	// A + B/T +C*lnT + D*T^E
	return coef[0] + coef[1] / x + coef[2] * log(x) + coef[3] * pow(x, coef[4]);
}

FluidComposition * PhasesEquilibrium::Get_Parent() {
	constexpr auto offset = decltype(FluidComposition::phasesEquilibrium)::offsetValue;
	return (FluidComposition*)(((char*)this) - decltype(FluidComposition::phasesEquilibrium)::offsetValue);
}

void ComputePhi(aVect<double> & phi_i, const aVect<double> & ai, const aVect<double> & bi,
	const aVect<double> & xi, const aMat<double> & bip, double Z, double A, double B, double P, double T) {

	aVect_static_for(bi, i) {

		auto&& Bi = bi[i] * P / (R * T);

		double sum = 0;
		aVect_static_for(bi, i2) {
			sum += xi[i2] * (1 - bip(i, i2))*sqrt(ai[i] * ai[i2]);
		}

		sum *= P / (Sqr(R)*Sqr(T));

		const double sqrt_2 = sqrt(2);

		//fugacity coef
		phi_i[i] = exp((Z - 1) * Bi / B - log(Z - B)
			- A / (2 * sqrt_2 * B) * (2 * sum / A - Bi / B) * log((Z + (1. + sqrt_2) * B) / (Z + (1. - sqrt_2) * B)));

		//fugacity
		phi_i[i] *= xi[i] * P;
	}
}

CubicCoef GetCoef(double A, double B) {

	auto u = 2, w = -1;

	CubicCoef retVal;
		
	retVal.a2 = (u - 1)*B - 1.;
	retVal.a1 = A + ((w - u)*B - u)*B;
	retVal.a0 = -((w*B + w)*B + A)*B;

	return retVal;
}

CubicRoots<double> GetRoots(double A, double B) {

	auto coefs = GetCoef(A, B);

	return SolveCubicEquation(coefs.a0, coefs.a1, coefs.a2);
}

bool PhasesEquilibrium::Update_aj() {
	COMPILE_TIME_OFFSET_CHECK(PhasesEquilibrium, aj);

	auto parent = this->Get_Parent();

	auto&& compo = parent->GetComponentsRef();
	auto&& bip = parent->GetBipRef();
	auto&& ai = parent->ai.GetRef();
	auto&& xij = this->xij.GetRef();
	auto&& aj = this->aj.GetRefForUpdate();

	for (size_t j = 0; j < Count(aj); ++j) {
		double sum = 0;
		auto&& xi = xij[j];
		aVect_static_for(compo, i1) {
			aVect_static_for(compo, i2) {
				sum += xi[i1] * xi[i2] * (1 - bip(i1, i2))*sqrt(ai[i1] * ai[i2]);
			}
		}
		aj[j] = sum;
	}

	return true;
}

bool PhasesEquilibrium::Update_aj_prime() {
	COMPILE_TIME_OFFSET_CHECK(PhasesEquilibrium, aj_prime);

	auto parent = this->Get_Parent();

	auto&& compo = parent->GetComponentsRef();
	auto&& bip = parent->GetBipRef();
	auto&& ai = parent->ai.GetRef();
	auto&& ai_prime = parent->ai_prime.GetRef();
	auto&& xij = this->xij.GetRef();
	auto&& aj_prime = this->aj_prime.GetRefForUpdate();

	for (size_t j = 0; j < Count(aj_prime); ++j) {
		double sum = 0;
		auto&& xi = xij[j];
		aVect_static_for(compo, i1) {
			aVect_static_for(compo, i2) {
				sum += xi[i1] * xi[i2] * (1 - bip(i1, i2)) *(sqrt(ai[i2] / ai[i1]) * ai_prime[i1] + sqrt(ai[i1] / ai[i2])*ai_prime[i2]);
			}
		}
		aj_prime[j] = 0.5 * sum;
	}

	return true;
}

bool PhasesEquilibrium::Update_aj_second() {
	COMPILE_TIME_OFFSET_CHECK(PhasesEquilibrium, aj_second);

	auto parent = this->Get_Parent();

	auto&& compo = parent->GetComponentsRef();
	auto&& bip = parent->GetBipRef();
	auto&& ai = parent->ai.GetRef();
	auto&& ai_prime = parent->ai_prime.GetRef();
	auto&& ai_second = parent->ai_second.GetRef();
	auto&& xij = this->xij.GetRef();
	auto&& aj_second = this->aj_second.GetRefForUpdate();

	for (size_t j = 0; j < Count(aj_second); ++j) {
		double sum = 0;
		auto&& xi = xij[j];
		aVect_static_for(compo, i1) {
			aVect_static_for(compo, i2) {
				sum += xi[i1] * xi[i2] * (1. - bip(i1, i2)) * (
					ai_prime[i1] * ai_prime[i2] / sqrt(ai[i1] * ai[i2])
					+ ai_second[i1] * sqrt(ai[i2] / ai[i1])
					+ ai_second[i2] * sqrt(ai[i1] / ai[i2])
					- (ai_prime[i1] * ai_prime[i1] / ai[i1] * sqrt(ai[i2] / ai[i1]) + ai_prime[i2] * ai_prime[i2] / ai[i2] * sqrt(ai[i1] / ai[i2])) / 2
					);
			}
		}

		aj_second[j] = 0.5 * sum;
	}

	return true;
}

bool PhasesEquilibrium::Update_bj() {
	COMPILE_TIME_OFFSET_CHECK(PhasesEquilibrium, bj);

	auto parent = this->Get_Parent();

	auto&& compo = parent->GetComponentsRef();
	auto&& bi = parent->bi.GetRef();
	auto&& xij = this->xij.GetRef();
	auto&& bj = this->bj.GetRefForUpdate();

	for (size_t j = 0; j < Count(bj); ++j) {
		double sum = 0;
		auto&& xi = xij[j];
		aVect_static_for(compo, i) {
			sum += xi[i] * bi[i];
		}
		bj[j] = sum;
	}

	return true;
}

bool PhasesEquilibrium::Update_Aj() {
	COMPILE_TIME_OFFSET_CHECK(PhasesEquilibrium, Aj);

	auto parent = this->Get_Parent();

	auto&& aj = this->aj.GetRef();

	auto T = parent->T.GetRef();
	auto P = parent->P.GetRef();

	auto&& Aj = this->Aj.GetRefForUpdate();

	for (size_t j = 0; j < Count(aj); ++j) {
		Aj[j] = aj[j] * P / (Sqr(R)*Sqr(T));
	}

	return true;
}

bool PhasesEquilibrium::Update_Bj() {
	COMPILE_TIME_OFFSET_CHECK(PhasesEquilibrium, Bj);

	auto parent = this->Get_Parent();

	auto T = parent->T.GetRef();
	auto P = parent->P.GetRef();
	auto&& bj = this->bj.GetRef();
	auto&& Bj = this->Bj.GetRefForUpdate();

	for (size_t j = 0; j < Count(bj); ++j) {
		Bj[j] = bj[j] * P / (R * T);
	}

	return true;
}

bool PhasesEquilibrium::Update_xij() {
	COMPILE_TIME_OFFSET_CHECK(PhasesEquilibrium, xij);

	return false;
}

bool PhasesEquilibrium::Update_Ki() {
	COMPILE_TIME_OFFSET_CHECK(PhasesEquilibrium, Ki);

	return false;
}

bool PhasesEquilibrium::Update_psi() {
	COMPILE_TIME_OFFSET_CHECK(PhasesEquilibrium, psi);

	return false;
}

bool PhasesEquilibrium::Update_phi_ij() {
	COMPILE_TIME_OFFSET_CHECK(PhasesEquilibrium, phi_ij);

	auto parent = this->Get_Parent();
	auto&& ai = parent->ai.GetRef();
	auto&& bi = parent->bi.GetRef();
	auto&& bip = parent->GetBipRef();
	auto&& Zj = this->Zj.GetRef();
	auto&& xij = this->xij.GetRef();
	auto&& Aj = this->Aj.GetRef();
	auto&& Bj = this->Bj.GetRef();
	auto&& phi_ij = this->phi_ij.GetRefForUpdate();

	auto   P = parent->P.value;
	auto   T = parent->T.value;

	for (size_t j = 0; j < Count(Zj); ++j) {
		auto&& A = Aj[j];
		auto&& B = Bj[j];
		auto&& Z = Zj[j];
		auto&& xi = xij[j];
		auto&& phi_i = phi_ij[j];

		phi_i.Redim(bi.Count());

		ComputePhi(phi_i, ai, bi, xi, bip, Z, A, B, P, T);
	}

	return true;
}

bool PhasesEquilibrium::Update_Zj() {
	COMPILE_TIME_OFFSET_CHECK(PhasesEquilibrium, Zj);

	auto&& Aj = this->Aj.GetRef();
	auto&& Bj = this->Bj.GetRef();
	auto&& Zj = this->Zj.GetRefForUpdate();

	for (size_t j = 0; j < Count(Zj); ++j) {
		
		auto Z = GetRoots(Aj[j], Bj[j]);

		Zj[j] = j == 0 || Z.onlyOneRoot ? Z.z1 : Z.z3;
	}

	return true;
}

void PhasesEquilibrium::DoNothing() {}

double HP_Flash_Func(double T, FluidComposition& f, double P) {
	f.T.SetValue(T);
	f.P.SetValue(P);
	return f.Enthalpy_MassWeightedAvg();
};

double VP_Flash_Func(double T, FluidComposition& f, double P) {
	f.T.SetValue(T);
	f.P.SetValue(P);
	return f.VaporMolarFraction();
};

double VVFT_Flash_Func(double P, FluidComposition& f, double T) {
	f.T.SetValue(T);
	f.P.SetValue(P);
	return f.VaporVolumeFraction();
};

struct RhoH_Flash_LocalParams {
#ifdef RhoH_Flash_MultiThread
	FluidComposition fluid;
#endif
};

struct RhoH_Flash_GlobalParams {
#ifndef RhoH_Flash_MultiThread
	FluidComposition * fluid;
#endif
	double density, enthalpy;

	aVect<double> history_P, history_T;

	PerformanceChronometer chrono;
	double maxTime = 5;
};

typedef Problem<RhoH_Flash_GlobalParams, RhoH_Flash_LocalParams> RhoH_Flash_ProblemType;

struct RhoH_Flash_Functions : RhoH_Flash_LocalParams {

	double Density(const RhoH_Flash_ProblemType & p) {

#ifdef RhoH_Flash_MultiThread
		auto&& fluid = this->fluid;
#else
		auto&& fluid = *p->fluid;
#endif

		fluid.P.SetValue(p[0]);
		fluid.T.SetValue(p[1]);
		auto retVal = fluid.Density_VolumeWeightedAvg() - p->density;
		return retVal;
	}

	double Enthalpy(const RhoH_Flash_ProblemType & p) {

#ifdef RhoH_Flash_MultiThread
		auto&& fluid = this->fluid;
#else
		auto&& fluid = *p->fluid;
#endif

		fluid.P.SetValue(p[0]);
		fluid.T.SetValue(p[1]);
		auto retVal = fluid.Enthalpy_MassWeightedAvg() - p->enthalpy;
		return retVal;
	}
};

PlotFunctionCallbackReturnValue PlotFunc(RhoH_Flash_ProblemType & p, void *) {

	p->history_P.Push(p[0] / 1e5);
	p->history_T.Push(p[1] - 273.15);

	p.axes.profiles.AddSerie(p->history_T, p->history_P).xLabel(L"T [°C]").yLabel(L"P [Bar]");

	return NEWTONSOLVER_REFRESHASYNC;
}

ExceptionCallbackReturnValue ExceptionCallback(RhoH_Flash_ProblemType & p, std::exception_ptr ePtr, int failSafe) {

	if (failSafe) return ExceptionCallbackReturnValue::NEWTONSOLVER_SOLVE_FAIL;
	else return ExceptionCallbackReturnValue::NEWTONSOLVER_RETHROW_EXCEPTION;

}

void IterationCallback(RhoH_Flash_ProblemType & p) {
	if (p->chrono.GetSeconds() > 1) {
		throw std::runtime_error("NewtonRaphson: no convergence (too long solve)");
	}
	if (p.nRetryNewtonRaphson == 1) {
		p.lineSearch = true;
	}
}

//#pragma optimize( "", off )
bool ApplyDeltaX(mVect<double> & X, const aVect<double> & delta_X, double relaxFactor, const RhoH_Flash_ProblemType & p) {

	auto&& delta_P = delta_X[0];
	auto&& delta_T = delta_X[1];

	if (p.nRetryNewtonRaphson > 0) {

		auto&& F_X = p.GetResidue();
		auto&& res_Density  = F_X[0];
		auto&& res_Enthalpy = F_X[1];

		auto maxRes = Max(res_Density, res_Enthalpy / 1000);
		//p.CheckConvergence<true, false>();

		if (relaxFactor * delta_P / 1e5 > 0.1 * maxRes) {
			relaxFactor = Min(relaxFactor, relaxFactor * 0.1 * maxRes / (delta_P / 1e5));
		}
		if (relaxFactor * delta_T > 0.1 * maxRes) {
			relaxFactor = Min(relaxFactor, relaxFactor * 0.1 * maxRes / delta_T);
		}
	}

	X[0] -= relaxFactor * delta_P;
	X[1] -= relaxFactor * delta_T;

	if (X[0] < 0 || X[1] < 0) {
		return false;
	} else {
		return true;
	}
}
//#pragma optimize( "", on )

struct RhoH_FlashBisect2 : Bisect2<RhoH_Flash_GlobalParams> {

	RhoH_FlashBisect2() : Bisect2() {}
	RhoH_FlashBisect2(int maxLevel, double tolerance) : Bisect2(maxLevel, tolerance) {}

	double f(double x, double y) override {
		this->userData.fluid->P.SetValue(x);
		this->userData.fluid->T.SetValue(y);

		return this->userData.fluid->Density_VolumeWeightedAvg() - this->userData.density;
	}

	double g(double x, double y) override {
		this->userData.fluid->P.SetValue(x);
		this->userData.fluid->T.SetValue(y);

		return 1e-3*(this->userData.fluid->Enthalpy_MassWeightedAvg() - this->userData.enthalpy);
	}

	void IterationCallback() override {
		if (this->userData.chrono.GetSeconds() > this->userData.maxTime) {
			throw std::runtime_error("RhoH Flash: too long solve");
		}
	}

};

void FluidComposition::RhoH_Flash_old(double density, double enthalpy, double tol, bool skipNewton) {

	static WinCriticalSection cs;
	ScopeCriticalSection<WinCriticalSection> guard_cs(cs);
	

	static RhoH_Flash_ProblemType p;
	static bool once;

	if (!once) {
		once = true;
		p.Redim(2);

		//p.sparse = false;

#ifndef RhoH_Flash_MultiThread
		p.EnableMultiThreading(false);
#endif
			
		p.functions[0].AddDependency(0).AddDependency(1);
		p.functions[1].AddDependency(0).AddDependency(1);
		p.SetPlotFunction(PlotFunc);

		p.functions[0].SetFunction(&RhoH_Flash_Functions::Density);
		p.functions[0].name = "Density";
		p.functions[1].SetFunction(&RhoH_Flash_Functions::Enthalpy);
		p.functions[1].name = "Enthalpy";

		p.variableNames.Erase().Push("Pressure").Push("Temperature");

		p.SetIterationCallback(IterationCallback);
		p.SetExceptionCallback(ExceptionCallback);
		//p.SetApplyDeltaX_Func(ApplyDeltaX);

		p.AddForbiddenNegativeValue(0);
		p.AddForbiddenNegativeValue(1);

		//p.debugMode = 5;
		p.maxNewtonRaphsonIterations = 500;
		p.L2_norm_lineSearch = true;

		p.InitVarFuncIndices();
	}
	
	if (!skipNewton) {
		p.convergenceCriterion = tol;

#ifdef RhoH_Flash_MultiThread
		p.functions[0].fluid = *this;
		p.functions[1].fluid = *this;
#else
		p->fluid = this;
#endif

		p->density = density;
		p->enthalpy = enthalpy;

		p->history_P.Erase();
		p->history_T.Erase();

		p[0] = 1e5;
		p[1] = 273.15;

		p->chrono.Restart();
		p.Invalidate_F_X();
		p.relaxFactor = 1;
		p.lineSearch = false;

		//OutputInfo outputInfo(OutputInfo::Buffer, 1000, 1000);
		OutputInfo outputInfo;

		//WinFile f("dbg2.log", "a");

		try {
			const bool mesureTiming = false, plotIterations = false, modified = false, computeMaxNorm = false, checkFinite = true;
			const int failSafe = 1;
			p.NewtonRaphsonSolve<mesureTiming, modified, computeMaxNorm, checkFinite, plotIterations, failSafe>(5, outputInfo);

			//p.GradientDescent<mesureTiming, modified, computeMaxNorm, checkFinite, plotIterations, failSafe>(5, outputInfo);
		}
		catch (const std::runtime_error & /*e*/) {
			//f.Write("NewtonRaphsonSolve failed with exception \"%s\"\n", e.what());
		}

		//f.Write(outputInfo.buffer);

		//p.DisplayTimings<true>(true, true, true);
	}

	if (!p.isConverged) {

		//f.Write("NewtonRaphsonSolve failed, trying 2D bisection...\n");

		static RhoH_FlashBisect2 bisect2D;

		bisect2D.Init(1000, 0.1*tol);

		bisect2D.userData.fluid = this;

		bisect2D.userData.density = density;
		bisect2D.userData.enthalpy = enthalpy;
		bisect2D.userData.chrono.Restart();

		double P, T;
		bool ok = false;

		ok = bisect2D.Bisect(0.1e5, 500e5, 10, 500, P, T);
				
		if (ok) {
			this->T.SetValue(T);
			this->P.SetValue(P);
		} else {
			bisect2D.userData.chrono.Restart();
			bisect2D.userData.maxTime = 10;
			ok = bisect2D.Bisect<true, 5>(0.1e5, 500e5, 10, 500, P, T);

			if (ok) {
				this->T.SetValue(T);
				this->P.SetValue(P);
			}
			else {
				throw std::runtime_error("RhoH Flash: no convergence");
			}
		}

	}

#ifdef RhoH_Flash_MultiThread
	*this = p.function[0].fluid;
#else
	//*this = p->fluid;
#endif
	
}

double RhoH_Flash2_Func(double P, FluidComposition& f, double enthalpy) {
	f.HP_Flash(enthalpy, P);
	return f.Density_VolumeWeightedAvg();
}

void FluidComposition::RhoH_Flash(double density, double enthalpy, double tol) {

	auto&& solver = *this->monotoneHybrid_solver2;

	solver.xInit = 1e5;
	this->phasesEquilibriumSolverTolerance = Max(tol * 1e-4, 1e-14);

	solver.tol = tol;
	solver.xMin = 0.1e5;
	solver.xMax = 1000e5;

	solver.y_target = density;
	
	solver.Solve<decltype(RhoH_Flash2_Func), RhoH_Flash2_Func, false>(*this, enthalpy);

	if (!solver.isConverged) throw std::runtime_error("RhoH_Flash2 Flash: no convergence");
}
