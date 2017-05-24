
#define _CRT_SECURE_NO_WARNINGS

#include "string.h"
#include "xVect.h"

unsigned long long g_nAllocation_xVect;
unsigned long long g_accAllocated_xVect;
unsigned long long g_nAllocation_yVect;
unsigned long long g_accAllocated_yVect;
unsigned long long g_allocID;

void *** g_pOwnerToClear_xVect;
int g_CopyRecursion_xVect;

unsigned long long g_dgbCatcher;
 
unsigned long long g_nAllocation;
unsigned long long g_accAllocated;
void *** g_pOwnerToClear;
int g_CopyRecursion;

//
//class NonNullPlacement {};
//void* operator new(size_t, void* ptr, NonNullPlacement&) {
//	return ptr;
//}
