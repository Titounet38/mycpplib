
#ifndef _MYUTILS_
#define _MYUTILS_

#include "time.h"
#include "ctype.h"
#include <limits>
#include "xVect.h"
#include <algorithm>

#ifdef SIMULATE_NO_WIN32
#define _WIN32_BKP   _WIN32
#define _MSC_VER_BKP _MSC_VER
#undef _WIN32
#undef _MSC_VER
#endif

#define NUMEL(a) (sizeof(a)/sizeof((a)[0]))
#define BIT(n) ((long long int)1<<((n)-1)) 

CharPointer MyGetLine(FILE * f);
void RemoveUnneededExposantChars(char * ptr1);

void GetDateStr(aVect<char> & str, char * formatString = "%.2d/%.2d/%.4d");
aVect<char> GetDateStr(char * formatString = "%.2d/%.2d/%.4d");
void GetTimeStr(aVect<char> & str, char * formatString = "%.2d:%.2d:%.2d");
aVect<char> GetTimeStr(char * formatString = "%.2d:%.2d:%.2d");
aVect<char> NiceTime(double secondes, bool noDays = false, bool integerSeconds = true, bool noSeconds = false);

void NiceFloat(aVect<char> & str, double tol = 1e-6);

template <class T>
void NiceFloat(aVect<char> & str, T&& val, double tol = 1e-6) {

	RemoveDecimalsWithPrec((double)val, str, tol, true);
	//RemoveUnneededExposantChars(str);
}

template <class T>
aVect<char> NiceFloat(T&& val, double tol = 1e-6) {

	aVect<char> str;
	NiceFloat(str, (double)val, tol);
	return std::move(str);
}

//aVect<char> DoubleToString(double val, double tol = 1e-6);
//void DoubleToString(aVect<char> & str, double val, double tol = 1e-6);
void RemoveDecimalsWithPrec(aVect<char> & str, double tol = 1e-6, bool removeUnneededExposantChars = true);
void RemoveDecimalsWithPrec(double val, aVect<char> & str, double tol = 1e-6, bool removeUnneededExposantChars = true);
//int strcmp_caseInsensitive(const char * str1, const char * str2);
int strcmp_caseInsensitive(const wchar_t * str1, const wchar_t * str2);
//char * strstr_caseInsensitive(const char * str, const char * subStr);
char * reverse_strstr(char * str, char * subStr, bool caseSensitive = true);
char * reverse_strstr_caseInsensitive(char * str, char * subStr);

//size_t ReplaceStr(aVect<char> & str, char * subStr, char * replaceStr, bool onlyFirstOccurence = false, bool caseSensitive = true, bool reverse = false, size_t startFrom = 0);
//
//size_t ReplaceStr_CaseInsensitive(aVect<char> & str, char * subStr, char * replaceStr, bool onlyFirstOccurence = false, size_t startFrom = 0);
//size_t ReplaceStr_FirstOccurence(aVect<char> & str, char * subStr, char * replaceStr, bool caseSensitive = true, size_t startFrom = 0);
//size_t ReplaceStr_LastOccurence(aVect<char> & str, char * subStr, char * replaceStr, bool caseSensitive = true, size_t startFrom = 0);
//size_t ReplaceStr_FirstOccurence_CaseInsensitive(aVect<char> & str, char * subStr, char * replaceStr, size_t startFrom = 0);
//size_t ReplaceStr_LastOccurence_CaseInsensitive(aVect<char> & str, char * subStr, char * replaceStr, size_t startFrom = 0);
//
//aVect<char> ReplaceStr(char * str, char * subStr, char * replaceStr, bool onlyFirstOccurence = false, bool caseSensitive = true, bool reverse = false, size_t startFrom = 0);
//
//aVect<char> ReplaceStr_CaseInsensitive(char * str, char * subStr, char * replaceStr, bool onlyFirstOccurence = false, bool reverse = false, size_t startFrom = 0);
//aVect<char> ReplaceStr_FirstOccurence(char * str, char * subStr, char * replaceStr, bool caseSensitive = true, size_t startFrom = 0);
//aVect<char> ReplaceStr_LastOccurence(char * str, char * subStr, char * replaceStr, bool caseSensitive = true, size_t startFrom = 0);
//aVect<char> ReplaceStr_FirstOccurence_CaseInsensitive(char * str, char * subStr, char * replaceStr, size_t startFrom = 0);
//aVect<char> ReplaceStr_LastOccurence_CaseInsensitive(char * str, char * subStr, char * replaceStr, size_t startFrom = 0);

#define CHECKNUM_ISNUMBER		BIT(1)
#define CHECKNUM_CONTAINSNUMBERFIRST BIT(2)
#define CHECKNUM_ISINF   		BIT(3)

bool SeparateNumber(aVect<char> & str);
bool IsNumber(const char * str, bool strict = true);
bool IsInf(const char * str);
char CheckNumber(aVect<char> & str, bool separate, bool strict);

bool SeparateNumber(aVect<wchar_t> & str);
bool IsNumber(const wchar_t * str, bool strict = true);
bool IsInf(const wchar_t * str);
wchar_t CheckNumber(aVect<wchar_t> & str, bool separate, bool strict);

void SetPromptBeforeExit();


template <class T> T * AlloCopy(T * el, unsigned int count) {

	unsigned int n = count*sizeof(T);
	T * retVal = (T*)malloc(n);
	if (!retVal) MY_ERROR("malloc = NULL");
	memcpy(retVal, el, n);
	return retVal;
}

template <class T> T * AlloCopy(T * el) {
	return AlloCopy(el, 1);
}

template<bool checkType, class U>
struct Strlen_Check {
	static_assert(!checkType, "wrong type");
};

template<bool checkType> struct Strlen_Check<checkType, wchar_t> {};
template<bool checkType> struct Strlen_Check<checkType, const wchar_t> {};
template<bool checkType> struct Strlen_Check<checkType, char> {};
template<bool checkType> struct Strlen_Check<checkType, const char> {};

template<bool checkType = true, class U>
size_t Strlen(U * ptr) {
	Strlen_Check<checkType, U>();
	return Strlen_Core(ptr);
}

template<class U>
size_t Strlen_Core(U * ptr) {
	MY_ERROR("wrong type");
	return 0;
}

template<>
inline size_t Strlen_Core<wchar_t>(wchar_t * ptr) {
	if (ptr) return wcslen(ptr);
	return 0;
}

template<>
inline size_t Strlen_Core<char>(char * ptr) {
	if (ptr) return strlen(ptr);
	return 0;
}

template<>
inline size_t Strlen_Core<const wchar_t>(const wchar_t * ptr) {
	if (ptr) return wcslen(ptr);
	return 0;
}

template<>
inline size_t Strlen_Core<const char>(const char * ptr) {
	if (ptr) return strlen(ptr);
	return 0;
}


template <int nSep, char sep1, char sep2, char sep3, char sep4, char sep5>
char * RetrieveStr(char *& str_orig, bool strict = false) {
	//static CharPointer ptr, retVal;
	CharPointer ptr, retVal;
	ptr = str_orig;
	retVal = RetrieveStr<CharPointer, char, nSep, sep1, sep2, sep3, sep4, sep5>(ptr, strict);
	str_orig = *(char**)&ptr;
	return *(char**)&retVal;
}


template <int nSep, wchar_t sep1, wchar_t sep2, wchar_t sep3, wchar_t sep4, wchar_t sep5>
wchar_t * RetrieveStr(wchar_t *& str_orig, bool strict = false) {
	//static WCharPointer ptr, retVal;
	WCharPointer ptr, retVal;
	ptr = str_orig;
	retVal = RetrieveStr<WCharPointer, wchar_t, nSep, sep1, sep2, sep3, sep4, sep5>(ptr, strict);
	str_orig = *(wchar_t**)&ptr;
	return *(wchar_t**)&retVal;
}


template <class CharPtrClass, class T, T nSep, T sep1, T sep2, T sep3, T sep4, T sep5>
CharPtrClass RetrieveStr(CharPtrClass & str_orig, bool strict = false) {

	//static CharPtrClass emptyVect;
	CharPtrClass emptyVect = 0;

    if (!str_orig) return emptyVect;
    //T * str_orig = str_orig_;
	T * str = str_orig;
    //size_t len_str = Strlen(str);
	bool cond = false;
	size_t i=0, j=0, k=0;

	if (*str == 0) str_orig.Free();

	if (strict) {
		cond = true;
	}
	else {
		for (i=0 ; /*i<len_str*/ ; ++i) {

			T c = str[i];

			if (!c) break;
		
			switch (nSep) {
				case 1: cond = (c != sep1);break;
				case 2: cond = (c != sep1 && c != sep2);break;
				case 3: cond = (c != sep1 && c != sep2 && c != sep3);break;
				case 4: cond = (c != sep1 && c != sep2 && c != sep3 && c != sep4);break;
				case 5: cond = (c != sep1 && c != sep2 && c != sep3 && c != sep4 && c != sep5);break;
				default: MY_ERROR("5 separators max");
			}

			if (cond) break;
		}
	}

    if (cond) {
		cond = false;
        str_orig.Free();

        for (j=i ; /*j < len_str*/ ; ++j) {

			T c = str[j];
			if (!c) break;

            switch (nSep) {
                case 1: cond = (c == sep1);break;
                case 2: cond = (c == sep1 || c == sep2);break;
                case 3: cond = (c == sep1 || c == sep2 || c == sep3);break;
                case 4: cond = (c == sep1 || c == sep2 || c == sep3 || c == sep4);break;
                case 5: cond = (c == sep1 || c == sep2 || c == sep3 || c == sep4 || c == sep5);break;
                default: MY_ERROR("5 separators max");
            }

            if (cond) {
				if (strict) {
					str_orig = &str[j+1]; // str_orig.Wrap(&str[j+1]);
				}
				else {
					cond = false;
					for (k=j+1 ; /*k<len_str*/ ; ++k) {

						T c = str[k];
						if (!c) break;
		
						switch (nSep) {
							case 1: cond = (c != sep1);break;
							case 2: cond = (c != sep1 && c != sep2);break;
							case 3: cond = (c != sep1 && c != sep2 && c != sep3);break;
							case 4: cond = (c != sep1 && c != sep2 && c != sep3 && c != sep4);break;
							case 5: cond = (c != sep1 && c != sep2 && c != sep3 && c != sep4 && c != sep5);break;
							default: MY_ERROR("5 separators max");
						}

						if (cond) break;
					}
					str_orig = &str[k];
					if (*str_orig == 0) str_orig.Free();
				}
				
                break;
            }
        }
        ((T *) &str[0])[j] = 0;
        /*CharPointer retVal = */;
		return &str[i];//retVal;
    }

    return emptyVect;
}

template <char sep1, char sep2, char sep3, char sep4, char sep5> CharPointer RetrieveStr(CharPointer & str_orig, bool strict = false) {
    return RetrieveStr<5, sep1, sep2, sep3, sep4, sep5>(str_orig, strict);
}

template <char sep1, char sep2, char sep3, char sep4> CharPointer RetrieveStr(CharPointer & str_orig, bool strict = false) {
    return RetrieveStr<4, sep1, sep2, sep3, sep4, 0>(str_orig, strict);
}

template <char sep1, char sep2, char sep3> CharPointer RetrieveStr(CharPointer & str_orig, bool strict = false) {
    return RetrieveStr<3, sep1, sep2, sep3, 0, 0>(str_orig, strict);
}

template <char sep1, char sep2> CharPointer RetrieveStr(CharPointer & str_orig, bool strict = false) {
    return RetrieveStr<2, sep1, sep2, 0, 0, 0>(str_orig, strict);
}

template <char sep1> CharPointer RetrieveStr(CharPointer & str_orig, bool strict = false) {
    return RetrieveStr<1, sep1, 0, 0, 0, 0>(str_orig, strict);
}

CharPointer RetrieveStr(CharPointer & str_orig, bool strict = false);

template <char sep1, char sep2, char sep3, char sep4, char sep5> char * RetrieveStr(char * & str_orig, bool strict = false) {
    return RetrieveStr<5, sep1, sep2, sep3, sep4, sep5>(str_orig, strict);
}

template <char sep1, char sep2, char sep3, char sep4> char * RetrieveStr(char * & str_orig, bool strict = false) {
    return RetrieveStr<4, sep1, sep2, sep3, sep4, 0>(str_orig, strict);
}

template <char sep1, char sep2, char sep3> char * RetrieveStr(char * & str_orig, bool strict = false) {
    return RetrieveStr<3, sep1, sep2, sep3, 0, 0>(str_orig, strict);
}

template <char sep1, char sep2> char * RetrieveStr(char * & str_orig, bool strict = false) {
    return RetrieveStr<2, sep1, sep2, 0, 0, 0>(str_orig, strict);
}

template <char sep1> char * RetrieveStr(char * & str_orig, bool strict = false) {
    return RetrieveStr<1, sep1, 0, 0, 0, 0>(str_orig, strict);
}

char * RetrieveStr(char * & str_orig, bool strict = false);

//Wide char versions
template <wchar_t sep1, wchar_t sep2, wchar_t sep3, wchar_t sep4, wchar_t sep5> WCharPointer RetrieveStr(WCharPointer & str_orig, bool strict = false) {
	return RetrieveStr<5, sep1, sep2, sep3, sep4, sep5>(str_orig, strict);
}

template <wchar_t sep1, wchar_t sep2, wchar_t sep3, wchar_t sep4> WCharPointer RetrieveStr(WCharPointer & str_orig, bool strict = false) {
	return RetrieveStr<4, sep1, sep2, sep3, sep4, 0>(str_orig, strict);
}

template <wchar_t sep1, wchar_t sep2, wchar_t sep3> WCharPointer RetrieveStr(WCharPointer & str_orig, bool strict = false) {
	return RetrieveStr<3, sep1, sep2, sep3, 0, 0>(str_orig, strict);
}

template <wchar_t sep1, wchar_t sep2> WCharPointer RetrieveStr(WCharPointer & str_orig, bool strict = false) {
	return RetrieveStr<2, sep1, sep2, 0, 0, 0>(str_orig, strict);
}

template <wchar_t sep1> WCharPointer RetrieveStr(WCharPointer & str_orig, bool strict = false) {
	return RetrieveStr<1, sep1, 0, 0, 0, 0>(str_orig, strict);
}

WCharPointer RetrieveStr(WCharPointer & str_orig, bool strict = false);

template <wchar_t sep1, wchar_t sep2, wchar_t sep3, wchar_t sep4, wchar_t sep5> wchar_t * RetrieveStr(wchar_t * & str_orig, bool strict = false) {
	return RetrieveStr<5, sep1, sep2, sep3, sep4, sep5>(str_orig, strict);
}

template <wchar_t sep1, wchar_t sep2, wchar_t sep3, wchar_t sep4> wchar_t * RetrieveStr(wchar_t * & str_orig, bool strict = false) {
	return RetrieveStr<4, sep1, sep2, sep3, sep4, 0>(str_orig, strict);
}

template <wchar_t sep1, wchar_t sep2, wchar_t sep3> wchar_t * RetrieveStr(wchar_t * & str_orig, bool strict = false) {
	return RetrieveStr<3, sep1, sep2, sep3, 0, 0>(str_orig, strict);
}

template <wchar_t sep1, wchar_t sep2> wchar_t * RetrieveStr(wchar_t * & str_orig, bool strict = false) {
	return RetrieveStr<2, sep1, sep2, 0, 0, 0>(str_orig, strict);
}

template <wchar_t sep1> wchar_t * RetrieveStr(wchar_t * & str_orig, bool strict = false) {
	return RetrieveStr<1, sep1, 0, 0, 0, 0>(str_orig, strict);
}

wchar_t * RetrieveStr(wchar_t * & str_orig, bool strict = false);

template <class T1, class T2> auto Max(T1 t1, T2 t2) -> typename std::remove_reference<decltype(t1 > t2 ? t1 : t2)>::type {

	return t1 > t2 ? t1 : t2;
}

template <class T1, class T2> auto Min(T1 t1, T2 t2) -> typename std::remove_reference<decltype(t1 < t2 ? t1 : t2)>::type {

	return t1 < t2 ? t1 : t2;
}

template <class T> void Swap(T& t1, T& t2) {
	PlaceHolder<T> tmp;
	tmp = *(PlaceHolder<T>*)&t1;
	*(PlaceHolder<T>*)&t1 = *(PlaceHolder<T>*)&t2;
	*(PlaceHolder<T>*)&t2 = *(PlaceHolder<T>*)&tmp;
}

//template <class T> void Swap(T& t1, T& t2) {
//	if (std::is_trivial<T>::value) {
//		T tmp;
//		tmp = t1;//		t2 = tmp;
//	}

//		t1 = t2;
//	else {
//		PlaceHolder<T> tmp;
//		PlaceHolder<T> * p1 = (PlaceHolder<T> *)&t1;
//		PlaceHolder<T> * p2 = (PlaceHolder<T> *)&t2;
//		tmp = *p1;
//		*p1 = *p2;
//		*p2 = tmp;
//	}
//}

bool IsEqualWithPrec(double a, double b, double prec);

int MyRound(double x);
#define SET_DWORD_BYTE_MASK(a, mask, b) {(a) = ((a) & ~(mask)) | ( ((b) ?  0xFFFFFFFF : 0) & (mask));}

#define SET_BYTE_MASK(a, mask, b) {(a) = (b) ? (a) | (mask) : (a) & ~(mask);}

#define ASSERT_ALWAYS(a, b)                                                     \
    if (!(a)) {                                                                 \
        MY_ERROR(b);															\
    }


#define MyAssert(a)														    \
if (!(a)) {																	\
	MY_ERROR(#a);															\
}


#ifndef _XVECT_
#define UINT_OVERFLOW_CHECK_AFFECT(a) (a);									\
{																			\
	if (((double)(a)) > ((double) UINT_MAX)) {								\
		MY_ERROR(aVect<char>("unsigned overflow affectation : %f > %u",		\
		((double)(a)), UINT_MAX));											\
	}																		\
}																			

#define UINT_OVERFLOW_CHECK_MULT(a, b) {								\
	if ((a) > UINT_MAX/(b)) {											\
		MY_ERROR(aVect<char>("unsigned overflow : %u * %u", (a), (b)));	\
	}																	\
}

#define UINT_OVERFLOW_CHECK_ADD(a, b) {									\
	if (UINT_MAX - (a) < (b)) {											\
		MY_ERROR(aVect<char>("unsigned overflow : %u + %u", (a), (b)));	\
	}																	\
}

#define UINT_OVERFLOW_CHECK_SUBSTRACT(a, b) {							\
	if ((a) < (b)) {													\
	MY_ERROR(aVecct<char>("unsigned overflow : %u - %u", (a), (b)));	\
	}																	\
}

#define SIZE_T_OVERFLOW_CHECK_AFFECT(a) (a);								\
{																			\
	if (((double)(a)) > ((double) SIZE_MAX)) {								\
		MY_ERROR(aVect<char>("unsigned overflow affectation : %f > %u",		\
		((double)(a)), SIZE_MAX));											\
	}																		\
}																			

#define SIZE_T_OVERFLOW_CHECK_MULT(a, b) {								\
	if ((a) > SIZE_MAX/(b)) {											\
		MY_ERROR(aVect<char>("unsigned overflow : %u * %u", (a), (b)));	\
	}																	\
}

#define SIZE_T_OVERFLOW_CHECK_ADD(a, b) {								\
	if (SIZE_MAX - (a) < (b)) {											\
		MY_ERROR(aVect<char>("unsigned overflow : %u + %u", (a), (b)));	\
	}																	\
}

#define SIZE_T_OVERFLOW_CHECK_SUBSTRACT(a, b) {							\
	if ((a) < (b)) {													\
	MY_ERROR(aVecct<char>("unsigned overflow : %u - %u", (a), (b)));	\
	}																	\
}
#endif

#define SIZE_T_OVERFLOW_CHECK_AFFECT_(a) (a);								\
{																			\
	if (((double)(a)) > ((double) SIZE_MAX)) {								\
		MY_ERROR("unsigned overflow affectation");							\
	}																		\
}																			

#define SIZE_T_OVERFLOW_CHECK_MULT_(a, b) {								\
	if ((a) > SIZE_MAX/(b)) {											\
		MY_ERROR("unsigned overflow");									\
	}																	\
}

#define SIZE_T_OVERFLOW_CHECK_ADD_(a, b) {								\
	if (SIZE_MAX - (a) < (b)) {											\
		MY_ERROR("unsigned overflow "));								\
	}																	\
}

#define SIZE_T_OVERFLOW_CHECK_SUBSTRACT_(a, b) {						\
	if ((a) < (b)) {													\
		MY_ERROR("unsigned overflow", (a), (b)));						\
	}																	\
}

class File {
	
protected:

	aVect<char> filePath, openMode;
	FILE * file;
	long long fileSize;
	int buf_nEl;
    char * buf;
    char * ptrStart;
    char * curPos;
    char * ptrEndRead;
    bool endOfFile;
	bool cachedData;
	bool writeMode;
	clock_t lastFlush;

	aVect<char> lineContinuationBuffer;

	File & Init();
	File & Reset();
	File & SetOpenMode(const char * om);

	virtual void CloseCore();
	virtual void OpenCore(const char * fp, const char * om);
	virtual void FlushCore();
	virtual size_t ReadCore(void*, size_t, size_t);
	virtual size_t WriteCore(void*, size_t, size_t);
	virtual bool File::EofCore();
	CharPointer GetLineCore(bool errorIfEndOfFile = false, bool nullEndLine = true);

	File & operator =(const File & f) = delete;
	File(const File & f) = delete;

public:

	File();
	File(const char * fn);
	File(const char * fn, const char * om);
	File(File && f);
	File & operator =(File && f);
	virtual ~File();

	virtual operator bool();
	
	long long GetSize();
	long long GetCurrentPosition();
	void Flush();

	File & Close();
	File & SetFile(const char * fp, const char * om = nullptr);
	aVect<char> GetFilePath() const;
	aVect<char> GetOpenMode();
	bool IsWriteMode(const char * om);
	bool IsReadMode(const char * om);
	bool IsAppendMode(const char * om);
	bool IsBinaryMode(const char * om);
	bool Eof();
	virtual File & Open(const char * fp = nullptr, const char * om = nullptr);
	operator FILE*();
	const FILE* GetFILEptr();
	FILE* GetFILEptr_UNSAFE();
	size_t Write(const char * str);
	size_t Read(aVect<char> & data, size_t n);
	template <char lineContinuation = 0, bool trim = false, char lineComment = 0, bool olgaStyle = false> 
	CharPointer GetLine(bool errorIfEndOfFile = false, bool nullEndLine = true); //nullEndLine = false => !Beware! No Final '\0' !!! For very specific use (mix of text/binary)
	CharPointer GetLastLine();
	void PosChanged();

	template <class T, class... Args>
	size_t Write(T* format, Args&&... args) {

		static aVect<std::remove_const<T>::type> buffer;
		static WinCriticalSection cs;

		Critical_Section(cs) {
			return this->Write(buffer.Format(format, std::forward<Args>(args)...));
		}

		//unreachable code, but this makes compiler happy
		return -1;
	}

	template <class T>
	size_t Write(T* str) {

		static aVect<std::remove_const<T>::type> buffer;
		static WinCriticalSection cs;

		Critical_Section(cs) {
			return this->Write(buffer.Copy(str));
		}
	}

	template <bool useActualCount = false, class T>
	size_t Write(aVect<T> & data) {
		if (!*this) {
			openMode.sprintf("w");
			if (!Open()) MY_ERROR(aVect<char>("Unable to open \"%s\"", (char*)filePath));
		}
		if (!data) return 0;
		size_t size;

		static_assert(!useActualCount || (std::is_same<T, char>::value || std::is_same<T, wchar_t>::value), "useActualCount = true is only intended for aVect<char> or aVect<wchar_t>");

		if (!useActualCount && (std::is_same<T, char>::value || std::is_same<T, wchar_t>::value)) size = (sizeof T) * (data.Last() == 0 ? data.Count() - 1 : data.Count());
		else size = (sizeof T) * data.Count();
		return WriteCore(&data[0], size, 1);
	}
};

template <char lineContinuation, bool trim, char lineComment, bool olgaStyle>
CharPointer File::GetLine(bool errorIfEndOfFile, bool nullEndLine) {

	if (lineContinuation) {

		if (!nullEndLine) MY_ERROR("TODO");

		CharPointer line = this->GetLineCore(errorIfEndOfFile, nullEndLine);
		
		for (;;) {
			if (trim) line.Trim(); 
			
			//static_assert(!lineComment, "TODO");
			//if (lineComment) {
			//	if (auto ptr = strchr(line, lineComment)) {
			//		*ptr = '\n';
			//	}
			//}
			if (!line || line.Last(-1) != lineContinuation) break;
			this->lineContinuationBuffer.Copy(line).Grow(-1).Last() = 0;

			auto newLine = this->GetLineCore(errorIfEndOfFile, nullEndLine);
			if (newLine && lineComment) {
				if (auto ptr = strchr(newLine, lineComment)) {
					if (olgaStyle) {
						if (ptr == newLine) {
							this->lineContinuationBuffer.Last() = lineContinuation;
							this->lineContinuationBuffer.Push(0);
						}
					}
					*ptr = 0;
				}
			}

			this->lineContinuationBuffer.Append(newLine);
			line = this->lineContinuationBuffer;
		}
		return line;
	}
	else {
		auto retVal = GetLineCore(errorIfEndOfFile, nullEndLine);
		if (retVal && lineComment) {
			if (!nullEndLine) MY_ERROR("TODO");
			if (auto ptr = strchr(retVal, lineComment)) {
				if (olgaStyle) MY_ERROR("TODO");
				*ptr = 0;
			}
		}
		if (trim) {
			if (!nullEndLine) MY_ERROR("TODO");
			retVal.Trim();
		}

		return retVal;
	}
}

class LogFile {

private:

	struct VerboseLvl {
		int verboseLvl;
		bool pendingNewLine;
	};

	aVect<FILE *> logFiles;
	aVect< aVect<wchar_t> > filePaths;
	aVect<aVect<wchar_t> > openModes;
	aVect<int> verboseLvls;
	int dispVerboseLvl, printLvl;
	bool display, tmpDisplay;
	bool printTime, tmpPrintTime;
	bool printDate, tmpPrintDate;
	bool write, tmpWrite;
	aVect<VerboseLvl> knownVerboseLvls;

	LogFile & Init(char * fName, bool erase = true);
	LogFile & Init(wchar_t * fName = nullptr, bool erase = true);
	LogFile & Open();
	//LogFile & vPrintf(char * fs, va_list args);
	LogFile & AddIfUnkownVerboseLvl(int verboseLvl, bool pendingLine = false);
	LogFile &  LogFile::ResetPendingLine(int verboseLvl);
	bool HasPendingLine(int verboseLvl);

public:

	LogFile();
	~LogFile();
	LogFile(char * fPath, bool erase = true);
	LogFile(wchar_t * fPath, bool erase = true);
	LogFile & Close();
	LogFile & SetFile(char * fPath, int verboseLvl = 1000, bool erase = true);
	LogFile & SetFile(wchar_t * fPath, int verboseLvl = 1000, bool erase = true);
	LogFile & AddFile(char * fPath, int verboseLvl = 1000, bool erase = true);
	LogFile & AddFile(wchar_t * fPath, int verboseLvl = 1000, bool erase = true);
	LogFile & RemoveFile(size_t i, bool erase = true);
	aVect<wchar_t> GetFilePath(int i=0);
	size_t GetFilesCount();
	FILE * GetFile(int i=0);
	aVect< aVect<wchar_t> > GetFilePaths();
	LogFile & SetVerboseLvl(int vLvl, int i=0);
	LogFile & SetDisplayVerboseLvl(int vLvl);

	template <class... Args>
	LogFile & Printf(const wchar_t  * fs, Args&&... args) {

		this->RuntimeArgChecker(std::forward<Args>(args)...);

		bool allTrue = true;

		aVect_static_for(verboseLvls, i) {
			allTrue = allTrue && printLvl > verboseLvls[i];
		}

		if (allTrue && printLvl > dispVerboseLvl) return *this;

		//static aVect<wchar_t> buffer;
		aVect<wchar_t> buffer;

		if (tmpWrite) {
			if (!filePaths) MY_ERROR("Aucun fichier n'a été spécifié");
			Open();
		}

		if (tmpPrintDate) {
			this->PrintDate();
		}

		buffer.sprintf(fs, std::forward<Args>(args)...);

		if (tmpPrintTime) {
			//static aVect<char> timeStr;
			aVect<char> timeStr;
			GetTimeStr(timeStr);
			buffer.sprintf(L"%S%s%s", (char*)timeStr, buffer ? L"  " : L"", (wchar_t*)buffer);
		}

		aVect_static_for(verboseLvls, i) {
			if (printLvl <= verboseLvls[i] && tmpWrite) {
				if (HasPendingLine(verboseLvls[i])) {
					fwprintf(logFiles[i], L"\n");
				}
				fwprintf(logFiles[i], L"%s", (wchar_t*)buffer);
				fflush(logFiles[i]);
			}
		}

		if (printLvl <= dispVerboseLvl && tmpDisplay) {
			if (HasPendingLine(dispVerboseLvl)) {
				wprintf(L"\n");
			}
			wprintf(L"%s", (wchar_t*)buffer);
			ResetPendingLine(dispVerboseLvl);
		}

		aVect_static_for(verboseLvls, i) {
			if (printLvl <= verboseLvls[i] && tmpWrite) {
				ResetPendingLine(verboseLvls[i]);
			}
			openModes[i] = "a";
		}
		tmpPrintTime = printTime;
		tmpPrintDate = printDate;
		tmpDisplay = display;
		tmpWrite = write;
		return *this;
	}

	template <class... Args>
	LogFile & Printf(const char * fs, Args&&... args) {

		//static aVect<char> buffer;
		aVect<char> buffer;

		this->RuntimeArgChecker(std::forward<Args>(args)...);

		buffer.sprintf(fs, std::forward<Args>(args)...);

		return this->Printf(L"%S", (char*)buffer);

		return *this;
	}

	template <class... Args>
	LogFile & xPrintf(const char * fs, Args&&... args) {

		aVect<char> buffer;

		buffer.Format(fs, std::forward<Args>(args)...);

		return this->Printf(L"%S", (char*)buffer);

		return *this;
	}

	template <class... Args>
	LogFile & xPrintf(const wchar_t * fs, Args&&... args) {

		aVect<wchar_t> buffer;

		buffer.Format(fs, std::forward<Args>(args)...);

		return this->Printf(L"%s", (wchar_t*)buffer);

		return *this;
	}

	LogFile & PrintTime();
	LogFile & PrintDate();

	void RuntimeArgChecker() {}

	template <class ToCheck, class... Args> void RuntimeArgChecker(ToCheck toCheck, Args&&... args) {
		return;
		
		//if (std::is_same<ToCheck, char*>::value || std::is_same<ToCheck, wchar_t*>::value) {
		//	if (!toCheck) MY_ERROR("null string pointer");
		//	if (toCheck == 0) MY_ERROR("empty string pointer");
		//}
		//this->RuntimeArgChecker(std::forward<Args>(args)...);
	}


	template <class... Args>
	LogFile & Printf(int vLvl, const char * fs, Args&&... args) {

		AddIfUnkownVerboseLvl(vLvl);

		printLvl = vLvl;
		this->Printf(fs, std::forward<Args>(args)...);
		return *this;
	}

	template <class... Args>
	LogFile & Printf(int vLvl, const wchar_t * fs, Args&&... args) {

		AddIfUnkownVerboseLvl(vLvl);

		printLvl = vLvl;
		this->Printf(fs, std::forward<Args>(args)...);
		return *this;
	}

	LogFile & NoTime();
	LogFile & Time();
	LogFile & NoDate();
	LogFile & Date();
	LogFile & NoDisp();
	LogFile & Disp();
	LogFile & NoWrite();
	LogFile & Write();
	LogFile & DisableDisplay();
	LogFile & DisableTime();
	LogFile & DisableDate();
	LogFile & DisableWrite();
	LogFile & EnableDisplay();
	LogFile & EnableTime();
	LogFile & EnableDate();
	LogFile & EnableWrite();
	LogFile & SetPendingNewLine();
	LogFile & UnsetPendingNewLine();
	LogFile & SetPendingNewLine(int vLvl);
	LogFile & UnsetPendingNewLine(int vLvl);
	operator bool();
};


bool IsStringInFile(File & f, const char * str);

template <class T = int, bool min = true> class BinaryHeap {

protected:

	aVect<T> keys;

	size_t LeftChild(size_t i) {
		return 2 * i + 1;
	}

	size_t RightChild(size_t i) {
		return 2 * i + 2;
	}

	size_t Parent(size_t i) {
		return (i - 1) / 2;
	}

	void BubbleUp(size_t i) {
		size_t p = this->Parent(i);
		while (i > 0 && (min ? this->keys[i] < this->keys[p] : this->keys[i] > this->keys[p])) {
			Swap(this->keys[i], this->keys[p]);
			i = p;
			p = this->Parent(i);
		}
	}

	void TrickleDown(size_t i) {
		size_t n = this->keys.Count();
		while (true) {
			size_t r = this->RightChild(i);
			size_t l = this->LeftChild(i);
			size_t j = i;
			if (r < n && (min ? this->keys[r] < this->keys[j] : this->keys[r] > this->keys[j])) {
				j = r;
			}
			if (l < n && (min ? this->keys[l] < this->keys[j] : this->keys[l] > this->keys[j])) {
				j = l;
			}

			if (j == i) break;

			Swap(this->keys[i], this->keys[j]);

			i = j;
		}
	}

public:

	T & operator[](size_t i) {
		return this->keys[i];
	}

	BinaryHeap & Update(size_t i, T & newVal) {
		T & oldVal = this->keys[i];
		bool up = (min ? newVal < oldVal : newVal > oldVal);
		if (std::is_trivial<T>::value) {
			oldVal = newVal;
		}
		else {
			T newValCopy(newVal);
			Swap(newValCopy, oldVal);
		}
		up ? this->BubbleUp(i) : this->TrickleDown(i);
	}

	BinaryHeap & Delete(size_t i) {
		size_t n = keys.Count();
		if (!n) MY_ERROR("heap is empty");
		if (n == 1) {
			this->Reset();
			return *this;
		}
		T & thisKey = this->keys[i];
		T & lastKey = this->keys.Last();

		Swap(thisKey, lastKey);

		this->keys.Redim(n - 1);
		this->TrickleDown(i);
	}

	size_t Count() {
		return this->keys.Count();
	}

	BinaryHeap & Reset() {
		this->keys.Redim(0);
		return *this;
	}

	BinaryHeap & Free() {
		this->keys.Free();
		return *this;
	}

	operator bool() {
		return (bool)keys;
	}

	BinaryHeap & Push(T & x) {
		this->keys.Push(x);
		this->BubbleUp(this->keys.Count()-1);
		return *this;
	}

	T & PrePush() {
		return this->keys.Grow(1).Last();
	}

	void PostPush() {
		this->BubbleUp(this->keys.Count() - 1);
	}

	void Pop(T & x) {
		if (this->keys.Count() > 2) {
			this->Pop_Fast_bug_if_small(x);
		}
		else {
			this->Pop_slow(x);
		}
	}

	void Pop_slow(T & x) {
		if (!this->keys) MY_ERROR("heap is empty");
		Swap(x, this->keys[0]);
		if (this->keys.Count() > 1) {
			Swap(this->keys[0], this->keys[this->keys.Count() - 1]);
			this->keys.Grow(-1);
			this->TrickleDown(0);
		}
		else this->keys.Redim(0);
	}

	void Pop_Fast_bug_if_small(T & x) {
		if (!this->keys) MY_ERROR("heap is empty");
		
		size_t hole = 1;
		size_t succ = 2;
		size_t sz = this->keys.Count();
		T * data = this->keys;
		
		data--;

		Swap(x, data[1]);

		while (succ < sz) {
			T key1 = data[succ];
			T key2 = data[succ + 1];
			if (min ? key1 > key2 : key1 < key2) {
				succ++;
				data[hole] = key2;
			}
			else {
				data[hole] = key1;
			}
			hole = succ;
			succ *= 2;
		}

		T bubble = data[sz];
		size_t pred = hole / 2;

		while (min ? data[pred] > bubble : data[pred] < bubble) {
			data[hole] = data[pred];
			hole = pred;
			pred /= 2;
		}

		data[hole] = bubble;

		this->keys.Grow(-1);
	}

	T Pop() {
		T x;
		this->Pop(x);
		return x;
	}

	void Read(T & x) {
		if (!this->keys) MY_ERROR("heap is empty");
		x = this->keys[0];
	}

	T Read() {
		T x;
		this->Read(x);
		return x;
	}
};

template <class T = int, bool min = true> class UpdatableBinaryHeap : public BinaryHeap<T, min> {

	aVect<size_t> P;
	aVect<size_t> Pp;
	aVect<size_t> freeId;

	void BubbleUp(size_t i) {
		size_t p = this->Parent(i);
		while (i > 0 && (min ? this->keys[i] < this->keys[p] : this->keys[i] > this->keys[p])) {
			Swap(this->keys[i], this->keys[p]);
			Swap(this->P[i], this->P[p]);
			Swap(this->Pp[this->P[i]], this->Pp[this->P[p]]);
			i = p;
			p = this->Parent(i);
		}
	}

	void TrickleDown(size_t i) {
		size_t n = this->keys.Count();
		while (true) {
			size_t r = this->RightChild(i);
			size_t l = this->LeftChild(i);
			size_t j = i;
			if (r < n && (min ? this->keys[r] < this->keys[j] : this->keys[r] > this->keys[j])) {
				j = r;
			}
			if (l < n && (min ? this->keys[l] < this->keys[j] : this->keys[l] > this->keys[j])) {
				j = l;
			}

			if (j == i) break;

			Swap(this->keys[i], this->keys[j]);
			Swap(this->P[i], this->P[j]);
			Swap(this->Pp[this->P[i]], this->Pp[this->P[j]]);

			i = j;
		}
	}

public:

	UpdatableBinaryHeap & Free() {
		this->keys.Free();
		this->P.Free();
		this->Pp.Free();
		this->freeId.Free();
	}

	UpdatableBinaryHeap & Reset() {
		this->keys.Redim(0);
		this->P.Redim(0);
		this->Pp.Redim(0);
		this->freeId.Redim(0);
		return *this;
	}

	UpdatableBinaryHeap & Push(T & x) {
		PushAndGetId(x);
		return *this;
	}

	size_t PushAndGetId(T & x) {

		size_t n = this->keys.Count();
		size_t id;

		this->keys.Push(x);

		if (n == this->P.Count()) {
			this->P.Push(n);
			this->Pp.Push(n);
			id = n;
		}
		else {
			id = this->freeId.Pop();
			this->P[n] = id;
		}

		this->BubbleUp(n);
		return id;
	}

	void Pop(T & x) {

		if (!this->keys) MY_ERROR("heap is empty");
		Swap(x, this->keys[0]);
		if (this->keys.Count() > 1) {
			size_t n = this->keys.Count() - 1;
			Swap(this->keys[0], this->keys[n]);
			Swap(this->Pp[this->P[0]], this->Pp[this->P[n]]);
			freeId.Push(this->P[0]);
			this->P[0] = (size_t)(-1);
			Swap(this->P[0], this->P[n]);
			this->keys.Grow(-1);
			this->TrickleDown(0);
		}
		else this->keys.Redim(0);
	}

	T Pop() {
		T x;
		this->Pop(x);
		return x;
	}

	void Update(size_t id, T & newVal) {
		if (!keys) MY_ERROR("heap is empty");
		size_t i = this->Pp[id];
		if (i == (size_t)(-1)) MY_ERROR("id not in heap anymore");
		T & oldVal = this->keys[i];
		bool up = (min ? newVal < oldVal : newVal > oldVal);
		if (std::is_trivial<T>::value) {
			oldVal = newVal;
		}
		else {
			T newValCopy(newVal);
			Swap(newValCopy, oldVal);
		}
		up ? this->BubbleUp(i) : this->TrickleDown(i);
	}

	void Delete(size_t id) {
		size_t n = keys.Count();
		if (!n) MY_ERROR("heap is empty");
		size_t i = this->Pp[id];
		if (i == (size_t)(-1)) MY_ERROR("id not in heap anymore");
		if (n == 1) {
			this->Reset();
			return;
		}
		T & thisKey = this->keys[i];
		T & lastKey = this->keys.Last();

		Swap(thisKey, lastKey);

		this->keys.Redim(n - 1);
		this->TrickleDown(i);
	}
};

class RecursionCounter {

	unsigned & recursionCount;

public:

	RecursionCounter(unsigned & rc) : recursionCount(rc) {
		this->recursionCount++;
	}

	RecursionCounter(const RecursionCounter &) = delete;
	RecursionCounter & operator=(const RecursionCounter &) = delete;

	~RecursionCounter() {
		this->recursionCount--;
	}
};

template <bool extrapolate = true, bool constant = false, bool errorIfOutside = false, class T = double>
T Interpolate(T x, T x1, T x2, T y1, T y2) {

	if (constant) {
		if (x >= x2) return y2;
		else return y1;
	}
	else {
		T alpha = (x - x1) / (x2 - x1);
		if (!extrapolate || errorIfOutside) {
			if (x > x2) {
				if (errorIfOutside) MY_ERROR("Interpolation outside of range");
				else return y2;
			}
			if (x < x1) {
				if (errorIfOutside) MY_ERROR("Interpolation outside of range");
				else return y1;
			}
		}

		return (1 - alpha)*y1 + alpha*y2;
	}
}

//x range must not be decreasing
template <bool extrapolate = true, bool constant = false, bool errorIfOutside = false, class T = double>
T Interpolate(T x, const T * xArray, const T * yArray, size_t I) {

	if (I > 1 && x < xArray[0]) return Interpolate<extrapolate, constant, errorIfOutside>(x, xArray[0], xArray[1], yArray[0], yArray[1]);

	for (size_t i = 1; i<I; ++i) {
		if (xArray[i] > x && xArray[i - 1] <= x) {
			return Interpolate<extrapolate, constant, errorIfOutside>(x, xArray[i - 1], xArray[i], yArray[i - 1], yArray[i]);
		}
	}

	if (I > 1) return Interpolate<extrapolate, constant, errorIfOutside>(x, xArray[I - 2], xArray[I - 1], yArray[I - 2], yArray[I - 1]);
	if (I == 1) return yArray[0];

	MY_ERROR("Interpolation needs at least 2 (1) points");
	return 0;
}

template <bool extrapolate = true, bool constant = false, bool errorIfOutside = false, class T = double>
T Interpolate(T x, const aVect<T> & xArray, const aVect<T> & yArray) {

	return Interpolate<extrapolate, constant, errorIfOutside>(x, (T*)xArray, (T*)yArray, xArray.Count());
}

template<int n>
double int_pow(double x) {
	static_assert(false, "not implemented");
	return 0;
}

inline int IsDigit(char str) {
	return isdigit((unsigned char)str);
}

inline int IsAlpha(char str) {
	if (str == 'µ' || str == '°') return true;
	return isalpha((unsigned char)str);
}

inline int IsSpace(char str) {
	return isspace((unsigned char)str);
}

double L2_Norm(const aVect<double> & v);
double L2_Norm(const mVect<double> & v);
double L2_Norm(const double * p, size_t n);

//template <class T> void Swap(T& t1, T& t2) {
//	T tmp;
//	tmp = t1;
//	t1 = t2;
//	t2 = tmp;
//}

template <class T>
struct Int_Iterator {
	
	T operator *() const { return this->i; }
	const Int_Iterator &operator ++() { ++this->i; return *this; }
	Int_Iterator operator ++(int) { Int_Iterator copy(*this); ++this->i; return copy; }

	bool operator ==(const Int_Iterator &that) const { return this->i == that.i; }
	bool operator !=(const Int_Iterator &that) const { return this->i != that.i; }

	Int_Iterator(T start) : i(start) { }

	T i;
};

template <class T>
struct RangeType {
	Int_Iterator<T> _begin;
	Int_Iterator<T> _end;

	auto begin() { return _begin; }
	auto end() { return _end; }

	RangeType(T begin, T end) : _begin(begin), _end(end) {}
	RangeType(T end) : _begin(0), _end(end) {}
};

template <class T> auto Range(T begin, T end) {
	return RangeType<T>(begin, end);
}

template <class T> auto Range(T end) {
	return RangeType<T>(end);
}

template <bool yes, class T, class U> struct Chooser {
	const T& operator()(const T& t, const U& u) const {
		return t;
	}
};

template <class T, class U> struct Chooser<false, T, U> {
	const U& operator()(const T& t, const U& u) const {
		return u;
	}
};

template <bool yes, class T, class U> auto& Choose(T&& t, U&& u) {
	Chooser<yes, T, U> c;
	return c(t, u);
}

bool MyIsFinite(double x);

																						// n multiplications
template <> inline double int_pow<2>(double x)  { return x*x; }							// 1
template <> inline double int_pow<3>(double x)  { return x*x*x; }						// 2
template <> inline double int_pow<4>(double x)  { return int_pow<2>(int_pow<2>(x)); }	// 2
template <> inline double int_pow<5>(double x)  { return int_pow<4>(x) * x; }			// 3
template <> inline double int_pow<6>(double x)  { return int_pow<3>(int_pow<2>(x)); }	// 3
template <> inline double int_pow<7>(double x)  { return int_pow<6>(x) * x; }			// 4
template <> inline double int_pow<8>(double x)  { return int_pow<4>(int_pow<2>(x)); }	// 3
template <> inline double int_pow<9>(double x)  { return int_pow<8>(x) * x; }			// 4
template <> inline double int_pow<10>(double x) { return int_pow<5>(int_pow<2>(x)); }	// 4
template <> inline double int_pow<11>(double x) { return int_pow<10>(x) * x; }			// 5
template <> inline double int_pow<12>(double x) { return int_pow<6>(int_pow<2>(x)); }	// 4
template <> inline double int_pow<13>(double x) { return int_pow<12>(x) * x; }			// 5
template <> inline double int_pow<14>(double x) { return int_pow<7>(int_pow<2>(x)); }	// 5
template <> inline double int_pow<15>(double x) { return int_pow<3>(int_pow<5>(x)); }	// 5
template <> inline double int_pow<16>(double x) { return int_pow<4>(int_pow<4>(x)); }	// 4
template <> inline double int_pow<17>(double x) { return int_pow<16>(x) * x; }			// 5
template <> inline double int_pow<18>(double x) { return int_pow<3>(int_pow<6>(x)); }	// 5
template <> inline double int_pow<19>(double x) { return int_pow<18>(x) * x; }			// 6
template <> inline double int_pow<20>(double x) { return int_pow<10>(int_pow<2>(x)); }	// 5

template <> inline double int_pow<-2>(double x) { return 1 / int_pow<2>(x); }
template <> inline double int_pow<-3>(double x) { return 1 / int_pow<3>(x); }
template <> inline double int_pow<-4>(double x) { return 1 / int_pow<4>(x); }
template <> inline double int_pow<-5>(double x) { return 1 / int_pow<5>(x); }
template <> inline double int_pow<-6>(double x) { return 1 / int_pow<6>(x); }
template <> inline double int_pow<-7>(double x) { return 1 / int_pow<7>(x); }
template <> inline double int_pow<-8>(double x) { return 1 / int_pow<8>(x); }
template <> inline double int_pow<-9>(double x) { return 1 / int_pow<9>(x); }
template <> inline double int_pow<-10>(double x) { return 1 / int_pow<10>(x); }
template <> inline double int_pow<-11>(double x) { return 1 / int_pow<11>(x); }
template <> inline double int_pow<-12>(double x) { return 1 / int_pow<12>(x); }
template <> inline double int_pow<-13>(double x) { return 1 / int_pow<13>(x); }
template <> inline double int_pow<-14>(double x) { return 1 / int_pow<14>(x); }
template <> inline double int_pow<-15>(double x) { return 1 / int_pow<15>(x); }
template <> inline double int_pow<-16>(double x) { return 1 / int_pow<16>(x); }
template <> inline double int_pow<-17>(double x) { return 1 / int_pow<17>(x); }
template <> inline double int_pow<-18>(double x) { return 1 / int_pow<18>(x); }
template <> inline double int_pow<-19>(double x) { return 1 / int_pow<19>(x); }
template <> inline double int_pow<-20>(double x) { return 1 / int_pow<20>(x); }

static const auto& Sqr = int_pow < 2 >;


template <class Enum, class T = int>
struct BitField {

	Enum bitField;

	BitField(Enum value) {
		this->bitField = value;
	}

	BitField(T value) {
		this->bitField = (Enum)value;
	}

	BitField & operator =(Enum that) {
		this->bitField = that.bitField;
		return *this;
	}

	BitField & operator =(T that) {
		this->bitField = (Enum)that;
		return *this;
	}

	operator Enum() {
		return this->bitField;
	}

	T & Reinterpret() {
		return *((T*)&this->bitField);
	}

	T & Reinterpret() const {
		return *((T*)&this->bitField);
	}

	BitField & operator &=(Enum that) {

		this->Reinterpret() &= (T)that;

		return *this;
	}

	BitField & operator &=(T that) {

		this->Reinterpret() &= that;

		return *this;
	}

	BitField & operator |=(Enum that) {

		this->Reinterpret() |= (T)that;

		return *this;
	}

	T operator ~() {
		return ~(T)this->bitField;
	}

	T operator & (Enum that) const {
		return ((T)this->bitField & (T)that);
	}

	T operator | (Enum that) const {
		return ((T)this->bitField | (T)that);
	}
};

template <class T>
struct ManagedPointer {

	T * ptr = nullptr;

	ManagedPointer() {}

	template<class... Args>
	ManagedPointer(Args&&... args) {
		this->Construct(std::forward<Args>(args)...);
	}

	ManagedPointer(const ManagedPointer & that) {
		if (that.ptr) {
			this->Construct(*that.ptr);
		}
	}

	ManagedPointer(ManagedPointer && that) {
		if (that.ptr) {
			this->ptr = that.ptr;
			that.ptr = nullptr;
		}
	}

	ManagedPointer & operator=(const ManagedPointer & that) {
		if (that.ptr) {
			if (!this->ptr) {
				this->Construct(*that.ptr);
			} else {
				*this->ptr = *that.ptr;
			}
		} else {
			this->Erase();
		}
		return *this;
	}

	ManagedPointer & operator=(ManagedPointer && that) {
		
		this->Erase();

		if (that.ptr) {
			this->ptr = that.ptr;
			that.ptr = nullptr;
		}

		return *this;
	}

	ManagedPointer & Erase() {
		if (this->ptr) {
			delete this->ptr;
			this->ptr = nullptr;
		}
		return *this;
	}

	~ManagedPointer() {
		this->Erase();
	}

	template<class... Args>
	void Construct(Args&&... args) {
		this->Erase();
		this->ptr = new T (std::forward<Args>(args)...);
	}

	explicit operator bool() {
		return this->ptr != 0;
	}

	T * operator->() {
		if (!this->ptr) this->Construct();
		return this->ptr;
	}

	T & operator*() {
		if (!this->ptr) this->Construct();
		return *this->ptr;
	}
};

double Atof(const char * p);
void * MyMemChr(void * ptr, int c, size_t length);

template <class T, int N>
struct Point {
	T arr[N];

	template <class U>
	T& operator[](U index) {
		static_assert(std::is_integral<U>::value, "Index must be integral");
		return this->arr[index];
	}
};

template <class T1, class T2> struct Pair {
	T1 v1;
	T2 v2;

	template<class U1, class U2>
	Pair(U1&& v1, U2&& v2) : v1(v1), v2(v2) {}

	Pair() = default;
	Pair(const Pair&) = default;
	Pair(Pair&&) = default;
};

template <class T1, class T2, class T3> struct Triplet {
	T1 v1;
	T2 v2;
	T3 v3;

	template<class U1, class U2, class U3>
	Triplet(U1&& v1, U2&& v2, U3&& v3) : v1(v1), v2(v2), v3(v3) {}

	Triplet() = default;
	Triplet(const Triplet&) = default;
	Triplet(Triplet&&) = default;

	bool operator==(const Triplet &that) const {
		return this->v1 == that.v1 && this->v2 == that.v2 && this->v3 == that.v3;
	}
};

namespace Series {

	struct Serie {
		aVect<double> x;
		aVect<double> y;

		Serie & Push(double x, double y) {
			this->x.Push(x);
			this->y.Push(y);
			return *this;
		}

		double yMax() {
			return *std::max_element(this->y.begin(), this->y.end());
		}

		double yMin() {
			return *std::min_element(this->y.begin(), this->y.end());
		}

		explicit operator bool() const {
			return (this->x && this->y);
		}

		template <bool extrapolate = false, bool constant = false, bool errorIfOutside = true>
		double Interpolate(double x) const {

			if (this->x.Count() == 1 /*&& this->y.Count() == 1 we assume "this" is valid*/) {
				return this->y[0];
			}

			return ::Interpolate<extrapolate, constant, errorIfOutside>(x, this->x, this->y);
		}

		template <bool extrapolate = false, bool constant = false, bool errorIfOutside = true>
		void ReInterpolate(aVect<double> & new_x) {
			aVect<double> new_y(new_x.Count());
			aVect_static_for(new_x, i) {
				new_y[i] = this->Interpolate<extrapolate, constant, errorIfOutside>(new_x[i]);
			}

			this->x = new_x;
			this->y = new_y;
		};
	};

	struct SpaceSerie : Serie {

		enum Interpolation { unspecified, vertical, horizontal, length } interpolation;

		template <bool extrapolate = false, bool constant = false, bool errorIfOutside = true, class T>
		double Interpolate(T&& func) {

			double x;

			if (this->x.Count() == 1 /*&& this->y.Count() == 1 we assume "this" is valid*/) {
				return this->y[0];
			}

			switch (this->interpolation) {
			case vertical:    x = func.z;      break;
			case horizontal:  x = func.x_proj; break;
			case length:	  x = func.x;      break;
			default: MY_ERROR("invalid interpolation type");
			}

			if (IsEqualWithPrec(this->x[0], x, 1e-10))		x = this->x[0];
			if (IsEqualWithPrec(this->x.Last(), x, 1e-10))	x = this->x.Last();

			return ::Interpolate<extrapolate, constant, errorIfOutside>(x, this->x, this->y);
		}
	};

	struct SpaceTextSerie : SpaceSerie {
		aVect<aVect<char>> str;

		template <bool extrapolate = false, bool errorIfOutside = true, class T>
		const char * TextInterpolate(T&& func) {
			auto y = SpaceSerie::Interpolate<extrapolate, true, errorIfOutside, T>(func);
			return this->str[(int)y];
		}

		template <bool extrapolate = false, bool errorIfOutside = true, class T>
		int IndexInterpolate(T&& func) {
			auto y = SpaceSerie::Interpolate<extrapolate, true, errorIfOutside, T>(func);
			return (int)y;
		}

	};
}



#endif


#ifdef _WIN32_BKP
#define _WIN32 _WIN32_BKP
#define _MSC_VER _MSC_VER_BKP
#undef _WIN32_BKP
#undef _MSC_VER_BKP
#endif







