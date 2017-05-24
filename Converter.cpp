
#define _CRT_SECURE_NO_WARNINGS

#include "xVect.h"
#include "math.h"
#include "string.h"
#include "Debug.h"
#include "MyUtils.h"
#include "WinUtils.h"
#include "Converter.h"
#include <limits> 

#define PI 3.1415926535897932384626433832795

char * stringsToReplace_before[] = { "STB" };
char * replaceWith_before[] =      { "sbbl" };

char * stringsToReplace[] = { "nauticalmiles",  "nauticmiles",    "seconde", "barrels", "grammes", "micron", "barrel", "heures", "gramme", "MMSCFD", "MMSCFd", "mmscf", "wéber", "metre", "litre", "heure", /*"milli", "micro",*/ "jours", "knots", "knot", "jour", "PSIA", "psia", "btu", "BTU", "SCF", "ATM", "PPM", "lbs", "STD", "std", "stb", "DAY", "PSI", "bpd", "cft", "Cal", "CFM", "CFH", "CFD", "degF", "degC", "KG", "PA", "kn",   "hr", "°f", "°c", "K", "L", "M" };
char * replaceWith[] =      { "nautical-miles", "nautical-miles", "second",  "bbl",     "grams",   "µm",     "bbl",    "h",      "gram",   "MMscfd", "MMscfd", "MMscf", "weber", "meter", "liter", "h",     /*"m",     "µ",    */ "day",   "NM/h",  "NM/h", "day",   "psi",  "psi", "Btu", "Btu", "scf", "atm", "ppm", "lb",  "s",   "s",  "sbbl", "day", "psi", "BPD", "cf",  "cal", "cfm", "cfh", "cfd",   "°F",   "°C", "kg", "Pa", "NM/h", "h",  "°F", "°C", "k", "l", "m" };

char * multipliersFullName[] = { "atto", "femto", "pico", "nano", "micro", "milli", "centi", "deci", "deca", "hecto", "kilo", "Millon", "Mega", "Giga", "Tera", "Peta", "Exa" };
char * multipliers[]         = { "a",    "f",     "p",    "n",    "µ",     "m",     "c",     "d",    "da",   "h",     "k",    "MM",     "M",    "G",    "T",    "P",    "E" };
double multiplierVal[]       = { 1e-18,  1e-15,   1e-12,  1e-9,   1e-6,    1e-3,    1e-2,    1e-1,    1e1,    1e2,    1e3,    1e6,      1e6,    1e9,    1e12,   1e15,   1e18 };

// www.eng.auth.gr/~chemtech/foititika/various/monades_metatropes_sp811.pdf

char * lengthUnitsFullNamePlural[] = { "meters", "feet", "inches", "miles",    "nautical-miles", "yards",  "light-years",    "parsecs" };
char * lengthUnitsFullName[]       = { "meter",  "foot", "inch",   "mile",     "nautical-mile",  "yard",   "light-year",     "parsec" };
char * lengthUnits[]               = { "m",      "ft",   "in",     "mi",       "NM",             "yd",     "ly",		     "pc" };
double lengthConvFact[]            = { 1,        0.3048, 0.0254,   1.609344e3, 1852,             0.9144,   9460730472580800,  3.085677581e16 };

//char * surfaceUnits[]    = {"sft",     "sf",        "sfoot",     "sfeet",     "sinch"};
//double surfaceConvFact[] = {0.09290304, 0.09290304,  0.09290304,  0.09290304,  0.00064516};

char * volumeUnitsFullNamePlural[] = { "liters", "cubic-feet",   "cubic-feet",   "cubic-feet",   "cubic-feet",   "cubic-inches", "cubic-inches", "cubic-inches", "oil-barrels", "US-gallons", "US-ounces" };
char * volumeUnitsFullName[]       = { "liter",  "cubic-foot",   "cubic-foot",   "cubic-foot",   "cubic-foot",   "cubic-inch",   "cubic-inch",   "cubic-inch",   "oil-barrel",  "US-gallon",  "US-ounce" };
char * volumeUnits[]               = { "l",      "cf",           "cft",          "cfoot",        "cfeet",        "cinches",      "cinch",        "cin",          "bbl",         "gal",        "oz" };
double volumeConvFact[]            = { 1e-3,     0.028316846592, 0.028316846592, 0.028316846592, 0.028316846592, 0.000016387064, 0.000016387064, 0.000016387064, 1.589873e-1,   0.00378541,   2.95735295625e-5 };

//char * FlowRateUnits[]    = {"sm3", "scf"};
//double FlowRateConvFact[] = {1,    0.028316846592};

char * timeUnitsFullNamePlural[] = { "seconds",  "minutes", "hours",   "days",    "years" };
char * timeUnitsFullName[]       = { "second",   "minute",  "hour",    "day",     "year" };
char * timeUnits[]               = { "s",        "min",     "h",       "d",       "y" };
double timeConvFact[]            = { 1,          60,        3600,      24 * 3600, 365.25 * 24 * 3600 };

char * massUnitsFullNamePlural[] = { "grams", "tonnes", "pounds" };
char * massUnitsFullName[]       = { "gram",  "tonne",  "pound" };
char * massUnits[]               = { "g",     "t",      "lb" };
double massConvFact[]            = { 1e-3,    1000,     0.45359237 };

char * intensityUnitsFullNamePlural[] = { "Amperes" };
char * intensityUnitsFullName[]       = { "Ampere" };
char * intensityUnits[]               = { "A" };
double intensityConvFact[]            = { 1 };

char * pressureUnitsFullNamePlural[] = { "Pascals", "bars", "bars-absolute", "pounds-per-square-inch", "atmospheres" };
char * pressureUnitsFullName[]       = { "Pascal",  "bar",  "bar-absolute",  "pound-per-square-inch",  "atmosphere" };
char * pressureUnits[]               = { "Pa",      "bar",  "bara",          "psi",                    "atm" };
double pressureConvFact[]            = { 1,         1e5,    1e5,             6894.757,                 1.01325e5 };

char * viscosityUnitsFullNamePlural[] = { "Poises", "Poises", "Poiseuilles" };
char * viscosityUnitsFullName[]       = { "Poise",  "Poise",  "Poiseuille" };
char * viscosityUnits[]               = { "P",      "Po",     "Pl" };
double viscosityConvFact[]            = { 0.1,      0.1,      1 };

char * kinematicViscosityUnitsFullNamePlural[] = { "Stokes" };
char * kinematicViscosityUnitsFullName[]       = { "Stokes" };
char * kinematicViscosityUnits[]               = { "St" };
double kinematicViscosityConvFact[]            = { 1e-4 };

char * energyUnitsFullNamePlural[] = { "Joules", "Watt-hours", "electron-Volts", "British-thermal-units", "Calories" };
char * energyUnitsFullName[]       = { "Joule",  "Watt-hour",  "electron-Volt",  "British-thermal-unit",  "Calorie" };
char * energyUnits[]               = { "J",      "Wh",         "eV",             "Btu",                   "cal"};
double energyConvFact[]            = { 1,        3600,         1.602177e-19,     1055.056,                4.18400 };

char * electricTensionUnitsFullNamePlural[] = { "Volts" };
char * electricTensionUnitsFullName[]       = { "Volt" };
char * electricTensionUnits[]               = { "V" };
double electricTensionConvFact[]            = { 1 };

char * frequencyUnitsFullNamePlural[] = { "Hertzes" };
char * frequencyUnitsFullName[]       = { "Hertz" }; 
char * frequencyUnits[]               = { "Hz" };
double frequencyConvFact[]            = { 1 };

char * powerUnitsFullNamePlural[] = { "Watts" };
char * powerUnitsFullName[]       = { "Watt" };
char * powerUnits[]               = { "W" };
double powerConvFact[]            = { 1 };

char * forceUnitsFullNamePlural[] = { "Newtons", "grammes-force", "dynes" };
char * forceUnitsFullName[]       = { "Newton",  "gramme-force",  "dyne" };
char * forceUnits[]               = { "N",       "gf",            "dyn" };
double forceConvFact[]            = { 1,         9.80665e-3,      1e-5 };

char * magneticInductionUnitsFullNamePlural[] = { "Teslas", "Gauss" };
char * magneticInductionUnitsFullName[]       = { "Tesla",  "Gauss" };
char * magneticInductionUnits[]               = { "T",      "G" };
double magneticInductionConvFact[]            = { 1,        1e-4 };

char * magneticFluxUnitsFullNamePlural[] = { "Webers", "Maxwells" };
char * magneticFluxUnitsFullName[]       = { "Weber",  "Maxwell" };
char * magneticFluxUnits[]               = { "Wb",     "Mx" };
double magneticFluxConvFact[]            = { 1,        1e-8 };

char * electricalResistanceUnitsFullNamePlural[] = { "Ohms" };
char * electricalResistanceUnitsFullName[]       = { "Ohm" };
char * electricalResistanceUnits[]               = { "O" };
double electricalResistanceConvFact[]            = { 1 };

char * electricalCapacitanceUnitsFullNamePlural[] = { "Farads" };
char * electricalCapacitanceUnitsFullName[]       = { "Farad" };
char * electricalCapacitanceUnits[]               = { "F" };
double electricalCapacitanceConvFact[]            = { 1 };

char * electricalConductanceUnitsFullNamePlural[] = { "Siemens" };
char * electricalConductanceUnitsFullName[]       = { "Siemen" };
char * electricalConductanceUnits[]               = { "S" };
double electricalConductanceConvFact[]            = { 1 };

char * inductanceUnitsFullNamePlural[] = { "Henrys" };
char * inductanceUnitsFullName[]       = { "Henry" };
char * inductanceUnits[]               = { "H" };
double inductanceConvFact[]            = { 1 };

char * electricChargeUnitsFullNamePlural[] = { "Coulombs" };
char * electricChargeUnitsFullName[]       = { "Coulomb" };
char * electricChargeUnits[]               = { "C" };
double electricChargeConvFact[]            = { 1 };

char * temperatureUnitsFullNamePlural[] = { "Celsius", /*"Celsius",*/ "Fahrenheits",     /*"Fahrenheits",*/  "Kelvins", "Kelvins", "Rankines" };
char * temperatureUnitsFullName[]       = { "Celsius", /*"Celsius",*/ "Fahrenheit",      /*"Fahrenheit",*/   "Kelvin",  "Kelvin",  "Rankine" };
char * temperatureUnits[]               = { "°C",      /*"C",*/       "°F",              /*"F",*/            "°K",      "K",       "°R" };
double temperatureConvFact[]            = { 1,         /*1,*/         1 / 1.8,           /*1/1.8,*/           1,        1,          1 / 1.8 };
double temperatureConvOffset[]          = { 273.15,    /*273.15,*/    273.15 - 32 / 1.8, /*273.15 - 32/1.8,*/ 0,        0,          0 };
 
char * stdFlowrateUnitsFullNamePlural[] = { "standard-cubic-feet-per-second", "standard-cubic-feet-per-minute", "standard-cubic-feet-per-hour", "standard-cubic-feet-per-day",    "standard-cubic-feet-per-year",            "standard-Oil-barrels-per-day" };
char * stdFlowrateUnitsFullName[]       = { "standard-cubic-foot-per-second", "standard-cubic-foot-per-minute", "standard-cubic-foot-per-hour", "standard-cubic-foot-per-day",    "standard-cubic-foot-per-year",            "standard-Oil-barrels-per-day" };
char * stdFlowrateUnits[]               = { "scfs",                           "scfm",                           "scfh",                         "scfd",                           "scfy",                                    "SBPD" };
double stdFlowrateConvFact[]            = { 0.0004719474432*60.0,             0.0004719474432,                  0.0004719474432 / 60.0,         0.0004719474432 / 60.0 / 24.0,    0.0004719474432 / 60.0 / 24.0 / 362.25,    1.840130787037e-6 };

char * flowrateUnitsFullNamePlural[] = { "cubic-feet-per-second", "cubic-feet-per-minute", "cubic-feet-per-hour",  "cubic-feet-per-day",          "cubic-feet-per-year",                      "Oil-barrels-per-day" };
char * flowrateUnitsFullName[]       = { "cubic-foot-per-second", "cubic-foot-per-minute", "cubic-foot-per-hour",  "cubic-foot-per-day",          "cubic-foot-per-year",                      "Oil-barrels-per-day" };
char * flowrateUnits[]               = { "cfs",                   "cfm",                   "cfh",                  "cfd",                         "cfy",                                      "BPD" };
double flowrateConvFact[]            = { 0.0004719474432*60.0,    0.0004719474432,         0.0004719474432 / 60.0, 0.0004719474432 / 60.0 / 24.0,  0.0004719474432 / 60.0 / 24.0 / 362.25,    1.840130787037e-6 };

char * adimUnitsFullNamePlural[] = { "1", "parts-per-million", "percents" };
char * adimUnitsFullName[]       = { "1", "part-per-million",  "percent" };
char * adimUnits[]               = { "1", "ppm",               "%" };
double adimConvFact[]            = { 1,   1e-6,                1e-2 };

char * angleUnitsFullNamePlural[] = { "radians", "degrees" };
char * angleUnitsFullName[]       = { "radian",  "degree" };
char * angleUnits[]               = { "rad",     "°" };
double angleConvFact[]            = { 1,         PI / 180 };

char * solidAngleUnitsFullNamePlural[]	= { "steradians" };
char * solidAngleUnitsFullName[]		= { "steradian" };
char * solidAngleUnits[]				= { "sr" };
double solidAngleConvFact[]				= { 1 };

char * luminousIntensityUnitsFullNamePlural[]	= { "candelas", "candlepower" };
char * luminousIntensityUnitsFullName[]			= { "candela", "candlepower" };
char * luminousIntensityUnits[]					= { "cd", "cp" };
double luminousIntensityConvFact[]				= { 1, 0.981 };

char * luminousPowerUnitsFullNamePlural[]	= { "lumens" };
char * luminousPowerUnitsFullName[]			= { "lumen" };
char * luminousPowerUnits[]					= { "lm" };
double luminousPowerConvFact[]				= { 1 };

char * illuminanceUnitsFullNamePlural[]	= { "lux" };
char * illuminanceUnitsFullName[]		= { "lux" };
char * illuminanceUnits[]				= { "lx" };
double illuminanceConvFact[]			= { 1 };

char * moleUnitsFullNamePlural[]	= { "moles", "kilo-gram-moles", "gram-moles" };
char * moleUnitsFullName[]			= { "mole",  "kilo-gram-mole",  "gram-mole" };
char * moleUnits[]					= { "mol",   "kgmol",           "gmol" };
double moleConvFact[]				= { 1,       1000,              1 };

char * informationUnitsFullNamePlural[] = { "bits", "Bytes", "octets" };
char * informationUnitsFullName[]		= { "bit",  "Byte",  "octet" };
char * informationUnits[]				= { "bit",  "B",     "o"};
double informationConvFact[]			= { 1,      8,       8 };

Dimension::Dimension() : length(0), time(0), mass(0), temperature(0), intensity(0), standard(0), angle(0), mole(0), luminousIntensity(0), information(0) {}

Dimension::Dimension(const Dimension & d) : length(d.length), time(d.time), mass(d.mass), temperature(d.temperature),
	intensity(d.intensity), standard(d.standard), angle(d.angle), mole(d.mole), luminousIntensity(d.luminousIntensity),
	information(d.information) {}

Dimension::Dimension(double pLength, double pTime, double pMass, double pTemperature, double pIntensity, double pStandard, double angle, double mole, double luminousIntensity, double information) :
	length(pLength), time(pTime), mass(pMass), temperature(pTemperature),
	intensity(pIntensity), standard(pStandard), angle(angle), mole(mole), 
	luminousIntensity(luminousIntensity), information(information) {}

bool Dimension::IsPureTemperature() {
	return 
		this->temperature == 1 &&
		this->length == 0      &&
		this->time == 0        &&
		this->mass == 0        &&
		this->intensity == 0   &&
		this->standard == 0    &&
		this->angle == 0	   &&
		this->mole == 0	       &&
		this->information == 0 &&
		this->luminousIntensity == 0;
}

Dimension & Dimension::Copy(Dimension & d) {
	this->length = d.length;
	this->time = d.time;
	this->mass = d.mass;
	this->temperature = d.temperature;
	this->intensity = d.intensity;
	this->standard = d.standard;
	this->angle = d.angle;
	this->mole = d.mole;
	this->luminousIntensity = d.luminousIntensity;
	this->information = d.information;
	return *this;
}

bool Dimension::operator==(Dimension & d) {
	return
		this->length == d.length           &&
		this->time == d.time               &&
		this->mass == d.mass               &&
		this->temperature == d.temperature &&
		this->intensity == d.intensity     &&
		this->standard == d.standard       &&
		this->angle == d.angle			   &&
		this->mole == d.mole			   &&
		this->information == d.information &&
		this->luminousIntensity == d.luminousIntensity;
}

bool Dimension::operator==(double val) { 
	return
		this->length == val      &&
		this->time == val        &&
		this->mass == val        &&
		this->temperature == val &&
		this->intensity == val   &&
		this->standard == val    &&
		this->angle == val		 &&
		this->mole == val		 &&
		this->information == val &&
		this->luminousIntensity == val;
}

bool Dimension::operator!=(double val) {
	return
		this->length != val      ||
		this->time != val        ||
		this->mass != val        ||
		this->temperature != val ||
		this->intensity != val   ||
		this->standard != val    ||
		this->angle != val		 ||
		this->mole != val		 ||
		this->information != val ||
		this->luminousIntensity != val;
}

bool Dimension::operator!=(Dimension & d) {
	return
		this->length != d.length           ||
		this->time != d.time               ||
		this->mass != d.mass               ||
		this->temperature != d.temperature ||
		this->intensity != d.intensity	   ||
		this->standard != d.standard       ||
		this->angle != d.angle			   ||
		this->mole != d.mole			   ||
		this->information != d.information ||
		this->luminousIntensity != d.luminousIntensity;
}

Dimension Dimension::operator+(Dimension & d) {
	return Dimension(
		this->length + d.length,
		this->time + d.time,
		this->mass + d.mass,
		this->temperature + d.temperature,
		this->intensity + d.intensity,
		this->standard + d.standard,
		this->angle + d.angle,
		this->mole + d.mole,
		this->luminousIntensity + d.luminousIntensity,
		this->information + d.information);
}

Dimension Dimension::operator-(Dimension & d) {
	return Dimension(
		this->length - d.length,
		this->time - d.time,
		this->mass - d.mass,
		this->temperature - d.temperature,
		this->intensity - d.intensity,
		this->standard - d.standard,
		this->angle - d.angle,
		this->mole - d.mole,
		this->luminousIntensity - d.luminousIntensity,
		this->information - d.information);
}

Dimension & Dimension::operator+=(Dimension & d) {
	this->length += d.length;
	this->time += d.time;
	this->mass += d.mass;
	this->temperature += d.temperature;
	this->intensity += d.intensity;
	this->standard += d.standard;
	this->angle += d.angle;
	this->mole += d.mole;
	this->luminousIntensity += d.luminousIntensity;
	this->information += d.information;
	return *this;
}

Dimension & Dimension::operator-=(Dimension & d) {
	this->length -= d.length;
	this->time -= d.time;
	this->mass -= d.mass;
	this->temperature -= d.temperature;
	this->intensity -= d.intensity;
	this->standard -= d.standard;
	this->angle -= d.angle;
	this->mole -= d.mole;
	this->luminousIntensity  -= d.luminousIntensity;
	this->information -= d.information;
	return *this;
}

aVect<char> Dimension::ToString() {

	aVect<char> retVal;

	auto f = [](auto&& str, auto&& name, auto val) {
		if (!val) return;
		if (str) str.Append(" ");
		str.Append("[%s", name);
		if (val != 1) str.Append("^%g", val);
		str.Append("]");
	};

	f(retVal, "Angle", this->angle);
	f(retVal, "Intensity", this->intensity);
	f(retVal, "Information", this->information);
	f(retVal, "Length", this->length);
	f(retVal, "Luminous Intensity", this->luminousIntensity);
	f(retVal, "Mass", this->mass);
	f(retVal, "Mole", this->mole);
	f(retVal, "Temperature", this->temperature);
	f(retVal, "Time", this->time);

	if (!retVal) retVal = "[.]";

	return retVal;
}

template <class T> Dimension operator*(T scalar, Dimension & d) {
	return Dimension(
		scalar * d.length,
		scalar * d.time,
		scalar * d.mass,
		scalar * d.temperature,
		scalar * d.intensity,
		scalar * d.standard,
		scalar * d.angle,
		scalar * d.mole,
		scalar * d.luminousIntensity,
		scalar * d.information);
}

template <class T> Dimension operator*(Dimension & d, T scalar) {
	return scalar * d;
}

template <class T> Dimension operator/(T scalar, Dimension & d) {
	return Dimension(
		scalar / d.length,
		scalar / d.time,
		scalar / d.mass,
		scalar / d.temperature,
		scalar / d.intensity,
		scalar / d.standard,
		scalar / d.angle,
		scalar / d.mole, 
		scalar / d.luminousIntensity,
		scalar / d.information);
}

template <class T> Dimension operator/(Dimension & d, T scalar) {
	return d * (1 / scalar);
}

struct UnitConversion {
	aVect<char> unit;
	aVect<char> unitFullName;
	aVect<char> unitFullNamePlural;
	double convFact;
	double convOffset;
	Dimension dim;
};

struct DimConversion {
	aVect<UnitConversion> uConv;
	Dimension dim;

	DimConversion(char ** u, char ** uFN, char ** uFNP, double * cf, size_t n, Dimension & d, double * co = nullptr) {
		uConv.Redim(n);
		for (size_t i = 0; i<n; ++i) {
			uConv[i].unit = u[i];
			uConv[i].unitFullName = uFN[i];
			uConv[i].unitFullNamePlural = uFNP[i];
			uConv[i].convFact = cf[i];
			uConv[i].convOffset = co ? co[i] : 0;
			uConv[i].dim = d;
		}
		dim = d;
	}

	template<class>
	DimConversion(DimConversion & that) {
		static_assert(false, "nope");
	}

	DimConversion(DimConversion && that) :
		uConv(std::move(that.uConv)),
		dim(that.dim)
	{}
};


ValueAndUnit::ValueAndUnit(double v, const char * u) : value(v) {
	if (u) unit.Copy(u);
}

void ValueAndUnit::ReuseCtor(double v, const char * u) {
	this->value = v;
	this->unit = u;
}

void MakeFractionStrCore(
	aVect<char> & fractionStr,
	aVect<char> * fractionStrPlural,
	CachedVect< aVect<char> > & numerators,
	CachedVect< aVect<char> > * numeratorsPlural,
	CachedVect< aVect<char> > & denominators,
	CachedVect< aVect<char> > * denominatorsPlural,
	bool spaces)
{

	fractionStr.Redim(0);
	if (fractionStrPlural) fractionStrPlural->Redim(1).Last() = 0;

	while (numerators) {
		if (fractionStr) {
			fractionStr.Append(spaces ? " * " : "*");
			if (fractionStrPlural) fractionStrPlural->Append(spaces ? " * " : "*");
		}
		fractionStr.Append(numerators[0]);
		if (numerators.Count() == 1) {
			if (fractionStrPlural) fractionStrPlural->Append((*numeratorsPlural)[0]);
		}
		else if (fractionStrPlural) fractionStrPlural->Append(numerators[0]);

		numerators.Remove(0);
		if (numeratorsPlural) numeratorsPlural->Remove(0);
	}
	while (denominators) {
		if (!fractionStr) {
			fractionStr.Append(spaces ? "1 / " : "1/");
			if (fractionStrPlural) fractionStrPlural->Append(spaces ? "1 / " : "1/");
		}
		else {
			fractionStr.Append(spaces ? " / " : "/");
			if (fractionStrPlural) fractionStrPlural->Append(spaces ? " / " : "/");
		}

		fractionStr.Append(denominators[0]);
		if (fractionStrPlural) fractionStrPlural->Append(denominators[0]);

		denominators.Remove(0);
		if (denominatorsPlural) denominatorsPlural->Remove(0);
	}

	if (!fractionStr) fractionStr.Redim(1).Last() = 0;
	if (!fractionStrPlural) if (fractionStrPlural) fractionStrPlural->Redim(1).Last() = 0;

	//if (str)
	ReplaceStr_CaseInsensitive(fractionStr, "(standard-", "standard-(");
	if (fractionStrPlural) ReplaceStr_CaseInsensitive(*fractionStrPlural, "(standard-", "standard-(");
}

void MakeFractionStr(
	aVect<char> & fractionStr,
	aVect<char> & fractionStrPlural,
	CachedVect< aVect<char> > & numerators,
	CachedVect< aVect<char> > & numeratorsPlural,
	CachedVect< aVect<char> > & denominators,
	CachedVect< aVect<char> > & denominatorsPlural,
	bool spaces = false)
{

	MakeFractionStrCore(
		fractionStr, &fractionStrPlural,
		numerators, &numeratorsPlural,
		denominators, &denominatorsPlural,
		spaces);
}

void MakeFractionStr(
	aVect<char> & fractionStr,
	CachedVect< aVect<char> > & numerators,
	CachedVect< aVect<char> > & denominators,
	bool spaces = false) {

	MakeFractionStrCore(
		fractionStr, nullptr,
		numerators, nullptr,
		denominators, nullptr,
		spaces);
}

SI_PhysicalValue::SI_PhysicalValue() : SI_value(0), orig_value(0), error(true) {}

SI_PhysicalValue::SI_PhysicalValue(double orig_v, double SI_v, 
	Dimension & d, char * orig_uS, char * orig_uSFN, char * orig_uSFNP) 
	:
		orig_value(orig_v), SI_value(SI_v), dim(d), error(false)
	{

		orig_unitStr.Copy(orig_uS);
		orig_unitStrFullName.Copy(orig_uSFN);
		orig_unitStrFullNamePlural.Copy(orig_uSFNP);
		MakeUnitStr();
	}

void SI_PhysicalValue::ReuseCtor(double orig_v, double SI_v,
	Dimension & d, char * orig_uS, char * orig_uSFN, char * orig_uSFNP) 
{

	this->orig_value = orig_v;
	this->SI_value = SI_v;
	this->dim = d;
	this->error = false;

	this->orig_unitStr = orig_uS;
	this->orig_unitStrFullName = orig_uSFN;
	this->orig_unitStrFullNamePlural = orig_uSFNP;
	this->MakeUnitStr();
}

void SI_PhysicalValue::ReuseCtor()
{
	
	this->orig_value = 0;
	this->SI_value = 0;
	this->dim = Dimension();
	this->error = true;

	this->orig_unitStr.Redim(0);
	this->orig_unitStrFullName.Redim(0);
	this->orig_unitStrFullNamePlural.Redim(0);
	this->SI_unitStr.Redim(0);
	this->SI_value = 0;
}

void SI_PhysicalValue::MakeExposantStr(aVect<char> & str, double val, double tol) {

	NiceFloat(str, val, tol);
}

void SI_PhysicalValue::MakeDimStr(
	CachedVect< aVect<char> > & numerators,
	CachedVect< aVect<char> > & denominators,
	char * str, double d) {

	if (d == 0) return;

	static WinCriticalSection cs;
	Critical_Section(cs) {
		static aVect<char> exposant;
		exposant.Redim(0);

		CachedVect< aVect<char> > & okString = (d > 0) ? numerators : denominators;

		if (d < 0) d *= -1;

		okString.Push(str);

		if (d != 1) {
			MakeExposantStr(exposant, d);
			okString.Last().Append(exposant);
		}
	}
}

void SI_PhysicalValue::MakeUnitStr() {

	static WinCriticalSection cs;
	Critical_Section(cs) {
		static CachedVect< aVect<char> > numerators, denominators;
		numerators.Redim(0);
		denominators.Redim(0);

		if (dim.standard) {
			if (!IsEqualWithPrec(this->dim.length, 3 * this->dim.standard, 1e-10)) {
				MessageBoxA(NULL, "Standard conditions should apply to a volume", "Warning", MB_ICONWARNING);
			}
			this->MakeDimStr(numerators, denominators, "Sm", this->dim.length);
		}
		else {
			this->MakeDimStr(numerators, denominators, "m", this->dim.length);
		}
		this->MakeDimStr(numerators, denominators, "s",   this->dim.time);
		this->MakeDimStr(numerators, denominators, "K",   this->dim.temperature);
		this->MakeDimStr(numerators, denominators, "A",   this->dim.intensity);
		this->MakeDimStr(numerators, denominators, "kg",  this->dim.mass);
		if (this->dim.angle == 2 || this->dim.angle == -2) {
			this->MakeDimStr(numerators, denominators, "sr", sign(this->dim.angle));
		}
		else {
			this->MakeDimStr(numerators, denominators, "rad", this->dim.angle);
		}
		this->MakeDimStr(numerators, denominators, "mol", this->dim.mole);
		this->MakeDimStr(numerators, denominators, "cd",  this->dim.luminousIntensity);
		this->MakeDimStr(numerators, denominators, "bit", this->dim.information);

		MakeFractionStr(this->SI_unitStr, numerators, denominators);
	}
}

struct UnitCompStrlen {
	ptrdiff_t strLength;
	UnitConversion * uConv;
	bool fullName;
	bool plural;

	UnitCompStrlen() {}
	UnitCompStrlen(UnitConversion & uc, bool fn = false, bool pl = false) :
		fullName(fn),
		plural(pl),
		strLength(strlen(fn ? (pl ? uc.unitFullNamePlural : uc.unitFullName) : uc.unit)),
		uConv(&uc)
	{}

	bool operator>(UnitCompStrlen & uc) {
		return this->strLength > uc.strLength;
	}

	bool operator<(UnitCompStrlen & uc) {
		return this->strLength < uc.strLength;
	}
};


DimensionTypes::DimensionTypes() {

	this->length.length = 1;
	this->time.time = 1;
	this->mass.mass = 1;
	this->temperature.temperature = 1;
	this->intensity.intensity = 1;
	this->angle.angle = 1;

	this->solidAngle.angle = 2;

	this->surface = this->length * 2;
	this->volume = this->length * 3;
	this->frequency = this->time * -1;
	this->stdVolume = this->volume, this->stdVolume.standard = 1;
	this->velocity = this->length - this->time;
	this->acceleration = this->velocity - this->time;
	this->force = this->acceleration + this->mass;
	this->pressure = this->force - this->surface;
	this->flowrate = this->volume - this->time;
	this->stdFlowrate = this->stdVolume - this->time;
	this->massFlowrate = this->mass - this->time;
	this->density = this->mass - this->volume;
	this->energy = this->force + this->length;
	this->power = this->energy - this->time;
	this->entropy = this->energy - this->temperature;
	this->viscosity = this->pressure + this->time;
	this->magneticInduction = this->mass - this->intensity - 2 * this->time;
	this->magneticFlux = this->magneticInduction + this->surface;
	this->electricTension = this->power - this->intensity;
	this->electricalResistance = this->electricTension - this->intensity;
	this->electricCharge = this->intensity - this->time;
	this->electricalCapacitance = this->electricCharge - this->electricTension;
	this->electricalConductance = this->electricalResistance * -1;
	this->inductance = this->electricalResistance + this->time;
	this->kinematicViscosity = this->viscosity - this->density;
	this->surfaceTension = this->force - this->length;

	this->mole.mole = 1;

	this->luminousIntensity.luminousIntensity = 1;
	this->luminousPower = this->luminousIntensity + 2 * this->angle;
	this->luminousEnergy = this->luminousPower + this->time;
	this->luminance = this->luminousIntensity - this->surface;
	this->illuminance = this->luminousPower - this->surface;
	this->luminousExposure = this->illuminance + this->time;
	this->luminousEnergyDensity = this->luminousEnergy - this->volume;
	this->luminousEfficacy = this->luminousPower - this->power;

	this->informationContent.information = 1;
}

DimensionInfo::DimensionInfo(char * t, char * SIuu) : type(t), SI_usualUnit(SIuu) {}

DimensionTypesEx::DimensionTypesEx() {
	this->thermalConductivity = this->power - this->length - this->temperature;
	this->heatTranfertCoefficient = this->thermalConductivity - this->length;
	this->heatFlux = this->heatTranfertCoefficient + this->temperature;
	this->density = this->mass - this->volume;
	this->specificHeat = this->entropy - this->mass;
}

DimensionInfo DimensionTypesEx::GetDimensionName(Dimension & dim) {

	if (dim == this->acceleration)	return DimensionInfo("Acceleration", "m/s2");
	if (dim == this->adim)			return DimensionInfo("Adimensional", "");
	if (dim == this->density)		return DimensionInfo("Density", "kg/m3");
	if (dim == this->angle)		    return DimensionInfo("Angle", "rad");
	if (dim == this->solidAngle)	return DimensionInfo("Solid angle", "sr");
	if (dim == this->electricalCapacitance)		return DimensionInfo("Electrical capacitance", "Farad");
	if (dim == this->electricalConductance)		return DimensionInfo("Electrical conductance", "Siemens");
	if (dim == this->electricalResistance)		return DimensionInfo("Electrical resistance", "Ohms");
	if (dim == this->electricCharge)		    return DimensionInfo("Electrical charge", "Coulombs");
	if (dim == this->electricTension)		    return DimensionInfo("Electrical tension", "Volts");
	if (dim == this->magneticFlux)				return DimensionInfo("Magnetic flux", "Webers");
	if (dim == this->magneticInduction)			return DimensionInfo("Magnetic induction", "Tesla");
	if (dim == this->energy)					return DimensionInfo("Energy", "Joules");
	if (dim == this->entropy)					return DimensionInfo("Entropy", "J/K");
	if (dim == this->flowrate)					return DimensionInfo("Flow rate", "m3/s");
	if (dim == this->force)						return DimensionInfo("Force", "Newtons");
	if (dim == this->frequency)					return DimensionInfo("Frequency", "Hertz");
	if (dim == this->heatTranfertCoefficient)	return DimensionInfo("Heat transfer coefficient", "W/m2/K");
	if (dim == this->heatFlux)				return DimensionInfo("Heat flux", "W/m2");
	if (dim == this->inductance)			return DimensionInfo("Inductance", "Henrys");
	if (dim == this->intensity)				return DimensionInfo("Electrical intensity", "Amperes");
	if (dim == this->kinematicViscosity)	return DimensionInfo("Kinematic viscosity", "m2/s");
	if (dim == this->length)				return DimensionInfo("Length", "meters");
	if (dim == this->mass)					return DimensionInfo("Mass", "kg");
	if (dim == this->massFlowrate)			return DimensionInfo("Mass flow rate", "kg/s");
	if (dim == this->power)					return DimensionInfo("Power", "Watts");
	if (dim == this->pressure)				return DimensionInfo("Pressure", "Pascals");
	if (dim == this->specificHeat)			return DimensionInfo("Specific heat", "J/kg/K");
	if (dim == this->stdFlowrate)			return DimensionInfo("Standard flow rate", "Sm3/s");
	if (dim == this->stdVolume)				return DimensionInfo("Standard volume", "Sm3");
	if (dim == this->surface)				return DimensionInfo("Surface", "m2");
	if (dim == this->surfaceTension)		return DimensionInfo("Surface tension", "N/m");
	if (dim == this->temperature)			return DimensionInfo("Temperature", "°K");
	if (dim == this->thermalConductivity)	return DimensionInfo("Thermal conductivity", "W/m/K");
	if (dim == this->time)			return DimensionInfo("Time", "seconds");
	if (dim == this->velocity)		return DimensionInfo("Velocity", "m/s");
	if (dim == this->viscosity)		return DimensionInfo("Dynamic viscosity", "Poiseuilles");
	if (dim == this->volume)		return DimensionInfo("Volume", "m3");

	if (dim == this->mole)			return DimensionInfo("Mole", "mol");

	if (dim == this->luminousIntensity) return DimensionInfo("Luminous intensity", "cd");
	if (dim == this->illuminance)		return DimensionInfo("Illuminance", "lux");
	if (dim == this->luminance)		    return DimensionInfo("Luminance", "cd/m2");
	if (dim == this->luminousEfficacy)	return DimensionInfo("Luminous efficacy", "lumen/W");
	if (dim == this->luminousEnergy)	return DimensionInfo("Luminous energy", "lumen*s");
	if (dim == this->luminousEnergyDensity)	return DimensionInfo("Luminous energy density", "lumen*s/m3");
	if (dim == this->luminousExposure)	return DimensionInfo("Luminous exposure", "lux/s");
	if (dim == this->luminousPower)	    return DimensionInfo("Luminous power", "lumen");

	if (dim == this->mass - this->mole)	    return DimensionInfo("Molar mass", "kg/mol");
	if (dim == this->mole - this->volume)	return DimensionInfo("Molar concentration", "mol/m3");

	if (dim == this->informationContent)	return DimensionInfo("Quantity of information", "bit");

	return DimensionInfo(nullptr, nullptr);
}

aVect<aVect<UnitCompStrlen>> g_sortedUnitArray;

void InitSortedUnitArray() {

	static aVect<DimConversion> dimConv;

	DimensionTypes dimTypes;

	DimConversion
		ucLength(lengthUnits, lengthUnitsFullName, lengthUnitsFullNamePlural, lengthConvFact, NUMEL(lengthUnits), dimTypes.length),
		ucVolume(volumeUnits, volumeUnitsFullName, volumeUnitsFullNamePlural, volumeConvFact, NUMEL(volumeUnits), dimTypes.volume),
		ucTime(timeUnits, timeUnitsFullName, timeUnitsFullNamePlural, timeConvFact, NUMEL(timeUnits), dimTypes.time),
		ucStdFlowrate(stdFlowrateUnits, stdFlowrateUnitsFullName, stdFlowrateUnitsFullNamePlural, stdFlowrateConvFact, NUMEL(stdFlowrateUnits), dimTypes.stdFlowrate),
		ucFlowrate(flowrateUnits, flowrateUnitsFullName, flowrateUnitsFullNamePlural, flowrateConvFact, NUMEL(flowrateUnits), dimTypes.flowrate),
		ucMass(massUnits, massUnitsFullName, massUnitsFullNamePlural, massConvFact, NUMEL(massUnits), dimTypes.mass),
		ucPressure(pressureUnits, pressureUnitsFullName, pressureUnitsFullNamePlural, pressureConvFact, NUMEL(pressureUnits), dimTypes.pressure),
		ucTemperature(temperatureUnits, temperatureUnitsFullName, temperatureUnitsFullNamePlural, temperatureConvFact, NUMEL(temperatureUnits), dimTypes.temperature, temperatureConvOffset),
		ucIntensity(intensityUnits, intensityUnitsFullName, intensityUnitsFullNamePlural, intensityConvFact, NUMEL(intensityUnits), dimTypes.intensity),
		ucViscosity(viscosityUnits, viscosityUnitsFullName, viscosityUnitsFullNamePlural, viscosityConvFact, NUMEL(viscosityUnits), dimTypes.viscosity),
		ucKinematicViscosity(kinematicViscosityUnits, kinematicViscosityUnitsFullName, kinematicViscosityUnitsFullNamePlural, kinematicViscosityConvFact, NUMEL(kinematicViscosityUnits), dimTypes.kinematicViscosity),
		ucEnergy(energyUnits, energyUnitsFullName, energyUnitsFullNamePlural, energyConvFact, NUMEL(energyUnits), dimTypes.energy),
		ucElectricTension(electricTensionUnits, electricTensionUnitsFullName, electricTensionUnitsFullNamePlural, electricTensionConvFact, NUMEL(electricTensionUnits), dimTypes.electricTension),
		ucFrequency(frequencyUnits, frequencyUnitsFullName, frequencyUnitsFullNamePlural, frequencyConvFact, NUMEL(frequencyUnits), dimTypes.frequency),
		ucPower(powerUnits, powerUnitsFullName, powerUnitsFullNamePlural, powerConvFact, NUMEL(powerUnits), dimTypes.power),
		ucForce(forceUnits, forceUnitsFullName, forceUnitsFullNamePlural, forceConvFact, NUMEL(forceUnits), dimTypes.force),
		ucmagneticInduction(magneticInductionUnits, magneticInductionUnitsFullName, magneticInductionUnitsFullNamePlural, magneticInductionConvFact, NUMEL(magneticInductionUnits), dimTypes.magneticInduction),
		ucmagneticFlux(magneticFluxUnits, magneticFluxUnitsFullName, magneticFluxUnitsFullNamePlural, magneticFluxConvFact, NUMEL(magneticFluxUnits), dimTypes.magneticFlux),
		ucElectricalResistance(electricalResistanceUnits, electricalResistanceUnitsFullName, electricalResistanceUnitsFullNamePlural, electricalResistanceConvFact, NUMEL(electricalResistanceUnits), dimTypes.electricalResistance),
		ucElectricalCapacitance(electricalCapacitanceUnits, electricalCapacitanceUnitsFullName, electricalCapacitanceUnitsFullNamePlural, electricalCapacitanceConvFact, NUMEL(electricalCapacitanceUnits), dimTypes.electricalCapacitance),
		ucElectricalConductance(electricalConductanceUnits, electricalConductanceUnitsFullName, electricalConductanceUnitsFullNamePlural, electricalConductanceConvFact, NUMEL(electricalConductanceUnits), dimTypes.electricalConductance),
		ucInductance(inductanceUnits, inductanceUnitsFullName, inductanceUnitsFullNamePlural, inductanceConvFact, NUMEL(inductanceUnits), dimTypes.inductance),
		ucElectricCharge(electricChargeUnits, electricChargeUnitsFullName, electricChargeUnitsFullNamePlural, electricChargeConvFact, NUMEL(electricChargeUnits), dimTypes.electricCharge),
		ucAdim(adimUnits, adimUnitsFullName, adimUnitsFullNamePlural, adimConvFact, NUMEL(adimUnits), dimTypes.adim),
		ucAngle(angleUnits, angleUnitsFullName, angleUnitsFullNamePlural, angleConvFact, NUMEL(angleUnits), dimTypes.angle),
		ucSolidAngle(solidAngleUnits, solidAngleUnitsFullName, solidAngleUnitsFullNamePlural, solidAngleConvFact, NUMEL(solidAngleUnits), dimTypes.solidAngle),
		ucLuminousIntensity(luminousIntensityUnits, luminousIntensityUnitsFullName, luminousIntensityUnitsFullNamePlural, luminousIntensityConvFact, NUMEL(luminousIntensityUnits), dimTypes.luminousIntensity),
		ucLuminousPower(luminousPowerUnits, luminousPowerUnitsFullName, luminousPowerUnitsFullNamePlural, luminousPowerConvFact, NUMEL(luminousPowerUnits), dimTypes.luminousPower),
		ucIlluminancePower(illuminanceUnits, illuminanceUnitsFullName, illuminanceUnitsFullNamePlural, illuminanceConvFact, NUMEL(illuminanceUnits), dimTypes.illuminance),
		ucMole(moleUnits, moleUnitsFullName, moleUnitsFullNamePlural, moleConvFact, NUMEL(moleUnits), dimTypes.mole),
		ucInformation(informationUnits, informationUnitsFullName, informationUnitsFullNamePlural, informationConvFact, NUMEL(informationUnits), dimTypes.informationContent);

	dimConv.Push(std::move(ucLength));
	dimConv.Push(std::move(ucVolume));
	dimConv.Push(std::move(ucTime));
	dimConv.Push(std::move(ucStdFlowrate));
	dimConv.Push(std::move(ucFlowrate));
	dimConv.Push(std::move(ucMass));
	dimConv.Push(std::move(ucPressure));
	dimConv.Push(std::move(ucTemperature));
	dimConv.Push(std::move(ucIntensity));
	dimConv.Push(std::move(ucViscosity));
	dimConv.Push(std::move(ucKinematicViscosity));
	dimConv.Push(std::move(ucEnergy));
	dimConv.Push(std::move(ucElectricTension));
	dimConv.Push(std::move(ucFrequency));
	dimConv.Push(std::move(ucPower));
	dimConv.Push(std::move(ucForce));
	dimConv.Push(std::move(ucmagneticInduction));
	dimConv.Push(std::move(ucmagneticFlux));
	dimConv.Push(std::move(ucElectricalResistance));
	dimConv.Push(std::move(ucElectricalCapacitance));
	dimConv.Push(std::move(ucElectricalConductance));
	dimConv.Push(std::move(ucInductance));
	dimConv.Push(std::move(ucElectricCharge));
	dimConv.Push(std::move(ucAdim));
	dimConv.Push(std::move(ucAngle));

	dimConv.Push(std::move(ucSolidAngle));
	dimConv.Push(std::move(ucLuminousIntensity));
	dimConv.Push(std::move(ucLuminousPower));
	dimConv.Push(std::move(ucIlluminancePower));
	dimConv.Push(std::move(ucMole));

	dimConv.Push(std::move(ucInformation));

	BinaryHeap<UnitCompStrlen, false> heap;

	for (int nTry = 0; nTry < 3; nTry++) {

		heap.Reset();

		aVect_static_for(dimConv, i) {
			auto & uConv = dimConv[i].uConv;
			aVect_static_for(uConv, j) {
				//special cases to deal with colisions between units and prefixes:
				// 'm' for "meter" and for "milli"
				// 's' for "second" and for "standard"
				// 'G' for "Gauss" and for "Giga"
				// 'S' for "Siemens" and for "Standard"
				// 'mi' for "miles" and for "milli, micro"
				// 'd' for day and 'deci'
				//They should be tested last,
				// and 's' after 'm' => -1
				if (uConv[j].unit[0] == 'm' && uConv[j].unit[1] == 0) {
					UnitCompStrlen expt;
					expt.uConv = &uConv[j];
					expt.strLength = -1 - nTry;
					expt.fullName = false;
					heap.Push(expt);
				}
				else if (uConv[j].unit[0] == 'd' && uConv[j].unit[1] == 0) {
					UnitCompStrlen expt;
					expt.uConv = &uConv[j];
					expt.strLength = -1 - nTry;
					expt.fullName = false;
					heap.Push(expt);
				}
				else if (uConv[j].unit[0] == 's' && uConv[j].unit[1] == 0) {
					UnitCompStrlen expt;
					expt.uConv = &uConv[j];
					expt.strLength = -2;
					expt.fullName = false;
					heap.Push(expt);
				}
				else if (uConv[j].unit[0] == '1' && uConv[j].unit[1] == 0) {
					UnitCompStrlen expt;
					expt.uConv = &uConv[j];
					expt.strLength = -10;
					expt.fullName = false;
					heap.Push(expt);
					continue;
				}
				else if (uConv[j].unit[0] == 'S' && uConv[j].unit[1] == 0) {
					UnitCompStrlen expt;
					expt.uConv = &uConv[j];
					expt.strLength = -2;
					expt.fullName = false;
					heap.Push(expt);
				}
				else if (uConv[j].unit[0] == 'G' && uConv[j].unit[1] == 0) {
					UnitCompStrlen expt;
					expt.uConv = &uConv[j];
					expt.strLength = -(int)nTry;
					expt.fullName = false;
					heap.Push(expt);
				}
				else if (uConv[j].unit[0] == 'm' && uConv[j].unit[1] == 'i' && uConv[j].unit[2] == 0) {
					UnitCompStrlen expt;
					expt.uConv = &uConv[j];
					expt.strLength = -(int)nTry;
					expt.fullName = false;
					heap.Push(expt);
				}
				else heap.Push(UnitCompStrlen(uConv[j]));

				heap.Push(UnitCompStrlen(uConv[j], true));
				heap.Push(UnitCompStrlen(uConv[j], true, true));
			}
		}

		auto&& ua = g_sortedUnitArray.Push().Last();

		while (heap) {
			ua.Push(heap.Pop());
		}
	}
}

bool UnitConverterCore::TranslateParenthesisAndStuff(aVect<char> & str) {

	size_t len = strlen(str);

	if (len == 0) return true;

	//replace '.' by '*' if not in a number
	for (size_t i = 1; i < len - 1; ++i) {
		if (str[i] == '.' && (IsAlpha(str[i - 1]) || IsAlpha(str[i + 1]))) {
			str[i] = '*';
		}
	}

	ReplaceStr(str, "^", "");
	len = strlen(str);

	for (size_t i = 0; i < len; ++i) {
		if (str[i] == ')' && i + 1 < len) {
			if (IsNumber(&str[i + 1], false)) {
				char * endExposant;
				double exposant = strtod(&str[i + 1], &endExposant);
				size_t exposantStrlen = endExposant - &str[i + 1];
				int innerParenthesisCount = 0;
				for (size_t j = i - 1; j < len; --j) {
					if (str[j] == ')') innerParenthesisCount++;
					else if (str[j] == '(') {
						if (innerParenthesisCount != 0) {
							innerParenthesisCount--;
						}
						else {
							for (size_t k = j + 1; k < i; ++k) {
								if (IsNumber(&str[k], false)) {
									char * end;
									double val = strtod(&str[k], &end);
									aVect<char> newVal(NiceFloat(val * exposant, 1e-8));
									size_t numLength = end - &str[k];
									size_t newNumLength = strlen(newVal);
									for (size_t l = 0; l < newNumLength; ++l) {
										if (l < numLength) {
											str[k + l] = newVal[l];
										}
										else {
											str.Insert(k + l, newVal[l]); //k+numLength+l-numLength
										}
									}
									if (newNumLength < numLength) {//remove surplus chars
										for (size_t l = newNumLength; l < numLength; ++l) {
											str.Remove(k + newNumLength);
										}
									}

									size_t diffLen = newNumLength - numLength;
									k += newNumLength;
									i += diffLen;
									len += diffLen;
								}
								else if (str[k] == '*' || str[k] == '/' || k + 1 == i) {
									for (size_t l = (k + 1 == i ? k : k - 1); l < len; --l) {
										if (IsAlpha(str[l])) {
											aVect<char> exposantStr(NiceFloat(exposant, 1e-8));
											size_t niceExposantStrLen = strlen(exposantStr);
											for (size_t m = 0; m < niceExposantStrLen; ++m) {
												str.Insert(l + m + 1, exposantStr[m]);
											}
											len += niceExposantStrLen;
											k += niceExposantStrLen;
											i += niceExposantStrLen;
											break;
										}
										else if (!IsSpace(str[l])) break;
									}
								}
							}

							for (size_t k = 0; k < exposantStrlen; ++k) {
								str.Remove(i + 1);
							}

							len -= exposantStrlen;

							break;
						}
					}
				}

			}
			else {
				//MY_ERROR("Syntax error");
				//MessageBox(NULL, "Something is wrong with the exponent after parenthesis", "Syntax error", MB_ICONEXCLAMATION);
				//return false;
			}
		}
	}

	for (size_t i = 0; i < len; ++i) {
		if (str[i] == ')') {
			size_t n_op = 0;
			size_t last_j;
			bool found_matching_parenthesis = false;
			bool removed_matching_parenthesis = false;
			for (size_t j = i - 1; j < len; --j) {
				last_j = j;
				bool removeParenthesisAndBreak = false;
				if (str[j] == '*' || str[j] == '/') {
					n_op++;
				}
				if (str[j] == '(') {
					found_matching_parenthesis = true;
				}
				if (str[j] == '(' && j > 0 && str[j - 1] == '/') {
					for (size_t k = j + 1; k < i; ++k) {
						if (str[k] == '*') str[k] = '/';
						else if (str[k] == '/') str[k] = '*';
					}
					removeParenthesisAndBreak = true;
				}
				else if (str[j] == '(' && (j == 0 || str[j - 1] == '*')) {
					removeParenthesisAndBreak = true;
				}

				if (removeParenthesisAndBreak) {
					if (!found_matching_parenthesis) MY_ERROR("parenthesis parsing bug");
					str.Remove(i).Remove(j);
					len -= 2;
					i -= 2;
					removed_matching_parenthesis = true;
					break;
				}
			}
			if (!removed_matching_parenthesis && (last_j == 0 || n_op == 0) && found_matching_parenthesis) {
				str.Remove(i).Remove(last_j);
				len -= 2;
				i -= 2;
				break;
			}
		}
	}

	return true;
}

bool UnitConverterCore::ScanPrefix(
	char *& str,
	const aVect<char> & pv_unit,
	int & standard_conditions,
	int & rewind,
	aVect<char> & prefixFullName,
	aVect<char> & prefixFullName_stdCond,
	double & multiplier,
	double & multiplier_stdCond,
	bool scan_multiplier_only_if_standard_condition)
{

	if (standard_conditions) return true;

	//standard conditions
	if (str >= (char*)pv_unit + 1) {
		if (str[-1] == 's' || str[-1] == 'S') {
			if (standard_conditions == 1) return false;//MY_ERROR("Standard conditions specified twice");
			standard_conditions = 1;
			str--;
			rewind++;
			if (prefixFullName) prefixFullName.Prepend("-");
			prefixFullName.Prepend("standard");
		}
	}

	if (scan_multiplier_only_if_standard_condition && standard_conditions == 0) return true;

	for (int i = 6; i > 0; --i) {
		if (str >= (char*)pv_unit + i) {
			for (size_t j = 0; j < NUMEL(multipliersFullName); ++j) {
				if (strlen(multipliersFullName[j]) == i && strstr_caseInsensitive(str - i, multipliersFullName[j]) == str - i) {

					aVect<char> & prefixFullName_ref = standard_conditions ? prefixFullName_stdCond : prefixFullName;

					if (standard_conditions) multiplier_stdCond = multiplierVal[j];
					else multiplier = multiplierVal[j];

					str -= i;
					rewind += i;
					if (prefixFullName_ref) prefixFullName_ref.Prepend("-");
					prefixFullName_ref.Prepend("%s", multipliersFullName[j]);
					return true;
				}
			}
		}
	}

	for (int i = 2; i > 0; --i) {
		if (str >= (char*)pv_unit + i) {
			for (size_t j = 0; j < NUMEL(multipliers); ++j) {
				if (strlen(multipliers[j]) == i && strstr(str - i, multipliers[j]) == str - i) {

					aVect<char> & prefixFullName_ref = standard_conditions ? prefixFullName_stdCond : prefixFullName;

					if (standard_conditions) multiplier_stdCond = multiplierVal[j];
					else multiplier = multiplierVal[j];

					str -= i;
					rewind += i;
					if (prefixFullName_ref) prefixFullName_ref.Prepend("-");
					prefixFullName_ref.Prepend("%s", multipliersFullName[j]);
					return true;
				}
			}
		}
	}

	return true;
}

SI_PhysicalValue UnitConverterCore::ToSI(
	ValueAndUnit & pv,
	bool inv,
	size_t nTry,
	size_t lastReplaceOffset,
	LogFile * pLogFile)
{

	SI_PhysicalValue retVal;

	ToSI(retVal, pv, inv, nTry, lastReplaceOffset, pLogFile);

	return retVal;
}

#pragma optimize("", off) 
void COMPILERBUG_WORKAROUND(size_t K, char * str, size_t & charsLeft) {
	for (size_t k = 0; k < K; ++k) {
		str[k] = 127; //Delete
		charsLeft--;
	}
}
#pragma optimize("", on) 

void UnitConverterCore::ToSI_COMPILERBUG(
	SI_PhysicalValue & retVal,
	ValueAndUnit & pv,
	bool inv,
	size_t nTry,
	size_t lastReplaceOffset,
	LogFile * pLogFile)
{
	
	pv.unit.RemoveBlanks();

	double value = pv.value;
	double value_with_offset = pv.value;
	static aVect<char> unitCopy;

	unitCopy = pv.unit;

	Dimension dim;
	static aVect<char> fullNameUnit, fullNameUnitPlural;
	static aVect<char> origFullUnitStr;
	static ValueAndUnit valueAndUnit;

	fullNameUnit.Redim(0);
	fullNameUnitPlural.Redim(0);
	origFullUnitStr = pv.unit;

	if (!TranslateParenthesisAndStuff(pv.unit)) {
		//retVal.error = false;
		//retVal.orig_value = pv.value;
		//retVal.SI_value = 0;
		//retVal.dim = Dimension();
		//retVal.orig_unitStr = origFullUnitStr;
		//retVal.orig_unitStrFullName = fullNameUnit;
		//retVal.orig_unitStrFullNamePlural = fullNameUnitPlural;
		////return SI_PhysicalValue(pv.value, 0, Dimension(), origFullUnitStr, fullNameUnit, fullNameUnitPlural);
		retVal.ReuseCtor(pv.value, 0, Dimension(), origFullUnitStr, fullNameUnit, fullNameUnitPlural);
		return;
	}

	if (pv.unit) {

		for (size_t i = 0; i < NUMEL(stringsToReplace_before); ++i) {
			ReplaceStr(pv.unit, stringsToReplace_before[i], replaceWith_before[i], false, true, false);
		}

		if (!g_sortedUnitArray) InitSortedUnitArray();

		size_t charsLeft = strlen(pv.unit);

		static CachedVect<aVect<char> > numerators, denominators, parsedToken, translatedToken;
		static CachedVect<aVect<char> > numeratorsPlural, denominatorsPlural;

		numerators.Redim(0);
		denominators.Redim(0);
		parsedToken.Redim(0);
		translatedToken.Redim(0);

		static aVect<char> pv_unit;
		pv_unit = pv.unit;

		auto&& unitArray = g_sortedUnitArray[nTry];

		for (size_t i = 0; charsLeft && i < unitArray.Count();) {

			auto&& heapMax = unitArray[i];

			char * unitStr = (heapMax.fullName ? (heapMax.plural ? heapMax.uConv->unitFullNamePlural : heapMax.uConv->unitFullName) : heapMax.uConv->unit);
			bool divide = false;
			int standard_conditions = 0;

			static aVect<char> prefixFullName, prefixFullName_stdCond;
			prefixFullName.Redim(0);
			prefixFullName_stdCond.Redim(0);

			char * origStr;
			bool recognizedToken = false;

			if (char * str = origStr = heapMax.fullName ?
				reverse_strstr_caseInsensitive(pv_unit, unitStr) :
				reverse_strstr(pv_unit, unitStr)) {

				int rewind = 0;
				double multiplier = 1, exposant = 1, multiplier_stdCond = 1;

				if (str[0] != '1' || str[1] != 0) {
					if (str >= (char*)pv_unit + 1) {
						if (str[-1] == '-' && !IsNumber(str)) {
							ptrdiff_t d = &str[-1] - (char*)pv_unit;
							pv_unit.Remove(d);
							str--;
							charsLeft--;
						}
					}

					if (!ScanPrefix(str, pv_unit, standard_conditions, rewind, prefixFullName,
						prefixFullName_stdCond, multiplier, multiplier_stdCond))
					{
						goto continueParse;
					};
				}

				if (!ScanPrefix(str, pv_unit, standard_conditions, rewind, prefixFullName,
					prefixFullName_stdCond, multiplier, multiplier_stdCond, true))
				{
					goto continueParse;
				}

				size_t unitStrLen = strlen(unitStr);
				static aVect<char> strBeforeDelete;
				strBeforeDelete = str;

				if (str >= (char*)pv_unit + 1) {
					if (str[-1] == '/') {
						divide = true;
					}
					else if (str[-1] != '*') {
						goto continueParse;
					}
					str[-1] = 127; //Delete
					charsLeft--;
				}


				for (size_t k = 0, K = unitStrLen + rewind; k < K; ++k) {
					str[k] = 127; //Delete
					charsLeft--;
				}
				//COMPILERBUG_WORKAROUND(unitStrLen + rewind, str, charsLeft);

				static aVect<char> strAfterDelete;
				strAfterDelete = str;

				char * strEndExposant = str = origStr + unitStrLen;

				char * pvUnitEnd = (char*)pv_unit + strlen(pv_unit);
				while (strEndExposant < pvUnitEnd &&
					(IsDigit(*strEndExposant) ||
						*strEndExposant == '-' ||
						*strEndExposant == '^' ||
						*strEndExposant == '.')) strEndExposant++;

				if (str < strEndExposant) {
					static aVect<char> aExp;

					aExp.Redim(strEndExposant - str);
					bool containsDigits = false;
					aVect_static_for(aExp, i) {
						aExp[i] = str[i];
						if (i > 0 && aExp[i] == '^') goto continueParse;
						if (i > 0 && aExp[i] == '-' && aExp[i - 1] != '^') goto continueParse;
						if (IsDigit(aExp[i])) containsDigits = true;
					}
					if (!containsDigits) goto continueParse;


					aVect_static_for(aExp, i) {
						str[i] = 127; //Delete;
						charsLeft--;
					}

					//COMPILERBUG_WORKAROUND(aExp.Count(), str, charsLeft);

					if (aExp[0] == '^') aExp.Remove(0);
					aExp.Push(0);
					exposant = atof(aExp);
				}

				if (divide) exposant *= -1, standard_conditions *= -1;

				Dimension additionalDimensions(0, 0, 0, 0, 0, standard_conditions, 0, 0, 0, 0);

				dim += exposant * (heapMax.uConv->dim) + additionalDimensions;
				double convFact = multiplier_stdCond * pow(multiplier * heapMax.uConv->convFact, exposant);

				value = inv ? (value / convFact) : (value * convFact);

				//value_with_offset makes sense only if converting "pure" temperatures
				if (inv) {
					if (divide) MY_ERROR("Non implemented");
					value_with_offset = 1 / multiplier * (value_with_offset - heapMax.uConv->convOffset) / heapMax.uConv->convFact;
				}
				else {
					value_with_offset = 1 / multiplier *
						(divide ?
							value_with_offset / heapMax.uConv->convFact :
							value_with_offset * heapMax.uConv->convFact)
						+ heapMax.uConv->convOffset;
				}

				auto * pStrVect = exposant > 0 ? &numerators : &denominators;
				auto * pStrVectPlural = exposant > 0 ? &numeratorsPlural : &denominatorsPlural;
				auto & string = pStrVect->Grow(1).Last();
				auto & stringPlural = pStrVectPlural->Grow(1).Last();

				if (prefixFullName_stdCond) string.Append("%s-", (char*)prefixFullName_stdCond), stringPlural.Append("%s-", (char*)prefixFullName_stdCond);
				if (abs(exposant) != 1) string.Append("("), stringPlural.Append("(");
				if (prefixFullName) string.Append("%s-", (char*)prefixFullName), stringPlural.Append("%s-", (char*)prefixFullName);
				string.Append("%s", (char*)heapMax.uConv->unitFullName);
				stringPlural.Append("%s", (char*)heapMax.uConv->unitFullNamePlural);


				if (abs(exposant) != 1) {

					static aVect<char> niceFloat;
					NiceFloat(niceFloat, abs(exposant), 1e-6);

					string.Append(")^%s", (char*)niceFloat);
					stringPlural.Append(")^%s", (char*)niceFloat);
				}

				static aVect<char> analysedToken;
				analysedToken = strBeforeDelete;

				aVect_static_for(analysedToken, j) {
					if (strAfterDelete[j] != 127 || strBeforeDelete[j] == 127) {
						analysedToken[j] = 0;
						break;
					}
				}

				parsedToken.Push(analysedToken);
				translatedToken.Push(string);

				recognizedToken = true;
			}
		continueParse:;
			if (!recognizedToken) i++;
		}

	}

	//return SI_PhysicalValue(pv.value, value, dim, origFullUnitStr, fullNameUnit, fullNameUnitPlural);
	retVal.ReuseCtor(pv.value, value, dim, origFullUnitStr, fullNameUnit, fullNameUnitPlural);
	return;
	

	//return SI_PhysicalValue();//unreachable code, to avoid compiler warnings;
	retVal.ReuseCtor();
	return;
}

void UnitConverterCore::ToSI(
	SI_PhysicalValue & retVal,
	ValueAndUnit & pv,
	bool inv, 
	size_t nTry, 
	size_t lastReplaceOffset,
	LogFile * pLogFile) 
{

	static WinCriticalSection cs;

	Critical_Section(cs) {

		pv.unit.RemoveBlanks();

		double value = pv.value;
		double value_with_offset = pv.value;
		static aVect<char> unitCopy;
		
		unitCopy = pv.unit;

		Dimension dim;
		static aVect<char> fullNameUnit, fullNameUnitPlural;
		static aVect<char> origFullUnitStr;
		static ValueAndUnit valueAndUnit;

		fullNameUnit.Redim(0);
		fullNameUnitPlural.Redim(0);
		origFullUnitStr = pv.unit;

		if (!TranslateParenthesisAndStuff(pv.unit)) {
			//retVal.error = false;
			//retVal.orig_value = pv.value;
			//retVal.SI_value = 0;
			//retVal.dim = Dimension();
			//retVal.orig_unitStr = origFullUnitStr;
			//retVal.orig_unitStrFullName = fullNameUnit;
			//retVal.orig_unitStrFullNamePlural = fullNameUnitPlural;
			////return SI_PhysicalValue(pv.value, 0, Dimension(), origFullUnitStr, fullNameUnit, fullNameUnitPlural);
			retVal.ReuseCtor(pv.value, 0, Dimension(), origFullUnitStr, fullNameUnit, fullNameUnitPlural);
			return;
		}

		if (pv.unit) {

			for (size_t i = 0; i < NUMEL(stringsToReplace_before); ++i) {
				ReplaceStr(pv.unit, stringsToReplace_before[i], replaceWith_before[i], false, true, false);
			}

			if (!g_sortedUnitArray) InitSortedUnitArray();

			size_t charsLeft = strlen(pv.unit);

			static CachedVect<aVect<char> > numerators, denominators, parsedToken, translatedToken;
			static CachedVect<aVect<char> > numeratorsPlural, denominatorsPlural;

			numerators.Redim(0);
			denominators.Redim(0);
			parsedToken.Redim(0);
			translatedToken.Redim(0);

			static aVect<char> pv_unit;
			pv_unit = pv.unit;

			auto&& unitArray = g_sortedUnitArray[nTry];

			for (size_t i = 0; charsLeft && i < unitArray.Count();) {

				auto&& heapMax = unitArray[i];

				char * unitStr = (heapMax.fullName ? (heapMax.plural ? heapMax.uConv->unitFullNamePlural : heapMax.uConv->unitFullName) : heapMax.uConv->unit);
				bool divide = false;
				int standard_conditions = 0;

				static aVect<char> prefixFullName, prefixFullName_stdCond;
				prefixFullName.Redim(0);
				prefixFullName_stdCond.Redim(0);

				char * origStr;
				bool recognizedToken = false;

				if (char * str = origStr = heapMax.fullName ?
					reverse_strstr_caseInsensitive(pv_unit, unitStr) :
					reverse_strstr(pv_unit, unitStr)) {

					int rewind = 0;
					double multiplier = 1, exposant = 1, multiplier_stdCond = 1;

					if (str[0] != '1' || str[1] != 0) {
						if (str >= (char*)pv_unit + 1) {
							if (str[-1] == '-' && !IsNumber(str)) {
								ptrdiff_t d = &str[-1] - (char*)pv_unit;
								pv_unit.Remove(d);
								str--;
								charsLeft--;
							}
						}

						if (!ScanPrefix(str, pv_unit, standard_conditions, rewind, prefixFullName,
							prefixFullName_stdCond, multiplier, multiplier_stdCond))
						{
							goto continueParse;
						};
					}

					if (!ScanPrefix(str, pv_unit, standard_conditions, rewind, prefixFullName,
						prefixFullName_stdCond, multiplier, multiplier_stdCond, true))
					{
						goto continueParse;
					}

					size_t unitStrLen = strlen(unitStr);
					static aVect<char> strBeforeDelete;
					strBeforeDelete = str;

					if (str >= (char*)pv_unit + 1) {
						if (str[-1] == '/') {
							divide = true;
						}
						else if (str[-1] != '*') {
							goto continueParse;
						}
						str[-1] = 127; //Delete
						charsLeft--;
					}

					//for (size_t k = 0, K = unitStrLen + rewind; k < K; ++k) {
					//	str[k] = 127; //Delete
					//	charsLeft--;
					//}
					COMPILERBUG_WORKAROUND(unitStrLen + rewind, str, charsLeft);

					static aVect<char> strAfterDelete;
					strAfterDelete = str;

					char * strEndExposant = str = origStr + unitStrLen;

					char * pvUnitEnd = (char*)pv_unit + strlen(pv_unit);
					while (strEndExposant < pvUnitEnd &&
						(IsDigit(*strEndExposant) ||
						*strEndExposant == '-' ||
						*strEndExposant == '^' ||
						*strEndExposant == '.')) strEndExposant++;

					if (str < strEndExposant) {
						static aVect<char> aExp;

						aExp.Redim(strEndExposant - str);
						bool containsDigits = false;
						aVect_static_for(aExp, i) {
							aExp[i] = str[i];
							if (i > 0 && aExp[i] == '^') goto continueParse;
							if (i > 0 && aExp[i] == '-' && aExp[i - 1] != '^') goto continueParse;
							if (IsDigit(aExp[i])) containsDigits = true;
						}
						if (!containsDigits) goto continueParse;

						//aVect_static_for(aExp, i) {
						//	str[i] = 127; //Delete;
						//	charsLeft--;
						//}

						COMPILERBUG_WORKAROUND(aExp.Count(), str, charsLeft);

						if (aExp[0] == '^') aExp.Remove(0);
						aExp.Push(0);
						exposant = atof(aExp);
					}

					if (divide) exposant *= -1, standard_conditions *= -1;

					Dimension additionalDimensions(0, 0, 0, 0, 0, standard_conditions, 0, 0, 0, 0);

					dim += exposant * (heapMax.uConv->dim) + additionalDimensions;
					double convFact = multiplier_stdCond * pow(multiplier * heapMax.uConv->convFact, exposant);

					value = inv ? (value / convFact) : (value * convFact);

					//value_with_offset makes sense only if converting "pure" temperatures
					if (inv) {
						if (divide) MY_ERROR("Non implemented");
						value_with_offset = 1 / multiplier * (value_with_offset - heapMax.uConv->convOffset) / heapMax.uConv->convFact;
					}
					else {
						value_with_offset = 1 / multiplier *
							(divide ?
							value_with_offset / heapMax.uConv->convFact :
							value_with_offset * heapMax.uConv->convFact)
							+ heapMax.uConv->convOffset;
					}

					auto * pStrVect = exposant > 0 ? &numerators : &denominators;
					auto * pStrVectPlural = exposant > 0 ? &numeratorsPlural : &denominatorsPlural;
					auto & string = pStrVect->Grow(1).Last();
					auto & stringPlural = pStrVectPlural->Grow(1).Last();

					if (prefixFullName_stdCond) string.Append("%s-", (char*)prefixFullName_stdCond), stringPlural.Append("%s-", (char*)prefixFullName_stdCond);
					if (abs(exposant) != 1) string.Append("("), stringPlural.Append("(");
					if (prefixFullName) string.Append("%s-", (char*)prefixFullName), stringPlural.Append("%s-", (char*)prefixFullName);
					string.Append("%s", (char*)heapMax.uConv->unitFullName);
					stringPlural.Append("%s", (char*)heapMax.uConv->unitFullNamePlural);
					

					if (abs(exposant) != 1) {

						static aVect<char> niceFloat;
						NiceFloat(niceFloat, abs(exposant), 1e-6);

						string.Append(")^%s", (char*)niceFloat);
						stringPlural.Append(")^%s", (char*)niceFloat);
					}

					static aVect<char> analysedToken;
					analysedToken = strBeforeDelete;

					aVect_static_for(analysedToken, j) {
						if (strAfterDelete[j] != 127 || strBeforeDelete[j] == 127) {
							analysedToken[j] = 0;
							break;
						}
					}

					parsedToken.Push(analysedToken);
					translatedToken.Push(string);

					recognizedToken = true;
				}
			continueParse:;
				if (!recognizedToken) i++;
			}

			MakeFractionStr(
				fullNameUnit, fullNameUnitPlural,
				numerators, numeratorsPlural,
				denominators, denominatorsPlural, 
				true);

			if (charsLeft) {

				if (nTry == 0 || nTry == 1) {

					valueAndUnit.ReuseCtor(pv.value, unitCopy);
					size_t replaceOffset = 0;// lastReplaceOffset;
					for (int j = 0; j < NUMEL(stringsToReplace); ++j) {
						replaceOffset = 0;// lastReplaceOffset;
						if (nTry == 0) {
							if (replaceOffset = ReplaceStr(valueAndUnit.unit, stringsToReplace[j], replaceWith[j], false, true, false, lastReplaceOffset)) break;
						}
						else if (nTry == 1) {
							if (replaceOffset = ReplaceStr_CaseInsensitive(valueAndUnit.unit, stringsToReplace[j], replaceWith[j], false, lastReplaceOffset)) break;
						}
					}
					//SI_PhysicalValue test = this->ToSI(secondTryPV, inv, replaceOffset ? nTry : nTry + 1, replaceOffset);
					this->ToSI(retVal, valueAndUnit, inv, replaceOffset ? nTry : nTry + 1, replaceOffset);
					if (!retVal.error || nTry > 0 || lastReplaceOffset > 0) {
						//return test;
						return;
					}
					else {

						if (pLogFile) {

							aVect<char *> unknownToken;
							bool inWord = false;

							for (size_t i = 0, I = pv_unit.Count() - 1; i < I; ++i) {
								if (pv_unit[i] == 127 || pv_unit[i] == '/' || pv_unit[i] == '*') {
									pv_unit[i] = 0;
									inWord = false;
								}
								else if (!inWord) {
									unknownToken.Push((char*)pv_unit + i);
									inWord = true;
								}
							}

							aVect<char> errorMsg("Unrecognized tokens in \"%s\":\n", (char*)unitCopy);
							aVect_static_for(unknownToken, i) {
								errorMsg.Append("- \"%s\"\n", unknownToken[i]);
							}

							if (parsedToken) {
								errorMsg.Append("\nRecognized tokens:\n");
								aVect_static_for(parsedToken, i) {
									errorMsg.Append("- \"%s\" : %s\n", (char*)parsedToken[i], (char*)translatedToken[i]);
								}
							}


							errorMsg.Append("\nHint : Abbreviations are case sensitive, for example:\n");
							errorMsg.Append("\"MPa\" = mega-pascal, but \"mPa\" = milli-pascal\n");
							errorMsg.Append("\"kN\" = kilo-Newton, but \"kn\" gives this error\n");

							MessageBoxA(NULL, errorMsg, nullptr, MB_ICONEXCLAMATION);

							pLogFile->DisableDisplay().Printf("\n").PrintDate().PrintTime()
								.Printf("\n[%s] Requested conversion : %g %s\n%s\n", getenv("USERNAME"), pv.value, (char*)unitCopy, (char*)errorMsg)
								.EnableDisplay();
						}

						//return SI_PhysicalValue();
						retVal.ReuseCtor();
						return;
					}
				}
				else {
					//return SI_PhysicalValue();
					retVal.ReuseCtor();
					return;
				}
			}

			if (dim.IsPureTemperature()) value = value_with_offset;
		}

		//return SI_PhysicalValue(pv.value, value, dim, origFullUnitStr, fullNameUnit, fullNameUnitPlural);
		retVal.ReuseCtor(pv.value, value, dim, origFullUnitStr, fullNameUnit, fullNameUnitPlural);
		return;
	}

	//return SI_PhysicalValue();//unreachable code, to avoid compiler warnings;
	retVal.ReuseCtor();
	return;
}

aVect<char> FormatResult(
	char * title,
	char * convFrom,
	char * unitFrom,
	char * unitFullName,
	SI_PhysicalValue & SI_convert,
	bool omitValue) {

	aVect<char> fullTitle;
	aVect<char> underline;
	aVect<char> interpretation;
	aVect<char> SI_unit;
	aVect<char> niceValue;

	DimensionTypesEx dimTypes;


	NiceFloat(niceValue, SI_convert.SI_value, 1e-6);

	if (omitValue)
		fullTitle.sprintf("%s: %s", title, unitFrom);
	else
		fullTitle.sprintf("%s: %s %s", title, convFrom, unitFrom);

	underline.Redim(fullTitle.Count()).Set('-').Last() = 0;
	interpretation.sprintf("    Interpretation: %s", unitFullName);
	auto dimInfo = dimTypes.GetDimensionName(SI_convert.dim);

	if (dimInfo.type) interpretation.Append("\n    Type:           %s", dimInfo.type);
	SI_unit.sprintf("    In SI units:    %s %s = %s %s",
		convFrom, unitFrom, (char*)niceValue, dimInfo.SI_usualUnit ? dimInfo.SI_usualUnit : (char*)SI_convert.SI_unitStr);

	return xFormat("%s\n%s\n%s\n%s\n", fullTitle, underline, interpretation, SI_unit);
}

UnitConverter::UnitConverter() {}
UnitConverter::UnitConverter(const char* unitFrom, const char* unitTo, bool strict) {
	this->SetUnits(unitFrom, unitTo, strict);
}

UnitConverter::ConvState UnitConverter::PreCompute_COMPILERBUG() {
	if (this->state == state_ready) return state_ready;

	if (this->convFromSI && this->convToSI) {
		this->factor = 1;
		this->offset = 0;
		return this->state = state_ready;
	}

	if ((this->convFrom || this->convFromSI) && (this->convTo || this->convToSI)) {

		UnitConverterCore converter;

		double x1 = 1;
		static WinCriticalSection cs;
		Critical_Section(cs) {
			static SI_PhysicalValue SI_convFrom_1;
			static SI_PhysicalValue SI_convTo;
			static ValueAndUnit valueAndUnit;

			if (!this->convFromSI) {
				valueAndUnit.ReuseCtor(x1, this->convFrom);
				converter.ToSI_COMPILERBUG(SI_convFrom_1, valueAndUnit);
				if (SI_convFrom_1.error) return this->state = state_convFrom_error;
			}

			bool isPureTemperature;
			bool SI_convTo_initialized = false;

			if (!this->convFromSI) {
				isPureTemperature = SI_convFrom_1.dim.IsPureTemperature();
			}
			else {
				valueAndUnit.ReuseCtor(1, this->convTo);
				converter.ToSI(SI_convTo, valueAndUnit);
				if (SI_convTo.error) return this->state = state_convTo_error;
				SI_convTo_initialized = true;
				isPureTemperature = SI_convTo.dim.IsPureTemperature();
			}

			if (isPureTemperature) {

				static SI_PhysicalValue c1;
				if (!this->convToSI) {
					valueAndUnit.ReuseCtor(this->convFromSI ? x1 : SI_convFrom_1.SI_value, this->convTo);
					converter.ToSI(c1, valueAndUnit, true);
					if (c1.error) return this->state = state_convTo_error;

					if (SI_convFrom_1.dim != c1.dim && !this->convToSI && !this->convFromSI) {
						return this->state = state_unit_mismatch;
					}
				}

				double x2 = 2;
				static SI_PhysicalValue SI_convFrom_2;
				if (!this->convFromSI) {
					valueAndUnit.ReuseCtor(x2, this->convFrom);
					converter.ToSI(SI_convFrom_2, valueAndUnit);
					if (SI_convFrom_2.error) return this->state = state_convFrom_error;
				}

				static SI_PhysicalValue c2;
				if (!this->convToSI) {
					valueAndUnit.ReuseCtor(this->convFromSI ? x2 : SI_convFrom_2.SI_value, this->convTo);
					converter.ToSI(c2, valueAndUnit, true);
					if (c2.error) return this->state = state_convTo_error;
				}

				double y1 = this->convToSI ? SI_convFrom_1.SI_value : c1.SI_value;
				double y2 = this->convToSI ? SI_convFrom_2.SI_value : c2.SI_value;

				this->factor = (y2 - y1) / (x2 - x1);
				this->offset = y1 - this->factor * x1;
			}
			else {

				if (!SI_convTo_initialized && !this->convToSI) {
					valueAndUnit.ReuseCtor(1, this->convTo);
					converter.ToSI(SI_convTo, valueAndUnit);
					if (SI_convTo.error) return this->state = state_convTo_error;
				}

				if (!this->strict) SI_convFrom_1.dim.standard = SI_convTo.dim.standard = 0;

				if (SI_convFrom_1.dim != SI_convTo.dim && !this->convToSI && !this->convFromSI) return this->state = state_unit_mismatch;

				this->factor = (this->convFromSI ? x1 : SI_convFrom_1.SI_value)
					/
					(this->convToSI ? 1 : SI_convTo.SI_value);

				this->offset = 0;
			}

			return this->state = state_ready;
		}
	}
	return this->state = state_unitialized;
}

UnitConverter::ConvState UnitConverter::PreCompute() {

	if (this->state == state_ready) return state_ready;

	if (this->convFromSI && this->convToSI) {
		this->factor = 1;
		this->offset = 0;
		return this->state = state_ready;
	}

	if ((this->convFrom || this->convFromSI) && (this->convTo || this->convToSI)) {

		UnitConverterCore converter;

		double x1 = 1;
		static WinCriticalSection cs;
		Critical_Section(cs) {
			static SI_PhysicalValue SI_convFrom_1;
			static SI_PhysicalValue SI_convTo;
			static ValueAndUnit valueAndUnit;

			if (!this->convFromSI) {
				valueAndUnit.ReuseCtor(x1, this->convFrom);
				converter.ToSI(SI_convFrom_1, valueAndUnit);
				if (SI_convFrom_1.error) return this->state = state_convFrom_error;
			}

			bool isPureTemperature;
			bool SI_convTo_initialized = false;

			if (!this->convFromSI) {
				isPureTemperature = SI_convFrom_1.dim.IsPureTemperature();
			}
			else {
				valueAndUnit.ReuseCtor(1, this->convTo);
				converter.ToSI(SI_convTo, valueAndUnit);
				if (SI_convTo.error) return this->state = state_convTo_error;
				SI_convTo_initialized = true;
				isPureTemperature = SI_convTo.dim.IsPureTemperature();
			}

			if (isPureTemperature) {

				static SI_PhysicalValue c1;
				if (!this->convToSI) {
					valueAndUnit.ReuseCtor(this->convFromSI ? x1 : SI_convFrom_1.SI_value, this->convTo);
					converter.ToSI(c1, valueAndUnit, true);
					if (c1.error) return this->state = state_convTo_error;

					if (SI_convFrom_1.dim != c1.dim && !this->convToSI && !this->convFromSI) {
						return this->state = state_unit_mismatch;
					}
				}

				double x2 = 2;
				static SI_PhysicalValue SI_convFrom_2;
				if (!this->convFromSI) {
					valueAndUnit.ReuseCtor(x2, this->convFrom);
					converter.ToSI(SI_convFrom_2, valueAndUnit);
					if (SI_convFrom_2.error) return this->state = state_convFrom_error;
				}

				static SI_PhysicalValue c2;
				if (!this->convToSI) {
					valueAndUnit.ReuseCtor(this->convFromSI ? x2 : SI_convFrom_2.SI_value, this->convTo);
					converter.ToSI(c2, valueAndUnit, true);
					if (c2.error) return this->state = state_convTo_error;
				}

				double y1 = this->convToSI ? SI_convFrom_1.SI_value : c1.SI_value;
				double y2 = this->convToSI ? SI_convFrom_2.SI_value : c2.SI_value;

				this->factor = (y2 - y1) / (x2 - x1);
				this->offset = y1 - this->factor * x1;
			}
			else {

				if (!SI_convTo_initialized && !this->convToSI) {
					valueAndUnit.ReuseCtor(1, this->convTo);
					converter.ToSI(SI_convTo, valueAndUnit);
					if (SI_convTo.error) return this->state = state_convTo_error;
				}

				if (!this->strict) SI_convFrom_1.dim.standard = SI_convTo.dim.standard = 0;

				if (SI_convFrom_1.dim != SI_convTo.dim && !this->convToSI && !this->convFromSI) return this->state = state_unit_mismatch;

				this->factor = (this->convFromSI ? x1 : SI_convFrom_1.SI_value)
					/
					(this->convToSI ? 1 : SI_convTo.SI_value);

				this->offset = 0;
			}

			return this->state = state_ready;
		}
	}
	return this->state = state_unitialized;
}

UnitConverter & UnitConverter::Reset() {

	this->convFrom.Redim(0);
	this->convTo.Redim(0);

	this->state  = state_unitialized;
	this->strict = true;

	return *this;
}

double UnitConverter::GetFactor() {
	if (this->IsReady()) {
		return this->factor;
	}
	return std::numeric_limits<double>::signaling_NaN();
}

double UnitConverter::GetOffset() {
	if (this->IsReady()) {
		return this->offset;
	}
	return std::numeric_limits<double>::signaling_NaN();
}

UnitConverter & UnitConverter::SetUnits(const char* unitFrom, const char* unitTo, bool strict) {

	this->convFrom = unitFrom;
	this->convTo = unitTo;
	this->convFromSI = false;
	this->convToSI = false;
	this->strict = strict;
	this->state = ConvState::state_unitialized;
	
	return *this;
}

UnitConverter & UnitConverter::From(const char* unitFrom, bool strict) {
	
	this->convFrom = unitFrom;
	this->convFromSI = false;
	this->strict = strict;
	this->state = ConvState::state_unitialized;

	return *this;
}

UnitConverter & UnitConverter::To(const char* unitTo, bool strict) {
	
	this->convTo = unitTo;
	this->convToSI = false;
	this->strict = strict;
	this->state = ConvState::state_unitialized;

	return *this;
}

UnitConverter & UnitConverter::FromSI(bool strict) {
	this->convFrom.Redim(0);
	this->convFromSI = true;
	this->strict = strict;
	this->state = ConvState::state_unitialized;

	return *this;
}

UnitConverter & UnitConverter::ToSI(bool strict) {
	this->convTo.Redim(0);
	this->convToSI = true;
	this->strict = strict;
	this->state = ConvState::state_unitialized;

	return *this;
}

double UnitConverter::operator()(double value) {
	return this->Convert(value);
}

double UnitConverter::Convert(double value) {

	if (this->IsReady()) {
		return this->factor * value + this->offset;
	}
	else {
		return std::numeric_limits<double>::quiet_NaN();
	}
}

double UnitConverter::Convert_COMPILERBUG(double value) {

	if (this->IsReady_COMPILERBUG()) {
		return this->factor * value + this->offset;
	}
	else {
		return std::numeric_limits<double>::quiet_NaN();
	}
}

void UnitConverter::Convert_InPlace(aVect<double>& values) {

	if (this->IsReady()) {
		for (auto&& v : values) {
			v = this->factor * v + this->offset;
		}
	}
	else {
		MY_ERROR("conversion error");
	}
}

aVect<double> UnitConverter::Convert(const aVect<double>& v) {

	if (this->IsReady()) {

		aVect<double> retVal(v.Count());

		aVect_static_for(v, i) {
			retVal[i] = this->factor * v[i] + this->offset;
		}

		return retVal;
	}
	else {
		return aVect<double>();
	}
}

bool UnitConverter::IsReady() {

	if (this->state == UnitConverter::state_unitialized) {
		this->PreCompute();
	}
	return this->state == state_ready;
}

bool UnitConverter::IsReady_COMPILERBUG() {

	if (this->state == UnitConverter::state_unitialized) {
		this->PreCompute_COMPILERBUG();
	}
	return this->state == state_ready;
}

UnitConverter::ConvState UnitConverter::State() {

	return this->state;
}

aVect<char> UnitConverter::GetError() {
	
	switch (this->state) {
		case UnitConverter::state_unitialized:
			//return xFormat("Unitialized");
			/*fallthrough*/
		case UnitConverter::state_ready:
			return aVect<char>();
		case UnitConverter::state_unit_mismatch:
			return xFormat("Dimension mismatch (\"%s\" to \"%s\")", this->convFrom, this->convTo);
		case UnitConverter::state_convFrom_error:
			return xFormat("Unrecognized source unit \"%s\"", this->convFrom);
		case UnitConverter::state_convTo_error:
			return xFormat("Unrecognized destination unit \"%s\"", this->convTo);
		default:
			MY_ERROR("wrong state");
			return aVect<char>();
	}
}

aVect<char> UnitFullName(const char * str) {
	
	UnitConverterCore converterEx;

	return std::move(converterEx.ToSI(ValueAndUnit(0, str)).orig_unitStrFullNamePlural);
}

aVect<char> UnitConverter::FullName_To() {

	return UnitFullName(this->convTo);
}

aVect<char> UnitConverter::FullName_From() {

	return UnitFullName(this->convFrom);
}

double ToSI(const double & value, const char * unit) {

	return UnitConverter().From(unit).ToSI().Convert(value);
}


double FromSI_To(const char * unit, const double & SI_value) {

	return UnitConverter().To(unit).FromSI().Convert(SI_value);
}

