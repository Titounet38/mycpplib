
#ifndef _CONVERTER_
#define _CONVERTER_

#include "xVect.h"
#include "MyUtils.h"

namespace Dim {
	enum { length, time, mass, temperature, intensity };
}

struct Dimension {

	double length, time, mass, temperature, intensity, standard, angle, mole, luminousIntensity, information;

	Dimension();
	Dimension(const Dimension & d);
	Dimension(double pLength, double pTime, 
		double pMass,  double pTemperature, 
		double pIntensity, double pStandard, 
		double pAngle, double pMole, 
		double pLuminousIntensity,
		double information);

	bool IsPureTemperature();
	Dimension & Copy(Dimension & d);
	bool operator==(Dimension & d);
	bool operator==(double val);
	bool operator!=(double val);
	bool operator!=(Dimension & d);
	Dimension operator+(Dimension & d);
	Dimension operator-(Dimension & d);
	Dimension & operator+=(Dimension & d);
	Dimension & operator-=(Dimension & d);
	aVect<char> ToString();
};

struct SI_PhysicalValue {

	double SI_value;
	double orig_value;
	Dimension dim;
	aVect<char> SI_unitStr;
	aVect<char> orig_unitStr;
	aVect<char> orig_unitStrFullName;
	aVect<char> orig_unitStrFullNamePlural;

	bool error;

	SI_PhysicalValue();
	SI_PhysicalValue(const SI_PhysicalValue &) = default;
	SI_PhysicalValue(SI_PhysicalValue &&) = default;

	SI_PhysicalValue& operator=(const SI_PhysicalValue &) = default;
	SI_PhysicalValue& operator=(SI_PhysicalValue &&) = default;

	SI_PhysicalValue(double orig_v, double SI_v, Dimension & d, 
		char * orig_uS, char * orig_uSFN, char * orig_uSFNP);

	void ReuseCtor(double orig_v, double SI_v, Dimension & d,
		char * orig_uS, char * orig_uSFN, char * orig_uSFNP);

	void ReuseCtor();

	void MakeExposantStr(aVect<char> & str, double val, double tol = 1e-6);

	void MakeDimStr(
		CachedVect< aVect<char> > & numerators,
		CachedVect< aVect<char> > & denominators,
		char * str, double d);

	void MakeUnitStr();
};

struct DimensionInfo {
	char * type;
	char * SI_usualUnit;

	DimensionInfo(char * t, char * SIuu);
};

struct DimensionTypes {

	Dimension length, time, mass, temperature, intensity, surface, volume,
		stdVolume, velocity, acceleration, force, pressure, flowrate,
		stdFlowrate, massFlowrate, density, energy, power, entropy,
		viscosity, electricTension, frequency, magneticInduction, magneticFlux,
		electricalResistance, electricalCapacitance, electricalConductance,
		inductance, electricCharge, adim, kinematicViscosity, angle, solidAngle, surfaceTension, mole,
		luminousIntensity, luminousPower, luminance, illuminance, luminousEnergy, 
		luminousExposure, luminousEfficacy, luminousEnergyDensity, informationContent;

	DimensionTypes();
};

struct DimensionTypesEx : DimensionTypes {

	Dimension thermalConductivity, heatTranfertCoefficient, density, specificHeat, heatFlux;

	DimensionTypesEx();

	DimensionInfo GetDimensionName(Dimension & dim);
};


struct ValueAndUnit {
	double value;
	aVect<char> unit;

	ValueAndUnit() = default;
	ValueAndUnit(double v, const char * u);
	void ReuseCtor(double v, const char * u);
};

class UnitConverterCore {

	static bool TranslateParenthesisAndStuff(aVect<char> & str);

	bool ScanPrefix(
		char *& str,
		const aVect<char> & pv_unit,
		int & standard_conditions,
		int & rewind,
		aVect<char> & prefixFullName,
		aVect<char> & prefixFullName_stdCond,
		double & multiplier,
		double & multiplier_stdCond,
		bool scan_multiplier_only_if_standard_condition = false);

public:
	SI_PhysicalValue ToSI(
		ValueAndUnit & pv,
		bool inv = false,
		size_t nTry = 0,
		size_t lastReplaceOffset = 0,
		LogFile * pLogFile = nullptr);

	void ToSI(
		SI_PhysicalValue & retVal,
		ValueAndUnit & pv,
		bool inv = false,
		size_t nTry = 0,
		size_t lastReplaceOffset = 0,
		LogFile * pLogFile = nullptr);

};

aVect<char> FormatResult(
	char * title,
	char * convFrom,
	char * unitFrom,
	char * unitFullName,
	SI_PhysicalValue & SI_convert,
	bool omitValue);

class UnitConverter {

	enum ConvState {
		state_unitialized, state_ready_for_precompute, state_ready, 
		state_unit_mismatch, state_convFrom_error, state_convTo_error };

	aVect<char> convFrom;
	aVect<char> convTo;

	ConvState state = state_unitialized;

	bool convFromSI = false;
	bool convToSI = false;
	bool strict = true;

	double factor;
	double offset;

	ConvState PreCompute();

public:

	UnitConverter();
	UnitConverter(const char* unitFrom, const char* unitTo, bool strict = true);
	UnitConverter & Reset();
	UnitConverter & FromSI(bool strict = true);
	UnitConverter & ToSI(bool strict = true);
	UnitConverter & From(const char* unitFrom, bool strict = true);
	UnitConverter & To(const char* unitTo, bool strict = true);
	UnitConverter & SetUnits(const char* unitFrom, const char* unitTo, bool strict = true);
	double GetFactor();
	double GetOffset();
	ConvState State();
	bool IsReady();
	aVect<char> GetError();
	double operator()(double value);
	double Convert(double value);
	aVect<double> Convert(const aVect<double>& v);
	void Convert_InPlace(aVect<double>& v);
	aVect<char> FullName_From();
	aVect<char> FullName_To();
};

double ToSI(const double & value, const char * unit);
double FromSI_To(const char * unit, const double & value);

class PhysicalValue {

	double SI_value;

public:

	PhysicalValue() {}

	PhysicalValue(const double & value, const char * unit) {
		this->SI_value = ToSI(value, unit);
	}

	operator double () const {
		return this->SI_value;
	}

	double operator()(const char * toUnit) {
		return FromSI_To(toUnit, this->SI_value);
	}

	double ExpressedIn(const char * toUnit) {
		return this->operator()(toUnit);
	}

	PhysicalValue& operator=(const double & value) {
		this->SI_value = value;
		return *this;
	}
};

#endif
