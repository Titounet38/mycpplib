
#include "xVect.h"

class Param {

private:

	//mVect<Param> paramVector;

public:

	mVect<char> name;
	mVect<char> value;
	mVect<char> comment;
	mVect<wchar_t> fromFile;

	void Reference(Param & p);
	//void FixRef(Param * new_ptr, Param * old_ptr);
	//Param & operator[](int i);

	//void Fix_mVect(Fix_mVect_Data data) {
	//	this->name.FixRef(data);
	//	this->value.FixRef(data);
	//	this->comment.FixRef(data);
	//	this->fromFile.FixRef(data);
	//}
};


class ParamNode {

private:

	void CheckTreeCore();

public:

	mVect<Param> params;
	ParamNode * parent;
	mVect<ParamNode> children;
	mVect<wchar_t> folder;
	mVect<wchar_t> name;
	bool flag;

	//void Fix_mVect(Fix_mVect_Data data) {
	//	this->params.FixRef(data);
	//	this->children.FixRef(data);
	//	this->folder.FixRef(data);
	//	this->name.FixRef(data);
	//}

	ParamNode(ParamNode & that) = delete;

	ParamNode(ParamNode && that) :
		params(std::move(that.params)),
		parent(that.parent),
		children(std::move(that.children)),
		folder(std::move(that.folder)),
		name(std::move(that.name)),
		flag(that.flag)
	{

		aVect_static_for(this->children, i) {
			if (this->children[i].parent != &that) MY_ERROR("strange");
			this->children[i].parent = this;
		}

	}

	ParamNode();
	void AddChildren(mVect<Param> & p);
	void RemoveChild(size_t i);
	void Free();
	void Print(char * prefix = "");

	void CheckTree();
	void Compact();
	void UnaliasNodeNames();
	void Inherit();
	void ComputeFormulas();
};


void WriteParams(aVect<wchar_t> & fileName, const aVect<Param> & params);
mVect<Param> GetInheritedParams(const aVect<wchar_t> & path, bool stopAtFirst = false, aVect<char> & specialComments = aVect<char>(), aVect<aVect<char> > * pParamReferences = nullptr);
mVect<Param> GetParams(mVect< mVect<wchar_t> > & fullPathArray, bool stopAtFirst = false, aVect<char> & hasSpecialComment = aVect<char>(), aVect<aVect<char> > * pParamReferences = nullptr);
mVect<Param> GetParams(aVect<wchar_t>& path, bool stopAtFirst = false, aVect<char> & hasSpecialComment = aVect<char>());
mVect< mVect<char> > ParamNamesFromTemplate(const aVect<wchar_t>& templateFile);
aVect<wchar_t> WriteInstanceFromTemplate(
	//const aVect<wchar_t>& instanceFile,
	aVect<wchar_t> instanceFile,
    const aVect<wchar_t>& templateFile,
	const aVect<Param>& parameters);

enum class AppendParamValueToFileNames { no, yes, parentFolder };

void GetParamTree(ParamNode & tree, const aVect<wchar_t>& path, mVect< mVect<char> > * paraList = nullptr, bool compactTree = true, bool displayBruteTree = true, bool displayFinalTree = true);
mVect< mVect<wchar_t> > WriteTemplateFromParamTree(
	ParamNode & node, 
	const aVect<wchar_t> & folder,
	const aVect<wchar_t> & templateFilePath,
	bool regroupLast = true,
	AppendParamValueToFileNames appendParamValueToFileNames = AppendParamValueToFileNames::yes);

void MergeParams(mVect<Param> & dominant, mVect<Param> & recessif);
Param & FindParam(mVect<Param> & params, const char * pName);
Param * FindParam(mVect<Param> & params, const char * pName, bool nullIfNotFound);
void PrintParams(const aVect<Param> & params);

void MakeCases(aVect<char> & casesFolder, aVect<char> & makeCasesFolder);
void AutoInstanciateTemplate(const aVect<wchar_t> & templateFilePath, bool compactTree = true,
	AppendParamValueToFileNames appendParamValueToFileNames = AppendParamValueToFileNames::yes);


