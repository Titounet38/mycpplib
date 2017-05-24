#include <limits>

#include "MyUtils.h"
#include "xMat.h"
#include "MagicRef.h"
#include "Converter.h"
#include "Axis.h"

#ifdef SIMULATE_NO_WIN32
#define _WIN32_BKP   _WIN32
#define _MSC_VER_BKP _MSC_VER
#undef _WIN32
#undef _MSC_VER
#endif

#ifndef _OLGAUTILS_
#define _OLGAUTILS_

#define OLGA_WP_RAS 0
#define OLGA_WP_CORRUPT 1
#define OLGA_WP_FINISHED 2

struct Branch {
	aVect<char> name;
	mVect<double> rho, v, x, z, twall, P, vLiq, HOL, VO, GT;
};


struct Olga_paramInfo {
    const char * name;
    bool isString;
    union {
        const char *** str;
        double * value;
    };
};

struct Olga_params {
    const char ** rho_gaz_str;
    const char ** v_gaz_str;
    const char ** Twall_str;
    const char ** P_str;
    const char ** vLiq_str;
    const char ** HOL_str;
	const char ** GT_str;
	const char ** VO_str;
    double rho_v2_max;
    double rho_v2_threshold;
	double vPig_max;
    double vPig_threshold;
    double speed_of_sound;
    double tWall_min;
	double tWall_threshold;
    double vLiq_max;
    double tCheck_vLiq;
    double HOL_Check_vLiq;
	double HOL_Check_rho_v2;
};

#define OLGA_PARAM_STRING(name) {"[" #name "]", true,  &g_params.name}
#define OLGA_PARAM_DOUBLE(name) {"[" #name "]", false, (const char***)&g_params.name}

aVect<char> GetBranchName(char * varName);

void Olga_SetLogFile(LogFile & p);

size_t Olga_AppendDataToArray(File & src, mVect<double> * t, size_t n, 
	size_t * nLines = nullptr, FILE * dst = nullptr);

size_t Olga_AppendDataToArray(
	File & src, aVect< aVect<double> > * t, size_t n,
	aVect<size_t> & indArray, FILE * dst = nullptr, double timeStop = -1,
	aVect<int> * wantedInd = nullptr);

void Olga_GetVarNames(aVect<char>& path);

aVect< aVect<double> > Olga_GetTrends(
	char * sourceFile, 
	aVect< aVect<char> > & varNames, 
	double stopTime = -1, 
	double * startTime = nullptr, 
	FILE * dstFile = nullptr, 
	bool writeHeader = false,
	aVect<aVect<char>> * types = nullptr,
	aVect<aVect<char>> * branchNames = nullptr,
	aVect<aVect<char>> * pipeNames = nullptr,
	aVect<aVect<char>> * sectionIndices = nullptr,
	aVect<aVect<char>> * units = nullptr,
	aVect<aVect<char>> * descriptions = nullptr);

void Olga_MergeProfileFiles2(
	aVect<char> & mergeFileName, 
	aVect< aVect<char> > files2Merge,
	double timeStop = -1);


struct Olga_OutputVariable {
	aVect<char> name;
	aVect<char> type;
	aVect<char> branchName;
	aVect<char> pipeName;
	aVect<char> sectionIndex;
	aVect<char> unit;
	aVect<char> description;
	bool isBoundaryValues;
	bool isGeometry;

	Olga_OutputVariable(
		const char * name, 
		const char * type = nullptr,
		const char * branchName = nullptr,
		const char * pipeName = nullptr,
		const char * sectionIndex = nullptr,
		const char * unit = nullptr,
		const char * description = nullptr,
		bool isBoundaryValues = false,
		bool isGeometry = false)
	: 
		name(name), 
		type(type),
		branchName(branchName),
		pipeName(pipeName),
		sectionIndex(sectionIndex),
		unit(unit),
		description(description),
		isBoundaryValues(isBoundaryValues),
		isGeometry(isGeometry)
	{}
};

int Olga_WalkProfiles(
	char * sourceFile,
	//mVect< mVect<char> > * pVarNames,
	aVect<Olga_OutputVariable> *  pVariables,
	bool (*func)(
		double time,
		aVect<Olga_OutputVariable> * pVariables,
		mVect< mVect<double> > & arrays,
		bool reInit, 
		bool noMoreTimeSteps, 
		bool beforeRestart,
		mVect< mVect<char> > * pTplVarNames,
		mVect<double> * pTplArray,
		void * userParam),
	char * mergeFile = nullptr, 
	double * startTime = nullptr,
	double * endTime = nullptr,
	size_t * nLinesToStartTime = nullptr, 
	char * outputFile = nullptr, 
	mVect< mVect<char> > * pTplVarNames = nullptr,
	char * tplFile = nullptr,
	void * userParam = nullptr,
	bool displayProgressBar = false
);

void Olga_MergeProfileFiles(aVect<char> & filePath, aVect<char> & mergeFilePath, aVect<char> & outputFilePath = aVect<char>());
void Olga_MergeTrendFiles(aVect<char> & filePath, aVect<char> & mergeFilePath, aVect<char> & outputFilePath = aVect<char>());
Olga_params & Olga_Params();
//void DbgCheckBranch(aVect<Branch> & branches);

const char ** Olga_x_str(char * new_x_str = nullptr);
const char ** Olga_z_str(char * new_x_str = nullptr);

struct Olga_PVT_Variable : MagicTarget<Olga_PVT_Variable> {

	aVect<char> name;
	aVect<char> unit;
	aMat<double> value;

	MagicCopyDecrementer copyDecrementer;

	double & operator()(size_t i, size_t j) {
		return this->value(i, j);
	}

	Olga_PVT_Variable & Set(double value) {
		this->value.Set(value);
		return *this;
	}
};

struct Olga_PVT_Table;

Olga_PVT_Table Olga_Read_PVT_Table(const wchar_t * fileName);
Olga_PVT_Table Olga_Read_PVT_Table(const char * fileName);

void Olga_Replace_PVT_Points(const wchar_t * destFilePath, const Olga_PVT_Table & table);

struct PVT_Variable_Cache;

template<
	class ExceptionClass = std::exception,
		long errCode_P_BELOW_RANGE = 0,
		long errCode_P_ABOVE_RANGE = 0,
		long errCode_T_BELOW_RANGE = 0,
		long errCode_T_ABOVE_RANGE = 0>
struct PVT_Variable {

	MagicCopyIncrementer copyIncrementer;

	MagicPointer<Olga_PVT_Table> table;
	MagicPointer<Olga_PVT_Variable> var;
	PVT_Variable_Cache cache;

	MagicCopyDecrementer copyDecrementer;

	double operator()(double P, double T) {

		bool P_interval_changed;
		bool T_interval_changed;

		if (P == this->cache.last_P && T == this->cache.last_T) {
			return this->cache.last_value;
		}

		if (this->cache.P1 <= P && this->cache.P2 >= P) {
			P_interval_changed = false;
		}
		else {
			P_interval_changed = true;
			size_t iP = this->cache.iP;
			const size_t n = this->table->P.Count() - 2;
			
			auto&& P_array = this->table->P;

			while (true) {
				if (P_array[iP] <= P) break;
				else if (--iP > n) throw ExceptionClass(xFormat("P below lowest table value (%g < %g)", P, P_array[0]), errCode_P_BELOW_RANGE);
			}

			while (true) {
				if (P_array[iP + 1] >= P) break;
				else if (++iP > n) throw ExceptionClass(xFormat("P above highest table value (%g > %g)", P, P_array.Last()), errCode_P_ABOVE_RANGE);
			}

			this->cache.iP = iP;
		}

		if (this->cache.T1 <= T && this->cache.T2 >= T) {
			T_interval_changed = false;
		}
		else {
			T_interval_changed = true;
			size_t iT = this->cache.iT;
			const size_t n = this->table->T.Count() - 2;

			auto&& T_array = this->table->T;

			while (true) {
				if (T_array[iT] <= T) break;
				else if (--iT > n) throw ExceptionClass(xFormat("T below lowest table value (%g < %g)", T, T_array[0]), errCode_T_BELOW_RANGE);
			}

			while (true) {
				if (T_array[iT + 1] >= T) break;
				else if (++iT > n) throw ExceptionClass(xFormat("T above highest table value (%g > %g)", T, T_array.Last()), errCode_T_ABOVE_RANGE);
			}

			this->cache.iT = iT;
		}

		if (P_interval_changed || T_interval_changed) {
			this->cache.Update(*this->table, this->var->value);
		}

		const double P2_m_P = this->cache.P2 - P;
		const double P_m_P1 = P - this->cache.P1;

		const double v1 = this->cache.v11 * P2_m_P + this->cache.v21 * P_m_P1;
		const double v2 = this->cache.v12 * P2_m_P + this->cache.v22 * P_m_P1;

		auto retVal = this->cache.precomputedFactor * (v1 * (this->cache.T2 - T) + v2*(T - this->cache.T1));

		this->cache.last_P = P;
		this->cache.last_T = T;
		this->cache.last_value = retVal;

		return retVal;
	}

	void Init(const Olga_PVT_Table & table, const Olga_PVT_Variable * var) {
		this->table = &table;
		this->var = var;
		this->cache.iP = 0;
		this->cache.iT = 0;
		this->cache.Update(table, var->value);
	}

	void Init(const Olga_PVT_Table & table, const char * varName) {
		this->Init(table, &table.FindVariableTable(varName));
	}

	void Set(double value) {
		this->var->Set(value);
	}
};

void Olga_Write_PVT_Table(const wchar_t * destFilePath, const Olga_PVT_Table & table);

struct Olga_PVT_Table : MagicTarget<Olga_PVT_Table> {

	aVect<char> name;
	aVect<char> fileName;
	aVect<double> T, P;
	aVect<char> T_unit;
	aVect<char> P_unit;

	bool old_format;

	double stdPressure     = std::numeric_limits<double>::quiet_NaN();
	double stdTemperature  = std::numeric_limits<double>::quiet_NaN();
	double GOR			   = std::numeric_limits<double>::quiet_NaN();
	double GLR			   = std::numeric_limits<double>::quiet_NaN();
	double WC			   = std::numeric_limits<double>::quiet_NaN();
	double stdOilDensity   = std::numeric_limits<double>::quiet_NaN();
	double stdGasDensity   = std::numeric_limits<double>::quiet_NaN();
	double stdWaterDensity = std::numeric_limits<double>::quiet_NaN();

	aMat<double> precomputedFactor;
	aVect<Olga_PVT_Variable> variables;

	MagicCopyDecrementer copyDecrementer;

	size_t FindVariableTableIndex(const char * varName) {

		size_t retVal;
		bool found = false;

		aVect_static_for(this->variables, i) {
			if (strcmp(this->variables[i].name, varName) == 0) {
				retVal = i;
				found = true;
				break;
			}
		}

		if (!found) {
			RECOVERABLE_ERROR(xFormat("variable \"%s\"not found", varName), ERROR_INVALID_DATA);
			return -1;
		}

		return retVal;
	}

	Olga_PVT_Variable * FindVariableTablePtr(const char * varName) const {

		size_t index;
		bool found = false;

		aVect_static_for(this->variables, i) {
			if (this->variables[i].name && strcmp(this->variables[i].name, varName) == 0) {
				index = i;
				found = true;
				break;
			}
		}

		if (!found) {
			return nullptr;
		}

		return &this->variables[index];
	}

	Olga_PVT_Variable & FindVariableTable(const char * varName) const {

		auto ptr = this->FindVariableTablePtr(varName);

		if (!ptr) {
			RECOVERABLE_ERROR(xFormat("variable \"%s\"not found", varName), ERROR_INVALID_DATA);
		}

		return *ptr;
	}

	Olga_PVT_Table & LoadTable(const char * filePath) {

		return *this = Olga_Read_PVT_Table(filePath);
	}

	Olga_PVT_Table & WriteToFile(const wchar_t * filePath) {
		Olga_Write_PVT_Table(filePath, *this);
		return *this;
	}

	Olga_PVT_Table & WriteToFile(const char * filePath) {
		Olga_Write_PVT_Table(aVect<wchar_t>(filePath), *this);
		return *this;
	}

	Olga_PVT_Table & UpdatePrecomputedFactor() {

		this->precomputedFactor.Redim(this->P.Count() - 1, this->T.Count() - 1);

		aMat_static_for(this->precomputedFactor, i, j) {
			this->precomputedFactor(i, j) = 1 / ((this->P[i + 1] - this->P[i]) * (this->T[j + 1] - this->T[j]));
		}

		return *this;
	}
	
	Olga_PVT_Table & ConvertTable(
		const char * P_convertTo,
		const char * T_convertTo)
	{

		if (P_convertTo && strcmp_caseInsensitive(this->P_unit, P_convertTo) != 0) {
			UnitConverter convertToBar(this->P_unit, P_convertTo);

			for (auto&& P : this->P) {
				P = convertToBar(P);
			}

			this->P_unit = P_convertTo;
		}

		if (T_convertTo && strcmp_caseInsensitive(this->T_unit, T_convertTo) != 0) {

			UnitConverter convertToBar(this->T_unit, T_convertTo);

			for (auto&& T : this->T) {
				T = convertToBar(T);
			}

			this->T_unit = T_convertTo;
		}

		this->UpdatePrecomputedFactor();

		return *this;
	}

	template<
		class ExceptionClass = std::exception,
		long errCode_P_BELOW_RANGE = 0,
		long errCode_P_ABOVE_RANGE = 0,
		long errCode_T_BELOW_RANGE = 0,
		long errCode_T_ABOVE_RANGE = 0>
	PVT_Variable<ExceptionClass, errCode_P_BELOW_RANGE, errCode_P_ABOVE_RANGE, errCode_T_BELOW_RANGE, errCode_T_ABOVE_RANGE> 
	GetVariable(const char * varName)
	{
		PVT_Variable<ExceptionClass, errCode_P_BELOW_RANGE, errCode_P_ABOVE_RANGE, errCode_T_BELOW_RANGE, errCode_T_ABOVE_RANGE> retVal;

		retVal.Init(*this, varName);

		return retVal;
	}

	auto operator()(const char * varName);
};

struct Olga_Trend {
	aVect<char> name;
	aVect<char> type;
	aVect<char> branchName;
	aVect<char> pipeName;
	aVect<char> sectionIndex;
	aVect<char> unit;
	aVect<char> description;
	mVect<double> time;
	aVect<double> values;

	void AddSerie(AxisClass& axis) {

		axis.AddSerie(this->time.ToAvect(), this->values, RGB(0, 0, 255));
	}

	void Plot(AxisClass * axisPtr = nullptr, bool forceAutoFit = false) {

		AxisClass localAxis;

		AxisClass& axis = axisPtr ? *axisPtr : localAxis;

		if (!axis) axis.Create().BringOnTop();

		axis.ClearSeries();
		this->AddSerie(axis);

		STATIC_ASSERT_SAME_TYPE(aVect<char>, this->name);
		STATIC_ASSERT_SAME_TYPE(aVect<char>, this->unit);
		STATIC_ASSERT_SAME_TYPE(aVect<char>, this->description);
		STATIC_ASSERT_SAME_TYPE(aVect<char>, this->branchName);
		STATIC_ASSERT_SAME_TYPE(aVect<char>, this->pipeName);
		STATIC_ASSERT_SAME_TYPE(aVect<char>, this->sectionIndex);

		auto title = xFormat(L"variable \"%S\" [%S] (\"%S\")", this->name, this->unit, this->description);

		if (this->branchName)   title.AppendFormat(L", branch \"%S\"", this->branchName);
		if (this->pipeName)     title.AppendFormat(L", pipe \"%S\"", this->pipeName);
		if (this->sectionIndex) title.AppendFormat(L", section index \"%S\"", this->sectionIndex);

		axis.SetTitle(title).SetFont().Title().FontSize(12);

		axis.xLabel(L"Time [s]")
			.yLabel(xFormat(L"%s [%s]", this->description, this->unit))
			.AutoFit(true, forceAutoFit).Refresh();
	}

}; 

struct Olga_Profile {
	aVect<char> name;
	aVect<char> branch;
	aVect<char> geometryUnit;
	aVect<char> description;
	mVect<double> time;
	aVect<double> x;
	aVect<double> z;
	aVect<aVect<double>> values;
	aVect<char> unit;

	void AddSerie(AxisClass& axis, double time) {

		aVect<double> * serie = nullptr;

		for (auto&& t : this->time) {
			if (t >= time) {
				serie = &this->values[this->time.Index(t)];
				time = t;
				break;
			}
		}

		if (!serie) MY_ERROR("time not in range");

		axis.AddSerie(this->x, *serie, RGB(0,0,255));
	}

	void Plot(double time, AxisClass * axisPtr = nullptr, bool forceAutoFit = false) {

		AxisClass localAxis;

		AxisClass& axis = axisPtr ? *axisPtr : localAxis;

		if (!axis) axis.Create().BringOnTop();

		axis.ClearSeries();
		this->AddSerie(axis, time);

		STATIC_ASSERT_SAME_TYPE(aVect<char>, this->branch);
		STATIC_ASSERT_SAME_TYPE(aVect<char>, this->name);
		STATIC_ASSERT_SAME_TYPE(aVect<char>, this->geometryUnit);

		axis.SetTitle(xFormat(L"Branch \"%S\", variable \"%S\"", this->branch, this->name))
			.xLabel(xFormat(L"Pipe length [%S]", this->geometryUnit))
			.yLabel(xFormat(L"%S [%S]", this->description, this->unit));


		if (forceAutoFit) {
			auto historyMin = std::numeric_limits<double>::infinity();
			auto historyMax = -std::numeric_limits<double>::infinity();

			for (auto&& t : this->time) {
				auto&& values = this->values[this->time.Index(t)];
				for (auto&& v : values) {
					if (v > historyMax && std::isfinite(v)) historyMax = v;
					if (v < historyMin && std::isfinite(v)) historyMin = v;
				}
			}
			axis.xLim(this->x[0], this->x.Last(), true)
				.yLim(historyMin, historyMax, true)
				.SetZoomed();
		}
		else {
			axis.AutoFit(true, forceAutoFit);
		}

		axis.Refresh();
	}
};

struct Olga_TrendExtractor {

	aVect<char> filePath;
	aVect<Olga_OutputVariable> variables;
	bool displayProgressBar = true;

	Olga_TrendExtractor() {}
	Olga_TrendExtractor(const char * filePath) : filePath(filePath) {}
	
	Olga_TrendExtractor& AddVariable(
		const char * name, 
		const char * type = nullptr,
		const char * typeName = nullptr,
		const char * unit = nullptr)
	{

		this->variables.Push(name, type, typeName, unit);
		return *this;
	}

	Olga_TrendExtractor& SetFile(const char * filePath) {
		this->~Olga_TrendExtractor();
		new (this) Olga_TrendExtractor(filePath);

		return *this;
	}

	//aVect<Olga_Trend> GetCatalog() {
	//	return std::move(this->GetVariables<true>(false));
	//}

	aVect<Olga_Trend> Extract() {

		auto foundTime = false;

		for (auto&& v : this->variables) {
			if (StrEqual(v.name, "time")) foundTime = true;
		}

		if (!foundTime) this->variables.Insert(0, "time");

		size_t N = this->variables.Count();

		aVect<aVect<char>> varNames(N), types(N), branchNames(N), 
			pipeNames(N), sectionIndices(N),  units(N), descriptions(N);

		for (size_t i = 0; i < N; ++i) {
			varNames[i]		= this->variables[i].name;
			types[i]		= this->variables[i].type;
			branchNames[i]	= this->variables[i].branchName;
			pipeNames[i]    = this->variables[i].pipeName;
			sectionIndices[i] = this->variables[i].sectionIndex;
			units[i]		= this->variables[i].unit;
			descriptions[i] = this->variables[i].description;
		}

		auto values = Olga_GetTrends(
			this->filePath, varNames, -1, nullptr,
			nullptr, false, &types, &branchNames, &pipeNames,
			&sectionIndices, &units, &descriptions);

		mVect<double> time = values.Pop(0);
		
		varNames.Remove(0);
		types.Remove(0);
		branchNames.Remove(0);
		pipeNames.Remove(0);
		sectionIndices.Remove(0);
		descriptions.Remove(0);
		units.Remove(0);
		

		N = varNames.Count();

		aVect<Olga_Trend> trends(N);

		for (size_t i = 0; i < N; ++i) {
			trends[i].time.Reference(time);
			trends[i].name		   = std::move(varNames[i]);
			trends[i].type		   = std::move(types[i]);
			trends[i].branchName   = std::move(branchNames[i]);
			trends[i].pipeName     = std::move(pipeNames[i]);
			trends[i].sectionIndex = std::move(sectionIndices[i]);
			trends[i].description  = std::move(descriptions[i]);
			trends[i].unit		   = std::move(units[i]);
			trends[i].values       = std::move(values[i]);
		}

		return std::move(trends);
	}
};

class Olga_ProfileExtractor {

	aVect<char> filePath;
	aVect<Olga_OutputVariable> variables;
	bool displayProgressBar = true;

	static void ToCentroids(
		const Olga_OutputVariable & v,
		Olga_Profile & prof,
		const aVect<Olga_OutputVariable> * pVariables,
		const mVect<mVect<double>> & arrays,
		char * str,
		aVect<double> Olga_Profile::*mPtr)
	{
		bool found = false;
		size_t i = -1;
		for (auto&& var : *pVariables) {
			++i;
			if (strcmp_caseInsensitive(var.branchName, v.branchName) == 0 &&
				strcmp_caseInsensitive(var.name, str) == 0)
			{
				if (found) MY_ERROR("Multiple matches");
				found = true;
				prof.geometryUnit = var.unit;
				auto&& a = arrays[i];
				if (v.isBoundaryValues) {
					prof.*mPtr = a.ToAvect();
				}
				else {
					(prof.*mPtr).Redim(a.Count() - 1);
					aVect_static_for(prof.*mPtr, j) {
						(prof.*mPtr)[j] = 0.5*(a[j] + a[j + 1]);
					}
				}
			}
		}

		if (!found) MY_ERROR("not found");
	}

	template <bool onlyCatalog>
	static bool Olga_Profiles_Func(
		double time,
		aVect<Olga_OutputVariable> * pVariables,
		mVect< mVect<double> > & arrays,
		bool reInit,
		bool noMoreTimeSteps,
		bool beforeRestart,
		mVect< mVect<char> > * pTplVarNames,
		mVect<double> * pTplArray,
		void * userParam)
	{

		static bool init;

		auto& profiles = *(aVect<Olga_Profile> *)userParam;

		if (noMoreTimeSteps) return true;

		if (reInit) init = false;

		if (!pVariables || beforeRestart) return false;

		if (!init) {
			init = true;
			profiles.Redim<false>(0);

			size_t i = -1;
			for (auto&& var : *pVariables) {

				i++;

				if (var.isGeometry) continue;

				auto&& prof = profiles.Push().Last();
				prof.name = var.name;
				prof.branch = var.branchName;
				prof.description = var.description;
				//prof.geometryUnit = var.isGeometry ? var.unit : nullptr;
				prof.unit = var.unit;

				if (&prof != &profiles[0]) {
					prof.time.Reference(profiles[0].time);
				}

				Olga_ProfileExtractor::ToCentroids(var, prof, pVariables, arrays, "Boundaries", &Olga_Profile::x);
				Olga_ProfileExtractor::ToCentroids(var, prof, pVariables, arrays, "BoundaryElevation", &Olga_Profile::z);
			}

			if (onlyCatalog) {
				return true;
			}
		}

		if (profiles) profiles[0].time.Push(time);

		size_t iProf = -1;
		size_t iArrays = -1;
		for (auto&& var : *pVariables) {

			++iArrays;

			if (var.isGeometry) continue;
			++iProf;

			auto&& prof = profiles[iProf];

			prof.values.Push().Last().Steal(arrays[iArrays]);
		}

		return false;
	}

	template<bool onlyCatalog>
	aVect<Olga_Profile> GetVariables(bool displayProgressBar) {

		aVect<Olga_Profile> profiles;

		Olga_Profiles_Func<onlyCatalog>(0, nullptr, mVect< mVect<double>>(), true, false, false, nullptr, nullptr, &profiles);

		Olga_WalkProfiles(this->filePath, &this->variables, Olga_Profiles_Func<onlyCatalog>, nullptr, nullptr,
			nullptr, nullptr, nullptr, nullptr, nullptr, &profiles, displayProgressBar);

		return std::move(profiles);
	}


public:
	Olga_ProfileExtractor() {}
	Olga_ProfileExtractor(const char * filePath) : filePath(filePath) {}
	Olga_ProfileExtractor(const wchar_t * filePath) : filePath(filePath) {
		MY_ERROR("TODO : wide char files");
	}

	Olga_ProfileExtractor& AddVariable(const char * name, const char * branch = nullptr) {

		this->variables.Push(name, "BRANCH", branch);
		return *this;
	}

	//Olga_ProfileExtractor& SetFile(const wchar_t * filePath) {
	//	this->~Olga_ProfileExtractor();
	//	new (this) Olga_ProfileExtractor(filePath);

	//	return *this;
	//}

	Olga_ProfileExtractor& SetFile(const char * filePath) {
		//return this->SetFile(aVect<wchar_t>(filePath));
		this->~Olga_ProfileExtractor();
		new (this) Olga_ProfileExtractor(filePath);

		return *this;
	
	}

	aVect<Olga_Profile> GetCatalog() {
		return std::move(this->GetVariables<true>(false));
	}

	aVect<Olga_Profile> Extract() {
		return std::move(this->GetVariables<false>(this->displayProgressBar));
	}
};


struct PVT_Variable_Cache {

	size_t iP, iT;
	double last_T, last_P, last_value;
	double P1, P2, T1, T2;
	double v11, v12, v21, v22;
	double precomputedFactor;
	 
	PVT_Variable_Cache() {
		this->last_T = this->last_P = this->last_value = std::numeric_limits<double>::quiet_NaN();
	}

	void Update(const Olga_PVT_Table & PVT_table, const aMat<double> & table) {

		this->T1 = PVT_table.T[this->iT];
		this->T2 = PVT_table.T[this->iT + 1];
		this->P1 = PVT_table.P[this->iP];
		this->P2 = PVT_table.P[this->iP + 1];
		this->v11 = table(this->iP, this->iT);
		this->v12 = table(this->iP, this->iT + 1);
		this->v21 = table(this->iP + 1, this->iT);
		this->v22 = table(this->iP + 1, this->iT + 1);
		this->precomputedFactor = PVT_table.precomputedFactor(this->iP, this->iT);
	}
};

inline auto Olga_PVT_Table::operator()(const char * varName) {
	return this->GetVariable(varName);
}

#endif//_OLGAUTILS_

#ifdef _WIN32_BKP
#define _WIN32 _WIN32_BKP
#define _MSC_VER _MSC_VER_BKP
#undef _WIN32_BKP
#undef _MSC_VER_BKP
#endif

