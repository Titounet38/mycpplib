
#include "xVect.h"
#include "MyUtils.h"

#ifndef _XMAT_
#define _XMAT_

#define aMat_static_for(M, i, j) for (size_t i=0, __I=M.nRow(); i<__I; ++i) for (size_t j=0, __J=M.nCol(); j<__J; ++j)
#define aPlan_static_for(M, x, y) for (size_t x=0, __X=M.xCount(); x<__X; ++x) for (size_t y=0, __Y=M.yCount(); y<__Y; ++y)
#define xTens_static_for(M, x, y) for (size_t x=0, __X=(M.DimArray())[0]; x<__X; ++x) for (size_t y=0, __Y=(M.DimArray())[1]; y<__Y; ++y)
#define aTens_static_for3(M, x, y, z) for (size_t x=0, __X=(M.DimArray())[0]; x<__X; ++x) for (size_t y=0, __Y=(M.DimArray())[1]; y<__Y; ++y) for (size_t z=0, __Z=(M.DimArray())[2]; z<__Z; ++z)

template <class T, size_t n> struct xTens_struct {
	size_t dims[n];
	size_t nDim;
	xVect_struct<T> vect;
};

template <class T> class xTens_Accessor {

	T * pData; 
	size_t ni, nj, nk, nl;

public:

	size_t * DimArray() {
		return xTens_DimArray(pData);
	}

	xTens_Accessor() : pData(0) {}

	xTens_Accessor(T * ptr) {
		Wrap(ptr);
	}

	xTens_Accessor & Reset() {
		pData = nullptr;
		return *this;
	}

	xTens_Accessor & Wrap(T * ptr) {
		pData = ptr;
		size_t * dimArray = xTens_DimArray(ptr);
		size_t nDim = xTens_nDim(ptr);
		switch (nDim) {
			case 5: {
				nl = dimArray[3];
				nk = nl * dimArray[2];
				nj = nk * dimArray[1];
				ni = nj * dimArray[0];
				break;
			}
			case 4: {
				nk = dimArray[3];
				nj = nk * dimArray[2];
				ni = nj * dimArray[1];
				break;
			}
			case 3: {
				nj = dimArray[2];
				ni = nj * dimArray[1];
				break;
			}
			case 2: {
				ni = dimArray[1];
				break;
			}
			default: MY_ERROR("Implemented for 1 < nDim < 6");
		}
		return *this;
	}

	T & operator()(size_t i, size_t j) {
		return pData[i*ni + j];
	}

	T & operator()(size_t i, size_t j, size_t k) {
		return pData[i*ni + j*nj + k];
	}

	T & operator()(size_t i, size_t j, size_t k, size_t l) {
		return pData[i*ni + j*nj + k*nk + l];
	}

	T & operator()(size_t i, size_t j, size_t k, size_t l, size_t m) {
		return pData[i*ni + j*nj + k*nk + l*nl + m];
	}
};

template <class T, size_t nDimension> class aTens : public aVect<T> {

	size_t dims[nDimension];
	size_t n[nDimension - 1];

public:

	aTens() : aVect<T>() {
		for (int i = 0; i < nDimension; ++i)   dims[i] = 0;
		for (int i = 0; i < nDimension - 1; ++i) n[i] = 0;
	}

	aTens(const aTens & that) : aVect<T>(that) {
		for (int i = 0; i < nDimension; ++i)   this->dims[i] = that.dims[i];
		for (int i = 0; i < nDimension - 1; ++i) this->n[i] = that.n[i];
	}

	aTens(aTens && that) : aVect<T>(std::move(that)) {
		for (int i = 0; i < nDimension; ++i)   this->dims[i] = that.dims[i];
		for (int i = 0; i < nDimension - 1; ++i) this->n[i] = that.n[i];
	}

	aTens & operator=(aTens && that) {
		aVect::operator=(std::move(that));
		for (int i = 0; i < nDimension; ++i)   this->dims[i] = that.dims[i];
		for (int i = 0; i < nDimension - 1; ++i) this->n[i] = that.n[i];
		return *this;
	}

	aTens & operator=(const aTens & that) {
		aVect::operator=(that);
		for (int i = 0; i < nDimension; ++i)   this->dims[i] = that.dims[i];
		for (int i = 0; i < nDimension - 1; ++i) this->n[i] = that.n[i];
		return *this;
	}

	aTens(size_t I, size_t J) : aVect<T>(I*J) {
		static_assert(nDimension == 2, "wrong number of dimensions");
		dims[0] = I;
		dims[1] = J;
		n[0] = J;
	}

	aTens(size_t I, size_t J, size_t K) : aVect<T>(I*J*K) {
		static_assert(nDimension == 3, "wrong number of dimensions");
		dims[0] = I;
		dims[1] = J;
		dims[2] = K;
		n[0] = J*K;
		n[1] = K;
	}

	aTens(size_t I, size_t J, size_t K, size_t L) : aVect<T>(I*J*K*L) {
		static_assert(nDimension == 4, "wrong number of dimensions");
		dims[0] = I;
		dims[1] = J;
		dims[2] = K;
		dims[3] = L;
		n[0] = J*K*L;
		n[1] = K*L;
		n[2] = L;
	}

	aTens(size_t I, size_t J, size_t K, size_t L, size_t M) : aVect<T>(I*J*K*L*M) {
		static_assert(nDimension == 5, "wrong number of dimensions");
		dims[0] = I;
		dims[1] = J;
		dims[2] = K;
		dims[3] = L;
		dims[4] = M;
		n[0] = J*K*L*M;
		n[1] = K*L*M;
		n[2] = L*M;
		n[3] = M;
	}

	aTens & Copy(const aTens & that) {
		aVect::Copy(that);
		for (int i = 0; i < nDimension; ++i)   this->dims[i] = that.dims[i];
		for (int i = 0; i < nDimension - 1; ++i) this->n[i] = that.n[i];
		return *this;
	}

	aTens & Steal(aTens & that) {
		aVect::Steal(that);
		for (int i = 0; i < nDimension; ++i)   this->dims[i] = that.dims[i];
		for (int i = 0; i < nDimension - 1; ++i) this->n[i] = that.n[i];
		return *this;
	}

	size_t nDim() {
		return nDimension;
	}

	template <class U> aTens & Redim(U & t) {
		if (nDimension != t.nDim()) MY_ERROR("wrong number of dimensions");
		size_t * dimArray = t.DimArray();
		switch (nDimension) {
		case 2: this->Redim(dimArray[0], dimArray[1]); break;
		case 3: this->Redim(dimArray[0], dimArray[1], dimArray[2]); break;
		case 4: this->Redim(dimArray[0], dimArray[1], dimArray[2], dimArray[3]); break;
		case 5: this->Redim(dimArray[0], dimArray[1], dimArray[2], dimArray[3], dimArray[4]); break;
		default: MY_ERROR("wrong number of dimensions");
		}
		return *this;
	}

	aTens & _RedimUnitialized(size_t I, size_t J) {

		if (nDimension != 2) MY_ERROR("wrong number of dimensions");
		dims[0] = I;
		dims[1] = J;
		n[0] = J;

		aVect<T>::_RedimUnitialized(I*J);

		return *this;
	}

	aTens & Redim(size_t I, size_t J) {

		if (nDimension != 2) MY_ERROR("wrong number of dimensions");
		dims[0] = I;
		dims[1] = J;
		n[0] = J;

		aVect<T>::Redim(I*J);

		return *this;
	}

	aTens & Redim(size_t I, size_t J, size_t K) {

		if (nDimension != 3) MY_ERROR("wrong number of dimensions");
		dims[0] = I;
		dims[1] = J;
		dims[2] = K;
		n[0] = J*K;
		n[1] = K;

		aVect<T>::Redim(I*J*K);

		return *this;
	}

	aTens & Redim(size_t I, size_t J, size_t K, size_t L) {

		if (nDimension != 4) MY_ERROR("wrong number of dimensions");
		dims[0] = I;
		dims[1] = J;
		dims[2] = K;
		dims[3] = L;
		n[0] = J*K*L;
		n[1] = K*L;
		n[2] = L;

		aVect<T>::Redim(I*J*K*L);

		return *this;
	}

	aTens & Redim(size_t I, size_t J, size_t K, size_t L, size_t M) {
		if (nDimension != 5) MY_ERROR("wrong number of dimensions");
		dims[0] = I;
		dims[1] = J;
		dims[2] = K;
		dims[3] = L;
		dims[4] = M;
		n[0] = J*K*L*M;
		n[1] = K*L*M;
		n[2] = L*M;
		n[3] = M;

		aVect<T>::Redim(I*J*K*L*M);

		return *this;
	}

	//needs reimplementation
	//aTens & Wrap(T * ptr) {

	//	size_t * dimArray = xTens_DimArray(ptr);

	//	dims[0] = dimArray[0];
	//	dims[1] = dimArray[1];
	//	n[0] = dimArray[1];

	//	this->Wrap(ptr);

	//	return *this;
	//}

	bool OutOfBounds(size_t i, size_t j) {
#ifdef _DEBUG
		if (nDimension != 2) MY_ERROR("wrong number of dimensions");
#endif
		if (i < this->dims[0] && j < this->dims[1]) return false;
		else return true;
	}

	bool OutOfBounds(size_t i, size_t j, size_t k) {
#ifdef _DEBUG
		if (nDimension != 3) MY_ERROR("wrong number of dimensions");
#endif
		if (i < this->dims[0] && j < this->dims[1] && j < this->dims[2]) return false;
		else return true;
	}

	bool OutOfBounds(size_t i, size_t j, size_t k, size_t l) {
#ifdef _DEBUG
		if (nDimension != 4) MY_ERROR("wrong number of dimensions");
#endif
		if (i < this->dims[0] && j < this->dims[1] && k < this->dims[2] && l < this->dims[3]) return false;
		else return true;
	}

	bool OutOfBounds(size_t i, size_t j, size_t k, size_t l, size_t m) {
#ifdef _DEBUG
		if (nDimension != 5) MY_ERROR("wrong number of dimensions");
#endif
		if (i < this->dims[0] && j < this->dims[1] && k < this->dims[2] && l < this->dims[3] && m < this->dims[4]) return false;
		else return true;
	}

	T & operator()(size_t i, size_t j) {
#ifdef _DEBUG
		if (nDimension != 2) MY_ERROR("wrong number of dimensions");
#endif
		return aVect<T>::operator[](i*n[0] + j);
	}

	T & operator()(size_t i, size_t j, size_t k) {
#ifdef _DEBUG
		if (nDimension != 3) MY_ERROR("wrong number of dimensions");
#endif
		return aVect<T>::operator[](i*n[0] + j*n[1] + k);
	}


	T & operator()(size_t i, size_t j, size_t k, size_t l) {
#ifdef _DEBUG
		if (nDimension != 4) MY_ERROR("wrong number of dimensions");
#endif
		return aVect<T>::operator[](i*n[0] + j*n[1] + k*n[2] + l);
	}


	T & operator()(size_t i, size_t j, size_t k, size_t l, size_t m) {
#ifdef _DEBUG
		if (nDimension != 5) MY_ERROR("wrong number of dimensions");
#endif
		return aVect<T>::operator[](i*n[0] + j*n[1] + k*n[2] + l*n[3] + m);
	}


	T & operator()(size_t i, size_t j) const {
#ifdef _DEBUG
		if (nDimension != 2) MY_ERROR("wrong number of dimensions");
#endif
		return aVect<T>::operator[](i*n[0] + j);
	}

	T & operator()(size_t i, size_t j, size_t k) const {
#ifdef _DEBUG
		if (nDimension != 3) MY_ERROR("wrong number of dimensions");
#endif
		return aVect<T>::operator[](i*n[0] + j*n[1] + k);
	}

	T & operator()(size_t i, size_t j, size_t k, size_t l) const {
#ifdef _DEBUG
		if (nDimension != 4) MY_ERROR("wrong number of dimensions");
#endif
		return aVect<T>::operator[](i*n[0] + j*n[1] + k*n[2] + l);
	}

	T & operator()(size_t i, size_t j, size_t k, size_t l, size_t m) const {
#ifdef _DEBUG
		if (nDimension != 5) MY_ERROR("wrong number of dimensions");
#endif
		return aVect<T>::operator[](i*n[0] + j*n[1] + k*n[2] + l*n[3] + m);
	}

	template <size_t dim> size_t DimCount() const {
		static_assert(dim < nDimension, "Wrong dimension");
		return dims[dim];
	}

	size_t * DimArray() {
		return dims;
	}
};

template <class T> class aMat : public aTens<T, 2> {

public:
	aMat() : aTens<T, 2>() {}
	aMat(size_t I, size_t J) : aTens(I, J) {}
	aMat(const aMat & that) : aTens(that) {}
	aMat(aMat && that) : aTens(std::move(that)) {}

	aMat & operator=(const aMat& that) {
		aTens::operator=(that);
		return *this;
	}

	aMat & operator=(aMat&& that) {
		aTens::operator=(std::move(that));
		return *this;
	}

	size_t nRow() const {
		return aTens<T, 2>::DimCount<0>();
	}
	size_t nCol() const {
		return aTens<T, 2>::DimCount<1>();
	}
};

template <class T> class aPlan : public aTens<T, 2> {

public:
	aPlan() : aTens<T, 2>() {}
	aPlan(size_t xCount, size_t yCount) : aTens<T, 2>(xCount, yCount) {}

	size_t xCount() const {
		return aTens<T, 2>::DimCount<0>();
	}
	size_t yCount() const {
		return aTens<T, 2>::DimCount<1>();
	}
};

template <class T> class autoTens {

	size_t nDim;

	union {
		aTens<T, 2> * m2;
		aTens<T, 3> * m3;
		aTens<T, 4> * m4;
		aTens<T, 5> * m5;
	};

public:
	autoTens(size_t I, size_t J) {
		nDim = 2;
		m2 = new aTens<T, 2>(I, J);
	}
	autoTens(size_t I, size_t J, size_t K) {
		nDim = 3;
		m3 = new aTens<T, 3>(I, J, K);
	}
	autoTens(size_t I, size_t J, size_t K, size_t L) {
		nDim = 4;
		m4 = new aTens<T, 4>(I, J, K, L);
	}
	autoTens(size_t I, size_t J, size_t K, size_t L, size_t M) {
		nDim = 5;
		m5 = new aTens<T, 5>(I, J, K, L, M);
	}
	//autoTens(size_t I, size_t J, size_t K) : m3(I, J, K) {}
	//autoTens(size_t I, size_t J, size_t K, size_t L) : m4(I, J, K, L) {}
	//autoTens(size_t I, size_t J, size_t K, size_t L, size_t M) : m5(I, J, K, L, M) {}

	inline T & operator()(size_t i, size_t j) {
		return (*m2)(i, j);
	}

	inline T & operator()(size_t i, size_t j, size_t k) {
		return (*m3)(i, j, k);
	}

	inline T & operator()(size_t i, size_t j, size_t k, size_t l) {
		return (*m4)(i, j, k, l);
	}

	inline T & operator()(size_t i, size_t j, size_t k, size_t l, size_t m) {
		return (*m5)(i, j, k, l, m);
	}

	inline size_t DimCount(size_t dim) {
		return m2->DimCount(dim);
	}

	//T & operator()(size_t i, size_t j, size_t k) {
	//	return m3(i, j, k);
	//}

	//T & operator()(size_t i, size_t j, size_t k, size_t l) {
	//	return m4(i, j, k, l);
	//}

	//T & operator()(size_t i, size_t j, size_t k, size_t l, size_t m) {
	//	return m5(i, j, k, l, m);
	//}

	~autoTens() {
		switch(nDim) {
			case 2: {
				(*m2).~aTens();
				break;
			}
			case 3: {
				(*m3).~aTens();
				break;
			}
			case 4: {
				(*m4).~aTens();
				break;
			}
			case 5: {
				(*m5).~aTens();
				break;
			}
			default: {
				MY_ERROR("wrong number of dimensions");
			}
		}
	}
};


template <class T, size_t nDim> xTens_struct<T, nDim> * xTens_BasePtr(T * ptr) {

	return (xTens_struct<T, nDim>*)xTens_BasePtrFromXvectBasePtr(xVect_BasePtr(ptr));
}

template <class T> void xTens_Free(T *& ptr) {

	if (ptr) {
		free(xTens_BasePtr<T, 1>(ptr));
		ptr = nullptr;
	}
}


template <class T> T * xTens_Create(size_t i, size_t j) {

	size_t dimArray[2] = {i, j};
	return xTens_Create<T, 2>(dimArray);
}

template <class T> T * xTens_Create(size_t i, size_t j, size_t k) {

	size_t dimArray[3] = {i, j, k};
	return xTens_Create<T, 3>(dimArray);
}

template <class T> T * xTens_Create(size_t i, size_t j, size_t k, size_t l) {

	size_t dimArray[4] = {i, j, k, l};
	return xTens_Create<T, 4>(dimArray);
}

template <class T> T * xTens_Create(size_t i, size_t j, size_t k, size_t l, size_t m) {

	size_t dimArray[5] = {i, j, k, l, m};
	return xTens_Create<T, 5>(dimArray);
}

template <class T, size_t nDim> T * xTens_Create(size_t * dimArray) {
	
	T * t = nullptr;
	xTens_Redim<T, nDim>(t, dimArray);
	
	return t;
}

template <class T> inline void * xTens_BasePtrFromXvectBasePtr(const xVect_struct<T> * const xVectBasePtr) {

	const size_t nDim = xTens_nDimFromXvectBasePtr(xVectBasePtr);
	size_t * dimArray = (((size_t*)xVectBasePtr) - 1 - nDim);

	const size_t padding = Padding((double*)dimArray);
	dimArray = (size_t *)((uintptr_t)dimArray - padding);

	return (void*)dimArray;
}

template <class T> inline size_t * xTens_DimArray(T * ptr) {
	return (size_t*)xTens_BasePtrFromXvectBasePtr(xVect_BasePtr(ptr));
}
template <class T> inline size_t & xTens_nDimFromXvectBasePtr(const xVect_struct<T> * const ptr) {
	return *(((size_t*)ptr) - 1);
}
template <class T> inline size_t xTens_nDim(T * ptr) {
	return xTens_nDimFromXvectBasePtr(xVect_BasePtr(ptr));
}
template <class T> inline size_t xTens_nCol(T * ptr) {
	return xTens_DimArray(ptr)[1];
}
template <class T> inline size_t xTens_nRow(T * ptr) {
	return xTens_DimArray(ptr)[0];
}
template <class T> inline size_t xTens_Count(T * ptr) {
	return xVect_Count(ptr);
}

//#define XTENS_SIZE(T, count, nDim) (sizeof(xTens_struct<T>) + (count)*sizeof(T) + (nDim-1)*sizeof(((xTens_struct<T>*)0)->dims))
#define XTENS_SIZE(size, T, count, nDim) \
	SIZE_T_OVERFLOW_CHECK_MULT(count, sizeof(T)); \
	SIZE_T_OVERFLOW_CHECK_ADD(sizeof(xTens_struct<T, nDim>), count*sizeof(T)); \
	size = sizeof(xTens_struct<T, nDim>) - sizeof(T) + count*sizeof(T);

#define XTENS_SIZE_RT(size, T, count, nDim) \
	switch (nDim) { \
		case 1:	XTENS_SIZE(size, T, count, 1); break; \
		case 2:	XTENS_SIZE(size, T, count, 2); break; \
		case 3:	XTENS_SIZE(size, T, count, 3); break; \
		case 4:	XTENS_SIZE(size, T, count, 4); break; \
		case 5:	XTENS_SIZE(size, T, count, 5); break; \
		default: MY_ERROR("nDim>5 not implemented"); \
	}

template <class T, bool exactAlloc = false, bool reserve = false, bool initialize = true> 
void xTens_Redim(T *& t, size_t i, size_t j) {
	size_t dimArray[2] = {i, j};
	xTens_Redim<T, 2, exactAlloc, reserve, initialize>(t, dimArray);
}

template <class T, bool exactAlloc = false, bool reserve = false, bool initialize = true> 
void xTens_Redim(T *& t, size_t i, size_t j, size_t k) {
	size_t dimArray[3] = {i, j, k};
	xTens_Redim<T, 3>(t, dimArray);
}

template <class T, bool exactAlloc = false, bool reserve = false, bool initialize = true> 
void xTens_Redim(T *& t, size_t i, size_t j, size_t k, size_t l) {
	size_t dimArray[4] = {i, j, k, l};
	xTens_Redim<T, 4>(t, dimArray);
}

template <class T, bool exactAlloc = false, bool reserve = false, bool initialize = true>
void xTens_Redim(T *& t, size_t i, size_t j, size_t k, size_t l, size_t m) {
	size_t dimArray[5] = {i, j, k, l, m};
	xTens_Redim<T, 5>(t, dimArray);
}

template <class T> inline size_t Padding(const T * const ptr) {
	const size_t alignment = sizeof T;
	const size_t offset = (uintptr_t)ptr % alignment;
	const size_t padding = (alignment - offset) % alignment;
	return padding;
}

template <class T, bool exactAlloc = false, bool reserve = false> 
void xTens_Redim_Core(T *& t, const size_t nDim, size_t * dimArray) {

	size_t newCount = 1;
	for (size_t i = 0; i<nDim; ++i) {
		newCount *= dimArray[i];
	}

	union matBasePtrUnion {
		xTens_struct<T, 1> * d1;
		xTens_struct<T, 2> * d2;
		xTens_struct<T, 3> * d3;
		xTens_struct<T, 4> * d4;
		xTens_struct<T, 5> * d5;
	} matBasePtr;

	xVect_struct<T> * vectBasePtr;
	size_t allocatedCount;

	if (t) {
		vectBasePtr = xVect_BasePtr(t); 
		matBasePtr.d1 = (xTens_struct<T, 1>*)xTens_BasePtrFromXvectBasePtr(vectBasePtr);
		allocatedCount = vectBasePtr->allocatedCount;
	}
	else allocatedCount = 0;

	size_t finalCount;

	if (t) {
		if ((exactAlloc && allocatedCount != newCount) || newCount > allocatedCount) {
			if (reserve) {
				finalCount = newCount;
				newCount = vectBasePtr->count;
			}
			else if (exactAlloc) {
				finalCount = newCount;
			}
			else {
				SIZE_T_OVERFLOW_CHECK_ADD(newCount, allocatedCount);
				SIZE_T_OVERFLOW_CHECK_ADD(newCount + allocatedCount, 5);
				finalCount = newCount + allocatedCount + 5;
			}
			
			size_t s;
			XTENS_SIZE_RT(s, T, finalCount, nDim);

			matBasePtr.d1 = (xTens_struct<T, 1>*)realloc(matBasePtr.d1, s);
			if (!matBasePtr.d1) MY_ERROR("realloc failed");
		}
		else {
			vectBasePtr->count = newCount;
			return;
		}
	}
	else {
		finalCount = newCount;
		size_t s;
		XTENS_SIZE_RT(s, T, finalCount, nDim);
		matBasePtr.d1 = (xTens_struct<T, 1>*)malloc(s);
		if (!matBasePtr.d1) MY_ERROR("malloc failed");
	}

	size_t * dims;
	
	switch (nDim) {
		case 1:	dims = matBasePtr.d1->dims; vectBasePtr = &matBasePtr.d1->vect; break;
		case 2:	dims = matBasePtr.d2->dims; vectBasePtr = &matBasePtr.d2->vect; break;
		case 3:	dims = matBasePtr.d3->dims; vectBasePtr = &matBasePtr.d3->vect; break;
		case 4:	dims = matBasePtr.d4->dims; vectBasePtr = &matBasePtr.d4->vect; break;
		case 5:	dims = matBasePtr.d5->dims; vectBasePtr = &matBasePtr.d5->vect; break;
		default: MY_ERROR("nDim>5 not implemented");
	}

	for (size_t i = 0; i<nDim; ++i) {
		dims[i] = dimArray[i];
	}

	xTens_nDimFromXvectBasePtr(vectBasePtr) = nDim;

	vectBasePtr->allocatedCount = finalCount;
	vectBasePtr->count = newCount;

#ifdef _DEBUG		
	vectBasePtr->cookie = XVECT_COOKIE;
#endif

	t = &vectBasePtr->vector[0];

	if (std::is_fundamental<T>::value && Padding(t) != 0) MY_ERROR("fundamental type non aligned");
}

template <class T, bool exactAlloc = false, bool reserve = false, bool initialize = true>
void xTens_Redim(T *& t, const size_t nDim, size_t * dimArray) {

	if (!std::is_trivial<T>::value) {

		size_t newCount = 1;
		for (size_t i = 0; i<nDim; ++i) {
			newCount *= dimArray[i];
		}

		size_t oldCount = xVect_Count(t);

		if (newCount < oldCount) {
			for (size_t i = newCount; i<oldCount; ++i) {
				(t + i)->~T();
			}
			xTens_Redim_Core<T, exactAlloc, reserve>(t, nDim, dimArray);
		}
		else if (oldCount < newCount) {
			xTens_Redim_Core<T, exactAlloc, reserve>(t, nDim, dimArray);
			if (initialize) {
				for (size_t i = oldCount; i<newCount; i++) {
					new(t + i) T;
				}
			}
		}
	}
	else {
		xTens_Redim_Core<T, exactAlloc, reserve>(t, nDim, dimArray);
	}
}

template <class T, size_t nDim, bool exactAlloc = false, bool reserve = false, bool initialize = true> 
void xTens_Redim(T *& t, size_t * dimArray) {
	xTens_Redim<T, exactAlloc, reserve, initialize>(t, nDim, dimArray);
}

//
//template <class T, size_t nDim> void xTens_Redim(T *& t, size_t * dimArray, bool exactAlloc = false) {
//	
//	size_t newCount = 1;
//	for (size_t i = 0; i<nDim; ++i) {
//		newCount *= dimArray[i];
//	}
//
//	xTens_struct<T, nDim> * matBasePtr;
//	xVect<T> * vectBasePtr;
//	size_t allocatedCount;
//
//	if (t) {
//		matBasePtr = xTens_BasePtr<T, nDim>(t);
//		vectBasePtr = xVect_BasePtr(t);
//		allocatedCount = vectBasePtr->allocatedCount;
//	}
//	else allocatedCount = 0;
//
//	size_t finalCount;
//
//	if (t) {
//		if (newCount > allocatedCount || exactAlloc) {
//			finalCount = exactAlloc ? newCount : newCount + allocatedCount + 5;
//
//			size_t s;
//			XTENS_SIZE(s, T, finalCount, nDim);
//			matBasePtr = (xTens_struct<T, nDim> *)realloc(matBasePtr, s);
//			if (!matBasePtr) MY_ERROR("realloc failed");
//		}
//		else {
//			vectBasePtr->count = newCount;
//			return;
//		}
//	}
//	else {
//		finalCount = newCount;
//		size_t s;
//		XTENS_SIZE(s, T, finalCount, nDim);
//		matBasePtr = (xTens_struct<T, nDim> *)malloc(s);
//		if (!matBasePtr) MY_ERROR("malloc failed");
//	}
//
//	for (size_t i = 0; i<nDim; ++i) {
//		matBasePtr->dims[i] = dimArray[i];
//	}
//
//	//matBasePtr = (xTens_struct<T> *)((size_t *)matBasePtr + (nDim - 1));
//
//	//matBasePtr->nDim = nDim;
//
//	*(((size_t*)&matBasePtr->vect) - 1) = nDim;
//
//	matBasePtr->vect.allocatedCount = finalCount;
//	matBasePtr->vect.count = newCount;
//
//#ifdef _DEBUG		
//	matBasePtr->vect.cookie = XVECT_COOKIE;
//#endif
//
//	t = &matBasePtr->vect.vector[0];
//
//	if (std::is_fundamental<T>::value && Padding(t) != 0) MY_ERROR("fundamental type non aligned");
//}

template <class T, bool leak = false> class bTens {

	T * pData;
	xTens_Accessor<T> accessor;
	
public:

	bTens(bTens & that) {//;//TODO
		switch (that.nDim()) {
			case 2: this->pData = xTens_Create<T, 2>(that.DimArray()); break;
			case 3: this->pData = xTens_Create<T, 3>(that.DimArray()); break;
			case 4: this->pData = xTens_Create<T, 4>(that.DimArray()); break;
			case 5: this->pData = xTens_Create<T, 5>(that.DimArray()); break;
			default:MY_ERROR("nDim>5");
		}
		
		memmove(this->pData, that.pData, that.Count() * sizeof T);
		this->accessor.Wrap(this->pData);
	}

	bTens() {
		this->pData = nullptr;
	}

	bTens(size_t I, size_t J) {
		this->pData = xTens_Create<T>(I, J);
		this->accessor.Wrap(this->pData);
	}

	bTens(size_t I, size_t J, size_t K) {
		this->pData = xTens_Create<T>(I, J, K);
		this->accessor.Wrap(this->pData);
	}

	bTens(size_t I, size_t J, size_t K, size_t L) {
		this->pData = xTens_Create<T>(I, J, K, L);
		this->accessor.Wrap(this->pData);
	}

	~bTens() {
		if (!leak) this->Free();
	}

	T & operator()(size_t i, size_t j) {
		return this->accessor(i, j);
	}

	T & operator()(size_t i, size_t j, size_t k) {
		return this->accessor(i, j, k);
	}

	T & operator()(size_t i, size_t j, size_t k, size_t l) {
		return this->accessor(i, j, k, l);
	}

	operator T * () {
		return pData;
	}

	operator bool() {
		return pData != nullptr;
	}

	size_t Count() {
		return xVect_Count(this->pData);
	}

	size_t nDim() {
		return xTens_nDim(this->pData);
	}

	size_t Dim(size_t n) {
		return xTens_DimArray(this->pData)[n];
	}

	size_t * DimArray() {
		return xTens_DimArray(this->pData);
	}

	template <bool exactAlloc = false, bool reserve = false, bool initialize = true>
	bTens & Redim(size_t I, size_t J) {
		xTens_Redim<T, exactAlloc, reserve, initialize>(this->pData, I, J);
		this->accessor.Wrap(this->pData);
		return *this;
	}

	template <bool exactAlloc = false, bool reserve = false, bool initialize = true>
	bTens & Redim(size_t I, size_t J, size_t K) {
		xTens_Redim<T, exactAlloc, reserve, initialize>(this->pData, I, J, K);
		this->accessor.Wrap(this->pData);
		return *this;
	}

	template <bool exactAlloc = false, bool reserve = false, bool initialize = true>
	bTens & Redim(size_t I, size_t J, size_t K, size_t L) {
		xTens_Redim<T, exactAlloc, reserve, initialize>(this->pData, I, J, K, L);
		this->accessor.Wrap(this->pData);
		return *this;
	}

	template <class U> bTens & Redim(U & t) {
		xTens_Redim(this->pData, t.nDim(), t.DimArray());
		this->accessor.Wrap(this->pData);
		return *this;
	}

	bTens & Free() {
		xTens_Free(this->pData);
		accessor.Reset();
		return *this;
	}

	bTens & Leak() {
		this->pData = nullptr;
		this->accessor.Reset();
		return *this;
	}

	bTens & Wrap(T * t) {
		this->Free();
		this->pData = t;
		this->accessor.Wrap(t);
		return *this;
	}

	T * ReturnOwnership() {
		T * ptr = this->pData;
		this->Leak();
		return ptr;
	}

	bTens & Set(T val) {
		xVect_Set(this->pData, val);
		return *this;
	}

	//doesn't really make sense for matrix...
	//bTens & Reserve(size_t howMuch) {
	//	xTens_Redim<T, false, true, false>(this->pData, xVect_Count(this->pData) + howMuch);
	//	return *this;
	//}
};

template <class T> void DisplayMatrix(const aMat<T> & M, unsigned prec = 5, const char * fileName = nullptr) {

	//static aVect<char> fstr, buf;
	aVect<char> fstr, buf;

	//bool is_mpf_class = std::is_same<T, mpf_class>::value;
	bool is_mpf_class = false;
	char * formatSpecifier = is_mpf_class ? "Fg" : "g";

	if (prec == -1) fstr.sprintf("%%.%s", formatSpecifier);
	else  fstr.sprintf("%%%d.%d%s", prec + 5, prec, formatSpecifier);
	buf.Redim(0);

	for (size_t i = 0, I = M.nRow(); i < I; ++i) {
		buf.Append(i == 0 ? "      [ " : "        ");
		for (size_t j = 0, J = M.nRow(); j < J; ++j) {
			if (j != 0) buf.Append("    ");
			/*if (is_mpf_class) {
			//static aVect<char> locBuf;
			aVect<char> locBuf;
			size_t length = gmp_snprintf(nullptr, 0, fstr, get_mpf_t(M(i, j)));
			locBuf.Redim(length + 1);
			gmp_snprintf(locBuf, locBuf.Count(), fstr, get_mpf_t(M(i, j)));
			buf.Append(locBuf);
			}
			else*/ buf.Append(fstr, M(i, j));
		}
		buf.Append(i == I - 1 ? " ]\n" : " ;\n");
	}
	buf.Append("\n");

	if (fileName) {
		File file(fileName);
		file.Write(buf);
	}
	else printf("%s", (char*)buf);
}

inline void Gradient(aMat<Point<double, 2>> & grad, aMat<double> & matrix) {

	grad.Redim(matrix.nRow() - 1, matrix.nCol() - 1);

	aMat_static_for(grad, i, j) {
		grad(i, j)[0] = matrix(i + 1, j) - matrix(i, j);
		grad(i, j)[1] = matrix(i, j + 1) - matrix(i, j);
	}
}

inline aMat<Point<double, 2>> Gradient(aMat<double> & matrix) {

	aMat<Point<double, 2>> grad;

	Gradient(grad, matrix);

	return grad;
}

inline void Laplacian(aMat<double> & laplacian, aMat<double> & matrix) {

	laplacian.Redim(matrix.nRow() - 1, matrix.nCol() - 1);

	auto grad = Gradient(matrix);

	aMat_static_for(laplacian, i, j) {
		laplacian(i, j) = grad(i + 1, j)[0] - grad(i, j)[0] + grad(i + 1, j)[1] - grad(i, j)[1];
	}
}

inline aMat<double> Laplacian(aMat<double> & matrix) {

	aMat<double> laplacian;

	Laplacian(laplacian, matrix);

	return laplacian;
}

#endif

