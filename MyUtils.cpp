
#define _CRT_SECURE_NO_WARNINGS

#include "xVect.h"
#include "ctype.h"
#include "MyUtils.h"
#include "math.h"
#include "float.h"

#ifdef SIMULATE_NO_WIN32
#define SIMULATE_NO_WIN32_ENABLED
#undef _WIN32
#endif

 
static CharPointer NullCharPointer;

void GetDateStr(aVect<char> & str, char * formatString) {

	time_t t;
	time(&t);
	tm * ti = localtime(&t);
	str.sprintf(formatString, ti->tm_mday, ti->tm_mon + 1, 1900 + ti->tm_year);
}

aVect<char> GetDateStr(char * formatString) {

	aVect<char> retVal;

	GetDateStr(retVal, formatString);

	return std::move(retVal);
}

void GetTimeStr(aVect<char> & str, char * formatString) {

	time_t t;
	time(&t);
	tm * ti = localtime(&t);
	str.sprintf(formatString, ti->tm_hour, ti->tm_min, ti->tm_sec);
}

aVect<char> GetTimeStr(char * formatString) {

	aVect<char> retVal;
	GetTimeStr(retVal, formatString);
	return std::move(retVal);
}


CharPointer RetrieveStr(CharPointer & str_orig, bool strict) {
    return RetrieveStr<2, ' ', '\t', 0, 0, 0>(str_orig, strict);
}

char * RetrieveStr(char * & str_orig, bool strict) {
    return RetrieveStr<2, ' ', '\t', 0, 0, 0>(str_orig, strict);
}

void RemoveUnneededExposantChars(char * ptr1) {
	//enlève les '+' et les '0' après 'e'
	while (true) {
		if (*(ptr1++) == 'e') {
			char * ptr2 = ptr1;
			bool flag = 0;
			while (1) {
				*(ptr1) = *(ptr2);
				if (*(ptr2) == 0) break;
				if (*(ptr2) != '0' && *(ptr2) != '+' || flag) {
					ptr1++;
					if (*(ptr2) != '-') flag = 1;
				}
				ptr2++;
			}
			break;
		}
		else if (*(ptr1) == 0) break;
	}
}

void RemoveDecimalsWithPrec(double val, aVect<char> & str, 
	double tol, bool removeUnneededExposantChars) {

	static WinCriticalSection cs;

	ScopeCriticalSection<WinCriticalSection> guard(cs);

	static aVect<char> fStr, str1, str2;

	//aVect<char> fStr, str1, str2;

	int prec = 0;

	if (val == 0) {
		str.sprintf("0");
		return;
	}
	
	if (_isnan(val) || !_finite(val)) {
		str.sprintf("%f", val);
		return;
	}

	while (true) {
		fStr.sprintf("%%.%dg", prec);
		str1.sprintf(fStr, val);
		if (IsEqualWithPrec(val, atof(str1), tol)) break;
		else prec++;
	}

	if (removeUnneededExposantChars) RemoveUnneededExposantChars(str1);

	prec = 0;
	while (true) {
		fStr.sprintf("%%.%df", prec);
		str2.sprintf(fStr, val);
		double atof_str2 = atof(str2);
		if (atof_str2 != 0 && IsEqualWithPrec(val, atof(str2), tol)) break;
		else prec++;
	}

	strlen(str1) + 1 < strlen(str2) ? str.Copy(str1) : str.Copy(str2);
}

void RemoveDecimalsWithPrec(aVect<char> & str, double tol, bool removeUnneededExposantChars) {

	double val = atof(str);

	RemoveDecimalsWithPrec(val, str, tol, removeUnneededExposantChars);
}

//void DoubleToString(aVect<char> & str, double val, double tol) {
//
//	RemoveDecimalsWithPrec(val, str, tol);
//	RemoveUnneededExposantChars(str);
//}

//aVect<char> DoubleToString(double val, double tol) {
//
//	aVect<char> str;
//
//	DoubleToString(str, val, tol);
//
//	return str;
//}

aVect<char> NiceTime(double seconds, bool noDays, bool integerSeconds, bool noSeconds) {
	unsigned long long minutes, hours, days;

	double origSeconds = seconds;

	if (integerSeconds) seconds = round(seconds);

	minutes  = (unsigned long long)seconds / 60;
	seconds -= 60 * minutes;

	if (noSeconds) {
		if (seconds >= 30) minutes++;
		seconds = 0;
	}

	hours  = minutes / 60;
	minutes = minutes % 60;
	
	if (noDays) {
		days = 0;
	}
	else {
		days = hours / 24;
		hours = hours % 24;
	}

	static WinCriticalSection cs;
	static aVect<char> secondStr;

	Critical_Section(cs) {

		secondStr = 
			noSeconds ? "" : 
			integerSeconds ? xFormat(" %.0fs", seconds) :
			xFormat(" %gs", seconds);


		if (!integerSeconds && minutes && strcmp(secondStr, " 60s") == 0) {
			return NiceTime(origSeconds, noDays, true, noSeconds);
		}

		if (integerSeconds) {
			if (days)         return xFormat("%llu %s, %lluh %llum%s", days, days > 1 ? "days" : "day", hours, minutes, secondStr);
			else if (hours)   return xFormat("%lluh %llum%s", hours, minutes, secondStr);
			else if (minutes) return xFormat("%llum%s", minutes, secondStr);
			else if (seconds) return xFormat("%s %s", (char*)NiceFloat(seconds, 1e-3), seconds >= 2 ? "seconds" : "second");
			else              return xFormat("0 seconds");
		}
		else {
			if (days)         return xFormat("%llu %s, %lluh %llum%s", days, days > 1 ? "days" : "day", hours, minutes, secondStr);
			else if (hours)   return xFormat("%lluh %llum%s", hours, minutes, secondStr);
			else if (minutes) return xFormat("%llum%s", minutes, secondStr);
			else if (seconds) return xFormat("%g %s", seconds, seconds >= 2 ? "seconds" : "second");
			else              return xFormat("0 seconds");
		}
	}

	return aVect<char>(); //unreachable code, to avoid warnings
}

void NiceFloat(aVect<char> & str, double tol) {

	RemoveDecimalsWithPrec(str, tol);
	//RemoveUnneededExposantChars(str);
}

//
//int strcmp_caseInsensitive(const char * str1, const char * str2) {
//
//    //static aVect<char> a;
//    //static aVect<char> b;
//
//	aVect<char> a;
//	aVect<char> b;
//
//    if (str1) {
//		size_t count_str1 = strlen(str1)+1;
//        if (count_str1 > a.Count()) a.Redim(count_str1);
//		for (size_t i=0; i<count_str1; ++i) a[i] = tolower(str1[i]);
//    }
//    else {
//        if (a.Count() < 1) a.Redim(1);
//        a[0] = 0;
//    }
//
//
//    if (str2) {
//		size_t count_str2 = strlen(str2)+1;
//        if (count_str2 > b.Count()) b.Redim(count_str2);
//		for (size_t i=0; i<count_str2; ++i) b[i] = tolower(str2[i]);
//    }
//    else {
//        if (b.Count() < 1) a.Redim(1); b.Redim(1);
//        b[0] = 0;
//    }
//
//    return strcmp(a, b);
//}

int strcmp_caseInsensitive(const wchar_t * str1, const wchar_t * str2) {

	wchar_t terminator = 0;

	const wchar_t * a = str1 ? str1 : &terminator;
	const wchar_t * b = str2 ? str2 : &terminator;

	return _wcsicmp(a, b);
}


//char * strstr_caseInsensitive(const char * str, const char * subStr) {
//
//	//static aVect<char> str_lowercase;
//	aVect<char> str_lowercase;
//	//static aVect<char> subStr_lowercase;
//	aVect<char> subStr_lowercase;
//	
//	str_lowercase.Redim(strlen(str) + 1);
//	subStr_lowercase.Redim(strlen(subStr) + 1);
//
//	aVect_static_for(str_lowercase, i) str_lowercase[i] = tolower(str[i]);
//	aVect_static_for(subStr_lowercase, i) subStr_lowercase[i] = tolower(subStr[i]);
//
//	char * ptr = strstr(str_lowercase, subStr_lowercase);
//
//	if (!ptr) return nullptr;
//
//	ptrdiff_t offset = ptr - (char*)str_lowercase;
//
//	return (char*)str + offset;
//}

//char * strstr_caseInsensitive(const char * str1, const char * str2) {
//
//    static aVect<char> a;
//    static aVect<char> b;
//
//    if (str1) {
//		size_t count_str1 = strlen(str1)+1;
//        if (count_str1 > a.Count()) a.Redim(count_str1);
//		for (size_t i=0; i<count_str1; ++i) a[i] = tolower(str1[i]);
//    }
//    else {
//        if (a.Count() < 1) a.Redim(1);
//        a[0] = 0;
//    }
//    if (str2) {
//		size_t count_str2 = strlen(str2)+1;
//        if (count_str2 > b.Count()) b.Redim(count_str2);
//		for (size_t i=0; i<count_str2; ++i) b[i] = tolower(str2[i]);
//    }
//    else {
//        if (b.Count() < 1) a.Redim(1); b.Redim(1);
//        b[0] = 0;
//    }
//
//    return (char*)(str1 + (ptrdiff_t)(strstr(a, b) - (char*)a));
//}
//
//size_t ReplaceStr(
//	aVect<char> & str,
//	char * subStr,
//	char * replaceStr,
//	bool onlyFirstOccurence,
//	bool caseSensitive,
//	bool reverse,
//	size_t startFrom) 
//{
//
//	size_t strLen        = strlen(str);
//	size_t subStrLen     = strlen(subStr);
//	size_t replaceStrLen = strlen(replaceStr);
//	
//	long long grow = replaceStrLen;
//	grow -= subStrLen; // because if (replaceStrLen - subStrLen) is negative the result is (size_t)-1 = 4294967295 :(
//
//	char * strEnd        = (char*)str + strLen;
//	char * subStrEnd     = subStr     + subStrLen;
//	char * replaceStrEnd = replaceStr + replaceStrLen;
//
//	if (startFrom > strLen) return 0;
//
//	size_t handledChars = startFrom;
//
//	while (true) {
//		char * foundSubStr = caseSensitive ?
//			(reverse ? 
//				reverse_strstr((char*)str - handledChars, subStr) 
//				: 
//				strstr((char*)str + handledChars, subStr))
//			: 
//			(reverse ? 
//				reverse_strstr_caseInsensitive((char*)str - handledChars, subStr) 
//				: 
//				strstr_caseInsensitive((char*)str + handledChars, subStr));
//
//		if (!foundSubStr) return handledChars == startFrom ? 0 : handledChars;
//
//		//First we must move not-to-be-erased data
//		if (grow > 0) {// if grow > 0, grow before moving data
//			char * oldPtr = str;
//			str.Grow(grow); 
//			if (str != oldPtr) {//str might have been reallocted at a new address
//				ptrdiff_t diff = str - oldPtr;
//				strEnd += diff;
//				foundSubStr += diff;
//			}//move data
//			for (char * ptr = strEnd; ptr >= foundSubStr + subStrLen; --ptr) {
//				*(ptr + grow) = *ptr;
//			}
//			
//		}
//		else if (grow < 0) {// if grow < 0, move data from begin to end
//			for (char * ptr = foundSubStr + subStrLen; ptr <= strEnd; ++ptr) {
//				*(ptr + grow) = *ptr;
//			}
//			char * oldPtr = str;
//			str.Grow(grow); // then shrink after moving data
//			if (str != oldPtr) {//str might have been reallocted at a new address
//				ptrdiff_t diff = str - oldPtr;
//				strEnd += diff;
//			}
//		}
//		strEnd += grow;
//
//		handledChars = reverse ? strEnd - foundSubStr : foundSubStr - str + replaceStrLen;
//
//		//Then we put replaceStr
//		for (size_t i=0; i<replaceStrLen; ++i) {
//			foundSubStr[i] = replaceStr[i];
//		}
//
//		if (onlyFirstOccurence) return handledChars;
//	}
//}
//
//size_t ReplaceStr_CaseInsensitive(aVect<char> & str, char * subStr, char * replaceStr, bool onlyFirstOccurence, size_t startFrom) {
//	return ReplaceStr(str, subStr, replaceStr, onlyFirstOccurence, false, false, startFrom);
//}
//
//size_t ReplaceStr_FirstOccurence(aVect<char> & str, char * subStr, char * replaceStr, bool caseSensitive, size_t startFrom) {
//	return ReplaceStr(str, subStr, replaceStr, true, caseSensitive, false, startFrom);
//}
//
//size_t ReplaceStr_LastOccurence(aVect<char> & str, char * subStr, char * replaceStr, bool caseSensitive, size_t startFrom) {
//	return ReplaceStr(str, subStr, replaceStr, true, caseSensitive, true, startFrom);
//}
//
//size_t ReplaceStr_FirstOccurence_CaseInsensitive(aVect<char> & str, char * subStr, char * replaceStr, size_t startFrom) {
//	return ReplaceStr(str, subStr, replaceStr, true, false, false, startFrom);
//}
//
//size_t ReplaceStr_LastOccurence_CaseInsensitive(aVect<char> & str, char * subStr, char * replaceStr, size_t startFrom) {
//	return ReplaceStr(str, subStr, replaceStr, true, false, true, startFrom);
//}
//
//aVect<char> ReplaceStr(
//	char * str, 
//	char * subStr, 
//	char * replaceStr, 
//	bool onlyFirstOccurence, 
//	bool caseSensitive, 
//	bool reverse, 
//	size_t startFrom) {
//
//	aVect<char> retVal(str);
//	ReplaceStr(retVal, subStr, replaceStr, onlyFirstOccurence, caseSensitive, reverse, startFrom);
//	return std::move(retVal);
//}
//
//aVect<char> ReplaceStr_CaseInsensitive(char * str, char * subStr, char * replaceStr, bool onlyFirstOccurence, bool reverse, size_t startFrom) {
//	return ReplaceStr(str, subStr, replaceStr, onlyFirstOccurence, false, reverse, startFrom);
//}
//
//aVect<char> ReplaceStr_FirstOccurence(char * str, char * subStr, char * replaceStr, bool caseSensitive, size_t startFrom) {
//	return ReplaceStr(str, subStr, replaceStr, true, caseSensitive, false, startFrom);
//}
//
//aVect<char> ReplaceStr_LastOccurence(char * str, char * subStr, char * replaceStr, bool caseSensitive, size_t startFrom) {
//	return ReplaceStr(str, subStr, replaceStr, true, caseSensitive, true, startFrom);
//}
//
//aVect<char> ReplaceStr_FirstOccurence_CaseInsensitive(char * str, char * subStr, char * replaceStr, size_t startFrom) {
//	return ReplaceStr(str, subStr, replaceStr, true, false, false, startFrom);
//}
//
//aVect<char> ReplaceStr_LastOccurence_CaseInsensitive(char * str, char * subStr, char * replaceStr, size_t startFrom) {
//	return ReplaceStr(str, subStr, replaceStr, true, false, true, startFrom);
//}
//
//char * reverse_strstr(char * str, char * subStr, bool caseSensitive) {
//
//	char * lastPtr = caseSensitive ? strstr(str, subStr) : strstr_caseInsensitive(str, subStr);
//
//	if (!lastPtr) return lastPtr;
//
//	while(true) {
//		char* newPtr = caseSensitive ? strstr(lastPtr+1, subStr) : strstr_caseInsensitive(lastPtr+1, subStr);
//		if (newPtr) lastPtr = newPtr;
//		else break;
//	}
//
//	return lastPtr;
//}
//
//char * reverse_strstr_caseInsensitive(char * str, char * subStr) {
//	return reverse_strstr(str, subStr, false);
//}

template <class Char, class UChar>
Char CheckNumberCore(const Char * str, bool separate, bool strict, ptrdiff_t * pDiff) {

	if (!str) return false;

	auto ptrOrig = (UChar *)str; //because isdigit() and co need unsigned char (!)
	auto * ptr = (UChar *)str;

	bool sign = false;
	bool decimalPoint = false;
	bool exposant = false;
	bool signExposant = false;
	bool first = true;
	bool firstAfterExposant = false;
	bool containDigit = false;
	bool isNumber = false;
	bool isInf = false;

	for(; ; ++ptr) {
		if (first && isspace(*ptr)) continue;
		else if (*ptr == '-' || *ptr == '+') {
			if (exposant) {
				if (signExposant) break; 
				else if (firstAfterExposant) signExposant = true;
				else break;
			}
			else if (sign) break;
			else if (first) sign = true;
			else break;
		}
		else if (*ptr == '#') {
			if (!decimalPoint) break; 
			if (ptr[1] != 'I' || ptr[2] != 'N' || ptr[3] != 'F') break;
			else {
				ptr += 3;
				isInf = true;
				isNumber = false;
				break;
			}
		}
		else if (*ptr == '.') {
			if (decimalPoint) break; 
			else decimalPoint = true;
		}
		else if (tolower(*ptr) == 'e') {
			if (exposant) break; 
			else {
				firstAfterExposant = exposant = true;
				first = false;
				continue;
			}
		}
		else if (isdigit(*ptr)) isNumber = containDigit = true;
		else break;
		firstAfterExposant = first = false;
	}

	if (strict && *ptr && !isspace(*ptr)) isNumber = false;

	if (separate) {
		if (!pDiff) MY_ERROR("separate: must specifiy pDiff");
		*pDiff = 0;
		ptrdiff_t diff = ptr - ptrOrig;
		if (diff && *ptr != 0 && *ptr != ' ') *pDiff = diff;//str.Insert(diff, ' ');
	}

	return (containDigit ? CHECKNUM_CONTAINSNUMBERFIRST : 0) |
		   (isNumber ? CHECKNUM_ISNUMBER : 0) | 
		   (isInf    ? CHECKNUM_ISINF    : 0);
}

char CheckNumber(const char * str, bool separate, bool strict, ptrdiff_t * pDiff = nullptr) {
	return CheckNumberCore<char, unsigned char>(str, separate, strict, pDiff);
}

wchar_t CheckNumber(const wchar_t * str, bool separate, bool strict, ptrdiff_t * pDiff = nullptr) {
	return CheckNumberCore<wchar_t, wchar_t>(str, separate, strict, pDiff);
}

char CheckNumber(aVect<char> & str, bool separate, bool strict) {
	ptrdiff_t diff;
	auto retVal = CheckNumber((char*)str, separate, strict, &diff);
	if (diff) str.Insert(diff, ' ');
	return retVal;
}


wchar_t CheckNumber(aVect<wchar_t> & str, bool separate, bool strict) {
	ptrdiff_t diff;
	auto retVal = CheckNumber((wchar_t*)str, separate, strict, &diff);
	if (diff) str.Insert(diff, L' ');
	return retVal;
}

bool SeparateNumber(aVect<char> & str) {
	return (CheckNumber(str, true, true) & CHECKNUM_CONTAINSNUMBERFIRST) != 0;
}

bool SeparateNumber(aVect<wchar_t> & str) {
	return (CheckNumber(str, true, true) & CHECKNUM_CONTAINSNUMBERFIRST) != 0;
}

bool IsNumber(const char * str, bool strict) {
	return (CheckNumber(str, false, strict) & CHECKNUM_ISNUMBER) != 0;
}

bool IsNumber(const wchar_t * str, bool strict) {
	return (CheckNumber(str, false, strict) & CHECKNUM_ISNUMBER) != 0;
}

bool IsInf(const char * str) {
	return (CheckNumber(str, false, true) & CHECKNUM_ISINF) != 0;
}

bool IsInf(const wchar_t * str) {
	return (CheckNumber(str, false, true) & CHECKNUM_ISINF) != 0;
}

CharPointer MyGetLine(FILE * f) {

    static int buf_nEl;
    static char * buf = NULL;
    static char * ptrStart = NULL;
    static char * curPos = NULL;
    static char * ptrEndRead = NULL;
    static bool endOfFile = false;

    if (!f) {
        //Reinit
        buf_nEl = 0;
        free(buf);
        buf = NULL;
        ptrStart = NULL;
        curPos = NULL;
        ptrEndRead = NULL;
        endOfFile = false;
        return CharPointer();
    }

    int nGrow = 10000;

    if (!buf) {
        buf = (char*)malloc(nGrow+1);
		if (!buf) MY_ERROR("malloc failed");
        buf_nEl = nGrow;
    }

    char * ptrStartBuf = buf;
    char * ptrEndBuf = buf + buf_nEl;

	if (endOfFile) return CharPointer();

    if (!curPos) {
        curPos = ptrStartBuf;

        size_t nElToRead = ptrEndBuf - ptrStartBuf;
        size_t read_nEl = fread(ptrStartBuf, sizeof(char), nElToRead, f);
        ptrEndRead = ptrStartBuf + read_nEl;
    }

    ptrStart = curPos;

    while(true) {

        for (char * ptr = curPos ; ptr<ptrEndRead ; ++ptr) {

            if (*ptr == '\n') {
                *ptr = 0;
                if (ptr>ptrStart && *(ptr-1) == '\r') {
                    *(ptr-1) = 0;
                }

                curPos = ptr + 1;

				return ptrStart;
            }
        }

        if (feof(f)) {
            if (curPos == ptrEndRead) {
				return CharPointer();
            }
            endOfFile = true;
            ptrEndRead[0] = 0;

			return ptrStart;
        }

        curPos = ptrEndRead;

        if (ptrStart == ptrStartBuf) {
            int old_buf_nEl = buf_nEl;
            buf_nEl += nGrow;

            buf = (char*)realloc(buf, buf_nEl+1);

			if (!buf) MY_ERROR("realloc failed");

            ptrStart = buf + old_buf_nEl;
            ptrEndBuf = buf + buf_nEl;
            ptrStartBuf = buf;
            curPos = ptrStart;

            size_t nElToRead = ptrEndBuf - ptrStart;
            size_t read_nEl = fread(ptrStart, sizeof(char), nElToRead, f);
            ptrEndRead = ptrStart + read_nEl;

            ptrStart = ptrStartBuf;
        }
        else {
            for (size_t i=0, I=ptrEndBuf - ptrStart ; i<I ; ++i) {
                ptrStartBuf[i] = ptrStart[i];
            }
            size_t dec = ptrStart - ptrStartBuf;
            curPos -= dec;
            ptrStart -= dec;
            ptrEndRead -= dec;

            size_t nElToRead = ptrEndBuf - ptrEndRead;
            size_t read_nEl = fread(ptrEndRead, sizeof(char), nElToRead, f);
            ptrEndRead += read_nEl;
        }
    }
}


//==============================
//============ File ============
//==============================


void File::CloseCore() {
	fclose(file);
	file = nullptr;
}

void File::OpenCore(const char * fp, const char * om) {
	file = fopen(fp, om);
	if (file) {
		fseek(file, 0, SEEK_END);
		fileSize = ftell(file);
		fseek(file, 0, SEEK_SET);
	}
}

void File::FlushCore() {
	fflush(file);
}

void File::Flush() {
	FlushCore();
}

size_t File::ReadCore(void* ptr, size_t s, size_t n) {
	return fread(ptr, s, n, file);
}

size_t File::WriteCore(void* ptr, size_t s, size_t n) {
	return fwrite(ptr, s, n, file);
}

File & File::Init() {
	buf_nEl = 0;
	file = nullptr;
	buf = ptrStart = curPos = ptrEndRead = nullptr;
	endOfFile = cachedData = writeMode = false;
	fileSize = 0;
	return *this;
}

File & File::Reset() {
	Close();
	openMode.Free();
	filePath.Free();
	return *this;
}

File & File::SetOpenMode(const char * om) {
	writeMode = IsWriteMode(om);
	openMode.Copy(om);
	return *this;
}

long long File::GetCurrentPosition() {
	if (!*this) return 0;
	return ftell(file);
}

long long File::GetSize() {
	if (!*this) this->Open();
	if (IsWriteMode(openMode)) MY_ERROR("Can't get size on write mode");
	return fileSize;
}

File::operator bool() {
	return file != 0;
}

File::File() {
	Init();
}

File::File(File && that) {

	this->buf = that.buf;
	this->buf_nEl = that.buf_nEl;
	this->cachedData = that.cachedData;
	this->curPos = that.curPos;
	this->endOfFile = that.endOfFile;
	this->file = that.file;
	this->filePath.Steal(that.filePath);
	this->fileSize = that.fileSize;
	this->lastFlush = that.lastFlush;
	this->openMode.Steal(that.openMode);
	this->ptrEndRead = that.ptrEndRead;
	this->ptrStart = that.ptrStart;
	this->writeMode = that.writeMode;

	that.buf = nullptr;
	that.buf_nEl = 0;
	that.cachedData = false;
	that.curPos = 0;
	that.endOfFile = false;
	that.file = nullptr;
	that.fileSize = 0;
	that.lastFlush = 0;

	that.ptrEndRead = nullptr;
	that.ptrStart = nullptr;
	that.writeMode = false;
}

File& File::operator=(File && that) {

	this->~File();

	new (this) File(std::move(that));

	return *this;
}

File::File(const char * fn) : filePath(fn) {
	Init();
}

File::File(const char * fn, const char * om) : filePath(fn), openMode(om) {
	Init();
	if (IsWriteMode(om)) writeMode = true;
}

File::~File() {
	Close();
	if (buf) free(buf);
}

File & File::Close() {
	if (*this) {
		CloseCore();
	}
	ptrStart = curPos = ptrEndRead = nullptr;
	endOfFile = cachedData = writeMode = false;
	fileSize = 0;
	return *this;
}

File & File::SetFile(const char * fp, const char * om) {
	//if (strcmp_caseInsensitive(fp, filePath) == 0 && (!om || strcmp_caseInsensitive(om, openMode) == 0)) return *this;
	Close();
	if (filePath && _strcmpi(fp, filePath) == 0 && (!om || (openMode && _strcmpi(om, openMode) == 0))) return *this;
	
	filePath.Copy(fp);
	if (om) SetOpenMode(om);
	return *this;
}

aVect<char> File::GetFilePath() const {

	return filePath;
}

aVect<char> File::GetOpenMode() {

	return openMode;
}

bool File::IsWriteMode(const char * om) {
	return (strchr(om, 'w') != nullptr || strchr(om, 'a') != nullptr || strchr(om, '+') != nullptr);
}

bool File::IsAppendMode(const char * om) {
	return (strchr(om, 'a') != nullptr);
}

bool File::IsReadMode(const char * om) {
	return (strchr(om, 'r') != nullptr);
}

bool File::IsBinaryMode(const char * om) {
	return (strchr(om, 'b') != nullptr);
}

File & File::Open(const char * fp, const char * om) {
	Close();
	if (fp) filePath.Copy(fp);
	else if (!filePath) {
		MY_ERROR("No file specified.");
		return *this;
	}
	if (om) SetOpenMode(om);
	else if (!openMode) {
		MY_ERROR("No open mode specified.");
		return *this;
	}
	OpenCore(filePath, openMode);
	lastFlush = clock();
	return *this;
}

const FILE * File::GetFILEptr() {
	return file;
}

FILE * File::GetFILEptr_UNSAFE() {
	return file;
}

File::operator FILE*() {

	if (!*this) Open();

	if (cachedData) MY_ERROR("Should not use FILE pointer, data has been cached by GetLine().");
	if (writeMode && *this && (lastFlush - clock()) / CLOCKS_PER_SEC > 10) {
		FlushCore();
		lastFlush = clock();
	}
	return file;
}

size_t File::Read(aVect<char> & data, size_t n) {
	if (!*this) {
		openMode.sprintf("r");
		if (!Open()) MY_ERROR(aVect<char>("Unable to open \"%s\"", (char*)filePath));
	}
	if (cachedData) MY_ERROR("Should not use Read() method after GetLine(), data has been cached");
	data.Redim(n);
	return ReadCore(&data[0], n, 1);
}

size_t File::Write(const char * str) {
	if (!*this) {
		openMode.sprintf("w");
		if (!Open()) MY_ERROR(aVect<char>("Unable to open \"%s\"", (char*)filePath));
	}
	return WriteCore((void*)str, strlen(str), 1);
}

CharPointer File::GetLastLine(){
	
	if (endOfFile) return CharPointer();
	return ptrStart;
}

bool File::EofCore() {
	return feof(file) != 0;
}

bool File::Eof() {
	return endOfFile;
}

void File::PosChanged() {

	this->ptrStart = this->curPos = this->ptrEndRead = nullptr;
	this->endOfFile = this->cachedData = false;
}

CharPointer File::GetLineCore(bool errorIfEndOfFile, bool nullEndLine) {

	if (!*this) {
		openMode.sprintf("rb");
		if (!Open()) MY_ERROR(aVect<char>("Unable to open \"%s\"", (char*)filePath));
	}

	//if (strcmp_caseInsensitive(openMode, "rb") != 0) MY_ERROR("File must be opened in binary read mode.");
	if (_strcmpi(openMode, "rb") != 0) MY_ERROR("File must be opened in binary read mode.");

	cachedData = true;
	int nGrow = 10000;

	if (!buf) {
		buf = (char*)malloc(nGrow+1);
		buf_nEl = nGrow;
	}

	char * ptrStartBuf = buf;
	char * ptrEndBuf = buf + buf_nEl;

	if (endOfFile) {
		if (errorIfEndOfFile) MY_ERROR(aVect<char>("Unexptected end of file \"%s\"", (char*)filePath));
		else return CharPointer();
	}

	if (!curPos) {
		curPos = ptrStartBuf;

		size_t nElToRead = ptrEndBuf - ptrStartBuf;
		size_t read_nEl = ReadCore(ptrStartBuf, sizeof(char), nElToRead);
		ptrEndRead = ptrStartBuf + read_nEl;
	}

	ptrStart = curPos;

	auto endlMarker = nullEndLine ? 0 : '\n';

	while(true) {

		//if (auto ptr = (char*)memchr(curPos, '\n', ptrEndRead - curPos)) {
		if (auto ptr = (char*)MyMemChr(curPos, '\n', ptrEndRead - curPos)) {
			*ptr = endlMarker;
			if (nullEndLine && ptr>ptrStart && *(ptr - 1) == '\r') *(ptr - 1) = endlMarker;

			curPos = ptr + 1;

			if (curPos == ptrEndRead && EofCore())
				endOfFile = true; // If there's nothing left in the file after the found '\n'

			return ptrStart;
		}
		else {
			int a = 0;
		}

		//for (char * ptr = curPos ; ptr<ptrEndRead ; ++ptr) {

		//	if (*ptr == '\n') {

		//		*ptr = endlMarker;
		//		if (nullEndLine && ptr>ptrStart && *(ptr-1) == '\r') *(ptr-1) = endlMarker;

		//		curPos = ptr + 1;

		//		if (curPos == ptrEndRead && EofCore()) 
		//			endOfFile = true; // If there's nothing left in the file after the found '\n'

		//		return ptrStart;
		//	}
		//}

		if (EofCore()) {

			endOfFile = true;

			if (curPos == ptrEndRead) {// should not happend, maybe if concurrent write since last GetLine() ?
				if (errorIfEndOfFile) MY_ERROR(aVect<char>("Unexptected end of file \"%s\"", (char*)filePath));
				else return CharPointer();
			}
			
			ptrEndRead[0] = endlMarker;

			return ptrStart;
		}

		curPos = ptrEndRead;

		if (ptrStart == ptrStartBuf) {
			int old_buf_nEl = buf_nEl;
			buf_nEl += nGrow;

			nGrow = (int)(nGrow * 1.5);

			buf = (char*)realloc(buf, buf_nEl+1);

			ptrStart = buf + old_buf_nEl;
			ptrEndBuf = buf + buf_nEl;
			ptrStartBuf = buf;
			curPos = ptrStart;

			size_t nElToRead = ptrEndBuf - ptrStart;
			size_t read_nEl = ReadCore(ptrStart, sizeof(char), nElToRead);
			ptrEndRead = ptrStart + read_nEl;

			ptrStart = ptrStartBuf;

			if (nElToRead && read_nEl == 0) {
				ptrEndRead[0] = endlMarker;
				return ptrStart;
			}
		}
		else {
			for (size_t i=0, I=ptrEndBuf - ptrStart ; i<I ; ++i) {
				ptrStartBuf[i] = ptrStart[i];
			}
			size_t dec = ptrStart - ptrStartBuf;
			curPos -= dec;
			ptrStart -= dec;
			ptrEndRead -= dec;

			size_t nElToRead = ptrEndBuf - ptrEndRead;
			size_t read_nEl = ReadCore(ptrEndRead, sizeof(char), nElToRead);
			ptrEndRead += read_nEl;

			if (nElToRead && read_nEl == 0) {
				ptrEndRead[0] = endlMarker;
				return ptrStart;
			}
		}
	}
}

bool IsStringInFile(File & f, const char * str) {

	f.Close();

	if (!str) return false;

	CharPointer line, token;
	while(line = f.GetLine()) {
		//if (strcmp_caseInsensitive(line, str)) return true;
		//if (_strcmpi(line, str) == 0) return true;
		if (reverse_strstr_caseInsensitive(line, (char*)str)) return true;
	}

	return false;
}

//==============================
//========== LogFile ===========
//==============================

LogFile & LogFile::Init(wchar_t * fName, bool erase) {
	if (fName) this->SetFile(fName, 1000, erase);
	dispVerboseLvl = 1000;
	tmpPrintTime = printTime = true;
	tmpPrintDate = printDate = false;
	display = tmpDisplay = false;
	write = tmpWrite = true;
	printLvl = 0;
	AddIfUnkownVerboseLvl(0);
	return *this;
}

LogFile & LogFile::Init(char * fName, bool erase) {
	return this->Init(aVect<wchar_t>(fName), erase);
}

LogFile & LogFile::Open() {
	if (!logFiles) {
		aVect_static_for(filePaths, i) {
			logFiles.Push(_wfopen(filePaths[i], openModes[i]));
			if (!logFiles[i]) MY_ERROR(xFormat("Impossible d'ouvrir \"%S\"", (wchar_t*)filePaths[i]));
		}
	}
	else {
		aVect_static_for(filePaths, i) {
			if (!logFiles[i]) logFiles[i] = _wfopen(filePaths[i], openModes[i]);
			if (!logFiles[i]) MY_ERROR(xFormat("Impossible d'ouvrir \"%S\"", (wchar_t*)filePaths[i]));
		}
	}
		
	return *this;
}


LogFile::LogFile() {
	Init();
}

	
LogFile::~LogFile() {
	Close();
}
	
LogFile::LogFile(char * fPath, bool erase) {
	Init(fPath, erase);
}

LogFile::LogFile(wchar_t * fPath, bool erase) {
	Init(fPath, erase);
}

LogFile & LogFile::Close() {
	aVect_static_for(logFiles, i) {
		if (logFiles[i]) {
			fclose(logFiles[i]);
			logFiles[i] = nullptr;
		}
	}
		
	return *this;
}

LogFile & LogFile::SetFile(char * fPath, int verboseLvl, bool erase) {

	return this->SetFile(aVect<wchar_t>(fPath), verboseLvl, erase);
}

LogFile & LogFile::SetFile(wchar_t * fPath, int verboseLvl, bool erase) {
		
	if (!fPath) return *this;
		
	Close();

	logFiles.Free();
	filePaths.Free();
	openModes.Free();
	verboseLvls.Free();
	knownVerboseLvls.Free();
	
	AddIfUnkownVerboseLvl(0);
	AddIfUnkownVerboseLvl(verboseLvl);

	return AddFile(fPath, verboseLvl, erase);
}

LogFile & LogFile::AddIfUnkownVerboseLvl(int verboseLvl, bool pendingLine) {
	
	aVect_static_for(knownVerboseLvls, i) {
		if (knownVerboseLvls[i].verboseLvl == verboseLvl) return *this;
	}
	
	VerboseLvl tmp = {verboseLvl, pendingLine};
	knownVerboseLvls.Push(tmp);
	return *this;
}

bool LogFile::HasPendingLine(int verboseLvl) {
	
	aVect_static_for(knownVerboseLvls, i) {
		if (knownVerboseLvls[i].verboseLvl == verboseLvl) {
			return knownVerboseLvls[i].pendingNewLine;
		}
	}

	AddIfUnkownVerboseLvl(verboseLvl);
	return false;
}

LogFile &  LogFile::ResetPendingLine(int verboseLvl) {
	
	aVect_static_for(knownVerboseLvls, i) {
		if (knownVerboseLvls[i].verboseLvl == verboseLvl) {
			knownVerboseLvls[i].pendingNewLine = false;
			return *this;
		}
	}

	AddIfUnkownVerboseLvl(verboseLvl);
	return *this;
}

LogFile & LogFile::AddFile(wchar_t * fPath, int verboseLvl, bool erase) {

	if (!fPath) return *this;

	filePaths.Push(fPath);
	openModes.Push(erase ? "w" : "a");
	verboseLvls.Push(verboseLvl);

	AddIfUnkownVerboseLvl(verboseLvl);

	return *this;
}

LogFile & LogFile::AddFile(char * fPath, int verboseLvl, bool erase) {
	return this->AddFile(aVect<wchar_t>(fPath), verboseLvl, erase);
}

LogFile & LogFile::RemoveFile(size_t i, bool erase) {
		
	filePaths.Remove(i);
	openModes.Remove(i);
	verboseLvls.Remove(i);

	return *this;
}

aVect<wchar_t> LogFile::GetFilePath(int i) {
	if (!filePaths) MY_ERROR("No files specified");
	
	return filePaths[i];
}

size_t LogFile::GetFilesCount() {
	return filePaths.Count();
}

FILE * LogFile::GetFile(int i) {
	if (!logFiles) MY_ERROR("No files specified");
	return logFiles[i];
}

aVect< aVect<wchar_t> > LogFile::GetFilePaths() {
	return filePaths;
}

LogFile & LogFile::SetVerboseLvl(int vLvl, int i) {
	AddIfUnkownVerboseLvl(vLvl);
	verboseLvls[i] = vLvl;
	return *this;
}

LogFile & LogFile::SetDisplayVerboseLvl(int vLvl) {
	AddIfUnkownVerboseLvl(vLvl);
	dispVerboseLvl = vLvl;
	return *this;
}

LogFile & LogFile::PrintTime() {
	printLvl = 0;
	tmpPrintTime = true;
	this->NoDate().Printf("");
	return *this;
}

LogFile & LogFile::PrintDate() {

	//static aVect<char> dateStr;
	aVect<char> dateStr;

	printLvl = 0;
	time_t t;
	time(&t);
	tm * ti = localtime(&t);
	GetDateStr(dateStr, "%.2d/%.2d/%.4d ");
	NoTime().NoDate().Printf("%s", (char*)dateStr);

	return *this;
}


LogFile & LogFile::NoTime() {
	tmpPrintTime = false;
	return *this;
}

LogFile & LogFile::Time() {
	tmpPrintTime = true;
	return *this;
}

LogFile & LogFile::NoDate() {
	tmpPrintDate = false;
	return *this;
}

LogFile & LogFile::Date() {
	tmpPrintDate = true;
	return *this;
}
LogFile & LogFile::NoDisp() {
	tmpDisplay = false;
	return *this;
}

LogFile & LogFile::Disp() {
	tmpDisplay = true;
	return *this;
}

LogFile & LogFile::NoWrite() {
	tmpWrite = false;
	return *this;
}

LogFile & LogFile::Write() {
	tmpWrite = true;
	return *this;
}

LogFile & LogFile::DisableDisplay() {
	display = tmpDisplay = false;
	return *this;
}

LogFile & LogFile::DisableTime() {
	printTime = tmpPrintTime = false;
	return *this;
}

LogFile & LogFile::DisableDate() {
	printDate = tmpPrintDate = false;
	return *this;
}

LogFile & LogFile::DisableWrite() {
	write = tmpWrite = false;
	return *this;
}

LogFile & LogFile::EnableDisplay() {
	display = tmpDisplay = true;
	return *this;
}

LogFile & LogFile::EnableTime() {
	printTime = tmpPrintTime = true;
	return *this;
}

LogFile & LogFile::EnableDate() {
	printDate = tmpPrintDate = true;
	return *this;
}

LogFile & LogFile::EnableWrite() {
	write = tmpWrite = true;
	return *this;
}

LogFile & LogFile::SetPendingNewLine() {
	aVect_static_for(knownVerboseLvls, i) {
		knownVerboseLvls[i].pendingNewLine = true;
	}
	return *this;
}

LogFile & LogFile::SetPendingNewLine(int vLvl) {
	aVect_static_for(knownVerboseLvls, i) {
		if (knownVerboseLvls[i].verboseLvl >= vLvl) knownVerboseLvls[i].pendingNewLine = true;
	}
	return *this;
}

LogFile & LogFile::UnsetPendingNewLine() {
	aVect_static_for(knownVerboseLvls, i) {
		knownVerboseLvls[i].pendingNewLine = false;
	}
	return *this;
}

LogFile & LogFile::UnsetPendingNewLine(int vLvl) {
	aVect_static_for(knownVerboseLvls, i) {
		if (knownVerboseLvls[i].verboseLvl >= vLvl) knownVerboseLvls[i].pendingNewLine = false;
	}
	return *this;
}

LogFile::operator bool() {
	return (tmpWrite && filePaths) || tmpDisplay;
}

int MyRound(double x) {
	//return (int)(x + 0.5);
	return std::lround(x);
}

bool IsEqualWithPrec(double a, double b, double prec) {
	return abs((a - b) / Min(abs(a), abs(b))) < prec;
	
	//double _a_ = abs(a);
	//double _b_ = abs(b);

	//return abs((a - b) / (_a_ > _b_ ? _b_ : _a_)) < prec;
}

void AtExitPromptBeforeExitFunc() {
	printf("Program terminated. Press any key...\n");
	fflush(stdin);
	getchar();
}

void SetPromptBeforeExit() {
	atexit(AtExitPromptBeforeExitFunc);
}

bool MyIsFinite(double x) {
	return (x * 0) == 0;
}

bool MyIsNaN(double x) {
	return x != x;
}

double L2_Norm(const double * p, size_t n) {
	
	double s = 0;

	if (n < UINT_MAX) {
		auto I = (unsigned int)n;
		for (unsigned int i = 0; i < I; ++i) {
			s += Sqr(p[i]);
		}
	}
	else {
		for (size_t i = 0; i < n; ++i) {
			s += Sqr(p[i]);
		}
	}

	return sqrt(s);
}

double L2_Norm(const aVect<double> & v) {
	return L2_Norm(v.GetDataPtr(), v.Count());
}

double L2_Norm(const mVect<double> & v) {
	return L2_Norm(v.GetDataPtr(), v.Count());
}


#define white_space(c) ((c) == ' ' || (c) == '\t')
#define valid_digit(c) ((c) >= '0' && (c) <= '9')

double NaiveAtof(const char *p) {

	while (white_space(*p)) p++;

	double r = 0.0;
	bool neg = false;
	if (*p == '-') {
		neg = true;
		++p;
	}
	else if (*p == '+') {
		++p;
	}

	if (tolower(p[0]) == 'i' && tolower(p[1]) == 'n' && tolower(p[2]) == 'f' && (p[3] == 0 || white_space(p[3]))) {
		return (neg ? -1 : 1) * std::numeric_limits<double>::infinity();
	}

	if (tolower(p[0]) == 'n' && tolower(p[1]) == 'a' && tolower(p[2]) == 'n' && (p[3] == 0 || white_space(p[3]))) {
		return (neg ? -1 : 1) * std::numeric_limits<double>::quiet_NaN();
	}

	while (*p >= '0' && *p <= '9') {
		r = (r*10.0) + (*p - '0');
		++p;
	}
	if (*p == '.') {
		double f = 0.0;
		int n = 0;
		++p;
		while (*p >= '0' && *p <= '9') {
			f = (f*10.0) + (*p - '0');
			++p;
			++n;
		}
		r += f / pow(10.0, n);
	}
	if (neg) {
		r = -r;
	}

	if (*p == 'E' || *p == 'e') {
		++p;

		neg = false;

		if (*p == '-') {
			neg = true;
			++p;
		}
		else if (*p == '+') {
			++p;
		}

		int f = 0;
		int n = 0;

		while (*p >= '0' && *p <= '9') {
			f = (f * 10) + (*p - '0');
			++p;
			++n;
		}

		if (neg) f = -f;
		r *= pow(10.0, f);
	}

	return r;
}

#undef white_space
#undef valid_digit

double Atof(const char * p) {

#if _DEBUG
	double n1 = NaiveAtof(p);
	double n2 = atof(p);

	if ((n1 || n2) && (_finite(n1) || _finite(n2)) && !IsEqualWithPrec(n1, n2, 1e-14)) MY_ERROR("wrong conversion");
	return n1;
#else
	return NaiveAtof(p);
#endif
}

//from https://sourceware.org/viewvc/src/newlib/libc/string/memchr.c

#define MYMEMCHR_UNALIGNED(X) ((ptrdiff_t)X & (sizeof(ptrdiff_t) - 1))
#define MYMEMCHR_TOO_SMALL(LEN)  ((LEN) < MYMEMCHR_LBLOCKSIZE)
#define MYMEMCHR_LBLOCKSIZE (sizeof (ptrdiff_t))
#if _WIN64
/* Nonzero if X contains a NULL byte. */
#define MYMEMCHR_DETECTNULL(X) (((X)-0x0101010101010101) & ~(X) & 0x8080808080808080)
#else
#define MYMEMCHR_DETECTNULL(X) (((X)-0x01010101) & ~(X) & 0x80808080)
#endif
#define MYMEMCHR_DETECTCHAR(X, MASK) (MYMEMCHR_DETECTNULL(X ^ MASK))

void * MyMemChr(void * ptr, int c, size_t length) {

	const unsigned char *src = (const unsigned char *)ptr;
	unsigned char d = c;

	ptrdiff_t *asrc;
	ptrdiff_t mask;
	unsigned int i;

	while (MYMEMCHR_UNALIGNED(src)) {
		if (!length--) return nullptr;
		if (*src == d) return (void *)src;
		src++;
	}

	if (!MYMEMCHR_TOO_SMALL(length)) {
		/* If we get this far, we know that length is large and src is
		   word-aligned. */
		/* The fast code reads the source one word at a time and only
			performs the bytewise search on word-sized segments if they
			contain the search character, which is detected by XORing
			the word-sized segment with a word-sized block of the search
			character and then detecting for the presence of NUL in the
			result.  */
		asrc = (ptrdiff_t *)src;
		mask = d << 8 | d;
		mask = mask << 16 | mask;
		for (i = 32; i < MYMEMCHR_LBLOCKSIZE * 8; i <<= 1) mask = (mask << i) | mask;

		while (length >= MYMEMCHR_LBLOCKSIZE) {
			if (MYMEMCHR_DETECTCHAR(*asrc, mask)) break;
			length -= MYMEMCHR_LBLOCKSIZE;
			asrc++;
		}
		/* If there are fewer than LBLOCKSIZE characters left,
		   then we resort to the bytewise loop.  */

		src = (unsigned char*)asrc;
	}

	while (length--) {
		if (*src == d) return (void *)src;
		src++;
	}
	
	return nullptr;
}
