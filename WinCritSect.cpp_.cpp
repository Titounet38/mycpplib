
#include "WinCritSect.h"

AutoCriticalSection::AutoCriticalSection() : pCs(nullptr), recursion(0) {}
AutoCriticalSection::AutoCriticalSection(CRITICAL_SECTION * ptr, bool enter) : pCs(ptr), recursion(0) {
	if (enter) {
		++recursion;
		EnterCriticalSection(ptr);
	}
}
AutoCriticalSection::~AutoCriticalSection() {
	Destroy();
}

AutoCriticalSection& AutoCriticalSection::Destroy() {
	while (recursion) Leave();
	pCs = nullptr;
	return *this;
}

AutoCriticalSection& AutoCriticalSection::Set(CRITICAL_SECTION * ptr) {
	if (pCs && pCs != ptr) Destroy();
	pCs = ptr;
	return *this;
}

AutoCriticalSection& AutoCriticalSection::Enter(CRITICAL_SECTION * ptr) {
	Set(ptr);
	Enter();
	return *this;
}

bool AutoCriticalSection::CheckReleased() {
	return recursion == 0;
}

AutoCriticalSection& AutoCriticalSection::Enter() {
	if (!pCs) MY_ERROR("Critical section not set");
	if (recursion == 0) EnterCriticalSection(pCs);
	recursion++;
	return *this;
}

bool AutoCriticalSection::TryEnter() {

	if (!pCs) MY_ERROR("Critical section not set");

	bool retVal;

	if (recursion == 0) {
		retVal = TryEnterCriticalSection(pCs);
		if (!retVal) {
			int a = 0;
		}
	} else {
		retVal = true;
	}

	if (retVal) recursion++;

	return retVal;
}

AutoCriticalSection& AutoCriticalSection::Leave() {
	if (recursion == 0) MY_ERROR("Leave with no matching Enter");
	recursion--;
	if (recursion == 0) LeaveCriticalSection(pCs);
	return *this;
}
