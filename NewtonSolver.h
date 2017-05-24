#ifndef _NEWTONSOLVER_
#define _NEWTONSOLVER_

//#define NEWTONSOLVER_BUGHUNT_1

#include "WinUtils.h"
#include "xMat.h"
#include "Axis.h"
#include "MagicRef.h"
#include <functional>

extern "C" {
#include "cs.h"
}

extern "C" {
#include "klu_internal.h"
}

#include "klu.h"

//#pragma warning( push )
//#pragma warning( disable : 4244 4800 )
//#include "mpirxx.h"
//#pragma warning( pop ) 

#pragma warning( push )
#pragma warning( disable : 4091 )
#include "Dbghelp.h"
#pragma warning( pop ) 
#include <limits>

#define NEWTONSOLVER_SET_FUNCTION(functionObject, Functions, function, problem)		\
functionObject.ComputeAutoDependancy(&Functions<true, true>::function, problem);	\
if (functionObject.usingBoundaryValue) {											\
	functionObject.SetFunction(&Functions<true>::function);							\
} else {																			\
	functionObject.SetFunction(&Functions<>::function);								\
}																					\
functionObject.name = xFormat(#functionObject "<%d>::" #function, functionObject.usingBoundaryValue);

#define USE_GLOBAL_GO_EVENT

template <class GlobalParams, class LocalParams, class Boundaries>
class Problem;

//template <class, unsigned, size_t, bool, bool>
//struct Convolution;

struct Boundary {

	MagicTarget<double> inf, sup;

	Boundary() :
		inf(std::numeric_limits<double>::infinity()),
		sup(std::numeric_limits<double>::infinity())
	{}

	Boundary(const Boundary &) = default;
	Boundary(Boundary &&) = default;
	Boundary & operator=(const Boundary &) = default;
	Boundary & operator=(Boundary &&) = default;
};


template <class UserDomain>
class Domain :public UserDomain {

	aVect<double> mesh;
	aVect<double> centroids;

	void ComputeCentroids() {
		this->centroids.Redim(this->mesh.Count() - 1);
		for (size_t i = 0, I = this->mesh.Count() - 1; i < I; ++i) {
			this->centroids[i] = 0.5 * (this->mesh[i] + this->mesh[i + 1]);
		}
	}

public:

	//UserBoundaries boundaries;

	double Mesh(int i) {
		return this->mesh[i];
	}

	double Centroid(int i) {
		return this->centroids[i];
	}

	const aVect<double> & GetMesh() const {
		return this->mesh;
	}

	const aVect<double> & GetCentroids() const {
		return this->centroids;
	}

	Domain & SetMesh(const aVect<double> & mesh) {
		this->mesh.Copy(mesh);
		this->ComputeCentroids();
		return *this;
	}

	Domain & SetMesh(aVect<double> && mesh) {
		this->mesh.Steal(mesh);
		this->ComputeCentroids();
		return *this;
	}
};

template <unsigned nVar = 1>
struct ConvolutionPoint {
	int ind[nVar];
	double coef;

	MagicPointer<double> ptr;

#ifdef _DEBUG
	double x, dist;
	int iMesh;
#endif

	static_assert(nVar < 4, "not implemented");

	template <bool usingBoundaryValue, bool singlePoint>
	double Value(const aVect<double> & X) {
		if (usingBoundaryValue) {
			if (singlePoint && nVar == 1) {
				return this->ind[0] < 0 ? *this->ptr : X[this->ind[0]];
			}
			double val = 1;
			for (int i = 0; i < nVar; ++i) {
				val *= this->ind[i] < 0 ? *this->ptr : X[this->ind[i]];
			}

			if (singlePoint) return val;

			return val* this->coef;
		} else {
			double val = X[this->ind[0]];
			if (nVar > 1) val *= X[this->ind[1]];
			if (nVar > 2) val *= X[this->ind[2]];
			if (nVar > 3) val *= X[this->ind[3]];

			if (singlePoint) {
				return val;
			}

			return val* this->coef;
		}
	}
};

inline aVect<int> *& Get_g_currentFuncObjInd() {
	static aVect<int> * g_currentFuncObjInd;
	return g_currentFuncObjInd;
}

inline bool *& Get_g_currentFuncObjUsingBoundaryFlag() {
	static bool * g_currentFuncObjUsingBoundaryFlag;
	return g_currentFuncObjUsingBoundaryFlag;
}

template <class T> void AutoDependancy(int varInd, T * setCurrentFuncObj = nullptr) {

	if (varInd == -1) {
		*Get_g_currentFuncObjUsingBoundaryFlag() = true;
		return;
	}

	if (setCurrentFuncObj) {
		Get_g_currentFuncObjInd() = &setCurrentFuncObj->I;
		Get_g_currentFuncObjUsingBoundaryFlag() = &setCurrentFuncObj->usingBoundaryValue;
	} else {
		if (!Get_g_currentFuncObjInd()->Find(varInd)) Get_g_currentFuncObjInd()->Push(varInd);
	}
}

template <
	class UserDomain = EmptyClass,
	unsigned nVar = 1,
	bool usingBoundaryValues = false,
	bool autoDependancy = false
>

struct Convolution {

	aVect<ConvolutionPoint<nVar> > points;
	int iDomain;

#ifdef _DEBUG
	double xOrig;
#endif

	bool UsingBoundaryValue() const {
		return this->UsingBoundaryValue_Inf() || this->UsingBoundaryValue_Sup();
	}

	bool UsingBoundaryValue_Inf() const {

		if (!this->points) return false;

		for (unsigned j = 0; j < nVar; ++j) {
			if (this->points[0].ind[j] == -1) return true;
		}

		return false;
	}

	bool UsingBoundaryValue_Sup() const {

		if (!this->points) return false;

		for (unsigned j = 0; j < nVar; ++j) {
			if (this->points.Last().ind[j] == -1) return true;
		}

		return false;
	}

	double PairWiseSum(const aVect<double> & v, size_t i, size_t j) const {
		if (i == j) return v[i];
		size_t n = j - i;
		if (n == 1) return v[i] + v[j];
		return PairWiseSum(v, i, i + n / 2) + PairWiseSum(v, i + n / 2 + 1, j);
	}

	double PairWiseSum(const aVect<double> & v) const {
		return PairWiseSum(v, 0, v.Count() - 1);
	}

	double SortedSum(aVect<double> & v) const {

		QuickSort<true>(v);

		double sum = 0;
		aVect_static_for(v, i) sum += v[i];

		return sum;
	}

	template <class T, class U, class V>
	double operator()(const Problem<T, U, V> & p) const {

		return this->operator()(p.GetVector());
	}

	template <class T, class U, class V>
	double operator()(const Problem<T, U, V> & p, const aVect<double> & X) const {

		static_assert(false, "obsolete, use operator()(const aVect<double> & X)");

		return 0;
	}

	double operator()(const aVect<double> & X) const {

		if (autoDependancy) {
			for (auto&& p : this->points) {
				for (auto i : p.ind) {
					AutoDependancy<FunctionObject<>>(i);
				}
			}
			//return std::numeric_limits<double>::quiet_NaN();
		}

#ifdef _DEBUG
		if (!this->points) MY_ERROR("uninitialized convolution");
#endif

		auto N = (int)this->points.Count();

		if (N == 1) return this->points[0].Value<usingBoundaryValues, true>(X);

		double value = 0;

		for (int i = 0; i < N; ++i) value += this->points[i].Value<usingBoundaryValues, false>(X);

		return value;
	}

	template <class T, class U, class V>
	double NormalizeFactor(const Problem<T, U, V> & p) const {

		const auto & domain = p->domains[this->iDomain];
		const int N = (int)this->points.Count() - boundary_Sup;

		double value = 0;

		if (boundary_Inf) value += abs(this->points[0].coef);
		if (boundary_Sup) value += abs(this->points[N].coef);

		for (int i = boundary_Inf; i < N; ++i) value += abs(this->points[i].coef);

		return value;
	}

	Convolution() : iDomain(0) {}

	Convolution(Convolution & that) :
		iDomain(that.iDomain),
#ifdef _DEBUG
		xOrig(that.xOrig),
#endif
		points(that.points)
	{}

	Convolution(Convolution && that) :
		iDomain(that.iDomain),
#ifdef _DEBUG
		xOrig(that.xOrig),
#endif
		points(std::move(that.points))
	{}
};

template <class GlobalParameters = EmptyClass, class UserDomain = EmptyClass>
struct Space_Parameters : GlobalParameters {
	MagicCopyIncrementer copyIncrementer;
	aVect<Domain<UserDomain> > domains;
	MagicCopyDecrementer copyDecrementer;
};

template <
	class UserDomain,
	template <bool ...> class LocalInfo,
	bool... args>
struct Space_LocalParameters : LocalInfo<args...> {

	int iDomain, iMesh;

	Space_LocalParameters() : iMesh(-1), iDomain(0) {}

	template <class T>
	double Mesh(const T & p) const {
		return p->domains[iDomain].Mesh(this->iMesh);
	}

	template <class T>
	double Centroid(const T & p) const {
		return p->domains[iDomain].Centroid(this->iMesh);
	}

	void SetDomain(int i) {
		LocalInfo::SetDomain(i);
		this->iDomain = i;
	}
};

template <class GlobalParameters, class UserDomain, template <bool...> class LocalParameters, bool... args>
using SpaceProblem = Problem < Space_Parameters<GlobalParameters, UserDomain>, Space_LocalParameters<UserDomain, LocalParameters, args...>, UserDomain>;

struct Variable;



template <class GlobalParams = EmptyClass, class LocalParams = EmptyClass, class Boundaries = EmptyClass>
struct FunctionObject : LocalParams {

	typedef void (FunctionObject::*DerivativeFunction)(const Problem<GlobalParams, LocalParams, Boundaries> & p, int funcInd, aVect<ptrdiff_t> & varIndices, aVect<double> & values) const;

	aVect<char> name;

	double (FunctionObject::*function)(const Problem<GlobalParams, LocalParams, Boundaries> & p) const;

	DerivativeFunction derivatives;
	bool usingBoundaryValue = false;
	aVect<int> I;

	FunctionObject() : function(nullptr), derivatives(nullptr) {}
	
	template <class T>
	void SetFunction(T f) {
		this->function = (decltype(this->function))f;
	}

	template < class Dom, template <bool, bool> class LI, bool boundaryValue, bool autoDep>
	auto GetAutoDepProbType(Space_LocalParameters<Dom, LI, boundaryValue, autoDep>&) {
		return (Problem<GlobalParams, Space_LocalParameters<Dom, LI, true, true>, Boundaries>*)0;
	}

	template <
		class T,
		template <bool...> class U,
		bool boundaryValue,
		bool autoDep,
		class SP
	>
	void ComputeAutoDependancy(T U<boundaryValue, autoDep>::*f, SP&& p) {

		auto&& depProb = *(decltype(GetAutoDepProbType(LocalParams())))&p;
		auto depFuncPtr = (decltype(depProb.functions[0].function))f;
		auto depThis = (decltype(&depProb.functions[0]))this;

		AutoDependancy(0, this);

		static aVect<double> bkp, X;
		static aVect<bool> unconstrainedIndex;

		bkp = X = p.GetVector();

		unconstrainedIndex.Redim(X.Count()).Set(true);

		for (auto&& v : p.validValuesRange) unconstrainedIndex[v.ind] = false;

		aVect_static_for(unconstrainedIndex, i) {
			if (!unconstrainedIndex[i]) continue;
			if (!X[i]) X[i] = 0.1;
		}

		p.InitVector(X);

		(depThis->*depFuncPtr)(depProb);

		aVect_static_for(unconstrainedIndex, i) {
			if (!unconstrainedIndex[i]) continue;
			X[i] *= -1;
		}
		p.InitVector(X);

		(depThis->*depFuncPtr)(depProb);

		p.InitVector(bkp);
		
	}

	template <class T>
	void SetDerivativesFunction(T f) {
		this->derivatives = (decltype(this->derivatives))f;
	}

	double CallFunc(const Problem<GlobalParams, LocalParams, Boundaries> & p) const {
		return (this->*this->function)(p);
	}

	FunctionObject & AddDependency(int i) {
		if (!this->I.Find(i)) this->I.Push(i);
		return *this;
	}

	template <
		class UserDomain,
		unsigned nVar,
		bool usingBoundaryValue>
	FunctionObject & AddDependency(const Convolution<UserDomain, nVar, usingBoundaryValue> & conv) {
		aVect_static_for(conv.points, i) {
			for (int j = 0; j < nVar; ++j) {
				int ind = conv.points[i].ind[j];
				if (ind == -1) {
					this->usingBoundaryValue = true;
				} else {
					this->AddDependency(ind);
				}
			}
		}

		return *this;
	}

	template <
		class UserDomain,
		unsigned nVar,
		bool usingBoundaryValue>
	FunctionObject & AddDependency(const aVect<Convolution<UserDomain, nVar, usingBoundaryValue>> & convs) {
		
		for (auto&& c : convs) this->AddDependency(c);

		return *this;
	}

	FunctionObject & AddDependency(const aVect<Variable> & vars) {

		for (auto&& v : vars) this->AddDependency(v);

		return *this;
	}
};

template <class Prob, class GlobalParameters>
struct MultiThread_ThreadInfo {

	bool masterThread;
	int i1, i2;

	Prob * pMasterProblem;
	Prob * pWorkerProblem;
	HANDLE hEventReady;

	HANDLE threadHandle;

#ifdef USE_GLOBAL_GO_EVENT
	HANDLE hEventGo[2];
	int currentEventGo;
#else
	HANDLE hEventGo;
#endif
	HANDLE hEventCloseThread;
	size_t threadID;

	MultiThread_ThreadInfo(Prob * pProblem, int i1, int i2, int threadNumber) : //Constructor to be called from master thread
		pMasterProblem(pProblem),
		pWorkerProblem(nullptr),
		masterThread(true),
		threadHandle(NULL),
#ifdef USE_GLOBAL_GO_EVENT
		currentEventGo(0),
#endif
		i1(i1),
		i2(i2)
	{
		this->hEventCloseThread = SAFE_CALL(CreateEvent(NULL, true, false, NULL));
		this->hEventReady       = SAFE_CALL(CreateEvent(NULL, false, false, NULL));
#ifdef USE_GLOBAL_GO_EVENT
		this->hEventGo[0] = pProblem->multiThreading.master.hEventGo[0];
		this->hEventGo[1] = pProblem->multiThreading.master.hEventGo[1];
#else
		this->hEventGo          = SAFE_CALL(CreateEvent(NULL, false, false, NULL));
#endif
		HANDLE hThread = SAFE_CALL(CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ProblemThreadProc<Prob, GlobalParameters>, (void*)this, 0, (LPDWORD)&this->threadID));
		//SAFE_CALL(SetThreadPriority(hThread, THREAD_PRIORITY_BELOW_NORMAL));

		DWORD_PTR affinityMask = (DWORD_PTR)1 << (DWORD_PTR)threadNumber;

		SAFE_CALL(SetThreadAffinityMask(hThread, affinityMask));

		this->threadHandle = hThread;
		//SAFE_CALL(CloseHandle(hThread));
	}

	MultiThread_ThreadInfo(MultiThread_ThreadInfo & that) : // Constructor to be called from worker threads
		masterThread(false),
		i1(that.i1),
		i2(that.i2),
		hEventReady(that.hEventReady),
#ifdef USE_GLOBAL_GO_EVENT
		currentEventGo(that.currentEventGo),
#else
		hEventGo(that.hEventGo),
#endif
		hEventCloseThread(that.hEventCloseThread),
		threadID(that.threadID),
		pMasterProblem(that.pMasterProblem),
		pWorkerProblem(that.pWorkerProblem)
	{
#ifdef USE_GLOBAL_GO_EVENT
		this->hEventGo[0] = that.hEventGo[0];
		this->hEventGo[1] = that.hEventGo[1];
#endif
	}

	MultiThread_ThreadInfo(MultiThread_ThreadInfo && that) :
		masterThread(that.masterThread),
		i1(that.i1),
		i2(that.i2),
		hEventReady(that.hEventReady),
#ifdef USE_GLOBAL_GO_EVENT
		currentEventGo(that.currentEventGo),
#else
		hEventGo(that.hEventGo),
#endif
		hEventCloseThread(that.hEventCloseThread),
		threadID(that.threadID),
		pMasterProblem(that.pMasterProblem),
		pWorkerProblem(that.pWorkerProblem)
	{
#ifdef USE_GLOBAL_GO_EVENT
		this->hEventGo[0] = that.hEventGo[0];
		this->hEventGo[1] = that.hEventGo[1];
#endif

		i1 = -1;
		i2 = -1;
		that.hEventReady = NULL;
#ifdef USE_GLOBAL_GO_EVENT
		that.hEventGo[0] = NULL;
		that.hEventGo[1] = NULL;
		that.currentEventGo = -1;
#else
		that.hEventGo = NULL;
#endif
		that.hEventCloseThread = NULL;
		that.threadID = -1;
		that.pMasterProblem = nullptr;
		that.pWorkerProblem = nullptr;
	}

	~MultiThread_ThreadInfo() {

		if (this->masterThread) {
			SAFE_CALL(SetEvent(this->hEventCloseThread));
			WaitForSingleObject(this->threadHandle, INFINITE);
			SAFE_CALL(CloseHandle(this->threadHandle));
		}
		else {
			if (this->hEventReady) SAFE_CALL(CloseHandle(this->hEventReady));
#ifdef USE_GLOBAL_GO_EVENT
#else
			if (this->hEventGo) SAFE_CALL(CloseHandle(this->hEventGo));
#endif
			if (this->hEventCloseThread) SAFE_CALL(CloseHandle(this->hEventCloseThread));
		}
	}
};

template<
	class Prob, 
	class GlobalParams
> 
bool WaitForGo(
	MultiThread_ThreadInfo<Prob, GlobalParams> & localParams, 
	Prob & localProblem) 
{

#ifdef USE_GLOBAL_GO_EVENT
	HANDLE hEventGo = localParams.hEventGo[localParams.currentEventGo];
	HANDLE handles[2] = { hEventGo, localParams.hEventCloseThread };
	localParams.currentEventGo = 1 - localParams.currentEventGo;
#else
	HANDLE handles[2] = { localParams.hEventGo, localParams.hEventCloseThread };
#endif

	//QueryPerformanceCounter(&localProblem.multiThreading.stopTicks.Grow(1).Last());
	//localProblem.multiThreading.workType.Push(localProblem.multiThreading.pMasterProblem->multiThreading.master.workType);

	DWORD result = WaitForMultipleObjects(2, handles, false, INFINITE);

	if (result == WAIT_ABANDONED) {
		MY_ERROR("WaitForMultipleObjects failed");
		return false;
	}
	if (result - WAIT_OBJECT_0 == 0) {
		QueryPerformanceCounter(&localProblem.multiThreading.startTicks.Grow(1).Last());
		localProblem.multiThreading.workType.Push(localProblem.multiThreading.pMasterProblem->multiThreading.master.workType);
		return true;
	}
	else {
		return false;
	}
}

template <class Prob>
bool CheckFinite(Prob * pMasterProblem, Prob * localProblem, bool checkFinite, int failSafe, double dfi_dxj, int i, ptrdiff_t j) {
	if ((checkFinite || failSafe) && !_finite(dfi_dxj)) {
		auto func = [pMasterProblem, i, j]() {
			return xFormat("Derivative not finite:\nFunction \"%s\"\nVariable \"%s\"", pMasterProblem->functions[i].name, pMasterProblem->variableNames[j]);
		};
		if (failSafe) {
			if (localProblem) localProblem->multiThreading.worker.failure = true;
			
			pMasterProblem->SetLastSolveError(func, 1);
			return false;
		}
		MY_ERROR(func());
		return false;
	}
	return true;
}

template <
	class Prob, 
	class GlobalParams>
bool ProblemThreadCompute(
	MultiThread_ThreadInfo<Prob, GlobalParams> & localParams,
	Prob & localProblem,
	Prob * pMasterProblem,
	bool checkFinite,
	int failSafe,
	bool allowZeroSparseCoeff)
{

	localProblem.multiThreading.worker.failure = false;
	localProblem.multiThreading.worker.exception = false;
	localProblem.multiThreading.worker.exception_ptr = nullptr;

	localProblem.X.Copy(pMasterProblem->X);

	localProblem.debugMode = pMasterProblem->debugMode;
	localProblem.allowZeroSparseCoeff = pMasterProblem->allowZeroSparseCoeff;
	
	Prob::WorkType  workType = pMasterProblem->multiThreading.master.workType;

	if (workType == Prob::compute_Jacobian) {
		localProblem.F_X.Copy(pMasterProblem->F_X);
		localProblem.jacobian.row.Redim(0);
		localProblem.jacobian.col.Redim(0);
		localProblem.jacobian.values.Redim(0);
	}

	try {
		if (workType == Prob::compute_Jacobian) {

			if (false && localProblem.debugMode >= 100) {
				for (int i = localParams.i1; i < localParams.i2; ++i) {
					auto check = localProblem.CallFunc<false>(i);
					if (check != localProblem.F_X[i]) MY_ERROR("gotcha");
				}
			}

			for (int i = localParams.i1; i < localParams.i2; ++i) {

				auto & func_i = pMasterProblem->functions[i];

				if (func_i.derivatives) {
					size_t oldN = localProblem.jacobian.col.Count();
					//(func_i.*func_i.derivatives)(*pMasterProblem, i, localProblem.jacobian.col, localProblem.jacobian.values);
					(func_i.*func_i.derivatives)(localProblem, i, localProblem.jacobian.col, localProblem.jacobian.values);
					for (size_t j = oldN, J = localProblem.jacobian.col.Count(); j < J; ++j) {
						localProblem.jacobian.row.Push(i);
						if (!CheckFinite(pMasterProblem, &localProblem, checkFinite, failSafe, localProblem.jacobian.values[j], i, localProblem.jacobian.col[j])) return false;
					}
				}
				else {
					aVect_static_for(func_i.I, k) {
						int j = func_i.I[k];
						double dfi_dxj = localProblem.Derivative(i, j);

						if (false && pMasterProblem->debugMode >= 100) {
							aVect_static_for(localProblem.X, i) {
								if (localProblem.X[i] != pMasterProblem->X[i]) {
									int a = 0;
									MY_ERROR("gotcha");
								}
							}
						}

						if (false && pMasterProblem->debugMode >= 100) {
							if (pMasterProblem->jacobian.full) {
								auto test = pMasterProblem->jacobian.full(i, j);
								if (dfi_dxj != test) MY_ERROR("gotcha");
							}
							for (auto k = 0; k < 1; k++) {
								auto test = localProblem.Derivative(i, j);
								if (dfi_dxj != test) MY_ERROR("gotcha");
							}
						}
						if (allowZeroSparseCoeff || dfi_dxj) {
						//double dfi_dxj = localProblem.Derivative(i, j);
							
							if (!CheckFinite(pMasterProblem, &localProblem, checkFinite, failSafe, dfi_dxj, i, j)) return false;

							if (localProblem.sparse) {
								localProblem.jacobian.values.Push(dfi_dxj);
								localProblem.jacobian.row.Push(i);
								localProblem.jacobian.col.Push(j);
							}
							else {
								MY_ERROR("TODO");
								//this->jacobian.full(i, j) = dfi_dxj;
							}
						}
					}
				}
			}
		}
		else {
			if (checkFinite || failSafe) {
				for (int i = localParams.i1; i < localParams.i2; ++i) {
					if (!_finite(pMasterProblem->CallFunc<true>(i))) {
						if (failSafe) {
							localProblem.multiThreading.worker.failure = true;
							auto func = [pMasterProblem, i]() {
								return xFormat("Function not finite:\nFunction \"%s\"", pMasterProblem->functions[i].name);
							};
							pMasterProblem->SetLastSolveError(func, 1);
							break;//return true;
						}
						else MY_ERROR("not finite");
					}
				}
			}
			else for (int i = localParams.i1; i < localParams.i2; ++i) {
				pMasterProblem->CallFunc<true>(i);
			}
		}
	}
	catch (...) {
		localProblem.multiThreading.worker.failure = true;
		localProblem.multiThreading.worker.exception = true;
		auto&& exception_ptr = localProblem.multiThreading.worker.exception_ptr;
		exception_ptr = std::current_exception();
		MY_ASSERT(!!exception_ptr);
		auto func = [&exception_ptr]() -> aVect<char> {
			try { std::rethrow_exception(exception_ptr); }
			catch (const std::exception &e) { return e.what(); }
			catch (const std::string    &e) { return e.c_str(); }
			catch (const char           *e) { return e; }
			catch (...) { return "unknown exception"; }
			return "???";	//happy compiler
		};
		pMasterProblem->SetLastSolveError(func, 1);
	}

	QueryPerformanceCounter(&localProblem.multiThreading.setEventTicks.Grow(1).Last());
	SAFE_CALL(SetEvent(localParams.hEventReady));

	if (!WaitForGo(localParams, localProblem)) return false;
	else return true;
}

template<class Prob, class GlobalParams>
DWORD ProblemThreadProc(LPVOID lpThreadParameter) {

	MultiThread_ThreadInfo<Prob, GlobalParams> * pMasterParams = (MultiThread_ThreadInfo<Prob, GlobalParams>*)lpThreadParameter;

	MultiThread_ThreadInfo<Prob, GlobalParams> localParams(*pMasterParams);

	Prob localProblem(*pMasterParams);

	QueryPerformanceCounter(&localProblem.multiThreading.startTicks.Grow(1).Last());
	localProblem.multiThreading.workType.Push(localProblem.multiThreading.pMasterProblem->multiThreading.master.workType);

	Prob * pMasterProblem = localParams.pMasterProblem;

	for (;;) {

		bool checkFinite = pMasterProblem->multiThreading.master.checkFinite;
		int failSafe = pMasterProblem->multiThreading.master.failSafe;
		bool allowZeroSparseCoeff = pMasterProblem->multiThreading.master.allowZeroSparseCoeff;

		localParams.i1 = pMasterParams->i1;
		localParams.i2 = pMasterParams->i2;

		localProblem.nCurrentIterations = pMasterProblem->nCurrentIterations;
		localProblem.nRetryNewtonRaphson = pMasterProblem->nRetryNewtonRaphson;

		if (!ProblemThreadCompute(localParams, localProblem, pMasterProblem, checkFinite, failSafe, allowZeroSparseCoeff)) {
			return 0;
		}
	}
	
}

template <bool mesureTiming>
class AutoMesureFunctionTiming {

	PerformanceChronometer & chrono;

public:

	AutoMesureFunctionTiming(PerformanceChronometer & chrono) : chrono(chrono) {
		chrono.Resume();
	}

	~AutoMesureFunctionTiming() {
		chrono.Stop();
	}
};

template <> class AutoMesureFunctionTiming<false> {
public:

	AutoMesureFunctionTiming(PerformanceChronometer & chrono) {}

};

inline void MakeSubVector(
	const aVect<double> & X,
	aVect<double> & retVal,
	const aVect<int> & indices,
	double conversionFactor = 1,
	double offset = 0) 
{
	retVal.Redim(indices.Count());
	if (conversionFactor == 1) {
		if (offset) {
			aVect_static_for(retVal, i) retVal[i] = X[indices[i]] + offset;
		}
		else {
			aVect_static_for(retVal, i) retVal[i] = X[indices[i]];
		}
	}
	else {
		if (offset) {
			aVect_static_for(retVal, i) retVal[i] = X[indices[i]] * conversionFactor + offset;
		}
		else {
			aVect_static_for(retVal, i) retVal[i] = X[indices[i]] * conversionFactor;
		}
	}
}

//volatile bool flagGo;
//std::atomic<bool> flagGo(0);

enum PlotFunctionCallbackReturnValue { NEWTONSOLVER_NOREFRESH, NEWTONSOLVER_REFRESH, NEWTONSOLVER_REFRESHASYNC };
enum ExceptionCallbackReturnValue    { NEWTONSOLVER_RETHROW_EXCEPTION, NEWTONSOLVER_SOLVE_FAIL };

struct ValidValueRange {
	int ind;
	double min;
	double max;
};

template <class GlobalParams = EmptyClass, class LocalParams = EmptyClass, class Boundaries = EmptyClass>
class Problem : MagicTarget<Problem<GlobalParams, LocalParams, Boundaries> > {

	enum WorkType { compute_Jacobian, compute_F_X };

	typedef Problem<GlobalParams, LocalParams, Boundaries> Self;
	friend DWORD ProblemThreadProc<Self, GlobalParams>(LPVOID);
	friend bool  ProblemThreadCompute<Self, GlobalParams>(MultiThread_ThreadInfo<Self, GlobalParams> &, Self &, Self *, bool, int, bool);
	friend bool WaitForGo<Self, GlobalParams>(MultiThread_ThreadInfo<Self, GlobalParams> &, Self &);
	friend struct MultiThread_ThreadInfo < Self, GlobalParams > ;
	friend bool  CheckFinite<Self>(Self*, Self*, bool, int, double, int, ptrdiff_t);

	mVect<double> X;
	aVect<double> F_X;
	aVect<double> delta_X;
	aVect<double> grad_L2Norm;
	aVect<aVect<int> > varFuncIndices;
	bool F_X_upToDate;
	bool residual_max_norm_upToDate;
	bool residual_L2_norm_upToDate;
	bool multithreadLoadBalancing;
	PerformanceChronometer lastMultithreadLoadBalancing;
	unsigned long long lastMultithreadLoadBalancing_it;

	GlobalParams * globalParams;

	//must be before MultiThreading, because we need
	//the critical section still valid during
	//MultiThreading destructor
	WinCriticalSection criticalSection;

	struct MultiThreading {

		aVect<LARGE_INTEGER> startTicks, stopTicks, setEventTicks;
		aVect<WorkType> workType;
		int nThreads = 0;
		Self * pMasterProblem;

		struct MultiThreading_Master {
			bool enabled;
			bool initialized;
			aVect<HANDLE> hEventReadyArray;
			aVect<MultiThread_ThreadInfo<Self, GlobalParams> > thread;
			bool checkFinite;
			int failSafe;
			bool allowZeroSparseCoeff;
#ifdef USE_GLOBAL_GO_EVENT
			HANDLE hEventGo[2];
			int currentEventGo;
#endif
			WorkType workType;

			MultiThreading_Master() :
				enabled(true),
				initialized(false),
				checkFinite(false),
				failSafe(0),
				allowZeroSparseCoeff(true),
				workType(compute_Jacobian)
			{
#ifdef USE_GLOBAL_GO_EVENT
				hEventGo[0] = hEventGo[1] = NULL;
				currentEventGo = 0;
#endif
			}

		} master;

		struct MultiThreading_Worker {
			
			bool failure;
			bool exception;
			std::exception_ptr exception_ptr;
			
			MultiThreading_Worker() :
				failure(false), exception_ptr(nullptr), exception(false)
			{}
		} worker;

		MultiThreading(Self * pMasterProblem = nullptr) : pMasterProblem(pMasterProblem)
		{}

	} multiThreading;

	enum class NewtonLoopAction { action_none, action_break, action_continue};

public:

	aVect<char> name;

	bool sparse;
	bool isConverged;
	bool failedNewton;
	bool busy;
	bool reuseJacobian;
	bool allowZeroSparseCoeff;
	int debugMode;
	bool noDebugStep = false;
	bool abortNewton = false;
	bool autoAdjustRelaxFactor = false;
	bool lineSearch = false;
	bool L2_norm_lineSearch = false;
	int noProgressGiveUp = 0;//n\B0 of iterations with no convergence progress required to give up
	unsigned long long iCurrentIterations_minNorm;
	double minNorm;
	bool gradientDescentFallBack = false;

	aVect<aVect<char>> variableNames;

	double residual_max_norm;
	double residual_L2_norm;
	aVect<double> residual_max_norm_history;
	unsigned long long iterationCounter;
	unsigned long long nCurrentIterations;
	unsigned long long maxNewtonRaphsonIterations;
	unsigned long long nSolve;
	unsigned nRetryNewtonRaphson;
	unsigned nConvProblem;
	aVect<ValidValueRange> validValuesRange;
	double relaxFactor;

	struct {
		aVect<double> backup_X;
		double backUpRelaxFactor;
		double minRelaxFactor;
		unsigned long long lastFailedIteration;
		unsigned long long lastFail_nIterations;
		bool linearIncrease_RelaxFactor;
		bool backUpAutoAdjustRelaxFactor;
	} failSafe;

	aVect<char> lastSolveError;

	mVect<FunctionObject<GlobalParams, LocalParams, Boundaries> > functions;
	double convergenceCriterion;

	struct {
		bool firstSolve;
#ifdef _WIN64
		klu_l_symbolic *symbolic;
		klu_l_numeric *numeric;
		klu_l_common common; 
#else
		klu_symbolic *symbolic;
		klu_numeric *numeric;
		klu_common common;
#endif
	} klu;

	struct {
		aMat<double> LU;
		aVect<size_t> P;
	} LUP;

	struct {
		AxisClass profiles, residues, convergence;
	} axes;

public:

	struct {
		aMat<double> full;
		aVect<csi> row;
		aVect<csi> col;
		aVect<double> values;
		int previous_nz;
		cs_sparse * compressed;
		csi * workspace;
		bool initialized;
	} jacobian;

	//MultiThread_Jacobian<Self> multiThread_Jacobian;

	struct  {
		aVect<csi> row;
		aVect<csi> col;
	} oldJacobian;

	struct Problem_Chronometers {
		PerformanceChronometer compute_Jacobian;
		PerformanceChronometer compress_Jacobian;
		PerformanceChronometer analyze;
		PerformanceChronometer factor;
		PerformanceChronometer solve;
		PerformanceChronometer compute_F_X;
		PerformanceChronometer check_Data; 
		PerformanceChronometer check_Convergence;
		PerformanceChronometer apply_Delta_X;
		PerformanceChronometer copy_Data;
		PerformanceChronometer gradDescent;
		PerformanceChronometer total;
	Problem_Chronometers() :
		compute_Jacobian(false),
		compress_Jacobian(false),
		compute_F_X(false), 
		analyze(false),
		factor(false),
		solve(false),
		check_Data(false),
		check_Convergence(false),
		apply_Delta_X(false),
		copy_Data(false),
		gradDescent(false),
		total(false)
		{}
	} chrono;

	struct {
		aVect<double> max_norm, iter;
	} convergenceHistory;

	typedef PlotFunctionCallbackReturnValue (*PlotFuncType)(Problem &, void *);
	typedef void (*IterationCallbackType)(Problem &);
	typedef ExceptionCallbackReturnValue(*ExceptionCallbackType)(Problem &, std::exception_ptr, int failSafe);
	typedef void(*VariableNamesCallbackType)(Problem &, aVect<aVect<char>> &);
	typedef bool(*Apply_Delta_x_FuncType)(mVect<double> & X, const aVect<double> & delta_X, double relaxFactor, const Problem &);

	PlotFuncType plotFunc;
	IterationCallbackType iterationCallback;
	ExceptionCallbackType solveExceptionCallback;
	//VariableNamesCallbackType variableNamesCallback;

	void * plotParam;

	Apply_Delta_x_FuncType apply_delta_x_func;

public:
	MagicCopyDecrementer copyDecrementer;
private:

	template <bool checkFinite, bool failSafe, bool mesureTiming>
	bool Compute_Grad_L2Norm() {

		this->grad_L2Norm.Redim(this->X.Count());

		if (this->grad_L2Norm.Count() >= INT_MAX) MY_ERROR("TODO");

		if (!this->jacobian.initialized) {
			if (!this->ComputeJacobian<checkFinite, failSafe, mesureTiming>()) return false;
			this->CompressJacobian<mesureTiming>();
		}

		if (!this->jacobian.compressed) this->CompressJacobian<mesureTiming>();

		if (!this->F_X_upToDate) if (!this->Compute_F_X<checkFinite, failSafe, mesureTiming>()) return false;

		//if (!this->varFuncIndices) this->InitVarFuncIndices(); to be done by user

		cs_sparse * cprsd = this->jacobian.compressed;

//		double sum2 = 0;
//
//		for (int i = 0, I = (int)this->functions.Count(); i < I; ++i) {
//			sum2 += Sqr(this->F_X[i]);
//		}
//
//		for (int j = 0, J = (int)this->grad_L2Norm.Count(); j < J; ++j) {
//
//
//			auto i_row = cprsd->p[j];
//			auto i_row_lim = cprsd->p[j + 1];
//			auto n = cprsd->p[cprsd->n];
//			auto Ai = cprsd->i;
//
//			double J_ij = 0;
//			double dxj = this->X[j] * 1e-3;
//
//			if (!dxj) dxj = 1e-3;
//
//			double sum1 = sum2;
//
//			//for (int i = 0, I = (int)this->functions.Count(); i < I; ++i) {
//			for (auto&& i : this->varFuncIndices[j]) {
//
//				for (auto k = i_row; k < i_row_lim; k++) {
//					if (i == Ai[k]) {
//						//found
//						J_ij = cprsd->x[k];
//						goto found;
//					}
//				}
//
//				MY_ERROR("not found");
//
//found:
//				sum1 += Sqr(J_ij * dxj) + 2 * J_ij * dxj * this->F_X[i];
//			}
//
//			this->grad_L2Norm[j] = -(sqrt(sum1) - sqrt(sum2)) / dxj;
//
//		}

		for (int j = 0, J = (int)this->grad_L2Norm.Count(); j < J; ++j) {
			auto i_row = cprsd->p[j];
			auto i_row_lim = cprsd->p[j + 1];
			auto n = cprsd->p[cprsd->n];
			auto Ai = cprsd->i;

			double sum = 0;

			for (auto k = i_row; k < i_row_lim; k++) {
				auto i = Ai[k];
				auto J_ij = cprsd->x[k];
				sum += J_ij * this->F_X[i];
			}

			this->grad_L2Norm[j] = sum;

		}

		return true;
	}

	template <bool update_F_X = true>
	double CallFunc(int funcInd) {
		double res = this->functions[funcInd].CallFunc(*this);
		if (update_F_X) this->F_X[funcInd] = res;
		return res;
	}

	double GetResSum(int varInd) {

		auto & functionIndices = this->varFuncIndices[varInd];
		double sum = 0;

		aVect_static_for(functionIndices, i) {
			double res = this->CallFunc(functionIndices[i]);
			sum += res;
		}

		return sum;
	}

	double GetResQuadraSum(int varInd) {

		auto & functionIndices = this->varFuncIndices[varInd];
		double sum = 0;

		aVect_static_for(functionIndices, i) {
			double res = this->CallFunc(functionIndices[i]);
			sum += res*res;
		}

		return sum;
	}

	void DicotoMin(int varInd, double a, double b) {

		while (true) {

			double pt = 0.5 * (a + b);

			double x1 = pt - 0.1 * (b - a);
			double x2 = pt + 0.1 * (b - a);

			this->X[varInd] = x1;
			double resSum1 = this->GetResQuadraSum(varInd);

			this->X[varInd] = x2;
			double resSum2 = this->GetResQuadraSum(varInd);

			if (resSum1 < resSum2) {

				if (abs((b - x2) / b) < 1e-15) {
					return;
				}
				b = x2;
			}
			else {
				if (abs((a - x1) / a) < 1e-15) {
					this->X[varInd] = x1;
					this->GetResQuadraSum(varInd);

					return;
				}
				a = x1;
			}
		}
	}

	void FindMinRes(int varInd) {

		double dx = this->X[varInd] * 1e-3;
		double resSum1 = this->GetResQuadraSum(varInd);

		double prevPt = this->X[varInd];
		double prevPt2 = prevPt;

		while (true) {

			this->X[varInd] += dx;

			double resSum2 = this->GetResQuadraSum(varInd);

			if (resSum2 > resSum1) {
				this->X[varInd] -= 2 * dx;
				resSum2 = this->GetResQuadraSum(varInd);
				if (resSum2 > resSum1) {
					this->DicotoMin(varInd, this->X[varInd], this->X[varInd] + 2 * dx);
					return;
				}
				dx *= -1;
			}

			while (true) {

				prevPt2 = prevPt;
				prevPt = this->X[varInd];

				this->X[varInd] += dx;
				resSum1 = resSum2;
				resSum2 = this->GetResQuadraSum(varInd);

				if (resSum2 > resSum1) {
					this->DicotoMin(varInd, prevPt2, this->X[varInd]);
					return;
				}

				dx *= 1.2;
			}
		}
	}

	csi RedimSparse(cs *A, csi m, csi n, csi nz){
		csi ok, oki, okp, okx;
		A->p = (csi*)cs_realloc(A->p, A->nz >= 0 ? nz : n + 1, sizeof(csi), &okp);
		A->i = (csi*)cs_realloc(A->i, nz, sizeof(csi), &oki);
		A->x = (double*)cs_realloc(A->x, nz, sizeof(double), &okx);
		ok = (oki && okp && okx);
		if (ok) {
			A->m = m;
			A->n = n;
			A->nzmax = nz = CS_MAX(nz, 1);
		}
		return (ok);
	}

	template <bool mesureTiming>
	void CompressJacobian() {

		AutoMesureFunctionTiming<mesureTiming> autoMesure(this->chrono.compress_Jacobian);

		int nz = (int)this->jacobian.values.Count();
		bool mustZeroW = true;

		if (this->jacobian.previous_nz < nz) {
			if (this->jacobian.previous_nz) {
				csi ok1, ok2;
				ok1 = this->RedimSparse(
					this->jacobian.compressed,
					this->functions.Count(),
					this->X.Count(),
					this->jacobian.values.Count());
				this->jacobian.workspace = (csi*)cs_realloc((void*)this->jacobian.workspace, this->X.Count(), sizeof(csi), &ok2);
				if (!ok1 || !ok2) MY_ERROR("memory reallocation failure");
			}
			else {
				this->jacobian.compressed = (cs_sparse*)cs_spalloc(
					this->functions.Count(),
					this->X.Count(),
					this->jacobian.values.Count(),
					(csi)this->jacobian.values.GetDataPtr(),
					0);
				this->jacobian.workspace = (csi*)cs_calloc(this->X.Count(), sizeof(csi));
				if (!this->jacobian.compressed || !this->jacobian.workspace) MY_ERROR("memory allocation failure");
				mustZeroW = false;
			}
		}

		csi *      Ci = this->jacobian.compressed->i;
		csi *      Cp = this->jacobian.compressed->p;
		double *   Cx = this->jacobian.compressed->x;
		csi *      w  = this->jacobian.workspace;
		csi *      Ti = this->jacobian.row;
		csi *      Tj = this->jacobian.col;
		double *   Tx = this->jacobian.values;

		csi p;

		if (mustZeroW) memset(w, 0, this->X.Count() * sizeof(csi));

		for (int k = 0; k < nz; k++) w[Tj[k]]++;       /* column counts */
		cs_cumsum(Cp, w, this->X.Count());                /* column pointers */
		for (int k = 0; k < nz; k++)
		{
			Ci[p = w[Tj[k]]++] = Ti[k];    /* A(i,j) is the pth entry in C */
			if (Cx) Cx[p] = Tx[k];
		}

		this->jacobian.previous_nz = (int)this->jacobian.values.Count();
	}

	void WaitForAllThreadsReady() {

		QueryPerformanceCounter(&this->multiThreading.stopTicks.Grow(1).Last());

		DWORD result = WaitForMultipleObjects(
			(DWORD)this->multiThreading.master.hEventReadyArray.Count(),
			this->multiThreading.master.hEventReadyArray,
			true,
			INFINITE);

		if (result == WAIT_ABANDONED || result == WAIT_FAILED) {
			MY_ERROR("WaitForMultipleObjects failed");
		}

		QueryPerformanceCounter(&this->multiThreading.startTicks.Grow(1).Last());
	}

	void InitializeMultiThreadComputation() {
		
		auto nThreads = this->multiThreading.nThreads;

		if (!nThreads) {
			SYSTEM_INFO si;
			GetSystemInfo(&si); 
			nThreads = si.dwNumberOfProcessors;
		}

		if (nThreads > (int)this->functions.Count()) nThreads = (int)this->functions.Count();

		this->multiThreading.master.thread.Reserve(nThreads);
		this->multiThreading.master.hEventReadyArray.Redim(nThreads);

#ifdef USE_GLOBAL_GO_EVENT
		this->multiThreading.master.hEventGo[0] = SAFE_CALL(CreateEvent(NULL, true, false, NULL));
		this->multiThreading.master.hEventGo[1] = SAFE_CALL(CreateEvent(NULL, true, false, NULL));
#endif

		int n = (int)this->functions.Count();
		int n_per_thread = n / nThreads;
		int n_remainder = n % nThreads;
		int i1 = 0;
		int i2 = n_per_thread;

		for (int i = 0; i < nThreads; ++i) {

			if (n_remainder > 0) n_remainder--, i2++;

			this->multiThreading.master.thread.Push(this, i1, i2, i);
			this->multiThreading.master.hEventReadyArray[i]  = this->multiThreading.master.thread.Last().hEventReady;

			i1 = i2;
			i2 = i1 + n_per_thread;
		}

		//this->WaitForAllThreadsReady();
		this->multiThreading.master.initialized = true;
	}

	void BalanceMultiThreadComputation() {

		if (this->multithreadLoadBalancing) {

			if (this->lastMultithreadLoadBalancing.State() == PerformanceChronometer::unstarted ||
				this->lastMultithreadLoadBalancing_it == 0)
			{
				this->lastMultithreadLoadBalancing.Start();
				this->lastMultithreadLoadBalancing_it = this->iterationCounter;
			}
			else if (this->lastMultithreadLoadBalancing.GetSeconds() > 0.5 &&
				this->iterationCounter - this->lastMultithreadLoadBalancing_it > 20)
			{
				aVect<double> threadsLoad;
				this->PlotMultiThreadingTiming<false>(nullptr, nullptr, lastMultithreadLoadBalancing.GetStartTick(), &threadsLoad);

				auto I = (int)this->X.Count();
				aVect<double> funcLoad(I);
				auto&& threads = this->multiThreading.master.thread;

				aVect_static_for(threads, i) {
					auto i1 = threads[i].i1;
					auto i2 = threads[i].i2;
					for (int j = i1; j < i2; ++j) {
						funcLoad[j] = threadsLoad[i] / (i2 - i1);
					}
				}

				double sum = 0;

				for (auto&& l : funcLoad) sum += l;

				double loadPerThread = sum / threadsLoad.Count();

				sum = 0;
				int iCurrentThread = 0;

				for (int i = 0; i < I - 1; ++i) {
					sum += funcLoad[i];
					if (sum >= loadPerThread) {
						threads[iCurrentThread].i2 = i;

						//DEBUG
						if (threads[iCurrentThread].i2 < threads[iCurrentThread].i1) MY_ERROR("gotcha");

						iCurrentThread++;
						threads[iCurrentThread].i1 = i;
						sum -= loadPerThread;
					}
				}

				if (iCurrentThread != threadsLoad.Count() - 1) MY_ERROR("gotcha");

				//DEBUG
				for (auto&& t1 : threads) {
					if (t1.i2 < t1.i1) MY_ERROR("gotcha");
					for (auto&& t2 : threads) {
						if (&t1 == &t2) continue;
						if (t1.i1 > t2.i1) {
							if (!(t1.i1 >= t2.i2)) MY_ERROR("gotcha");
						}
						if (t1.i1 < t2.i1) {
							if (!(t1.i2 <= t2.i1)) MY_ERROR("gotcha");
						}
					}
				}

				if (false) {
					aVect<char> loads;

					aVect_static_for(threads, i) {
						auto i1 = threads[i].i1;
						auto i2 = threads[i].i2;

						//auto & ref = this->multiThreading.master.thread[i].pWorkerProblem;

						if (loads) loads.Append(", ");
						loads.AppendFormat("%s%%", NiceFloat(100 * (i2 - i1) / (double)I, 1e-3));
					}

					xPrintf("Threads load redistribution: (%s)\n", loads);
				}

				lastMultithreadLoadBalancing.Restart();
				this->lastMultithreadLoadBalancing_it = this->iterationCounter;
			}
		}

		if (this->multiThreading.startTicks.Count() > 100000) {

			auto Func = [](auto&& tickVect) {
				if (tickVect) {
					tickVect.First() = tickVect.Last();
					tickVect.Redim(1);
				}
			};

			this->multiThreading.setEventTicks.Redim(0);
			Func(this->multiThreading.startTicks);
			Func(this->multiThreading.stopTicks);

			for (auto&& t : this->multiThreading.master.thread) {
				t.pWorkerProblem->multiThreading.setEventTicks.Redim(0);
				t.pWorkerProblem->multiThreading.startTicks.Redim(0);
			}
		}
	}

	void LaunchMultiThreadComputation() {

#ifdef USE_GLOBAL_GO_EVENT
		SAFE_CALL(ResetEvent(this->multiThreading.master.hEventGo[1 - this->multiThreading.master.currentEventGo]));
		QueryPerformanceCounter(&this->multiThreading.setEventTicks.Grow(1).Last());
		SAFE_CALL(SetEvent(this->multiThreading.master.hEventGo[this->multiThreading.master.currentEventGo]));
		this->multiThreading.master.currentEventGo = 1 - this->multiThreading.master.currentEventGo;
#else
		aVect_static_for(this->multiThreading.master.thread, i) {
			QueryPerformanceCounter(&this->multiThreading.setEventTicks.Grow(1).Last());
			SAFE_CALL(SetEvent(this->multiThreading.master.thread[i].hEventGo));
		}
#endif
	}

	void CombineThreadsDerivative() {

		aVect_static_for(this->multiThreading.master.thread, i) {
			auto & jacobian_i = this->multiThreading.master.thread[i].pWorkerProblem->jacobian;

			this->jacobian.col.Append(jacobian_i.col);
			this->jacobian.row.Append(jacobian_i.row);
			this->jacobian.values.Append(jacobian_i.values);
		}
	}

	bool CheckMultiThreadSuccess() {

		aVect_static_for(this->multiThreading.master.thread, i) {
			auto&& workerInfo = this->multiThreading.master.thread[i].pWorkerProblem->multiThreading.worker;
			if (workerInfo.failure) {
				if (workerInfo.exception) {
					if (workerInfo.exception_ptr) {
						std::rethrow_exception(workerInfo.exception_ptr);
					}
					else {
						MY_ERROR("???");
					}
				}
				return false;
			}
		}

		return true;
	}
	
	template <class T>
	char * KluErrorDescription(T status) {
		switch (status) {
			case KLU_SINGULAR: return "matrix is singular";
			case KLU_OUT_OF_MEMORY: return "out of memory";
			case KLU_INVALID: return "invalid parameter";
			case KLU_TOO_LARGE: return "matrix too large (integer overflow has occured)";
		}
		return "unknown status";
	}

	template <bool errorIfSingular = true, bool reuseLUP = false>
	bool LUP_Solve(aVect<double> & x, const aMat<double> & A, const aVect<double> & b) {

		if (/*false && */A.nRow() == 2 && A.nCol() == 2) {
			x.Redim(2);

			//auto det = A(0, 0) * A(1, 1) - A(1, 0) * A(0, 1);
			auto det = A[0] * A[3] - A[1] * A[2];

			if (!det) {
				if (errorIfSingular) {
					SpyMatrix(A, "Singular jacobian");
					DisplayMatrix(A);
					MY_ERROR("matrix is singular");
				}
				else return false;
			}

			//x[0] = (b[0] * A(1, 1) - b[1] * A(0, 1)) / det;
			//x[1] = (b[1] * A(0, 0) - b[0] * A(1, 0)) / det;

			x[0] = (b[0] * A[3] - b[1] * A[1]) / det;
			x[1] = (b[1] * A[0] - b[0] * A[2]) / det;

			return true;
		}

		if (!reuseLUP) {
			if (A.nRow() != A.nCol()) MY_ERROR("matrix must be square");

			this->LUP.LU.Redim(A.nRow(), A.nCol());
			memcpy(this->LUP.LU.GetDataPtr(), A.GetDataPtr(), this->LUP.LU.Count() * sizeof(double));

			if (!LUP_Decomposition<false>(this->LUP.LU, this->LUP.P)) {
				if (errorIfSingular) {
					SpyMatrix(this->LUP.LU, "Singular jacobian");
					DisplayMatrix(this->LUP.LU);
					MY_ERROR("matrix is singular");
				}
				else return false;
			}
		}

		LUP_Substitute(x, this->LUP.LU, this->LUP.P, b);

		return true;
	}

	template <
		bool mesureTiming = false, 
		bool reuseJacobian = false, 
		bool checkFinite = false,
		int failSafe = false>
	bool ComputeDelta_X() {

		try {
			if (!reuseJacobian || !this->jacobian.initialized) {

#ifdef NEWTONSOLVER_BUGHUNT_1
				_CrtCheckMemory();
#endif

				if (!this->ComputeJacobian<checkFinite, failSafe, mesureTiming>()) return false;

#ifdef NEWTONSOLVER_BUGHUNT_1
				_CrtCheckMemory();
#endif

			}

			if (!this->F_X_upToDate) if (!this->Compute_F_X<checkFinite, failSafe, mesureTiming>()) return false;

#ifdef NEWTONSOLVER_BUGHUNT_1
			_CrtCheckMemory();
#endif

			auto FailFunc = [this](auto&& str, auto dbgLvl) {this->SetLastSolveError(str, dbgLvl); return false; };

			if (this->sparse) {
				if (!reuseJacobian || this->klu.firstSolve) {

#ifdef NEWTONSOLVER_BUGHUNT_1
					_CrtCheckMemory();
#endif

					this->CompressJacobian<mesureTiming>();

#ifdef NEWTONSOLVER_BUGHUNT_1
					_CrtCheckMemory();
#endif

					if (this->klu.firstSolve) {
						if (mesureTiming) this->chrono.analyze.Resume();
#ifdef _WIN64
						if (this->klu.symbolic) klu_l_free_symbolic(&this->klu.symbolic, &this->klu.common);
						this->klu.symbolic = klu_l_analyze(this->X.Count(), this->jacobian.compressed->p, this->jacobian.compressed->i, &this->klu.common);
#else
						if (this->klu.symbolic) klu_free_symbolic(&this->klu.symbolic, &this->klu.common);
						this->klu.symbolic = klu_analyze(this->X.Count(), this->jacobian.compressed->p, this->jacobian.compressed->i, &this->klu.common);
#endif

#ifdef NEWTONSOLVER_BUGHUNT_1
						_CrtCheckMemory();
#endif

						if (mesureTiming) this->chrono.analyze.Stop();
						if (!this->klu.symbolic) this->KluError("klu_analyze");
						if (!this->CheckNotSingular<failSafe>()) return false;
						if (mesureTiming) this->chrono.factor.Resume();
#ifdef _WIN64
						if (this->klu.numeric) klu_l_free_numeric(&this->klu.numeric, &this->klu.common);
						this->klu.numeric = klu_l_factor(this->jacobian.compressed->p, this->jacobian.compressed->i, this->jacobian.compressed->x, this->klu.symbolic, &this->klu.common);
#else
						if (this->klu.numeric) klu_free_numeric(&this->klu.numeric, &this->klu.common);
						this->klu.numeric = klu_factor(this->jacobian.compressed->p, this->jacobian.compressed->i, this->jacobian.compressed->x, this->klu.symbolic, &this->klu.common);
#endif

#ifdef NEWTONSOLVER_BUGHUNT_1
						_CrtCheckMemory();
#endif

						if (mesureTiming) this->chrono.factor.Stop();
						if (!this->CheckNotSingular<failSafe>()) return false;
						if (!this->klu.numeric) if (failSafe) return FailFunc("klu_factor", 0); else this->KluError("klu_factor");
						this->klu.firstSolve = false;

#ifdef NEWTONSOLVER_BUGHUNT_1
						_CrtCheckMemory();
#endif
					}
					else {
						if (mesureTiming) this->chrono.factor.Resume();

#ifdef NEWTONSOLVER_BUGHUNT_1
						_CrtCheckMemory();
						if (!My_klu_valid(this->jacobian.compressed->n, this->jacobian.compressed->p, this->jacobian.compressed->i, this->jacobian.compressed->x)) MY_ERROR("assertion failed");
#endif

#ifdef _WIN64
						auto success = klu_l_refactor(this->jacobian.compressed->p, this->jacobian.compressed->i, this->jacobian.compressed->x, this->klu.symbolic, this->klu.numeric, &this->klu.common);
#else
						auto success = klu_refactor(this->jacobian.compressed->p, this->jacobian.compressed->i, this->jacobian.compressed->x, this->klu.symbolic, this->klu.numeric, &this->klu.common);
#endif

#ifdef NEWTONSOLVER_BUGHUNT_1
						_CrtCheckMemory();
#endif

						if (mesureTiming) this->chrono.factor.Stop();
						if (!this->CheckNotSingular<failSafe>()) return false;
						if (!success) if (failSafe) return FailFunc("klu_refactor", 0); else this->KluError("klu_refactor");
					}
				}

#ifdef NEWTONSOLVER_BUGHUNT_1
				_CrtCheckMemory();
#endif

				if (mesureTiming) this->chrono.copy_Data.Resume();
				this->delta_X.Copy(this->F_X);
				if (mesureTiming) this->chrono.copy_Data.Stop();

#ifdef NEWTONSOLVER_BUGHUNT_1
				_CrtCheckMemory();
#endif

				if (mesureTiming) this->chrono.solve.Resume();
#ifdef _WIN64
				auto success = klu_l_solve(this->klu.symbolic, this->klu.numeric, this->X.Count(), 1, this->delta_X, &this->klu.common);
#else
				auto success = klu_solve(this->klu.symbolic, this->klu.numeric, this->X.Count(), 1, this->delta_X, &this->klu.common);
#endif

#ifdef NEWTONSOLVER_BUGHUNT_1
				_CrtCheckMemory();
#endif

				if (mesureTiming) this->chrono.solve.Stop();
				if (!this->CheckNotSingular<failSafe>()) return false;
				if (!success) {
					auto func = [this]() {return xFormat("klu_solve failed (%s)", KluErrorDescription(this->klu.common.status)); };
					if (failSafe) {
						this->SetLastSolveError(func, 1);
						return false;
					} else {
						MY_ERROR(func());
					}
					
				}

#ifdef NEWTONSOLVER_BUGHUNT_1
				_CrtCheckMemory();
#endif

			}
			else {
				if (!this->LUP_Solve<true, reuseJacobian>(this->delta_X, this->jacobian.full, this->F_X)) {
					if (failSafe) return false;
					else {
						this->SpyJacobian();
						MY_ERROR("singular jacobian");
					}
				}
			}

#ifdef NEWTONSOLVER_BUGHUNT_1
			_CrtCheckMemory();
#endif
			if (checkFinite || failSafe) {
				if (mesureTiming) this->chrono.check_Data.Resume();
				aVect_static_for(this->delta_X, i) if (!_finite(this->delta_X[i])) {
					auto func = [this, i]() {return xFormat("delta X not finite: variable \"%s\"", this->variableNames[i]); };
					if (failSafe) {
						this->SetLastSolveError(func, 1);
						return false;
					} else {
						MY_ERROR(func());
					}
				}
				if (mesureTiming) this->chrono.check_Data.Stop();
			}

#ifdef NEWTONSOLVER_BUGHUNT_1
			_CrtCheckMemory();
#endif
		}
		catch (...) {
			return this->SolveExceptionHandler(std::current_exception(), failSafe);
		}

		return true;
	}

	bool SolveExceptionHandler(std::exception_ptr eptr, int failSafe) {

		if (this->solveExceptionCallback) {
			auto retVal = this->solveExceptionCallback(*this, eptr, failSafe);
			if (retVal == NEWTONSOLVER_SOLVE_FAIL) return false;
			if (retVal == NEWTONSOLVER_RETHROW_EXCEPTION) std::rethrow_exception(eptr);
			MY_ERROR("Wrong exception callback return value");
		}
		else {
			std::rethrow_exception(eptr);
		}
		return false; //unreachable code
	}

	void ResetKlu() {
		
#ifdef NEWTONSOLVER_BUGHUNT_1
		_CrtCheckMemory();
#endif

		if (this->klu.symbolic) {
#ifdef _WIN64
			klu_l_free_symbolic(&this->klu.symbolic, &this->klu.common);
#else
			klu_free_symbolic(&this->klu.symbolic, &this->klu.common); 
#endif
		}

#ifdef NEWTONSOLVER_BUGHUNT_1
		_CrtCheckMemory();
#endif

		if (this->klu.numeric) {
#ifdef _WIN64
			klu_l_free_numeric(&this->klu.numeric, &this->klu.common);
#else
			klu_free_numeric(&this->klu.numeric, &this->klu.common);
#endif

		}

#ifdef NEWTONSOLVER_BUGHUNT_1
		_CrtCheckMemory();
#endif

		this->klu.firstSolve = true;
	}

	template <bool mesureTiming, bool isRecursion = false>
	bool ApplyDelta_X(OutputInfo & outputInfo, bool gradientDescentFallbackRecursion = false/*, bool L2_norm_lineSearch = false, bool gradientDescentFallback = true*/) {
		
		static WinCriticalSection cs;

		Critical_Section(cs) {

			if (!isRecursion && this->lineSearch) {
				//if (this->autoAdjustRelaxFactor) MY_ERROR("options autoAdjustRelaxFactor and lineSearch are not compatible");

				auto gammaMin = std::numeric_limits<double>::infinity();
				auto resNormMin = std::numeric_limits<double>::infinity();
				int iMin;

				static aVect<double> F_X_min, X_min, X0;

				//auto L2_norm_lineSearch = true;
				//auto L2_norm_lineSearch = false;

				auto&& res = this->L2_norm_lineSearch ? this->residual_L2_norm : this->residual_max_norm;
				auto&& upToDate = this->L2_norm_lineSearch ? this->residual_L2_norm_upToDate : this->residual_max_norm_upToDate;

				bool (Self::* CheckConv_L2)(bool)  = &Self::CheckConvergence<false, mesureTiming, true>;
				bool (Self::* CheckConv_Max)(bool) = &Self::CheckConvergence<true, mesureTiming, false>;

				bool (Self::* CheckConv)(bool) = this->L2_norm_lineSearch ? CheckConv_L2 : CheckConv_Max;

				//CheckConv = L2_norm_lineSearch ? this->CheckConvergence<false, mesureTiming, true> : this->CheckConvergence<true, mesureTiming, false>;

				if (!this->F_X_upToDate) MY_ERROR("gotcha!");
				if (!upToDate) (this->*CheckConv)(false);
				auto residual_norm_0 = res;

				F_X_min.Redim(0);
				X_min.Redim(0);
				X0 = this->X.ToAvect();

				////[DEBUG]
				//struct DoAtExit {

				//	mVect<double> * pX;
				//	aVect<double> * pX0;
				//	unsigned long long itCount;

				//	DoAtExit(mVect<double> * pX, aVect<double> * pX0, unsigned long long itCount) : pX(pX), pX0(pX0), itCount(itCount) {}

				//	~DoAtExit() {

				//		{
				//			static aVect<double> iterations, delta_X_norm, DX;
				//			static AxisClass axis;

				//			auto&& X = *this->pX;
				//			auto&& X0 = *this->pX0;

				//			DX.Redim(X.Count());

				//			aVect_static_for(DX, i) {
				//				DX[i] = X[i] - X0[i];
				//			}

				//			iterations.Push((double)this->itCount);
				//			delta_X_norm.Push(L2_Norm(DX));

				//			if (!axis) axis.Create().SetTitle("delta_X_norm");

				//			if (axis.Exist()) axis.ClearSeries().AddSerie("delta_X_norm", iterations, delta_X_norm).AutoFit().Refresh();

				//		}
				//	}

				//};

				//DoAtExit doAtExit(&this->X, &X0, this->iterationCounter);
				////[\DEBUG]


				//{//[DEBUG]
				//	if (!this->Compute_F_X<true, true, mesureTiming>())    MY_ERROR("??");
				//	CheckConv(true);
				//	if (res != residual_norm_0) MY_ERROR("??");
				//}//[\DEBUG]

				//int nIter =
				//	this->nRetryNewtonRaphson == 0 ? 10 :
				//	this->nRetryNewtonRaphson == 1 ? 15 :
				//	this->nRetryNewtonRaphson == 2 ? 20 :
				//	this->nRetryNewtonRaphson == 3 ? 25 :
				//	this->nRetryNewtonRaphson == 4 ? 30 :
				//	this->nRetryNewtonRaphson == 5 ? 35 :
				//	this->nRetryNewtonRaphson == 6 ? 40 :
				//	this->nRetryNewtonRaphson == 7 ? 45 :
				//	this->nRetryNewtonRaphson == 8 ? 50 : 
				//	this->nRetryNewtonRaphson == 9 ? 60 : 100;

				int nIter =
					this->nRetryNewtonRaphson == 0 ? 20 :
					this->nRetryNewtonRaphson == 1 ? 30 :
					this->nRetryNewtonRaphson == 2 ? 40 :
					this->nRetryNewtonRaphson == 3 ? 50 :
					this->nRetryNewtonRaphson == 4 ? 60 :
					this->nRetryNewtonRaphson == 5 ? 70 : 70/*
					this->nRetryNewtonRaphson == 6 ? 80 :
					this->nRetryNewtonRaphson == 7 ? 90 :
					this->nRetryNewtonRaphson == 8 ? 100 : 120*/;

				if (this->nRetryNewtonRaphson > 1) {
					int a = 0;
				}

				//for (int k = 0; k < 2; k++) {
				for (int k = 0; k < 1; k++) {

					auto iStart = k == 0 ? 0 : 20;
					auto iStop = k == 0 ? nIter : 15;
					auto iDir = k == 0 ? 1 : -1;

					auto cond = [](int k, int i, int iStop) { return k == 0 ? i < iStop : i > iStop; };

					if (k == 1) {
						if (res < residual_norm_0 || this->nRetryNewtonRaphson <= 2) {
							break;
						}
						else {
							if (this->debugMode > 0) outputInfo.printf("Trying negative delta X...\n");
						}
					}

					for (auto i = iStart; cond(k, i, iStop); i += iDir) {

						//auto gamma = iDir * pow(2, 0.5*(1 - i));
						auto gamma = iDir * pow(3, 0.9*(1 - i));

						//double r = (double)rand() / RAND_MAX;
						//gamma *= (r + 9) / 10;

						if (this->nRetryNewtonRaphson > 0 && gamma > this->relaxFactor) continue;

						auto relaxFactorOrig = this->relaxFactor;
						this->relaxFactor = gamma;

						auto ok = this->ApplyDelta_X<mesureTiming, true>(outputInfo);
						this->relaxFactor = relaxFactorOrig;

						if (ok) ok = this->Compute_F_X<true, true, mesureTiming>();

						if (ok) {
							//this->Compute_F_X<true, true, mesureTiming>();
							if ((this->*CheckConv)(true)) {
								if (this->debugMode > 1) outputInfo.printf("Line Search conv ok: using gamma = %g, (i=%d)\n", gamma, i);
								return true;
							}

							if (res < resNormMin) {

								if (res < residual_norm_0 ||
									res > 1.01 * residual_norm_0 ||
									resNormMin > 1.5 * residual_norm_0)
								{
									//if (this->nRetryNewtonRaphson == 0 || (residual_norm_0 - res) / residual_norm_0 < )
									resNormMin = res;
									gammaMin = gamma;
									F_X_min = this->F_X;
									X_min = this->X.ToAvect();
									iMin = i;

									if (k == 1 && i == iStop - 1 && res < residual_norm_0) {
										if (iStop < nIter + 5) iStop++;
									}
								}
							}
						}

						this->X = X0;
						this->Invalidate_F_X();

						if (this->abortNewton) {
							outputInfo.printf("Newton aborted by user request\n");
							return false;
						}

						//{//[DEBUG]
						//	if (!this->Compute_F_X<true, true, mesureTiming>())    MY_ERROR("??");
						//	CheckConv(true);
						//	if (res != residual_norm_0) MY_ERROR("??");
						//}//[\DEBUG]
					}
				}

				if (F_X_min) {

					if (this->debugMode > 1) outputInfo.printf("Line Search: using gamma = %g, (i=%d)\n", gammaMin, iMin);

					//if (this->relaxFactor == 1 || gammaMin < this->relaxFactor) {
					res = resNormMin;
					this->F_X = F_X_min;
					this->X = X_min;
					//}
					//else {
					//	this->X = X0;
					//	auto oldValue = this->relaxFactor;
					//	this->relaxFactor *= gammaMin;
					//	auto ok = this->ApplyDelta_X<mesureTiming, true>();
					//	this->relaxFactor = oldValue;
					//	if (!ok) {
					//		res = resNormMin;
					//		this->F_X = F_X_min;
					//		this->X = X_min;
					//	}
					//}

					if (!gradientDescentFallbackRecursion && this->gradientDescentFallBack && this->nRetryNewtonRaphson > 0 && (1 - 1e-6) * residual_norm_0 < resNormMin) {
						if (this->Compute_Grad_L2Norm<true, true, mesureTiming>()) {

							outputInfo.printf("Gradient Descent Fallback...\n");

							aVect_static_for(this->delta_X, i) this->delta_X[i] = 1e-5 * this->grad_L2Norm[i];

							if (!this->ApplyDelta_X<mesureTiming, false>(outputInfo, true)) return false;
						}
					}

					if (false && this->nRetryNewtonRaphson > 0) {

						double limFactor =
							this->nRetryNewtonRaphson == 1 ? 0.005 :
							this->nRetryNewtonRaphson == 2 ? 0.001 :
							this->nRetryNewtonRaphson == 3 ? 0.0005 :
							this->nRetryNewtonRaphson == 4 ? 0.0002 : 0.0001;

						auto X_Norm = L2_Norm(X0);

						auto DX = this->delta_X;

						for (auto&& v : DX) v *= gammaMin;

						auto DX_Norm = L2_Norm(DX);

						if (DX_Norm / X_Norm > limFactor) {
							auto correction = limFactor / (DX_Norm / X_Norm);

							if (this->debugMode > 1) outputInfo.printf("Limiting DX norm: correction = %g\n", correction);

							this->X = X0;
							this->Invalidate_F_X();

							auto oldValue = this->relaxFactor;
							this->relaxFactor = gammaMin * correction;
							auto ok = this->ApplyDelta_X<mesureTiming, true>(outputInfo);
							this->relaxFactor = oldValue;

							if (!ok) {
								res = resNormMin;
								this->F_X = F_X_min;
								this->X = X_min;
							}
						}

					}

					if (false && this->nRetryNewtonRaphson > 0) {

						double maxVar =
							this->nRetryNewtonRaphson == 1 ? 0.2  :
							this->nRetryNewtonRaphson == 2 ? 0.1  :
							this->nRetryNewtonRaphson == 3 ? 0.05 :
							this->nRetryNewtonRaphson == 4 ? 0.02 : 0.01;

						for (double fact = 0.5; abs(log(residual_norm_0) - log(resNormMin)) > maxVar; fact *= 0.5) {

							if (this->debugMode > 1) outputInfo.printf("slowing down convergence...\n");

							this->X = X0;
							this->Invalidate_F_X();

							//{//[DEBUG]
							//	if (!this->Compute_F_X<true, true, mesureTiming>())    MY_ERROR("??");
							//	CheckConv(true);
							//	if (res != residual_norm_0) MY_ERROR("??");
							//}//[\DEBUG]
							

							auto oldValue = this->relaxFactor;
							this->relaxFactor = fact * gammaMin;
							auto ok = this->ApplyDelta_X<mesureTiming, true>(outputInfo);
							this->relaxFactor = oldValue;

							if (ok) ok = this->Compute_F_X<true, true, mesureTiming>();

							if (ok) if ((this->*CheckConv)(true)) {
								if (this->debugMode > 1) outputInfo.printf("Line Search conv ok: using gamma = %g, (i=%d)\n", this->relaxFactor, iMin);
								return true;
							}

							resNormMin = res;

							if (!ok) {
								res = resNormMin;
								this->F_X = F_X_min;
								this->X = X_min;
								break;
							}

						}
					}
				}
				else {
					if (this->debugMode > 0) outputInfo.printf("Line Search: fail\n");
					return false;
					//MY_ERROR("Line search failed");
				}

				return true;
			}

			//if (!isRecursion) MY_ERROR("gotcha");

			AutoMesureFunctionTiming<mesureTiming> autoMesure(this->chrono.apply_Delta_X);

			//this->F_X_upToDate = false;
			//this->residual_max_norm_upToDate = false;
			this->Invalidate_F_X();

			double * X = this->X;

			if (this->apply_delta_x_func) {
				return this->apply_delta_x_func(this->X, this->delta_X, this->relaxFactor, *this);
			}
			else {
				double * delta_X = this->delta_X;
				double relaxFactor = this->relaxFactor;

				if (relaxFactor != 1) {
					aVect_static_for(this->X, i) X[i] -= relaxFactor * delta_X[i];
				}
				else aVect_static_for(this->X, i) X[i] -= delta_X[i];
			}

			if (mesureTiming) this->chrono.check_Data.Resume();
			aVect_static_for(this->X, i) {
				double & val = this->X[i];
				if (std::fpclassify(val) == FP_SUBNORMAL)
					val = 0;
			}
			if (mesureTiming) this->chrono.check_Data.Stop();

			if (this->validValuesRange) {
				auto validValuesRange = this->validValuesRange.GetDataPtr();
				aVect_static_for(this->validValuesRange, i) {
					auto ind = validValuesRange[i].ind;
					auto min = validValuesRange[i].min;
					auto max = validValuesRange[i].max;
					if (X[ind] < min || X[ind] > max) {

						if (this->debugMode > 1) {
							aVect<char> msg;

							if (this->variableNames) {
								outputInfo.printf(xFormat("Value out of valid range: %s = %g\n", this->variableNames[ind], X[ind]));
							} else {
								outputInfo.printf("Value out of valid range: X[%d] = %g\n", ind, X[ind]);
							}
						}

						if (this->debugMode) {
							this->Plot();
							if (this->debugMode > 2 && !this->noDebugStep) {
								this->ConsolePrompt(xFormat("Debug mode = %d > 1: plot and step", this->debugMode));
							}
						}
						return false;
					}
				}
			}
		}
		
		return true;
	}

	static void ResiduePlotStatusTextCleanUp(AxisHandle axis, void * callbacksData) {
		////auto ptr = (void**)callbacksData;
		////xVect_Free(ptr);

		//auto ptr = (char**)callbacksData;

		//xVect_static_for(ptr, i) {
		//	xVect_Free(ptr[i]);
		//}

		auto ptr = (char (*)[80])callbacksData;

		xVect_Free(ptr);
	}
	 
	template <bool userAxisGiven>
	void PlotResidueVectorCore(AxisClass & userAxis) {

		//static aVect<double> ramp;
		aVect<double> ramp;

		AxisClass & axis = userAxisGiven ? *(AxisClass*)&userAxis : *(AxisClass*)&this->axes.residues;

		if (!axis) axis.Create();

		int N = (int)this->GetResidue().Count();
		if (ramp.Count() != N) {
			ramp.Redim(N);
			aVect_static_for(ramp, i) ramp[i] = (double)i;
		}

		//void ** functions = nullptr;
		char (*functions)[80] = nullptr;

		if (xVect_Count(functions) != N || axis.GetStatusTextCallback() != ResiduePlotStatusText) {

			xVect_Redim(functions, N);
			
			mVect_static_for(this->functions, i) {

				if (!this->functions[i].name) {
					this->functions[i].name = GetFuncName(i, false);
				}

				//functions[i] = xVect_Create<char>(this->functions[i].name.Count());
				//memcpy(functions[i], this->functions[i].name, sizeof(char)*xVect_Count(functions[i]));

				if (auto count = Min(sizeof(functions[i]), this->functions[i].name.Count())) {
					memcpy(functions[i], this->functions[i].name, count);
					if (this->functions[i].name.Count() > sizeof(functions[i])) {
						functions[i][NUMEL(functions[i]) - 2] = '.';
						functions[i][NUMEL(functions[i]) - 3] = '.';
						functions[i][NUMEL(functions[i]) - 4] = '.';
					}
					functions[i][NUMEL(functions[i]) - 1] = 0;
				} else {
					functions[i][0] = 0;
				}
				
			}
			//xVect_static_for(functions, i) functions[i] = *(void**)(&this->functions[i].function);

			axis.SetCallbacksData(functions);
			axis.SetCallbacksCleanUpCallback(ResiduePlotStatusTextCleanUp);
			axis.SetStatusTextCallback(ResiduePlotStatusText);
		}

		axis.ClearSeries();

		if (!this->F_X_upToDate) {
			if (this->Compute_F_X<false, false, false>()) {
				axis.AddSerie(ramp, this->GetResidue());
			}
		}
		else {
			axis.AddSerie(ramp, this->GetResidue());
		}		

		axis.SetTitle(L"Residue").AutoFit().RefreshAsync();
	}

public:

	Problem() : Problem(0, 0) {}

	Problem(int n) : Problem(n, n) {}

	Problem(int nVar, int nFunc) :
		iterationCounter(0),
		X(nVar),
		F_X(nFunc),
		delta_X(nVar),
		varFuncIndices(nVar),
		functions(nFunc),
		convergenceCriterion(1e-6),
		F_X_upToDate(false),
		residual_max_norm_upToDate(false),
		residual_L2_norm_upToDate(false),
		multithreadLoadBalancing(false),
		lastMultithreadLoadBalancing(false),
		lastMultithreadLoadBalancing_it(0),
		sparse(true),
		isConverged(false),
		allowZeroSparseCoeff(true),
		maxNewtonRaphsonIterations(1000),
		nSolve(0),
		residual_max_norm(std::numeric_limits<double>::infinity()),
		residual_L2_norm(std::numeric_limits<double>::infinity()),
		plotFunc(nullptr),
		iterationCallback(nullptr),
		solveExceptionCallback(nullptr),
		//variableNamesCallback(nullptr),
		plotParam(nullptr),
		busy(false),
		reuseJacobian(false),
		apply_delta_x_func(nullptr),
		relaxFactor(1),
		nRetryNewtonRaphson(0),
		nConvProblem(0),
		failedNewton(false),
		debugMode(false)
	{
		this->globalParams = new GlobalParams;

		//_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
		_controlfp(_DN_FLUSH, _MCW_DN);

		this->F_X.Set(std::numeric_limits<double>::infinity());
		this->X.Set(std::numeric_limits<double>::infinity());
		this->jacobian.compressed = nullptr;
		this->jacobian.workspace = nullptr;
		this->jacobian.previous_nz = 0;
		this->jacobian.initialized = false;
		this->klu.firstSolve = true;
		this->klu.symbolic = nullptr;
		this->klu.numeric = nullptr;
#ifdef _WIN64
		klu_l_defaults(&this->klu.common);
#else
		klu_defaults(&this->klu.common);
#endif

		this->failSafe.lastFailedIteration = 0;
		this->failSafe.lastFail_nIterations = 0;

		//flagGo = false;
		//SetCriticalSectionSpinCount(g_cs, 400);
		//this->klu.common.btf = 1;		//default = 1
		//this->klu.common.ordering = 1;  //default = 0
		this->klu.common.scale = 1;     //default = 2

		this->klu.common.halt_if_singular = false;
	}

	Problem(MultiThread_ThreadInfo<Self, GlobalParams> & threadInfo) : //to be called by worker thread
		multiThreading(false)
	{

		this->multiThreading.pMasterProblem = threadInfo.pMasterProblem;
		this->globalParams = threadInfo.pMasterProblem->globalParams;

		//threadInfo.pMasterProblem->multiThreading.initialization.Enter();
		threadInfo.pMasterProblem->criticalSection.Enter();
		if (threadInfo.pMasterProblem->criticalSection.GetEnterCount() != 1) MY_ERROR("gotcha");
		this->functions.Reference(threadInfo.pMasterProblem->functions);
		//threadInfo.pMasterProblem->multiThreading.initialization.Leave();
		threadInfo.pMasterProblem->criticalSection.Leave();

		this->F_X_upToDate = threadInfo.pMasterProblem->F_X_upToDate;
		this->sparse = threadInfo.pMasterProblem->sparse;
		threadInfo.pWorkerProblem = this;
	}

	void SetLastSolveError(const char * str) {
		Critical_Section(this->criticalSection) {
			this->lastSolveError = str;
		}
	}

	void SetLastSolveError(const char * str, int debugModeLevel) {
		if (this->debugMode >= debugModeLevel) {
			this->SetLastSolveError(str);
		}
	}

	template <class T>
	void SetLastSolveError(T&& func, int debugModeLevel) {
		if (this->debugMode >= debugModeLevel) {
			this->SetLastSolveError(func());
		}
	}

	template <
		bool checkFinite,
		int failSafe,
		bool mesureTiming
	>
		bool Compute_F_X() {

		AutoMesureFunctionTiming<mesureTiming> autoMesure(this->chrono.compute_F_X);

		try {
			if (this->multiThreading.master.enabled) {

				this->multiThreading.master.checkFinite = checkFinite;
				this->multiThreading.master.failSafe = failSafe;
				this->multiThreading.master.allowZeroSparseCoeff = this->allowZeroSparseCoeff;

				this->multiThreading.master.workType = this->compute_F_X;

				if (!this->multiThreading.master.initialized) {
					this->InitializeMultiThreadComputation();
				}
				else {
					this->LaunchMultiThreadComputation();
				}

				this->WaitForAllThreadsReady();

				if (!CheckMultiThreadSuccess()) return false;
				this->BalanceMultiThreadComputation();
			}
			else {
				if (checkFinite || failSafe) {
					aVect_static_for(this->functions, i) {
						if (!_finite(this->CallFunc<true>((int)i))) {
							if (failSafe) return false;
							else MY_ERROR("not finite");
						}
					}
				}
				else aVect_static_for(this->functions, i) (this->CallFunc<true>((int)i));
			}

			this->F_X_upToDate = true;
		}
		catch (...) {
			return this->SolveExceptionHandler(std::current_exception(), failSafe);
		}

		return true;
	}

	bool CheckFinite(bool checkFinite, int failSafe, double dfi_dxj, int i, ptrdiff_t j) {
		return ::CheckFinite<Self>(nullptr, this, checkFinite, failSafe, dfi_dxj, i, j);
	}

	template <
		bool checkFinite,
		int failSafe,
		bool mesureTiming
	>
	bool ComputeJacobian() {

		if (this->multiThreading.master.enabled) {

			if (false && this->debugMode >= 100) {
				this->multiThreading.master.enabled = false;

				this->ComputeJacobian<checkFinite, failSafe, mesureTiming>();

				auto I = this->X.Count();
				this->jacobian.full.Redim(I, I);
				this->jacobian.full.Set(0);

				aVect_static_for(this->jacobian.row, k) {
					auto i = this->jacobian.row[k];
					auto j = this->jacobian.col[k];
					auto val = this->jacobian.values[k];
					this->jacobian.full(i, j) = val;
				}

				this->multiThreading.master.enabled = true;
			}
		}

		this->jacobian.initialized = false;

		if (this->sparse) {
			if (!this->klu.firstSolve) {
				if (mesureTiming) this->chrono.copy_Data.Resume();
				this->oldJacobian.row.Copy(this->jacobian.row);
				this->oldJacobian.col.Copy(this->jacobian.col);
				if (mesureTiming) this->chrono.copy_Data.Stop();
			}
			this->jacobian.row.Redim(0);
			this->jacobian.col.Redim(0);
			this->jacobian.values.Redim(0);
		}
		else {
			if (!this->jacobian.full) this->jacobian.full.Redim(this->functions.Count(), this->X.Count()).Set(0);
		}

		if (!this->F_X_upToDate) if (!this->Compute_F_X<checkFinite, failSafe, mesureTiming>()) return false;

		AutoMesureFunctionTiming<mesureTiming> autoMesure(this->chrono.compute_Jacobian);

		if (this->multiThreading.master.enabled) {

			this->multiThreading.master.checkFinite = checkFinite;
			this->multiThreading.master.failSafe = failSafe;
			this->multiThreading.master.allowZeroSparseCoeff = this->allowZeroSparseCoeff;
			this->multiThreading.master.workType = this->compute_Jacobian;

			if (!this->multiThreading.master.initialized) {
				this->InitializeMultiThreadComputation();
			}
			else {
				//each thread copies X, F_X and GlobalParams
				//g_cs.Enter();
				///*this->multiThreading.*/flagGo.store(false, std::memory_order_seq_cst);
				//flagGo = false;
				//g_cs.Leave();
				this->LaunchMultiThreadComputation();
				//this->WaitForAllThreadsReady();
			}

			//each thread compute their derivatives
			//this->LaunchMultiThreadComputation();
			//g_cs.Enter();
			///*this->multiThreading.*/flagGo.store(true, std::memory_order_seq_cst);
			//flagGo = true;
			//g_cs.Leave();
			this->WaitForAllThreadsReady();

			if (!CheckMultiThreadSuccess()) return false;
			this->BalanceMultiThreadComputation();
			this->CombineThreadsDerivative();

//#ifdef _DEBUG
//			aVect<int> row(this->jacobian.row);
//			aVect<int> col(this->jacobian.col);
//			aVect<double> values(this->jacobian.values);
//
//			this->multiThreading.enabled = false;
//
//			this->ComputeJacobian<checkFinite, failSafe>();
//
//			if (this->jacobian.row.Count() != row.Count()) MY_ERROR("gotcha");
//			if (this->jacobian.col.Count() != col.Count()) MY_ERROR("gotcha");
//			if (this->jacobian.values.Count() != values.Count()) MY_ERROR("gotcha");
//
//			aVect_static_for(row, i) {
//				if (this->jacobian.row[i] != row[i]) MY_ERROR("gotcha");
//				if (this->jacobian.col[i] != col[i]) MY_ERROR("gotcha");
//				if (this->jacobian.values[i] != values[i]) MY_ERROR("gotcha");
//			}
//
//			this->multiThreading.enabled = true;
//#endif

		}
		else {
			aVect_static_for(this->functions, i) {
				auto & func_i = this->functions[i];
				if (func_i.derivatives) {
					int oldN = (int)this->jacobian.col.Count();
					(func_i.*func_i.derivatives)(*this, (int)i, this->jacobian.col, this->jacobian.values);
					for (int j = oldN, J = (int)this->jacobian.col.Count(); j < J; ++j) {
						this->jacobian.row.Push(i);
						if (!this->CheckFinite(checkFinite, failSafe, this->jacobian.values[j], (int)i, this->jacobian.col[j])) return false;
					}
				}
				else {
					aVect_static_for(func_i.I, k) {
						int j = func_i.I[k];
						double dfi_dxj = this->Derivative((int)i, j);
						if (this->allowZeroSparseCoeff || dfi_dxj) {
							//double dfi_dxj = this->Derivative(i, j);
						
							if (!dfi_dxj) {
								double a = 0;
							}

							//if ((checkFinite || failSafe) && !_finite(dfi_dxj)) {
							//	this->jacobian.row.Copy(this->oldJacobian.row);
							//	this->jacobian.col.Copy(this->oldJacobian.col);
							//	if (failSafe) return false;
							//	MY_ERROR("not finite");
							//}

							if (!CheckFinite(checkFinite, failSafe, dfi_dxj, (int)i, j)) return false;

							if (this->sparse) {
								this->jacobian.values.Push(dfi_dxj);
								this->jacobian.row.Push(i);
								this->jacobian.col.Push(j);
							}
							else {
								this->jacobian.full(i, j) = dfi_dxj;
							}
						}
					}
				}
			}
		}

		if (false && !this->klu.firstSolve) {

			bool patternChanged = false;

			if (this->jacobian.row.Count() != this->oldJacobian.row.Count()) 
				patternChanged = true;

			//DBG
			//if (!patternChanged) {
				//aVect_for_inv(this->jacobian.row, i) {
			for (int i = 0, I = Min((int)this->jacobian.row.Count(), (int)this->oldJacobian.row.Count()); i < I; ++i) {
					if (this->jacobian.row[i] != this->oldJacobian.row[i]) {
						patternChanged = true;
						break;
					}
				}
			//}

			if (patternChanged) {
				printf("jacobian iter %llu (solve n\B0%llu) : non zero pattern changed (nValues = %llud).\n", this->nCurrentIterations, this->nSolve, (unsigned long long)this->jacobian.row.Count());
				this->klu.firstSolve = true;
				this->oldJacobian.row.Copy(this->jacobian.row);
				this->oldJacobian.col.Copy(this->jacobian.col);
			}
			//else {
			//	printf("jacobian: same non zero pattern.\n");
			//}
		}

		//printf("===============================\n");

		this->jacobian.initialized = true;

		return true;
	}

	Problem & AddValidValueRange(int i, double min, double max) {
		this->validValuesRange.Push(ValidValueRange{ i, min, max});
		return *this;
	}

	Problem & AddValidValueRange(aVect<int> & indices, double min, double max) {
		for (auto&& i : indices) {
			this->AddValidValueRange(i, min, max );
		}
		return *this;
	}

	Problem & AddValidValueRange(aVect<aVect<int>> & indices, double min, double max) {
		for (auto&& i : indices) {
			this->AddValidValueRange(i, min, max);
		}
		return *this;
	}

	Problem & AddForbiddenNegativeValue(int i) {
		this->validValuesRange.Push(ValidValueRange{i, 0, std::numeric_limits<double>::infinity()});
		return *this;
	}

	Problem & AddForbiddenNegativeValue(aVect<int> & indices) {
		for (auto&& i : indices) {
			this->AddForbiddenNegativeValue(i);
		}
		
		return *this;
	}

	Problem & AddForbiddenNegativeValue(aVect<aVect<int>> & indices) {
		for (auto&& i : indices) this->AddForbiddenNegativeValue(i);
		return *this;
	}

	Problem & SetPlotFunction(PlotFuncType func) {
		this->plotFunc = func;
		return *this;
	}

	Problem & SetPlotParam(void * param) {
		this->plotFunc = param;
		return *this;
	}

	Problem & SetIterationCallback(IterationCallbackType func) {
		this->iterationCallback = func;
		return *this;
	}

	Problem & SetExceptionCallback(ExceptionCallbackType func) {
		this->solveExceptionCallback = func;
		return *this;
	}

	//Problem & SetVariableNamesCallback(VariableNamesCallbackType func) {
	//	this->variableNamesCallback = func;
	//	return *this;
	//}
	
	const aVect<double> & GetJacobianValues() const {
		return this->jacobian.values;
	}

	const aVect<csi> & GetJacobianRow() const {
		return this->jacobian.row;
	}

	const aVect<csi> & GetJacobianCol() const {
		return this->jacobian.col;
	}

	Problem & Invalidate_F_X() {
		this->F_X_upToDate = false;
		this->residual_max_norm_upToDate = false;
		this->residual_L2_norm_upToDate = false;
		return *this;
	}

	Problem & ClosePlot() {
		this->axes.profiles.CloseWindow();
		this->axes.residues.CloseWindow();
		this->axes.convergence.CloseWindow();

		return *this;
	}

	Problem & Plot(void * param = nullptr) {

		//if (!this->plotFunc) MY_ERROR("Plot function not set");

		if (!this->axes.profiles)    this->axes.profiles.CreateAt(0.5, 0.5, 0.5, 0);
		this->axes.profiles.ClearSeries().SetTitle(Wide(this->name ? this->name : "Profiles"));

		//call userfunc
		PlotFunctionCallbackReturnValue retVal = this->plotFunc ? this->plotFunc(*this, param) : NEWTONSOLVER_NOREFRESH;

		if (retVal == NEWTONSOLVER_REFRESH) {
			this->axes.profiles.AutoFit().Refresh();
		} else if (retVal == NEWTONSOLVER_REFRESHASYNC) {
			this->axes.profiles.AutoFit().RefreshAsync();
		}

		if (this->busy)	{//Sleep(5);// getchar();
			
			if (!this->F_X_upToDate) {
				if (!this->Compute_F_X<false, true, false>()) return *this;
			}

			if (!this->axes.convergence) this->axes.convergence.CreateAt(0.5, 0.5, 0.5, 0.5);
			if (!this->axes.residues)    this->axes.residues.CreateAt(0.5, 0.5, 0, 0.5);

			if (!this->iterationCounter) {
				this->convergenceHistory.max_norm.Redim(0);
				this->convergenceHistory.iter.Redim(0);
			}

			this->CheckConvergence<true, false>();
			this->convergenceHistory.max_norm.Push(log10(this->residual_max_norm));
			this->convergenceHistory.iter.Push((double)this->iterationCounter);
			this->axes.convergence.ClearSeries()
				.AddSerie(this->convergenceHistory.iter, this->convergenceHistory.max_norm)
				.AutoFit().RefreshAsync();

			STATIC_ASSERT_SAME_TYPE(aVect<char>, this->name);

			auto buf = xFormat(L"%S, iteration %llu", this->name, this->nCurrentIterations - this->failSafe.lastFailedIteration);

			if (this->isConverged) {
				buf.Append(L" (converged)");
			}
			else if (this->failedNewton) {
				buf.Append(L" (failed Newton)");
			}

			this->axes.profiles.SetTitle(buf).RefreshAsync();
			this->axes.convergence.SetTitle(buf).RefreshAsync();

			this->PlotResidueVector();

			if (this->debugMode > 100) this->CheckJacobian(3600);
			if (this->debugMode > 1 && !this->noDebugStep) {
				this->ConsolePrompt(xFormat("Debug mode = %d > 1: plot and step", this->debugMode));
			}
			
		}

		return *this;
	}

	void ConsolePrompt(const char * str) {

		static int reentrancyLevel;

		struct ReentrancyGuard {
			int& ref;
			ReentrancyGuard(int& ref) : ref(++ref) {}
			~ReentrancyGuard() { --(this->ref); }
		};

		ReentrancyGuard guard(reentrancyLevel);

		if (reentrancyLevel > 1) {
			return;
		}

		WinCurrentConsole console;

		if (str) console.Write(str);

		struct UserCommand {
			const char * str; 
			const char * desc;
			std::function<void()> func;
		};

		aVect<char> helpString;

		UserCommand commands[] = {
			{"c", "Check jacobian", [&]() {
				try {
					this->CheckJacobian(3600);
				}
				catch (const std::runtime_error & e) {
					printf("exception %s\n", e.what());
				}
				catch (...) {
					printf("exception occured\n");
				}
			}},
			{ "s", "Spy jacobian", [&]() {
				this->SpyJacobian();
			}},
			{ "r", "Reset jacobian", [&]() {
				this->ResetJacobian();
			}},
			{ "j", "compute jacobian", [&]() {
				this->ComputeJacobian<true, 1, false>();
			}},
			{ "l", "List jacobian 0 coefs", [&]() {
				this->ListJacobian0Coef();
			}},
			{ "0", "Debug mode", [&]() {
				this->debugMode = 0;
			}},
			{ "1", "Debug mode", [&]() {
				this->debugMode = 1;
			}},
			{ "2", "Debug mode", [&]() {
				this->debugMode = 2;
			}},
			{ "3", "Debug mode", [&]() {
				this->debugMode = 3;
			}},
			{ "4", "Debug mode", [&]() {
				this->debugMode = 4;
			}},
			{ "5", "Debug mode", [&]() {
				this->debugMode = 5;
			}},
			{ "n", "Toggle debug step", [&]() {
				this->noDebugStep = !this->noDebugStep;
			}},
			{ "b", "Debug break", [&]() {
				if (IsDebuggerPresent()) {
					__debugbreak();
				} else {
					console.Write("Process is not being debugged\n");
				}
			}},
			{ "g", "Abort sub newton", [&]() {
				this->abortNewton = true;
			}},
			{ "?", "Help", [&]() {
				console.Write("");
				console.Write(helpString);
			}},
			{ "dx", "Plot Delta X", [&]() {
				this->Plot((void*)&this->delta_X);
			}},
			{ "p", "Plot", [&]() {
				this->Invalidate_F_X();
				this->Plot();
			}}
		};

		for (auto&& c : commands) {
			helpString.AppendFormat("%s: %s\n", c.str, c.desc);
		}

		for (;;) {
			console.Write(">");
			auto line = console.GetLine();

			auto knownCommand = [&]() {
				for (auto&& c : commands) {
					if (StrEqual(line, c.str)) {
						c.func();
						return true;
					}
				}
				
				return false; 
			}();
			
			if (!knownCommand) {
				if (StrEqual(line, "")) {
					console.Write("Computation resumed\n");
					return;
				}
				console.Write("Unrecognized command\n");
			}
		}
	}

	const Problem & PlotResidueVector() {
		PlotResidueVectorCore<false>(AxisClass());
		return *this;
	}

	const Problem & PlotResidueVector(AxisClass & userAxis) const {
		PlotResidueVectorCore<true>(userAxis);
		return *this;
	}

	template <class T>
	double CenteredDerivative(T&& func, int varInd) const {

		double dx = this->X[varInd] * (0.5e-3);

		if (dx == 0) {
			dx = 0.5e-3;
		}

		double oldVal = this->X[varInd];
		double res1, res2;

		this->X[varInd] = oldVal - dx;
		res1 = func();
		
		this->X[varInd] = oldVal + dx;
		res2 = func();
		
		this->X[varInd] = oldVal;

		return (res2 - res1) / (2*dx);
	}

	double Derivative(int funcInd, int varInd) const {
		return this->Derivative([this, funcInd]() { return ((Problem*)(this))->CallFunc<false>(funcInd); }, varInd, funcInd);
	}

	template <class T, bool standAloneFunction = false>
	double Derivative(T&& func, int varInd, int funcInd) const {

		if (this->nRetryNewtonRaphson > 0) return this->CenteredDerivative(func, varInd);

		double dx = this->X[varInd] * (1e-3);

		if (dx == 0) {
			dx = 1e-3;
		}

		double oldVal = this->X[varInd];
		double res1 = standAloneFunction ? func() : this->F_X[funcInd];

		if (false && this->debugMode >= 100) {
			auto checkRes1 = ((Problem*)(this))->CallFunc<false>(funcInd);
			if (checkRes1 != res1) {
				int a = 0;
				MY_ERROR("gotcha");
			}
		}

		//if (dx == 0)

		this->X[varInd] += dx;

		double res2;

		try {
			res2 = func();
		}
		catch (...) {
			dx *= -1;
			this->X[varInd] = oldVal + dx;
			res2 = func();
		}

		this->X[varInd] = oldVal;

		return (res2 - res1) / dx;
	}

	void PushDerivativeResult_Slow_(
		aVect<ptrdiff_t> & varIndices,
		aVect<double> & values, 
		int funcIndex,
		ptrdiff_t varIndex,
		size_t initCount)
		const
	{

		if (!varIndices.Find(varIndex, initCount)) {
			auto d = this->Derivative(funcIndex, (int)varIndex);
			if (this->allowZeroSparseCoeff || d) {
				varIndices.Push(varIndex);
				values.Push(d);
			}
		}
	}

	void PushDerivativeValue(
		aVect<ptrdiff_t> & varIndices,
		aVect<double> & values,
		ptrdiff_t varIndex,
		double value)
		const
	{

		if (this->allowZeroSparseCoeff || value) {
			varIndices.Push(varIndex);
			values.Push(value);
		}
	}

	template <class Domain, size_t nVar, bool... args>
	void PushDerivativeResult_Slow_(
		aVect<ptrdiff_t> & varIndices,
		aVect<double> & values,
		int funcIndex, 
		const Convolution<Domain, nVar, args...> & conv,
		size_t initCount)
		const
	{
		for (auto&& pt : conv.points) {
			for (int i : pt.ind) {
				if (i == -1) continue;
				PushDerivativeResult_Slow_(varIndices, values, funcIndex, i, initCount);
			}
		}
	}

	template <class T>
	void PushDerivativeResult_Slow_(
		aVect<ptrdiff_t> & varIndices,
		aVect<double> & values,
		int funcIndex,
		const aVect<T> & vect,
		size_t initCount)
		const
	{
		for (auto&& v : vect) {
			PushDerivativeResult_Slow_(varIndices, values, funcIndex, v, initCount);
		}
	}

	template <bool resetFunctions = false>
	Problem & Redim(int nVar, int nFunc) {

		this->ResetKlu();
		this->X.Redim(nVar);
		this->F_X.Redim(nFunc);
		this->delta_X.Redim(nVar);
		this->variableNames.Redim(nVar);

		if (resetFunctions) {
			this->varFuncIndices.Redim(0);
			this->functions.Redim<false>(0);
		}

		this->varFuncIndices.Redim(nVar);	//reset function indices
		this->functions.Redim(nFunc);		//reset function local

		this->Invalidate_F_X();

		return *this;
	}

	template <bool resetFunctions = false>
	Problem & Redim(int n) {
		return this->Redim<resetFunctions>(n, n);
	}

	const aVect<double> & GetVector() const {
		return this->X.ToAvect();
	}

	void ReferenceStateVector(mVect<double> & v) {
		this->X.Reference(v);
	}

	mVect<double> GetStateVectorReference() {
		mVect<double> v;
		v.Reference(this->X);
		return v;
	}
	
	const aVect<double> & GetResidue() const {
		return this->F_X;
	}

	void MakeSubVector(
		aVect<double> & retVal, 
		const aVect<int> & indices, 
		double conversionFactor = 1,
		double offset = 0) const
	{
		::MakeSubVector(this->X.ToAvect(), retVal, indices, conversionFactor, offset);
	}

	aVect<double> MakeSubVector(const aVect<int> & indices, double conversionFactor = 1) const {
		aVect<double> retVal(indices.Count());
		this->MakeSubVector(retVal, indices, conversionFactor);
		return std::move(retVal);
	}

	Problem & InitVarFuncIndices() {

		if (!this->sparse) this->jacobian.full.Redim(this->functions.Count(), this->X.Count()).Set(0);

		aVect_static_for(this->functions, i) {
			auto & func_i = this->functions[i];
			aVect_static_for(func_i.I, j) {
				//sanity check
				auto & varFuncIndices_j = this->varFuncIndices[func_i.I[j]];
				aVect_static_for(varFuncIndices_j, k) {
					if (varFuncIndices_j[k] == i) MY_ERROR("indice of variable present multiple times");
				}

				varFuncIndices_j.Push((int)i);
			}
		}

		return *this;
	}

	template <
		bool computeMaxNorm,
		bool mesureTiming,
		bool computeL2Norm = false
	>
	bool CheckConvergence(bool adjustRelaxFactor = false) const {
		return ((Problem*)this)->CheckConvergence<computeMaxNorm, mesureTiming, computeL2Norm>(adjustRelaxFactor);
	}

	template <
		bool computeMaxNorm,
		bool mesureTiming,
		bool computeL2Norm = false
	>
	bool CheckConvergence(bool adjustRelaxFactor = false) {
		
		if (!this->F_X_upToDate) if (!this->Compute_F_X<false, true, mesureTiming>()) return false;

		if (computeL2Norm) {
			this->residual_L2_norm = L2_Norm(this->F_X);
			this->residual_L2_norm_upToDate = true;
		}

		if (this->residual_max_norm_upToDate) {
			this->isConverged = this->residual_max_norm < this->convergenceCriterion;
			return this->isConverged;
		}

		auto autoAdjustRelaxFactor = this->autoAdjustRelaxFactor;

		if (!adjustRelaxFactor) autoAdjustRelaxFactor = false;

		if (!computeMaxNorm && autoAdjustRelaxFactor) return this->CheckConvergence<true, mesureTiming>(true);

		AutoMesureFunctionTiming<mesureTiming> autoMesure(this->chrono.check_Convergence);

		if (computeMaxNorm) this->residual_max_norm = 0;
		this->isConverged = true;
		this->residual_max_norm_upToDate = false;

		for (int i = 0, I = (int)this->X.Count(); i < I; ++i) {
			double abs_res = abs(this->F_X[i]);
			if (abs_res > this->convergenceCriterion) {
				this->isConverged = false;
				if (!computeMaxNorm) return false;
			}
			if (computeMaxNorm) {
				if (abs_res > this->residual_max_norm) this->residual_max_norm = abs_res;
			}
		}

		if (autoAdjustRelaxFactor) {
			if (this->nCurrentIterations == 1) {
				this->residual_max_norm_history.Redim(0);
			}
			
			this->residual_max_norm_history.Push(this->residual_max_norm);

			if (residual_max_norm_history.Count() > 5) {
				auto delta = this->residual_max_norm - residual_max_norm_history.Last(-5);
				if (delta >= 0) {
					if (this->debugMode > 2) printf("relaxFactor decreased from %g", this->relaxFactor);
					this->relaxFactor *= 0.95;
					if (this->debugMode > 2) printf(" to %g\n", this->relaxFactor);
					residual_max_norm_history.Redim(0).Push(this->residual_max_norm);
				}
			}
		}

		if (computeMaxNorm) this->residual_max_norm_upToDate = true;

		return this->isConverged;
	}

	Problem & DumbSolve(double convergenceCriterion = 0) {

		aVect_static_for(this->X, i) {
			if (!_finite(this->X[i])) MY_ERROR("Infinite value in X. Has the problem been initialized ?");
		}

		if (convergenceCriterion) Swap(this->convergenceCriterion, convergenceCriterion);

		while (true) {
			for (int i = 0, I = this->X.Count(); i < I; ++i) {
				this->FindMinRes(i);
			}

			this->nCurrentIterations++;
			this->iterationCounter++;

			if (this->CheckConvergence<false, false>()) break; 
		}

		if (convergenceCriterion) Swap(this->convergenceCriterion, convergenceCriterion);

		return *this;
	}
	
	Problem & InitVector(double val) {
		this->X.Set(val);
		this->F_X_upToDate = false;
		this->residual_max_norm_upToDate = false;
		return *this;
	}

	Problem & InitVector(const aVect<double> & X) {
		if (this->X.Count() != X.Count()) MY_ERROR("vectors must have the same number of elements");
		this->X.Copy(X);
		this->F_X_upToDate = false;
		this->residual_max_norm_upToDate = false;
		return *this;
	}

	Problem & ListJacobian0Coef() {

		aVect_static_for(this->jacobian.values, i) {
			if (!this->jacobian.values[i]) {
				auto iRow = this->jacobian.row[i];
				auto iCol = this->jacobian.col[i];
				xPrintf("J(%d, %d) = 0: function %s, var %s\n", iRow, iCol, this->functions[iRow].name, this->variableNames[iCol]);
			}
		}


		return *this;
	}

	Problem & SpyJacobian(char * title = nullptr, bool toConsole = false) {
		if (this->sparse) {
			aMat<double> fullJacobian(this->functions.Count(), this->X.Count());
			double absMin = std::numeric_limits<double>::infinity();
			double absMax = 0;
			fullJacobian.Set(0);
			aVect_static_for(this->jacobian.row, i) {
				//fullJacobian(this->jacobian.row[i], this->jacobian.col[i]) = (float)this->jacobian.values[i];
				double val = this->jacobian.values[i];
				if (val == 0) {
					double a=0;
				} 
				if (abs(val) > absMax) absMax = abs(val);
				if (abs(val) < absMin) absMin = abs(val);
				fullJacobian(this->jacobian.row[i], this->jacobian.col[i]) = val;
			}
			aVect<char> newTitle("%s [%g ; %g]", title, absMin, absMax);
			AxisClass axis;
			SpyMatrix(fullJacobian, newTitle, toConsole, true, &axis);

			auto callbackData = new SpyJacobianStatusTextData;
			callbackData->values = std::move(fullJacobian);

			//for (auto&& f : this->functions) callbackData->functions.Push(*(void**)(&f.function));
			for (auto&& f : this->functions) callbackData->funcNames.Push(f.name);

			callbackData->varNames = this->variableNames;

			//if (this->variableNamesCallback) {
			//	this->variableNamesCallback(*this, callbackData->varNames);
			//}

			axis.SetCallbacksData(callbackData);
			axis.SetCallbacksCleanUpCallback(SpyJacobianStatusTextCleanUp);
			axis.SetStatusTextCallback(SpyJacobianStatusText);
		}
		else {
			SpyMatrix(this->jacobian.full, title, toConsole, true);
		}
		return *this;
	}

	template <int failSafe = true>
	bool CheckNotSingular() {

		if (this->klu.common.status == KLU_SINGULAR) {
			
			aVect<char> buffer;
			auto func = [this, &buffer]() -> auto & {return buffer ? buffer : buffer.Format("singular jacobian: singular column = %d", this->klu.common.singular_col); };

			if (failSafe) {
				this->SetLastSolveError(func, 1);
				if (this->debugMode > 1) this->ConsolePrompt(func());
				return false;
			}

			this->SpyJacobian(func());

			auto errMsg = func();

			errMsg.Print();

			getchar();

			MY_ERROR(errMsg);
		}

		return true;
	}

	void KluError(char * function) {
		MY_ERROR(aVect<char>("%s failed (%s)", function, KluErrorDescription(this->klu.common.status)));
	}

	Problem & ResetPerformanceChronometers(bool reset_nIter = true) {
		if (reset_nIter) this->iterationCounter = 0;
		this->chrono.compute_Jacobian.Reset();
		this->chrono.compress_Jacobian.Reset();
		this->chrono.analyze.Reset();
		this->chrono.factor.Reset();
		this->chrono.solve.Reset();
		this->chrono.check_Data.Reset();
		this->chrono.check_Convergence.Reset();
		this->chrono.apply_Delta_X.Reset();
		this->chrono.copy_Data.Reset();
		this->chrono.compute_F_X.Reset();
		this->chrono.total.Reset();
		return *this;
	}

	Problem &  SetApplyDeltaX_Func(Apply_Delta_x_FuncType func) {
		this->apply_delta_x_func = func;
		return *this;
	}

	Problem & SetGlobalParams(GlobalParams & params) {
		*this->globalParams = params;
		return *this;
	}

	const GlobalParams & GetGlobalParams() {
		return *this->globalParams;
	}

	bool SetUpForRetryNewtonRaphsonSolve(
		int verboseLvl,
		OutputInfo & output = OutputInfo(),
		const char * desc = nullptr)
	{

		if (this->nRetryNewtonRaphson == 25) {
			if (verboseLvl >= 0) output.printf("Newton-Raphson failed 25 times, starting again with linear relaxation factor growth.\n");
			this->failSafe.linearIncrease_RelaxFactor = true;
			this->relaxFactor = this->failSafe.backUpRelaxFactor;
		}
		else {	
			if (this->debugMode >= 2 && this->nRetryNewtonRaphson == 0) {
				this->relaxFactor = this->failSafe.minRelaxFactor;
			}
			else {
				this->relaxFactor = 0.5 * this->failSafe.minRelaxFactor;
				//this->autoAdjustRelaxFactor = false;
			}
		}
		this->failSafe.minRelaxFactor = this->relaxFactor;

#ifdef NEWTONSOLVER_BUGHUNT_1
		_CrtCheckMemory();
#endif

		this->X.Copy(this->failSafe.backup_X);
		this->F_X_upToDate = false;
		this->residual_max_norm_upToDate = false;
		this->nRetryNewtonRaphson++;
		if (this->iterationCallback) this->iterationCallback(*this);
		this->ResetJacobian();

		if (this->nRetryNewtonRaphson > 40) {
			if (verboseLvl >= 0) output.printf("Newton-Raphson failed 40 times, giving up.\n");
			return false;
		}
		if (verboseLvl >= 0) {
			if (this->debugMode > 0 && this->lastSolveError) output.printf("%s\n", (char*)this->lastSolveError);
			output.printf("Newton-Raphson failed (it %llu%s%s), retrying with relaxFactor = %g\n", this->nCurrentIterations, desc ? " " : "", desc ? desc : "", this->relaxFactor);
		}
		this->failSafe.lastFail_nIterations = this->nCurrentIterations - this->failSafe.lastFailedIteration;
		this->failSafe.lastFailedIteration = this->nCurrentIterations;

		if (this->noProgressGiveUp) {
			this->minNorm = std::numeric_limits<double>::infinity();
			this->iCurrentIterations_minNorm = this->nCurrentIterations;
		}

		this->abortNewton = false;
		return true;
	}

	template <bool mesureTiming>
	void NewtonRaphsonBackup() {

		if (mesureTiming) this->chrono.copy_Data.Resume();
		this->failSafe.backup_X.Copy(this->X);
		if (mesureTiming) this->chrono.copy_Data.Stop();

		this->failSafe.backUpRelaxFactor = this->relaxFactor;
		this->failSafe.backUpAutoAdjustRelaxFactor = this->autoAdjustRelaxFactor;
		this->failSafe.lastFail_nIterations = 0;
		this->failSafe.lastFailedIteration = 0;
		this->failSafe.minRelaxFactor = this->relaxFactor;
		this->failSafe.linearIncrease_RelaxFactor = false;
	}

	aVect<char> GetFuncName(size_t i, bool full = true) {
		
		aVect<char> retVal;
		
		if (full) retVal = xFormat("function %d", i);

		aVect<char> funcName;

		if (this->functions[i].name) {
			funcName = this->functions[i].name;
		} else {
			::GetFuncName(*(void**)(&this->functions[i].function), funcName);
		}

		if (funcName) {
			if (full) retVal.Append(" (");
			retVal.AppendFormat("%s", funcName);
			if (full) retVal.Append(")");
		}

		return retVal;
	}

	aVect<char> GetVarName(size_t i, bool full = true) {
		
		aVect<char> retVal;
		if (full) retVal = xFormat("variable %d", i);

		//if (this->variableNamesCallback) {
		//	aVect<aVect<char>> varNames;
		//	this->variableNamesCallback(*this, varNames);

		if (this->variableNames && this->variableNames[i]) {
			if (full) retVal.Append(" (");
			retVal.AppendFormat("%s", this->variableNames[i]);
			if (full) retVal.Append(")");
		}
		
		return retVal; 

	}

	Problem & CheckJacobian(double skipDuration = 1, int beginAtFunction = 0, int beginAtVariable = 0) {
		
		PerformanceChronometer chrono, chronoDisp;

		printf("Checking jacobian...\n");

		this->Invalidate_F_X();
		this->ComputeJacobian<true, false, false>();
		this->CompressJacobian<false>();

		for (int i = beginAtFunction; i < (int)this->functions.Count(); ++i) {
			for (int j = beginAtVariable; j < (int)this->X.Count(); ++j) {

				if (j % 100 == 0) {
					if (skipDuration && chrono.GetSeconds() > skipDuration) {
						printf("jacobian Check takes too long, skipping.\n");
						return *this;
					}
					else {
						if (chronoDisp.GetSeconds() > 1) {
							printf("%.2g%% (Row %d/%llu)\n", 100 * (double)i / this->functions.Count(), i, (unsigned long long)this->functions.Count());
							chronoDisp.Restart();
							if (WinKbHit()) {
								auto c = WinGetChar();
								if (c == 27) return *this;
								if (c == '+') {
									i += ((int)this->functions.Count() - beginAtFunction) / 10;
									continue;
								}
							}
						}
					}
				}

				double dfi_dxj = this->Derivative(i, j);

				cs_sparse * cprsd = this->jacobian.compressed;
				
				auto i_row = cprsd->p[j];
				auto i_row_lim = cprsd->p[j+1];
				auto n = cprsd->p[cprsd->n];
				auto Ai = cprsd->i;
				
				for (auto k = i_row; k < i_row_lim; k++) {
					if (i == Ai[k]) {
						//found
						double dfi_dxi_check = cprsd->x[k];
						auto ok = true;
						if (dfi_dxi_check && dfi_dxj) {
							if (!IsEqualWithPrec(dfi_dxi_check, dfi_dxj, 1e-5)) {
								ok = false;
							}
						}
						else {
							if (abs(dfi_dxi_check - dfi_dxj) > 1e-10) ok = false;
						}

						if (!ok) {
							MY_ERROR(xFormat("Wrong derivative : %s, %s, : %g should be %g", this->GetFuncName(i), this->GetVarName(j), dfi_dxi_check, dfi_dxj));
						}
						goto found;
					}
				}
				if (dfi_dxj) {
					MY_ERROR(xFormat("Value missing : %s, %s, value %g", this->GetFuncName(i), this->GetVarName(j), dfi_dxj));
				}
				
//				aVect_static_for(this->jacobian.row, k) {
					//if (this->jacobian.row[k] == i && this->jacobian.col[k] == j) {
					//	double dfi_dxi_check = this->jacobian.values[k];
					//	auto ok = true;
					//	if (dfi_dxi_check && dfi_dxj) {
					//		if (!IsEqualWithPrec(dfi_dxi_check, dfi_dxj, 1e-5)) {
					//			ok = false;
					//		}
					//	}
					//	else {
					//		if (abs(dfi_dxi_check - dfi_dxj) > 1e-10) ok = false;
					//	}
					//	
					//	if (!ok) MY_ERROR(aVect<char>("Wrong derivative : function %d, variable %d : %g should be %g", i, j, dfi_dxi_check, dfi_dxj));
					//	goto found;
					//}
//				}
//				if (dfi_dxj) MY_ERROR(aVect<char>("Value missing : function %d, variable %d, value %g", i, j, dfi_dxj));
found:;
			}
		}

		printf("Done.\n");

		return *this;
	}

	template <
		bool mesureTiming = false,
		bool modified = false,
		bool computeMaxNorm = false,
		bool checkFinite = false,
		bool plotIterations = false,
		int failSafe = 0>
	Problem & GradientDescent(
			int verboseLvl = 0,
			OutputInfo & output = OutputInfo())
	{

		AutoMesureFunctionTiming<mesureTiming> autoMesure(this->chrono.gradDescent);


		if (verboseLvl>1) output.printf("Gradient descent...\n");

		auto oldLineSearch = this->lineSearch;
		auto oldnRetryNewtonRaphson = this->nRetryNewtonRaphson;

		this->lineSearch = true;
		this->nRetryNewtonRaphson = 10;

		this->busy = true;
		this->isConverged = false;
		this->failedNewton = false;
		//this->nRetryNewtonRaphson = 0;
		this->nCurrentIterations = 0;

		for (;;) {

			if (plotIterations ||
				(this->debugMode && this->nRetryNewtonRaphson > 0) ||
				(this->debugMode > 5 && this->nCurrentIterations == 0))
			{
				this->Plot();
			}

			if (WinKbHit()) this->ConsolePrompt("?");

			if (this->nRetryNewtonRaphson > 2) this->ResetJacobian();

			if (!this->ComputeJacobian<checkFinite, failSafe, mesureTiming>()) {
				output.printf("Gradient descent failed...\n");
				break;// return *this;
			}

			this->CompressJacobian<mesureTiming>();

			if (!this->Compute_Grad_L2Norm<checkFinite, failSafe, mesureTiming>()) {
				output.printf("Gradient descent failed...\n");
				break;// return *this;
			}

			this->delta_X = this->grad_L2Norm;
			this->lineSearch = true;
			this->L2_norm_lineSearch = true;
			this->gradientDescentFallBack = false;

			if (!this->ApplyDelta_X<mesureTiming>(output)) {
				output.printf("Gradient descent failed...\n");
				break;// return *this;
			}

			this->nCurrentIterations++;
			this->iterationCounter++;
			if (this->iterationCallback) this->iterationCallback(*this);

			auto converged = this->CheckConvergence<false, mesureTiming, true>();

			static AxisClass axis;
			static aVect<double> x, y;

			x.Push((double)this->iterationCounter);
			y.Push(this->residual_L2_norm);

			axis.ClearSeries().Plot(x, y).SetTitle("L2 Norm").SetLogScaleY(true);

			if (converged) break;
		}

		this->lineSearch = oldLineSearch;
		this->nRetryNewtonRaphson = oldnRetryNewtonRaphson;

		return *this;
	}

	template <bool mesureTiming>
	NewtonLoopAction CheckConvergenceProgress(int verboseLvl, OutputInfo & output) {

		auto convOk = this->noProgressGiveUp ? this->CheckConvergence<true, mesureTiming>(true) : this->CheckConvergence<false, mesureTiming>(true);

		if (convOk) return NewtonLoopAction::action_break; //goto double_break;//break;

		if (this->noProgressGiveUp) {
			if ((1 + 1e-3) * this->residual_max_norm < this->minNorm) {
				this->minNorm = this->residual_max_norm;
				this->iCurrentIterations_minNorm = this->nCurrentIterations;
			}
			else {
				auto noProgress = this->nCurrentIterations - this->iCurrentIterations_minNorm;
				if (noProgress > this->noProgressGiveUp) {
					auto desc = xFormat("no progress for %llu it", noProgress);
					if (!this->SetUpForRetryNewtonRaphsonSolve(verboseLvl, output, desc)) return NewtonLoopAction::action_break;// goto double_break;//break;
					else return NewtonLoopAction::action_continue;// goto continue_loop;// continue;
				}
			}
		}

		return NewtonLoopAction::action_none;
	}

	template <
		bool mesureTiming = false,
		bool modified = false,
		bool computeMaxNorm = false,
		bool checkFinite = false,
		bool plotIterations = false,
		int failSafe = 0>
	Problem & NewtonRaphsonSolve(
		int verboseLvl = 0,
		OutputInfo & output = OutputInfo()) 
	{

		AutoMesureFunctionTiming<mesureTiming> autoMesure(this->chrono.total); 

		if (verboseLvl>1) output.printf("Newton-Raphson...\n");

		//if (this->multiThreading.master.enabled) {
		//	QueryPerformanceCounter(&this->multiThreading.startTicks.Grow(1).Last());
		//}

		this->busy = true;
		this->isConverged = false;
		this->failedNewton = false;
		this->nRetryNewtonRaphson = 0;
		this->nCurrentIterations = 0;
		this->minNorm = std::numeric_limits<double>::infinity();
		this->iCurrentIterations_minNorm = 0;
		this->failSafe.lastFailedIteration = 0;
		this->failSafe.lastFail_nIterations = 0;

		if (failSafe) this->NewtonRaphsonBackup<mesureTiming>();
		
		auto origRelaxFactor = this->relaxFactor;

		while (true) {

		continue_loop:

			this->lastSolveError.Erase();

			auto DbgPlotAndPrompt = [&] {
				auto cond1 = [this]() {return this->debugMode >= 5 || (this->debugMode >= 4 && this->nCurrentIterations - this->failSafe.lastFailedIteration == 0); };
				auto cond2 = [this]() {return this->debugMode >= 5 && this->nCurrentIterations - this->failSafe.lastFailedIteration == 0; };

				if (plotIterations ||
					(this->debugMode && (
					(this->nRetryNewtonRaphson > 0) ||
						(cond1()))))
				{
					this->Plot();
					if (cond2()) {
						this->ConsolePrompt(xFormat("Debug mode %d >= 5, step on first newton sub iteration", this->debugMode));
					}
				}

				if (WinKbHit()) this->ConsolePrompt("?");
			};

			DbgPlotAndPrompt();

			if (this->nRetryNewtonRaphson > 2) this->ResetJacobian();

#ifdef NEWTONSOLVER_BUGHUNT_1
			_CrtCheckMemory();
#endif

			auto ok = this->reuseJacobian ?
				this->ComputeDelta_X<mesureTiming, true, checkFinite, failSafe>()
				:
				this->ComputeDelta_X<mesureTiming, false, checkFinite, failSafe>();
			
#ifdef NEWTONSOLVER_BUGHUNT_1
			_CrtCheckMemory();
#endif

			if (!ok) {
				if (!this->SetUpForRetryNewtonRaphsonSolve(verboseLvl, output)) break;
				else continue;
			}

#ifdef NEWTONSOLVER_BUGHUNT_1
			_CrtCheckMemory();
#endif

			if (modified && this->nRetryNewtonRaphson == 0) {

				for (int repeat = 0; repeat < 10; ++repeat) {

#ifdef NEWTONSOLVER_BUGHUNT_1
					_CrtCheckMemory();
#endif
					if (!this->ApplyDelta_X<mesureTiming>(output)) {
						if (!failSafe || !this->SetUpForRetryNewtonRaphsonSolve(verboseLvl, output)) goto double_break;//break;
						else goto continue_loop;// continue;
					}

#ifdef NEWTONSOLVER_BUGHUNT_1
					_CrtCheckMemory();
#endif

					if (!this->F_X_upToDate) {
						if (!this->Compute_F_X<checkFinite, failSafe, mesureTiming>()) {
							if (!failSafe || !this->SetUpForRetryNewtonRaphsonSolve(verboseLvl, output)) goto double_break;//break;
							else goto continue_loop;// continue;
						}
					}

#ifdef NEWTONSOLVER_BUGHUNT_1
					_CrtCheckMemory();
#endif

					this->nCurrentIterations++;
					this->iterationCounter++;
					if (this->iterationCallback) this->iterationCallback(*this);

#ifdef NEWTONSOLVER_BUGHUNT_1
					_CrtCheckMemory();
#endif

					switch (this->CheckConvergenceProgress<mesureTiming>(verboseLvl, output)) {
						case NewtonLoopAction::action_break:    goto double_break;
						case NewtonLoopAction::action_continue: goto continue_loop;
					}

#ifdef NEWTONSOLVER_BUGHUNT_1
					_CrtCheckMemory();
#endif

					if (!this->jacobian.initialized) {
						if (!this->ComputeJacobian<checkFinite, failSafe, mesureTiming>()) {
							if (!failSafe || !this->SetUpForRetryNewtonRaphsonSolve(verboseLvl, output)) goto double_break;//break;
							else goto continue_loop;// continue;
						}
					}

					DbgPlotAndPrompt();

					if (!this->ComputeDelta_X<mesureTiming, true, checkFinite, failSafe>()) {
						if (!this->SetUpForRetryNewtonRaphsonSolve(verboseLvl, output)) goto double_break;//break;
						else goto continue_loop;// continue;
					}
#ifdef NEWTONSOLVER_BUGHUNT_1
					_CrtCheckMemory();
#endif

				}
			}

			if (!this->ApplyDelta_X<mesureTiming>(output)) {
				if (!failSafe || !this->SetUpForRetryNewtonRaphsonSolve(verboseLvl, output)) break;
				else continue;
			}

#ifdef NEWTONSOLVER_BUGHUNT_1
			_CrtCheckMemory();
#endif
			if (!this->F_X_upToDate) {
				if (!this->Compute_F_X<checkFinite, failSafe, mesureTiming>()) {
					if (!failSafe || !this->SetUpForRetryNewtonRaphsonSolve(verboseLvl, output)) break;
					else continue;
				}
			}

#ifdef NEWTONSOLVER_BUGHUNT_1
			_CrtCheckMemory();
#endif

			if (false && failSafe && !this->autoAdjustRelaxFactor &&
				this->relaxFactor != this->failSafe.backUpRelaxFactor && 
				this->nCurrentIterations - this->failSafe.lastFail_nIterations > 1.5 * this->failSafe.lastFail_nIterations) 
			{
				if (!this->failSafe.linearIncrease_RelaxFactor) this->relaxFactor = Min(1.1 * this->relaxFactor, 1);
				else this->relaxFactor = Min(this->relaxFactor + 0.001 * this->failSafe.minRelaxFactor, 1);
				if (verboseLvl > 2) output.printf("Increasing relaxation factor to %g\n", this->relaxFactor);
			}

#ifdef NEWTONSOLVER_BUGHUNT_1
			_CrtCheckMemory();
#endif

			this->nCurrentIterations++;
			this->iterationCounter++;
			if (this->iterationCallback) this->iterationCallback(*this);

#ifdef NEWTONSOLVER_BUGHUNT_1
			_CrtCheckMemory();
#endif

			switch (this->CheckConvergenceProgress<mesureTiming>(verboseLvl, output)) {
				case NewtonLoopAction::action_break:    goto double_break;
				case NewtonLoopAction::action_continue: goto continue_loop;
			}

#ifdef NEWTONSOLVER_BUGHUNT_1
			_CrtCheckMemory();
#endif

			if (failSafe) {
				if (this->nCurrentIterations - this->failSafe.lastFailedIteration >= (failSafe == 1 ? 1 : failSafe == 2 ? 10 : 100)*this->maxNewtonRaphsonIterations) {
					if (!this->SetUpForRetryNewtonRaphsonSolve(verboseLvl, output)) break;
					else continue;
				}
			}
			else if (this->nCurrentIterations >= this->maxNewtonRaphsonIterations) break;
		}

double_break:

		if (this->nRetryNewtonRaphson) this->nConvProblem++;

		this->relaxFactor = origRelaxFactor;

		if (!this->isConverged) {
			if (verboseLvl >= 0) {
				this->CheckConvergence<true, mesureTiming>();
				output.printf("Newton Raphson method didn't converge after %llu iterations. Max norm = %.4e > %.4e\n",
					this->nCurrentIterations, this->residual_max_norm, this->convergenceCriterion);
				this->failedNewton = true;
			}
		}
		else {
			if (verboseLvl > 1) output.printf("Done.\n");
			if (verboseLvl >= 0 && this->nRetryNewtonRaphson) {
				output.printf("Convergence reached after %llu iterations.\n", this->nCurrentIterations);
			}
			this->nSolve++;
		}

		if (plotIterations) this->Plot();

		if (this->debugMode >= 4) {
			this->Plot();
			this->ConsolePrompt("Newton convergence OK");
		}

		if (computeMaxNorm) {
			this->CheckConvergence<true, mesureTiming>();
		}

		if (failSafe) {
			this->relaxFactor = this->failSafe.backUpRelaxFactor;
			this->autoAdjustRelaxFactor = this->failSafe.backUpAutoAdjustRelaxFactor;
		}
		this->busy = false;

		//if (this->multiThreading.master.enabled) {
		//	QueryPerformanceCounter(&this->multiThreading.stopTicks.Grow(1).Last());
		//}

		return *this;
	}
	
	void DisplayTiming(
		PerformanceChronometer & chrono, 
		const char * name, 
		bool perIter, 
		double * percentOf = nullptr,
		OutputInfo & output = OutputInfo())
	{

		static WinCriticalSection cs;

		Critical_Section(cs) {

			double val = chrono.GetMicroSeconds();
			const char * unit = "micro seconds";
			//static aVect<char> buf, blanks;
			aVect<char> buf, blanks;

			double origVal = val;

			if (val > 1000) {
				val /= 1000;
				unit = "ms";
			}

			if (val > 1000) {
				val /= 1000;
				unit = "s";
			}
			
			buf.Format("    %s: %6s %s", name, NiceFloat(val, 1e-4), unit); 
			
			if (percentOf) {
				int tab = 50;
				int nBlanks = tab - (int)buf.Count();
				if (nBlanks == 0 || nBlanks> tab) nBlanks = 1;
				
				blanks.Redim(nBlanks).Set(' ').Push(0);
				buf.AppendFormat("%s %4.1f %%", blanks, origVal / *percentOf * 100);
			}
			if (perIter) buf.Append(" (%f iter/s)", this->iterationCounter / chrono.GetSeconds());

			output.Format("%s\n", buf);
		}
	}

	template <bool reset = false>
	Problem & DisplayTimings(
		bool nIter = true, 
		bool nVar = true, 
		bool perIter = true, 
		OutputInfo & output = OutputInfo())
	{

		output.printf("Newton-Raphson Elpased time");
		if (nIter || nVar) output.printf(" (");
		if (nIter) output.printf("nIter = %llu", this->iterationCounter);
		if (nIter && nVar) output.printf(", ");
		if (nVar) output.printf("nVar = %d", this->X.Count());
		if (nIter || nVar) output.printf(")");
		output.printf(":\n");

		double totalTime = this->chrono.total.GetMicroSeconds();

		DisplayTiming(this->chrono.compute_Jacobian,  "Compute Jacobian  ", false, &totalTime,  output);
		DisplayTiming(this->chrono.compress_Jacobian, "Compress Jacobian ", false, &totalTime,  output);
		DisplayTiming(this->chrono.compute_F_X,       "Compute F_X       ", false, &totalTime,  output);
		DisplayTiming(this->chrono.analyze,           "klu Analyze       ", false, &totalTime,  output);
		DisplayTiming(this->chrono.factor,            "klu Factor        ", false, &totalTime,  output);
		DisplayTiming(this->chrono.solve,             "klu Solve         ", false, &totalTime,  output);
		DisplayTiming(this->chrono.check_Data,        "Check Data        ", false, &totalTime,  output);
		DisplayTiming(this->chrono.check_Convergence, "Check Convergence ", false, &totalTime,  output);
		DisplayTiming(this->chrono.apply_Delta_X,     "Apply Delta X     ", false, &totalTime,  output);
		DisplayTiming(this->chrono.copy_Data,         "Copy Data         ", false, &totalTime,  output);
		DisplayTiming(this->chrono.total,             "Total             ", perIter, nullptr,   output);

		if (reset) this->ResetPerformanceChronometers(true);

		return *this;
	}

	template <bool plot = true>
	void PlotMultiThreadingTiming(
		AxisClass * pUserAxis = nullptr, 
		const char * title = nullptr, 
		LONGLONG startFrom = 0, 
		aVect<double> * getThreadsLoad = nullptr)
	{

		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);

		if (this->multiThreading.pMasterProblem) MY_ERROR("must be called only by master thread");

		if (this->multiThreading.stopTicks.Count() != this->multiThreading.startTicks.Count()) MY_ERROR("counts not equal");
#ifdef USE_GLOBAL_GO_EVENT
		if (this->multiThreading.stopTicks.Count() - 1 != this->multiThreading.setEventTicks.Count()) MY_ERROR("counts not equal");
#else
		int N = (int)this->multiThreading.master.thread.Count();
		if ((this->multiThreading.stopTicks.Count()-1) * N != this->multiThreading.setEventTicks.Count()) MY_ERROR("counts not equal");
#endif

		LARGE_INTEGER start = this->multiThreading.startTicks[0];

		aVect<double> x, y;
		
		if (plot) {

			aVect_static_for(this->multiThreading.startTicks, i) {

				if (this->multiThreading.stopTicks[i].QuadPart < startFrom) continue;

				double t1 = (double)(this->multiThreading.stopTicks[i].QuadPart  - start.QuadPart);
				double t2 = (double)(this->multiThreading.startTicks[i].QuadPart - start.QuadPart);

				x.Push(t1).Push(t1).Push(t2).Push(t2);
				y.Push(2) .Push(0) .Push(0) .Push(2);

				if (i != this->multiThreading.startTicks.Count() - 1) {
#ifdef USE_GLOBAL_GO_EVENT
					double t3 = (double)(this->multiThreading.setEventTicks[i].QuadPart - start.QuadPart);

#else
					double t3 = (double)(this->multiThreading.setEventTicks[i*N].QuadPart - start.QuadPart);
#endif
					x.Push(t3).Push(t3) .Push(t3);
					y.Push(2) .Push(0.2).Push(2);

#ifdef USE_GLOBAL_GO_EVENT
#else
					for (int j = 1; j < N; ++j) {
						double t4 = (double)(this->multiThreading.setEventTicks[N*i + j].QuadPart - start.QuadPart);
						x.Push(t4).Push(t4) .Push(t4);
						y.Push(2) .Push(0.2).Push(2);
					}
#endif
				}
			}

		}

		AxisClass localAxis;
		AxisClass & axis = pUserAxis ? *pUserAxis : localAxis;

		if (plot) {
			if (!axis) axis.Create();

			aVect_static_for(x, i) x[i] /= freq.QuadPart;

			axis.ClearSeries().AddSerie(L"Master thread", x, y);
		}

		int nThread = (int)this->multiThreading.master.thread.Count();

		if (getThreadsLoad) getThreadsLoad->Redim(nThread).Set(0);

		aVect_static_for(this->multiThreading.master.thread, i) {

			auto & ref = this->multiThreading.master.thread[i].pWorkerProblem->multiThreading;
			//if (ref.stopTicks.Count() != ref.startTicks.Count()) MY_ERROR("counts not equal");
			//if (ref.stopTicks.Count() != ref.setEventTicks.Count()) MY_ERROR("counts not equal");
			if (ref.startTicks.Count() != ref.setEventTicks.Count()) MY_ERROR("counts not equal");

			x.Redim(0);
			y.Redim(0);

			aVect_static_for(ref.startTicks, j) {
				
				if (ref.startTicks[j].QuadPart < startFrom) continue;

				double t1 = (double)(ref.startTicks[j].QuadPart - start.QuadPart);
				//double t2 = (double)(ref.setEventTicks[j].QuadPart - start.QuadPart);
				//double t3 = (double)(ref.stopTicks[j].QuadPart - start.QuadPart);
				double t3 = (double)(ref.setEventTicks[j].QuadPart - start.QuadPart);

				if (plot) {
					double val = 1 - (double)i / nThread;
					val *= ref.workType[j] == this->compute_Jacobian ? 1 : -1;

					x.Push(t1).Push(t1) /*.Push(t2) .Push(t2)       .Push(t2) */.Push(t3) .Push(t3);
					y.Push(0) .Push(val)/*.Push(val).Push(0.9 * val).Push(val)*/.Push(val).Push(0);
				}

				//if (getThreadsLoad) (*getThreadsLoad)[i] += t2 - t1;
				if (getThreadsLoad) (*getThreadsLoad)[i] += t3 - t1;
			}

			if (plot) {
				aVect_static_for(x, i) x[i] /= freq.QuadPart;

				axis.AddSerie(xFormat(L"Thread %d [id %d]", i, this->multiThreading.master.thread[i].threadID), x, y);
			}
		}

		if (plot) axis.SetTitle(Wide(title)).AutoFit().Refresh();
	}

	void EnableMultiThreading(bool enable, int nThreads = 0, bool enableLoadBalancing = false) {
		this->multiThreading.master.enabled = enable;
		this->multiThreading.nThreads = nThreads;
		this->multithreadLoadBalancing = enableLoadBalancing;
	}

	double & operator [](int i) const {
		return this->X[i];
	}

	GlobalParams * operator->() const {
		return this->globalParams;
	}

	Problem & ResetJacobian() {
		this->ResetKlu();
		this->jacobian.initialized = false;
		return *this;
	}

	~Problem() {

		if (!this->multiThreading.pMasterProblem) {// is not worker thread ?

			if (this->jacobian.compressed) {
				this->jacobian.compressed = cs_spfree(this->jacobian.compressed);
			}

			if (this->jacobian.workspace) {
				this->jacobian.workspace = (csi*)cs_free((void*)this->jacobian.workspace);
			}

			this->ResetKlu();

			delete this->globalParams;

#ifdef USE_GLOBAL_GO_EVENT
			if (this->multiThreading.master.hEventGo[0]) {
				SAFE_CALL(CloseHandle(this->multiThreading.master.hEventGo[0]));
			}
			if (this->multiThreading.master.hEventGo[1]) {
				SAFE_CALL(CloseHandle(this->multiThreading.master.hEventGo[1]));
			}
#endif
		}

		auto pMasterProblem = this->multiThreading.pMasterProblem;
		if (pMasterProblem || this->multiThreading.master.thread) { // is worker thread or has worker threads ?
			if (!pMasterProblem) pMasterProblem = this;
			pMasterProblem->criticalSection.Enter();
			if (pMasterProblem->criticalSection.GetEnterCount() != 1) MY_ERROR("gotcha");
			this->functions.~mVect();
			pMasterProblem->criticalSection.Leave();
		}

		//not here, in MultiThread_ThreadInfo destructor 
		//if (this->multiThreading.master.thread) {
		//	for (auto&& t : this->multiThreading.master.thread) {
		//		if (!t.threadHandle) MY_ERROR("gotcha");
		//		WaitForSingleObject(t.threadHandle, INFINITE);
		//		SAFE_CALL(CloseHandle(t.threadHandle));
		//	}
		//}
	}
};

//
//aMat<double> CompressedToFull(cs_sparse * S) {
//	
//	aMat<double> F(S->m, S->n);
//
//	F.Set(0);
//
//	for (int j = 0; j < S->n; ++j) {
//		for (int k = S->p[j]; k < S->p[j + 1]; ++k) {
//
//			int i = S->i[k];
//			F(i, j) = S->x[k];
//		}
//	}
//	return std::move(F);
//}

struct Variable {

	int ind = -1;
	MagicPointer<double> pConstant;

	Variable() {}
	Variable(int i) : ind(i) {}
	Variable(MagicTarget<double> & constant) : pConstant(&constant) {}

	template <class T, class U, class V>
	double operator()(const Problem<T, U, V> & p) const {
		if (this->ind != -1) return p[this->ind];
		return *pConstant;
	}

	double operator()(const aVect<double> & X) const {
		if (this->ind != -1) return X[this->ind];
		return *pConstant;
	}

	operator int() const {
		return this->ind;
	}

	Variable & SetIndex(int i) {
		this->ind = i;
		return *this;
	}

	Variable & SetConstantValue(MagicTarget<double> * constant) {
		this->ind = -1;
		this->pConstant = constant;
		return *this;
	}
};

inline void GetFuncName(void * address, aVect<char> & name) {


	//// SymCleanup()
	//typedef BOOL(__stdcall *tSC)(IN HANDLE hProcess);
	//tSC pSymCleanup;

	//// SymFunctionTableAccess64()
	//typedef PVOID(__stdcall *tSFTA)(HANDLE hProcess, DWORD64 AddrBase);
	//tSFTA pSymFunctionTableAccess;

	//// SymGetLineFromAddr64()
	//typedef BOOL(__stdcall *tSGLFA)(IN HANDLE hProcess, IN DWORD64 dwAddr,
	//	OUT PDWORD pdwDisplacement, OUT PIMAGEHLP_LINE64 Line);
	//tSGLFA pSymGetLineFromAddr;

	//// SymGetModuleBase64()
	//typedef DWORD64(__stdcall *tSGMB)(IN HANDLE hProcess, IN DWORD64 dwAddr);
	//tSGMB pSymGetModuleBase;

	////// SymGetModuleInfo64()
	////typedef BOOL(__stdcall *tSGMI)(IN HANDLE hProcess, IN DWORD64 dwAddr, OUT IMAGEHLP_MODULE64_V3 *ModuleInfo);
	////tSGMI pSymGetModuleInfo;

	//// SymGetOptions()
	//typedef DWORD(__stdcall *tSGO)(VOID);
	//tSGO pSymGetOptions;

	//// SymGetSymFromAddr64()
	//typedef BOOL(__stdcall *tSGSFA)(IN HANDLE hProcess, IN DWORD64 dwAddr,
	//	OUT PDWORD64 pdwDisplacement, OUT PIMAGEHLP_SYMBOL64 Symbol);
	//tSGSFA pSymGetSymFromAddr;

	//// SymInitialize()
	//typedef BOOL(__stdcall *tSI)(IN HANDLE hProcess, IN PSTR UserSearchPath, IN BOOL fInvadeProcess);
	//tSI pSymInitialize;

	//// SymLoadModule64()
	//typedef DWORD64(__stdcall *tSLM)(IN HANDLE hProcess, IN HANDLE hFile,
	//	IN PSTR ImageName, IN PSTR ModuleName, IN DWORD64 BaseOfDll, IN DWORD SizeOfDll);
	//tSLM pSymLoadModule;

	//// SymSetOptions()
	//typedef DWORD(__stdcall *tSSO)(IN DWORD SymOptions);
	//tSSO pSymSetOptions;

	//// StackWalk64()
	//typedef BOOL(__stdcall *tSW)(
	//	DWORD MachineType,
	//	HANDLE hProcess,
	//	HANDLE hThread,
	//	LPSTACKFRAME64 StackFrame,
	//	PVOID ContextRecord,
	//	PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
	//	PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
	//	PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
	//	PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress);
	//tSW pStackWalk;

	//// UnDecorateSymbolName()
	//typedef DWORD(__stdcall WINAPI *tUDSN)(PCSTR DecoratedName, PSTR UnDecoratedName,
	//	DWORD UndecoratedLength, DWORD Flags);
	//tUDSN pUnDecorateSymbolName;

	////SymGetSearchPath()
	//typedef BOOL(__stdcall WINAPI *tSGSP)(HANDLE hProcess, PSTR SearchPath, DWORD SearchPathLength);
	//tSGSP pSymGetSearchPath;


	static bool symInitialized;

	if (!symInitialized) {

		//auto m_hDbhHelp = LoadLibrary("dbghelp.dll");
		//if (!m_hDbhHelp) return;

		//pSymInitialize = (tSI)GetProcAddress(m_hDbhHelp, "SymInitialize");
		//pSymCleanup = (tSC)GetProcAddress(m_hDbhHelp, "SymCleanup");
		//pStackWalk = (tSW)GetProcAddress(m_hDbhHelp, "StackWalk64");
		//pSymGetOptions = (tSGO)GetProcAddress(m_hDbhHelp, "SymGetOptions");
		//pSymSetOptions = (tSSO)GetProcAddress(m_hDbhHelp, "SymSetOptions");
		//pSymFunctionTableAccess = (tSFTA)GetProcAddress(m_hDbhHelp, "SymFunctionTableAccess64");
		//pSymGetLineFromAddr = (tSGLFA)GetProcAddress(m_hDbhHelp, "SymGetLineFromAddr64");
		//pSymGetModuleBase = (tSGMB)GetProcAddress(m_hDbhHelp, "SymGetModuleBase64");
		////pSymGetModuleInfo = (tSGMI)GetProcAddress(m_hDbhHelp, "SymGetModuleInfo64");
		//pSymGetSymFromAddr = (tSGSFA)GetProcAddress(m_hDbhHelp, "SymGetSymFromAddr64");
		//pUnDecorateSymbolName = (tUDSN)GetProcAddress(m_hDbhHelp, "UnDecorateSymbolName");
		//pSymLoadModule = (tSLM)GetProcAddress(m_hDbhHelp, "SymLoadModule64");
		//pSymGetSearchPath = (tSGSP)GetProcAddress(m_hDbhHelp, "SymGetSearchPath");



		//SAFE_CALL(pSymInitialize(GetCurrentProcess(), NULL, true));

		////auto options = SymGetOptions();
		////DWORD newOptions = options & ~SYMOPT_UNDNAME;
		////newOptions = newOptions | SYMOPT_PUBLICS_ONLY;
		////SymSetOptions(newOptions);

		//auto options = pSymGetOptions();
		////options |= SYMOPT_LOAD_LINES;
		////options |= SYMOPT_FAIL_CRITICAL_ERRORS;

		//SAFE_CALL(pSymSetOptions(options));

		////SAFE_CALL(SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_ALLOW_ABSOLUTE_SYMBOLS | SYMOPT_UNDNAME));
		//symInitialized = true;

		//auto path = R"(.;C:\users-data\saf924000\Documents\Visual Studio 2015\Projects\SolverExample\SolverExample;C:\users-data\saf924000\Documents\Visual Studio 2015\Projects\SolverExample\x64\Debug;C:\WINDOWS;C:\WINDOWS\system32;SRV*C:\websymbols*http://msdl.microsoft.com/download/symbols;)";

		SAFE_CALL(SymInitialize(GetCurrentProcess(), NULL, true));
		//
		////auto options = SymGetOptions();
		////DWORD newOptions = options & ~SYMOPT_UNDNAME;
		////newOptions = newOptions | SYMOPT_PUBLICS_ONLY;
		////SymSetOptions(newOptions);

		//auto options = SymGetOptions();
		//options |= SYMOPT_LOAD_LINES;
		//options |= SYMOPT_FAIL_CRITICAL_ERRORS;

		//SAFE_CALL(SymSetOptions(options));

		////SAFE_CALL(SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_ALLOW_ABSOLUTE_SYMBOLS | SYMOPT_UNDNAME));
		symInitialized = true;
	}
	
	//if (false) {
		//StackWalker sw;
		//sw.ShowCallstack();

		//sw.Test((void*)address);
		////sw.Test((void*)&Problem_Functions<>::MassConservation);
		//sw.Test((void*)0x000000013ffee234);//Problem_Functions<>::MassConservation
		
	//}

	DWORD64 dwAddress = (DWORD64)address;
	DWORD64 dwDisplacement = 0;

	name.Redim(sizeof(IMAGEHLP_SYMBOL64) + MAX_SYM_NAME * sizeof(TCHAR));
	auto pSymbol = (IMAGEHLP_SYMBOL64*)name.GetDataPtr();

	pSymbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
	pSymbol->MaxNameLength = MAX_SYM_NAME;

	auto hCurrentProcess = GetCurrentProcess();

#ifdef _DEBUG

	auto jmp_offset = (*(int*)(dwAddress) >> 8) + 5;
	dwAddress += jmp_offset;

#endif

	//SAFE_CALL(SymGetSymFromAddr64(hCurrentProcess, dwAddress, &dwDisplacement, pSymbol));

	if (!SymGetSymFromAddr64(hCurrentProcess, dwAddress, &dwDisplacement, pSymbol)) {
		if (GetLastError() == 487) {
			name.Redim(0);
			return;
		}
		SAFE_CALL(SymGetSymFromAddr64(hCurrentProcess, dwAddress, &dwDisplacement, pSymbol));
	}

	char undcName[MAX_SYM_NAME];

	char * symbol = pSymbol->Name;

	if (!UnDecorateSymbolName(symbol, undcName, sizeof(undcName), UNDNAME_COMPLETE)) {
		auto grr = GetWin32ErrorDescription(GetLastError());
	}

#ifdef _DEBUG 
	name = symbol;
	//symbol = strchr(symbol, '?') + 1;

	//if (!UnDecorateSymbolName(symbol-1, undcName, sizeof(undcName), UNDNAME_COMPLETE)) {
	//	auto grr = GetErrorDescription(GetLastError());
	//	;
	//}

	//if (symbol) {
	//	auto c = strchr(symbol, '?');
	//	if (c) c[0] = 0;
	//	name = symbol;
	//}
	//else {
	//	name = "error";
	//}
#else
	name = symbol;
	if (symbol) symbol = strchr(symbol, ':');
	if (symbol) name = symbol + 2;
#endif


}

inline bool ResiduePlotStatusText(
	wchar_t * buf,
	size_t bufSize,
	size_t iSerie,
	const wchar_t * serieName,
	double x,
	double y,
	void * data) 
{

	////void ** functions = (void**)data;
	//auto functions = (char**)data;
	auto functions = (char(*)[80])data;

	size_t iFunc = (size_t)x;

	if (iFunc > xVect_Count(functions) - 1) return false;

	//aVect<char> funcName;
	//GetFuncName(functions[iFunc], funcName);
	
	//sprintf_s(buf, bufSize, "function %llu: %s", (unsigned long long)iFunc, funcName.GetDataPtr());
	
	STATIC_ASSERT_SAME_TYPE(char(&)[80], functions[iFunc]);

	swprintf_s(buf, bufSize / sizeof(wchar_t), L"function %llu: %S", (unsigned long long)iFunc, functions[iFunc]);

	return true;
}

struct SpyJacobianStatusTextData {
	//aVect<void*> functions;
	aVect<aVect<char>> funcNames;
	aVect<aVect<char>> varNames;
	aMat<double> values;
};

inline bool SpyJacobianStatusText(
	wchar_t * buf,
	size_t bufSize,
	size_t iSerie,
	const wchar_t * serieName,
	double x,
	double y,
	void * pvData)
{

	auto data = (SpyJacobianStatusTextData*)pvData;
	
	int iFunc = (int)y;
	int iVar  = (int)x;

	if (data->funcNames.OutOfBounds(iFunc)) return false;
	if (data->varNames.OutOfBounds(iFunc)) return false;

	auto&& funcName = data->funcNames[iFunc];

	double value = data->values.OutOfBounds(iFunc, iVar) ? std::numeric_limits<double>::quiet_NaN() : data->values(iFunc, iVar);

	STATIC_ASSERT_SAME_TYPE(aVect<char>&, data->varNames[0]);
	STATIC_ASSERT_SAME_TYPE(aVect<char>&, funcName);

	if ((size_t)iVar < data->varNames.Count()) {
		swprintf_s(buf, bufSize / sizeof(wchar_t), L"var %d: %S, func %d: %S, value %g", iVar, data->varNames[iVar].GetDataPtr(), iFunc, funcName.GetDataPtr(), value);
	} else {
		swprintf_s(buf, bufSize / sizeof(wchar_t), L"var %d, func %d: %S, value %g", iVar, iFunc, funcName.GetDataPtr(), value);
	}

	return true;
}

static void SpyJacobianStatusTextCleanUp(AxisHandle axis, void * callbacksData) {
	auto ptr = (SpyJacobianStatusTextData*)callbacksData;
	delete ptr;
}

template <class T>
void SpyMatrix(const aMat<T> & M, char * title = nullptr, bool toConsole = false, bool colorLogScale = false, AxisClass * pAxis = nullptr) {

	//static aMat<float> M2;
	aMat<float> M2;
	M2.Redim(M.nRow(), M.nCol());

	aMat_static_for(M, i, j) {
		float val = (float)M(i, j);
		M2(i, j) = val ? colorLogScale ? log(abs(val)) : val : std::numeric_limits<float>::quiet_NaN();
	}

	if (toConsole) DisplayMatrix(M);

	AxisClass localAxis;

	AxisClass& axis = pAxis ? *pAxis : localAxis;
	if (!axis) axis.Create();
	axis.CreateMatrixView(M2);
	if (title) axis.SetTitle(Wide(title));
	axis.AutoFit().Refresh();
}

//template <> inline 
//void SpyMatrix<mpf_class>(aMat<mpf_class> & M, char * title) {
//
//	//static aMat<float> M2;
//	aMat<float> M2;
//	M2.Redim(M.nRow(), M.nCol());
//
//	aMat_static_for(M, i, j) {
//		float val = (float)M(i, j).get_d();
//		M2(i, j) = val ? val : std::numeric_limits<float>::quiet_NaN();
//	}
//
//	AxisClass axis;
//	axis.Create().CreateMatrixView(M2);
//	if (title) axis.SetTitle(title);
//	axis.AutoFit().RefreshAsync();
//
//	DisplayMatrix(M);
//}


//template <class T> mpf_ptr get_mpf_t(T & f) {
//	mpf_ptr a = nullptr;
//	return a;
//}
//
//template <> inline mpf_ptr get_mpf_t<mpf_class>(mpf_class & f) {
//	mpf_ptr a = f.get_mpf_t();
//	return a;
//}
//
//inline void print_mpf(mpf_class & f) {
//	gmp_printf("%.Fg\n", f.get_mpf_t());
//}


//unsigned long long factorial(unsigned long long n) {
//	unsigned long long val = 1;
//	for (unsigned long long i = 2; i <= n; ++i) val *= i;
//	return val;
//}

template <unsigned prec> struct SetMpfPrecFunctor {
	template <class T> SetMpfPrecFunctor(aVect<T> & a) {
		aVect_static_for(a, i) a[i].set_prec(prec);
	}
	template <class T> SetMpfPrecFunctor(aMat<T> & a) {
		aVect_static_for(a, i) a[i].set_prec(prec);
	}
	template <class T> SetMpfPrecFunctor(T & a) {
		a.set_prec(prec);
	}
};

template <unsigned prec, bool errorIfSingular = true>
bool LUP_Solve_gmp(aVect<double> & X, const aMat<double> & A, const aVect<double> & B) {

	//mpf_set_default_prec(5000);

	// static aVect<mpf_class> gmp_X;
	// static aMat<mpf_class>  gmp_A;
	// static aVect<mpf_class> gmp_B;
	aVect<mpf_class> gmp_X;
	aMat<mpf_class>  gmp_A;
	aVect<mpf_class> gmp_B;

	if (gmp_X.Count() != B.Count()) {
		gmp_X.Redim(B.Count());
		aVect_static_for(gmp_X, i) gmp_X[i].set_prec(prec);
	}

	if (gmp_B.Count() != B.Count()) {
		gmp_B.Redim(B.Count());
		aVect_static_for(gmp_B, i) gmp_B[i].set_prec(prec);
	}

	if (gmp_A.nRow() != A.nRow() || gmp_A.nRow() != A.nRow()) {
		gmp_A.Redim(A.nRow(), A.nCol());
		aMat_static_for(gmp_A, i, j) gmp_A(i, j).set_prec(prec);
	}

	//DisplayMatrix(A, 20, "mat.txt");
	//DisplayMatrix(gmp_A, -1, "gmp_mat.txt");

	//aVect_static_for(gmp_X, i) gmp_X[i] = X[i];
	aVect_static_for(gmp_B, i) gmp_B[i] = B[i];
	aMat_static_for(gmp_A, i, j) gmp_A(i, j) = A(i, j);

	//DisplayMatrix(A, 20, "mat.txt");
	//DisplayMatrix(gmp_A, -1, "gmp_mat.txt");
	//gmp_printf("A(i, k) = %.Fg\n", get_mpf_t(A(i, k)));

	bool retVal = LUP_Solve<errorIfSingular, SetMpfPrecFunctor<prec> >(gmp_X, gmp_A, gmp_B);

	X.Redim(gmp_X.Count());
	aVect_static_for(gmp_X, i) X[i] = gmp_X[i].get_d();

	return retVal;
}

template <class T>
void factorial(T & val, unsigned n) {
	val = 1;
	for (unsigned i = 2; i <= n; ++i) val *= i;
}


template <class T> double get_val(T & val) {
	return val;
}

//template <> inline double get_val<mpf_class>(mpf_class & val) {
//	return val.get_d();
//}

template <class T, class U> void compute_pow(T & res, T & val, U power) {
	res = pow(val, power);
}

//template <> inline void compute_pow<mpf_class, size_t>(mpf_class & res, mpf_class & val, size_t power) {
//	mpf_pow_ui(res.get_mpf_t(), val.get_mpf_t(), (mpir_ui)power);
//}

inline void TrouverUnNom(int(&ind)[1], int i) {
	ind[0] = i;
}

template <class... Args>
void TrouverUnNom(int(&ind)[1], int i, aVect<int> & varIndices, Args&&... args) {

	static_assert(sizeof...(args) == 0, "oops");
	ind[0] = varIndices[i];
}

template <int N, class... Args>
void TrouverUnNom(int(&ind)[N], int i, aVect<int> & varIndices, Args&&... args) {
	ind[0] = varIndices[i];
	int * next = &ind[1];
	TrouverUnNom(((int(&)[N - 1]) *next), i, std::forward<Args>(args)...);
}


template <int N>
void TrouverUnNom(int(&ind)[N], int i) {
	ind[0] = i;
	int * next = &ind[1];
	TrouverUnNom(((int(&)[N - 1]) *next), i);
}

//
//template <
//	class T = double,
//	class Functor = EmptyClass,
//	class Conv,
//	class UserDomain,
//	class... Args>
//		bool DerivativeConvolutionCoefficientsCore(
//		Conv & convolution,
//		bool centroid,
//		const Domain<UserDomain> & domain,
//		size_t order,
//		size_t p,
//		double dx,
//		size_t nPoints,
//		bool include_boundary_inf = false,
//		bool include_boundary_sup = false,
//		bool leapBoundary_inf = false,
//		bool leapBoundary_sup = false,
//		int offset = 0,
//		Args&&... args) {
//		//aVect<size_t> & varIndices = aVect<size_t>()) {
//
//		//if (nPoints % 2 != 1) MY_ERROR("TODO : even N");
//		if (nPoints > domain.GetMesh().Count()) MY_ERROR("not enough points in mesh");
//		if (nPoints < order + 1) MY_ERROR("derivative order require more points");
//
//		static aMat<T> A;
//		static aVect<size_t> ind;
//		static aVect<T> B;
//		static aVect<T> X;
//
//		auto & mesh = centroid ? domain.GetCentroids() : domain.GetMesh();
//		auto & leapMesh = !centroid ? domain.GetCentroids() : domain.GetMesh();
//
//		size_t N = mesh.Count();
//
//		if (false) {
//			size_t leftPoints = (nPoints - 1) / 2;
//			size_t rightPoints = leftPoints;
//
//			if (p + rightPoints >= N) rightPoints = N - 1 - p;
//			if (p - leftPoints >= N)  leftPoints = p;
//
//			if (leftPoints + rightPoints + 1 != nPoints) {
//				if (leftPoints + rightPoints + 1 >= order + 1) {
//					nPoints = leftPoints + rightPoints + 1;
//				}
//			}
//		}
//
//		ind.Redim(nPoints);
//
//		bool isBoundary_sup = false, isBoundary_inf = false;
//
//		if (p - nPoints / 2 + offset >= N) isBoundary_inf = true;
//		else if (p + (nPoints + 1) / 2 + offset > N) isBoundary_sup = true;
//
//		if (p - nPoints / 2 + offset >= N) {
//			for (size_t i = 0; i < nPoints; ++i) ind[i] = i;
//		}
//		else if (p + nPoints / 2 + offset >= N) {
//			for (size_t i = 0; i < nPoints; ++i) ind[i] = i + N - nPoints;
//		}
//		else {
//			for (size_t i = 0; i < nPoints; ++i) ind[i] = i + p - nPoints / 2 + offset;
//		}
//
//		aVect_static_for(ind, i) if (ind[i] >= N) MY_ERROR("gotcha");
//
//		//double maxNegativeDistance = 0;
//		//double maxPositiveDistance = 0;
//
//		//aVect_static_for(ind, i) {
//		//	double delta_x = mesh[ind[i]] - mesh[p] + dx;
//		//	if (delta_x > maxPositiveDistance) maxPositiveDistance = dx;
//		//	if (delta_x < -maxNegativeDistance) maxNegativeDistance = -dx;
//		//}
//
//		//double maxDistance = Min(maxPositiveDistance, maxNegativeDistance);
//
//		//aVect_for_inv(ind, i) {
//		//	double delta_x = mesh[ind[i]] - mesh[p] + dx;
//		//	if (delta_x > maxDistance || delta_x < -maxDistance) {
//		//		if (nPoints > order + 1) {
//		//			ind.Remove(i);
//		//			nPoints--;
//		//		}
//		//	}
//		//}
//
//		A.Redim(nPoints, nPoints);
//		B.Redim(nPoints);
//
//		T val, dist;
//
//		(Functor)val;
//		(Functor)dist;
//		(Functor)A;
//		(Functor)B;
//		(Functor)X;
//
//		B.Set(0)[order] = 1;
//
//		short dec_ind = 0;
//		if (isBoundary_inf && leapBoundary_inf) dec_ind = -1;
//		if (isBoundary_sup && leapBoundary_sup) dec_ind = 1;
//
//		convolution.points.Redim(nPoints);
//
//#ifdef _DEBUG
//
//		convolution.xOrig = mesh[p] + dx;
//
//		double distance;
//
//		for (size_t j = 0; j < nPoints; ++j) {
//			if (j == 0 && dec_ind == -1) {
//				distance = leapMesh[0] - mesh[p] - dx;
//				convolution.points[j].x = leapMesh[0];
//				convolution.points[j].iMesh = -1;
//			}
//			else if (j == nPoints - 1 && dec_ind == 1) {
//				distance = leapMesh.Last() - mesh[p] - dx;
//				convolution.points[j].x = leapMesh.Last();
//				convolution.points[j].iMesh = -1;
//			}
//			else {
//				distance = mesh[ind[j] + dec_ind] - mesh[p] - dx;
//				convolution.points[j].x = mesh[ind[j] + dec_ind];
//				convolution.points[j].iMesh = ind[j] + dec_ind;
//			}
//			convolution.points[j].dist = distance;
//		}
//
//#endif
//
//		aMat_static_for(A, i, j) {
//			factorial(val, i);
//			if (j == 0 && dec_ind == -1)
//				dist = leapMesh[0] - mesh[p] - dx;
//			else if (j == nPoints - 1 && dec_ind == 1)
//				dist = leapMesh.Last() - mesh[p] - dx;
//			else
//				dist = mesh[ind[j] + dec_ind] - mesh[p] - dx;
//			compute_pow(dist, dist, i);
//			A(i, j) = dist / val;
//		}
//
//		//LUP_Solve_gmp<prec>(X, A, B);
//		LUP_Solve<true, Functor, T>(X, A, B);
//
//		double res = CheckLinearSolve(A, X, B);
//		if (res > 1e-14) return false;// MY_ERROR("could be better");
//
//		//if (yes) DisplayMatrix(A, -1, "mpf_mat.txt");
//
//
//
//		aVect_static_for(convolution.points, i) {
//			if ((ind[i] > 0 || include_boundary_inf) && (ind[i] < N - 1 || include_boundary_sup)) {
//				size_t ind_i = include_boundary_inf ? ind[i] : ind[i] - 1;
//				if (isBoundary_sup && leapBoundary_sup) ind_i++;
//				if (isBoundary_inf && leapBoundary_inf) ind_i--;
//				if (ind_i > N - 1) {
//					TrouverUnNom(convolution.points[i].ind, -1);
//				}
//				else {
//					TrouverUnNom(convolution.points[i].ind, ind_i, std::forward<Args>(args)...);
//				}
//				//convolution.points[i].ind = 
//				//if (varIndices) convolution.points[i].ind = varIndices[convolution.points[i].ind];
//			}
//			else {
//				TrouverUnNom(convolution.points[i].ind, -1);
//			}
//			convolution.points[i].coef = get_val(X[i]);
//		}
//
//		return true;
//	}
//
//	template <class... Args >
//	void DerivativeConvolutionCoefficients(Args&&... args) {
//		if (DerivativeConvolutionCoefficientsCore<double>(std::forward<Args>(args)...)) return;
//		if (DerivativeConvolutionCoefficientsCore<mpf_class, SetMpfPrecFunctor<128> >(std::forward<Args>(args)...)) return;
//		if (DerivativeConvolutionCoefficientsCore<mpf_class, SetMpfPrecFunctor<256> >(std::forward<Args>(args)...)) return;
//		if (DerivativeConvolutionCoefficientsCore<mpf_class, SetMpfPrecFunctor<521> >(std::forward<Args>(args)...)) return;
//		if (DerivativeConvolutionCoefficientsCore<mpf_class, SetMpfPrecFunctor<1024> >(std::forward<Args>(args)...)) return;
//		MY_ERROR("Unable to solve linear system");
//	}

template <class T>
double CheckLinearSolve(const aMat<T> & A, aVect<T> & x, const aVect<T> & b) {

	//static T sum;
	//static T norm;

	T sum;
	T norm;

	//sum.~T();
	//norm.~T();

	//new (&sum) T(b[0]);
	//new (&norm) T(sum);

	norm = 0;

	for (int i = 0, I = (int)A.nRow(); i < I; ++i) {

		sum = 0;
		for (int j = 0, J = (int)A.nCol(); j < J; ++j) {
			sum += A(i, j) * x[j];
		}
		sum -= b[i];
		norm += sum*sum;
	}

	norm = sqrt(norm);

	return get_val(norm);
}


template <bool abortIfSingular = true, class Functor = EmptyClass, class T>
bool LUP_Decomposition(aMat<T> & A, aVect<size_t> & P) {

	size_t N = A.nRow();

	P.Redim(N);

	for (size_t i = 0; i < N; ++i) P[i] = i;

	bool isSingular = false;

	T p/*, tmp*/;
	(Functor)p;
	//(Functor)tmp;

	for (size_t k = 0; k < N; ++k) {

		p = 0;

		size_t k2;
		for (size_t i = k; i < N; ++i) {
			if (abs(A(i, k)) > p) {
				p = abs(A(i, k));
				k2 = i;
			}
		}
		if (p == 0) {
			isSingular = true;
			if (abortIfSingular) return false;
		}
		else {
			if (k != k2) {
				Swap(P[k], P[k2]);

				for (size_t i = 0; i < N; ++i) {
					Swap(A(k, i), A(k2, i));
					//tmp     = A(k, i);
					//A(k, i) = A(k2, i);
					//A(k2, i) = tmp;
				}
			}
		}
		for (size_t i = k + 1; i < N; ++i) {
			A(i, k) /= A(k, k);
			for (size_t j = k + 1; j < N; ++j) {
				A(i, j) -= A(i, k)*A(k, j);
			}
		}
	}

	return !isSingular;
}

template <class Functor = EmptyClass, class T>
void LUP_Substitute(aVect<T> & x, aMat<T> & A, const aVect<size_t> & P, const aVect<T> & b) {

	auto N = A.nRow();

	static WinCriticalSection cs;

	Critical_Section(cs) {
		static aVect<T> y;
		//aVect<T> y;

		if (x.Count() != N) {
			x.Redim(N);
			(Functor)x;
		}

		if (y.Count() != N) {
			y.Redim(N);
			(Functor)y;
		}

		for (size_t i = 0; i < N; ++i) {
			y[i] = b[P[i]];
			for (size_t j = 0; j < i; ++j) {
				y[i] -= A(i, j) * y[j];
			}
		}

		for (size_t i = N - 1; i < N; --i) {
			x[i] = y[i];
			for (size_t j = i + 1; j < N; ++j) {
				x[i] -= A(i, j) * x[j];
			}
			x[i] /= A(i, i);
		}
	}
}

template <bool errorIfSingular = true, class Functor = EmptyClass, class T>
bool LUP_Solve(aVect<T> & x, const aMat<T> & A_orig, const aVect<T> & b) {

	// static aVect<size_t> P;
	// static aMat<T> A;
	aVect<size_t> P;
	aMat<T> A;

	if (A_orig.nRow() != A_orig.nCol()) MY_ERROR("matrix must be square");

	A.Redim(A_orig.nRow(), A_orig.nCol());
	//memcpy(A.GetDataPtr(), A_orig.GetDataPtr(), A.Count() * sizeof(T));

	(Functor)A;

	aMat_static_for(A, i, j) A(i, j) = A_orig(i, j);
	//A.Copy(A_orig);

	if (!LUP_Decomposition<false, Functor>(A, P)) {
		if (errorIfSingular) {
			SpyMatrix(A, "Singular jacobian");
			DisplayMatrix(A);
			MY_ERROR("matrix is singular");
		}
		else return false;
	}
	LUP_Substitute<Functor>(x, A, P, b);

	//A.Leak();

	return true;
}

/*
#ifdef _DEBUG

	template <
		class UserDomain,
			unsigned N,
			size_t hack,
			bool boundary_Inf,
			bool boundary_Sup>
			void OutputConvolutionCoefficient(
			const aVect<double> & mesh,
			const aVect<Convolution<UserDomain, N, hack, boundary_Inf, boundary_Sup> > & conv,
			char * fileName)
		{

			size_t maxCount = 0;

			aVect_static_for(conv, i) {
				if (conv[i].points.Count() > maxCount) maxCount = conv[i].points.Count();
			}

			aMat<aVect<char> > coef(maxCount, conv.Count());
			aMat<aVect<char> > ind[N];
			aMat<aVect<char> > iMesh(maxCount, conv.Count());
			aMat<aVect<char> > x(maxCount, conv.Count());
			aMat<aVect<char> > dist(maxCount, conv.Count());
			aVect<aVect<char> > columnTitles(conv.Count());

			for (size_t k = 0; k < N; ++k) ind[k].Redim(maxCount, conv.Count());

			aVect_static_for(conv, i) {
				if (!conv[i].points) MY_ERROR("empty");
				aVect_static_for(conv[i].points, j) {
					coef(j, i).sprintf("%25.16g", conv[i].points[j].coef);
					for (size_t k = 0; k < N; ++k) ind[k](j, i).sprintf("%25d", conv[i].points[j].ind[k]);
					x(j, i).sprintf("%25.16g", conv[i].points[j].x);
					dist(j, i).sprintf("%25.16g", conv[i].points[j].dist);
					iMesh(j, i).sprintf("%25d", conv[i].points[j].iMesh);
				}
				columnTitles[i].sprintf("%25.16g", conv[i].xOrig);
			}

			File file(fileName);

			file.Write("Origin:\n");

			aVect_static_for(columnTitles, i) {
				file.Write(columnTitles[i]);
				file.Write("\t");
			}

			file.Write("\nCoefficients:\n");

			for (size_t i = 0, I = coef.nRow(); i < I; ++i) {
				for (size_t j = 0, J = coef.nCol(); j < J; ++j) {
					file.Write(coef(i, j));
					file.Write("\t");
				}
				file.Write("\n");
			}

			file.Write("Indices:\n");

			for (size_t k = 0; k < N; ++k) {
				for (size_t i = 0, I = coef.nRow(); i < I; ++i) {
					for (size_t j = 0, J = coef.nCol(); j < J; ++j) {
						file.Write(ind[k](i, j));
						file.Write("\t");
					}
					file.Write("\n");
				}
				file.Write("\n");
			}

			file.Write("Mesh index:\n");

			for (size_t i = 0, I = coef.nRow(); i < I; ++i) {
				for (size_t j = 0, J = coef.nCol(); j < J; ++j) {
					file.Write(iMesh(i, j));
					file.Write("\t");
				}
				file.Write("\n");
			}

			file.Write("Positions:\n");

			for (size_t i = 0, I = coef.nRow(); i < I; ++i) {
				for (size_t j = 0, J = coef.nCol(); j < J; ++j) {
					file.Write(x(i, j));
					file.Write("\t");
				}
				file.Write("\n");
			}

			file.Write("Distances from origin:\n");

			for (size_t k = 0; k < N; ++k) {
				for (size_t i = 0, I = coef.nRow(); i < I; ++i) {
					for (size_t j = 0, J = coef.nCol(); j < J; ++j) {
						file.Write(dist(i, j));
						file.Write("\t");
					}
					file.Write("\n");
				}
				file.Write("\n");
			}
		}
#endif*/

#endif //_NEWTONSOLVER_
