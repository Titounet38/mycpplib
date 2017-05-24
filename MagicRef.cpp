#define _CRT_SECURE_NO_WARNINGS

#include "xVect.h"
#include "MagicRef.h"

//WinCriticalSection g_MagicCopy_cs;
WinCriticalSection g_Magic_cs;

size_t g_MagicTarget_CopyRecursion;
aVect<MagicPointer_CopyOp> g_MagicPointer_CopyOp;
aVect<void**> g_MagicPointer_CopiedTo_ClearList;

void MagicCopyIncrement() {
	Critical_Section(g_Magic_cs) {
		g_MagicTarget_CopyRecursion++;
		if (g_MagicTarget_CopyRecursion == 1) {
			g_Magic_cs.Enter();
		}
	}
}

//
//template<> /*inline __nothrow*/ bool std::isfinite(const MagicTarget<double> & mt) {
//	return std::isfinite((double)mt);
//}
//
