#ifndef _THERMO_
#define _THERMO_

#include <stdexcept>
#include "MagicRef.h"
#include "xMat.h"
#include "WinUtils.h"
#include "NumericMethods.h"
#include "MyUtils.h"

#define COMPILE_TIME_OFFSET_CHECK(parent, member) StaticCheckOffset<(size_t)&(((parent*)0)->member)>(member);

extern const double R;
extern const double NaN;

template<int offset, class T>
void StaticCheckOffset(T&&) {
	static_assert(std::remove_reference<T>::type::offsetValue == offset, "wrong offset");
};

template <class T>
size_t Count(const aVect<T>& v) { return v.Count(); }

template <class T, size_t N>
size_t Count(T(&)[N]) { return N; }

template <class Array>
double Polynome(Array&& coef, double x) {

	auto i = Count(coef) - 1;
	double retVal = coef[i];

	while (i-- != 0) retVal = x*retVal + coef[i];

	return retVal;
}

template <class Array>
double  PolynomeDerivative(Array&& coef, double x) {

	auto i = Count(coef) - 1;
	double retVal = i*coef[i];

	while (i-- != 1) {
		retVal = x*retVal + i*coef[i];
	}

	return retVal;
}

#ifdef _DEBUG
#define DEBUG_THERMO_THREADSAFE if (!this->dbgCritSect.TryEnter()) MY_ERROR("assertion failed: critical section already acquired"); ScopeCriticalSection<WinCriticalSection> acs(this->dbgCritSect); this->dbgCritSect.Leave();
#else
#define DEBUG_THERMO_THREADSAFE 
#endif

double Equation6(double(&coef)[5], double x);
CubicRoots<double> GetRoots(double A, double B);

struct CubicCoef {
	double a2, a1, a0;
};

CubicCoef GetCoef(double A, double B);

template <int bitIndex, int offset, class T, class Parent, bool (Parent::*updateFunc)(), void (Parent::*invalidateFunc)()>
struct TestField {

	T value;

	enum StaticParams { offsetValue = offset, indexValue = bitIndex };

	constexpr Parent* Get_Parent() const {
		return (Parent*)(((char*)this) - offset);
	}

	auto IsUpdated(Parent* parent) const { return parent->updatedBitField & BIT(bitIndex); }
	auto IsUpdated() const { return this->IsUpdated(this->Get_Parent()); }

	void Invalidate(Parent* parent) {
		if (this->IsUpdated(parent)) {
			parent->updatedBitField &= ~BIT(bitIndex);
			(parent->*invalidateFunc)();
		}
	}
	void Invalidate() { this->Invalidate(this->Get_Parent()); }

	void SetUpdated(Parent* parent) { parent->updatedBitField |= BIT(bitIndex); }
	void SetUpdated() { this->SetUpdated(this->Get_Parent()); }

	template <bool checkUpdated = true>
	const T& GetRef() const {

		auto&& parent = this->Get_Parent();

		if (checkUpdated && !this->IsUpdated(parent)) {
			if (!(parent->*updateFunc)()) __debugbreak();//MY_ERROR("uninitialized");
		}

		return this->value;
	}

	//for large objects, but field is always invalidated as it can't check for unnecessary updates (when value doesn't change)
	template<bool allowInvalidation = true>
	T& GetRefForUpdate() {

		auto&& parent = this->Get_Parent();

		if (allowInvalidation && this->IsUpdated(parent)) {
			(parent->*invalidateFunc)();
		}

		this->SetUpdated(parent);
		return this->value;
	}

	template <class U>
	void SetValue(U&& a) {

		auto&& parent = this->Get_Parent();

		if (this->IsUpdated(parent)) {
			if (this->value == a) return;
			(parent->*invalidateFunc)();
		}

		this->value = a;
		this->SetUpdated(parent);
	}
};


struct Component {
	aVect<char> name;
	aVect<char> chemicalFormula;
	double Tc	 = NaN;		//critical temperature		[K]
	double Pc	 = NaN;		//critical pressure			[Pa]
	double Vc    = NaN;		//critical volume			[m3]
	double omega = NaN;		//acentric factor			[-]
	double mW	 = NaN;		//molecular weigth			[kg/kmol]     (kmol aka kilogram-mole)
	double Z_ra  = NaN;		//Racket coeff				[-]
	double H0    = 0;		//reference enthalpy at 298K - enthalpy offset
	double G0	 = 0;		//reference free energy at 298K
	double V_CTD = NaN;		//COSTALD volume			[m3/kmol]
	double omega_CTD = NaN; //COSTALD acentric factor	[-]
	//double Zc    = NaN;		//Critical Z factor // always equal to 0.30744752401796566 for Peng Robinson
	aVect<double> lambda_L_coef;
	aVect<double> lambda_V_coef;
	aVect<double> H_coef;
	aVect<double> sigma_coef;
	double mu_coef[5];

	double H_id(double T) { 
		auto v = this->H0 + Polynome(this->H_coef, T) * this->mW;// kJ / kmol
		return v * 1000; // J / kmol
	}

	double Mu_L(double T) {
		return Equation6(this->mu_coef, T);
	}

	double Mu_V(double T) {

		double	eta = 5.44*pow(this->Tc, 1. / 6.) / (sqrt(this->mW)*pow(this->Pc, 2. / 3.));

		double Tr = T / this->Tc;

		double muj = Tr< 1.5 ?
			0.00034*pow(Tr, 0.94) :
			0.0001776*pow(4.48*Tr - 1.67, 5. / 8.);
		
		muj *= 0.82e-4 / eta;

		return muj;
	}

	double Lambda_L(double T) {
		return Polynome(this->lambda_L_coef, T);
	}

	double Lambda_V(double T) {
		return Polynome(this->lambda_V_coef, T);
	}

	double Sigma(double T) {
		return Polynome(this->sigma_coef, T / Tc);
	}

	double Cp_id(double T) {
		auto v = PolynomeDerivative(this->H_coef, T) * this->mW; // kJ/kmole/K
		return v * 1000;	// J/kmole/K
	}

	void ReadMathildaComponentFile(const wchar_t * fluidsPath) {

		static WinCriticalSection cs;

		Critical_Section(cs) {

			static CachedVect<CachedVect<aVect<char>>> fileMatrix;

			auto fileName = xFormat(L"%s\\Pure\\%S.dat", fluidsPath, this->name);

			WinFile file(fileName);

			fileMatrix.Erase();

			for (;;) {
				auto line = file.GetLine();
				if (!line) break;
				fileMatrix.Push();
				for (;;) {
					auto tok = RetrieveStr(line);
					if (!tok) break;
					fileMatrix.Last().Push(tok.Trim());
				}
			}

			int a = 0;

			auto Find = [&file](const char * desc, bool errorIfNotFound = true) -> CachedVect<aVect<char>> * {
				for (auto&& l : fileMatrix) {
					if (l && strcmp_caseInsensitive(l.First(), desc) == 0) {
						return &l;
					}
				}
				if (errorIfNotFound) MY_ERROR(xFormat("file \"%s\":\n\n\"%s\" not found", file.GetFilePath(), desc));
				return nullptr;
			};

			auto GetScalar = [&Find, &file](const char * desc, bool errorIfNotFound = true) {
				auto e = Find(desc, errorIfNotFound);
				if (!e) return NaN;
				if (e->Count() != 2) MY_ERROR(xFormat("file \"%s\":\n\n\"%s\" scalar expected", file.GetFilePath(), desc));
				return atof((*e)[1]);
			};

			auto GetVector = [&Find, &file](const char * desc, bool errorIfNotFound = true) {
				auto e = Find(desc, errorIfNotFound);
				aVect<double> retVal;
				if (!e) return retVal;
				if (e->Count() < 2) MY_ERROR(xFormat("file \"%s\":\n\n\"%s\" vector expected", file.GetFilePath(), desc));
				for (auto i = e->begin() + 1; i < e->end(); ++i) {
					retVal.Push(atof(*i));
				}
				return retVal;
			};

			auto GetArray = [&Find, &file](double(&retVal)[5], const char * desc, bool errorIfNotFound = true) {
				auto e = Find(desc, errorIfNotFound);
				if (!e) {
					for (auto&& v : retVal) v = NaN;
					return;
				}
				if (e->Count() < 2) MY_ERROR(xFormat("file \"%s\":\n\n\"%s\" vector expected", file.GetFilePath(), desc));
				for (auto i = e->begin() + 1; i < e->end(); ++i) {
					auto ind = e->Index(i);
					retVal[ind - 1] = atof(*i);
				}
			};

			this->mW			= GetScalar("Molar_weight_(g/mol)");
			this->Tc			= GetScalar("Critical_temperature_(dC)") + 273.15;
			this->Pc			= GetScalar("Critical_pressure_(bar)")* 1e5;
			this->omega			= GetScalar("Acentric_factor_(-)");
			this->Vc			= GetScalar("Critical_volume_(m3/mol)") * 1000;
			this->H_coef		= GetVector("Ideal_Enthalpy_(A+B*T+C*T2+D*T3+E*T4+F*T5)");
			this->H0			= GetScalar("Enthalpy_offset_(kJ/kmol)");
			this->lambda_L_coef = GetVector("Liquid_thermal_conductivity_(A+B*T+C*T2+D*T3+E*T4)");
			this->lambda_V_coef = GetVector("Vapor_thermal_conductivity_(A+B*T+C*T2)");
			this->sigma_coef	= GetVector("Surface_tension_(A+B*Tr+C*Tr2+D*Tr3)");
			this->Z_ra			= GetScalar("Compressibilite_de_Rackett_(Zra)", false);
			this->V_CTD			= GetScalar("COSTALD_Volume_(m3/kgmol)", false);
			this->omega_CTD		= GetScalar("COSTALD_Acentric_factor_(-)", false); 

			// Peng Robinson Z critical = 0.30744752401796566
			//auto a_crit = 0.457235528921 * Sqr(R) * Sqr(this->Tc) / this->Pc;
			//auto b_crit = 0.0777960739039 * R * this->Tc / this->Pc;
			//auto A_crit = a_crit * this->Pc / Sqr(R * this->Tc);
			//auto B_crit = b_crit * this->Pc / R / this->Tc;
			//auto Z_crit = GetRoots(A_crit, B_crit);
			//this->Zc = Z_crit.z1;

			if (std::isnan(this->Z_ra)) this->Z_ra = GetScalar("Rackett_Compressibility_(Zra)", false);
			GetArray(this->mu_coef, "Liquid_viscosity_(A+B/T+C*lnT+DT^E)");

			//auto line = file.GetLine();//discard first line (name)

			//auto Next = [&file, &line]() { line = file.GetLine(); RetrieveStr(line); };

			//Next(); this->mW = atof(line.Trim()) / 1000;
			//Next(); this->Tc = atof(line.Trim()) + 273.15;
			//Next(); this->Pc = atof(line.Trim()) * 1e5;
			//Next(); this->omega = atof(line.Trim());
			//Next(); this->Vc = atof(line.Trim());
			//Next(); //discard specific gravity
			//Next(); while (auto value = RetrieveStr(line)) this->H_coef.Push(atof(value.Trim()));
			//Next(); this->H0 = atof(line.Trim());
			//Next(); for (size_t i = 0; i < Count(mu_coef); ++i) this->mu_coef[i] = atof(RetrieveStr(line).Trim());
			//Next(); while (auto value = RetrieveStr(line)) this->lambda_L_coef.Push(atof(value.Trim()));
			//Next(); while (auto value = RetrieveStr(line)) this->lambda_V_coef.Push(atof(value.Trim()));
			//Next(); while (auto value = RetrieveStr(line)) this->sigma_coef.Push(atof(value.Trim()));
			//Next(); this->Z_ra = atof(line.Trim());
		}
	}

	Component() {}
	Component(const char * name) : name(name) {}
};

struct GlobalComponents : MagicTarget<GlobalComponents> {
	aVect<Component> component;

	MagicCopyDecrementer copyDecrementer;
};

struct GlobalBip : MagicTarget<GlobalBip> {
	aMat<double> bip;

	MagicCopyDecrementer copyDecrementer;
};

#ifdef _DEBUG
#define DEBUG_MODE 1
#else
#define DEBUG_MODE 0
#endif

#define FLUIDCOMPO_DBG_STR(str) if (DEBUG_MODE) DbgStr(str "\n")

struct FluidComposition;

struct PhasesEquilibrium {

	int updatedBitField = 0;

	FluidComposition * PhasesEquilibrium::Get_Parent();

	bool Update_aj();
	bool Update_aj_prime();
	bool Update_aj_second();
	bool Update_bj();
	bool Update_Aj();
	bool Update_Bj();
	bool Update_xij();
	bool Update_Ki();
	bool Update_psi();
	bool Update_phi_ij();
	bool Update_Zj();
	void DoNothing();

#ifdef _WIN64
	TestField<1,  8,   double[2],			 PhasesEquilibrium, &Update_aj,		   &DoNothing>		aj;
	TestField<2,  24,  double[2],			 PhasesEquilibrium, &Update_aj_prime,  &DoNothing>		aj_prime;
	TestField<3,  40,  double[2],			 PhasesEquilibrium, &Update_aj_second, &DoNothing>		aj_second;
	TestField<4,  56,  double[2],			 PhasesEquilibrium, &Update_bj,		   &DoNothing>		bj;
	TestField<5,  72,  double[2],			 PhasesEquilibrium, &Update_Aj,		   &DoNothing>		Aj;
	TestField<6,  88,  double[2],			 PhasesEquilibrium, &Update_Bj,		   &DoNothing>		Bj;
	TestField<7,  104, aVect<double>[2],	 PhasesEquilibrium, &Update_xij,	   &DoNothing>		xij;
	TestField<8,  152, aVect<double>,		 PhasesEquilibrium, &Update_Ki,		   &DoNothing>		Ki;
	TestField<9,  176, double,				 PhasesEquilibrium, &Update_psi,	   &DoNothing>		psi;
	TestField<10, 184, aVect<double>[2],	 PhasesEquilibrium, &Update_phi_ij,	   &DoNothing>		phi_ij;
	TestField<11, 232, double[2],			 PhasesEquilibrium, &Update_Zj,		   &DoNothing>		Zj;
#else
	TestField<1,  8,   double[2],			 PhasesEquilibrium, &Update_aj,		   &DoNothing>		aj;
	TestField<2,  24,  double[2],			 PhasesEquilibrium, &Update_aj_prime,  &DoNothing>		aj_prime;
	TestField<3,  40,  double[2],			 PhasesEquilibrium, &Update_aj_second, &DoNothing>		aj_second;
	TestField<4,  56,  double[2],			 PhasesEquilibrium, &Update_bj,		   &DoNothing>		bj;
	TestField<5,  72,  double[2],			 PhasesEquilibrium, &Update_Aj,		   &DoNothing>		Aj;
	TestField<6,  88,  double[2],			 PhasesEquilibrium, &Update_Bj,		   &DoNothing>		Bj;
	TestField<7,  104, aVect<double>[2],	 PhasesEquilibrium, &Update_xij,	   &DoNothing>		xij;
	TestField<8,  128, aVect<double>,		 PhasesEquilibrium, &Update_Ki,		   &DoNothing>		Ki;
	TestField<9,  144, double,				 PhasesEquilibrium, &Update_psi,	   &DoNothing>		psi;
	TestField<10, 152, aVect<double>[2],	 PhasesEquilibrium, &Update_phi_ij,	   &DoNothing>		phi_ij;
	TestField<11, 176, double[2],			 PhasesEquilibrium, &Update_Zj,		   &DoNothing>		Zj;
#endif
};

template <bool assumeNoZeroZi>
double RRFunc(double x, const aVect<double>& Ki, const aVect<double>& zi) {
	
	double sum = 0;
	auto I = zi.Count();

	if (x == 0) {
		for (size_t i = 0; i < I; ++i) {
			sum += zi[i] * (Ki[i] - 1);
		}
	} else if (x == 1) {
		for (size_t i = 0; i < I; ++i) {
			if (assumeNoZeroZi || zi[i]) sum += zi[i] * (Ki[i] - 1) / Ki[i];
		}
	} else {
		for (size_t i = 0; i < I; ++i) {
			if (assumeNoZeroZi || zi[i]) sum += zi[i] * (Ki[i] - 1) / (1 + x * (Ki[i] - 1));
		}
	}

	return sum;
};

double HP_Flash_Func(double T, FluidComposition& f, double P);
double VP_Flash_Func(double T, FluidComposition& f, double P);
double VVFT_Flash_Func(double P, FluidComposition& f, double T);

struct BipData {
	aMat<double> bip;
	aVect<aVect<char>> names_i;
	aVect<aVect<char>> names_j;
};

struct FluidComposition {
	
#define BIT_INDEX(f) BIT(decltype(FluidComposition::f)::indexValue)

	__int64 updatedBitField = 0;

	bool Update_components() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, components);

		this->zi.GetRef();	//special case: zi update takes care of filtering out components that have zi = 0

		return true;
	}

	bool Update_bip() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, bip);

		this->zi.GetRef(); ////special case: zi update takes care of filtering out bip that have zi = 0

		return false;
	}

	const aVect<Component> & GetComponentsRef() const {
		if (this->pGlobalComponents) return this->pGlobalComponents->component;
		else return this->components.GetRef();
	}
	
	const aMat<double> & GetBipRef() const {
		if (this->pGlobalBip) return this->pGlobalBip->bip;
		else return this->bip.GetRef();
	}

	auto TestBinaryOption(int opt) {
		return this->binaryOptions & opt;
	}

	auto Option_AssumeNoZeroZi() {
		return TestBinaryOption(BinaryOptions::assumeNoZeroZi);
	}

	auto Option_RachfordRice_UseNewtonSolver() {
		return TestBinaryOption(BinaryOptions::rachfordRice_useNewtonSolver);
	}

	auto Option_PhasesEquilibrium_InitFromPreviousState() {
		return TestBinaryOption(BinaryOptions::phasesEquilibrium_initFromPreviousState);
	}

	auto Option_UseVolumeCorrection_Peneloux() {
		return TestBinaryOption(BinaryOptions::useVolumeCorrection_Peneloux);
	}

	auto Option_UseVolumeCorrection_COSTALD() {
		return TestBinaryOption(BinaryOptions::useVolumeCorrection_COSTALD);
	}

	auto Option_COSTALD_PressureCorrection_Prausnitz() {
		return TestBinaryOption(BinaryOptions::COSTALD_PressureCorrection_Prausnitz);
	}

	auto Option_Peneloux_Eq1() {
		return TestBinaryOption(BinaryOptions::peneloux_Eq1);
	}

	void Option_Peneloux_Eq1(bool enable) {
		this->SetBinaryOption(BinaryOptions::peneloux_Eq1, enable);
		this->V_shift_j.Invalidate();
	}

	void Invalidate_components() {

		constexpr __int64 validMask = 
			BIT_INDEX(P) | BIT_INDEX(T) | BIT_INDEX(bipData);

		this->updatedBitField &= validMask;
	}

	template <bool reset_assumeNoZeroZi = true>
	void Invalidate_zi() {

		constexpr __int64 validMask = 
		BIT_INDEX(T)     | BIT_INDEX(P)		   | BIT_INDEX(components)      |
		BIT_INDEX(bip)   | BIT_INDEX(bipData)  | BIT_INDEX(orig_components) |
		BIT_INDEX(Hid_i) | BIT_INDEX(orig_bip) | BIT_INDEX(orig_zi);

		this->updatedBitField &= validMask;
		this->AssumeNoZeroZiCore(false);
	}

	bool Update_T() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, T);
		return false;
	}

	bool Update_T_ref() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, T_ref);
		return false;
	}

	bool Update_P_ref() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, P_ref);
		return false;
	}

	bool Update_orig_components() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, orig_components);
		return false;
	}

	void Invalidate_orig_components() {
		
		constexpr __int64 validMask = 
		BIT_INDEX(T)   | BIT_INDEX(P) | BIT_INDEX(bipData);

		this->updatedBitField &= validMask;
	}

	bool Update_orig_bip() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, orig_bip);

		auto&& bipData  = this->bipData.GetRef();
		auto&& orig_compo  = this->orig_components.GetRef();
		auto&& orig_bip = this->orig_bip.GetRefForUpdate();

		auto I = orig_compo.Count();

		orig_bip.Redim(I, I);

		aVect_static_for(orig_compo, i) {
			aVect_static_for(orig_compo, j) {
				auto FindIndex = [](const aVect<aVect<char>>& names, const char * str) {
					for (auto&& name : names) {
						if (strcmp_caseInsensitive(name, str) == 0) {
							return (int)names.Index(name);
						}
					}
					return -1;
				};

				auto data_i = FindIndex(bipData.names_i, orig_compo[i].name);
				auto data_j = FindIndex(bipData.names_j, orig_compo[j].name);

				if (data_i < 0 || data_j < 0) throw std::runtime_error("missing bip component");

				orig_bip(i, j) = bipData.bip(data_i, data_j);
			}
		}

		return true;
	}
	
	void Invalidate_orig_bip() {

		__debugbreak(); //not a root, not user specifiable
	}

	bool Update_orig_zi() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, orig_zi);
		return false;
	}

	void Invalidate_orig_zi() {

		constexpr __int64 validMask = 
		BIT_INDEX(T) | BIT_INDEX(P) | BIT_INDEX(orig_bip) | BIT_INDEX(bipData) | BIT_INDEX(orig_components);

		this->updatedBitField &= validMask;
	}

	bool Update_bipData() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, bipData);
		return false;
	}

	void Invalidate_bipData() {

		constexpr __int64 validMask =
			BIT_INDEX(T) | BIT_INDEX(P) | BIT_INDEX(orig_zi) |
			BIT_INDEX(orig_components)  | BIT_INDEX(Hid_i);

		this->updatedBitField &= validMask;
	}

	bool Update_H0_j() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, H0_j);

		//??????????????????????????????

		return true;
	}

	void Invalidate_referenceState() {

		//this->H_ref.Invalidate();
	}

	void Invalidate_T() {

		constexpr __int64 validMask = 
		BIT_INDEX(P)   |
		BIT_INDEX(bi)  | BIT_INDEX(ci)			    | BIT_INDEX(zi)        | BIT_INDEX(components) |
		BIT_INDEX(bip) | BIT_INDEX(orig_components) | BIT_INDEX(orig_bip)  | BIT_INDEX(orig_zi)    |
		BIT_INDEX(bipData);

		this->updatedBitField &= validMask;
	}

	bool Update_P() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, P);
		return false;
	}

	bool Update_zi() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, zi);

		auto&& orig_bip    = this->orig_bip.GetRef();
		auto&& orig_zi     = this->orig_zi.GetRef();
		auto&& orig_compo  = this->orig_components.GetRef();
		
		auto&& zi    = this->zi.GetRefForUpdate();
		auto&& compo = this->components.GetRefForUpdate<false>();
		auto&& bip   = this->bip.GetRefForUpdate<false>();

		auto nonZeroZi = 0;

		aVect_static_for(orig_zi, i) {
			if (orig_zi[i]) nonZeroZi++;
		}

		zi.Redim(nonZeroZi);
		compo.Redim(nonZeroZi);
		bip.Redim(nonZeroZi, nonZeroZi);

		size_t i = 0;

		aVect_static_for(orig_zi, orig_i) {
			if (orig_zi[orig_i]) {
				zi[i] = orig_zi[orig_i];
				compo[i] = orig_compo[orig_i];
				size_t j = 0;
				aVect_static_for(orig_zi, orig_j) {
					if (orig_zi[orig_j]) {
						bip(i, j) = orig_bip(orig_i, orig_j);
						j++;
					}
				}
				i++;
			}
		}

		this->AssumeNoZeroZiCore(true);

		return true;
	}

	void Invalidate_P() {

		constexpr __int64 validMask = 
		BIT_INDEX(T)     | BIT_INDEX(ai)			  | BIT_INDEX(ai_prime) | BIT_INDEX(ai_second)  |
		BIT_INDEX(bi)    | BIT_INDEX(ci)			  | BIT_INDEX(zi)       | BIT_INDEX(components) |
		BIT_INDEX(bip)   | BIT_INDEX(orig_components) | BIT_INDEX(orig_bip) | BIT_INDEX(orig_zi)    |
		BIT_INDEX(Hid_i) | BIT_INDEX(bipData);

		this->updatedBitField &= validMask;
	}

	void Invalidate_Vj() {

		constexpr __int64 validMask = 
		BIT_INDEX(T)     | BIT_INDEX(ai)			  | BIT_INDEX(ai_prime) | BIT_INDEX(ai_second)		   |
		BIT_INDEX(bi)    | BIT_INDEX(ci)			  | BIT_INDEX(zi)       | BIT_INDEX(components)		   |
		BIT_INDEX(bip)   | BIT_INDEX(orig_components) | BIT_INDEX(orig_bip) | BIT_INDEX(orig_zi)		   |
		BIT_INDEX(Hid_i) | BIT_INDEX(bipData)		  | BIT_INDEX(Ki_init)  | BIT_INDEX(phasesEquilibrium) |
		BIT_INDEX(mW_j)  | BIT_INDEX(cp_id_i)		  | BIT_INDEX(P);

		this->updatedBitField &= validMask;
	}

	void Invalidate_phasesEquilibrium() {

		constexpr __int64 validMask = 
		BIT_INDEX(T)     | BIT_INDEX(ai)			  | BIT_INDEX(ai_prime) | BIT_INDEX(ai_second)	|
		BIT_INDEX(bi)    | BIT_INDEX(ci)			  | BIT_INDEX(zi)       | BIT_INDEX(components)	|
		BIT_INDEX(bip)   | BIT_INDEX(orig_components) | BIT_INDEX(orig_bip) | BIT_INDEX(orig_zi)	|
		BIT_INDEX(Hid_i) | BIT_INDEX(bipData)		  | BIT_INDEX(Ki_init)  | BIT_INDEX(cp_id_i)    |
		BIT_INDEX(P);

		this->updatedBitField &= validMask;
	}

	bool Update_ai() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, ai);

		auto&& compo   = this->GetComponentsRef();
		auto&& aci     = this->aci.GetRef();
		auto   T       = this->T.GetRef();
		auto&& kappa_i = this->kappa_i.GetRef();
		auto&& ai      = this->ai.GetRefForUpdate();

		ai.Redim(compo.Count());

		aVect_static_for(compo, i) {
			auto alph_i = Sqr(1 + kappa_i[i] * (1 - sqrt(T / compo[i].Tc)));
			ai[i] = aci[i] * alph_i;
		}

		return true;
	}

	bool Update_ai_prime() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, ai_prime);

		auto&& compo	= this->GetComponentsRef();
		auto&& kappa_i  = this->kappa_i.GetRef();
		auto&& aci		= this->aci.GetRef();
		auto   T		= this->T.GetRef();
		auto&& ai_prime = this->ai_prime.GetRefForUpdate();

		ai_prime.Redim(compo.Count());

		aVect_static_for(compo, i) {
			auto&& compo_i = compo[i];
			auto kappa = kappa_i[i];
			ai_prime[i] = - kappa *aci[i]*(1 + kappa*(1 - sqrt(T / compo_i.Tc))) / sqrt(T * compo_i.Tc);
		}

		return true;
	}

	bool Update_ai_second() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, ai_second);

		auto&& compo	 = this->GetComponentsRef();
		auto&& aci		 = this->aci.GetRef();
		auto   T		 = this->T.GetRef();
		auto&& ai_second = this->ai_second.GetRefForUpdate();
		auto&& kappa_i   = this->kappa_i.GetRef();

		ai_second.Redim(compo.Count());

		aVect_static_for(compo, i) {
			auto&& compo_i = compo[i];
			auto kappa = kappa_i[i];
			ai_second[i] = aci[i] * kappa * sqrt(compo_i.Tc / T) * (1 + kappa) / (2 * T * compo_i.Tc);
		}

		return true;
	}

	bool Update_bi() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, bi);

		auto&& compo = this->GetComponentsRef();
		auto&& bi = this->bi.GetRefForUpdate();

		bi.Redim(compo.Count());

		aVect_static_for(compo, i) {
			auto&& compo_i = compo[i];
			bi[i] = 0.0777960739039 * R * compo_i.Tc / compo_i.Pc;
		}
		
		return true;
	}
	
	bool Update_ci() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, ci);

		auto&& compo = this->GetComponentsRef();
		auto&& ci = this->ci.GetRefForUpdate();

		ci.Redim(compo.Count());

		if (this->Option_UseVolumeCorrection_Peneloux()) {
			if (this->Option_Peneloux_Eq1()) {
				aVect_static_for(compo, i) {
					auto&& compo_i = compo[i];
					ci[i] = 0.50033 * R * compo_i.Tc / compo_i.Pc * (0.25969 - compo_i.Z_ra);
				}
			} else {
				aVect_static_for(compo, i) {
					auto&& compo_i = compo[i];
					ci[i] = (-0.440642 * compo_i.Z_ra + 0.1153758) * R * compo_i.Tc / compo_i.Pc;
				}
			}
		} else /*if (this->Option_UseVolumeCorrection_COSTALD()) */ {
			MY_ERROR("ci: expected peneloux");
		}

		return true;
	}

	bool Update_Ki_init() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, Ki_init);

		auto&& compo   = this->GetComponentsRef();
		auto   T	   = this->T.GetRef();
		auto   P	   = this->P.GetRef();
		auto&& Ki_init = this->Ki_init.GetRefForUpdate();

		Ki_init.Redim(compo.Count());

		aVect_static_for(Ki_init, i) {
			auto&& compo_i = compo[i];
			Ki_init[i] = compo_i.Pc / P * exp(-log(10.) / (1 - 1 / 0.7) * (1 + compo_i.omega) * (1 - compo_i.Tc / T));
		}
		
		return true;
	}

	bool Update_aci() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, aci);

		auto&& compo = this->GetComponentsRef();
		auto&& aci = this->aci.GetRefForUpdate();

		aci.Redim(compo.Count());

		aVect_static_for(compo, i) {
			auto&& compo_i = compo[i];
			aci[i] = 0.457235528921 * Sqr(R) * Sqr(compo_i.Tc) / compo_i.Pc;
		}

		return true;
	}

	bool Update_kappa_i() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, kappa_i);

		auto&& compo = this->GetComponentsRef();
		auto&& kappa_i = this->kappa_i.GetRefForUpdate();
		
		kappa_i.Redim(compo.Count());

		aVect_static_for(kappa_i, i) {
			auto&& omega_i = compo[i].omega;
			kappa_i[i] = omega_i <= 0.491 ?
				0.37464 + (1.54226 - 0.26992*omega_i)*omega_i :
				0.379642 + (1.48503 - (0.164423 - 0.016666*omega_i)*omega_i)*omega_i;
		}

		return true;
	}

	template <bool noZeroZi>
	bool Update_phasesEquilibrium_Core() {

		auto&& compo   = this->GetComponentsRef();
		auto&& pe      = this->phasesEquilibrium.GetRefForUpdate();

		auto prevInit = this->Option_PhasesEquilibrium_InitFromPreviousState() && pe.Ki.IsUpdated() && _finite(pe.Ki.value[0]);

		auto&& Ki_init = prevInit ? this->Ki_init.value : this->Ki_init.GetRef();
		auto&& zi      = this->zi.GetRef();
		auto&& Ki  = pe.Ki.GetRefForUpdate();
		auto&& psi = pe.psi.GetRefForUpdate();
		auto&& phi = pe.phi_ij.GetRefForUpdate();
		auto&& xij = pe.xij.GetRefForUpdate();

		auto I = compo.Count();

		auto&& xi = xij[0];
		auto&& yi = xij[1];

		xi.Redim(I);
		yi.Redim(I);

		if (!prevInit) Ki = Ki_init;
		else Ki.Redim(zi.Count());

		//TEST DBG
//#ifdef _DEBUG
//		{
//			xi = zi;
//			yi = zi;
//
//			auto&& Aj = pe.Aj.GetRef();
//			auto&& Bj = pe.Bj.GetRef();
//			 
//			auto Z = GetRoots(Aj[0], Bj[0]);
//			
//			if (Z.onlyOneRoot) {
//				DbgStr("One phase ? T = %g, P = %g, only Z root = %g\n", T, P, Z.z1);
//			}
//			else {
//				DbgStr("Two phases ? T = %g, P = %g, Z1 = %g, Z2 = %g\n", T, P, Z.z1, Z.z3);
//			}
//
//		}
//#endif

		//double sum_ksi0 = 0;
		//double sum_ksi10 = 0;

		//aVect_static_for(Ki, i) sum_ksi0  += zi[i] / Ki[i];
		//aVect_static_for(Ki, i) sum_ksi10 += zi[i] * Ki[i];

		//if (sum_ksi10 < 1 || sum_ksi0 < 1) {
		//	//one phase ?

		//	xi = zi;

		//	auto&& A = pe.Aj.GetRef()[0];
		//	auto&& B = pe.Bj.GetRef()[0];
		//	auto&& ai = this->ai.GetRef();
		//	auto&& bi = this->bi.GetRef();
		//	
		//	auto Z = GetRoots(A, B);

		//	static aVect<double> phi_0, phi_2;

		//	phi_0.Redim(xi.Count());
		//	phi_2.Redim(xi.Count());

		//	ComputePhi(phi_0, ai, bi, xi, bip, Z.z1)
		//}

		bool isConverged;

		for (size_t nIter = 0; nIter < 1000; nIter++) {

			bool isSolverConverged;
			double solverResult;

			if (this->Option_RachfordRice_UseNewtonSolver()) {
				this->newton_solver->Solve<false>(RRFunc<noZeroZi>, Ki, zi);
				isSolverConverged = this->newton_solver->isConverged;
				solverResult = this->newton_solver->result;
			} else {
				auto oldMaxRecursion = this->dicotomy_solver->maxRecursion;
				this->dicotomy_solver->maxRecursion = 0;
				this->dicotomy_solver->Solve<decltype(RRFunc<noZeroZi>), RRFunc<noZeroZi>, false, true>(Ki, zi);
				this->dicotomy_solver->maxRecursion = oldMaxRecursion;
				isSolverConverged = this->dicotomy_solver->isConverged;
				solverResult = this->dicotomy_solver->result;
			}

			if (isSolverConverged) {
				psi = solverResult;
			} else {
				auto RR0 = RRFunc<noZeroZi>(0, Ki, zi);
				auto RR1 = RRFunc<noZeroZi>(1, Ki, zi);

				psi = abs(RR0) < abs(RR1) ? 0 : 1;
			}

			for (size_t i = 0; i < I; ++i) {
				xi[i] = !noZeroZi && zi[i] == 0 ? 0 : zi[i] / (1. + psi * (Ki[i] - 1));
				yi[i] = Ki[i] * xi[i];
			}

			pe.updatedBitField = 0;
			pe.xij.SetUpdated();

			auto&& Zj = pe.Zj.GetRef();

			if (psi == 1 && Zj[0] < 0) {
				isConverged = true;
			} else {

				auto&& phi_ij = pe.phi_ij.GetRef();
				auto&& phi_Liq = phi_ij[0];
				auto&& phi_Vap = phi_ij[1];

				for (size_t i = 0; i < I; ++i) {
					//Ki[i] = !noZeroZi && phi_Liq[i] == 0 ? 0 : 0.6 * Ki[i] + 0.4 * Ki[i] * phi_Liq[i] / phi_Vap[i];
					Ki[i] = !noZeroZi && phi_Liq[i] == 0 ? 0 : Ki[i] * phi_Liq[i] / phi_Vap[i];
				}

				isConverged = true;
				for (size_t i = 0; i < I; ++i) {
					if (noZeroZi || zi[i]) {
						//if (abs(phi_Vap[i] / phi_Liq[i] - 1) > this->phasesEquilibriumSolverTolerance
						//	&& xi[i] > 1e-10 && yi[i] > 1e-10)

						if (!IsEqualWithPrec(phi_Vap[i], phi_Liq[i], this->phasesEquilibriumSolverTolerance)
							&& xi[i] > 1e-10 && yi[i] > 1e-10)
						{
							isConverged = false;
							break;
						}
					}
				}
			}

			if (isConverged) {

#ifdef _DEBUG
				//DbgStr("phases eq convengence ok (%g °C, %g bar): psi = %g, nIter = %d\n", this->T.value - 273.15, this->P.value /1e5, psi, nIter);
#endif

				if (IsEqualWithPrec(Zj[0], Zj[1], 1e-6)) psi = 1;

				pe.Ki.SetUpdated();
				pe.psi.SetUpdated();
				pe.xij.SetUpdated();

				break;
			}
		}

		if (!isConverged) {
			if (psi == 1 || psi == 0) {
				pe.Ki.SetUpdated();
				pe.psi.SetUpdated();
				pe.xij.SetUpdated();
			}
			else {
#if defined(THERMO_PERMISSIF_RACHFORDRICE) || defined(THERMO_PERMISSIF_RACHFORDRICE2)
				auto&& phi_ij = this->phasesEquilibrium.value.phi_ij.GetRef();
				auto&& phi_Liq = phi_ij[0];
				auto&& phi_Vap = phi_ij[1];

#ifdef THERMO_PERMISSIF_RACHFORDRICE2
				auto newtol = sqrt(this->phasesEquilibriumSolverTolerance);
#else
				auto newtol = 0.1;
#endif
				for (size_t i = 0; i < I; ++i) {
					if (noZeroZi || zi[i]) {
						if (abs(phi_Vap[i] / phi_Liq[i] - 1) > newtol
							&& xi[i] > 1e-10 && yi[i] > 1e-10)
						{
							throw std::runtime_error("Rachford rice: no convergence"); //isConverged = false;
							break;
						}
					}
				}

				{
					static bool init;
					static double T, P;

					if (init) {
						if (T != this->T.value || P != this->P.value) {
							printf("phase equilibrium: convergence not reached (%g °C, %g Bar)\n", this->T.value - 273.15, this->P.value / 1e5);
							T = this->T.value;
							P = this->P.value;
						}
					}

					init = true;
				}

#else
				throw std::runtime_error("Rachford rice: no convergence"); 
#endif

				pe.Ki.SetUpdated();
				pe.psi.SetUpdated();
				pe.xij.SetUpdated();
			}
		}

		return true;
	}

	bool Update_phasesEquilibrium() {

		DEBUG_THERMO_THREADSAFE

		COMPILE_TIME_OFFSET_CHECK(FluidComposition, phasesEquilibrium);

		return this->Option_AssumeNoZeroZi() ? 
			this->Update_phasesEquilibrium_Core<true>() : 
			this->Update_phasesEquilibrium_Core<false>();
	}
	
	bool Update_Mw_j() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, mW_j);

		auto&& compo = this->GetComponentsRef();
		auto&& xij   = this->phasesEquilibrium.GetRef().xij.value;
		auto&& mW_j  = this->mW_j.GetRefForUpdate();

		for (size_t j = 0; j < Count(xij); ++j) {
			auto&& xi = xij[j];

			double sum = 0;
			aVect_static_for(compo, i) sum += compo[i].mW * xi[i];

			mW_j[j] = sum;
		}

		return true;
	}

	bool Update_V_shift_j() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, V_shift_j);
		 
		auto&& pe = this->phasesEquilibrium.GetRef();
		auto&& V_shift_j = this->V_shift_j.GetRefForUpdate();
		auto&& xij = pe.xij.value;
		auto&& ci = this->ci.GetRef();

		for (size_t j = 0; j < Count(V_shift_j); ++j) {
			auto&& xi = xij[j];

			double sum = 0;
			aVect_static_for(ci, i) sum += ci[i] * xi[i];

			V_shift_j[j] = -sum;
		}

		return true;
	}

	bool Update_Vj() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, Vj);

		//auto&& pe        = this->phasesEquilibrium.value;
		auto&& pe        = this->phasesEquilibrium.GetRef();
		auto&& Zj        = pe.Zj.value;
		
		auto&& Vj		 = this->Vj.GetRefForUpdate();

		auto   T = this->T.value;
		auto   P = this->P.value;
		
		for (size_t j = 0; j < Count(Vj); ++j) {
			Vj[j] = Zj[j] * R * T / P;
		}

		if (this->Option_UseVolumeCorrection_COSTALD()) {
			if (this->Option_UseVolumeCorrection_Peneloux()) {
				MY_ERROR("COSTALD and Peneloux: options are not compatible");
			}

			//from http://docs.chejunkie.com/wp-content/uploads/sites/2/2014/11/AspenPhysPropModelsV8_4-Ref.pdf

			auto T = this->T.GetRef();
			auto&& xi = pe.xij.GetRef()[0];
			//decltype(pe.Aj.value) rho_j;
			double rho_liq = 0;
			auto&& compo = this->components.GetRef();

			double s1 = 0;
			double s2 = 0;
			double s3 = 0;

			auto I = compo.Count();

			for (size_t i = 0; i<I; ++i) {
				auto x = xi[i];
				auto V_CTD = compo[i].V_CTD;
				if (!_finite(V_CTD)) throw std::runtime_error(xFormat("Missing costald volume for component %s", compo[i].name));
				s1 += x * V_CTD;
				s2 += x * pow(V_CTD, 2.0 / 3.0);
				s3 += x * pow(V_CTD, 1.0 / 3.0);
			}

			//HYSYS documentation formula is wrong ??
			//auto V_m = 0.25 * (s1 + 3 * s2) * s3;
			
			auto V_m = 0.25 * (s1 + 3 * s2 * s3);

			double s = 0;

			//TODO: optimize (recompute only if components/composition has changed)
			for (size_t i = 0; i < I; ++i) {
				for (size_t j = 0; j < I; ++j) {
					s += xi[i] * xi[j] * sqrt(compo[i].V_CTD * compo[i].Tc * compo[j].V_CTD * compo[j].Tc);
				}
			}

			auto Tc = s / V_m;

			double omega = 0;

			for (size_t i = 0; i < I; ++i) {
				auto omega_CTD = compo[i].omega_CTD;
				if (!_finite(omega_CTD)) throw std::runtime_error(xFormat("Missing COSTALD acentric factor for component %", compo[i].name));
				omega += xi[i] * omega_CTD;
			}

			auto Tr = T / Tc;

			//HYSYS method, keep EOS if Tr > 1, interpolate if Tr is between 0.95 and 1
			if (Tr < 1) {

				auto Tr2 = Tr * Tr;
				auto Tr3 = Tr * Tr2;
				auto v_0 = 1 - 1.52816 * pow(1 - Tr, 1.0 / 3.0) + 1.43907 * pow(1 - Tr, 2.0 / 3.0) - 0.81446 * (1 - Tr) + 0.190454 * pow(1 - Tr, 4.0 / 3.0);
				auto v_1 = (-0.296123 + 0.386914 * Tr - 0.042725 * Tr2 - 0.0480645 * Tr3) / (Tr - 1.00001);

				auto V_COSTALD_sat = V_m * v_0 * (1 - omega * v_1);
				double V_COSTALD_compressed;

				auto Z_c = 0.30744752401796566;
				double V_cm = 0;
				for (size_t i = 0; i < I; ++i) {
					V_cm += xi[i] * Z_c * compo[i].Tc / compo[i].Pc;
				}

				double T_cm = 0;
				for (size_t i = 0; i < I; ++i) {
					T_cm += xi[i] * Z_c * pow(compo[i].Tc, 3.0 / 2.0) / compo[i].Pc;
				}

				T_cm *= T_cm / Sqr(V_cm);
				auto P_cm = Z_c * T_cm / V_cm;

				double omega_m = 0;
				for (size_t i = 0; i < I; ++i) {
					omega_m += xi[i] * compo[i].omega;
				}

				auto T_rm = T / T_cm;
				auto P_sat = P_cm * exp(-log(10.0) / (1.0 - 1.0 / 0.7) * (1 + omega_m) * (1.0 - 1.0 / T_rm));
				
				if (this->Option_COSTALD_PressureCorrection_Prausnitz()) {

					auto T_rm2 = T_rm * T_rm;
					auto T_rm3 = T_rm * T_rm2;
					auto T_rm4 = T_rm * T_rm3;
					auto f = 6.9547 - 76.2853 * T_rm + 191.306 * T_rm2 - 203.5472 * T_rm3 + 82.7631 * T_rm4;
					auto N = (1 - 0.89*Sqr(omega_m))*exp(f);

					V_COSTALD_compressed = V_COSTALD_sat / pow(1 + 9 * Z_c * N * (P - P_sat) / P_cm, 1.0/9.0);
				} else {

					auto tau = 1 - T / Tc;
					auto e = exp(4.79594 + 0.250047*omega + 1.14188*Sqr(omega));
					auto C = 0.0861488 + 0.0344483*omega;
					auto B = P_cm * (-1 -9.070217 * pow(tau, 1.0 / 3.0) + 62.45326 * pow(tau, 2.0 / 3.0) + -135.1102 * tau + e*pow(tau, 4.0 / 3.0));
					V_COSTALD_compressed = V_COSTALD_sat * (1 - C*log((B + P) / (B + P_sat)));

				}

				if (Tr < 0.95) {
					Vj[0] = V_COSTALD_compressed;
				} else {
					auto alpha = (Tr - 0.95) / (1.0 - 0.95);
					Vj[0] = V_COSTALD_compressed * (1 - alpha) + Vj[0] * alpha;
				}
			} else {
				/* do nothing, keep Vj[0] unchanged */
			}
		}
		else {
			if (this->Option_UseVolumeCorrection_Peneloux()) {
				auto&& V_shift_j = this->V_shift_j.GetRef();

				for (size_t j = 0; j < Count(Vj); ++j) {
					Vj[j] -= V_shift_j[j];
				}
			}
		}

		return true;
	}

	bool Update_rho_j() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, rho_j);

		auto&& mW_j  = this->mW_j.GetRef();
		auto&& Vj    = this->Vj.GetRef();
		auto&& rho_j = this->rho_j.GetRefForUpdate();

		for (size_t j = 0; j < Count(rho_j); ++j) {
			rho_j[j] = mW_j[j] / Vj[j];
		}

		return true;
	}

	bool Update_FracVolVap() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, FracVolVap);

		auto&& Vj		  = this->Vj.GetRef();
		auto&& psi		  = this->phasesEquilibrium.value.psi.value;
		auto&& FracVolVap = this->FracVolVap.GetRefForUpdate();

		FracVolVap = psi*Vj[1] / ((1 - psi)*Vj[0] + psi*Vj[1]);

		return true;
	}

	bool Update_FracMassVap() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, FracMassVap);

		auto&& mW_j		   = this->mW_j.GetRef();
		auto&& psi		   = this->phasesEquilibrium.value.psi.value;
		
		auto&& FracMassVap = this->FracMassVap.GetRefForUpdate();

		FracMassVap = psi * mW_j[1] / ((1 - psi)*mW_j[0] + psi*mW_j[1]);

		return true;
	}

	bool Update_Hid_i() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, Hid_i);

		auto&& compo = this->GetComponentsRef();
		auto&& Hid_i = this->Hid_i.GetRefForUpdate();

		auto   T = this->T.value;

		Hid_i.Redim(compo.Count());
		 
		aVect_static_for(compo, i) Hid_i[i] = compo[i].H_id(T);

		return true;
	}

	bool Update_cp_id_i() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, cp_id_i);

		auto&& compo   = this->GetComponentsRef();
		auto&& cp_id_i = this->cp_id_i.GetRefForUpdate();

		auto   T = this->T.value;

		cp_id_i.Redim(compo.Count());

		aVect_static_for(compo, i) cp_id_i[i] = compo[i].Cp_id(T);

		return true;
	}
	
	bool Update_H_j() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, H_j);
		
		auto&& Hid_i = this->Hid_i.GetRef();
		auto&& pe = this->phasesEquilibrium.GetRef();
		auto&& Zj = pe.Zj.value;
		auto&& aj = pe.aj.value;
		auto&& Aj = pe.Aj.value;
		auto&& aj_prime = pe.aj_prime.GetRef();
		auto&& bj   = pe.bj.value;
		auto&& Bj   = pe.Bj.value;
		auto&& xij  = pe.xij.value;
		auto&& H_j  = this->H_j.GetRefForUpdate();
		auto&& mW_j = this->mW_j.GetRef();
		auto&& Vj = this->Vj.GetRef();

		auto   T = this->T.value;
		auto   P = this->P.value;

		for (size_t j = 0; j < Count(Zj); ++j) {
			const double sqrt_8 = 2.82842712474619, sqrt_2 = 1.414213562373;

			double Z = Zj[j], a_prime = aj_prime[j], a = aj[j], b = bj[j], B = Bj[j], V = Vj[j];

			//double H_dep = R*T*(Z - 1) + (T*a_prime - a) / (sqrt_8*b) * log((Z + (1 + sqrt_2)*B) / (Z + (1 - sqrt_2)*B));

			double H_dep = P*V - R*T - (a - T*a_prime) / (sqrt_8*b) * log((V + (1 + sqrt_2)*b) / (V + (1 - sqrt_2)*b));

			auto&& xi = xij[j];

			double sum = 0;
			aVect_static_for(Hid_i, i) sum += Hid_i[i] * xi[i];

			H_j[j] = (sum + H_dep) / mW_j[j];
		}

		return true;
	}

	bool Update_sigma_j() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, sigma_j);
		
		auto&& compo   = this->GetComponentsRef();
		auto&& xij     = this->phasesEquilibrium.GetRef().xij.GetRef();

		auto   T = this->T.value;

		auto&& sigma_j = this->sigma_j.GetRefForUpdate();

		for (size_t j = 0; j < Count(sigma_j); ++j) {
			double sum = 0;
			auto&& xi = xij[j];
			aVect_static_for(compo, i) sum += xi[i] * compo[i].Sigma(T);
			sigma_j[j] = sum;
		}
		
		return true;
	}

	bool Update_dZ_dT_P_j() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, dZ_dT_P_j);

		auto&& compo    = this->GetComponentsRef();
		auto&& pe	    = this->phasesEquilibrium.GetRef();
		auto&& aj       = pe.aj.value;
		auto&& aj_prime = pe.aj_prime.GetRef();
		auto&& bj       = pe.bj.value;
		auto&& Aj       = pe.Aj.value;
		auto&& Bj       = pe.Bj.value;
		auto&& Zj       = pe.Zj.value;

		auto&& dZ_dT_P_j = this->dZ_dT_P_j.GetRefForUpdate();

		auto T = this->T.value, P = this->P.value;

		for (size_t j = 0; j < Count(dZ_dT_P_j); ++j) {
			
			auto B = Bj[j], Z = Zj[j];

			dZ_dT_P_j[j] = ((P / Sqr(R*T) * (aj_prime[j] - 2.*aj[j] / T))*(B - Z) - bj[j]*P/(R*T*T)
				*(6*B*Z + 2*Z - 3*Sqr(B) - 2*B + Aj[j] - Sqr(Z))) / (3*Sqr(Z) + 2*(B - 1)*Z + Aj[j] - 2*B - 3*Sqr(B));

		}

		return true;
	}
	
	bool Update_dV_dT_P_j() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, dV_dT_P_j);

		auto&& dZ_dT_P_j = this->dZ_dT_P_j.GetRef();
		auto&& Zj		 = this->phasesEquilibrium.value.Zj.value;
		auto&& dV_dT_P_j = this->dV_dT_P_j.GetRefForUpdate();
		
		auto T = this->T.value, P = this->P.value;

		for (size_t j = 0; j < Count(dV_dT_P_j); ++j) {

			dV_dT_P_j[j] = R / P * (T * dZ_dT_P_j[j] + Zj[j]);
		}

		return true;
	}
	
	bool Update_dP_dT_V_j() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, dP_dT_V_j);

		auto&& Vj = this->Vj.GetRef();
		auto&& aj_prime = this->phasesEquilibrium.GetRef().aj_prime.GetRef();
		auto&& bj = this->phasesEquilibrium.value.bj.value;

		auto&& dP_dT_V_j = this->dP_dT_V_j.GetRefForUpdate();

		auto T = this->T.value, P = this->P.value;

		for (size_t j = 0; j < Count(dP_dT_V_j); ++j) {

			auto b = bj[j];
			auto V = Vj[j];

			dP_dT_V_j[j] = R / (V - b) - aj_prime[j] / (V*(V + b) + b*(V - b));
		}

		return true;
	}

	bool Update_dP_dV_T_j() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, dP_dV_T_j);

		auto&& Vj = this->Vj.GetRef();
		auto&& aj = this->phasesEquilibrium.value.aj.value;
		auto&& bj = this->phasesEquilibrium.value.bj.value;

		auto&& dP_dV_T_j = this->dP_dV_T_j.GetRefForUpdate();

		auto T = this->T.value;
		auto P = this->P.value;

		for (size_t j = 0; j < Count(dP_dV_T_j); ++j) {

			auto b = bj[j];
			auto V = Vj[j];

			dP_dV_T_j[j] = -R * T / (Sqr(V - b)) + 2*aj[j]*(V + b)
				/ ((V * (V + b) + b*(V - b))*(V * (V + b) + b*(V - b)));
		}

		return true;
	}
	
	bool Update_cp_j() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, cp_j);

		auto&& dV_dT_P_j = this->dV_dT_P_j.GetRef();
		auto&& dP_dT_V_j = this->dP_dT_V_j.GetRef();
		auto&& mW_j      = this->mW_j.GetRef();
		auto&& cv_j		 = this->cv_j.GetRef();
		
		auto T = this->T.value;
		auto P = this->P.value;

		auto&& cp_j = this->cp_j.GetRefForUpdate();

		for (size_t j = 0; j < Count(cp_j); ++j) {

			cp_j[j] = cv_j[j] + T *dV_dT_P_j[j] * dP_dT_V_j[j] / mW_j[j];
		}

		return true;
	}

	bool Update_mu_j() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, mu_j);

		auto&& compo = this->GetComponentsRef();
		auto&& xij   = this->phasesEquilibrium.GetRef().xij.GetRef();

		auto T = this->T.value;
		auto P = this->P.value;

		double muL = 0;
		double muV = 0;
		double m = 0;

		auto&& xL = xij[0];
		auto&& xV = xij[1];

		auto&& mu_j = this->mu_j.GetRefForUpdate();

		aVect_static_for(compo, i) {
			//if (T > compo[i].Tc) {
			//	muL += log(compo[i].Mu_V(T)) * xL[i];
			//} else {
			muL += compo[i].Mu_L(T) * xL[i];
			//}
			muV += compo[i].Mu_V(T) * xV[i] * compo[i].mW;
			m += xV[i] * compo[i].mW;
		}

		mu_j[0] = exp(muL);
		mu_j[1] = muV / m;

		return true;
	}
	
	bool Update_lambda_j() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, lambda_j);

		auto&& compo = this->GetComponentsRef();
		auto&& xij   = this->phasesEquilibrium.GetRef().xij.GetRef();

		auto T = this->T.value;
		auto P = this->P.value;

		auto&& xL = xij[0];
		auto&& xV = xij[1];

		auto&& lambda_j = this->lambda_j.GetRefForUpdate();

		double sumL = 0;
		double sumV = 0;

		aVect_static_for(compo, i1) {
			auto lambda_i1 = compo[i1].Lambda_L(T);
			aVect_static_for(compo, i2) {
				auto lambda_i2 = compo[i2].Lambda_L(T);
				sumL += 2/(1/lambda_i1 + 1/lambda_i2) * xL[i1] * xL[i2];
			}
			sumV += xV[i1] * compo[i1].Lambda_V(T);
		}

		lambda_j[0] = sumL;
		lambda_j[1] = sumV;

		return true;
	}

	bool Update_JT_j() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, JT_j);
		
		auto&& cp_j      = this->cp_j.GetRef();
		auto&& dV_dT_P_j = this->dV_dT_P_j.value;
		auto&& Vj        = this->Vj.value;
		auto&& mW_j      = this->mW_j.value;
		
		auto&& JT_j      = this->JT_j.GetRefForUpdate();

		auto T = this->T.value;

		for (size_t j = 0; j < Count(JT_j); ++j) {
			JT_j[j] = (T * dV_dT_P_j[j] - Vj[j]) / (cp_j[j] * mW_j[j]);
		}

		return true;
	}

	bool Update_soundVel_j() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, soundVel_j);

		auto&& cp_j = this->cp_j.GetRef();
		auto&& cv_j = this->cv_j.value;
		auto&& dP_dV_T_j = this->dP_dV_T_j.GetRef();
		auto&& Vj = this->Vj.value;
		auto&& mW_j = this->mW_j.GetRef();

		auto&& soundVel_j = this->soundVel_j.GetRefForUpdate();

		auto T = this->T.value;

		for (size_t j = 0; j < Count(soundVel_j); ++j) {
			soundVel_j[j] = Vj[j] * sqrt(-cp_j[j] / cv_j[j] * dP_dV_T_j[j] / mW_j[j]);
		}

		return true;
	}

	bool Update_cv_j() {
		COMPILE_TIME_OFFSET_CHECK(FluidComposition, cv_j);

		auto&& pe = this->phasesEquilibrium.GetRef();
		auto&& aj_second = pe.aj_second.GetRef();
		auto&& bj		 = pe.bj.value;
		auto&& Bj		 = pe.Bj.value;
		auto&& Zj		 = pe.Zj.value;
		auto&& xij		 = pe.xij.value;

		auto&& cv_j    = this->cv_j.GetRefForUpdate();
		auto&& cp_id_i = this->cp_id_i.GetRef();
		auto&& mW_j    = this->mW_j.GetRef();

		auto T = this->T.value;
		auto P = this->P.value;

		for (size_t j = 0; j < Count(cv_j); ++j) {

			auto&& xi = xij[j];

			double sum = 0;
			
			aVect_static_for(cp_id_i, i) sum += (cp_id_i[i] - R)*xi[i];

			auto b = bj[j], Z = Zj[j], B = Bj[j];
			const double sqrt_8 = 2.82842712474619, sqrt_2 = 1.414213562373;

			sum += T * aj_second[j] / (b*sqrt_8) * log((Z + (1 + sqrt_2)*B) / (Z + (1 - sqrt_2)*B));

			cv_j[j] = sum / mW_j[j];
		}

		return true;
	}

	template <bool normalize = true, class T>
	void Set_zi(const T& new_zi) {

		double sum;
		if (normalize) {
			sum = 0;
			for (auto&& z : new_zi) sum += z;

			if (IsEqualWithPrec(sum, 1, 1e-9)) return this->Set_zi<false>(new_zi);
		}

		auto zi_initialized = this->zi.IsUpdated();
		auto&& orig_zi = this->orig_zi.value;

		if (zi_initialized) {

			auto&& zi = this->zi.value;
		
			auto I = Count(orig_zi);
			auto new_I = Count(new_zi);

			if (I && I != new_I) __debugbreak();

			[&]() {

				for (size_t i = 0; i < I; ++i) {
					if ((orig_zi[i] == 0) != (new_zi[i] == 0)) {
						this->orig_zi.Invalidate();
						return;
					}
				}

				size_t new_i = 0;
				for (size_t i = 0; i < I; ++i) {
					if (new_zi[i]) {
						auto new_z = new_zi[i];
						if (normalize) new_z /= sum;
						if (!IsEqualWithPrec(zi[new_i], new_z, 1e-9)) {
							for (;;) {
								if (new_z) zi[new_i++] = new_z;
								if (++i >= I) break;
								new_z = new_zi[i];
								if (normalize) new_z /= sum;
							}
							this->zi.Invalidate();
							this->AssumeNoZeroZiCore(true);
							this->zi.SetUpdated();
							break;
						}

						new_i++;
					}
				}
			}();
		}

		orig_zi = new_zi;

		if (normalize) {
			for (auto&& z : orig_zi) z /= sum;
		}

		this->orig_zi.SetUpdated();
	}

	void DoNothing() {} 

#ifdef _WIN64
	TestField<1,  8,   aVect<Component>, FluidComposition, &Update_components, &Invalidate_components>	components;
	TestField<2,  32,  aMat<double>,     FluidComposition, &Update_bip,		   &Invalidate_components>	bip;
	TestField<3,  80,  double,           FluidComposition, &Update_T,		   &Invalidate_T>			T;
	TestField<4,  88,  double,		     FluidComposition, &Update_P,		   &Invalidate_P>			P;
	TestField<5,  96,  aVect<double>,    FluidComposition, &Update_zi,		   &Invalidate_zi>			zi;
	TestField<6,  120,  aVect<double>,    FluidComposition, &Update_ai,		   &DoNothing>				ai;
	TestField<7,  144,  aVect<double>,    FluidComposition, &Update_ai_prime,   &DoNothing>				ai_prime;
	TestField<8,  168, aVect<double>,    FluidComposition, &Update_ai_second,  &DoNothing>				ai_second;
	TestField<9,  192, aVect<double>,    FluidComposition, &Update_bi,		   &DoNothing>				bi;
	TestField<10, 216, aVect<double>,    FluidComposition, &Update_ci,		   &DoNothing>				ci;
	TestField<11, 240, aVect<double>,    FluidComposition, &Update_Ki_init,    &DoNothing>				Ki_init;
	TestField<12, 264, aVect<double>,    FluidComposition, &Update_aci,	       &DoNothing>				aci;
	TestField<13, 288, aVect<double>,    FluidComposition, &Update_kappa_i,	   &DoNothing>				kappa_i;

	TestField<14, 312, PhasesEquilibrium, FluidComposition, &Update_phasesEquilibrium, &Invalidate_phasesEquilibrium>	 phasesEquilibrium;

	TestField<15, 560, double[2],	  FluidComposition, &Update_Mw_j,		   &DoNothing>	mW_j;
	TestField<16, 576, double[2],	  FluidComposition, &Update_Vj,			   &DoNothing>	Vj;
	TestField<17, 592, double[2],	  FluidComposition, &Update_V_shift_j,     &Invalidate_Vj>	V_shift_j;
	TestField<18, 608, double[2],	  FluidComposition, &Update_rho_j,		   &DoNothing>	rho_j;
	TestField<19, 624, double,		  FluidComposition, &Update_FracVolVap,    &DoNothing>	FracVolVap;
	TestField<20, 632, double,		  FluidComposition, &Update_FracMassVap,   &DoNothing>	FracMassVap;
	TestField<21, 640, aVect<double>, FluidComposition, &Update_Hid_i,		   &DoNothing>	Hid_i;
	TestField<22, 664, double[2],	  FluidComposition, &Update_H_j,		   &DoNothing>	H_j;

	TestField<23, 680, double, FluidComposition,    &Update_T_ref, &Invalidate_referenceState>  T_ref;
	TestField<24, 688, double, FluidComposition,    &Update_P_ref, &Invalidate_referenceState>  P_ref;
	TestField<25, 696, double[2], FluidComposition, &Update_H0_j,  &DoNothing>					H0_j;
	
	TestField<26, 712, BipData,			 FluidComposition, &Update_bipData,			&Invalidate_bipData>			bipData;
	TestField<27, 808, aMat<double>,	 FluidComposition, &Update_orig_bip,		&Invalidate_orig_bip>			orig_bip;
	TestField<28, 856, aVect<Component>, FluidComposition, &Update_orig_components, &Invalidate_orig_components>	orig_components;
	TestField<29, 880, aVect<double>,	 FluidComposition, &Update_orig_zi,			&Invalidate_orig_zi>			orig_zi;

	TestField<30, 904, double[2],		 FluidComposition, &Update_sigma_j,		&DoNothing>	sigma_j;
	TestField<31, 920, double[2],		 FluidComposition, &Update_dZ_dT_P_j,	&DoNothing>	dZ_dT_P_j;
	TestField<32, 936, double[2],		 FluidComposition, &Update_dV_dT_P_j,	&DoNothing>	dV_dT_P_j;
	TestField<33, 952, double[2],		 FluidComposition, &Update_dP_dT_V_j,	&DoNothing>	dP_dT_V_j;
	TestField<34, 968, double[2],		 FluidComposition, &Update_cv_j,		&DoNothing>	cv_j;
	TestField<35, 984, double[2],		 FluidComposition, &Update_cp_j,		&DoNothing>	cp_j;
	TestField<36, 1000, aVect<double>,   FluidComposition, &Update_cp_id_i,		&DoNothing>	cp_id_i;
	TestField<37, 1024, double[2],       FluidComposition, &Update_mu_j,		&DoNothing>	mu_j;
	TestField<38, 1040, double[2],       FluidComposition, &Update_lambda_j,    &DoNothing>	lambda_j;
	TestField<39, 1056, double[2],		 FluidComposition, &Update_JT_j,		&DoNothing>	JT_j;
	TestField<40, 1072, double[2],		 FluidComposition, &Update_soundVel_j,  &DoNothing>	soundVel_j;
	TestField<41, 1088, double[2],		 FluidComposition, &Update_dP_dV_T_j,   &DoNothing>	dP_dV_T_j;
#else
	TestField<1,  8,   aVect<Component>, FluidComposition, &Update_components, &Invalidate_components>	components;
	TestField<2,  20,  aMat<double>,     FluidComposition, &Update_bip,		   &Invalidate_components>	bip;
	TestField<3,  48,  double,           FluidComposition, &Update_T,		   &Invalidate_T>			T;
	TestField<4,  56,  double,		     FluidComposition, &Update_P,		   &Invalidate_P>			P;
	TestField<5,  64,  aVect<double>,    FluidComposition, &Update_zi,		   &Invalidate_zi>			zi;
	TestField<6,  76,  aVect<double>,    FluidComposition, &Update_ai,		   &DoNothing>				ai;
	TestField<7,  88,  aVect<double>,    FluidComposition, &Update_ai_prime,   &DoNothing>				ai_prime;
	TestField<8,  100, aVect<double>,    FluidComposition, &Update_ai_second,  &DoNothing>				ai_second;
	TestField<9,  112, aVect<double>,    FluidComposition, &Update_bi,		   &DoNothing>				bi;
	TestField<10, 124, aVect<double>,    FluidComposition, &Update_ci,		   &DoNothing>				ci;
	TestField<11, 136, aVect<double>,    FluidComposition, &Update_Ki_init,    &DoNothing>				Ki_init;
	TestField<12, 148, aVect<double>,    FluidComposition, &Update_aci,	       &DoNothing>				aci;
	TestField<13, 160, aVect<double>,    FluidComposition, &Update_kappa_i,	   &DoNothing>				kappa_i;

	TestField<14, 176, PhasesEquilibrium, FluidComposition, &Update_phasesEquilibrium, &Invalidate_phasesEquilibrium>	 phasesEquilibrium;

	TestField<15, 368, double[2],	  FluidComposition, &Update_Mw_j,		   &DoNothing>	mW_j;
	TestField<16, 384, double[2],	  FluidComposition, &Update_Vj,			   &Invalidate_Vj>	Vj;
	TestField<17, 400, double[2],	  FluidComposition, &Update_V_shift_j,     &DoNothing>	V_shift_j;
	TestField<18, 416, double[2],	  FluidComposition, &Update_rho_j,		   &DoNothing>	rho_j;
	TestField<19, 432, double,		  FluidComposition, &Update_FracVolVap,    &DoNothing>	FracVolVap;
	TestField<20, 440, double,		  FluidComposition, &Update_FracMassVap,   &DoNothing>	FracMassVap;
	TestField<21, 448, aVect<double>, FluidComposition, &Update_Hid_i,		   &DoNothing>	Hid_i;
	TestField<22, 464, double[2],	  FluidComposition, &Update_H_j,		   &DoNothing>	H_j;

	TestField<23, 480, double, FluidComposition,    &Update_T_ref, &Invalidate_referenceState>  T_ref;
	TestField<24, 488, double, FluidComposition,    &Update_P_ref, &Invalidate_referenceState>  P_ref;
	TestField<25, 496, double[2], FluidComposition, &Update_H0_j,  &DoNothing>					H0_j;
	
	TestField<26, 512, BipData,			 FluidComposition, &Update_bipData,			&Invalidate_bipData>			bipData;
	TestField<27, 560, aMat<double>,	 FluidComposition, &Update_orig_bip,		&Invalidate_orig_bip>			orig_bip;
	TestField<28, 584, aVect<Component>, FluidComposition, &Update_orig_components, &Invalidate_orig_components>	orig_components;
	TestField<29, 596, aVect<double>,	 FluidComposition, &Update_orig_zi,			&Invalidate_orig_zi>			orig_zi;

	TestField<30, 608, double[2],		 FluidComposition, &Update_sigma_j,		&DoNothing>	sigma_j;
	TestField<31, 624, double[2],		 FluidComposition, &Update_dZ_dT_P_j,	&DoNothing>	dZ_dT_P_j;
	TestField<32, 640, double[2],		 FluidComposition, &Update_dV_dT_P_j,	&DoNothing>	dV_dT_P_j;
	TestField<33, 656, double[2],		 FluidComposition, &Update_dP_dT_V_j,	&DoNothing>	dP_dT_V_j;
	TestField<34, 672, double[2],		 FluidComposition, &Update_cv_j,		&DoNothing>	cv_j;
	TestField<35, 688, double[2],		 FluidComposition, &Update_cp_j,		&DoNothing>	cp_j;
	TestField<36, 704, aVect<double>,    FluidComposition, &Update_cp_id_i,		&DoNothing>	cp_id_i;
	TestField<37, 720, double[2],        FluidComposition, &Update_mu_j,		&DoNothing>	mu_j;
	TestField<38, 736, double[2],        FluidComposition, &Update_lambda_j,    &DoNothing>	lambda_j;
	TestField<39, 752, double[2],		 FluidComposition, &Update_JT_j,		&DoNothing>	JT_j;
	TestField<40, 768, double[2],		 FluidComposition, &Update_soundVel_j,  &DoNothing>	soundVel_j;
	TestField<41, 784, double[2],		 FluidComposition, &Update_dP_dV_T_j,   &DoNothing>	dP_dV_T_j;
#endif

	enum BinaryOptions { 
		assumeNoZeroZi							  = BIT(1), 
		rachfordRice_useNewtonSolver			  = BIT(2), 
		useVolumeCorrection_Peneloux			  = BIT(3),
		useVolumeCorrection_COSTALD				  = BIT(4),
		COSTALD_PressureCorrection_Prausnitz	  = BIT(5),
		phasesEquilibrium_initFromPreviousState	  = BIT(6),
		peneloux_Eq1							  = BIT(7),
		viscosity_useHYSYSCorrelations			  = BIT(8),	// TODO
		thermalConductivity_useHYSYSCorrelations  = BIT(9)	// TODO
	};

	//int binaryOptions = 0;// BinaryOptions::useVolumeCorrection_Peneloux;
	BitField<BinaryOptions> binaryOptions = 0;

	double phasesEquilibriumSolverTolerance = 1e-9;
	ManagedPointer<Dicotomy1D_Solver> dicotomy_solver;
	ManagedPointer<SecantMethod_Solver2> newton_solver;
	ManagedPointer<MonotoneHybrid_Solver> monotoneHybrid_solver;
	ManagedPointer<MonotoneHybrid_Solver> monotoneHybrid_solver2;

	MagicPointer<GlobalComponents> pGlobalComponents;
	MagicPointer<GlobalBip> pGlobalBip;

#ifdef _DEBUG
	WinCriticalSection dbgCritSect;
#endif

	void SetBinaryOption(BinaryOptions opt, bool enable) {
		if (enable) this->binaryOptions |= opt;
		else		this->binaryOptions &= ~opt;
	}

	void AssumeNoZeroZiCore(bool enable) {
		this->SetBinaryOption(BinaryOptions::assumeNoZeroZi, enable);
	}

public:

	FluidComposition() {
		this->dicotomy_solver->xMin = 0.0000001;
		this->dicotomy_solver->xMax = 0.9999999;
		this->dicotomy_solver->maxIter = 200;
		this->dicotomy_solver->xInit = 0.5;
		this->dicotomy_solver->tolCheckX = true;
		this->dicotomy_solver->tol = 1e-6;
		this->dicotomy_solver->tol_x = 1e-6;
		//this->dicotomy_solver->recurse = 0;

		this->newton_solver->xMin = 0.0000001;
		this->newton_solver->xMax = 0.9999999;
		this->newton_solver->maxIter = 200;
		this->newton_solver->xInit = 0.5;
		this->newton_solver->tol = 1e-6;
	}
	
	void ReadMathildaComponentsProperties(const wchar_t * fluidsPath) {

		auto& components =
			this->pGlobalComponents ?
			this->pGlobalComponents->component :
			this->orig_components.GetRef();

		for (auto&& c : components) {
			c.ReadMathildaComponentFile(fluidsPath);
		}
	}

	void ReadMathildaComponentsProperties(const char * fluidsPath) {
		ReadMathildaComponentsProperties(aVect<wchar_t>(fluidsPath));
	}

	void ReadMathildaBipFile(const wchar_t * bipFile) {

		auto&& bipData = this->bipData.GetRefForUpdate();

		WinFile file(bipFile);

		auto line = file.GetLine();

		bipData.names_j.Redim(0);
		while (auto tok = RetrieveStr(line)) bipData.names_j.Push(tok.Trim());

		auto I = (int)bipData.names_j.Count();

		bipData.bip.Redim(I, I);
		bipData.names_i.Redim(I);

		for (int i = 0; i < I; i++) {
			line = file.GetLine();
			bipData.names_i[i] = RetrieveStr(line);
			
			for (int j = 0; j < I; j++) {
				auto tok = RetrieveStr(line);
				bipData.bip(i, j) = atof(tok.Trim());
			}
		}
	}
	void ReadMathildaBipFile(const char * bipFile) {
		this->ReadMathildaBipFile(aVect<wchar_t>(bipFile));
	}

	void Option_AssumeNoZeroZi(bool enable) {
		this->AssumeNoZeroZiCore(enable);
		this->phasesEquilibrium.Invalidate();
	}

	void Option_RachfordRice_UseNewtonSolver(bool enable) {
		this->SetBinaryOption(BinaryOptions::rachfordRice_useNewtonSolver, enable);
	}

	void Option_UseVolumeCorrection_Peneloux(bool enable) {
		this->SetBinaryOption(BinaryOptions::useVolumeCorrection_Peneloux, enable);
		this->Vj.Invalidate();
	}

	void Option_UseVolumeCorrection_COSTALD(bool enable) {
		this->SetBinaryOption(BinaryOptions::useVolumeCorrection_COSTALD, enable);
		this->Vj.Invalidate();
	}

	void Option_COSTALD_PressureCorrection_Prausnitz(bool enable) {
		this->SetBinaryOption(BinaryOptions::COSTALD_PressureCorrection_Prausnitz, enable);
		this->Vj.Invalidate();
	}

	void Option_PhasesEquilibrium_InitFromPreviousState(bool enable) {
		this->SetBinaryOption(BinaryOptions::phasesEquilibrium_initFromPreviousState, enable);
	}

	double VaporMolarFraction() {
		return this->phasesEquilibrium.GetRef().psi.GetRef();
	}

	double VaporVolumeFraction() {
		return this->FracVolVap.GetRef();
	}

	double VaporMassFraction() {
		return this->FracMassVap.GetRef();
	}

	double SuperficialTension() {
		return this->sigma_j.GetRef()[0];
	}

	double MolarMass(size_t iPhase) {
		return this->mW_j.GetRef()[iPhase];
	}

	double Fugacity(size_t iPhase) {

		double retVal = 0;

		auto&& pe = this->phasesEquilibrium.GetRef();

		auto&& phi_i = pe.phi_ij.GetRef()[iPhase];
		auto&& xi = pe.xij.GetRef()[iPhase];

		aVect_static_for(phi_i, i) retVal += xi[i] * phi_i[i];

		return retVal;
	}

	double CompressibilityFactor(size_t iPhase) {
		return this->phasesEquilibrium.GetRef().Zj.GetRef()[iPhase];
	}

	double MolarVolume(size_t iPhase) {
		return this->Vj.GetRef()[iPhase];
	}

	double VolumeShift(size_t iPhase) {
		return this->V_shift_j.GetRef()[iPhase];
	}

	double Density(size_t iPhase) {
		return this->rho_j.GetRef()[iPhase];
	}

	double IsobaricSpecificHeat(size_t iPhase) {
		return this->cp_j.GetRef()[iPhase];
	}

	double IsochoricSpecificHeat(size_t iPhase) {
		return this->cv_j.GetRef()[iPhase];
	}

	double Viscosity(size_t iPhase) {
		return this->mu_j.GetRef()[iPhase];
	}

	double ThermalConductivity(size_t iPhase) {
		return this->lambda_j.GetRef()[iPhase];
	}

	double JoulesThomson(size_t iPhase) {
		return this->JT_j.GetRef()[iPhase];
	}
	 
	double SoundVelocity(size_t iPhase) {
		return this->soundVel_j.GetRef()[iPhase];
	}

	double Enthalpy(size_t iPhase) {
		return this->H_j.GetRef()[iPhase];
	}

	double Enthalpy_MassWeightedAvg() {

		DEBUG_THERMO_THREADSAFE

		auto&& pe = this->phasesEquilibrium.GetRef();
		if (pe.psi.value == 0) return this->H_j.GetRef()[0];
		if (pe.psi.value == 1) return this->H_j.GetRef()[1];

		auto&& vmf = this->FracMassVap.GetRef();
		return this->H_j.GetRef()[0] * (1 - vmf) + this->H_j.GetRef()[1] * vmf;
	}

	double Density_VolumeWeightedAvg() {

		DEBUG_THERMO_THREADSAFE

		auto&& pe = this->phasesEquilibrium.GetRef();
		if (pe.psi.value == 0) return this->rho_j.GetRef()[0];
		if (pe.psi.value == 1) return this->rho_j.GetRef()[1];

		auto&& vvf = this->FracVolVap.GetRef();
		return this->rho_j.GetRef()[0] * (1 - vvf) + this->rho_j.GetRef()[1] * vvf;
	}

	double Viscosity_MassWeightedAvg() {

		DEBUG_THERMO_THREADSAFE

		auto&& pe = this->phasesEquilibrium.GetRef();
		if (pe.psi.value == 0) return this->mu_j.GetRef()[0];
		if (pe.psi.value == 1) return this->mu_j.GetRef()[1];
		
		auto&& vmf = this->FracMassVap.GetRef();
		return this->mu_j.GetRef()[0] * (1 - vmf) + this->mu_j.GetRef()[1] * vmf;
	}

	double ThermalConductivity_MassWeightedAvg() {

		DEBUG_THERMO_THREADSAFE

		auto&& pe = this->phasesEquilibrium.GetRef();
		if (pe.psi.value == 0) return this->lambda_j.GetRef()[0];
		if (pe.psi.value == 1) return this->lambda_j.GetRef()[1];

		auto&& vmf = this->FracMassVap.GetRef();
		return this->lambda_j.GetRef()[0] * (1 - vmf) + this->lambda_j.GetRef()[1] * vmf;
	}

	double Beta_j(size_t j) {

		DEBUG_THERMO_THREADSAFE

		return this->dV_dT_P_j.GetRef()[j] / this->Vj.GetRef()[j];
	}

	double ThermalExpansionCoefficient_VolumeWeightedAvg() {

		DEBUG_THERMO_THREADSAFE

		auto&& pe = this->phasesEquilibrium.GetRef();
		if (pe.psi.value == 0) return this->Beta_j(0);
		if (pe.psi.value == 1) return this->Beta_j(1);

		auto&& vvf = this->FracVolVap.GetRef();
		return this->Beta_j(0) * (1 - vvf) + this->Beta_j(1) * vvf;
	}

	double SpecificHeatCapacity_MassWeightedAvg() {

		DEBUG_THERMO_THREADSAFE

		auto&& pe = this->phasesEquilibrium.GetRef();
		if (pe.psi.value == 0) return this->cp_j.GetRef()[0];
		if (pe.psi.value == 1) return this->cp_j.GetRef()[1];

		auto&& vmf = this->FracMassVap.GetRef();
		return this->cp_j.GetRef()[0] * (1 - vmf) + this->cp_j.GetRef()[1] * vmf;
	}

	void HP_Flash(double enthalpy, double pressure, double tol = 1e-6) {

		auto&& solver = *this->monotoneHybrid_solver;

		solver.xInit = 273.15;
		this->phasesEquilibriumSolverTolerance = Max(tol * 1e-4, 1e-14);

		solver.tol = tol;
		solver.xMin = 1;
		solver.xMax = 500 + 273.15;

		solver.y_target = enthalpy;

		//AxisClass axis;

		//solver.pPlot = &axis;

		solver.Solve<decltype(HP_Flash_Func), HP_Flash_Func, false, true>(*this, pressure);

		if (!solver.isConverged) {
			this->phasesEquilibriumSolverTolerance = sqrt(this->phasesEquilibriumSolverTolerance);
			solver.Solve<decltype(HP_Flash_Func), HP_Flash_Func, false, true>(*this, pressure);
		}

		if (!solver.isConverged && IsDebuggerPresent()) {
			static bool once;

			if (!once) {
				once = true;

				if (IsDebuggerPresent()) __debugbreak();
				AxisClass axis;
				solver.pPlot = &axis;

				solver.Solve<decltype(HP_Flash_Func), HP_Flash_Func, true, true>(*this, pressure);
			}

			throw std::runtime_error("HP Flash: no convergence");
		}
	}

	void VP_Flash(double vaporMolarFraction, double pressure, double tol = 1e-6) {

		auto&& solver = *this->monotoneHybrid_solver;

		solver.xInit = 273.15;
		this->phasesEquilibriumSolverTolerance = Max(tol * 1e-4, 1e-14);

		solver.tol = tol;
		solver.xMin = 1;
		solver.xMax = 500 + 273.15;

		solver.y_target = vaporMolarFraction;

		//AxisClass axis;

		//solver.pPlot = &axis;

		solver.Solve<decltype(VP_Flash_Func), VP_Flash_Func, false>(*this, pressure);

		if (!solver.isConverged) throw std::runtime_error("HP Flash: no convergence");
	}

	void VVFT_Flash(double vaporVolumeFraction, double temperature, double tol = 1e-6) {

		auto&& solver = *this->dicotomy_solver;

		solver.xInit = 2e5; 
		this->phasesEquilibriumSolverTolerance = Max(tol * 1e-4, 1e-14);

		solver.tol = tol;
		solver.xMin = 0.05e5;
		solver.xMax = 1000e5;

		solver.y_target = vaporVolumeFraction;

		//AxisClass axis;
		//solver.pPlot = &axis;

		solver.Solve<decltype(VVFT_Flash_Func), VVFT_Flash_Func, false>(*this, temperature);

		if (!solver.isConverged) throw std::runtime_error("VVFT Flash: no convergence");
	}

	void RhoH_Flash(double density, double enthalpy, double tol = 1e-6);
	void RhoH_Flash_old(double density, double enthalpy, double tol = 1e-6, bool skipNewton = false);

	double Temperature_From_HP_Flash(double enthalpy, double pressure, double tol = 1e-6) {
		this->HP_Flash(enthalpy, pressure, tol);
		return this->T.GetRef();
	}

	double Pressure_From_VVFT_Flash(double vaporVolumeFraction, double temperature, double tol = 1e-6) {
		this->VVFT_Flash(vaporVolumeFraction, temperature, tol);
		return this->P.GetRef();
	}

	double Temperature_From_VP_Flash(double vaporMolarFraction, double pressure, double tol = 1e-6) {
		this->VP_Flash(vaporMolarFraction, pressure, tol);
		return this->T.GetRef();
	}

	double Temperature_From_RhoH_Flash(double density, double enthalpy, double tol = 1e-6) {
		//this->RhoH_Flash_old(density, enthalpy, tol);
		this->RhoH_Flash(density, enthalpy, tol);
		return this->T.GetRef();
	}

	double Pressure_From_RhoH_Flash(double density, double enthalpy, double tol = 1e-6) {
		//this->RhoH_Flash_old(density, enthalpy, tol);
		this->RhoH_Flash(density, enthalpy, tol);
		return this->P.GetRef();
	}

	//don't work
	//bool IsSuperCritical() {

	//	auto&& pe = this->phasesEquilibrium.GetRef();
	//	auto&& psi = pe.psi.GetRef();

	//	if (psi == 0 || psi == 1) {
	//		auto&& Zj = pe.Zj.GetRef();
	//		if (IsEqualWithPrec(Zj[0], Zj[1], 1e-6)) {
	//			auto&& Aj = pe.Aj.GetRef();
	//			auto&& Bj = pe.Bj.GetRef();

	//			auto c = GetCoef(Aj[0], Bj[0]);

	//			auto delta = int_pow<2>(2 * c.a2) - 4 * 3 * c.a1;

	//			if (delta < 0) return true;
	//		}
	//	}
	//	return false;
	//}
};

#endif
