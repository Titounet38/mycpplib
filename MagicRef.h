#ifndef _MagicRef_
#define _MagicRef_

#include "xVect.h"
#include "MyUtils.h"
#include "WinUtils.h"

#define MAGICREF_N_CACHED_POINTERS 3
//#define SLOW_MAGIC_POINTERS 100

//#define DBG_CHECK_ACTIVE

#define GENERATE_BINARY_OPERATOR(var, op)			\
template <class>									\
auto operator op(T val) -> decltype(var op val){	\
	return this->var op val;						\
}

#define GENERATE_PRE_UNARY_OPERATOR(var, op)	\
template <class>								\
auto operator op() -> decltype(op var){			\
	return op this->var;						\
}

#define GENERATE_POST_UNARY_OPERATOR(var, op)	\
template <class>								\
auto operator op(int) -> decltype(var op){		\
	return this->var op;						\
}

#define GENERATE_BINARY_OPERATOR2(T, op)				\
template <class>										\
auto operator op(T val) -> decltype(*(T*)0 op va){		\
	return *(T*)this op val;							\
}

#define GENERATE_PRE_UNARY_OPERATOR2(T, op)			\
template <class>									\
auto operator op() -> decltype(op (*(T*)0)){		\
	return op (*(T*)this);							\
}

#define GENERATE_POST_UNARY_OPERATOR2(T, op)		\
template <class>									\
auto operator op(int) -> decltype((*(T*)0) op){		\
	return (*(T*)this) op;							\
}
//
//template <class N, class T>
//class aVectCached : public aVect <T> {
//
//	size_t n;
//	T cached[N];
//
//	size_t Count() {
//		if (this->n < N) return this->n;
//		return N + aVect::Count();
//	}
//
//	T & operator[](size_t i) {
//		if (i < this->n) return this->cached[i];
//		if (this->n < N) MY_ERROR("out of range");
//		return aVect::operator[](i - N);
//	}
//
//	template <Args...>
//	aVectCached & Push(Args&&... args) {
//		if (this->n < N) {
//			new (&this->cached[this->n], NoNullCheck()) T(std::forward<Args>(args)...);
//			this->n++;
//		}
//		else {
//			aVect::Push(std::forward<Args>(args)...);
//		}
//		return *this;
//	}
//
//	T Pop(size_t i) {
//		if (aVect::Count()) return aVect::Pop(i);
//#if defined(_DEBUG) || defined(MY_DEBUG) 
//		if (!this->n) MY_ERROR("empty vector");
//#endif
//		T el(std::move(this->cached[this->n - 1]));
//		this->cached[this->n - 1].~T();
//		this->n--;
//		return el;
//	}
//};

//extern WinCriticalSection g_MagicCopy_cs;
extern WinCriticalSection g_Magic_cs;

struct MagicPointer_CopyOp {

	bool builtin_type;
	void * This;
	void * That;

	MagicPointer_CopyOp(void * This, void * That, bool builtin_type) :
		This(This),
		That(That),
		builtin_type(builtin_type)
	{}
};

extern size_t g_MagicTarget_CopyRecursion;
extern aVect<MagicPointer_CopyOp> g_MagicPointer_CopyOp;
extern aVect<void**> g_MagicPointer_CopiedTo_ClearList;

void MagicCopyIncrement();

struct MagicCopyIncrementer {

	MagicCopyIncrementer() {}
	MagicCopyIncrementer(MagicCopyIncrementer && that) {}

	MagicCopyIncrementer(const MagicCopyIncrementer & that) {
		MagicCopyIncrement();
	}

	MagicCopyIncrementer & operator=(const MagicCopyIncrementer & that) {
		MagicCopyIncrement();
		return *this;
	}
	MagicCopyIncrementer & operator=(MagicCopyIncrementer && that) = default;
};

struct MagicCopyDecrementer {

	void FixCopyOp() {

		Critical_Section(g_Magic_cs) {

			if (g_MagicTarget_CopyRecursion == 0) MY_ERROR("gotcha");

			g_MagicTarget_CopyRecursion--;

			if (g_MagicTarget_CopyRecursion == 0) {

				g_Magic_cs.Leave();

				if (g_MagicPointer_CopiedTo_ClearList || g_MagicPointer_CopyOp) {

#ifdef SLOW_MAGIC_POINTERS
					Sleep(SLOW_MAGIC_POINTERS);
#endif
					size_t i = -1;
					for (auto&& copyOp : g_MagicPointer_CopyOp) {
						i++;
						if (copyOp.builtin_type) {
							this->FixPointers<true>(copyOp);
						}
						else {
							this->FixPointers<false>(copyOp);
						}
					}

					g_MagicPointer_CopyOp.Redim<false>(0);

					for (auto&& ptrToClear : g_MagicPointer_CopiedTo_ClearList) {
						*ptrToClear = nullptr;
					}
					g_MagicPointer_CopiedTo_ClearList.Redim(0);
				}
			}
		} 
	}

	MagicCopyDecrementer() {}
	MagicCopyDecrementer(MagicCopyDecrementer && that) {}

	MagicCopyDecrementer(const MagicCopyDecrementer & that) {

		this->FixCopyOp();
	}

	MagicCopyDecrementer & operator=(const MagicCopyDecrementer & that) {
		this->FixCopyOp();
		return *this;
	}

	MagicCopyDecrementer & operator=(MagicCopyDecrementer && that) = default;

	template <bool isBuiltin_type>
	void FixPointers(MagicPointer_CopyOp & copyOp); 
};

template<typename T>
struct non_void_fundamental : 
std::integral_constant<
	bool,
	std::is_fundamental<T>::value && !std::is_same<T, void>::value>
{};

template <class T, bool builtin_type = non_void_fundamental<T>::value | std::is_pointer<T>::value>
class MagicPointer;

template <class T, bool builtin_type = non_void_fundamental<T>::value | std::is_pointer<T>::value>
class MagicTarget;

template <class T, bool builtin_type>
struct MagicPointerBase {

	typedef MagicPointer<T, builtin_type> Derived;

	

	void Attach(T * _ptr) {

		Critical_Section(g_Magic_cs) {

			MagicTarget<T, builtin_type> * ptr = (MagicTarget<T, builtin_type> *)_ptr;
			Derived * dThis = ((Derived*)this);

			dThis->ptr = (decltype(dThis->ptr))ptr;

			if (!dThis->ptr) return;

#ifdef DBG_CHECK_ACTIVE
			ptr->DBG_CHECK();
#endif

			size_t nCachedPointers = ptr->nCachedPointers;

			while (true) {
				if (ptr->nCachedFreeList) {
					size_t freeInd = ptr->freeList ?
						ptr->freeList.Pop() : 
						ptr->cachedFreeList[--ptr->nCachedFreeList];
					if (freeInd < NUMEL(ptr->cachedPointers)) {
						if (freeInd < nCachedPointers) {
							ptr->cachedPointers[freeInd] = dThis;
							dThis->selfIndex = freeInd;
#ifdef DBG_CHECK_ACTIVE
							ptr->DBG_CHECK();
#endif
							return;
						}
						else MY_ERROR("invalid free index");
						//old: else freeInd is no longer valid, continue
						//now: removed pointer vect auto shrink
					}
					else {
						size_t ind = freeInd - NUMEL(ptr->cachedPointers);
						if (ind < ptr->pointers.Count()) {
							ptr->pointers[ind] = dThis;
							dThis->selfIndex = freeInd;
#ifdef DBG_CHECK_ACTIVE
							ptr->DBG_CHECK();
#endif
							return;
						}
						else MY_ERROR("invalid free index");
						//old: else freeInd is no longer valid, continue
						//now: removed pointer vect auto shrink
					}
				}
				else  break;
			}


			if (ptr->nCachedPointers < NUMEL(ptr->cachedPointers)) {
				dThis->selfIndex = ptr->nCachedPointers;
				ptr->cachedPointers[ptr->nCachedPointers++] = dThis;
			}
			else {
				dThis->selfIndex = NUMEL(ptr->cachedPointers) + ptr->pointers.Count();
#ifdef SLOW_MAGIC_POINTERS
				if (ptr->pointers.Count() == ptr->pointers.AllocatedCount()) Sleep(SLOW_MAGIC_POINTERS);
#endif
				ptr->pointers.Push(dThis); 
			}

#ifdef DBG_CHECK_ACTIVE
			ptr->DBG_CHECK();
#endif
		}
	}

	void Detach() {

//#ifdef SLOW_MAGIC_POINTERS
//		Sleep(10);
//#endif

		Critical_Section(g_Magic_cs) {
			Derived * dThis = ((Derived*)this);
			MagicTarget<T, builtin_type> * ptr = (MagicTarget<T, builtin_type> *)dThis->ptr;

#ifdef DBG_CHECK_ACTIVE
			ptr->DBG_CHECK();
#endif

			size_t selfIndex = dThis->selfIndex;
			size_t nCachedPointers = ptr->nCachedPointers;

			if (selfIndex < NUMEL(ptr->cachedPointers)) {
				ptr->cachedPointers[selfIndex] = nullptr;
				//if (selfIndex == nCachedPointers - 1 && !ptr->pointers) {
				//	size_t i = nCachedPointers - 2;
				//	for (; i < nCachedPointers; --i) {
				//		if (ptr->cachedPointers[i]) break;
				//	}
				//	ptr->nCachedPointers = i + 1;
				//}
				if (selfIndex < ptr->nCachedPointers + ptr->pointers.Count()) {//duplicate code (see below): todo: write a push function
					if (ptr->nCachedFreeList < NUMEL(ptr->cachedFreeList)) {
						ptr->cachedFreeList[ptr->nCachedFreeList] = selfIndex;
						ptr->nCachedFreeList++;
					}
					else {
#ifdef SLOW_MAGIC_POINTERS
						if (ptr->freeList.Count() == ptr->freeList.AllocatedCount()) Sleep(SLOW_MAGIC_POINTERS);
#endif
						ptr->freeList.Push(selfIndex);
					}
				}
			}
			else {
				size_t ind = selfIndex - NUMEL(ptr->cachedPointers);
				ptr->pointers[ind] = nullptr;
				//size_t I = ptr->pointers.Count();
				//if (ind == I - 1) {
				//	size_t i = I - 2;
				//	for (; i < I; --i) {
				//		if (ptr->pointers[i]) break;
				//	}
				//	ptr->pointers.Redim(i + 1);
				//}
				if (ind < ptr->pointers.Count()) {
					if (selfIndex < ptr->nCachedPointers + ptr->pointers.Count()) {//duplicate code (see above): todo: write a push function
						if (ptr->nCachedFreeList < NUMEL(ptr->cachedFreeList)) {
							ptr->cachedFreeList[ptr->nCachedFreeList] = selfIndex;
							ptr->nCachedFreeList++;
						}
						else {
#ifdef SLOW_MAGIC_POINTERS
							if (ptr->freeList.Count() == ptr->freeList.AllocatedCount()) Sleep(SLOW_MAGIC_POINTERS);
#endif
							ptr->freeList.Push(selfIndex);
						}
					}
				}
			}

#ifdef DBG_CHECK_ACTIVE
			ptr->DBG_CHECK();
#endif
		}
	}

	void Move(MagicPointer<T, builtin_type> & that) {

//#ifdef SLOW_MAGIC_POINTERS
//		Sleep(10);
//#endif

		Critical_Section(g_Magic_cs) {
			Derived * dThis = ((Derived*)this);
			dThis->ptr = that.ptr;
			dThis->selfIndex = that.selfIndex;

			if (!dThis->ptr) return;

			that.ptr = nullptr;

			MagicTarget<T, builtin_type> * ptr = (MagicTarget<T, builtin_type> *)dThis->ptr;

#ifdef DBG_CHECK_ACTIVE
			ptr->DBG_CHECK();
#endif

			if (dThis->selfIndex < NUMEL(ptr->cachedPointers)) {
				ptr->cachedPointers[dThis->selfIndex] = dThis;
			}
			else {
				size_t ind = dThis->selfIndex - NUMEL(ptr->cachedPointers);
				ptr->pointers[ind] = dThis;
			}

#ifdef DBG_CHECK_ACTIVE
			ptr->DBG_CHECK();
#endif
		}
	}

	void Copy(const MagicPointer<T, builtin_type> & that) {

//#ifdef SLOW_MAGIC_POINTERS
//		Sleep(10);
//#endif

		Critical_Section(g_Magic_cs) {
			Derived * dThis = ((Derived*)this);

			if (g_MagicTarget_CopyRecursion > 0) {
				g_MagicPointer_CopyOp.Push((void**)dThis, (void**)that.ptr, builtin_type);
			}
			else {
				dThis->Attach((T*)that.ptr);
			}
		}
	}
};


//template <class T, bool builtin_type>
//WinCriticalSection MagicPointerBase<T, builtin_type>::cs;


template <class T, bool builtin_type>
struct MagicTargetBase {

	typedef MagicTarget<T, builtin_type> Derived;

	void Detach() {

		Critical_Section(g_Magic_cs) {
			Derived * dThis = ((Derived*)this);

#ifdef DBG_CHECK_ACTIVE
			dThis->DBG_CHECK();
#endif

			for (size_t i = 0; i < dThis->nCachedPointers; ++i) {
				auto && ref = dThis->cachedPointers[i];
				if (ref) {
					ref->ptr = nullptr;
				}
			}

			for (auto&& ref : dThis->pointers) {
				if (ref) {
					ref->ptr = nullptr;
				}
			}
		}
	}

	void Move(MagicTarget<T, builtin_type> & that) {

		Critical_Section(g_Magic_cs) {
			Derived * dThis = ((Derived*)this);

			dThis->nCachedPointers = that.nCachedPointers;
			dThis->nCachedFreeList = that.nCachedFreeList;
			dThis->beingCopiedTo = that.beingCopiedTo;
			that.beingCopiedTo = nullptr;
			that.nCachedPointers = 0;
			that.nCachedFreeList = 0;

#ifdef DBG_CHECK_ACTIVE
			dThis->DBG_CHECK();
#endif

			for (size_t i = 0; i < dThis->nCachedPointers; ++i) {
				auto && ref = that.cachedPointers[i];
				dThis->cachedPointers[i] = ref;
				if (ref) ref->ptr = (decltype(ref->ptr))dThis;
			}

			if (that.pointers) {
				dThis->pointers.Steal(that.pointers);
				for (auto&& p : dThis->pointers) if (p) p->ptr = (decltype(p->ptr))dThis;
			}

			for (size_t i = 0; i < dThis->nCachedFreeList; ++i) {
				dThis->cachedFreeList[i] = that.cachedFreeList[i];
			}

			if (that.freeList) {
				dThis->freeList.Steal(that.freeList);
			}
		}
	}

	void Copy(const MagicTarget<T, builtin_type> & that) {

		Critical_Section(g_Magic_cs) {
			Derived * dThis = ((Derived*)this);

#ifdef DBG_CHECK_ACTIVE
			dThis->DBG_CHECK();
#endif

			if (g_MagicTarget_CopyRecursion > 0) {
				*(void**)&that.beingCopiedTo = dThis;
				g_MagicPointer_CopiedTo_ClearList.Push((void**)&that.beingCopiedTo);
			}
		}
	}
};

template <class T>
struct TestEmbeddedSize {
	T test;
	int a;
};

template <class T>
struct TestPadding {
	double padding;
	T a;
};

//static TestPadding<MagicCopyDecrementer> compilerTricker[20];

CREATE_MEMBER_DETECTOR(copyDecrementer);
CREATE_MEMBER_DETECTOR(copyIncrementer);

template <class T, bool builtin_type>
class MagicPointer {

	friend class MagicTarget <T, builtin_type>;
	friend struct MagicCopyDecrementer;
	friend struct MagicPointerBase <T, builtin_type>;
	friend struct MagicTargetBase  <T, builtin_type>;

	typedef MagicPointerBase<T, builtin_type> MagicBase;

	T * ptr;
	size_t selfIndex;

	void Attach(T * _ptr)		   { ((MagicBase*)this)->Attach(_ptr); }
	void Detach()				   { ((MagicBase*)this)->Detach();	  }
	void Move(MagicPointer & that) { ((MagicBase*)this)->Move(that);	  }
	void Copy(const MagicPointer & that) { ((MagicBase*)this)->Copy(that);	  }

	
	void static StaticErrorCheck() {

		static_assert(Detect_Member_copyDecrementer<T>::value, "class must have a copyDecrementer member");
		static_assert(!Detect_Member_copyIncrementer<T>::value, "class must not have a copyIncrementer member");

		static_assert(std::is_same<decltype(((T*)0)->copyDecrementer), MagicCopyDecrementer>::value, "copyDecrementer type is not MagicCopyDecrementer");
		const size_t test1 = (char*)&(((T*)0)->copyDecrementer) - (char*)0;
		const size_t MagicCopyDecrementerEmbeddedSize = (char*)&((TestEmbeddedSize<MagicCopyDecrementer>*)0)->a - (char*)0;
		const size_t MagicCopyDecrementerEndPadding = sizeof(TestPadding<MagicCopyDecrementer>) - ((char*)&((TestPadding<MagicCopyDecrementer>*)0)->a - (char*)0) - sizeof(MagicCopyDecrementer);
		const size_t MagicCopyDecrementerEndPadding2 = sizeof(TestPadding<MagicCopyDecrementer>) - sizeof(TestPadding<MagicCopyDecrementer>::padding) - sizeof(MagicCopyDecrementer);
		const size_t test2 = sizeof(T) - MagicCopyDecrementerEmbeddedSize - MagicCopyDecrementerEndPadding;
		//static_assert(test1 >= test2, "copyDecrementer should be the last member");

		if (!(test1 >= test2)) 
			MY_ERROR("copyDecrementer should be the last member"); //won't catch all possible not conforming classes 
	}

public:

	MagicPointer() : ptr(nullptr) {
		this->StaticErrorCheck();
	}

	MagicPointer(T * ptr) {

		this->StaticErrorCheck();
		static_assert(std::is_base_of<MagicTarget<T, builtin_type>, T>::value,
			"MagicPointer<T> can only point to a class derived from MagicTarget<T>");

		this->Attach(ptr);
	}

	MagicPointer(const MagicPointer & that) {
		this->StaticErrorCheck(); 
		this->Copy(that);
	}

	MagicPointer(MagicPointer && that) {
		this->StaticErrorCheck(); 
		this->Move(that);
	}

	MagicPointer & operator=(MagicPointer && that) {

		if (this == &that) return *this;

		if (this->ptr) this->Detach();
		this->Move(that);
		return *this;
	}

	MagicPointer & operator=(const MagicPointer & that) {
		this->operator=((T*)that.ptr);
		return *this;
	}

	T * operator=(const T * ptr) {

		static_assert(std::is_base_of<MagicTarget<T, builtin_type>, T>::value,
			"MagicPointer<T> can only point to a class derived from MagicTarget<T>");

		if (this->ptr) this->Detach();
		if (ptr) this->Copy(*(MagicPointer*)&ptr);
		else this->ptr = nullptr;
		return (T*)this->ptr;
	}

	T & operator *() {
		return *(T*)this->ptr;
	}

	T * operator ->() {
		return (T*)this->ptr;
	}

	T * operator ->() const {
		return (T*)this->ptr;
	}

	operator T*() {
		return (T*)this->ptr;
	}

	operator T*() const {
		return (T*)this->ptr;
	}

	explicit operator bool() {
		return this->ptr != nullptr;
	}

	~MagicPointer() {

		if (!this->ptr) return;

		this->Detach();
	}
};

//class EmptyTarget : 

template <class T, bool builtin_type>
class MagicTarget {

	friend class MagicPointer <T, builtin_type>;
	friend struct MagicCopyDecrementer;
	friend struct MagicPointerBase <T, builtin_type>;
	friend struct MagicTargetBase  <T, builtin_type>;

	typedef MagicTargetBase<T, builtin_type> MagicBase;

	size_t nCachedPointers;
	MagicPointer<T, builtin_type> * cachedPointers[MAGICREF_N_CACHED_POINTERS];

	aVect<MagicPointer<T, builtin_type> *> pointers;

	MagicTarget * beingCopiedTo;

	size_t nCachedFreeList;
	size_t cachedFreeList[MAGICREF_N_CACHED_POINTERS];
	aVect<size_t> freeList;

	void DBG_CHECK() {
#if defined(_DEBUG) || defined(MY_DEBUG) 
		if (g_MagicTarget_CopyRecursion == 0 && this->beingCopiedTo) MY_ERROR("gotcha");
		if (this->pointers) _CrtIsValidHeapPointer(this->pointers.GetDataPtr());
#endif
	}

	void Detach()						{ ((MagicBase*)this)->Detach(); }
	void Move(MagicTarget & that)		{ ((MagicBase*)this)->Move(that); }
	void Copy(const MagicTarget & that) { 
		Critical_Section(g_Magic_cs) {
			MagicCopyIncrement();
#if defined(_DEBUG) || defined(MY_DEBUG) 
			if (g_MagicTarget_CopyRecursion == 0) MY_ERROR("gotcha");
#endif
			((MagicBase*)this)->Copy(that);
		}
	}

public:

	MagicTarget() : nCachedPointers(0), nCachedFreeList(0), beingCopiedTo(0) {}

	MagicTarget(const MagicTarget & that) : nCachedPointers(0), nCachedFreeList(0), beingCopiedTo(0) {
		this->Copy(that);
	}

	MagicTarget(MagicTarget && that) : nCachedPointers(0), nCachedFreeList(0), beingCopiedTo(0) {
		this->Move(that);
	}

	template <class... Args>
	auto begin(Args&&... args) -> decltype(((T*)0)->begin()) {
		return ((T*)this)->begin();
	}

	template <class... Args>
	auto end(Args&&... args) -> decltype(((T*)0)->end()) {
		return ((T*)this)->end();
	}

	void operator=(const MagicTarget & that) {

#ifdef DBG_CHECK_ACTIVE
		this->DBG_CHECK();
#endif

		if (this == &that) {
			Critical_Section(g_Magic_cs) {
				MagicCopyIncrement();
#if defined(_DEBUG) || defined(MY_DEBUG) 
				if (g_MagicTarget_CopyRecursion == 0) MY_ERROR("gotcha");
#endif
			}
			return;
		}

		this->Detach();
		this->nCachedPointers = 0;
		this->nCachedFreeList = 0;
		if (this->pointers) this->pointers.Redim(0);
		if (this->freeList) this->freeList.Redim(0);
		this->Copy(that);

#ifdef DBG_CHECK_ACTIVE
		this->DBG_CHECK();
#endif
	}

	void operator=(MagicTarget && that) {

#ifdef DBG_CHECK_ACTIVE
		this->DBG_CHECK();
#endif

		if (this == &that) return;

		this->Detach();
		this->Move(that);

#ifdef DBG_CHECK_ACTIVE
		this->DBG_CHECK();
#endif
	}

	GENERATE_BINARY_OPERATOR2(T, = )
	GENERATE_BINARY_OPERATOR2(T, +)
	GENERATE_BINARY_OPERATOR2(T, -)
	GENERATE_BINARY_OPERATOR2(T, *)
	GENERATE_BINARY_OPERATOR2(T, / )
	GENERATE_BINARY_OPERATOR2(T, += )
	GENERATE_BINARY_OPERATOR2(T, -= )
	GENERATE_BINARY_OPERATOR2(T, *= )
	GENERATE_BINARY_OPERATOR2(T, /= )
	GENERATE_POST_UNARY_OPERATOR2(T, ++)
	GENERATE_POST_UNARY_OPERATOR2(T, --)
	GENERATE_PRE_UNARY_OPERATOR2(T, ++)
	GENERATE_PRE_UNARY_OPERATOR2(T, --)
	GENERATE_PRE_UNARY_OPERATOR2(T, +)
	GENERATE_PRE_UNARY_OPERATOR2(T, -)

	template <class... Args>
	auto operator ()(Args&&... args) -> decltype((*(T*)0)(std::forward<Args>(args)...)) {
		return (*(T*)this)(std::forward<Args>(args)...);
	}

	operator T() {
#ifdef DBG_CHECK_ACTIVE
		this->DBG_CHECK();
#endif
		return (*(T*)this);
	}

	~MagicTarget() {
#ifdef DBG_CHECK_ACTIVE
		this->DBG_CHECK();
#endif
		this->Detach();
	}
};

template <class T>
class MagicPointer<T, true> {

	friend class MagicTarget <T, true>;
	friend struct MagicCopyDecrementer;
	friend struct MagicPointerBase<T, true>;
	friend struct MagicTargetBase  <T, true>;

	typedef MagicPointerBase<T, true> MagicBase;

	MagicTarget<T, true> * ptr;
	size_t selfIndex;

	void Attach(T * _ptr)			{ ((MagicBase*)this)->Attach(_ptr); }
	void Detach()					{ ((MagicBase*)this)->Detach(); }
	void Move(MagicPointer & that)	{ ((MagicBase*)this)->Move(that); }
	void Copy(const MagicPointer & that)	{ ((MagicBase*)this)->Copy(that); }

public:

	MagicPointer() : ptr(nullptr) {}

	MagicPointer(MagicTarget<T, true> * ptr) {
		this->Attach((T*)ptr);
	}

	MagicPointer(const MagicPointer & that) {
		this->Copy(that);
	}

	MagicPointer(MagicPointer && that) {
		this->Move(that);
	}

	MagicPointer & operator=(MagicPointer && that) {

		if (this == &that) return *this;

		if (this->ptr) this->Detach();
		this->Move(that);
		return this->ptr;
	}

	MagicPointer & operator=(const MagicPointer & that) {

		return this->operator=((MagicTarget<T, true> *)that.ptr);
	}

	MagicPointer & operator=(MagicTarget<T, true> * ptr) {

		if (this->ptr) this->Detach();
		if (ptr) this->Copy(*(MagicPointer*)&ptr);
		else this->ptr = nullptr;
		return *this;
	}

	T & operator *() const {
		return ((MagicTarget<T, true> *)this->ptr)->value;
	}

	operator MagicTarget<T, true> * () {
		return (MagicTarget<T, true>*)this->ptr;
	}

	explicit operator bool() {
		return this->ptr != nullptr;
	}

	~MagicPointer() {

		if (!this->ptr) return;

		this->Detach();
		this->ptr = nullptr;
	}
};

template <class T>
class MagicTarget<T, true> {

	friend class MagicPointer <T, true>;
	friend struct MagicCopyDecrementer;
	friend struct MagicPointerBase <T, true>;
	friend struct MagicTargetBase  <T, true>;

	typedef MagicTargetBase<T, true> MagicBase;

	size_t nCachedPointers;
	MagicPointer<T, true> * cachedPointers[MAGICREF_N_CACHED_POINTERS];

	aVect<MagicPointer<T, true> *> pointers;

	MagicTarget * beingCopiedTo;

	size_t nCachedFreeList;
	size_t cachedFreeList[MAGICREF_N_CACHED_POINTERS];
	aVect<size_t> freeList;

	T value;

	void DBG_CHECK() const {
#if defined(_DEBUG) || defined(MY_DEBUG) 
		if (g_MagicTarget_CopyRecursion == 0 && this->beingCopiedTo) MY_ERROR("gotcha");
#endif
	}

	void Detach() { ((MagicBase*)this)->Detach(); }

	void Move(MagicTarget & that) {
		this->value = that.value;
		((MagicBase*)this)->Move(that);
	}

	void Copy(const MagicTarget & that) {
		this->value = that.value;
		((MagicBase*)this)->Copy(that); 
	}

public:

	MagicTarget() : nCachedPointers(0), nCachedFreeList(0), beingCopiedTo(0) {}
	MagicTarget(T value) : value(value), nCachedPointers(0), nCachedFreeList(0), beingCopiedTo(0) {}

	MagicTarget(const MagicTarget & that) : nCachedPointers(0), nCachedFreeList(0), value(that.value), beingCopiedTo(0) {
		this->Copy(that);
	}

	MagicTarget(MagicTarget && that) : nCachedPointers(0), nCachedFreeList(0), beingCopiedTo(0) {
		this->Move(that);
	}

	void operator=(const MagicTarget & that) {

#ifdef DBG_CHECK_ACTIVE
		this->DBG_CHECK();
#endif

		if (this == &that) {
			Critical_Section(g_Magic_cs) {
				MagicCopyIncrement();
#if defined(_DEBUG) || defined(MY_DEBUG) 
				if (g_MagicTarget_CopyRecursion == 0) MY_ERROR("gotcha");
#endif
			}
			return;
		}

		this->Detach();
		this->nCachedPointers = 0;
		this->nCachedFreeList = 0;
		if (this->pointers) this->pointers.Redim(0);
		if (this->freeList) this->freeList.Redim(0);
		this->Copy(that);

#ifdef DBG_CHECK_ACTIVE
		this->DBG_CHECK();
#endif
	}

	void operator=(MagicTarget && that) {

#ifdef DBG_CHECK_ACTIVE
		this->DBG_CHECK();
#endif

		if (this == &that) return;

		this->Detach();
		this->Move(that);

#ifdef DBG_CHECK_ACTIVE
		this->DBG_CHECK();
#endif
	}

	void operator=(T value) {
#ifdef DBG_CHECK_ACTIVE
		this->DBG_CHECK();
#endif
		this->value = value;
#ifdef DBG_CHECK_ACTIVE
		this->DBG_CHECK();
#endif
	}

	GENERATE_BINARY_OPERATOR(value, =)
	GENERATE_BINARY_OPERATOR(value, +)
	GENERATE_BINARY_OPERATOR(value, -)
	GENERATE_BINARY_OPERATOR(value, *)
	GENERATE_BINARY_OPERATOR(value, /)
	GENERATE_BINARY_OPERATOR(value, +=)
	GENERATE_BINARY_OPERATOR(value, -=)
	GENERATE_BINARY_OPERATOR(value, *=)
	GENERATE_BINARY_OPERATOR(value, /=)
	GENERATE_POST_UNARY_OPERATOR(value, ++)
	GENERATE_POST_UNARY_OPERATOR(value, --)
	GENERATE_PRE_UNARY_OPERATOR(value, ++)
	GENERATE_PRE_UNARY_OPERATOR(value, --)
	GENERATE_PRE_UNARY_OPERATOR(value, +)
	GENERATE_PRE_UNARY_OPERATOR(value, -)

	template <class... Args>
	auto operator ()(Args&&... args) -> decltype((*(T*)0)(std::forward<Args>(args)...)) {
		return (*(T*)this)(std::forward<Args>(args)...);
	}

	operator T() const {
#ifdef DBG_CHECK_ACTIVE
		this->DBG_CHECK();
#endif
		return (*(T*)&this->value);
	}

	~MagicTarget() {
#ifdef DBG_CHECK_ACTIVE
		this->DBG_CHECK();
#endif
		this->Detach();
	}
}; 

template <bool isBuiltin_type> 
void MagicCopyDecrementer::FixPointers(MagicPointer_CopyOp & copyOp) {

	typedef int whatever;

	MagicPointer<whatever, isBuiltin_type> * This	  = (MagicPointer<whatever, isBuiltin_type>*)copyOp.This;
	MagicTarget <whatever, isBuiltin_type> * That_ptr = (MagicTarget <whatever, isBuiltin_type>*)copyOp.That;

	if (That_ptr && That_ptr->beingCopiedTo) {
		This->Attach((whatever*)That_ptr->beingCopiedTo);
		return;
	}

	This->Attach((whatever*)That_ptr);
}

//template<> inline __nothrow bool std::isfinite(const MagicTarget<double> & mt) {
//	return std::isfinite((double)mt);
//}

	inline bool isfinite(const MagicTarget<double> & dw) {
		return isfinite((double)dw);
	}

	using std::isfinite;

#endif
