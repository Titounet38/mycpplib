
#ifndef _WINCRITSECT_
#define _WINCRITSECT_

#include "windows.h"
#include "Debug.h"

class WinCriticalSection {

	CRITICAL_SECTION criticalSection;
	size_t enterCount;
	bool initialized;

public:

	WinCriticalSection(const WinCriticalSection & that) {
		this->initialized = that.initialized;
		this->enterCount = 0;

		if (this->initialized) {
			InitializeCriticalSection(&this->criticalSection);
		}
	}

	WinCriticalSection & operator=(const WinCriticalSection & that) {
		
		if (this->initialized && that.initialized) return *this;

		if (!this->initialized && that.initialized) {
			InitializeCriticalSection(&this->criticalSection);
			this->initialized = true;
			this->enterCount = 0;
			return *this;
		}

		if (this->initialized && !that.initialized) {
			if (this->enterCount != 0) MY_ERROR("critical section not released");
			DeleteCriticalSection(&this->criticalSection);
			this->initialized = false;
			this->enterCount = 0;
		}

		return *this;
	}

	WinCriticalSection & operator=(WinCriticalSection && that) {
		
		this->criticalSection = that.criticalSection;
		this->enterCount = that.enterCount;
		this->initialized = that.initialized;

		that.initialized = false;
		that.enterCount = 0;

		return *this;
	}

	WinCriticalSection(WinCriticalSection && that) {
		this->criticalSection = that.criticalSection;
		this->enterCount = that.enterCount;
		this->initialized = that.initialized;

		that.initialized = false;
		that.enterCount = 0;
	}

	WinCriticalSection(bool initialize = true) : enterCount(0) {
		if (initialize) {
			InitializeCriticalSection(&this->criticalSection);
		}
		this->initialized = initialize;
	}

	WinCriticalSection(CRITICAL_SECTION * pCriticalSection) : enterCount(0) {
		this->criticalSection = *pCriticalSection;
#ifdef _DEBUG
		this->initialized = true;
#endif
	}

	operator CRITICAL_SECTION * () {
#ifdef _DEBUG
		if (!this->initialized) MY_ERROR("disabled critical section");
#endif
		return &criticalSection;
	}

	WinCriticalSection & Enter() {
#ifdef _DEBUG
		if (!this->initialized) MY_ERROR("disabled critical section");
#endif
		EnterCriticalSection(&this->criticalSection);

		this->enterCount++;

		return *this;
	}

	bool TryEnter() {
#ifdef _DEBUG
		if (!this->initialized) MY_ERROR("disabled critical section");
#endif
		if (TryEnterCriticalSection(&this->criticalSection)) {
			this->enterCount++;
			return true;
		}

		return false;
	}

	size_t GetEnterCount() {
		return this->enterCount;
	}

	WinCriticalSection & Initialize() {

		if (this->initialized) MY_ERROR("critical section already initialized");
		InitializeCriticalSection(&this->criticalSection);
		this->initialized = true;
	}

	WinCriticalSection & Leave() {
#ifdef _DEBUG
		if (!this->initialized) MY_ERROR("uninitialized critical section");
#endif

		if (this->enterCount == 0) MY_ERROR("enterCount = 0, cannot leave critical section");

		this->enterCount--;
		LeaveCriticalSection(&this->criticalSection);

		return *this;
	}

	~WinCriticalSection() {
		if (this->enterCount) MY_ERROR("critical section not released");
		if (this->initialized) {
#ifdef TMVX_BUGHUNT_1
#ifdef _DEBUG
			_CrtCheckMemory();
#else
			if (!HeapValidate(GetProcessHeap(), 0, NULL)) MY_ERROR("Heap corruption");
#endif
#endif
			DeleteCriticalSection(&this->criticalSection);
		}
	}
};


template <class CS>
class ScopeCriticalSection {

	CS pCs;

public:

	ScopeCriticalSection(CRITICAL_SECTION * ptr) {
		this->pCs = ptr;
		if (this->pCs) EnterCriticalSection(this->pCs);
	}

	explicit operator bool() {
		return true;
		//return this->pCs != nullptr; //No, Axis.cpp relies on return true even if pCs == nullptr;
	}

	~ScopeCriticalSection() {
		if (this->pCs) LeaveCriticalSection(this->pCs);
	}
};


template <>
class ScopeCriticalSection<WinCriticalSection> {

	WinCriticalSection * pWcs;

public:
	//operator bool();
	//ScopeCriticalSection(CS * ptr);
	//~ScopeCriticalSection();

	ScopeCriticalSection(WinCriticalSection & wcs) {
		this->pWcs = &wcs;
		this->pWcs->Enter();
	}

	operator bool() {
		return true;
	}

	~ScopeCriticalSection() {
		if (pWcs) this->pWcs->Leave();
	}
};


#define Critical_Section(a) if (ScopeCriticalSection<decltype(a)> __scs = a)

class AutoCriticalSection {
	CRITICAL_SECTION * pCs;
	int recursion;
public:
	AutoCriticalSection();
	AutoCriticalSection(CRITICAL_SECTION * ptr);
	~AutoCriticalSection();
	AutoCriticalSection& Set(CRITICAL_SECTION * ptr);
	AutoCriticalSection& Enter(CRITICAL_SECTION * ptr);
	AutoCriticalSection& Enter();
	AutoCriticalSection& Leave();
	AutoCriticalSection& Destroy();
	bool CheckReleased();
};


#endif
