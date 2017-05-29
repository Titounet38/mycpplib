
#define _CRT_SECURE_NO_WARNINGS

//#define _

#include "MyUtils.h"
#include "WinUtils.h"
#include "Params.h"
#include "direct.h"
#include "Shellapi.h"
#include <limits>


char * GetParamBlockCore(const char * line, ConstCharPointer & balise) {

	if (!line) return nullptr;

	for (;;) {
		char * start = strstr((char*)line, balise);
		if (!start) return nullptr;

		char * ptr = start;

		bool found = false;
		for (;;) {
			if (ptr <= line) break;
			if (isspace(ptr[-1])) ptr--;
			else if (ptr[-1] == '{') {
				found = true;
				break;
			}
			else break;
		}

		if (!found) {
			line = start + 1;
			continue;
		}

		ptr = start + strlen(balise);
		const char * lineEnd = line + strlen(line);

		while (ptr < lineEnd) {
			if (isspace(ptr[0])) ptr++;
			else if (ptr[0] == ':') return start - 1;
			else break;
		}

		start++;
	}
	return nullptr;
}

char * GetParamBlock(const char * line) {

	static ConstCharPointer paramBalise("parameter"); // really is "{parameter:" (or "{ parameter:") or "{parameter :" or ...)

	return GetParamBlockCore(line, paramBalise);
}

char * GetCommandBlock(const char * line) {

	static ConstCharPointer paramBalise("command"); // really is "{parameter:" (or "{ parameter:") or "{parameter :" or ...)

	return GetParamBlockCore(line, paramBalise);
}

bool GetValue(double & value, const char * str, mVect<Param> & params, Param * alreadyFoundParam = nullptr) {

	if (IsNumber(str))   value = atof(str);
	else if (IsInf(str)) value = std::numeric_limits<double>::infinity();
	else {
		Param * p = alreadyFoundParam ? alreadyFoundParam : FindParam(params, str, true);
		if (!p || (p && !IsNumber(p->value))) return false;//MY_ERROR("can't find a value");
		value = atof(p->value);
	}

	return true;
}

void AddToParamReferenceIfNotAlreadyInThere(
	aVect<aVect<char> > & paramReferences,
	const char * paramName) {

	bool found = false;
	aVect_static_for(paramReferences, i) {
		if (strcmp_caseInsensitive(paramReferences[i], paramName) == 0) {
			found = true;
			break;
		}
	}

	if (!found) paramReferences.Push(paramName);
}

bool IsValidParamName(const char * str) {
	
	if (!str || !str[0]) return false;
	
	if (!isalpha(str[0])) return false;

	size_t len = strlen(str);
	for (size_t i=0; i<len; ++i) {
		if (!isalnum(str[0])) return false;
	}

	return true;
}

bool IsFormula(const char * str) {
	
	if (!str || !str[0]) return false;
	
	size_t len = strlen(str);
	for (size_t i=0; i<len; ++i) {
		if (str[i] == '}') {
			for (size_t j=i-1; j<len; --j) {
				if (str[j] == '$' && str[j+1] == '{') {
					return true;
				}
			}
		}
	}
	return false;
}

//if paramReferences, only accumulate references and copy token into value
bool TranslateValue(
	aVect<char> & value, 
	const char * token, 
	mVect<Param> & params,
	aVect<aVect<char> > * pParamReferences = nullptr) {

	if (!token || token[0] == 0) return true;

	size_t I = strlen(token);
	aVect<bool> parsed(I);
	parsed.Set(false);

	static WinCriticalSection cs;
	ScopeCriticalSection<WinCriticalSection> guard(cs);

	static aVect<char> buffer;

	for (size_t i=0; i<I; ++i) {
		if (!parsed[i] && token[i] == '}') {
			parsed[i] = true;
			for (size_t j=i; j<I; --j) {
				if (!parsed[j] && token[j] == '{') {
					parsed[j] = true;
					if (j>0 && !parsed[j-1] && token[j-1] == '$') {
						parsed[j-1] = true;

						buffer.Redim(i-j);
						bool * parsedPtr = &parsed[j+1];
						aVect_static_for(buffer , k) {
							buffer[k] = token[j+k+1];
						}

						buffer.Last() = 0;

						char op[2] = {0, 0};
						size_t curOp = 0;
						aVect<char> result;
						size_t opPos[2];
						size_t nOperandToEval = 1;

						aVect_static_for(buffer , k) {
							
							if (!parsedPtr[k]) {
								switch (buffer[k]) {
									case ':' : {
										if (curOp > 0) {
											if (op[0] == '?') nOperandToEval = 1;
											else MY_ERROR(aVect<char>("too many operation : ${%s}", (char*)buffer));
										}
										else nOperandToEval = 2;
										break;
									}
									case '-':
									case '+':
										//check this '-' is not in the exposant (as in '1e-7')
										if (k > 1 && buffer[k - 1] == 'e') {
											auto c = buffer[k-1];
											buffer[k-1] = 0;
											const char * ptr;
											for (ptr = &buffer[k - 2]; ptr > buffer; ptr--) {
												if (IsSpace(*ptr) || (!IsDigit(*ptr) && *ptr != '.')) break;
											}

											bool isNumber = IsNumber(ptr);

											buffer[k-1] = c;

											if (isNumber) continue;
										}
									case '?' : 
									case '*' :
									case '/' :
									case '>' :
									case '<' :
									case '=' :
									case '^' : {
										nOperandToEval = 2;
										if (curOp > 0) MY_ERROR(aVect<char>("too many operation : ${%s}", (char*)buffer));
										break;
									}
									default: continue;
								}

								op[curOp] = buffer[k];
								parsedPtr[k] = true;
								opPos[curOp] = k;
								if (++curOp > 2) MY_ERROR(aVect<char>("too many operation : ${%s}", (char*)buffer));
							}
						}

						CharPointer operand1(buffer);
						CharPointer operand2, operand3;

						if (op[0]) {
							buffer[opPos[0]] = 0;
							operand2 = (char*)buffer + opPos[0] + 1;
						}
						else {
							printf("");
						}
						if (op[1]) {
							buffer[opPos[1]] = 0;
							operand3 = (char*)buffer + opPos[1] + 1;
						}

						operand1.Trim();
						operand2.Trim();
						operand3.Trim();

						double value1, value2;

						//switch(op) {
						//	case '+': operand1 = RetrieveStr<'+'>(operand2); break;
						//	case '-': operand1 = RetrieveStr<'-'>(operand2); break;
						//	case '*': operand1 = RetrieveStr<'*'>(operand2); break;
						//	case '/': operand1 = RetrieveStr<'/'>(operand2); break;
						//	case '^': operand1 = RetrieveStr<'^'>(operand2); break;
						//	case ':': operand1 = RetrieveStr<':'>(operand2); break;
						//}

						if (!operand1) MY_ERROR("usage : at least 1 operands per ${}");
								
						if (pParamReferences) {
							if (operand1 && !IsNumber(operand1) && !IsFormula(operand1) && IsValidParamName(operand1)) AddToParamReferenceIfNotAlreadyInThere(*pParamReferences, operand1);
							if (operand2 && !IsNumber(operand2) && !IsFormula(operand2) && IsValidParamName(operand2)) AddToParamReferenceIfNotAlreadyInThere(*pParamReferences, operand2);
							if (operand3 && !IsNumber(operand3) && !IsFormula(operand3) && IsValidParamName(operand3)) AddToParamReferenceIfNotAlreadyInThere(*pParamReferences, operand3);
						}
						else {

							Param * p = FindParam(params, operand1, true);

							bool result_GetValue1 = GetValue(value1, operand1, params, p);

							if (nOperandToEval == 1 && op[0] == 0 && p && !result_GetValue1) {
								//ok, but must substitute string instead of a double prec value
								result.Copy(p->value);
							}
							else {
								if (nOperandToEval > 0 && !result_GetValue1) goto abortTranslation;
								if (nOperandToEval > 1 && !GetValue(value2, operand2, params)) goto abortTranslation;
							}

							switch(op[0]) {
								case '>': result = NiceFloat(value1 > value2); break;
								case '<': result = NiceFloat(value1 < value2); break;
								case '=': {
									result = value1 == value2 ? "1" : "0"; 
									break;
								}
								case '+': result = NiceFloat(value1 + value2); break;
								case '-': result = NiceFloat(value1 - value2); break;
								case '*': result = NiceFloat(value1 * value2); break;
								case '/': {
									//if (value2 == 0) MY_ERROR(aVect<char>("division by 0 while computing %s", token));
									result = NiceFloat(value1 / value2);break;
								}
								case '^': {
									//if (value2 != (int)value2 && value1 < 0) MY_ERROR(aVect<char>("non-integer power of negative number while computing %s", token));
									result = NiceFloat(pow(value1, value2));break;
								}
								case ':': {
									if (value1 < 0 || value1 != (int)value1) MY_ERROR("operator : requires a positive integer");
									int l = (int)value1;
									if (l>0) {
										aVect<char> niceValue2 = NiceFloat(value2);
										result.Redim(1)[0] = 0;
										while(true) {
											//result.Append("%s", (char*)niceValue2); //  too slow for l>50000
											result.Grow(niceValue2.Count() - 1).Last() = 0;
											char * pStart = &result.Last() - niceValue2.Count() + 1;
											aVect_static_for(niceValue2, k) {
												pStart[k] = niceValue2[k];
											}
											if (--l <= 0) break;
											//result.Append(", "); //  too slow for l>50000
											result.Grow(2).Last() = 0;
											pStart = &result.Last() - 3 + 1;
											pStart[0] = ',';
											pStart[1] = ' ';
										}
									}
									break;
								}
								case '?': {
									if (value1) {
										result.Copy(operand2);
									} else {
										if (op[1] != ':') {
											return false;
										}
										result.Copy(operand3);
									}
									break;
								}
								case 0: {
									if (result_GetValue1) result = NiceFloat(value1);
									break;
								}
								default:MY_ERROR("Invalid operand");
							}
						
							size_t resLen = result ? strlen(result) : 0;
							aVect<char> translatedString(j-1 + resLen+1 + (I-i)-1);
							translatedString.Last() = 0;
							for (size_t k=0; k<j-1; ++k) {
								translatedString[k] = token[k];
							}
							for (size_t k=0; k<resLen; ++k) {
								translatedString[k + j - 1] = result[k];
							}
							for (size_t k=0; k<I-i-1; ++k) {
								translatedString[k + j + resLen-1] = token[i+k+1];
							}

							if (IsFormula(translatedString)) {
								if (!TranslateValue(value, translatedString, params)) {
									return false;
								}
							}
							else value.Steal(translatedString);

							return true;
						}
					}
					break;
				}
			}
		}
	}

abortTranslation:
	value.Copy(token);
	return true;
}

mVect<Param> GetParams(
	mVect< mVect<wchar_t> > & fullPathArray,
	bool stopAtFirst, 
	aVect<char> & specialComments, 
	aVect<aVect<char> > * pParamReferences) {

	mVect<Param> parameters;
	CharPointer line, token, token2;

	static WinCriticalSection cs;
	ScopeCriticalSection<WinCriticalSection> guard(cs);

    aVect_static_for(fullPathArray, i) {

        WinFile f((wchar_t*)fullPathArray[i], "rb");

		if (!f.Open()) MY_ERROR("Fichier introuvable");

        while (line = f.GetLine()) {
			token2 = RetrieveStr<'#'>(line, true).Trim();
			bool hasSpecialComment = token2 && line && specialComments ? strstr(line, specialComments)!=nullptr : false;
            if ((token = RetrieveStr<'='>(token2)) || hasSpecialComment) {
				if (token.Trim() && !strchr(token, ' ') && !strchr(token, '\t') || hasSpecialComment) {
					if (token2.Trim() || hasSpecialComment) {

						bool found = false;
						aVect_static_for(parameters, j)
							if (strcmp_caseInsensitive(parameters[j].name, token) == 0) {
								found = true; break;
							}

						static aVect<char> value;
						TranslateValue(value, token2, parameters, pParamReferences);

						////DBG
						//if (StrEqual(token, "FlowlineGeometry")) {
						//	if (value[0] != '{') __debugbreak();
						//}
						////!DBG

						if (!found) {
							Param P;
							P.name.Copy(token);
							P.value.Copy(value);
							P.comment.Copy(line.Trim());
							P.fromFile.Reference(fullPathArray[i]);

							parameters.Push(std::move(P));
						}
					}
				}
            }
        }
    }

    return std::move(parameters);
}


void MergeParams(mVect<Param> & dominant, mVect<Param> & recessif) {
	
	mVect_static_for(recessif, i) {
		bool found = false;
		mVect_static_for(dominant, j) {
			if (strcmp_caseInsensitive(recessif[i].name, dominant[j].name) == 0) {
				found = true;
				break;
			}
		}
		if (!found) {
			//Param * old_ptr = dominant.GetDataPtr();
			dominant.Push(recessif[i]);
			//Param * new_ptr = dominant.GetDataPtr();
			//mVect_static_for(dominant, j) {
			//	if (new_ptr != old_ptr) dominant[j].FixRef(new_ptr+j, old_ptr+j);
			//}
		}
	}
}

//void Param::FixRef(Param * new_ptr, Param * old_ptr) {
//
//	aVect< mVect<char> > hack1;
//
//	hack1.FixRefInArray(&name,     1, &old_ptr->name,     1);
//	hack1.FixRefInArray(&value,    1, &old_ptr->value,    1);
//	hack1.FixRefInArray(&comment,  1, &old_ptr->comment,  1);
//	hack1.FixRefInArray(&fromFile, 1, &old_ptr->fromFile, 1);
//}

void Param::Reference(Param & p) {
	name.Reference(p.name);
	value.Reference(p.value);
	comment.Reference(p.comment);
	fromFile.Reference(p.fromFile);
}

ParamNode::ParamNode() {
	parent = nullptr;
	flag = false;
}

//void ParamNode::FixRef(ParamNode * new_ptr, ParamNode * old_ptr) {
//
//	aVect< mVect<Param> > hack1;
//	aVect< mVect<ParamNode> > hack2;
//	aVect< mVect<char> > hack3;
//	mVect_static_for(children, i) {
//		hack1.FixRefInArray(&children[i].params,   1,   &(old_ptr+i)->params,   1);
//		hack2.FixRefInArray(&children[i].children, 1,   &(old_ptr+i)->children, 1);
//		hack3.FixRefInArray(&children[i].folder,   1,   &(old_ptr+i)->folder,   1);
//		hack3.FixRefInArray(&children[i].name,     1,   &(old_ptr+i)->name,     1);
//		mVect_static_for(children[i].children, j) {
//			if (children[i].children[j].parent == old_ptr+i) {
//				children[i].children[j].parent = new_ptr+i;
//			}
//		}
//	}
//}

void ParamNode::AddChildren(mVect<Param> & p) {

	children.Redim(children.Count()+1);

	children.Last().params.Reference(p);
	children.Last().parent = this;
}

void ParamNode::RemoveChild(size_t i) {
		
	children.Remove(i);
	for (size_t j=i, J=children.Count(); j<J; ++j) {
		mVect_static_for(children[j].children, k) {
			if (children[j].children[k].parent != &children[j]) {
				children[j].children[k].parent = &children[j];
			}
		}
	}
}

void ParamNode::Free() {
ParamNode::~ParamNode();
}

void ParamNode::Print(char * prefix) {

	static WinCriticalSection cs;
	ScopeCriticalSection<WinCriticalSection> guard(cs);

	static size_t recLvl;
	aVect<char> nextPrefix, blanks;

	size_t len = strlen(prefix);
	if (len >= 4) {
		for (size_t i = 0; i < len - 4; ++i) putchar(prefix[i]);
		printf("+---");
	}

	CONSOLE_SCREEN_BUFFER_INFO csbi;
	SAFE_CALL(GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi));
	int consoleWidth = csbi.dwMaximumWindowSize.X;

	xPrintf(L"%s\n", name);

	size_t nAdditionalBlanks = 8;

	mVect_static_for(params, i) {
		size_t nDec = printf("%s# %s = %s", prefix, (char*)params[i].name, (char*)params[i].value);
		//size_t substract = strlen(params[i].name) + strlen(params[i].value) + strlen(prefix);

		nDec += nAdditionalBlanks;

		int spaceLeft = consoleWidth - nDec - 2;
		int fileNameSize = GraphemCount(params[i].fromFile);
		int nBlanks = spaceLeft - (fileNameSize);
		int fileNameDec = nBlanks > 0 ? 0 : -nBlanks + 3;
		fileNameDec = Min(fileNameDec, Max(1, fileNameSize - 3));

		blanks.Redim((nBlanks > 0 ? nBlanks : 0) + nAdditionalBlanks).Set(' ').Last() = 0;

		wprintf(L"%S %S%s\n", (char*)blanks, fileNameDec ? "..." : "", (wchar_t*)params[i].fromFile + fileNameDec);
		int a = 0;
	}
	recLvl++;

	mVect_static_for(children, i) {
		if (i == children.Count() - 1) nextPrefix.sprintf("%s    ", prefix);
		else nextPrefix.sprintf("%s|   ", prefix);
		children[i].Print(nextPrefix);
	}

	recLvl--;
}

void ParamNode::CheckTreeCore() {

	mVect_static_for(children, i) {
		children[i].CheckTreeCore();
		if (children[i].parent != this) {
			MY_ERROR(aVect<char>("Child of Node (%S) : Node (%S) child (%S) parent (%S) != Node (%S)",
				(wchar_t*)parent->name,
				(wchar_t*)name, (wchar_t*)children[i].name,
				(wchar_t*)children[i].parent->name, (wchar_t*)name));
		}
	}
}

void ParamNode::CheckTree() {

	ParamNode * ptr = this;
	while (ptr->parent) ptr = ptr->parent;

	ptr->CheckTreeCore();

}

void ParamNode::ComputeFormulas(bool thisNodeAndNoChildrenRecurse) {

	if (!thisNodeAndNoChildrenRecurse && this->children) {
		mVect_static_for(this->children, i) {
			this->children[i].ComputeFormulas();
		}
	}
	else {
		size_t oldNremainingFormulas = -1;
		while (true) {
			size_t nRemainingFormulas = 0;
			aVect<char> value;
			mVect_static_for(this->params, i) {
				if (IsFormula(this->params[i].value)) {
					if (TranslateValue(value, this->params[i].value, this->params)) {
						this->params[i].value.Steal(value);
						if (IsFormula(this->params[i].value)) {
							TranslateValue(value, this->params[i].value, this->params);
							nRemainingFormulas++;
						}
					}
					else {
						auto prevCount = this->params.Count();
						auto removedParam = this->params.Pop(i);
						this->Inherit();
						//MY_ASSERT(prevCount == this->params.Count());
						if (prevCount != this->params.Count()) {
							MY_ERROR(xFormat(
								"Parameter not found in parent nodes : %s\n"
								"current node: %S",
								removedParam.name, this->folder));
						}
						auto tmp = this->params.Pop();
						this->params.Insert(i, std::move(tmp));
						this->parent->ComputeFormulas(true);
						this->ComputeFormulas(true);
						MY_ASSERT(!IsFormula(this->params[i].value));
						int a = 0;
					}
				}
			}

			if (nRemainingFormulas == 0) break;

			if (nRemainingFormulas && 
				oldNremainingFormulas != -1 && 
				nRemainingFormulas >= oldNremainingFormulas) 
			{
				aVect<aVect<char> > missingParams;
				mVect_static_for(this->params, i) {
					if (IsFormula(this->params[i].value)) {
						TranslateValue(value, this->params[i].value, this->params, &missingParams);
					}
				}
				aVect<char> buffer;
				aVect_static_for(missingParams, i) {
					buffer.Append("- %s\n", (char*)missingParams[i]);
				}
				MY_ERROR(aVect<char>("Some parameters are missing:\n%s\n", (char*)buffer));
			}
			
			oldNremainingFormulas = nRemainingFormulas;
		}
	}
}

void ParamNode_KeepOnlyOneNode(ParamNode * discard_all_other_cases_node) {

	auto parent = discard_all_other_cases_node->parent;

	if (!parent) return;

	mVect_for_inv(parent->children, i) {
		auto&& c = parent->children[i];
		if (&c == discard_all_other_cases_node) {
			/* ok */
		} else {
			parent->RemoveChild(i);
		}
	}
		
	ParamNode_KeepOnlyOneNode(parent);

}

void ParamNode_DiscardNodes(
	ParamNode * node, mVect<mVect<wchar_t>> & fullPathArray, 
	MagicPointer<ParamNode> & discard_all_other_cases_node,
	int recursionLevel, int pass) {

	if (pass == 0) {
		mVect_for_inv(node->children, i) {

			auto&& c = node->children[i];

			if (auto p = FindParam(c.params, "__DISCARD_ALL_OTHER_CASES__", true)) {
				if (IsNumber(p->value) && atof(p->value) == 1) {
					if (discard_all_other_cases_node) MY_ERROR("multiple __DISCARD_ALL_OTHER_CASES__ = 1 cases");
					discard_all_other_cases_node = &c;
				}
			}

			ParamNode_DiscardNodes(&c, fullPathArray, discard_all_other_cases_node, recursionLevel + 1, 0);
		}

		if (recursionLevel == 0 && discard_all_other_cases_node) {
			ParamNode_KeepOnlyOneNode(discard_all_other_cases_node);
			mVect_for_inv(fullPathArray, j) {
				if (!wcsstr(fullPathArray[j].GetDataPtr(), discard_all_other_cases_node->folder.GetDataPtr())) {
					fullPathArray.Remove(j);
				}
			}
		}
	} else {
		if (node->children) {
			mVect_for_inv(node->children, i) {

				auto&& c = node->children[i];

				if (auto p = FindParam(c.params, "__DISCARD_CASE__", true)) {
					if (IsNumber(p->value) && atof(p->value) == 1) {
						mVect_for_inv(fullPathArray, j) {
							if (wcsstr(fullPathArray[j].GetDataPtr(), c.folder.GetDataPtr())) {
								fullPathArray.Remove(j);
							}
						}
						node->RemoveChild(i);
						continue;
					}
				}

				ParamNode_DiscardNodes(&c, fullPathArray, discard_all_other_cases_node, recursionLevel + 1, 1);
			}
			if (!node->children) { // has become a leaf, remove
				mVect_for_inv(node->parent->children, i) {
					if (&node->parent->children[i] == node) {
						node->parent->RemoveChild(i);
						node = nullptr;//just to be safe, as node has just been deleted
						break;
					}
				}
			}
		}
	}
}

void ParamNode::DiscardNodes(mVect<mVect<wchar_t>> & fullPathArray) {

	MagicPointer<ParamNode> discard_all_other_cases_node = nullptr;

	ParamNode_DiscardNodes(this, fullPathArray, discard_all_other_cases_node, 0, 0);
	ParamNode_DiscardNodes(this, fullPathArray, discard_all_other_cases_node, 0, 1);
}

void ParamNode::Compact() {

	mVect_for_inv(children, i) {
		
		children[i].Compact();

		mVect_for_inv(children, j) {
			if (j<=i) break;
			bool sameParams = true;
			bool found = false;

			if (children[i].params.Count() == 0) {
				if (children[j].params.Count() != 0) {
					sameParams = false;
				}
				goto dbl_break2;
			}

			mVect_static_for(children[i].params, k) {
				found = false;
				mVect_static_for(children[j].params, l) {
					if (strcmp_caseInsensitive(children[i].params[k].name, children[j].params[l].name) == 0) {
						found = true;
						if (strcmp_caseInsensitive(children[i].params[k].value, children[j].params[l].value) != 0) {
							sameParams = false;
							goto dbl_break2;
						}
					}
				}
				if (!found) {
					sameParams = false;
					goto dbl_break2;
				}
			}

			if (found) {
				mVect_static_for(children[j].params, l) {
					bool found = false;
					mVect_static_for(children[i].params, k) {
						if (strcmp_caseInsensitive(children[i].params[k].name, children[j].params[l].name) == 0) {
							found = true;
						}
					}
					if (!found) {
						sameParams = false;
						goto dbl_break2;
					}
				}
			}


dbl_break2:
			if (sameParams) {
				mVect_for(children[j].children, k) {
					children[i].AddChildren(children[j].children[k].params);
					children[i].children.Last().folder.Reference(children[j].children[k].folder);
					children[i].children.Last().name.Reference(children[j].children[k].name);
					children[i].children.Last().children.Steal(children[j].children[k].children);
					mVect_static_for(children[i].children.Last().children, l) {
						children[i].children.Last().children[l].parent = &children[i].children.Last();
					}
				}
				if (strcmp_caseInsensitive(children[i].name, children[j].name) != 0) {
					children[i].name.sprintf(L"%s_%s", (wchar_t*)children[i].name, (wchar_t*)children[j].name);
				}

				RemoveChild(j);
				CheckTree();
				children[i].Compact();
			}
		}
	}
}


void ParamNode::UnaliasNodeNames() {

	mVect_static_for(children, i) {
		children[i].UnaliasNodeNames();
	}

	mVect_static_for(children, i) {
		while(true) {
			bool found = false;
			mVect_static_for(children, j) {
				if (j>=i) break;
				if (strcmp_caseInsensitive(children[i].name, children[j].name) == 0) {
					children[i].name.sprintf(L"%s_bis", (wchar_t*)children[i].name);
					found = true;
					break;
				}
			}
			if (!found) break;
		}
	}
}
	
void ParamNode::Inherit() {

	mVect_static_for(children, i) {
		children[i].Inherit();
	}

	ParamNode * ptr = this;
	while (ptr->parent) {
		ptr = ptr->parent;
		MergeParams(params, ptr->params);
	}
}

mVect<Param> GetInheritedParams(
	const aVect<wchar_t> & path, 
	bool stopAtFirst, 
	aVect<char> & specialComments,
	aVect<aVect<char> > * pParamReferences) {

	/*static */mVect< mVect<wchar_t> > fullPathArray;

	if (GetFileAttributesW(path) & FILE_ATTRIBUTE_DIRECTORY) {
		aVect<wchar_t> paramFileName("Params.txt"), autoParamFileName("AutoParams.txt");
		fullPathArray = WalkBackPathForFile(path, paramFileName, autoParamFileName, nullptr, stopAtFirst);
	}
	else {
		aVect<wchar_t> fName, fPath;
		SplitPathW(path, nullptr, &fPath, &fName, nullptr);
		fullPathArray = WalkBackPathForFile(fPath, fName, nullptr, stopAtFirst);
	}
	if (!fullPathArray) MY_ERROR("Fichier \"Params.txt\" introuvable");

    return GetParams(fullPathArray, stopAtFirst, specialComments, pParamReferences);
}

mVect<Param> GetParams(const aVect<wchar_t>& path, bool stopAtFirst, aVect<char> & specialComments) {

	if (IsFileReadable(path)) {

		if (SAFE_CALL(GetFileAttributesW(path), INVALID_FILE_ATTRIBUTES) & FILE_ATTRIBUTE_DIRECTORY) {
			MY_ERROR("Must specifie a file");
		}

		mVect< mVect<wchar_t> > fullPathArray;
		fullPathArray.Push(path);

		return GetParams(fullPathArray, stopAtFirst, specialComments);
	}
	else return mVect<Param>();
}

void WriteParamsCore(WinFile& f, const aVect<Param> & params) {

	mVect<wchar_t> curFileName;
	aVect<char> blanks;

	params.Count();


	aVect_static_for(params, i) {
		if (!curFileName || strcmp_caseInsensitive(curFileName, params[i].fromFile) != 0) {
			curFileName.Reference(params[i].fromFile);
			f.Write("\n[From \"%S\"]\n\n", (wchar_t*)curFileName);
		}

		aVect<char> line("    %s = %s", (char*)params[i].name, (char*)params[i].value);
		if (params[i].comment) {
			size_t nBlanks = 80 - strlen(line);
			if (nBlanks < 1) nBlanks = 1;
			blanks.Redim(nBlanks + 1).Set(' ').Last() = 0;
			f.Write("%s%s# %s\n", (char*)line, (char*)blanks, (char*)params[i].comment);
		}
		else f.Write("%s\n", (char*)line);
	}

}

void PrintParams(const aVect<Param> & params) {

	WriteParamsCore(WinFile("CONOUT$"), params);
}

void WriteParams(aVect<wchar_t> & fileName, const aVect<Param> & params) {

	WinFile f(fileName, "w");

	if (!f.Open()) {
		MY_ERROR(aVect<char>("Unable to create file %S.", (wchar_t*)fileName));
	}

	WriteParamsCore(f, params);
}

mVect< mVect<char> > ParamNamesFromTemplate(const aVect<wchar_t>& templateFile) {

	WinFile input(templateFile, "rb");
	if (!input.Open())  MY_ERROR("Fichier template introuvable");

	static ConstCharPointer paramBalise("{parameter}");
	CharPointer line;
	mVect< mVect<char> > paraNames;
	aVect<char> buf;

	while (line = input.GetLine()) {
		buf.Copy(line);

		if (strstr(buf, paramBalise)) {
			CharPointer str, ptr = buf;
			str = RetrieveStr<','>(ptr);
			if (strcmp_caseInsensitive(str.Trim(), "parassign") == 0) {
				str = RetrieveStr<','>(ptr);
				paraNames.Push(aVect<char> (RetrieveStr<','>(*(char**)&str)).Trim());
			}
			buf.Copy(line);
		}
		
		CharPointer ptr = buf;
		while (ptr = GetParamBlock(ptr)) {
			RetrieveStr<':'>(ptr);
			CharPointer pName(RetrieveStr<'}'>(ptr));
			paraNames.Push(pName.Trim());
		 }
	}

	return paraNames;
}

mVect< mVect<char> > CommandsFromTemplate(const aVect<wchar_t>& templateFile) {

	WinFile input(templateFile, "rb");
	if (!input.Open())  MY_ERROR("Fichier template introuvable");

	mVect< mVect<char> > commands;
	
	while (CharPointer ptr = input.GetLine()) {
		while (ptr = GetCommandBlock(ptr)) {
			RetrieveStr<':'>(ptr);
			commands.Push(RetrieveStr<'}'>(ptr).Trim());
		}
	}

	return commands;
}

aVect<wchar_t> WriteInstanceFromTemplate(
	//const aVect<wchar_t>& instanceFile,
	aVect<wchar_t> instanceFile,
	const aVect<wchar_t>& templateFile,
	const aVect<Param>& parameters)
{

	auto commands = CommandsFromTemplate(templateFile);

	for (auto&& command : commands) {
		CharPointer rest = command;
		auto token = RetrieveStr<' ','('>(rest).Trim();
		if (!token) MY_ERROR("syntax error");
		if (strcmp(token, "MoveToSubFolder") == 0) {
			token = RetrieveStr<')'>(rest).Trim();
			if (!token) MY_ERROR("syntax error");
			auto folderName = RetrieveStr<'\"'>(token);
			if (!folderName) MY_ERROR("syntax error");
			wchar_t * path, * name;
			SplitPathW((wchar_t*)instanceFile, nullptr, &path, &name, nullptr);
			instanceFile.sprintf(L"%s\\%S", path, folderName);
			if (!IsDirectory(instanceFile)) SAFE_CALL(CreateDirectoryW(instanceFile, NULL));
			instanceFile.sprintf(L"%s\\%S\\%s", path, folderName, name);
		}
		else MY_ERROR(aVect<char>("unkown command : \"%s\"", (char*)command));
	}

    WinFile input(templateFile, "rb");
    WinFile output(instanceFile, "w");

    if (!input.Open())  MY_ERROR("Fichier template introuvable");
    if (!output.Open()) MY_ERROR("Unable to create instance file");

    CharPointer line, token;
    aVect<char> lineToWrite, blanks;

    static ConstCharPointer paramBalise("{parameter}");
	static ConstCharPointer pathBalise("{problem_path}");
	
	//printf("writing \"%s\"... ", (char*)instanceFile);
	
    while (line = input.GetLine()) {
        lineToWrite.Copy(line);
		bool lineParsed, skipLine;
		do {
			lineParsed = true, skipLine = false;
			if (char * startReplace = strstr(lineToWrite, pathBalise)) {
				wchar_t * path;
				SplitPathW(instanceFile, NULL, &path, NULL, NULL);
				*startReplace = 0;
				const char * endReplace   = startReplace + strlen(pathBalise);
				lineToWrite.sprintf("%s%s%s", (char*)lineToWrite, path, endReplace);
				lineParsed = false;
			}
			else if (char * startReplace = GetParamBlock(lineToWrite)) {
				*startReplace = 0;
				startReplace++;
				CharPointer endReplace(startReplace);
				RetrieveStr<':'>(endReplace);
				CharPointer paraName;
				paraName = RetrieveStr<'}'>(endReplace);
				paraName.Trim();
				bool found = false;
				aVect_static_for(parameters, i) {
					if (strcmp_caseInsensitive(parameters[i].name, paraName) == 0) {
						lineToWrite.sprintf("%s%s%s", (char*)lineToWrite, (char*)parameters[i].value, endReplace ? endReplace : "");
						found = true;
						lineParsed = false;
						break;
					}
				}
				if (!found) MY_ERROR(aVect<char>("Parametre inconnu \"%s\" dans le fichier template :\n\"%S\"\n", paraName, (wchar_t*)templateFile));
			}
			else if (char * startReplace = GetCommandBlock(lineToWrite)) {
				*startReplace = 0;
				startReplace++;
				CharPointer endReplace(startReplace);
				RetrieveStr<':'>(endReplace);
				auto command = RetrieveStr<'}'>(endReplace);
				auto commandName = RetrieveStr<'('>(command);
				if (!endReplace) endReplace = "";
				if (commandName && _strcmpi(commandName, "include") == 0) {
					aVect<wchar_t> includeFile = RetrieveStr<')'>(command);
					includeFile.Trim<L'\"'>();
					auto files = WalkBackPathForFile(instanceFile, includeFile, nullptr, true);
					if (!files) MY_ERROR(xFormat("include file not found: \"%s\"", includeFile));
					WinFile f(files[0]);
					lineToWrite.Erase();
					for (;;) {
						auto line = f.GetLine();
						if (!line) break;
						lineToWrite.AppendFormat("%s\n", line);
					}
				} else {
					lineToWrite.sprintf("%s%s", (char*)lineToWrite, endReplace).Trim();
				}
				
				if (!lineToWrite) skipLine = true;
				else lineParsed = false;
			}
			else {
				while (token = RetrieveStr<' ', ',', '\t'>(line)) {
					if (strcmp_caseInsensitive(token, paramBalise) == 0) {
						aVect<char> buf("Balise %s trouvee mais sans parametre associe",
										(const char*)paramBalise);
						MY_ERROR((char*)buf);
					}
					if (strcmp_caseInsensitive(token, "parassign") == 0) {
						if (token = RetrieveStr<' ', ',', '\t'>(line)) {
							aVect_static_for(parameters, i) {
								if (strcmp_caseInsensitive(parameters[i].name, token) == 0) {
									bool found = false;
									while (token = RetrieveStr<' ', ',', '\t'>(line)) {
										if (strcmp_caseInsensitive(token, paramBalise) == 0) {
											char * startReplace = strstr(lineToWrite, paramBalise);
											*startReplace = 0;
											const char * endReplace   = startReplace + strlen(paramBalise);
											size_t nBlanks = 0;
											if (*endReplace) {
												size_t strLen = endReplace - (char*)lineToWrite;
												for(int j=0;; ++j) {
													if (endReplace[j] == '\t') {
														int trueTab = 4 - strLen%4;
														nBlanks += trueTab;
														strLen  += trueTab;
													}
													else if (endReplace[j] == ' ') {
														nBlanks++;
														strLen++;
													}
													else {
														endReplace = &endReplace[j];
														break;
													}
												}
												nBlanks += strlen(paramBalise) - strlen(parameters[i].value);
												if (nBlanks < 1) nBlanks = 1;
											}
											blanks.Redim(nBlanks + 1).Set(' ').Last() = 0;
											lineToWrite.sprintf("%s%s%s%s", (char*)lineToWrite,
												(char*)parameters[i].value, (char*)blanks, endReplace);
											found = true;
											lineParsed = false;
											break;
										}
									}
									if (!found) {
										aVect<char> buf("Parametre %s trouve mais sans balise %s associee",
														(char*)parameters[i].name, (const char*)paramBalise);
										MY_ERROR((char*)buf);
									}
									goto break_while_2;
								}
							}
						}
					}
				}
			}
break_while_2:;
		} while(!lineParsed);
        
		if (!skipLine) output.Write("%s\n", (char*)lineToWrite);
    }

	return std::move(instanceFile);
}
//fullPath = fullPathArray[i]
void GetParamBranchCore(ParamNode & tree, const aVect<wchar_t> & fullPath, const wchar_t * path, mVect<mVect<char>> * paraList, const wchar_t * fileName = L"Params.txt") {

	auto autoFileName = "AutoParams.txt";

	aVect<wchar_t> tmpPath, tmpFileName;
	SplitPathW(fullPath, nullptr, &tmpPath, &tmpFileName, nullptr);
	auto branchParamFiles = WalkBackPathForFile(tmpPath, autoFileName, fileName); 

	//remove branchParamFiles[j] if not a descendant of path
	mVect_for_inv(branchParamFiles, j) {
		aVect<wchar_t> branchParamFilePath;
		SplitPathW(branchParamFiles[j], nullptr, &branchParamFilePath, nullptr, nullptr);
		if (wcslen(branchParamFilePath) < wcslen(path)) branchParamFiles.Remove(j);
	}

	ParamNode * pCurNode = &tree;
	mVect<Param> params;

	mVect< mVect<char> > emptyParaList;

	//add to paraList all param references in formulas ${...}
	if (!paraList) paraList = &emptyParaList;
	aVect<aVect<char> > paramReferences;

	mVect_for_inv(branchParamFiles, j) {

		params = GetInheritedParams(branchParamFiles[j].ToAvect(), true, aVect<char>(), &paramReferences);

		aVect_static_for(paramReferences, k) {
			bool found = false;
			aVect_static_for(*paraList, l) {
				if (strcmp_caseInsensitive(paramReferences[k], (*paraList)[l]) == 0) {
					found = true;
					break;
				}
			}
			if (!found) paraList->Push(paramReferences[k]);
		}

		bool found = false;
		if (paraList) {
			aVect_static_for(*paraList, k) {
				mVect_static_for(params, l) {
					if (strcmp_caseInsensitive((*paraList)[k], params[l].name) == 0) {
						found = true;
						goto dbl_break;
					}
				}
			}
		dbl_break:
			if (!found) continue;
		}

		mVect<wchar_t> fPath, fName;

		while (true) {
			aVect<wchar_t> curPath;
			SplitPathW(branchParamFiles[j], nullptr, &fPath, &fName, nullptr);
			SplitPathW(fPath, nullptr, &curPath, &fName, nullptr);

			if (strcmp_caseInsensitive(pCurNode->folder, curPath) != 0
				&& strcmp_caseInsensitive(pCurNode->folder, fPath) != 0) {
				aVect<wchar_t> str_n(fPath), str_np1;
				while (true) {
					SplitPathW(str_n, nullptr, &str_np1, nullptr, nullptr);
					if (strcmp_caseInsensitive(pCurNode->folder, str_np1) == 0) {
						break;
					}
					str_n.Steal(str_np1);
				}
				params.Free();
				str_n.sprintf(L"%s\\.", (wchar_t*)str_n);
				if (j == branchParamFiles.Count() - 1) {
					branchParamFiles.Push(str_n);
				}
				else {
					branchParamFiles.Insert(j + 1, str_n);
				}
				j++;
				//break;
			}
			else break;
		}

		found = false;

		if (strcmp_caseInsensitive(pCurNode->folder, fPath) == 0) {
			found = true;
		}
		else {
			mVect_static_for(pCurNode->children, k) {
				if (strcmp_caseInsensitive(pCurNode->children[k].folder, fPath) == 0) {
					found = true;
					pCurNode = &pCurNode->children[k];
					break;
				}
			}
		}

		if (!found) {
			ParamNode * ptr = pCurNode;
			while (ptr->parent) {
				if (strcmp_caseInsensitive(ptr->parent->folder, fPath) == 0) {
					found = true;
					break;
				}
				ptr = ptr->parent;
			}
		}

		if (!found) {
			pCurNode->AddChildren(params);
			pCurNode->children.Last().folder.Reference(fPath);
			pCurNode->children.Last().name.Reference(fName);
			pCurNode = &pCurNode->children.Last();
		}
		else {
			MergeParams(pCurNode->params, params);
		}
	}
}

mVect<mVect<wchar_t>> GetParamTreeCore(
	ParamNode & tree, 
	aVect<wchar_t> path, 
	mVect< mVect<char> > * paraList, 
	bool compactTree = true,
	bool displayBruteTree = true,
	bool displayFinalTree = true)
{

	aVect<wchar_t> buffer, fileName = L"Params.txt";
	mVect< mVect<wchar_t> > fullPathArray, branchParamFiles;
	aVect<wchar_t> fName, fPath;

	SplitPathW(path, nullptr, &fPath, &fName, nullptr);

	tree.name.sprintf("Cases");
	tree.folder.Copy(path);

	if (strcmp_caseInsensitive(fName, fileName) == 0) path.Copy(fPath);

	fullPathArray = WalkForwardPathForFile(path, fileName);

	if (!fullPathArray) MY_ERROR("Fichier \"Params.txt\" introuvable");

	mVect_static_for(fullPathArray, i) {

		wprintf(L"Gathering parameters for \"%s\"... ", (wchar_t*)fullPathArray[i]);

		GetParamBranchCore(tree, fullPathArray[i].ToAvect(), fPath, paraList, fileName);
		
		printf("Done\n");
	}

	if (displayBruteTree) {
		printf("Brute parameter tree:\n");
		tree.Print();
	}

	if (compactTree) {
		printf("Compacting parameter tree... ");
		tree.Compact();
		printf("Done\n");
	}

	printf("Unaliasing Node Names... ");
	tree.UnaliasNodeNames();
	printf("Done\n");
	
	printf("Inheriting parameters... ");
	tree.Inherit();
	printf("Done\n");

	printf("Computing formulas... ");
	tree.ComputeFormulas();
	printf("Done\n");

	printf("Discarding nodes... ");
	tree.DiscardNodes(fullPathArray);
	printf("Done\n");

	if (displayFinalTree) {
		printf("Final parameter tree:\n");
		tree.Print();
	}

	return fullPathArray;
}

mVect<mVect<wchar_t>> GetParamTree(
	ParamNode & tree, 
	const aVect<wchar_t>& path, 
	mVect< mVect<char> > * paraList, 
	bool compactTree,
	bool displayBruteTree,
	bool displayFinalTree)
{

	tree.Free();
	tree.name.sprintf("Root");
	tree.AddChildren(mVect<Param>());

	return GetParamTreeCore(tree.children[0], path, paraList, compactTree, displayBruteTree, displayFinalTree);
}

void GetParamBranch(
	ParamNode & tree,
	const wchar_t * file,
	const wchar_t * basePath,
	mVect< mVect<char> > * paraList,
	bool compactTree) {

	tree.Free();
	//tree.name.sprintf("Root");
	//tree.AddChildren(mVect<Param>());

	tree.name.sprintf("Cases");
	tree.folder.Copy(basePath);

	GetParamBranchCore(tree, file, basePath, paraList);

	if (compactTree) {
		tree.Compact();
	}

	tree.UnaliasNodeNames();
	tree.Inherit();
	tree.ComputeFormulas();

}

mVect< mVect<wchar_t> > WriteTemplateFromParamTree(
	ParamNode & node, 
	const aVect<wchar_t> & folder,
	const aVect<wchar_t> & templateFilePath,
	bool regroupLast, 
	AppendParamValueToFileNames appendParamValueToFileNames) {

	aVect<wchar_t> buffer, nextFolder, curTemplateFileName, paramSummaryFileName;
	mVect< mVect<wchar_t> > retVal;

	mVect_static_for(node.children, i) {
		if (node.parent) {
			nextFolder.sprintf(L"%s\\%s", (wchar_t*)folder, (wchar_t*)node.children[i].name);
			CreateDirectoryW(regroupLast ? folder : nextFolder, NULL);
		}
		else {
			nextFolder.Copy(folder);
		}

		if (node.children[i].children) {
			mVect< mVect<wchar_t> > tmp;
			tmp = WriteTemplateFromParamTree(node.children[i], nextFolder, templateFilePath, regroupLast, appendParamValueToFileNames);
			mVect_static_for(tmp, j) {
				retVal.Push(tmp[j]);
			}
		}
		else {
			ParamNode * ptr = &node.children[i];
			aVect<wchar_t> buf(L"");
			if (appendParamValueToFileNames == AppendParamValueToFileNames::yes ||
				appendParamValueToFileNames == AppendParamValueToFileNames::parentFolder) {
				while (ptr->parent && ptr->parent->parent) {
					if (!StrEqual(ptr->name, L"()")) {
						if (!buf || buf[0] == 0) buf.sprintf(L"_%s", (wchar_t*)ptr->name);
						else buf.sprintf(L"_%s_%s", (wchar_t*)ptr->name, (wchar_t*)buf + 1);
					}

					ptr = ptr->parent;
					if (appendParamValueToFileNames == AppendParamValueToFileNames::parentFolder) break;
				}
			}

			aVect<wchar_t> destTemplateFilePath;
			SplitPathW(templateFilePath, nullptr, nullptr, &destTemplateFilePath, nullptr);

			destTemplateFilePath.sprintf(
				L"%s\\%s",
				(wchar_t*)(regroupLast ? folder : nextFolder),
				(wchar_t*)destTemplateFilePath);
			
			buffer.Copy(destTemplateFilePath);

			wchar_t * f_path, *f_name, *f_ext;
			SplitPathW((wchar_t*)buffer, nullptr, &f_path, &f_name, &f_ext);
			curTemplateFileName.sprintf(L"%s\\%s%s.%s", f_path, f_name, (wchar_t*)buf, f_ext);
			auto possiblyRedirectedInstancePath = WriteInstanceFromTemplate(curTemplateFileName, templateFilePath, node.children[i].params.ToAvect());
			SplitPathW((wchar_t*)possiblyRedirectedInstancePath, nullptr, &f_path, &f_name, &f_ext);
			paramSummaryFileName.sprintf(L"%s\\%s_params.txt", f_path, f_name);
			
			WriteParams(paramSummaryFileName, node.children[i].params.ToAvect());
			
			retVal.Push(possiblyRedirectedInstancePath);

			xPrintf(L"Ecriture : %s\n", possiblyRedirectedInstancePath);
		}
	}

	return std::move(retVal);
}

Param * FindParam(mVect<Param> & params, const char * pName, bool nullIfNotFound) {

	mVect_static_for(params, i) {
		if (strcmp_caseInsensitive(params[i].name, pName) == 0) {
			return &params[i];
		}
	}

	if (nullIfNotFound) return nullptr;
	MY_ERROR(aVect<char>("Parametre \"%s\" introuvable", pName));
	return &params[0];
}

Param & FindParam(mVect<Param> & params, const char * pName) {

	return *FindParam(params, pName, false);
}

void MakeCases(aVect<wchar_t> & casesFolder, aVect<wchar_t> & tmpCasesFolder, aVect<wchar_t> & makeCasesFolder, bool putAutoParamsAtLeaves) {

	aVect<wchar_t> fileName;
	mVect<Param> parameters;
	aVect<char> specialComment("name");
	parameters = GetParams(xFormat(L"%s\\AutoParams.txt", makeCasesFolder), true, specialComment);
	
	CreateDirectoryW(tmpCasesFolder, NULL);

	//delete TMP case folder
	printf("Deleting TMP folder... ");
	aVect<wchar_t> deleteFromStr(L"%s\\*", (wchar_t*)tmpCasesFolder);
	deleteFromStr.Grow(1)[wcslen(deleteFromStr)+1] = 0; // double null terminate
	SHFILEOPSTRUCTW sDelete = { 0 };
	sDelete.hwnd = NULL;
	sDelete.wFunc = FO_DELETE;
	sDelete.fFlags = FOF_NOCONFIRMATION/* | FOF_SILENT*/;
	sDelete.pTo = NULL;
	sDelete.pFrom = deleteFromStr;
	int result = SHFileOperationW(&sDelete);
	if (result/* && result!=2*/) MY_ERROR(aVect<char>("SHFileOperation delete operation failed with error code : %d", result));

	printf("Done.\n");

	auto copyCases = [&](const wchar_t * toFolder) {

		printf("Copying cases to TMP folder... ");

		auto copyFromStr = xFormat(L"%s\\*", (wchar_t*)casesFolder);
		aVect<wchar_t> copyToStr = toFolder;//xFormat(L"%s", (wchar_t*)tmpCasesFolder);
		copyFromStr.Grow(1)[wcslen(copyFromStr) + 1] = 0; // double null terminate
		copyToStr.Grow(1)[wcslen(copyToStr) + 1] = 0; // double null terminate
		SHFILEOPSTRUCTW sCopy = { 0 };
		sCopy.hwnd = NULL;
		sCopy.wFunc = FO_COPY;
		sCopy.fFlags = FOF_NOCONFIRMATION /*| FOF_SILENT*/;
		sCopy.pTo = copyToStr;
		sCopy.pFrom = copyFromStr;
		result = SHFileOperationW(&sCopy);
		if (result) MY_ERROR(xFormat("SHFileOperation copy operation failed with error code : %S", GetWin32ErrorDescription(result)));

		printf("Done.\n");
	};

	if (putAutoParamsAtLeaves) {
		//copy case folder to TMP case folder
		copyCases(tmpCasesFolder);
	}

	aVect< aVect<wchar_t> > curFolders, nextCurFolders;
	aVect< aVect<char> > paramValues, names;

	mVect< mVect<wchar_t> > leafFiles;

	if (putAutoParamsAtLeaves) {
		leafFiles = WalkForwardPathForFile(tmpCasesFolder, aVect<wchar_t>("Params.txt"));
	}

	mVect_static_for(leafFiles, i) {
		aVect<wchar_t> path;
		SplitPathW(leafFiles[i], nullptr, &path, nullptr, nullptr);
		curFolders.Push(path);
	}

	if (!curFolders) curFolders.Push(tmpCasesFolder);

	//curFolders.Push_ByVal(tmpCasesFolder);

	printf("Creating parameter folder tree... ");
	aVect_for(parameters, i) {
		CharPointer token, remain(parameters[i].value);
		while(token = RetrieveStr<','>(remain)) paramValues.Push(token.Trim());

		if (parameters[i].comment && parameters[i].comment == strstr(parameters[i].comment, specialComment)) {
			CharPointer token, remain(parameters[i].comment);
			token = RetrieveStr<'='>(remain);
			while (token = RetrieveStr<','>(remain)) {
				if (StrEqual(token.Trim(), "\"\"")) token = nullptr;
				names.Push(token);
			}
		}

		aVect_static_for(curFolders, j) {
			for (size_t k=0, K=Max(paramValues.Count(), names.Count()); k<K; ++k) {
				aVect<wchar_t> folderName;
				if (k<names.Count()) folderName.Copy(aVect<wchar_t>(names[k]));
				//if (k<names.Count()) folderName = aVect<char>(names[k]);
				else  folderName.sprintf(L"%S=%S", (char*)parameters[i].name, (char*)paramValues[k]);

				//if (folderName) {
				nextCurFolders.Push(aVect<wchar_t>(L"%s\\%s", (wchar_t*)curFolders[j], folderName ? (wchar_t*)folderName : L"()"));
				CreateDirectoryW(nextCurFolders.Last(), NULL);
				//}
				WinFile f;
			
				if (k<paramValues.Count()) {
					if (!(f.Open(fileName.Format(L"%s\\AutoParams.txt", nextCurFolders.Last()), "w"))) MY_ERROR(xFormat("\"%S\" introuvable", fileName));
					f.Write("%s = %s\n", (char*)parameters[i].name, (char*)paramValues[k]);
				}
			}
		}
		curFolders.Steal(nextCurFolders);
		paramValues.Redim(0);
		names.Redim(0);
	}

	if (!putAutoParamsAtLeaves) {
		leafFiles = WalkForwardPathForFile(tmpCasesFolder, L"Params.txt", L"AutoParams.txt");

		for (auto&& f : leafFiles) {
			wchar_t * path;
			SplitPathW(f.GetDataPtr(), nullptr, &path, nullptr, nullptr);
			copyCases(path);
		}
	}

	printf("Done.\n");
}

void AutoInstanciateTemplate(
	const aVect<wchar_t> & templateFilePath, 
	bool compactTree,
	AppendParamValueToFileNames appendParamValueToFileNames) {

	aVect<wchar_t> buffer, baseFolder, templateFileName, commonFolder, relTemplatePath;
	mVect< mVect<wchar_t> > caseParamFiles, paramFiles;
	mVect< mVect<char> > paraNamesInTemplate;

	SplitPathW(templateFilePath, nullptr, &baseFolder, &templateFileName, nullptr);

	commonFolder.Copy(baseFolder);

	while(true) {
		SplitPathW(commonFolder, nullptr, &commonFolder, &buffer, nullptr);
		if (strcmp_caseInsensitive(buffer, L"Templates") == 0) break;
		relTemplatePath.sprintf(relTemplatePath ? L"%s\\%s" : L"%s", (wchar_t*)buffer, (wchar_t*)relTemplatePath);
	};
	
	aVect<wchar_t> casesFolder(L"%s\\Cases", (wchar_t*)commonFolder);
	aVect<wchar_t> tmpFolder(L"%s\\Tmp", (wchar_t*)commonFolder);
	aVect<wchar_t> tmpCasesFolder(L"%s\\Tmp\\Cases", (wchar_t*)commonFolder);
	aVect<wchar_t> makeCasesFolder(L"%s\\MakeCases", (wchar_t*)commonFolder);
	
	CreateDirectoryW(tmpFolder, NULL);
	MakeCases(casesFolder, tmpCasesFolder, makeCasesFolder);

	paraNamesInTemplate = ParamNamesFromTemplate(templateFilePath);

	ParamNode paramTree;
	GetParamTree(paramTree, tmpCasesFolder, &paraNamesInTemplate, compactTree);

	paramTree.Print();

	aVect<wchar_t> curFolder(L"%s\\Output", (wchar_t*)commonFolder);
	CreateDirectoryW(curFolder, NULL);

	wchar_t * ptr = buffer.Copy(relTemplatePath);
	while (wchar_t * ptrStart = RetrieveStr<L'\\'>(ptr)) {
		curFolder.sprintf(L"%s\\%s", (wchar_t*)curFolder, ptrStart);
		CreateDirectoryW(curFolder, NULL);
	}

	mVect< mVect<wchar_t> > fileList;
	fileList = WriteTemplateFromParamTree(paramTree, curFolder, templateFilePath, true, appendParamValueToFileNames);

	WinFile f(buffer.sprintf(L"%s\\%s.txt", (wchar_t*)commonFolder, (wchar_t*)templateFileName), "w");
	if (!f.Open()) MY_ERROR(aVect<char>("impossible de creer le fichier \"%S\"", (wchar_t*)buffer));
	mVect_static_for(fileList, i) {
		f.Write(aVect<char>("%S\n", (wchar_t*)fileList[i]));
	}
}

