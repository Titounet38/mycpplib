#define _CRT_SECURE_NO_WARNINGS

#include "xVect.h"
#include "OlgaUtils.h"
#include "Converter.h"
#include "Axis.h"
#include "OlgaUtils.h"

#ifdef SIMULATE_NO_WIN32
#define SIMULATE_NO_WIN32_ENABLED
#undef _WIN32
#endif

#ifdef _WIN32
#include "WinUtils.h" // Only for SplitPath (path separator is OS dependant)
#endif

extern aVect<Branch> branches;

LogFile * pLogFile;
Olga_params g_params;

const char * g_x_str     = "Boundaries";
const char * g_z_str     = "BoundaryElevation";

struct Olga_paramInfo g_paramInfo[] = {
    OLGA_PARAM_STRING(rho_gaz_str),
    OLGA_PARAM_STRING(v_gaz_str),
    OLGA_PARAM_STRING(Twall_str),
    OLGA_PARAM_STRING(P_str),
    OLGA_PARAM_STRING(vLiq_str),
    OLGA_PARAM_STRING(HOL_str),
	OLGA_PARAM_STRING(GT_str),
    OLGA_PARAM_DOUBLE(rho_v2_max),
    OLGA_PARAM_DOUBLE(rho_v2_threshold),
	OLGA_PARAM_DOUBLE(vPig_max),
    OLGA_PARAM_DOUBLE(vPig_threshold),
	OLGA_PARAM_DOUBLE(tWall_threshold),
    OLGA_PARAM_DOUBLE(speed_of_sound),
    OLGA_PARAM_DOUBLE(tWall_min),
    OLGA_PARAM_DOUBLE(vLiq_max),
    OLGA_PARAM_DOUBLE(tCheck_vLiq),
    OLGA_PARAM_DOUBLE(HOL_Check_vLiq),
	OLGA_PARAM_DOUBLE(HOL_Check_rho_v2)
};

Olga_params & Olga_Params() {
	return g_params;
}

const char ** Olga_x_str(char * new_x_str) {

	static char ** x_str = nullptr;

	if (new_x_str) {
		bool found = false;
		xVect_static_for(x_str, i) {
			if (strcmp_caseInsensitive(x_str[i], new_x_str) == 0) {
				found = true;
				break;
			}
		}
		if (!found) xVect_Push_ByVal(x_str, xVect_Asprintf("%s", new_x_str));
	}

	return (const char**)x_str;
}

const char ** Olga_z_str(char * new_z_str) {

	static char ** z_str = nullptr;

	if (new_z_str) {
		bool found = false;
		xVect_static_for(z_str, i) {
			if (strcmp_caseInsensitive(z_str[i], new_z_str) == 0) {
				found = true;
				break;
			}
		}

		if (!found) xVect_Push_ByVal(z_str, xVect_Asprintf("%s", new_z_str));
	}

	return (const char**)z_str;
}

void Olga_SetLogFile(LogFile & p) {
	pLogFile = &p;
}

size_t Olga_AppendDataToArray(File & src, mVect<double> * t, size_t n, 
	size_t * nLines, FILE * dst) {

    size_t i = 0, j = 0;
    CharPointer buffer, str;

    while(true) {

        buffer = src.GetLine();
		if (dst) fprintf(dst, "%s\n", (const char*)buffer);
		if (nLines && buffer) (*nLines)++;
        if (!buffer) {
            if (pLogFile) pLogFile->NoTime().Printf(2, "\nUnexpected end of file\n");
            break;
        }

        while(true) {
            str = RetrieveStr(buffer);
            if (!str) break;

            if (t) t->Push(Atof(str));
            ++i;

            if (i == n) {
                if (!false) {
                    goto fin;
                }
                i = 0;
                ++j;
            }
        }
    }
fin:

    return i;
}


size_t Olga_AppendDataToArray(
	File & src, 
	aVect< aVect<double> > * t,
	size_t n, 
	aVect<size_t> & indArray, 
	FILE * dst, 
	double timeStop,
	aVect<int> * optional_wantedInd) 
{

    int i = 0, j = 0;
    CharPointer buffer, str;

	if (t) t->Redim(indArray.Count());

	aVect<int> localWantedInd;

	auto&& wantedInd = optional_wantedInd ? *optional_wantedInd : localWantedInd;

	if (wantedInd.Count() == 0) {

		wantedInd.Redim(n);
		wantedInd.Set(-1);

		for (size_t i = 0; i < n; ++i) {
			aVect_static_for(indArray, k) {
				if (i == indArray[k]) wantedInd[i] = (int)k;
			}
		}
	}

    while(true) {

        buffer = src.GetLine();

		if (i==0 && timeStop >= 0 && buffer && Atof(buffer) >= timeStop) break;

		if (dst && buffer) fprintf(dst, "%s\n", (const char*)buffer);

        if (!buffer) {
            if (pLogFile) pLogFile->NoTime().Printf(2, "\nUnexpected end of file\n");
            break;
        }

        while(true) {
            str = RetrieveStr(buffer);
            if (!str) break;

			if (t) {
				//aVect_static_for(indArray, k) {
				//	if (i==indArray[k]) (*t)[k].Push(Atof(str));
				//}
				if (wantedInd[i] >= 0) (*t)[wantedInd[i]].Push(Atof(str));
			}

            ++i;

            if (i == n) {
                if (!false) {
                    goto fin;
                }
                i = 0;
                ++j;
            }
        }
    }
fin:

    return i;
}

void Olga_GetVarNames(aVect<char>& path) {

    aVect<char> fileName("OLGA_VarNames.txt");
    aVect<char> fullPath("%s\\%s", (char*)path, (char*)fileName);

    int pathLvl = 0;

    aVect_static_for(fullPath, i) if (fullPath[i]=='\\') pathLvl++;

    FILE * f = NULL;

    while(true) {
        f = fopen(fullPath, "rb");
        if (f) break;
        if (--pathLvl<0) MY_ERROR("Fichier ""OLGA_VarNames.txt"" introuvable");
        aVect<char> tmp("..\\%s", (char*)fileName);
        fileName.Steal(tmp);
        fullPath.sprintf("%s\\%s", (char*)path, (char*)fileName);
    };

    aVect<bool> found(NUMEL(g_paramInfo));
    found.Set(false);
	CharPointer buf, str;

    while(buf = MyGetLine(f)) {
        for (size_t i=0; i<NUMEL(g_paramInfo); ++i) {
            if (!found[i] && strcmp_caseInsensitive(buf, g_paramInfo[i].name) == 0) {
                found[i] = true;
				while (true) {
					str = MyGetLine(f);
					str.RemoveExtraBlanks();
					if (!str || !str[0] || str[0] == '[') break;
					if (str && (str[0] || !str[1]) && str[0] != '/' && str[1] != '/') {
						if (g_paramInfo[i].isString) {
							
							char *** ptr = (char ***)g_paramInfo[i].str;
							xVect_Push_ByVal(*ptr, (char*)nullptr);
							xVect_sprintf(xVect_Last(*ptr), "%s", str);
						}
						else *(g_paramInfo[i].value) = Atof(str);
					}
				}
				break;
            }
        }
    }

    for (size_t i=0; i<NUMEL(g_paramInfo); ++i) {
        if (!found[i]) {
            aVect<char> str("Parametre ""%s"" introuvable", g_paramInfo[i].name);
            MY_ERROR((char*)str);
        }
    }

    fclose(f);

    MyGetLine(NULL);
}

aVect< aVect<double> > Olga_GetTrends(
	char * sourceFile, 
	aVect< aVect<char> > & varNames, 
	double stopTime, 
	double * startTime, 
	FILE * dstFile, 
	bool writeHeader,
	aVect<aVect<char>> * types,
	aVect<aVect<char>> * branchNames,
	aVect<aVect<char>> * pipeNames,
	aVect<aVect<char>> * sectionIndices,
	aVect<aVect<char>> * units,
	aVect<aVect<char>> * descriptions) 
{

    double t_start = clock();
    CharPointer buffer;
    File src(sourceFile, "rb");

    ASSERT_ALWAYS(src.Open(), aVect<char>("Could not open input file : %s", strerror(errno)));

    do {
        buffer = src.GetLine().Trim();
		if (writeHeader && dstFile) fprintf(dstFile, "%s\n", (const char*)buffer);
        if (!buffer) {
			if (pLogFile) pLogFile->Printf("Error in tpl file : 'CATALOG' not found !");
			return aVect< aVect<double> >();
		}
    } while (strcmp(buffer, "CATALOG"));

    buffer = src.GetLine();
	if (writeHeader && dstFile) fprintf(dstFile, "%s\n", (const char*)buffer);
    int nVar = atoi(buffer);

	if ((types          && types->Count()          != varNames.Count()) ||
		(branchNames    && branchNames->Count()    != varNames.Count()) ||
		(pipeNames      && pipeNames->Count()      != varNames.Count()) ||
		(sectionIndices && sectionIndices->Count() != varNames.Count()) ||
		(units          && units->Count()          != varNames.Count()) ||
		(descriptions   && descriptions->Count()   != varNames.Count())) 
	{
		MY_ERROR("arrays must be the same size");
	}

	aVect<size_t> indArray;
	aVect<size_t> perm;
	size_t N = varNames ? varNames.Count() : nVar;
	
	if (!varNames) {
		N++;
		varNames.Redim(N);
		varNames[0] = "time";
		if (types)		    types->Redim(0).Redim(N);
		if (branchNames)    branchNames->Redim(0).Redim(N);
		if (pipeNames)      pipeNames->Redim(0).Redim(N);
		if (sectionIndices) sectionIndices->Redim(0).Redim(N);
		if (descriptions)   descriptions->Redim(0).Redim(N);
		if (units)		    units->Redim(0).Redim(N);
	}

	aVect<bool> found(N);

	found.Set(false); 

    for (int i=0 ; i<nVar ; ++i) {
        buffer = src.GetLine();
		if (writeHeader && dstFile) fprintf(dstFile, "%s\n", (const char*)buffer);
		
		CharPointer varName = RetrieveStr(buffer);
		CharPointer type = RetrieveStr<' ', '\'', ':'>(buffer);

		auto isGlobal = strcmp(type, "GLOBAL") == 0;
		auto isBranch = strcmp(type, "BRANCH") == 0;
		auto isSectionOrBoundary = strcmp(type, "SECTION") == 0 || strcmp(type, "BOUNDARY") == 0;

		if (isSectionOrBoundary) RetrieveStr<'\'', ' '>(buffer);

		CharPointer branchName = isGlobal ? nullptr : RetrieveStr<'\''>(buffer);

		CharPointer pipeName = isSectionOrBoundary ? RetrieveStr<'\'', ' '>(buffer), RetrieveStr<'\''>(buffer) : nullptr;
		CharPointer sectionIndex = isSectionOrBoundary ? RetrieveStr<'\'', ' '>(buffer), RetrieveStr<'\''>(buffer) : nullptr;
		CharPointer unit = RetrieveStr<' ', '\'', '(', ')'>(buffer);
		CharPointer description = RetrieveStr<'\''>(buffer);

		bool ok = false;

		for (size_t j = 0; j < N; ++j) {
			if ((                 !varNames[j]          || strcmp_caseInsensitive(varName,      varNames[j]) == 0)          &&
				(!types        || !(*types)[j]          || strcmp_caseInsensitive(type,         (*types)[j]) == 0)          &&
				(!branchNames  || !(*branchNames)[j]    || strcmp_caseInsensitive(branchName,   (*branchNames)[j]) == 0)    &&
				(!pipeNames    || !(*pipeNames)[j]      || strcmp_caseInsensitive(pipeName,     (*pipeNames)[j]) == 0)      &&
				(!sectionIndex || !(*sectionIndices)[j] || strcmp_caseInsensitive(sectionIndex, (*sectionIndices)[j]) == 0) &&
				(!descriptions || !(*descriptions)[j]   || strcmp_caseInsensitive(description,  (*descriptions)[j]) == 0)   &&
				(!units        || !(*units)[j]          || strcmp_caseInsensitive(unit,         (*units)[j]) == 0))
			{
				indArray.Push(i + 1);
				perm.Push(j);
				if (found[j]) {
					j++; N++;
					found.Insert(j, true);
					varNames.Insert(j, varName);
					if (types)		  types->Insert(j, type);
					if (branchNames)  branchNames->Insert(j, branchName);
					if (pipeNames)    pipeNames->Insert(j, pipeName);
					if (sectionIndex) sectionIndices->Insert(j, sectionIndex);
					if (descriptions) descriptions->Insert(j, description);
					if (units)		  units->Insert(j, unit);
				}
				else {
					found[j] = true;
					varNames[j] = varName;
					if (types)		    (*types)[j] = type;
					if (branchNames)    (*branchNames)[j] = branchName;
					if (pipeNames)	    (*pipeNames)[j] = pipeName;
					if (sectionIndices)	(*sectionIndices)[j] = sectionIndex;
					if (descriptions)   (*descriptions)[j] = description;
					if (units)		    (*units)[j] = unit;
				}
				break;
			}
		}
    }

	bool timeRequested = false;
	bool timeInserted = false;
	size_t timeInd = 0;

	aVect_static_for(varNames, i) {
		if (strcmp_caseInsensitive("time", varNames[i]) == 0) {
			indArray.Insert(0, i);
			found[i] = true;
			timeRequested = true;
			timeInd = i;
		}
	}

	if ((stopTime >= 0 || startTime) && !timeRequested) {
		timeInd = 0;
		indArray.Insert(0, timeInd);
		timeInserted = true;
	}

	aVect_for_inv(found, i) {
		if (!found[i]) {
			if (varNames[i]) {
				MY_ERROR(aVect<char>("Variable \"%s\" introuvable", (char*)varNames[i]));
			}
			else {
				found.Remove(i);
				varNames.Remove(i);
				if (types)		    types->Remove(i);
				if (branchNames)    branchNames->Remove(i);
				if (pipeNames)      pipeNames->Remove(i);
				if (sectionIndices) sectionIndices->Remove(i);
				if (descriptions)   descriptions->Remove(i);
				if (units)		    units->Remove(i);
			}
		}
	}

    buffer = src.GetLine();
	if (writeHeader && dstFile) fprintf(dstFile, "%s\n", (const char*)buffer);
	aVect< aVect<double> > t(2);

	size_t n = 0;

	aVect<int> wantedInd;

	while(true) {
		if (nVar + 1 != Olga_AppendDataToArray(src, &t, nVar + 1, indArray, dstFile, stopTime, &wantedInd)) {
			aVect_static_for(t, i) t[i].Redim(n);
			break;
		}
		n++;
		if (src.Eof()) break;
	}
    src.Close();

	if (startTime) *startTime = t ? t[timeInd] ? t[timeInd][0] : -1 : -1;
	if (!timeRequested && timeInserted) t.Remove(timeInd);

	aVect<aVect<double>> t2(t.Count());

	if (timeRequested) perm.Insert(0, 0);

	aVect_static_for(perm, i) {
		t2[perm[i]] = std::move(t[i]);
	}

	return std::move(t2);
}


aVect<char> GetBranchName(char * varName) {

	aVect<char> retVal;

	const char * ptr = strstr(varName, "'BRANCH:'");

	if (ptr) {
		ptr += strlen("'BRANCH:'");
		aVect<char> str(ptr);
		CharPointer token(str);
		token = RetrieveStr<'\''>(token.Trim());
		retVal.Copy(token);
	}

	return std::move(retVal);
}

bool Olga_FileFastForwardToString(File & f, const char * str, bool errorIfNotFound = true) {

	CharPointer line;

	while(line = f.GetLine()) {
		if (strstr(line, str)) {
			return true;
		}
	}
	if (errorIfNotFound) MY_ERROR(aVect<char>("\"%s\" not found in file \"%s\"", str, (char*)f.GetFilePath()));
	return false;
}

void Olga_MergeProfileFiles2(aVect<char> & mergeFileName, aVect< aVect<char> > files2Merge, double timeStop) {

	//Get start time and durations
	aVect<double> filesStartTime(files2Merge.Count());
	aVect<double> filesDuration (files2Merge.Count());

	filesStartTime.Set(std::numeric_limits<double>::infinity());
	filesDuration .Set(std::numeric_limits<double>::infinity());

	size_t headerNumberOfLines = 0;

	aVect_static_for(files2Merge, i) {

		File f(files2Merge[i], "rb");
		CharPointer line;
		ConstCharPointer balise("TIME SERIES");
		const short baliseLen = 11;
		size_t nLines = 0;

		while (line = f.GetLine()) {
			nLines++;
			for (int j=0; j<baliseLen; ++j) {
				if (line[j] != balise[j]) goto next_line;
			}
			filesStartTime[i] = Atof(f.GetLine(true));
			goto next_file;
next_line:;
		}
		MY_ERROR(aVect<char>("File \"%s\" is corrupted ('TIME SERIES' not found)", (char*)files2Merge[i]));
next_file:
		if (headerNumberOfLines == 0) headerNumberOfLines = nLines;
		else if (headerNumberOfLines != nLines) MY_ERROR("headers should be the same");
	}

	for (int i=(long)filesDuration.Count() - 2; i >= 0; --i) {
		double duration = filesStartTime[i+1] - filesStartTime[i];
		if (!_finite(duration)) MY_ERROR("uninitialized filesStartTime");
		if (duration <= 0) {
			filesStartTime.Remove(i);
			filesDuration .Remove(i);
			files2Merge   .Remove(i);
		}
		else {
			filesDuration[i] = duration;
		}
	}

	if (files2Merge) {

		File f(files2Merge[0], "rb");
		CharPointer line, token;

		//Read geometry
		Olga_FileFastForwardToString(f, "NETWORK");
		unsigned nBranches = atoi(f.GetLine(true));
		aVect<unsigned>    branchesNumberOfElements(nBranches);
		aVect<aVect<char>> branchesName(nBranches);
		Olga_FileFastForwardToString(f, "GEOMETRY");
		for (unsigned i=0; i<nBranches; ++i) {
			Olga_FileFastForwardToString(f, "BRANCH");
			line = f.GetLine(true);
			branchesName[i].Copy(RetrieveStr<'\''>(line));	//branch name
			branchesNumberOfElements[i] = atoi(f.GetLine(true));		//number of elements
		}

		//Read catalog
		Olga_FileFastForwardToString(f, "CATALOG");
		unsigned nVar = atoi(f.GetLine(true));
		aVect<unsigned> varNumberOfElements(nVar);
		for (unsigned i=0; i<nVar; ++i) {
			line = f.GetLine(true);
			RetrieveStr<' ', '\''>(line);			//variable name
			token = RetrieveStr<' ', '\''>(line);	//element type 
			bool isBoundary = (strcmp(token, "BOUNDARY:")==0);
			RetrieveStr<' ', '\''>(line);			//"BRANCH"
			token = RetrieveStr<'\''>(line);	    // branch name
			aVect_static_for(branchesName, j) {
				if (strcmp(branchesName[j], token) == 0) {
					varNumberOfElements[i] = branchesNumberOfElements[j] + isBoundary;
					goto found_branch;
				}
			}
			MY_ERROR(aVect<char>("branch not found : \"%s\"", token));
found_branch:;
		}

		//Compute nLinePerTimeRecord
		const short olga_nValuesPerLine = 6;
		size_t nLinePerTimeRecord = 0;
		for (unsigned i=0; i<nVar; ++i) {
			nLinePerTimeRecord += varNumberOfElements[i] / olga_nValuesPerLine;
			if (varNumberOfElements[i] % olga_nValuesPerLine) nLinePerTimeRecord++;
		}


		File dst(mergeFileName, "w");
		File src(files2Merge[0], "w");
		
		//Copy header once
		for (unsigned i=0; i<headerNumberOfLines; ++i) {
			fprintf(dst, "%s\n", (char*)src.GetLine(true));
		}
		
		//Merge files into destination
		aVect_static_for(files2Merge, i) {
			src.Open(files2Merge[i], "rb");

			//skip source header
			for (unsigned j=0; j<headerNumberOfLines; ++j) {
				src.GetLine(true);
			}
			
			//copy data until start time + duration
			double timeLimit = filesStartTime[i] + filesDuration[i];
			
			if (pLogFile) pLogFile->Printf("Working on: \"%s\"\n", (char*)src.GetFilePath());

			while (true) {
				line = src.GetLine();
				if (!line) {
					if (pLogFile) pLogFile->Printf("Unexpected end of file: \"%s\"\n", (char*)src.GetFilePath());
					break;
				}
				double time = Atof(line);
				if (timeStop > 0 && time > timeStop) return;
				if (time > timeLimit) break;
				fprintf(dst, "%s\n", (const char*)line);
				for (unsigned j=0; j<nLinePerTimeRecord; ++j) {
					line = src.GetLine();
					if (!line) {
						if (pLogFile) pLogFile->Printf("Unexpected end of file: \"%s\"\n", (char*)src.GetFilePath());
						if (pLogFile) pLogFile->Printf("Writing \"#MISSING DATA#\" in place of mising data.\n");
						fprintf(dst, "#MISSING DATA#\n");
						break;
					}
					fprintf(dst, "%s\n", (const char*)line);
				}
			} 
		}
	}
}

int Olga_WalkProfiles(
	char * sourceFile,
	//mVect< mVect<char> > * pVarNames,
	aVect<Olga_OutputVariable> *  pVarNames,
	bool (*func)(
		double, aVect<Olga_OutputVariable> *,
		mVect< mVect<double> > &,
		bool, bool, bool,
		mVect< mVect<char> > *,
		mVect<double> *,
		void *
	), 
	char * mergeFile, 
	double * startTime, 
	double * endTime,
	size_t * nLinesToStartTime, 
	char * outputFile,
	mVect< mVect<char> > * pTplVarNames,
	char * tplFile,
	void * userParam,
	bool displayProgressBar)
{

	int retCode = OLGA_WP_RAS;

	double t_start = clock();

	File src;
	FILE * merge = nullptr;
	
	__int64 srcSize = 0;
	bool justGetStartAndEndTime = false;
	aVect<char> tmpMergeFile;

	//static mVect<double> tplResults;
	mVect<double> tplResults;

	bool getAllVar = (pVarNames && pVarNames->Count() == 0);

	if (sourceFile) {
		src.Open(sourceFile, "rb");

		ASSERT_ALWAYS(src, aVect<char>("Could not open input file : \"%s\"", sourceFile));

		srcSize = src.GetSize();

		if (sourceFile) if (pLogFile) pLogFile->NoTime().Printf(2, "%.2f Mo\n", srcSize/(1024.*1024.));

		if (mergeFile) {
			double srcStartTime;
			size_t nLines = 0;
			Olga_WalkProfiles(nullptr, nullptr, nullptr, sourceFile, &srcStartTime, nullptr, nullptr, outputFile);
			Olga_WalkProfiles(nullptr, nullptr, nullptr, mergeFile, &srcStartTime, nullptr, &nLines, outputFile);
			if (nLines == 0) Olga_WalkProfiles(nullptr, nullptr, nullptr, sourceFile, &srcStartTime, nullptr, &nLines, outputFile);
			merge = fopen(mergeFile, "rb");
			tmpMergeFile.sprintf("%s.tmp", mergeFile);
			FILE * tmpMerge = fopen(tmpMergeFile, "w");
			ASSERT_ALWAYS(tmpMerge, "Could not open tmp file");
			if (!merge) merge = src;
			CharPointer line;
			size_t i=0;
			while(line = MyGetLine(merge)) {
				i++;
				if (i>nLines) break;
				fprintf(tmpMerge, "%s\n", (char*)line);
			}
			fclose(merge);
			fflush(tmpMerge);
			MyGetLine(nullptr);
			src.Close();
			src.Open(sourceFile, "rb");
			merge = tmpMerge;
		}
		else {
			if (!pVarNames) {
				src.Close();
				return OLGA_WP_RAS;
			}
		}
	}
	else if (mergeFile) {
		if (startTime || endTime) justGetStartAndEndTime = true;
		else return OLGA_WP_RAS;
		src.Open(mergeFile, "rb");
		if (!src) {
			if (startTime) *startTime = 0;
			if (endTime) *endTime = 0;
			return OLGA_WP_RAS;
		}

		_fseeki64(src, 0, SEEK_END);
		srcSize = _ftelli64(src);
		_fseeki64(src, 0, SEEK_SET);
	}

	CharPointer buffer, ptr, str;

	mVect< mVect<double> > arrays;

	bool firstPass = true;
	aVect<bool> isBoundary;
	size_t nVar = 0;
	aVect<size_t> nElem, var_nElem;
	aVect< aVect<char> > branchNames;
	unsigned nFakeVar = 0;
	mVect<double> boundaries, boundaryElevation;
	aVect<int> wantedArrays;
	UnitConverter convToSeconds;
	CharPointer timeUnit;

	ProgressBar progressBar(xFormat("Loading %s MB", NiceFloat(srcSize / 1e6, 1e-3)), true);

	//progressBar.SetProgress(0).Br

	if (func) func(0, nullptr, arrays, true, false, false, nullptr, nullptr, userParam);
	MyGetLine(nullptr);

	if (nLinesToStartTime) *nLinesToStartTime = 0;
	
	while(true) {
		if (firstPass) {
			do {
				buffer = src.GetLine();
				if (nLinesToStartTime) (*nLinesToStartTime)++;
				ptr = buffer;
				RetrieveStr(ptr);
				if (!buffer) {
					if (pLogFile) pLogFile->Printf(2, "'GEOMETRY' not found !\n");
					return OLGA_WP_CORRUPT;
				}
			} while (strcmp(buffer, "GEOMETRY"));

			bool branchFound = false;
			auto geometryUnit = RetrieveStr<' ', '\'', '(', ')'>(ptr);

			if (strcmp(geometryUnit, "M") == 0) geometryUnit = "m";

			if (sourceFile) if (pLogFile) pLogFile->NoTime().Printf(2, "%s :\n", sourceFile);

			while (true) {
				buffer = src.GetLine();
				if (strcmp(buffer, "BRANCH") != 0) {
					if (!branchFound) {
						if (pLogFile) pLogFile->Printf(2, "'BRANCH' not found !\n");
						return OLGA_WP_CORRUPT;
					}
					else {
						break;
					}
				}

				branchFound = true;

				//read branch name
				buffer = src.GetLine();
				if (nLinesToStartTime) (*nLinesToStartTime) += 2;

				ptr = buffer;
				str = RetrieveStr<'\''>(ptr);

				branchNames.Push(str);

				if (sourceFile) if (pLogFile) pLogFile->NoTime().Printf(2, "\tBranch Name: %s\n", (char*)branchNames.Last());

				buffer = src.GetLine();
				if (nLinesToStartTime) (*nLinesToStartTime)++;
				if (!buffer) {
					if (pLogFile) pLogFile->Printf(2, "'GEOMETRY' not found !\n");
					return OLGA_WP_CORRUPT;
				}
				nElem.Push(atoi(buffer));

				boundaries.Redim(0);
				Olga_AppendDataToArray(src, &boundaries, nElem.Last()+1, nLinesToStartTime);
				boundaryElevation.Redim(0);
				Olga_AppendDataToArray(src, &boundaryElevation, nElem.Last()+1, nLinesToStartTime);

				if (pVarNames) {
					aVect<char> x_str("Boundaries 'BRANCH:' '%s'", (char*)branchNames.Last());
					aVect<char> z_str("BoundaryElevation 'BRANCH:' '%s'", (char*)branchNames.Last());

					Olga_x_str(x_str);
					Olga_z_str(z_str);

					pVarNames->Insert(0, "Boundaries", "branch", branchNames.Last(), nullptr, nullptr, geometryUnit, "Boundaries", true, true);
					pVarNames->Insert(0, "BoundaryElevation", "branch", branchNames.Last(), nullptr, nullptr, geometryUnit, "Boundaries Elevation", true, true);

					arrays.Insert(0, boundaries);
					arrays.Insert(0, boundaryElevation);
					nFakeVar += 2;
				}
			}
		}

		if (firstPass) {

			if (nLinesToStartTime) (*nLinesToStartTime)++;
			if (!buffer || strcmp(buffer.Trim(), "CATALOG")!=0) {
				if (pLogFile) pLogFile->Printf(2, "'GEOMETRY' not found !\n");
				return OLGA_WP_CORRUPT;
			}
			buffer = src.GetLine();
			if (nLinesToStartTime) (*nLinesToStartTime)++;

			nVar = atoi(buffer);
			if (sourceFile) if (pLogFile) pLogFile->NoTime().Printf(2, "\t%d variables\n", nVar);
            
			arrays.Grow(nVar);
			isBoundary.Redim(nVar);
			var_nElem.Redim(nVar);
			wantedArrays.Redim(nVar);
			wantedArrays.Set(-1);

			for (size_t i=0 ; i<nVar ; ++i) {
				buffer = src.GetLine();

				if (!buffer) if (pLogFile) {
					pLogFile->NoTime().Printf(2, "_a_ Unexpected end of file\n", nVar);
					retCode = OLGA_WP_CORRUPT;
					goto fin;
				}

				buffer.RemoveExtraBlanks();
				if (nLinesToStartTime) (*nLinesToStartTime)++;

				if (const char * a = strstr(buffer, "BOUNDARY")) {
					if (const char * b = strstr(buffer, "SECTION")) {
						if (a<b) isBoundary[i] = true;
						else isBoundary[i] = false;
					}
					else isBoundary[i] = true;
				}
				else isBoundary[i] = false;

				if (pVarNames) {

					CharPointer varName = RetrieveStr(buffer);
					RetrieveStr<' ', '\''>(buffer);
					RetrieveStr<' ', '\''>(buffer);
					CharPointer branchName = RetrieveStr<'\''>(buffer);
					CharPointer unit = RetrieveStr<' ', '\'', '(', ')'>(buffer);
					CharPointer description = RetrieveStr<'\''>(buffer);
					//for (size_t j=nFakeVar, J=pVarNames->Count() ; j<J ; ++j) {
					
					bool foundBranch = false;
					aVect_static_for(branchNames, k) {
						if (strcmp_caseInsensitive(branchNames[k], branchName) == 0) {
							var_nElem[i] = nElem[k];
							if (isBoundary[i]) var_nElem[i]++;
							foundBranch = true;
						}
					}

					if (!foundBranch) MY_ERROR(aVect<char>("Branch name not found in var \"%s\"", buffer));

					if (getAllVar) {
						pVarNames->Push(varName, "branch", branchName, nullptr, nullptr, unit, description, isBoundary[i], false);
						wantedArrays[i] = (int)pVarNames->Index(pVarNames->Last());
					}
					else {
						for (size_t j = nFakeVar, J = pVarNames->Count(); j<J; ++j) {
							auto&& requestedVar = (*pVarNames)[j];
							if (strcmp_caseInsensitive(requestedVar.name, varName) == 0) {
								if (!requestedVar.branchName || strcmp_caseInsensitive(requestedVar.branchName, branchName) == 0) {
									requestedVar.isBoundaryValues = isBoundary[i];
									wantedArrays[i] = (int)j;
									requestedVar.unit = unit;
									requestedVar.description = description;
									if (!requestedVar.branchName) {
										requestedVar.branchName = branchName;
										//auto ind = pVarNames->Index(requestedVar);
										pVarNames->Push(varName);
									}
									break;
								}
							}
						}
					}
				}
			}

			for (auto&& requestedVar : Reverse(*pVarNames)) {
				if (!requestedVar.branchName) pVarNames->RemoveElement(requestedVar);
			}

			if (pVarNames) {
				aVect<bool> check(pVarNames->Count());
				check.Set(false);
				aVect_for_inv(wantedArrays, i) {
					if (wantedArrays[i]>0) check[wantedArrays[i]] = true;
				}
				for (size_t i=nFakeVar, I=check.Count() ; i<I ; ++i) {
					if (!check[i]) {
						MY_ERROR(xFormat("Variable introuvable :\n%s\n branch\n:%s\n", (*pVarNames)[i].name, (*pVarNames)[i].branchName));
					}
				}
			}

			buffer = src.GetLine();
			if (nLinesToStartTime) (*nLinesToStartTime)++;
			firstPass = false;
		}
		break;
	}

	int clk = 0;
	int j=0;

	double time = 0;
	if (startTime && !nLinesToStartTime) *startTime = -1;
	if (endTime)   *endTime = -1;

	RetrieveStr<'(', ')'>(buffer);
	timeUnit = RetrieveStr<'(', ')'>(buffer);

	if (strcmp(timeUnit, "S") == 0) timeUnit = "s";

	convToSeconds.From(timeUnit).To("s");

	while(true) {

		buffer = src.GetLine();
		if (merge && mergeFile && buffer) {
			fprintf(merge, "%s\n", (char*)buffer);
		}

		buffer = RetrieveStr(buffer);

		if (displayProgressBar) {
			progressBar.SetProgress(src.GetCurrentPosition() / (double)srcSize);
			if (!progressBar) return -1;
		}

		if (func && !buffer || clock() - clk > 100) {
			if (pLogFile) pLogFile->NoWrite().Printf(2, " %.2f %%  (t = %f s)        \r", 100.*src.GetCurrentPosition()/(double)srcSize, time);
			clk = clock();
		}

		if (!buffer) {
			for (unsigned i=0; i<nFakeVar; ++i) arrays[i].Redim(0);
			if (func) {
				func(std::numeric_limits<double>::infinity(), pVarNames, arrays, false, true, false, nullptr, nullptr, userParam);
				if (pLogFile) pLogFile->NoWrite().Printf(2, "\n");
			}
			break;
		}

		time = convToSeconds(Atof(buffer));
		if (startTime && *startTime == -1) 
			*startTime = time;

		if (nLinesToStartTime && startTime && time < *startTime) (*nLinesToStartTime)++;

		if (src) {
			for (size_t i=0 ; i<nVar ; ++i) {
				//size_t n = nElem;

				mVect<double> * t = (wantedArrays[i]>0) ? &arrays[wantedArrays[i]] : NULL;
				
				if (t) t->Redim(0);
				size_t nElRead = Olga_AppendDataToArray(src, t, var_nElem[i],
					(startTime && time < *startTime) ? nLinesToStartTime : nullptr, merge);
				if (var_nElem[i]!=nElRead) {
					if (pLogFile) pLogFile->Printf(2, "Erreur de lecture.\n");
					retCode = OLGA_WP_CORRUPT;
					goto fin;
				};
			}
		}

		aVect< aVect<char> >   tplVarNames;
		aVect< aVect<double> > tplResults_aVect;

		if (pTplVarNames) {

			aVect<char> tplFileStr(tplFile);

			if (!tplFileStr) {
				aVect<char> fp, fn, fe;
				SplitPath(sourceFile, nullptr, &fp, &fn, &fe);
				tplFileStr.sprintf("%s\\%s.tpl", (char*)fp, (char*)fn);
			}

			tplVarNames.Push("time");
			mVect_static_for(*pTplVarNames, i) tplVarNames.Push((*pTplVarNames)[i]);
			tplResults_aVect = Olga_GetTrends(tplFileStr, tplVarNames);

			tplResults.Redim(tplResults_aVect.Count());

			for (size_t i=1, I=tplResults_aVect.Count(); i<I; ++i) {
				//tplResults[i].Steal(tplResults_aVect[i]);
				tplResults[i] = Interpolate(time, tplResults_aVect[0], tplResults_aVect[i]);
			}
		}

		bool retVal = func ? func(time, pVarNames, arrays, false, false, false, pTplVarNames, &tplResults, userParam) : false;

		//for (size_t i=2, I=arrays.Count() ; i<I ; ++i)  {
		//    arrays[i].Redim(0);
		//}

		if (retVal) goto fin;

		j++;
	}

	if (pLogFile) pLogFile->NoTime().Printf(2, "\nVerification terminee\n");
	retCode = OLGA_WP_FINISHED;

fin:

	if (endTime) *endTime = time;

	if (pLogFile) pLogFile->NoTime().Printf(2, "\nElapsed time : %f secondes\n", (clock() - t_start)/1000.0);

	src.Close();
	if (merge) fclose(merge);
	if (tmpMergeFile && mergeFile) {
		if (outputFile) {
			remove(outputFile);
			rename(tmpMergeFile, outputFile);
		}
		else {
			remove(mergeFile);
			rename(tmpMergeFile, mergeFile);
		}
	}

	return retCode;
}


void Olga_MergeProfileFiles(aVect<char> & filePath, aVect<char> & mergeFilePath, aVect<char> & outputFilePath) {
	Olga_WalkProfiles(filePath, nullptr, nullptr, mergeFilePath, nullptr, nullptr, nullptr, outputFilePath);
}

void Olga_MergeTrendFiles(aVect<char> & filePath, aVect<char> & mergeFilePath,
	aVect<char> & outputFilePath) {

	aVect<char> tmpTplFilePath("%s.tmp", (char*)mergeFilePath);
	FILE * tmpTplFile = fopen(tmpTplFilePath, "w");
	if (!tmpTplFile) MY_ERROR(aVect<char>("Impossible de creer le fichier \"%s\"", (char*)tmpTplFilePath));

	aVect< aVect<char> > emptyVect;
	bool writeHeader = true;

	MY_ERROR("TODO : wchar_t everywhere");
	if (1/*IsFileReadable(mergeFilePath)*/) {
		double startTime;
		Olga_GetTrends(filePath, emptyVect, -1, &startTime);
		Olga_GetTrends(mergeFilePath, emptyVect, startTime, nullptr, tmpTplFile, true);
		writeHeader = false;
	}

	Olga_GetTrends(filePath, emptyVect, -1, nullptr, tmpTplFile, writeHeader);

	fclose(tmpTplFile);
	if (outputFilePath) {
		remove(outputFilePath);
		rename(tmpTplFilePath, outputFilePath);
	}
	else {
		remove(mergeFilePath);
		rename(tmpTplFilePath, mergeFilePath);
	}
}

//Following functions are usable in Excel dll
//interface functions to be guarded by a try-catch

bool CheckKeyword(const File & file, const char * tok, const char * keyword, bool abortIfError = false) {

	if (strcmp(tok, keyword) != 0) {
		if (abortIfError) {
			RECOVERABLE_ERROR(
				aVect<char>("File \"%s\": Missing keyword \"%s\"", 
					(char*)file.GetFilePath(), keyword),
				ERROR_INVALID_DATA);
		}
		return false;
	}
	else {
		return true;
	}
}

bool ReadKeyword(const File & file, CharPointer & line, const char * keyword, bool abortIfError = false) {

	if (!keyword) return true;

	CharPointer tok = RetrieveStr<' ', ','>(line);
	if (!tok) {
		if (abortIfError) {
			RECOVERABLE_ERROR(
				aVect<char>("File \"%s\": Unexpected end of line while searching for \"%s\"",
					(char*)file.GetFilePath(), keyword),
				ERROR_INVALID_DATA);
		}
		return false;
	}
	return CheckKeyword(file, tok, keyword, abortIfError);
}

bool FastForwardToKeyword(File & file, CharPointer & line, const char * keyword, bool abortIfError = false) {

	if (!keyword) return true;

	while (true) {

		if (!line) line = file.GetLine<'\\', true>();

		if (!line) {
			if (abortIfError) {
				RECOVERABLE_ERROR(
					aVect<char>(
						"File \"%s\": unexpected end of file while searching for \"%s\"", 
						(char*)file.GetFilePath(), keyword),
					ERROR_INVALID_DATA);
			}
			return false;
		}

		while (true) {
			if (ReadKeyword(file, line, keyword)) return true;
			else if (!line.Trim()) break;
		}
	}
}

bool Read_PVT_List(
	CachedVect<aVect<char> > & stringList,
	File & file, 
	CharPointer & line, 
	aVect<aVect<char> > & keywords,
	bool seekKeyword = true,
	bool abortIfError = false)
{

	stringList.Redim(0);

	if (seekKeyword) {
		if (!FastForwardToKeyword(file, line, keywords[0], abortIfError)) return false;

		for (size_t i = 1, I = keywords.Count(); i < I; ++i) {
			if (!ReadKeyword(file, line, keywords[i], abortIfError)) return false;
		}

		if (!ReadKeyword(file, line, "=", abortIfError)) return false;
	}

	CharPointer list = RetrieveStr<'(', ')'>(line);
	if (!list) {
		if (abortIfError) {
			RECOVERABLE_ERROR(
				xFormat("File \"%s\" : list expected after \"%s\"",
					file.GetFilePath(), keywords),
				ERROR_INVALID_DATA);
		}
		return false;
	}

	while (true) {
		CharPointer str = RetrieveStr<',', ' '>(list);
		if (!str) {
			if (!stringList) {
				if (abortIfError) {
					RECOVERABLE_ERROR(
						xFormat("File \"%s\" : number expected after \"%s\"", 
							file.GetFilePath(), keywords[0]),
						ERROR_INVALID_DATA);
				}
				return false;
			}
			else break;
		}

		stringList.Push(str);
	}

	return true;
}

bool Read_PVT_List(
	CachedVect<aVect<char> > & stringList,
	File & file,
	CharPointer & line,
	const char * keyword, 
	bool abortIfError = false)
{

	static WinCriticalSection cs;

	Critical_Section(cs) {
		
		static aVect<aVect<char> > keywords(1);

		bool seekKeyword = true;

		if (keyword) keywords[0].Copy(keyword);
		else seekKeyword = false;

		return Read_PVT_List(stringList, file, line, keywords, seekKeyword, abortIfError);
	}
	
	return false;//unreachable code, just to avoid compiler warnings
}

Olga_PVT_Table PVT_ReadError(const File & file, const char * msg) {
	
	RECOVERABLE_ERROR(
		aVect<char>("File \"%s\": %s",
			(char*)file.GetFilePath(),
			msg),
		ERROR_INVALID_DATA);

	return Olga_PVT_Table();
}

double GetValueInUnit(CharPointer & line, const char * unitTo) {
	
	static WinCriticalSection cs;

	Critical_Section(cs) {
		
		static UnitConverter converter;
		static aVect<char> retry;

		double value = Atof(RetrieveStr<' ', '=', ','>(line));

		if (!unitTo) return value;

		auto unitFrom = RetrieveStr<' ', '=', ','>(line);

		if (strcmp_caseInsensitive(unitFrom, "C") == 0) unitFrom = "°C";
		if (strcmp_caseInsensitive(unitFrom, "F") == 0) unitFrom = "°F";

		//ReplaceStr(unitFrom, "SM3", "sm3");

		if (!converter.SetUnits(unitFrom, unitTo).IsReady()) {
			retry = unitFrom;

			ReplaceStr(retry, "PA", "Pa");
			
			if (!converter.SetUnits(retry, unitTo).IsReady()) {
				RECOVERABLE_ERROR(converter.GetError(), ERROR_INVALID_DATA);
				return std::numeric_limits<double>::quiet_NaN();
			}
		}

		return converter.Convert(value);
	}
	
	return 0;//unreachable code, to avoid compiler warings;
}

void PushNumbers(
	aVect<double> * var, 
	size_t nPoints, 
	CharPointer & line, 
	WinFile & file) 
{

	if (var) var->Redim<false>(0);

	for (size_t i = 0; i<nPoints; ++i) {
		if (!line) {
			if (!(line = file.GetLine<0, true>())) MY_ERROR("unexpected end of file");
		}

		CharPointer tok = RetrieveStr(line);

		if (!tok) MY_ERROR("unexpected end of line");

		if (var) var->Push(Atof(tok));
	}
}

bool ReadVariableTable(
	CharPointer & line, 
	Olga_PVT_Table & table, 
	size_t nPressurePoints,
	size_t nTemperaturePoints,
	WinFile & file,
	//const char * strToFind,
	const char * varName)
{

	static WinCriticalSection cs;

	Critical_Section(cs) {
		//if (strstr(line, strToFind) == line) {

			auto&& var = table.variables.Push().Last();
			var.name = varName;

			//line += strlen(strToFind);
			//CharPointer tok = RetrieveStr<'(', ')'>(line.Trim());

			//static UnitConverter conv;

			//if (!tok) {
			//	conv.FromSI().ToSI();
			//}
			//else {
			//	tok.Trim();
			//	if (strcmp(tok, "KG/M3")		== 0) tok = "kg/m3";
			//	else if (strcmp(tok, "NS/M2")   == 0) tok = "N.s/m2";
			//	else if (strcmp(tok, "J/KG K")  == 0) tok = "J/kg/K";
			//	else if (strcmp(tok, "J/KG")	== 0) tok = "J/kg";
			//	else if (strcmp(tok, "J/KG")	== 0) tok = "J/kg";
			//	else if (strcmp(tok, "W/MK")	== 0) tok = "W/m/K";
			//	else if (strcmp(tok, "N/M")		== 0) tok = "N/m";
			//	else if (strcmp(tok, "J/KG/C")	== 0) tok = "J/kg/K";
			//	else if (strcmp(tok, "S2/M2")	== 0) tok = "s2/m2";
			//	else if (strcmp(tok, "KG/M3/K")	== 0) tok = "kg/m3/K";
			//	else if (strcmp(tok, "-") == 0)		  tok = "1";
			//	else MY_ERROR(xFormat("Unkown unit: %s", tok));

			//	if (!conv.From(tok).ToSI().IsReady()) MY_ERROR(xFormat("unkown unit: %s", tok));
			//}

			//var.unit.Copy(tok);
			var.value.Redim(nPressurePoints, nTemperaturePoints);

			static aVect<double> buf;
			buf.Redim(0);

			line = 0;
			PushNumbers(&buf, nTemperaturePoints * nPressurePoints, line, file);

			size_t k = 0;
			aMat_static_for(var.value, iP, iT) {
				//var.value(iP, iT) = conv(buf[k++]);
				var.value(iP, iT) = buf[k++];
			}

			//return true;
		//}
	}
	return true;
	//return false;
}

struct PVTVarNames {
	const char * newFormat;
	const char * oldFormat;

	PVTVarNames(const char * newFormat, const char * oldFormat)
		: newFormat(newFormat), oldFormat(oldFormat) {}
};

Olga_PVT_Table Olga_Read_PVT_Table_Format2(const wchar_t * fileName) {

	Olga_PVT_Table table;

	WinFile file(fileName, "rb");

	if (!file.Open()) {
		RECOVERABLE_ERROR(xFormat("Unable to open file:\n\"%S\"", fileName), ERROR_FILE_NOT_FOUND);
		return Olga_PVT_Table();
	}

	CharPointer line = file.GetLine().Trim();

	if (!line) {
		RECOVERABLE_ERROR(xFormat("\"%S\"\n\n file is empty", file.GetFilePath()), ERROR_INVALID_DATA);
		return Olga_PVT_Table();
	}

	bool noneq = false, water_opt = false;

	for (; line[0] == '\''; ) {
		line++;
		while (line) {
			if (line[0] == '\'') break;
			auto tok = RetrieveStr(line);
			if (strcmp_caseInsensitive(tok, "NONEQ") == 0) noneq = true;
			if (strcmp_caseInsensitive(tok, "WATER-OPTION") == 0) water_opt = true;
		}
		line = file.GetLine().Trim();
	}

	bool multiple_tables = false;

	CharPointer tok = RetrieveStr(line);
	if (!IsNumber(tok)) {
		//TODO: multiple tables
		multiple_tables = true;
		line = file.GetLine().Trim();
		line = file.GetLine().Trim();
		line = file.GetLine().Trim();
		line = file.GetLine().Trim();
		tok = RetrieveStr(line);
		if (!IsNumber(tok)) {
			MY_ERROR("expected a number");
		}
	}
	size_t nPressurePoints = atoi(tok);

	tok = RetrieveStr(line);
	if (!IsNumber(tok)) MY_ERROR("expected a number");
	size_t nTemperaturePoints = atoi(tok);

	if (line) {
		tok = RetrieveStr(line);
		if (!IsNumber(tok)) MY_ERROR("expected a number");

		double whatIsThat = Atof(tok);
	}

	if (line) MY_ERROR("unexpected token");

	if (noneq) {
		PushNumbers(&table.P, nPressurePoints, line, file);
		PushNumbers(&table.T, nTemperaturePoints, line, file);
	}
	else {
		line = file.GetLine().Trim();
		if (!IsNumber(tok = RetrieveStr(line))) MY_ERROR("expected a number");
		double delta_P = Atof(tok);
		if (!IsNumber(tok = RetrieveStr(line))) MY_ERROR("expected a number");
		double delta_T = Atof(tok);
		if (line) {
			//if (multiple_tables) goto 
			MY_ERROR("unexpected token");
		}
		line = file.GetLine().Trim();
		if (!IsNumber(tok = RetrieveStr(line))) MY_ERROR("expected a number");
		double start_P = Atof(tok);
		if (!IsNumber(tok = RetrieveStr(line))) MY_ERROR("expected a number");
		double start_T = Atof(tok);
		if (line) MY_ERROR("unexpected token");

		table.P.Redim(0);
		table.T.Redim(0);

		for (size_t i = 0; i < nPressurePoints; ++i)    table.P.Push(start_P + i*delta_P);
		for (size_t i = 0; i < nTemperaturePoints; ++i) table.T.Push(start_T + i*delta_T);
	}
	PushNumbers(nullptr, nTemperaturePoints, line, file);
	PushNumbers(nullptr, nTemperaturePoints, line, file);
	 
	table.P_unit = "Pa";
	table.T_unit = "°C";

	const aVect<PVTVarNames> varNames = { 
		PVTVarNames("GAS DENSITY",						"ROG"),
		PVTVarNames("LIQUID DENSITY",					"ROHL"),
		PVTVarNames("WATER DENSITY",					"ROWT"),
		PVTVarNames("PRES. DERIV. OF GAS DENS.",		"DROGDP"),
		PVTVarNames("PRES. DERIV. OF LIQUID DENS.",		"DROHLDP"),
		PVTVarNames("PRES. DERIV. OF WATER DENS.",		"DROWTDT"),
		PVTVarNames("TEMP. DERIV. OF GAS DENS.",		"DROGDT"),
		PVTVarNames("TEMP. DERIV. OF LIQUID DENS.",		"DROHLDT"),
		PVTVarNames("TEMP. DERIV. OF WATER DENS.",		"DROWTDT"),
		PVTVarNames("GAS MASS FRACTION OF GAS + OIL",	"RS"),
		PVTVarNames("WATER MASS FRACTION OF GAS",		"RSW"),
		PVTVarNames("GAS VISCOSITY",					"VISG"),
		PVTVarNames("LIQ. VISCOSITY",					"VISHL"),
		PVTVarNames("WAT. VISCOSITY",					"VISWT"),
		PVTVarNames("GAS SPECIFIC HEAT",				"CPG"),
		PVTVarNames("LIQ. SPECIFIC HEAT",				"CPHL"),
		PVTVarNames("WAT. SPECIFIC HEAT",				"CPWT"),
		PVTVarNames("GAS ENTHALPY",						"HG"),
		PVTVarNames("LIQ. ENTHALPY",					"HHL"),
		PVTVarNames("WAT. ENTHALPY",					"HWT"),
		PVTVarNames("GAS THERMAL COND.",				"TCG"),
		PVTVarNames("LIQ. THERMAL COND.",				"TCHL"),
		PVTVarNames("WAT. THERMAL COND.",				"TCWT"),
		PVTVarNames("SURFACE TENSION GAS/OIL",			"SIGGHL"),
		PVTVarNames("SURFACE TENSION GAS/WATER",		"SIGGWT"),
		PVTVarNames("SURFACE TENSION WATER/OIL",		"SIGGWT"),
		PVTVarNames("GAS ENTROPY",						"SEG"),
		PVTVarNames("LIQUID ENTROPY",					"SEHL"),
		PVTVarNames("WATER ENTROPY",					"SEWT")/*,
		PVTVarNames("DRHOG/DP",							"DROGDP"),
		PVTVarNames("DRHOL/DP",							"DROHLDP"),
		PVTVarNames("DRHOG/DT",							"DROGDT"),
		PVTVarNames("DRHOL/DT",							"DROHLDT"),
		PVTVarNames("GAS MASS FRACTION",				"RS"),*/
	};


	int i = 0;
	//for (; line = file.GetLine().Trim();) {
	for (size_t i = 0; !file.Eof(); ++i) {

		//bool found = false;

		//aVect_static_for(varNames, i) {
			//auto&& varName = varNames[i];
			//if (ReadVariableTable(line, table, nPressurePoints,
			//	nTemperaturePoints, file, varName.newFormat, varName.oldFormat)) 
			//{
			//	found = true;
			//	break;
			//}
		//}

		//if (!found) MY_ERROR(xFormat("unkown entry : %s", line));
		if (line && i >= varNames.Count()) MY_ERROR("Unexpected data");

		int dec = 0;

		if (!water_opt && (i == 10 || i == 24 || i == 25)) continue;

		if (i >= 11) dec = 11;

		if (!water_opt && (i - dec + 1 ) % 3 == 0) continue;

		line = file.GetLine().Trim();
		if (line) {
			ReadVariableTable(line, table, nPressurePoints, nTemperaturePoints, file, varNames[i].oldFormat);
		}
	}

	table.UpdatePrecomputedFactor();

	table.old_format = true;

	return std::move(table);
}

Olga_PVT_Table Olga_Read_PVT_Table_Format1(const wchar_t * fileName) {

	Olga_PVT_Table table;

	WinFile file(fileName, "rb");

	if (!file.Open()) {
		RECOVERABLE_ERROR(xFormat("Unable to open file:\n\"%S\"", fileName), ERROR_FILE_NOT_FOUND);
		return Olga_PVT_Table();
	}

	CharPointer line = file.GetLine<'\\', true, '!', true>();

	if (!line) {
		RECOVERABLE_ERROR(xFormat("\"%s\"\n\n file is empty", file.GetFilePath()), ERROR_INVALID_DATA);
		return Olga_PVT_Table();
	}

	if (!ReadKeyword(file, line, "PVTTABLE", false)) {
		//RECOVERABLE_ERROR("Keyword not found: PVTTABLE", ERROR_INVALID_DATA);
		return Olga_PVT_Table();
	}

	if (!ReadKeyword(file, line, "LABEL", true)) MY_ERROR("Keyword not found: LABEL");
	if (!ReadKeyword(file, line, "=", true)) MY_ERROR("Missing '=' character");

	CharPointer tok;

	tok = RetrieveStr<'\"', ','>(line);
	if (!tok) return PVT_ReadError(file, "unexpected end of line while searching for table name");

	table.name.Copy(tok.Trim());
	table.fileName = fileName;

	CachedVect<aVect<char> > list;

	for (;;) {

		tok = RetrieveStr<' ', ',', '='>(line);

		if (!tok) break;

		if      (0 == strcmp_caseInsensitive(tok, "STDPRESSURE"))    table.stdPressure     = GetValueInUnit(line, "bar");
		else if (0 == strcmp_caseInsensitive(tok, "STDTEMPERATURE")) table.stdTemperature  = GetValueInUnit(line, "°C");
		else if (0 == strcmp_caseInsensitive(tok, "GOR"))            table.GOR			   = GetValueInUnit(line, "sm3/sm3");
		else if (0 == strcmp_caseInsensitive(tok, "GLR"))            table.GLR			   = GetValueInUnit(line, "sm3/sm3");
		else if (0 == strcmp_caseInsensitive(tok, "WC"))             table.WC			   = GetValueInUnit(line, nullptr);
		else if (0 == strcmp_caseInsensitive(tok, "STDOILDENSITY"))	 table.stdOilDensity   = GetValueInUnit(line, "kg/m3");
		else if (0 == strcmp_caseInsensitive(tok, "STDGASDENSITY"))	 table.stdGasDensity   = GetValueInUnit(line, "kg/m3");
		else if (0 == strcmp_caseInsensitive(tok, "STDWATDENSITY"))	 table.stdWaterDensity = GetValueInUnit(line, "kg/m3");
		else if (0 == strcmp_caseInsensitive(tok, "PRESSURE"))	
		{
			if (!Read_PVT_List(list, file, line, nullptr, true))  MY_ERROR("PVT file read error"); 
			table.P.Redim(list.Count());
			aVect_static_for(list, i) table.P[i] = Atof(list[i]);

			tok = RetrieveStr<','>(line).Trim();
			if (!tok) return PVT_ReadError(file, "unexpected end of line while searching for pressure unit");
			if (strcmp(tok, "PA") == 0) tok = "Pa";
			table.P_unit.Copy(tok);
		}
		else if (0 == strcmp_caseInsensitive(tok, "TEMPERATURE"))
		{
			if (!Read_PVT_List(list, file, line, nullptr, true)) MY_ERROR("PVT file read error");
			table.T.Redim(list.Count());
			aVect_static_for(list, i) table.T[i] = Atof(list[i]);

			tok = RetrieveStr<','>(line).Trim();
			if (!tok) return PVT_ReadError(file, "unexpected end of line while searching for temperature unit");
			table.T_unit.Copy(tok).Prepend("°");
		}
		else  if (0 == strcmp_caseInsensitive(tok, "COLUMNS")) {
			if (!Read_PVT_List(list, file, line, nullptr, true))  MY_ERROR("PVT file read error");

			aVect_static_for(list, i) {
				table.variables.Grow(1);
				table.variables[i].name.Copy(list[i]);
				table.variables[i].value.Redim(table.P.Count(), table.T.Count());
				table.variables[i].value.Set(std::numeric_limits<double>::infinity());
			}
		}
	}

	if (!table.P || !table.T || !table.variables) return Olga_PVT_Table();

	aVect<aVect<char> > keywords;

	keywords.Push("PVTTABLE").Push("POINT");

	while (true) {
		
		if (!Read_PVT_List(list, file, line, keywords)) break;

		double P = Atof(list[0]);
		double T = Atof(list[1]);

		size_t iP, iT;
		bool found = false;

		aVect_static_for(table.P, i) {
			if (IsEqualWithPrec(P, table.P[i], 1e-10)) {
				iP = i;
				found = true;
				break;
			}
		}
		if (!found) {
			RECOVERABLE_ERROR("Invalid pressure point", ERROR_INVALID_DATA);
			return Olga_PVT_Table();
		}

		found = false;
		
		aVect_static_for(table.T, i) {
			if ((T == 0 && table.T[i] == 0) || IsEqualWithPrec(T, table.T[i], 1e-10)) {
				iT = i;
				found = true;
				break;
			}
		}
		if (!found) {
			RECOVERABLE_ERROR("Invalid temperature point", ERROR_INVALID_DATA);
			return Olga_PVT_Table();
		}

		aVect_static_for(table.variables, i) {
			table.variables[i](iP, iT) = Atof(list[i]);
		}
	}

	aVect_static_for(table.variables, i) {
		aMat_static_for(table.variables[i].value, k, l) {
			if (!_finite(table.variables[i](k, l))) {
				RECOVERABLE_ERROR("missing (P,T) point", ERROR_INVALID_DATA);
				return Olga_PVT_Table();
			}
		}
	}

	table.UpdatePrecomputedFactor();

	table.old_format = false;

	return std::move(table);
}

Olga_PVT_Table Olga_Read_PVT_Table(const wchar_t * fileName) {

	Olga_PVT_Table retVal = Olga_Read_PVT_Table_Format1(fileName);

	if (retVal.variables) return std::move(retVal);

	retVal = Olga_Read_PVT_Table_Format2(fileName);

	if (retVal.variables) return std::move(retVal);

	MY_ERROR("Unable to read PVT table");

	return Olga_PVT_Table();
}

Olga_PVT_Table Olga_Read_PVT_Table(const char * fileName) {

	return std::move(Olga_Read_PVT_Table(xFormat(L"%S", fileName)));
}

void Olga_Write_PVT_Table(
	const wchar_t * destFilePath,
	const Olga_PVT_Table & table) 
{
	if (table.old_format) MY_ERROR("Writing old format PVT tables is not implemented yet");
	
	wchar_t szTempFileName[MAX_PATH];

	wchar_t * path;

	SplitPathW(destFilePath, nullptr, &path, nullptr, nullptr);

	SAFE_CALL(GetTempFileNameW(path, L"tmp_", 0, szTempFileName));

	CopyFileW(aVect<wchar_t>(table.fileName), szTempFileName, false);

	WinFile sourceFile(szTempFileName, "rb");
	WinFile destFile(destFilePath, "w");

	if (!sourceFile.Open()) MY_ERROR(xFormat("Unable to open %S", table.fileName));
	if (!destFile.Open())   MY_ERROR(xFormat("Unable to open %S", destFilePath));

	for (;;) {
		auto line = sourceFile.GetLine();
		if (strstr(line, "PVTTABLE POINT")) break;
		destFile.Write(line);
		destFile.Write("\n");
	}

	aVect<char> buffer;

	size_t i = -1;
	for (auto&& P : table.P) {
		i++;
		size_t j = -1;
		for (auto&& T : table.T) {
			j++;
			buffer.Append("PVTTABLE POINT = (%.7e, %.7e, ", P, T);

			for (size_t k = 2; k < table.variables.Count(); k++) {
				buffer.Append("%.7e, ", table.variables[k].value(i, j));
			}
			buffer.Last(-2) = ')';
			buffer.Last(-1) = '\n';
			buffer.Last() = 0;
		}
	}

	destFile.Write(buffer);
	sourceFile.Close();

	DeleteFileW(szTempFileName);
}


