#include "Date.h"

using namespace std;

const int Date::daysLeapYear[12] = {31,29,31,30,31,30,31,31,30,31,30,31};
const int Date::daysNonLeapYear[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
const long Date::offset = 2415021;	//we define julian date as days since 1900/01/01, so we have an offset compared to std julian dates

Date::Date(const double& julian_in){
	julian = julian_in;
	calculateValues();
}

void Date::setDate(const double& julian_in){
	julian = julian_in;
	calculateValues();
}

double Date::getJulian() const { return julian; }

Date::Date(const Date& orig){
	julian = orig.julian;
	year = orig.year;
	month = orig.month;
	day = orig.day;
	hour = orig.hour;
	minute = orig.minute;
}

Date& Date::operator=(const Date& orig){
	if (this == &orig) //Check for self assignment
		return *this;

	julian = orig.julian;
	year = orig.year;
	month = orig.month;
	day = orig.day;
	hour = orig.hour;
	minute = orig.minute;

	return *this;
}

Date& Date::operator+=(const Date& indate){
	julian += indate.julian;
	calculateValues();
	return *this;
}

Date& Date::operator-=(const Date& indate){
	julian -= indate.julian;
	calculateValues();
	return *this;
}

bool Date::operator==(const Date& indate) const{ 
	const double epsilon=1./(24.*3600.);	//that is, 1 second in units of days
  
	return( fabs(indate.getJulian() - getJulian()) < epsilon );
	/*return ((indate.year==year) && (indate.month==month) 
	  && (indate.day == day) && (indate.hour == hour)
	  && (indate.minute==minute));*/
}

bool Date::operator<(const Date& indate) const{
	if (*this == indate)
		return false;

	return (julian < indate.julian);
}

const Date Date::operator+(const Date& indate) const{
	Date tmp(julian + indate.julian);
	return tmp;
}

const Date Date::operator-(const Date& indate) const{
	Date tmp(julian - indate.julian);
	return tmp;
}

Date::Date(const int& in_year, const int& in_month, const int& in_day, const int& in_hour, const int& in_minute){
	plausibilityCheck(in_year, in_month, in_day, in_hour, in_minute);

	year = in_year;
	month = in_month;
	day = in_day;
	hour=in_hour;
	minute=in_minute;

	calculateJulianDate();
}

void Date::setDate(const int& in_year, const int& in_month, const int& in_day, const int& in_hour, const int& in_minute){
	plausibilityCheck(in_year, in_month, in_day, in_hour, in_minute);

	year = in_year;
	month = in_month;
	day = in_day;
	hour=in_hour;
	minute=in_minute;

	calculateJulianDate();
}

void Date::calculateJulianDate(void){
	const long julday = getJulianDay(year, month, day) - offset; // Begin on the 1.1.1900, OFFSET
	double frac = (minute+60.0*hour) / 1440.0;

	julian = ((double)julday) + frac;
}

void Date::calculateValues(void){
	long t1, t2, yr, mo, julday;

	julday = (long) floor(julian);
	julday += offset; //OFFSET

	t1 = julday + 68569L;
	t2 = 4L * t1 / 146097L;
	t1 = t1 - ( 146097L * t2 + 3L ) / 4L;
	yr = 4000L * ( t1 + 1L ) / 1461001L;
	t1 = t1 - 1461L * yr / 4L + 31L;
	mo = 80L * t1 / 2447L;

	day = (int) ( t1 - 2447L * mo / 80L );
	t1 = mo / 11L;
	month = (int) ( mo + 2L - 12L * t1 );
	year = (int) ( 100L * ( t2 - 49L ) + yr + t1 );
    
	// Correct for BC years
	//if ( year <= 0 )
	//year -= 1;

	//year = year + 1900 + 4713; //OFFSET

	double frac = julian - floor(julian);

	minute = ((int)round(frac*((double)24.0*60.0))) % 60; 
	hour = (int) round((((double)1440.0)*frac-(double)minute)/(double)60.0);
}


bool Date::isLeapYear(const int& iYear) const{
	long jd1, jd2;
	jd1 = getJulianDay( iYear, 2, 28 );
	jd2 = getJulianDay( iYear, 3, 1 );
	return ( (jd2-jd1) > 1 );
}


long Date::getJulianDay(const int& inyear, const int& inmonth, const int& inday) const{
	const long lmonth = (long) inmonth, lday = (long) inday;
	long lyear = (long) inyear;
	long julday = 0;

	if ( lyear < 0 ) // Adjust BC years
		lyear++;

	julday = lday - 32075L +
		1461L * ( lyear + 4800L + ( lmonth - 14L ) / 12L ) / 4L +
		367L * ( lmonth - 2L - ( lmonth - 14L ) / 12L * 12L ) / 12L -
		3L * ( ( lyear + 4900L + ( lmonth - 14L ) / 12L ) / 100L ) / 4L;

	return julday;
}

const string Date::toString() const{
	stringstream tmpstr;
	tmpstr << setprecision(10) << julian << "  " << year << "/" << month << "/" << day << " " 
		  << setw(2) << setfill('0') << hour << ":" 
		  << setw(2) << setfill('0') << minute;
	return tmpstr.str();
}

ostream& operator<<(ostream &os, const Date &date){
	os<<date.toString();
	return os;
}

void Date::plausibilityCheck(const int& in_year, const int& in_month, const int& in_day, const int& in_hour, const int& in_minute) const{
	if ((in_year < -3000) || (in_year >3000) 
	    || (in_month < 1) || (in_month > 12) 
	    || (in_day < 1) || ((in_day > daysNonLeapYear[in_month-1]) && !isLeapYear(in_year)) 
	    || ((in_day > daysLeapYear[in_month-1]) && isLeapYear(in_year)) 
	    || (in_hour < 0) || (in_hour > 24) 
	    || (in_minute < 0) || (in_minute > 59))
		{
			THROW SLFException("InvalidDateFormat", AT);
		}

	if ((in_hour == 24) && (in_minute != 0))
		{
			THROW SLFException("InvalidDateFormat", AT);
		}
}

void Date::getDate(double& julian_out) const{
	julian_out = julian;
}

void Date::getDate(int& year_out, int& month_out, int& day_out) const{
	int tmp;
	getDate(year_out, month_out, day_out, tmp, tmp);
}

void Date::getDate(int& year_out, int& month_out, int& day_out, int& hour_out) const{
	int tmp;
	getDate(year_out, month_out, day_out, hour_out, tmp);
}
 
void Date::getDate(int& year_out, int& month_out, int& day_out, int& hour_out, int& minute_out) const{
	year_out = year;
	month_out=month;
	day_out=day;
	hour_out=hour;
	minute_out=minute;
}


/*
  The following functions are based on information from 
  http://aa.usno.navy.mil/data/docs/JulianDate.php
*/ 
void Date::setRealJulianDate(const double& julian_in){
	julian = julian_in - (double)offset + 0.5;
	calculateValues();
}

void Date::getRealJulianDate(double& julian_out) const{
	julian_out = julian + (double)offset - 0.5;
}
